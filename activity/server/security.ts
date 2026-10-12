import { randomBytes, scryptSync, timingSafeEqual, createHash } from 'node:crypto';
export const token = () => randomBytes(32).toString('hex');
export const hash = (value:string) => createHash('sha256').update(value).digest('hex');
export function passwordHash(password:string) { const salt=token(); return `${salt}:${scryptSync(password,salt,64).toString('hex')}`; }
export function checkPassword(password:string, stored:string) { const [salt,expected]=stored.split(':'); if(!salt || !expected) return false; const actual=scryptSync(password,salt,64), bytes=Buffer.from(expected,'hex'); return bytes.length===actual.length && timingSafeEqual(actual,bytes); }
export function mayRead(role:string, own:string, target:string) { return role==='admin' || own===target; }
