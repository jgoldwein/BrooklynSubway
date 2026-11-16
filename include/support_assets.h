// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
namespace brooklyn::web {
constexpr char kSupportPage[]=R"SUP(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Brooklyn Subway - Get help</title><link rel="stylesheet" href="/ui.css?v=038"><script defer src="/support.js?v=072"></script></head><body><header><div><h1>Brooklyn Subway</h1><p class="subtitle">Specially Designed for Avi Yao</p></div><a href="/help">Back to Help</a></header><main><section class="hero"><h2>Get help</h2><p>If something looks wrong, make a support report and send it to Joel along with what you noticed.</p><p>The report includes software version, uptime, Wi-Fi signal, train-feed health, screen status, memory and recent feed errors. It leaves out passwords, sign-in details, Wi-Fi names and network addresses.</p><p>Review it below before sharing. Send report to Joel opens an email draft to goldwein@gmail.com; you choose when to send it. Nothing is sent automatically.</p><div class="actions"><button id="refresh" type="button">Make support report</button><button id="email" type="button" disabled>Send report to Joel</button><button id="copy" type="button" disabled>Copy support report</button><button id="save" type="button" class="secondary" disabled>Save report file</button></div><p id="notice" role="status" aria-live="polite">Press Make support report to begin.</p><label for="report">Your support report</label><textarea id="report" readonly rows="18" style="width:100%;max-width:100%;font:13px monospace" spellcheck="false"></textarea><noscript><p>Enable JavaScript in your browser to make a report.</p></noscript><p class="hint">This is a snapshot, not a continuous recording. If the board restarts or the problem changes, make a new report.</p></section></main></body></html>)SUP";
constexpr char kSupportScript[]=R"SUP(
'use strict';
// Explicit allowlists: never copy whole API objects into the shared report.
function supportPick(source,keys){const out={};for(const key of keys){const v=source&&source[key];if(v===null||typeof v==='boolean'||(typeof v==='number'&&Number.isFinite(v)))out[key]=v;else if(typeof v==='string')out[key]=v.slice(0,180);}return out;}
function makeSupportReport(s,r){
 const report={reportSchema:1,capturedAt:new Date().toISOString(),note:'Snapshot only. Send with a description of the problem.'};
 report.device=supportPick(s,['firmwareVersion','target','deviceId','bootId','resetReason','uptimeSeconds','otaBootConfirmed','unexpectedWarmResets']);
 report.network=supportPick(s,['networkConnected','rssi','timeSynchronized','fetchingFeeds']);
 report.trains=supportPick(s,['feedCycleState','feedsSucceeded','feedsTotal','dataAgeSeconds','completeAgeSeconds','resolvedTrains','occupiedLeds','visibleMapLeds','darkOccupiedMapLeds','collisionCount','pollSeconds']);
 report.display=supportPick(s,['displayState','physicalLedOutputEnabled','physicalPowerPaused','startupIndicatorsActive','brightness','physicalFramesWritten','physicalMapChanges','routeVisibilityMask','routeFocusActive','routeFocusRoute','quietHoursEnabled','quietHoursActive','quietMapBrightness','unknownTimingEnabled','unknownTimingMode','unknownTimingBrightness','lastReportedFadeSeconds','blueHoldSeconds','oledStatus','oledAddress','oledActiveAddress','oledI2cError','oledContrast']);
 report.memory=supportPick(s,['freeInternalHeap','minimumInternalHeap','largestInternalBlock','freePsram','minimumFreePsram','psramPresent','psramFeedBufferPresent']);
 report.lastFeedFailure=s&&s.lastFeedFailure?supportPick(s.lastFeedFailure,['feed','reason','bytes','elapsedMs']):null;
 if(!s)report.statusUnavailable=true;
 if(r&&s&&r.bootId!==s.bootId){report.runtimeUnavailable='Board restarted between samples. Make a new report.';}
 else if(r){report.runtime=supportPick(r,['bootId','uptimeMs','warmSessionBoots','unexpectedWarmResets','workerStackMinimumBytes','loopStackMinimumBytes','liveStackMinimumBytes','newestFailureSequence','oldestFailureSequence']);report.recentFeedErrors=Array.isArray(r.failures)?r.failures.slice(-12).map(f=>supportPick(f,['sequence','feed','category','bytes','elapsedMs','httpCode','tlsError','startedAtUptimeMs','unixTime','heapBefore','largestBefore','heapBeforeClose','largestBeforeClose','minimumHeap'])):[];}
 else report.runtimeUnavailable=true;
 return report;
}
function buildSupportMail(report){
 const subject='Brooklyn Subway support - '+String(report.device.firmwareVersion||'unknown version').slice(0,16);
 const intro='Hi Joel,\n\nWhat I noticed: [please describe the problem]\n\n';
 let body=intro+JSON.stringify(report,null,2),shortened=false;
 const prefix='mailto:goldwein@gmail.com?subject='+encodeURIComponent(subject)+'&body=';
 if((prefix+encodeURIComponent(body)).length>1800){shortened=true;body=intro+'Full report: please attach brooklyn-subway-support.txt using your email app.\n\n'+JSON.stringify({version:report.device.firmwareVersion,uptime:report.device.uptimeSeconds,reset:report.device.resetReason,wifi:report.network.networkConnected,rssi:report.network.rssi,feeds:report.trains.feedsSucceeded,total:report.trains.feedsTotal,state:report.display.displayState,oled:report.display.oledStatus,minimumHeap:report.memory.minimumInternalHeap,lastFailure:report.lastFeedFailure},null,2);}
 if((prefix+encodeURIComponent(body)).length>1800){shortened=true;body=intro+'Please attach brooklyn-subway-support.txt for the full diagnostics.';}
 return {url:prefix+encodeURIComponent(body),shortened};
}
(()=>{
 const by=id=>document.getElementById(id),notice=message=>{by('notice').textContent=message;};
 async function read(path){const c=new AbortController(),timer=setTimeout(()=>c.abort(),8000);try{const response=await fetch(path,{cache:'no-store',signal:c.signal});if(!response.ok)throw Error('unavailable');return await response.json();}finally{clearTimeout(timer);}}
 by('refresh').onclick=async()=>{by('refresh').disabled=true;by('email').disabled=by('copy').disabled=by('save').disabled=true;by('report').value='';notice('Collecting the report...');let s=null,r=null;try{s=await read('/api/status');}catch(e){}try{r=await read('/api/runtime-diagnostics');}catch(e){}if(!s&&!r){notice('Could not reach your map. Check that your phone is on the same Wi-Fi, then try again.');}else{by('report').value=JSON.stringify(makeSupportReport(s,r),null,2);by('email').disabled=by('copy').disabled=by('save').disabled=false;notice(s&&r&&s.bootId===r.bootId?'Report ready. Review it, then copy or save it.':'Partial report ready. Some details were unavailable; you can share it or try again.');}by('refresh').disabled=false;};
 by('email').onclick=()=>{const mail=buildSupportMail(JSON.parse(by('report').value)),a=document.createElement('a');a.href=mail.url;a.click();notice(mail.shortened?'Email draft requested. Use Save report file, then attach that file in your email app and tap Send. If no email app opens, use Copy instead.':'Email draft requested. Review it in your email app and tap Send. If no email app opens, use Copy instead.');};
 by('copy').onclick=async()=>{try{if(!navigator.clipboard)throw Error('manual');await navigator.clipboard.writeText(by('report').value);notice('Copied! Paste the report into your message to Joel.');}catch(e){by('report').focus();by('report').select();notice('Report selected. Choose Copy on your phone, press Ctrl+C on a computer, or use Save report file.');}};
 by('save').onclick=()=>{const url=URL.createObjectURL(new Blob([by('report').value],{type:'text/plain;charset=utf-8'})),a=document.createElement('a');a.href=url;a.download='brooklyn-subway-support.txt';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);notice('Report file prepared. Attach it to your message to Joel.');};
})();
)SUP";
}
