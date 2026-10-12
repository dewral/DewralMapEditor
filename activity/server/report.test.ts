import {test} from 'node:test';
import assert from 'node:assert/strict';
import {aggregate,type ActivityEvent} from './report.js';
import {DateTime} from 'luxon';
function segment(start:string,end:string,active=true):ActivityEvent {return {id:'test',sessionId:'session',mapId:'map',seq:1,kind:'duration',start,at:end,[active?'activeMs':'idleMs']:Date.parse(end)-Date.parse(start)};}
test('duration clipping, AFK and midnight split',()=>{
 const result=aggregate([segment('2026-10-11T21:59:00Z','2026-10-11T22:01:00Z'),segment('2026-10-11T22:01:00Z','2026-10-11T22:02:00Z',false)],0,Date.parse('2026-10-12T00:00:00Z'),'Europe/Warsaw');
 assert.equal(result.totals.activeMs,120000);assert.equal(result.totals.idleMs,60000);assert.equal(result.days.length,2);assert.equal(result.days[0].activeMs,60000);
 assert.equal(aggregate([segment('2026-10-11T22:00:00Z','2026-10-11T22:02:00Z')],Date.parse('2026-10-11T22:01:00Z'),Date.parse('2026-10-11T22:03:00Z'),'UTC').totals.activeMs,60000);
});
test('fall DST retains both repeated hours',()=>{
 const start=DateTime.fromISO('2026-10-25',{zone:'Europe/Warsaw'}),end=start.plus({days:1});
 const result=aggregate([segment(start.toUTC().toISO()!,end.toUTC().toISO()!)],start.toMillis(),end.toMillis(),'Europe/Warsaw');assert.equal(result.totals.activeMs,25*3600000);assert.equal(result.hours.length,25);
});
test('spring DST has 23 hours',()=>{
 const start=DateTime.fromISO('2026-03-29',{zone:'Europe/Warsaw'}),end=start.plus({days:1});
 const result=aggregate([segment(start.toUTC().toISO()!,end.toUTC().toISO()!)],start.toMillis(),end.toMillis(),'Europe/Warsaw');assert.equal(result.hours.length,23);assert.equal(result.totals.activeMs,23*3600000);
});
test('Undo and Redo do not inflate edits or tiles; save errors separate',()=>{
 const events=['edit','undo','redo','save','autosave','save_error','autosave_error','heartbeat'].map((kind,i)=>({id:String(i),sessionId:'s',mapId:'m',seq:i,at:'2026-10-12T00:00:00Z',kind,tiles:100}));
 const t=aggregate(events,0,Date.now()+1000000,'UTC').totals;
 assert.deepEqual(t,{activeMs:0,idleMs:0,edits:1,tiles:100,undo:1,redo:1,saves:1,autosaves:1,saveErrors:2});
});
