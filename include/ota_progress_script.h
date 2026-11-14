// Copyright 2026 Joel W. Goldwein
// SPDX-License-Identifier: Apache-2.0
#pragma once
namespace brooklyn::web {
// Page supplies the signed expected version and whether restart was approved.
// Public status survives session expiry; success requires the expected running
// version AND the SDK's successful boot-health confirmation.
constexpr char kOtaProgressScript[] = R"JS(
(function(){
var started=Date.now(),done=false;
var title=document.getElementById('oh'),text=document.getElementById('pt'),bar=document.getElementById('pg');
function later(fn){
 if(done)return;
 if(Date.now()-started>180000){title.textContent='Completion not confirmed';text.textContent='Open the status page to check the running version. Do not start another update until you have checked.';return;}
 setTimeout(fn,2000);
}
function get(url,callback){
 var x=new XMLHttpRequest();x.open('GET',url);x.timeout=5000;
 x.onload=function(){if(!done)callback(x)};
 x.onerror=x.ontimeout=function(){text.textContent='Waiting for the device to restart or reconnect...';later(probe)};
 x.send();
}
function probe(){get('/api/status?ota='+Date.now(),function(x){
 if(x.status!==200){later(probe);return;}
 var d;try{d=JSON.parse(x.responseText)}catch(e){later(probe);return;}
 if(d.firmwareVersion===expected){
  if(d.otaBootConfirmed===true){done=true;bar.value=100;title.textContent='Update complete';text.textContent='Running version '+expected+'. Opening the status page...';setTimeout(function(){location.replace('/')},2000);return;}
  title.textContent='Confirming startup';text.textContent='Version '+expected+' has started. Waiting for its boot health check...';later(probe);return;
 }
 text.textContent='Waiting for version '+expected+(d.firmwareVersion?'; device reports '+d.firmwareVersion:'')+'...';
 if(!waitingForRestart&&d.adminSessionActive===true)later(poll);else later(probe);
})}
function poll(){get('/update/progress',function(x){
 if(x.status===401||x.status===403||(x.responseURL&&new URL(x.responseURL).pathname==="/login")){waitingForRestart=true;probe();return;}
 if(x.status!==200){later(probe);return;}
 var d;try{d=JSON.parse(x.responseText)}catch(e){later(probe);return;}
 if(d.phase===3){title.textContent='Installation failed';text.textContent='The update was not completed. Open the update page for details.';return;}
 if(d.phase===2){if(d.restarting){waitingForRestart=true;probe();return;}bar.value=100;title.textContent='Download verified';text.textContent='Opening restart approval...';setTimeout(function(){location.reload()},1000);return;}
 if(d.phase!==1){waitingForRestart=true;probe();return;}
 var p=d.total>0?Math.min(99,Math.round(100*d.received/d.total)):0;bar.value=p;
 text.textContent=d.total>0?'Downloaded '+Math.round(d.received/1024)+' of '+Math.round(d.total/1024)+' KB ('+p+'%)':'Starting download...';
 if(d.total>0&&d.received>=d.total)text.textContent='Download received. Verifying and preparing restart...';
 later(poll);
})}
if(waitingForRestart)probe();else poll();
})();
)JS";
}
