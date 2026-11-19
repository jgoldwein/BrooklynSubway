// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const manifest=JSON.parse(fs.readFileSync(__dirname+'/easyeda/extension.json','utf8'));
assert(Array.isArray(manifest.keywords),'EasyEDA 3.2 details panel requires keywords');
const context={console};vm.runInNewContext(fs.readFileSync(__dirname+'/easyeda/index.js','utf8'),context);
const api=context.edaEsbuildExportName;
function mock(options={}) {
 const events=[];let objects=[],count=0,createdArgs;
 const obj=(id,w,h)=>({getState_PrimitiveId:()=>id,getState_Width:()=>w,getState_Height:()=>h,getState_Layer:()=>3});
 if(options.existing)objects=[obj('existing',20000,20000)];
 const eda={
  sys_Environment:{getEditorCurrentVersion:()=> '3.2.test'},
  sys_ClientUrl:{request:async(url,method,body)=>{
   const route=url.split('/').pop();events.push(route);
   if(route==='job')return{ok:true,json:async()=>({placement:{units:'mm',layer:'Top Silkscreen Layer',widthMm:342.31,heightMm:297.81,startXmm:0,startYmm:297.81,rotationDegrees:0,mirror:false},imageDataUri:'data:image/png;base64,AA==',fileName:'prepared-map.png'})};
   if(route==='baseline'&&options.badBaseline)return{ok:false,json:async()=>({error:'wrong template'})};
   return{ok:true,json:async()=>({output:'result.zip'})};
  }},
  pcb_PrimitiveObject:{
   getAll:async()=>objects.slice(),
   create:async(...args)=>{events.push('create');createdArgs=args;const o=obj('added',options.badWidth?1:args[4],args[5]);objects.push(o);return o},
   delete:async id=>{events.push('delete');if(options.badCleanup)return false;objects=objects.filter(o=>o.getState_PrimitiveId()!==id);return true}
  },
  pcb_Document:{convertCanvasOriginToDataOrigin:async(x,y)=>({x:x+10,y:y+20})},
  pcb_ManufactureData:{getGerberFile:async(name,color,unit,format,other,layers)=>{assert.strictEqual(color,true);assert.strictEqual(unit,'mm');assert.strictEqual(format.integerNumber,4);assert.strictEqual(format.decimalNumber,5);assert(other.drillTable);assert(layers.some(l=>l.layerId===56));events.push('export');if(++count===2&&options.exportFailure)throw Error('export failed');return 'ZIP'}}
 };
 return{eda,events,args:()=>createdArgs,objects:()=>objects};
}
(async()=>{
 const url='http://127.0.0.1:12345/'+'a'.repeat(64);let m=mock();await api.build(m.eda,url);
 assert(m.events.indexOf('baseline')<m.events.indexOf('create'));assert(m.events.indexOf('delete')<m.events.indexOf('finish'));assert.strictEqual(m.objects().length,0);
 const args=m.args();assert.strictEqual(args[0],3);assert.strictEqual(args[1],10);assert(Math.abs(args[2]-(297.81*1000/25.4+20))<1e-6);assert(Math.abs(args[4]-342.31*1000/25.4)<1e-6);
 for(const options of [{badBaseline:true},{existing:true},{exportFailure:true},{badWidth:true},{badCleanup:true}]){
  m=mock(options);await assert.rejects(api.build(m.eda,url));assert(!m.events.includes('finish'));
  if(!options.badCleanup)assert.strictEqual(m.objects().length,options.existing?1:0);
 }
 m=mock();delete m.eda.pcb_PrimitiveObject.create;await assert.rejects(api.build(m.eda,url));assert.strictEqual(m.events.length,0);
 await assert.rejects(api.build(mock().eda,'https://example.com/upload'));
 const logs=[],messages=[];let confirm;
 context.eda={sys_Log:{add:text=>logs.push(text)},sys_Dialog:{
  showInputDialog:(...args)=>{confirm=args[6]},
  showInformationMessage:(...args)=>messages.push(args)
 }};
 api.run();assert(logs[0].includes('0.1.3'));confirm('');assert(messages[0][0].includes('No URL'));
 confirm('not a job URL');await new Promise(resolve=>setImmediate(resolve));
 assert(logs.some(text=>text.includes('Failed:')));assert(messages.some(args=>args[1]==='Overlay failed'));
 console.log('Extension checks PASS: unit/origin conversion, blank guard, baseline gate, placement, cleanup, failed export and local-only URL.');
})().catch(e=>{console.error(e);process.exit(1)});
