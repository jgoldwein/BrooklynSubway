#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Local helper for the EasyEDA overlay extension. No manual image placement."""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import secrets
import subprocess
import threading
import time
import zipfile
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

import overlay

HERE = Path(__file__).resolve().parent


def extension_package(output):
    """Package the documented EasyEDA IIFE entry and extension manifest."""
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('extension.json',(HERE/'easyeda/extension.json').read_text(encoding='utf-8-sig').encode())
        z.writestr('dist/index.js',(HERE/'easyeda/index.js').read_text(encoding='utf-8-sig').encode())


class Job:
    def __init__(self, baseline, prepared, output):
        self.baseline = Path(baseline)
        self.output = Path(output)
        self.manifest = overlay.load_json(Path(prepared) / 'manifest.json')
        image = overlay.read(Path(prepared) / 'prepared-map.png')
        overlay.check_hash(image, self.manifest['artworkSha256'])
        self.image_uri = 'data:image/png;base64,' + base64.b64encode(image).decode()
        self.token = secrets.token_hex(32)
        self.state = 'waiting'
        self.error = None
        self.probe = None

    def info(self):
        return dict(placement=self.manifest['placement'], imageDataUri=self.image_uri,
                    fileName='brooklyn-prepared-map.png', state=self.state)

    def baseline_check(self, data):
        if self.state != 'waiting':
            raise ValueError('This job has already checked a template')
        path = self.output / 'editor-before.zip'
        overlay.write_new(path, data)
        report = overlay.compare(self.baseline, path)
        overlay.save_json(self.output / 'template-check.json', report)
        if not report['nonArtworkIntegrityPassed']:
            raise ValueError('The active EasyEDA PCB does not match the selected blank Gerber')
        self.state = 'ready'
        return dict(ok=True, state=self.state)

    def finish(self, data):
        if self.state != 'ready':
            raise ValueError('Verify the blank template before exporting the map')
        candidate = self.output / 'editor-with-map.zip'
        overlay.write_new(candidate, data)
        overlay.package(self.baseline, candidate, self.output / 'result')
        self.state = 'complete'
        return dict(ok=True, state=self.state,
                    output=str((self.output / 'result/Gerbers-with-prepared-artwork.zip').resolve()))


def handler_for(job):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def reply(self, code, value):
            data = json.dumps(value).encode()
            self.send_response(code)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Cache-Control', 'no-store')
            # The unguessable, per-run URL is required even for local requests.
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(data)

        def authorized_route(self):
            prefix = '/' + job.token + '/'
            if not self.path.startswith(prefix):
                self.reply(403, dict(error='Invalid job token'))
                return None
            return self.path[len(prefix):]

        def do_OPTIONS(self):
            if self.authorized_route() is None:
                return
            self.send_response(204)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
            self.send_header('Access-Control-Allow-Headers', 'Content-Type')
            self.end_headers()

        def do_GET(self):
            route = self.authorized_route()
            if route is None:
                return
            if route == 'job':
                self.reply(200, job.info())
            else:
                self.reply(404, dict(error='Unknown route'))

        def do_POST(self):
            route = self.authorized_route()
            if route is None:
                return
            try:
                size = int(self.headers.get('Content-Length', '-1'))
                if not 0 < size <= overlay.MAX_ARCHIVE:
                    raise ValueError('Invalid request size')
                self.connection.settimeout(30)
                data = self.rfile.read(size)
                if len(data) != size:
                    raise ValueError('Incomplete request')
                if route == 'baseline':
                    answer = job.baseline_check(data)
                elif route == 'finish':
                    answer = job.finish(data)
                elif route == 'probe':
                    job.probe = json.loads(data)
                    overlay.save_json(job.output / 'easyeda-probe.json', job.probe)
                    answer = dict(ok=True)
                elif route == 'error':
                    raise ValueError(json.loads(data).get('error', 'Editor operation failed'))
                else:
                    self.reply(404, dict(error='Unknown route'))
                    return
                self.reply(200, answer)
            except (ValueError, KeyError, OSError, zipfile.BadZipFile) as error:
                job.error = str(error)
                job.state = 'failed'
                self.reply(409, dict(error=job.error))
    return Handler


def prepare_job(args):
    output = Path(args.output)
    if output.exists():
        raise ValueError('Choose a new output directory; existing results are never overwritten')
    project = Path(args.project)
    if project.suffix.lower() != '.eprj2':
        raise ValueError('This version requires a single-file .eprj2 blank project template')
    project_data = overlay.read(project)
    baseline_data = overlay.read(args.baseline)
    overlay.archive_bytes(baseline_data)
    recipe = overlay.load_json(args.recipe)
    output.mkdir(parents=True)
    local_pdf = output / 'source-map.pdf'
    if args.pdf:
        data = overlay.read(args.pdf)
        overlay.check_hash(data, recipe['source']['sha256'])
        overlay.write_new(local_pdf, data)
    else:
        try:
            overlay.fetch(recipe['source']['url'], recipe['source']['sha256'], local_pdf)
        except Exception as error:
            raise ValueError('Automatic MTA download failed or the map edition changed. '
                             'Use --pdf with the reviewed hash-matching PDF. Details: ' + str(error)) from error
    copied_project = output / 'Blank-template-COPY.eprj2'
    overlay.write_new(copied_project, project_data)
    copied_baseline = output / 'Blank-Gerbers.zip'
    overlay.write_new(copied_baseline, baseline_data)
    overlay.prepare_pdf(local_pdf, args.recipe, output / 'prepared', args.dpi)
    extension_package(output / 'Brooklyn-Overlay-Builder.eext')
    overlay.save_json(output / 'inputs.json', dict(
        projectSha256=hashlib.sha256(project_data).hexdigest(),
        baselineSha256=hashlib.sha256(baseline_data).hexdigest(),
        sourcePdfSha256=overlay.digest(overlay.read(local_pdf)),
        recipeSha256=overlay.digest(overlay.read(args.recipe)), manufacturingApproval=False))
    return Job(copied_baseline, output / 'prepared', output)


def cli_call(executable, *arguments):
    result = subprocess.run([str(executable), *arguments], capture_output=True,
                            text=True, encoding='utf-8-sig', timeout=240)
    try:
        answer = json.loads(result.stdout)
    except ValueError as error:
        raise ValueError('EasyEDA command-line client did not return JSON. Use a CLI-capable version (4.1.60+).') from error
    if result.returncode or not answer.get('ok'):
        raise ValueError('EasyEDA CLI failed: ' + str(answer.get('error', result.stderr))[:1000])
    return answer.get('value')


def run_cli(executable, project, url):
    # Do not probe an older Windows GUI-only binary: it could open an unwanted window.
    import os
    if os.name == 'nt':
        env = dict(os.environ, BROOKLYN_EASYEDA_EXE=str(Path(executable).resolve()))
        version = subprocess.run(['powershell','-NoProfile','-Command',
            "(Get-Item -LiteralPath $env:BROOKLYN_EASYEDA_EXE).VersionInfo.ProductVersion"],
            env=env,capture_output=True,text=True,timeout=10)
        import re
        numbers = re.findall(r'\d+',version.stdout)
        if len(numbers)<3 or tuple(map(int,numbers[:3]))<(4,1,60):
            raise ValueError('Installed EasyEDA lacks the documented CLI (requires 4.1.60+). Use the supplied extension instead.')
    cli_call(executable,'doctor')
    session = cli_call(executable,'open','--path',str(Path(project).resolve()),'--headless','true')
    session_id = str(session['sessionId'])
    try:
        code = (HERE/'easyeda/index.js').read_text(encoding='utf-8-sig')
        code += "\nconst pcbs=await eda.dmt_Pcb.getAllPcbsInfo(); if(pcbs.length!==1)throw new Error('Template must contain exactly one PCB');"
        code += "\nawait eda.dmt_EditorControl.openDocument(pcbs[0].uuid);"
        code += "\nif(eda.dmt_EditorControl.waitForDocumentReady)await eda.dmt_EditorControl.waitForDocumentReady(pcbs[0].uuid,30000);"
        code += "\nreturn await edaEsbuildExportName.build(eda,__CLI__.args.url);"
        return cli_call(executable,'invoke','--session',session_id,'--ext-uuid','eda',
                        '--code',code,'--args',json.dumps(dict(url=url)),'--timeout','180000')
    finally:
        cli_call(executable,'session','close','--session',session_id,'--destroy')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', required=True, help='Depersonalized blank EasyEDA .eprj2 template')
    parser.add_argument('--baseline', required=True, help='Matching blank Gerber ZIP')
    parser.add_argument('--recipe', default=str(HERE / 'recipes/brooklyn-september-2026.json'))
    parser.add_argument('--pdf', help='Reviewed local source PDF; otherwise download from the recipe URL')
    parser.add_argument('--dpi', type=int, default=300)
    parser.add_argument('--output', required=True)
    parser.add_argument('--timeout', type=int, default=1200)
    parser.add_argument('--easyeda-cli', help='Optional EasyEDA 4.1.60+ executable for unattended, hidden-window operation')
    args = parser.parse_args(argv)
    try:
        job = prepare_job(args)
        with HTTPServer(('127.0.0.1', 0), handler_for(job)) as server:
            server.timeout = 1
            url = f'http://127.0.0.1:{server.server_port}/{job.token}'
            print('Prepared the map. Original files are unchanged.', flush=True)
            print('Open:', (job.output / 'Blank-template-COPY.eprj2').resolve(), flush=True)
            print('One-time setup: import Brooklyn-Overlay-Builder.eext and allow external interaction.', flush=True)
            print('In the blank PCB choose Overlay Builder > Build map, then paste this local job URL:', flush=True)
            print(url, flush=True)
            if args.easyeda_cli:
                thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
                try:
                    run_cli(args.easyeda_cli,job.output/'Blank-template-COPY.eprj2',url)
                finally:
                    server.shutdown();thread.join(timeout=5)
                if job.state!='complete':raise ValueError('Editor returned without a verified Gerber result')
            until = time.monotonic() + args.timeout
            while job.state not in ('complete', 'failed') and time.monotonic() < until:
                server.handle_request()
            if job.state != 'complete':
                raise ValueError(job.error or 'Timed out waiting for the EasyEDA extension')
            print('COMPLETE:', (job.output / 'result/Gerbers-with-prepared-artwork.zip').resolve())
            print('All non-color Gerber payloads are byte-identical to the chosen blank. Review the color preview before manufacture.')
        return 0
    except (ValueError, OSError, KeyError, subprocess.TimeoutExpired) as error:
        print('ERROR:', error)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
