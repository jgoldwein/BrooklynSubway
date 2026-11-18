// Run with Node.js: node tools/host_tests/support_report_test.cjs
// Uses only Node built-ins; no firmware dependency.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const header=fs.readFileSync(path.join(__dirname,'../../include/support_assets.h'),'utf8');
const script=header.match(/kSupportScript\[\]=R"SUP\(([\s\S]*?)\)SUP";/)[1];
const context=vm.createContext({});vm.runInContext(script.slice(0,script.indexOf('(()=>{')),context);
const status={firmwareVersion:'0.2.72',bootId:'boot-a',uptimeSeconds:120,networkConnected:true,rssi:-60,password:'SECRET',ssid:'SECRET',ip:'SECRET',csrfToken:'SECRET',lastFeedFailure:{feed:'F',reason:'timeout',password:'SECRET'}};
const runtime={bootId:'boot-a',failures:Array.from({length:20},(_,i)=>({sequence:i,feed:'F',category:'timeout',tlsError:-32512,password:'SECRET'}))};
const report=context.makeSupportReport(status,runtime);
assert(!JSON.stringify(report).includes('SECRET'));
assert.equal(report.recentFeedErrors.length,12);
assert.equal(report.recentFeedErrors[0].sequence,8);
assert.equal(report.recentFeedErrors[0].tlsError,-32512);
assert.equal(report.device.uptimeSeconds,120);
assert(context.makeSupportReport(status,{bootId:'boot-b'}).runtimeUnavailable.includes('restarted'));
assert.equal(context.makeSupportReport(null,null).statusUnavailable,true);
assert.equal(context.makeSupportReport(status,null).runtimeUnavailable,true);
assert.equal(context.buildSupportMail(report).unconfigured,true);
context.document={body:{dataset:{supportEmail:'support@example.com',supportName:'The Developer'}}};
const email=context.buildSupportMail(report);
assert(email.url.startsWith('mailto:support@example.com?subject='));
assert(email.url.length<=1800 && email.shortened);
assert(decodeURIComponent(email.url).includes('please attach brooklyn-subway-support.txt'));
const minimal=context.makeSupportReport({},null);
// A genuinely short synthetic report can fit completely in an email draft.
const tiny={device:{},network:{},trains:{},display:{},memory:{}};
assert.equal(context.buildSupportMail(tiny).shortened,false);
const malicious=context.makeSupportReport({firmwareVersion:'\u2603'.repeat(180),oledStatus:'\u2603'.repeat(180),lastFeedFailure:{feed:'\u2603'.repeat(180),reason:'\u2603'.repeat(180)}},null);
assert(context.buildSupportMail(malicious).url.length<=1800);
context.document={body:{dataset:{supportEmail:'support@example.com',supportName:'Sam'}}};
assert(context.buildSupportMail(tiny).url.startsWith('mailto:support@example.com?subject='));
assert(decodeURIComponent(context.buildSupportMail(tiny).url).includes('Hi Sam,'));
module.exports='Support report privacy, error bounds, restart/partial reports and email length/fallback PASS';
if(require.main===module)console.log(module.exports);
