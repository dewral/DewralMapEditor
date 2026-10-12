import type {FastifyInstance} from 'fastify';
import serveStatic from '@fastify/static';
export async function registerFrontend(app:FastifyInstance,root:string) {
 // Resolve assets at request time, including files added by a later rebuild.
 await app.register(serveStatic,{root,wildcard:true});
 app.setNotFoundHandler((req,reply)=>req.url.startsWith('/api/') || req.url.startsWith('/assets/') ? reply.code(404).send({error:'Not found'}) : reply.sendFile('index.html'));
}
