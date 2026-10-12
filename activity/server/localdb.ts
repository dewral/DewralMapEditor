import { PGlite } from '@electric-sql/pglite';
import type {Pool} from 'pg';
// Local UniServer pilot only. Production uses PostgreSQL through pg.Pool.
// Serialize complete transactions on PGlite's one connection.
export async function localPool(folder:string):Promise<Pool> {
 const db=new PGlite(folder);await db.waitReady;
 let tail=Promise.resolve();
 async function acquire(){let release!:()=>void;const before=tail;tail=new Promise<void>(r=>{release=r;});await before;return release;}
 async function query(sql:string,params?:any[]){if(!params && sql.includes(';')){await db.exec(sql);return {rows:[],rowCount:0};}const r=await db.query(sql,params);return {rows:r.rows,rowCount:r.rows.length||r.affectedRows||0};}
 const adapter={query:async(sql:string,params?:any[])=>{const release=await acquire();try{return await query(sql,params);}finally{release();}},connect:async()=>{const release=await acquire();return {query,release};},end:async()=>{await tail;await db.close();}};
 return adapter as unknown as Pool;
}
