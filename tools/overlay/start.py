#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Beginner launcher: download the official map, check its edition and open a working PCB."""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import venv
import urllib.request

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
MTA_URL = 'https://www.mta.info/map/5256'
MAX_PDF_BYTES = 32 * 1024 * 1024


def download_pdf(folder):
    """Save a bounded PDF download locally; recipe selection remains a separate gate."""
    print('Downloading the subway map directly from MTA...', flush=True)
    request = urllib.request.Request(MTA_URL, headers={
        'User-Agent': 'BrooklynSubway-OverlayBuilder/1.0',
        'Accept': 'application/pdf'})
    with urllib.request.urlopen(request, timeout=30) as response:
        data = response.read(MAX_PDF_BYTES + 1)
    if len(data) > MAX_PDF_BYTES:
        raise ValueError('MTA returned a file larger than the 32 MB download limit')
    if not data.startswith(b'%PDF-'):
        raise ValueError('MTA returned a web page or another non-PDF response')
    folder.mkdir(parents=True, exist_ok=True)
    path = folder/'Source_Map.pdf'
    with path.open('xb') as output:
        output.write(data)
    print('Downloaded PDF saved locally:', path, flush=True)
    return path


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
    source = p.add_mutually_exclusive_group()
    source.add_argument('--pdf', type=Path, help='Use a local PDF instead of downloading')
    source.add_argument('--choose-pdf', action='store_true', help='Choose a local PDF instead of downloading')
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
        if args.choose_pdf: command.append('--choose-pdf')
        return subprocess.call(command, cwd=ROOT)
    run_name = datetime.now().strftime('Map_%Y%m%d_%H%M%S_%f')
    output = ROOT/'builds'/run_name
    selection = args.pdf
    if not selection and not args.choose_pdf:
        try:
            selection = download_pdf(ROOT/'builds'/(run_name+'_source'))
        except (OSError, ValueError) as error:
            print('Automatic MTA download could not finish:', error, flush=True)
            print('Download the PDF in your browser from '+MTA_URL+' and select it in the file picker. Cancel to stop.', flush=True)
    if not selection:
        selection = choose_pdf()
    if not selection:
        print('Cancelled; no board files changed.')
        return 0
    pdf = Path(selection).resolve()
    recipe = recipe_for(pdf)
    print('Using calibrated recipe:', recipe.name, flush=True)
    print('The generated working copy will open in EasyEDA automatically.\nThen select Advanced > Overlay Builder > Build map and paste THIS RUN\'S local URL.\nKeep this window open until COMPLETE.', flush=True)
    return subprocess.call([sys.executable, str(HERE/'automate.py'),
                            '--project', str(ROOT/'inputs/Blank_Map.eprj2'),
                            '--baseline', str(ROOT/'inputs/Blank_Gerbers.zip'),
                            '--open-project', '--pdf', str(pdf), '--recipe', str(recipe), '--output', str(output)], cwd=ROOT)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print('ERROR:', error)
        raise SystemExit(1)
