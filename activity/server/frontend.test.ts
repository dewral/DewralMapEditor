import {test} from 'node:test';
import assert from 'node:assert/strict';
import Fastify from 'fastify';
import {mkdtemp,mkdir,writeFile,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {registerFrontend} from './frontend.js';
test('rebuilt assets are served as JavaScript, not SPA HTML',async(t)=>{
 const root=await mkdtemp(path.join(tmpdir(),'dme-static-')),app=Fastify();
 t.after(async()=>{await app.close();await rm(root,{recursive:true,force:true});});
 await mkdir(path.join(root,'assets'));await writeFile(path.join(root,'index.html'),'<html>DME Activity</html>');
 await registerFrontend(app,root);await app.ready();
 await writeFile(path.join(root,'assets','new-build.js'),'console.log("ready");');
 const script=await app.inject('/assets/new-build.js');assert.equal(script.statusCode,200);assert.match(String(script.headers['content-type']),/javascript/);assert.equal(script.body,'console.log("ready");');
 assert.equal((await app.inject('/assets/missing.js')).statusCode,404);
 assert.equal((await app.inject('/api/missing')).statusCode,404);
 assert.match((await app.inject('/dashboard')).body,/<html>/);
});
