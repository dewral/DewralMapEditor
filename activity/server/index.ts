import { Pool } from 'pg';
import { readFile } from 'node:fs/promises';
import { randomUUID } from 'node:crypto';
import path from 'node:path';
import { registerFrontend } from './frontend.js';
import { makeApp } from './app.js';
import { localPool } from './localdb.js';
import { passwordHash } from './security.js';
const pool=process.env.LOCAL_DATABASE ? await localPool(process.env.LOCAL_DATABASE) : new Pool({connectionString:process.env.DATABASE_URL});
await pool.query(await readFile('server/schema.sql','utf8'));
if(!(await pool.query('SELECT 1 FROM users LIMIT 1')).rowCount) {
 const password=process.env.ADMIN_PASSWORD, email=process.env.ADMIN_EMAIL;
 if(!email || !password || password.length<12) throw new Error('Set ADMIN_EMAIL and ADMIN_PASSWORD (at least 12 characters)');
 await pool.query('INSERT INTO users VALUES($1,$2,$3,$4,\'admin\',true)',[randomUUID(),'Administrator',email.toLowerCase(),passwordHash(password)]);
}
if(process.env.LOCAL_DATABASE && process.env.PILOT_PASSWORD && !(await pool.query("SELECT 1 FROM projects LIMIT 1")).rowCount) {
 const project=randomUUID();await pool.query('INSERT INTO projects VALUES($1,$2)',[project,'Pilot lokalny']);await pool.query('INSERT INTO maps(id,project_id,name,filename,tracked) VALUES($1,$2,$3,$4,true)',[randomUUID(),project,'Mapa pilotażowa','pilot.otbm']);
 const admin=(await pool.query('SELECT id FROM users WHERE email=$1',[process.env.ADMIN_EMAIL])).rows[0].id;
 await pool.query('INSERT INTO members VALUES($1,$2)',[project,admin]);
 for(let i=1;i<=2;i++){const id=randomUUID();await pool.query("INSERT INTO users VALUES($1,$2,$3,$4,'mapper',true)",[id,'Mapper testowy '+i,'mapper'+i+'@dme.local',passwordHash(process.env.PILOT_PASSWORD)]);await pool.query('INSERT INTO members VALUES($1,$2)',[project,id]);}
}
const app=await makeApp(pool);
await registerFrontend(app,path.resolve('dist/public'));
app.addHook('onClose',async()=>{await pool.end();});
for(const signal of ['SIGTERM','SIGINT']) process.on(signal,()=>void app.close());
await app.listen({host:process.env.LOCAL_DATABASE?'127.0.0.1':'0.0.0.0',port:Number(process.env.PORT??3000)});
