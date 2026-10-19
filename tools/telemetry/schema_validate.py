# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Stdlib validator for the JSON-Schema subset used by docs/schemas/*.json.

Supported keywords: type, enum, const, pattern, minimum, maximum, maxLength,
maxProperties, propertyNames, properties, required, additionalProperties,
oneOf, allOf, if/then, $ref/$defs, and a date-time check for format.
Unsupported keywords raise at load time so the schemas cannot silently drift
beyond what this validator enforces.
"""

import json
import re
from datetime import datetime
from pathlib import Path

SCHEMA_DIR = Path(__file__).resolve().parents[2] / "docs" / "schemas"

SUPPORTED = {
    "$defs", "$id", "$ref", "$schema", "additionalProperties", "allOf", "const",
    "description", "enum", "format", "if", "maxLength", "maxProperties",
    "maximum", "minimum", "oneOf", "pattern", "properties", "propertyNames",
    "required", "then", "title", "type",
}

_TYPES = {
    "object": lambda v: isinstance(v, dict),
    "string": lambda v: isinstance(v, str),
    "boolean": lambda v: isinstance(v, bool),
    "null": lambda v: v is None,
    "integer": lambda v: isinstance(v, int) and not isinstance(v, bool),
    "number": lambda v: isinstance(v, (int, float)) and not isinstance(v, bool),
}


def _check_keywords(node, path="#"):
    if isinstance(node, dict):
        for key, value in node.items():
            if key in ("properties", "$defs"):
                for name, child in value.items():
                    _check_keywords(child, f"{path}/{key}/{name}")
                continue
            if key not in SUPPORTED:
                raise ValueError(f"unsupported schema keyword {key!r} at {path}")
            if key in ("oneOf", "allOf"):
                for index, child in enumerate(value):
                    _check_keywords(child, f"{path}/{key}/{index}")
            elif key in ("if", "then", "propertyNames") or (
                key == "additionalProperties" and isinstance(value, dict)
            ):
                _check_keywords(value, f"{path}/{key}")


def load_schema(name):
    schema = json.loads((SCHEMA_DIR / name).read_text(encoding="utf-8"))
    _check_keywords(schema)
    return schema


def _is_date_time(value):
    try:
        datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return False
    return True


def validate(instance, schema, root=None, path="$"):
    """Return a list of error strings (empty when valid)."""
    root = root if root is not None else schema
    errors = []
    if "$ref" in schema:
        node = root
        for part in schema["$ref"].lstrip("#/").split("/"):
            node = node[part]
        return validate(instance, node, root, path)

    if "type" in schema:
        allowed = schema["type"] if isinstance(schema["type"], list) else [schema["type"]]
        if not any(_TYPES[name](instance) for name in allowed):
            return [f"{path}: expected {schema['type']}"]
    if "const" in schema:
        expected = schema["const"]
        if instance != expected or type(instance) is not type(expected):
            errors.append(f"{path}: must equal {expected!r}")
    if "enum" in schema and instance not in schema["enum"]:
        errors.append(f"{path}: not an allowed value")
    if isinstance(instance, str):
        if "maxLength" in schema and len(instance) > schema["maxLength"]:
            errors.append(f"{path}: too long")
        if "pattern" in schema and not re.search(schema["pattern"], instance):
            errors.append(f"{path}: does not match pattern")
        if schema.get("format") == "date-time" and not _is_date_time(instance):
            errors.append(f"{path}: invalid date-time")
    if isinstance(instance, int) and not isinstance(instance, bool):
        if "minimum" in schema and instance < schema["minimum"]:
            errors.append(f"{path}: below minimum")
        if "maximum" in schema and instance > schema["maximum"]:
            errors.append(f"{path}: above maximum")
    if isinstance(instance, dict):
        if "maxProperties" in schema and len(instance) > schema["maxProperties"]:
            errors.append(f"{path}: too many properties")
        for key in schema.get("required", []):
            if key not in instance:
                errors.append(f"{path}: missing {key}")
        if "propertyNames" in schema:
            for key in instance:
                if validate(key, schema["propertyNames"], root, path):
                    errors.append(f"{path}: invalid property name")
        properties = schema.get("properties", {})
        additional = schema.get("additionalProperties", True)
        for key, value in instance.items():
            if key in properties:
                errors.extend(validate(value, properties[key], root, f"{path}.{key}"))
            elif additional is False:
                errors.append(f"{path}: unexpected property")
            elif isinstance(additional, dict):
                errors.extend(validate(value, additional, root, f"{path}.{key}"))
    if "oneOf" in schema:
        matches = sum(1 for sub in schema["oneOf"] if not validate(instance, sub, root, path))
        if matches != 1:
            errors.append(f"{path}: must match exactly one alternative")
    for sub in schema.get("allOf", []):
        errors.extend(validate(instance, sub, root, path))
    if "if" in schema and not validate(instance, schema["if"], root, path):
        errors.extend(validate(instance, schema.get("then", {}), root, path))
    return errors
