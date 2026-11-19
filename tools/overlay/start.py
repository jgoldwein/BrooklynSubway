#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Beginner launcher: check bundled inputs, set up Python and choose a local PDF."""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import venv

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def missing_inputs(root):
    return [root/'inputs'/name for name in ('Blank_Map.eprj2', 'Blank_Gerbers.zip')
            if not (root/'inputs'/name).is_file()]


def recipe_for(pdf):
    sha = hashlib.sha256(pdf.read_bytes()).hexdigest()
    for recipe in sorted((HERE/'recipes').glob('*.json')):
        if json.loads(recipe.read_text(encoding='utf-8-sig'))['source']['sha256'] == sha:
            return recipe
    raise ValueError('This PDF edition has not been calibrated. Keep the PDF and report this fingerprint to the maintainer: ' + sha)


def choose_pdf():
    import tkinter
    from tkinter import filedialog
    window = tkinter.Tk()
    window.withdraw()
    try:
        return filedialog.askopenfilename(title='Choose your downloaded MTA map PDF',
                                         filetypes=[('PDF map', '*.pdf')])
    finally:
        window.destroy()


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bootstrap', action='store_true')
    p.add_argument('--pdf', type=Path, help='Optional local PDF; otherwise show a file picker')
    args = p.parse_args(argv)
    missing = missing_inputs(ROOT)
    if missing:
        print('BUILD CANNOT START: this download is missing the required PCB template pair.', flush=True)
        for path in missing: print('  Missing:', path, flush=True)
        print('Download and extract the latest complete software ZIP; it includes this matching pair. A README is not a substitute.\nDo not use the historical art-free manufacturing ZIP as the builder baseline.', flush=True)
        return 1
    if args.bootstrap:
        environment = ROOT/'.venv-overlay'
        python = environment/('Scripts/python.exe' if sys.platform == 'win32' else 'bin/python')
        if not python.is_file():
            print('Preparing the private Python environment...', flush=True)
            venv.EnvBuilder(with_pip=True).create(environment)
        print('Checking the required image/PDF packages (first run may download them)...', flush=True)
        subprocess.run([str(python), '-m', 'pip', 'install', '-r', str(HERE/'requirements.txt')], check=True)
        command = [str(python), str(Path(__file__).resolve())]
        if args.pdf: command.extend(['--pdf', str(args.pdf.resolve())])
        return subprocess.call(command, cwd=ROOT)
    selection = args.pdf or choose_pdf()
    if not selection:
        print('Cancelled; no board files changed.')
        return 0
    pdf = Path(selection).resolve()
    recipe = recipe_for(pdf)
    output = ROOT/'builds'/datetime.now().strftime('Map_%Y%m%d_%H%M%S_%f')
    print('Using calibrated recipe:', recipe.name, flush=True)
    print('When the helper prints Open:, open THAT project copy in EasyEDA.\nThen select Advanced > Overlay Builder > Build map and paste THIS RUN\'S local URL.\nKeep this window open until COMPLETE.', flush=True)
    return subprocess.call([sys.executable, str(HERE/'automate.py'),
                            '--project', str(ROOT/'inputs/Blank_Map.eprj2'),
                            '--baseline', str(ROOT/'inputs/Blank_Gerbers.zip'),
                            '--pdf', str(pdf), '--recipe', str(recipe), '--output', str(output)], cwd=ROOT)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print('ERROR:', error)
        raise SystemExit(1)
