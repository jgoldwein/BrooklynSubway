#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build the EasyEDA installer without downloading a map or starting a job."""
import argparse
from pathlib import Path

from automate import extension_package


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path, help='New .eext file to create')
    args = parser.parse_args()
    if args.output.suffix.lower() != '.eext':
        parser.error('Output filename must end in .eext')
    try:
        extension_package(args.output)
    except OSError as error:
        parser.exit(1, f'ERROR: {error}\n')
    print(f'Installer created: {args.output.resolve()}')


if __name__ == '__main__':
    main()
