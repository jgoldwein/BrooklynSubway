#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Put full notes on historical release pages and inline notes on the latest page."""
import argparse,json,subprocess
from pathlib import Path
import release_notes as rn
API='repos/jgoldwein/BrooklynSubway/releases'
def sync_pages(api,root,summary,apply=False,backup=None):
    latest=api(API+'/latest');latest_tag=latest['tag_name'];releases=[]
    for page in range(1,20):
        batch=api(API+'?per_page=100&page='+str(page));releases.extend(batch)
        if len(batch)<100:break
    else:raise ValueError('Unexpectedly many release pages')
    published=[r for r in releases if not r.get('draft')]
    planned=[]
    for release in published:
        version=rn.version_of(release['tag_name']);notes=rn.read_notes(root/'docs'/('RELEASE_'+version+'.md'))
        body=rn.page_body(release,notes,release['tag_name']==latest_tag,summary)
        planned.append((release,body))
    if apply:
        if backup is None:raise ValueError('A backup path is required when applying changes')
        with Path(backup).open('x',encoding='utf-8') as f:json.dump(published,f,indent=2)
    changed=0
    # Update the latest release last, explicitly preserving its latest designation.
    planned.sort(key=lambda pair:pair[0]['tag_name']==latest_tag)
    for release,body in planned:
        if body==(release.get('body') or ''):continue
        changed+=1
        if apply:
            result=api(API+'/'+str(release['id']),'PATCH',{'body':body,'make_latest':'true' if release['tag_name']==latest_tag else 'false'})
            before=[(a['id'],a['name'],a['size']) for a in release['assets']]
            after=[(a['id'],a['name'],a['size']) for a in result['assets']]
            if before!=after or result['tag_name']!=release['tag_name']:raise ValueError('Release identity/assets changed unexpectedly')
        print(('Updated ' if apply else 'Would update ')+release['tag_name'],flush=True)
    if apply:
        final=api(API+'/latest')
        if final['tag_name']!=latest_tag or rn.metadata_bytes(final,final['body'])>rn.METADATA_LIMIT:raise ValueError('Latest-release OTA metadata check failed')
        (root/'CHANGELOG.md').write_text(rn.changelog(published),encoding='utf-8')
    return changed

def gh_api(path,method='GET',data=None):
    cmd=['gh','api','--method',method,path]
    if data is not None:cmd+=['--input','-']
    result=subprocess.run(cmd,input=json.dumps(data) if data is not None else None,text=True,capture_output=True,check=True)
    return json.loads(result.stdout)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--latest-summary',required=True,help='UTF-8 file containing readable inline Markdown notes for the latest release');p.add_argument('--apply',action='store_true');p.add_argument('--backup');a=p.parse_args()
    root=Path(__file__).resolve().parents[1]
    try:
        count=sync_pages(gh_api,root,Path(a.latest_summary).read_text(encoding='utf-8-sig'),a.apply,a.backup);print('Release pages changed:',count)
    except (ValueError,OSError,subprocess.CalledProcessError) as e:p.exit(1,str(e)+'\n')
if __name__=='__main__':main()
