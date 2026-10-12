import { DateTime } from 'luxon';
export type ActivityEvent = {id:string; sessionId:string|null; mapId:string|null; seq:number; at:string; kind:string; tiles?:number; start?:string; activeMs?:number; idleMs?:number; status?:string};
export type Totals = {activeMs:number;idleMs:number;edits:number;tiles:number;undo:number;redo:number;saves:number;autosaves:number;saveErrors:number};
export const emptyTotals = ():Totals => ({activeMs:0,idleMs:0,edits:0,tiles:0,undo:0,redo:0,saves:0,autosaves:0,saveErrors:0});
// Duration events contain exactly one activity state. Split the real interval at
// local hour boundaries, which also handles short/long DST days.
export function aggregate(events:ActivityEvent[], from:number, to:number, zone:string) {
 const totals=emptyTotals(), days=new Map<string,Totals>(), hours=new Map<string,Totals>();
 function add(at:number, values:Partial<Totals>) {
  const local=DateTime.fromMillis(at,{zone}), day=local.toISODate()!, hour=local.startOf('hour').toISO()!;
  for(const bucket of [totals, days.get(day) ?? (days.set(day,emptyTotals()),days.get(day)!), hours.get(hour) ?? (hours.set(hour,emptyTotals()),hours.get(hour)!)])
   for(const [key,value] of Object.entries(values)) bucket[key as keyof Totals]+=value!;
 }
 for(const e of events) {
  const at=Date.parse(e.at);
  if(e.kind==='duration' && e.start) {
   const start=Date.parse(e.start), duration=(e.activeMs??0)+(e.idleMs??0);
   if(at<=start || duration<=0) continue;
   let cursor=Math.max(start,from), end=Math.min(at,to);
   while(cursor<end) {
    const boundary=DateTime.fromMillis(cursor,{zone}).startOf('hour').plus({hours:1}).toMillis();
    const next=Math.min(end,boundary), fraction=(next-cursor)/(at-start);
    add(cursor,{activeMs:(e.activeMs??0)*fraction,idleMs:(e.idleMs??0)*fraction}); cursor=next;
   }
  } else if(at>=from && at<to) {
   if(e.kind==='edit') add(at,{edits:1,tiles:e.tiles??0});
   else if(e.kind==='undo') add(at,{undo:1});
   else if(e.kind==='redo') add(at,{redo:1});
   else if(e.kind==='save') add(at,{saves:1});
   else if(e.kind==='autosave') add(at,{autosaves:1});
   else if(e.kind==='save_error' || e.kind==='autosave_error') add(at,{saveErrors:1});
  }
 }
 return {totals,days:[...days].sort().map(([label,values])=>({label,...values})),hours:[...hours].sort().map(([label,values])=>({label,...values}))};
}
