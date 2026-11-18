# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Readable release pages, with a compact latest page for legacy OTA readers."""
import json,re
ROOT_URL='https://github.com/jgoldwein/BrooklynSubway'
METADATA_LIMIT=8160  # 8192-byte firmware limit, leaving a little headroom.
def read_notes(path):
    data=path.read_bytes()
    try:return data.decode('utf-8-sig')
    except UnicodeDecodeError:return data.decode('cp1252')
def version_of(tag):
    if not re.fullmatch(r'v\d+\.\d+\.\d+',tag):raise ValueError('Unexpected firmware tag')
    return tag[1:]
def compact_body(version,summary):
    version_of('v'+version)
    return summary.strip()+'\n\n[Detailed release notes (read online)]('+ROOT_URL+'/blob/main/docs/RELEASE_'+version+'.md)'
def metadata_bytes(release,body):
    candidate=dict(release);candidate['body']=body
    return len(json.dumps(candidate,separators=(',',':'),ensure_ascii=False).encode('utf-8'))
def page_body(release,notes,latest=False,summary=None):
    if not notes.strip():raise ValueError('Release notes are empty')
    if not latest:return notes.strip()+'\n'
    if not summary:raise ValueError('Latest release requires an explicit readable inline summary')
    body=compact_body(version_of(release['tag_name']),summary)
    if metadata_bytes(release,body)>METADATA_LIMIT:raise ValueError('Release page would exceed the legacy OTA metadata budget; shorten the inline summary')
    return body
def changelog(releases):
    published=[r for r in releases if not r.get('draft') and re.fullmatch(r'v\d+\.\d+\.\d+',r['tag_name'])]
    published.sort(key=lambda r:tuple(map(int,version_of(r['tag_name']).split('.'))),reverse=True)
    lines=['# Changelog','','Firmware changes are recorded in release notes for every published version. Open a version below to read its notes on the release page. Attachments are optional. The newest page has concise inline notes plus a link to the full text on GitHub; older pages include their full notes.','','The latest page stays compact so older maps can continue discovering updates.','','[All releases]('+ROOT_URL+'/releases)','','## Published firmware','']
    for release in published:
        version=version_of(release['tag_name'])
        lines.append('- **['+version+']('+release['html_url']+')** - [Full notes online]('+ROOT_URL+'/blob/main/docs/RELEASE_'+version+'.md)')
    return '\n'.join(lines)+'\n'
