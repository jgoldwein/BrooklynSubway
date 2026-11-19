#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Create a separate fabrication ZIP without printed or colorful artwork."""
import argparse
import io
import zipfile
from pathlib import Path

import overlay

PRINT_LAYERS = {'.GTO', '.GBO', '.GDL'}
EMPTY_GERBER = b'G04 Printed artwork intentionally removed*\n%FSLAX45Y45*%\n%MOMM*%\nM02*\n'


def prepare(source, output, report_path):
    original = overlay.archive(source)
    result = {}
    removed, cleared = [], []
    for name, data in original.items():
        ext = Path(name).suffix.upper()
        if ext in overlay.COLOR:
            removed.append(name)
        elif ext in PRINT_LAYERS:
            result[name] = EMPTY_GERBER
            cleared.append(name)
        else:
            result[name] = data
    if not cleared:
        raise ValueError('No recognized printed layers; review this archive manually')
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, data in result.items():
            z.writestr(name, data)
    data = buffer.getvalue()
    actual = overlay.archive_bytes(data)
    preserved = [name for name in original if name not in removed + cleared]
    assert all(actual[name] == original[name] for name in preserved)
    report = dict(schema=1, sourceSha256=overlay.digest(overlay.read(source)),
                  outputSha256=overlay.digest(data), removedColorMembers=removed,
                  clearedPrintedMembers=cleared, preservedMembers=preserved,
                  protectedPayloadsByteIdentical=True, manufacturingApproval=False,
                  note='All printed labels are removed along with artwork. This is an art-free derivative, not a matching editable builder template or manufacturing approval.')
    if Path(output).exists() or Path(report_path).exists():
        raise ValueError('Choose new output and report paths')
    overlay.write_new(output, data)
    overlay.save_json(report_path, report)
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source')
    p.add_argument('--output', required=True)
    p.add_argument('--report', required=True)
    args = p.parse_args()
    prepare(args.source, args.output, args.report)
    print('Created art-free derivative:', args.output)


if __name__ == '__main__':
    main()
