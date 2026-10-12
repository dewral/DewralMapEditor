import React, { useEffect, useState } from 'react';
import { t, locale, useLanguage, LanguageSwitch } from './i18n';
import { createRoot } from 'react-dom/client';
import './style.css';
import { Dashboard } from './dashboard';
type User = {
    id: string;
    name: string;
    role: string;
};
type Catalog = {
    projects: any[];
    maps: any[];
    users: any[];
    devices: any[];
    members: any[];
};
const initial: Catalog = { projects: [], maps: [], users: [], devices: [], members: [] };
async function api(url: string, method = 'GET', body?: unknown) {
    const response = await fetch(window.location.pathname.startsWith('/dme-activity/') ? '/dme-activity/api.php?path=' + encodeURIComponent(url) : '/api' + url, { method, credentials: 'same-origin', headers: body ? { 'Content-Type': 'application/json' } : {}, body: body ? JSON.stringify(body) : undefined });
    const result = await response.json();
    if (!response.ok)
        throw new Error(result.error ?? t("B\u0142\u0105d po\u0142\u0105czenia"));
    return result;
}
const number = (v: number) => Math.round(v ?? 0).toLocaleString(locale());
const duration = (v: number) => v > 0 && v < 1000 ? "<1 s" : v > 0 && v < 60000 ? `${Math.max(1, Math.round(v / 1000))} s` : `${Math.floor((v ?? 0) / 3600000)} h ${Math.floor((v ?? 0) % 3600000 / 60000)} min`;
const date = (v: string, zone = 'Europe/Warsaw') => v ? new Intl.DateTimeFormat(locale(), { timeZone: zone, dateStyle: 'short', timeStyle: 'short' }).format(new Date(v)) : '—';
const localDate = (offset = 0) => { const d = new Date(); d.setDate(d.getDate() + offset); return new Intl.DateTimeFormat('sv-SE', { timeZone: 'Europe/Warsaw' }).format(d); };
const states: Record<string, string> = { active: 'Aktywny', afk: 'AFK', editor_open: 'Edytor otwarty', offline: 'Offline' };
const actions: Record<string, string> = { edit: 'Edycja', undo: 'Undo', redo: 'Redo', save: 'Zapis', autosave: 'Autosave', save_error: 'Błąd zapisu', autosave_error: 'Błąd autosave', start: 'Początek sesji', end: 'Koniec sesji' };
function App() {
    useLanguage();
    const [user, setUser] = useState<User | null>(null), [loaded, setLoaded] = useState(false), [error, setError] = useState(''), [catalog, setCatalog] = useState(initial), [report, setReport] = useState<any>(null), [tab, setTab] = useState('activity'), [detail, setDetail] = useState<any>(null), [note, setNote] = useState(''), [refreshed, setRefreshed] = useState('');
    const [filters, setFilters] = useState({ from: localDate(-6), to: localDate(), zone: 'Europe/Warsaw', projectId: '', mapId: '', userId: '' }), [applied, setApplied] = useState(filters);
    function fail(e: unknown) { setError(e instanceof Error ? e.message : String(e)); }
    async function catalogLoad() { setCatalog(await api('/catalog')); }
    useEffect(() => { api('/me').then(setUser).catch(() => { }).finally(() => setLoaded(true)); }, []);
    useEffect(() => { if (user)
        catalogLoad().catch(fail); }, [user]);
    useEffect(() => {
        if (!user)
            return;
        let closed = false;
        const refresh = async () => { try {
            const query = new URLSearchParams(Object.entries(applied).filter(([, v]) => !!v));
            const value = await api('/report?' + query);
            if (!closed) {
                setReport(value);
                setRefreshed(new Intl.DateTimeFormat(locale(), { hour: '2-digit', minute: '2-digit', second: '2-digit' }).format(new Date()));
                setError('');
            }
        }
        catch (e) {
            if (!closed)
                fail(e);
        } };
        void refresh();
        const timer = setInterval(refresh, 15000);
        return () => { closed = true; clearInterval(timer); };
    }, [user, applied]);
    async function submit(event: React.FormEvent<HTMLFormElement>, url: string, transform?: (data: any) => any, method = "POST") { event.preventDefault(); const form = event.currentTarget, b = Object.fromEntries(new FormData(form)); try {
        await api(url, method, transform ? transform(b) : b);
        form.reset();
        await catalogLoad();
        setError('');
    }
    catch (e) {
        fail(e);
    } }
    async function openSession(id: string) { try {
        const value = await api('/sessions/' + id);
        setDetail(value);
        setNote(value.note);
    }
    catch (e) {
        fail(e);
    } }
    if (!loaded)
        return <main className="login">{t("\u0141adowanie\u2026")}</main>;
    if (!user)
        return <main className="login"><h1>DME <span>Activity</span></h1><LanguageSwitch /><p>{t("Panel pracy mapper\u00F3w")}</p><form onSubmit={async (e) => { e.preventDefault(); try {
            await api('/login', 'POST', Object.fromEntries(new FormData(e.currentTarget)));
            setUser(await api('/me'));
            setError('');
        }
        catch (e) {
            fail(e);
        } }}><label>{t("E-mail")}<input name="email" type="email" autoComplete="username" required/></label><label>{t("Has\u0142o")}<input name="password" type="password" autoComplete="current-password" required/></label><button>{t("Zaloguj si\u0119")}</button></form>{error && <p className="error" role="alert">{t(error)}</p>}</main>;
    return <><header><div><h1>DME <span>Activity</span></h1><p>{t("Praca zespo\u0142u \u00B7") + " "}{user.name}</p></div><nav><button className={tab === 'activity' ? 'selected' : ''} onClick={() => setTab('activity')}>▦ &nbsp; Dashboard</button><button className={tab === 'settings' ? 'selected' : ''} onClick={() => setTab('settings')}>{t("\u2699 \u00A0 Po\u0142\u0105czenie")}{user.role === 'admin' ? t(" i zesp\u00F3\u0142") : ''}</button><button onClick={async () => { await api('/logout', 'POST', {}); setUser(null); setReport(null); setCatalog(initial); }}>{t("Wyloguj")}</button></nav><LanguageSwitch /></header>
 <main><div className="mobile-brand">DME Activity</div>{error && <p className="error" role="alert">{error}</p>}
 {tab === 'settings' ? <>
 <section><h2>{t("Konta mapper\u00F3w w DME")}</h2><p>{t("Administrator tworzy konto i przekazuje mapperowi e-mail oraz has\u0142o. Mapper wpisuje je w DME \u2192 View \u2192 DME Activity. \u015Aledzone mapy ustawia administrator poni\u017Cej; DME rozpoznaje je automatycznie. Adres serwera lokalnego: http://localhost:3000.")}</p><h3>{t("Urz\u0105dzenia")}</h3><div className="table"><table><thead><tr><th>{t("Nazwa")}</th><th>{t("Ostatnie po\u0142\u0105czenie")}</th><th>{t("Dost\u0119p")}</th></tr></thead><tbody>{catalog.devices.map(d => <tr key={d.id}><td>{d.name}</td><td>{date(d.last_received)}</td><td>{d.revoked ? t("Odwo\u0142any") : <button onClick={async () => { try {
            await api('/devices/' + d.id + '/revoke', 'POST', {});
            await catalogLoad();
        }
        catch (e) {
            fail(e);
        } }}>{t("Odwo\u0142aj dost\u0119p")}</button>}</td></tr>)}</tbody></table></div></section>
 {user.role === 'admin' && <>
 <div className="columns"><section><h2>{t("Dodaj mappera")}</h2><form onSubmit={e => submit(e, '/users')}><label>{t("Nazwa")}<input name="name" required maxLength={120}/></label><label>{t("E-mail")}<input type="email" name="email" autoComplete="off" required/></label><label>{t("Has\u0142o pocz\u0105tkowe")}<input type="password" name="password" autoComplete="new-password" required minLength={12}/></label><label>{t("Rola")}<select name="role"><option value="mapper">Mapper</option><option value="admin">{t("Administrator")}</option></select></label><button>{t("Utw\u00F3rz konto")}</button></form></section><section><h2>{t("Projekty i mapy")}</h2><form onSubmit={e => submit(e, '/projects')}><label>{t("Nowy projekt")}<input name="name" required maxLength={120}/></label><button>{t("Dodaj projekt")}</button></form><hr /><form onSubmit={e => submit(e, '/maps', b => ({ ...b, tracked: b.tracked === 'on' }))}><label>{t("Projekt")}<select name="projectId" required><option value="">{t("Wybierz")}</option>{catalog.projects.map(p => <option key={p.id} value={p.id}>{p.name}</option>)}</select></label><label>{t("Nazwa mapy")}<input name="name" required maxLength={120}/></label><label>{t("Nazwa pliku OTBM")}<input name="filename" placeholder="blez_1.otbm" required maxLength={255}/></label><label className="tracking-toggle"><input type="checkbox" name="tracked" defaultChecked/>{t("\u015Aled\u017A automatycznie")}</label><button>{t("Dodaj map\u0119")}</button></form></section></div>
 <section><h2>{t("\u015Aledzenie map")}</h2><p>{t("DME rozpoznaje plik automatycznie po nazwie, bez rozr\u00F3\u017Cniania wielko\u015Bci liter. Nie wpisuj \u015Bcie\u017Cki. Zmiany docieraj\u0105 do DME w ci\u0105gu 30 sekund.")}</p>{catalog.maps.map(m => <form className="tracking-rule" key={m.id + ':' + m.filename + ':' + m.tracked} onSubmit={e => submit(e, '/maps/' + m.id, b => ({ filename: b.filename, tracked: b.tracked === 'on' }), 'PATCH')}><strong>{catalog.projects.find(p => p.id === m.project_id)?.name} / {m.name}</strong><label>{t("Nazwa pliku OTBM")}<input name="filename" defaultValue={m.filename} placeholder="blez_1.otbm" required maxLength={255}/></label><label className="tracking-toggle"><input type="checkbox" name="tracked" defaultChecked={m.tracked}/>{t("\u015Aled\u017A automatycznie")}</label><button>{t("Zapisz regu\u0142\u0119")}</button>{m.tracked && catalog.maps.some(other => other.id !== m.id && other.tracked && other.filename?.toLowerCase() === m.filename?.toLowerCase()) && <small>{t("Powt\u00F3rzona nazwa: mapper maj\u0105cy dost\u0119p do obu map zobaczy konflikt.")}</small>}</form>)}</section><section><h2>{t("Dost\u0119p do projekt\u00F3w")}</h2><p>{t("Rejestracja obejmuje wy\u0142\u0105cznie mapy z przypisanych projekt\u00F3w.")}</p>{catalog.projects.map(p => <div key={p.id} className="membership"><strong>{p.name}</strong>{catalog.users.map(u => <label key={u.id}><input type="checkbox" checked={catalog.members.some(m => m.project_id === p.id && m.user_id === u.id)} onChange={async (e) => { try {
                await api('/members', 'PUT', { projectId: p.id, userId: u.id, enabled: e.target.checked });
                await catalogLoad();
            }
            catch (e) {
                fail(e);
            } }}/>{u.name}</label>)}</div>)}</section>
 </>}
 </> : <>
 <Dashboard user={user} catalog={catalog} report={report} filters={filters} setFilters={setFilters} applied={applied} setApplied={setApplied} openSession={openSession} refreshed={refreshed}/>
 </>}
 </main>{detail && <div className="backdrop"><section className="dialog" role="dialog" aria-modal="true" aria-label={t("Raport sesji")}><div className="section-title"><h2>{detail.user_name} · {detail.map_name}</h2><button onClick={() => setDetail(null)}>{t("Zamknij")}</button></div><p>{t("Aktywny czas:") + " "}{duration(detail.activeMs)}{" " + (t("\u00B7 AFK:") + " ")}{duration(detail.idleMs)}</p><p>{t("Edycje:") + " "}{number(detail.edits)} · Undo: {detail.undo} · Redo: {detail.redo}</p><p>{t("Zapisy r\u0119czne:") + " "}{detail.saves} · Autosave: {detail.autosaves}{" " + (t("\u00B7 B\u0142\u0119dy:") + " ")}{detail.saveErrors}</p><p>{t("Tempo pomocnicze:") + " "}{detail.activeMs >= 60000 ? number(detail.tiles / (detail.activeMs / 60000)) + t(" zmian tile\u2019\u00F3w/min") : t("sesja kr\u00F3tsza ni\u017C minuta")}</p><label>{t("Notatka")}<textarea maxLength={4000} value={note} onChange={e => setNote(e.target.value)}/></label><button onClick={async () => { try {
        await api('/sessions/' + detail.id + '/note', 'PATCH', { note });
        setDetail({ ...detail, note, saved: true });
        setError('');
    }
    catch (e) {
        fail(e);
    } }}>{t("Zapisz notatk\u0119")}</button>{detail.saved && <small>{" " + t("Zapisano notatk\u0119.")}</small>}<h3>{t("Operacje")}</h3><div className="timeline">{detail.events.filter((e: any) => actions[e.kind]).slice(-200).reverse().map((e: any) => <p key={e.id}><time>{date(e.at, applied.zone)}</time> {t(actions[e.kind])} {e.kind === 'edit' && `· ${number(e.tiles)} ${t("tile’ów")}`}</p>)}</div><small>{t("Ostatnie 200 operacji. Zapisane liczniki nie podlegaj\u0105 edycji.")}</small></section></div>}</>;
}
createRoot(document.getElementById('root')!).render(<App />);
