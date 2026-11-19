// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
// EasyEDA's documented extension entry format (same IIFE global as its SDK).
var edaEsbuildExportName = (() => {
  const mmToMil = value => value * 1000 / 25.4;
  const near = (a, b) => Number.isFinite(a) && Math.abs(a - b) < 0.001;
  function assertCapabilities(api) {
    const required = [
      [api.pcb_PrimitiveObject, 'create'], [api.pcb_PrimitiveObject, 'delete'],
      [api.pcb_PrimitiveObject, 'getAll'], [api.pcb_ManufactureData, 'getGerberFile'],
      [api.pcb_Document, 'convertCanvasOriginToDataOrigin'], [api.sys_ClientUrl, 'request']
    ];
    if (required.some(([owner, key]) => !owner || typeof owner[key] !== 'function'))
      throw new Error('This EasyEDA version does not expose all required overlay APIs. No artwork was changed.');
  }
  async function build(api, url) {
    if (!/^http:\/\/127\.0\.0\.1:\d{1,5}\/[0-9a-f]{64}$/.test(url))
      throw new Error('Paste the local job URL printed by automate.py.');
    assertCapabilities(api);
    const request = async (route, method = 'GET', body) => {
      const response = await api.sys_ClientUrl.request(url + '/' + route, method, body);
      const result = await response.json();
      if (!response.ok || result.error) throw new Error(result.error || 'Local helper request failed');
      return result;
    };
    const job = await request('job');
    const p = job.placement;
    if (p.units !== 'mm' || p.layer !== 'Top Silkscreen Layer' || p.rotationDegrees !== 0 || p.mirror !== false)
      throw new Error('Unsupported placement recipe');
    const width = mmToMil(p.widthMm), height = mmToMil(p.heightMm);
    const before = await api.pcb_PrimitiveObject.getAll(3);
    if (before.some(o => o.getState_Width() > width * 0.7 && o.getState_Height() > height * 0.7))
      throw new Error('This PCB already contains a map-size image. Open the map-less COPY of the template.');
    await request('probe', 'POST', JSON.stringify({
      objectCount: before.length, widthMil: width, heightMil: height,
      editorVersion: api.sys_Environment?.getEditorCurrentVersion?.() || 'unknown'
    }));
    const blank = await api.pcb_ManufactureData.getGerberFile('overlay-before', true);
    if (!blank) throw new Error('EasyEDA did not produce a blank-template export');
    await request('baseline', 'POST', blank);
    const position = await api.pcb_Document.convertCanvasOriginToDataOrigin(
      mmToMil(p.startXmm), mmToMil(p.startYmm));
    if (!Number.isFinite(position.x) || !Number.isFinite(position.y)) throw new Error('Invalid canvas-origin conversion');
    let created;
    let candidate;
    try {
      created = await api.pcb_PrimitiveObject.create(3, position.x, position.y,
        job.imageDataUri, width, height, 0, false, job.fileName, false);
      if (!created) throw new Error('EasyEDA failed to create the color image');
      if (!near(created.getState_Width(), width) || !near(created.getState_Height(), height)
          || created.getState_Layer() !== 3) throw new Error('Created image dimensions/layer do not match');
      const placed = await api.pcb_PrimitiveObject.getAll(3);
      if (placed.length !== before.length + 1) throw new Error('Unexpected image count after placement');
      candidate = await api.pcb_ManufactureData.getGerberFile('overlay-with-map', true);
      if (!candidate) throw new Error('EasyEDA did not produce the mapped export');
    } finally {
      if (created) {
        const removed = await api.pcb_PrimitiveObject.delete(created.getState_PrimitiveId());
        if (!removed) throw new Error('Could not remove the temporary image from the template copy');
        const after = await api.pcb_PrimitiveObject.getAll(3);
        const ids = items => items.map(o => o.getState_PrimitiveId()).sort().join(',');
        if (ids(before) !== ids(after)) throw new Error('Template image objects changed unexpectedly');
      }
    }
    return await request('finish', 'POST', candidate);
  }
  function run() {
    eda.sys_Dialog.showInputDialog('Paste the local job URL from the Python helper.',
      'Open Blank-template-COPY.eprj2 and activate its map-less PCB first.',
      'Build subway map', 'text', '', {}, value => {
        if (!value) return;
        build(eda, String(value).trim()).then(result => {
          console.log('Overlay complete:', result.output);
        }).catch(async error => {
          console.error('Overlay failed:', error);
          const url = String(value).trim();
          if (/^http:\/\/127\.0\.0\.1:\d{1,5}\/[0-9a-f]{64}$/.test(url)) {
            try { await eda.sys_ClientUrl.request(url + '/error', 'POST', JSON.stringify({error: String(error.message || error)})); } catch (_) {}
          }
        });
      });
  }
  return {run, build, mmToMil, assertCapabilities};
})();
