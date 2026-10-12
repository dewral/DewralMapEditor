import Fastify from 'fastify';
import cookie from '@fastify/cookie';
import rateLimit from '@fastify/rate-limit';
import { Pool } from 'pg';
import { randomUUID, randomBytes } from 'node:crypto';
import { z } from 'zod';
import { DateTime } from 'luxon';
import { aggregate, type ActivityEvent } from './report.js';
import { hash, token, passwordHash, checkPassword, mayRead } from './security.js';

const uuid=z.string().uuid(), name=z.string().trim().min(1).max(120);
const filename=z.string().trim().min(1).max(255).regex(/^[^/\\:*?"<>|\x00-\x1f]+\.otbm$/i);
const event=z.object({id:uuid,sessionId:uuid.nullable(),mapId:uuid.nullable(),seq:z.number().int().nonnegative().safe(),at:z.string().datetime(),
 kind:z.enum(['start','end','duration','heartbeat','logout','edit','undo','redo','save','autosave','save_error','autosave_error']),
 tiles:z.number().int().nonnegative().max(100000000).optional(),start:z.string().datetime().optional(),
 activeMs:z.number().int().nonnegative().max(3600000).optional(),idleMs:z.number().int().nonnegative().max(3600000).optional(),
 status:z.enum(['active','afk','editor_open','offline']).optional()
}).superRefine((e,ctx)=>{
 if(e.kind!=='heartbeat' && e.kind!=='logout' && (!e.sessionId || !e.mapId)) ctx.addIssue({code:'custom',message:'Session and map required'});
 if(e.kind==='duration') {
  const duration=(e.activeMs??0)+(e.idleMs??0), elapsed=Date.parse(e.at)-Date.parse(e.start??'');
  if(!Number.isFinite(elapsed)||elapsed<=0||elapsed>3600000||duration!==elapsed||((e.activeMs??0)>0 && (e.idleMs??0)>0)) ctx.addIssue({code:'custom',message:'Invalid activity interval'});
 }
});
type User={id:string;name:string;role:string};
declare module 'fastify' { interface FastifyRequest { user?:User; device?:{id:string;user_id:string} } }
export async function makeApp(pool:Pool) {
 const app=Fastify({logger:true,trustProxy:true,bodyLimit:1024*1024});
 await app.register(cookie);
 await app.register(rateLimit,{max:120,timeWindow:'1 minute'});
 const secure=process.env.NODE_ENV==='production';
 app.addHook('onRequest',async(req,reply)=>{
  if(['POST','PUT','PATCH','DELETE'].includes(req.method) && !req.url.startsWith('/api/device/')) {
   if(req.headers['content-type']?.split(';')[0]!=='application/json') return reply.code(415).send({error:'JSON required'});
   const origin=req.headers.origin;
   if(origin && ![process.env.PUBLIC_ORIGIN,...(process.env.LOCAL_DATABASE?['http://localhost','http://127.0.0.1','http://localhost:3000','http://127.0.0.1:3000']:[])].includes(origin)) return reply.code(403).send({error:'Invalid origin'});
  }
 });
 app.setErrorHandler((error,_req,reply)=>{ app.log.error(error); reply.code(error instanceof z.ZodError ? 400 : error.statusCode??500).send({error:error instanceof z.ZodError ? 'Invalid request' : error.statusCode ? error.message : 'Server error'}); });
 const fail=(statusCode:number,message:string)=>{throw Object.assign(new Error(message),{statusCode});};
 const auth=async(req:any)=>{
  const result=await pool.query('SELECT u.id,u.name,u.role FROM logins l JOIN users u ON u.id=l.user_id WHERE l.token=$1 AND l.expires_at>now() AND u.enabled',[hash(req.cookies.dme_session??'')]);
  if(!result.rowCount) fail(401,'Login required'); req.user=result.rows[0];
 };
 const admin=async(req:any)=>{await auth(req); if(req.user.role!=='admin') fail(403,'Administrator required');};
 if(process.env.LOCAL_DATABASE) app.post('/api/local/stop',{preHandler:admin},async(req,reply)=>{
  if(!['127.0.0.1','::1'].includes(req.ip)) fail(403,'Local access only');
  reply.send({ok:true});setImmediate(()=>void app.close().then(()=>process.exit(0)));return reply;
 });
 const deviceAuth=async(req:any)=>{
  const result=await pool.query('SELECT d.id,d.user_id FROM devices d JOIN users u ON u.id=d.user_id WHERE d.token=$1 AND NOT d.revoked AND u.enabled',[hash(String(req.headers.authorization??'').replace(/^Bearer /,''))]);
  if(!result.rowCount) fail(401,'Device access revoked or invalid'); req.device=result.rows[0];
 };
 app.get('/api/health',async()=>{await pool.query('SELECT 1');return {ok:true,service:'dme-activity'};});
 app.post('/api/login',{config:{rateLimit:{max:10,timeWindow:'1 minute'}}},async(req,reply)=>{
  const input=z.object({email:z.string().email(),password:z.string().min(1).max(256)}).parse(req.body);
  const found=await pool.query('SELECT * FROM users WHERE email=$1 AND enabled',[input.email.toLowerCase()]);
  // Equal work for missing accounts avoids a cheap account-existence probe.
  const stored=found.rows[0]?.password??passwordHash('unavailable');
  if(!checkPassword(input.password,stored)||!found.rowCount) return reply.code(401).send({error:'Nieprawidłowy login lub hasło'});
  const secret=token(); await pool.query('INSERT INTO logins VALUES($1,$2,now()+interval \'12 hours\')',[hash(secret),found.rows[0].id]);
  reply.setCookie('dme_session',secret,{path:'/',httpOnly:true,secure,sameSite:'strict',maxAge:43200}); return {ok:true};
 });
 app.post('/api/logout',{preHandler:auth},async(req,reply)=>{await pool.query('DELETE FROM logins WHERE token=$1',[hash(req.cookies.dme_session??'')]);reply.clearCookie('dme_session',{path:'/'});return {ok:true};});
 app.get('/api/me',{preHandler:auth},async(req)=>req.user);
 app.get('/api/catalog',{preHandler:auth},async(req)=>{
  const a=req.user!.role==='admin', id=req.user!.id;
  const projects=await pool.query('SELECT p.* FROM projects p WHERE $1 OR EXISTS(SELECT 1 FROM members WHERE project_id=p.id AND user_id=$2) ORDER BY name',[a,id]);
  const maps=await pool.query('SELECT m.* FROM maps m WHERE $1 OR EXISTS(SELECT 1 FROM members WHERE project_id=m.project_id AND user_id=$2) ORDER BY name',[a,id]);
  const users=await pool.query('SELECT id,name,role,enabled FROM users WHERE $1 OR id=$2 ORDER BY name',[a,id]);
  const devices=await pool.query('SELECT id,user_id,name,revoked,last_received FROM devices WHERE $1 OR user_id=$2',[a,id]);
  const members=a ? (await pool.query('SELECT * FROM members')).rows : [];
  return {projects:projects.rows,maps:maps.rows,users:users.rows,devices:devices.rows,members};
 });
 app.post('/api/users',{preHandler:admin},async(req)=>{
  const b=z.object({name,email:z.string().email(),password:z.string().min(12).max(256),role:z.enum(['admin','mapper'])}).parse(req.body);
  const id=randomUUID(); await pool.query('INSERT INTO users VALUES($1,$2,$3,$4,$5,true)',[id,b.name,b.email.toLowerCase(),passwordHash(b.password),b.role]);return {id};
 });
 app.post('/api/projects',{preHandler:admin},async(req)=>{const b=z.object({name}).parse(req.body),id=randomUUID();await pool.query('INSERT INTO projects VALUES($1,$2)',[id,b.name]);return {id};});
 app.post('/api/maps',{preHandler:admin},async(req)=>{const b=z.object({name,projectId:uuid,filename,tracked:z.boolean().default(true)}).parse(req.body),id=randomUUID();await pool.query('INSERT INTO maps(id,project_id,name,filename,tracked) VALUES($1,$2,$3,$4,$5)',[id,b.projectId,b.name,b.filename,b.tracked]);return {id};});
 app.patch('/api/maps/:id',{preHandler:admin},async(req)=>{const id=uuid.parse((req.params as any).id),b=z.object({filename,tracked:z.boolean()}).parse(req.body);const result=await pool.query('UPDATE maps SET filename=$2,tracked=$3 WHERE id=$1 RETURNING id',[id,b.filename,b.tracked]);if(!result.rowCount)fail(404,'Map not found');return {ok:true};});
 app.put('/api/members',{preHandler:admin},async(req)=>{const b=z.object({projectId:uuid,userId:uuid,enabled:z.boolean()}).parse(req.body);await pool.query(b.enabled ? 'INSERT INTO members VALUES($1,$2) ON CONFLICT DO NOTHING' : 'DELETE FROM members WHERE project_id=$1 AND user_id=$2',[b.projectId,b.userId]);return {ok:true};});
 app.post('/api/device/login',{config:{rateLimit:{max:10,timeWindow:'1 minute'}}},async(req,reply)=>{
  const b=z.object({email:z.string().email(),password:z.string().min(1).max(256),name}).parse(req.body);
  const found=await pool.query('SELECT id,name,email,password FROM users WHERE email=$1 AND enabled',[b.email.toLowerCase()]);
  const stored=found.rows[0]?.password??passwordHash('unavailable');
  if(!checkPassword(b.password,stored)||!found.rowCount)return reply.code(401).send({error:'Nieprawidłowy login lub hasło'});
  const id=randomUUID(),secret=token();await pool.query('INSERT INTO devices(id,user_id,token,name) VALUES($1,$2,$3,$4)',[id,found.rows[0].id,hash(secret),b.name]);
  return {deviceId:id,token:secret,accountName:found.rows[0].name,accountEmail:found.rows[0].email};
 });
 app.get('/api/device/summary',{preHandler:deviceAuth},async(req)=>{
  // Device credentials always read their own account, even for administrators.
  z.object({}).strict().parse(req.query);
  const zone='Europe/Warsaw',start=DateTime.now().setZone(zone).startOf('day'),end=start.plus({days:1});
  const rows=await pool.query(`SELECT e.payload,e.session_id FROM events e JOIN sessions s ON s.id=e.session_id WHERE s.user_id=$1 AND e.at>=$2::timestamptz-interval '1 hour' AND e.at<$3 ORDER BY e.at LIMIT 200001`,[req.device!.user_id,start.toISO(),end.toISO()]);
  if(rows.rows.length>200000)fail(413,'Too much activity for the daily summary');
  const totals=aggregate(rows.rows.map(r=>r.payload),start.toMillis(),end.toMillis(),zone).totals;
  const sessions=new Set(rows.rows.filter(r=>Date.parse(r.payload.at)>=start.toMillis()).map(r=>r.session_id)).size;
  return {day:start.toISODate(),zone,totals,sessions,updatedAt:DateTime.utc().toISO()};
 });
 app.post('/api/devices/:id/revoke',{preHandler:auth},async(req)=>{
  const id=uuid.parse((req.params as any).id),d=await pool.query('SELECT user_id FROM devices WHERE id=$1',[id]);
  if(!d.rowCount||!mayRead(req.user!.role,req.user!.id,d.rows[0].user_id)) fail(403,'Access denied');await pool.query('UPDATE devices SET revoked=true,status=\'offline\' WHERE id=$1',[id]);return {ok:true};
 });
 app.get('/api/device/maps',{preHandler:deviceAuth},async(req)=>(await pool.query('SELECT m.id,m.name,m.filename,m.tracked,p.name AS project FROM maps m JOIN projects p ON p.id=m.project_id JOIN members x ON x.project_id=p.id WHERE x.user_id=$1 ORDER BY p.name,m.name',[req.device!.user_id])).rows);
 app.post('/api/device/events',{preHandler:deviceAuth},async(req)=>{
  const events=z.object({events:z.array(event).min(1).max(500)}).parse(req.body).events;
  const client=await pool.connect(), accepted:string[]=[];
  try {await client.query('BEGIN');
   for(const e of events) {
    if(Date.parse(e.at)>Date.now()+300000) fail(400,'Event time is in the future');
    if(e.mapId) {
     const access=await client.query('SELECT 1 FROM maps m JOIN members x ON x.project_id=m.project_id WHERE m.id=$1 AND x.user_id=$2',[e.mapId,req.device!.user_id]);if(!access.rowCount) fail(403,'Map not assigned');
    }
    if(e.sessionId) {
     if(!e.mapId) fail(400,'Map required');
     await client.query('INSERT INTO sessions(id,user_id,device_id,map_id) VALUES($1,$2,$3,$4) ON CONFLICT DO NOTHING',[e.sessionId,req.device!.user_id,req.device!.id,e.mapId]);
     const session=await client.query('SELECT 1 FROM sessions WHERE id=$1 AND device_id=$2 AND map_id=$3',[e.sessionId,req.device!.id,e.mapId]);if(!session.rowCount) fail(403,'Session does not belong to device');
    }
    const inserted=await client.query('INSERT INTO events VALUES($1,$2,$3,$4,$5,$6,$7) ON CONFLICT DO NOTHING RETURNING id',[e.id,req.device!.id,e.sessionId,e.seq,e.at,e.kind,e]);
    if(!inserted.rowCount) {const prior=await client.query('SELECT 1 FROM events WHERE id=$1 AND device_id=$2 AND payload=$3::jsonb',[e.id,req.device!.id,e]);if(!prior.rowCount) fail(409,'Event ID or sequence conflict');}
    accepted.push(e.id);
    if(e.kind==='heartbeat') await client.query('UPDATE devices SET last_received=now(),last_event=$2,status=$3,current_map=$4 WHERE id=$1 AND (last_event IS NULL OR last_event<$2)',[req.device!.id,e.at,e.status??'editor_open',e.mapId]);
    if(e.kind==='logout')await client.query('UPDATE devices SET revoked=true,status=\'offline\',current_map=NULL WHERE id=$1',[req.device!.id]);
   }
   await client.query('COMMIT'); return {accepted};
  }catch(e){await client.query('ROLLBACK');throw e;}finally{client.release();}
 });
 const reportFilters=z.object({from:z.string().regex(/^\d{4}-\d{2}-\d{2}$/),to:z.string().regex(/^\d{4}-\d{2}-\d{2}$/),zone:z.string().default('Europe/Warsaw'),projectId:uuid.optional(),mapId:uuid.optional(),userId:uuid.optional()});
 app.get('/api/report',{preHandler:auth},async(req)=>{
  const q=reportFilters.parse(req.query), from=DateTime.fromISO(q.from,{zone:q.zone}).startOf('day'),to=DateTime.fromISO(q.to,{zone:q.zone}).plus({days:1}).startOf('day');
  if(!from.isValid||!to.isValid||to<=from||to.diff(from,'days').days>93) fail(400,'Choose a valid range of up to 93 days');
  if(q.userId&&!mayRead(req.user!.role,req.user!.id,q.userId)) fail(403,'Access denied');
  const params=[req.user!.role==='admin',req.user!.id,q.projectId??null,q.mapId??null,q.userId??null,from.toISO(),to.toISO()];
  const rows=await pool.query(`SELECT e.payload,s.user_id,s.map_id,s.id AS session_id,s.note,u.name AS user_name,m.name AS map_name,p.name AS project_name FROM events e JOIN sessions s ON s.id=e.session_id JOIN users u ON u.id=s.user_id JOIN maps m ON m.id=s.map_id JOIN projects p ON p.id=m.project_id WHERE ($1 OR s.user_id=$2) AND ($3::uuid IS NULL OR p.id=$3) AND ($4::uuid IS NULL OR m.id=$4) AND ($5::uuid IS NULL OR u.id=$5) AND e.at>=$6::timestamptz-interval '1 hour' AND e.at<$7 ORDER BY e.at LIMIT 200001`,params);
  if(rows.rows.length>200000) fail(413,'Too much activity: select a shorter date range');
  const summary=aggregate(rows.rows.map(r=>r.payload),from.toMillis(),to.toMillis(),q.zone);
  const mapperGroups=new Map<string,{id:string;name:string;events:ActivityEvent[]}>();
  for(const row of rows.rows) {
   if(!mapperGroups.has(row.user_id)) mapperGroups.set(row.user_id,{id:row.user_id,name:row.user_name,events:[]});
   mapperGroups.get(row.user_id)!.events.push(row.payload);
  }
  const groups=new Map<string,any>();
  for(const row of rows.rows) { if(!groups.has(row.session_id)) groups.set(row.session_id,{...row,events:[]});groups.get(row.session_id).events.push(row.payload); }
  const sessions=[...groups.values()].map(s=>{const relevant=s.events.filter((e:ActivityEvent)=>Date.parse(e.at)>=from.toMillis() || (e.kind==='duration'&&Date.parse(e.at)>from.toMillis()));return {id:s.session_id,userId:s.user_id,user:s.user_name,map:s.map_name,project:s.project_name,note:s.note,first:relevant[0]?.at,last:relevant.at(-1)?.at,...aggregate(s.events,from.toMillis(),to.toMillis(),q.zone).totals};}).filter(s=>s.first).sort((a,b)=>b.first.localeCompare(a.first));
  const live=await pool.query(`SELECT u.id,u.name,d.name AS device,m.name AS map,d.last_event,d.last_received, CASE WHEN d.revoked OR d.last_received<now()-interval '90 seconds' OR d.last_event<now()-interval '90 seconds' OR d.id IS NULL THEN 'offline' ELSE d.status END AS status FROM users u LEFT JOIN devices d ON d.user_id=u.id LEFT JOIN maps m ON m.id=d.current_map WHERE u.enabled AND ($1 OR u.id=$2) AND ($5::uuid IS NULL OR u.id=$5) AND ($3::uuid IS NULL OR EXISTS(SELECT 1 FROM members x WHERE x.user_id=u.id AND x.project_id=$3)) AND ($4::uuid IS NULL OR d.current_map=$4) ORDER BY u.name`,params.slice(0,5));
  const today=DateTime.now().setZone(q.zone).startOf('day');
  const todayRows=await pool.query(`SELECT e.payload,s.user_id FROM events e JOIN sessions s ON s.id=e.session_id JOIN maps m ON m.id=s.map_id WHERE ($1 OR s.user_id=$2) AND ($3::uuid IS NULL OR m.project_id=$3) AND ($4::uuid IS NULL OR m.id=$4) AND ($5::uuid IS NULL OR s.user_id=$5) AND e.at>=$6::timestamptz-interval '1 hour' AND e.at<$7`,[...params.slice(0,5),today.toISO(),today.plus({days:1}).toISO()]);
  const latest=await pool.query(`SELECT s.user_id,MAX(e.at) FILTER(WHERE e.kind='edit') AS last_edit,MAX(e.at) FILTER(WHERE e.kind IN ('save','autosave')) AS last_save FROM events e JOIN sessions s ON s.id=e.session_id JOIN maps m ON m.id=s.map_id WHERE ($1 OR s.user_id=$2) AND ($3::uuid IS NULL OR m.project_id=$3) AND ($4::uuid IS NULL OR m.id=$4) AND ($5::uuid IS NULL OR s.user_id=$5) GROUP BY s.user_id`,params.slice(0,5));
  const mappers=[...mapperGroups.values()].map(u=>{
   const stats=aggregate(u.events,from.toMillis(),to.toMillis(),q.zone);
   return {id:u.id,name:u.name,...stats.totals,sessions:sessions.filter(s=>s.userId===u.id).length,
    mostActiveDay:stats.days.filter(d=>d.activeMs>0).sort((a,b)=>b.activeMs-a.activeMs)[0]??null};
  }).sort((a,b)=>a.name.localeCompare(b.name));
  return {...summary,sessions,mappers,live:live.rows.map(r=>({...r,...latest.rows.find(s=>s.user_id===r.id),todayActiveMs:aggregate(todayRows.rows.filter(s=>s.user_id===r.id).map(s=>s.payload),today.toMillis(),today.plus({days:1}).toMillis(),q.zone).totals.activeMs}))};
 });
 app.get('/api/sessions/:id',{preHandler:auth},async(req)=>{
  const id=uuid.parse((req.params as any).id),s=await pool.query('SELECT s.*,u.name AS user_name,m.name AS map_name FROM sessions s JOIN users u ON u.id=s.user_id JOIN maps m ON m.id=s.map_id WHERE s.id=$1',[id]);
  if(!s.rowCount||!mayRead(req.user!.role,req.user!.id,s.rows[0].user_id)) fail(403,'Access denied');
  const rows=await pool.query('SELECT payload FROM events WHERE session_id=$1 ORDER BY at,seq LIMIT 200001',[id]);if(rows.rows.length>200000) fail(413,'Session too large');
  const events=rows.rows.map(r=>r.payload as ActivityEvent);return {...s.rows[0],events,...aggregate(events,0,Date.now()+300000,'Europe/Warsaw').totals};
 });
 app.patch('/api/sessions/:id/note',{preHandler:auth},async(req)=>{
  const id=uuid.parse((req.params as any).id),b=z.object({note:z.string().max(4000)}).parse(req.body);
  const updated=await pool.query('UPDATE sessions SET note=$1 WHERE id=$2 AND ($3 OR user_id=$4) RETURNING id',[b.note,id,req.user!.role==='admin',req.user!.id]);if(!updated.rowCount) fail(403,'Access denied');return {ok:true};
 });
 return app;
}
