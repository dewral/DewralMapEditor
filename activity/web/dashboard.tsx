import React, { useState } from 'react';
import { t, locale, useLanguage, LanguageSwitch } from './i18n';
export const number = (v: number) => Math.round(v ?? 0).toLocaleString(locale());
export const duration = (v: number) => v > 0 && v < 60000 ? '<1 min' : `${Math.floor((v ?? 0) / 3600000)} h ${Math.floor((v ?? 0) % 3600000 / 60000)} min`;
const date = (v: string, zone = 'Europe/Warsaw') => v ? new Intl.DateTimeFormat(locale(), { timeZone: zone, dateStyle: 'short', timeStyle: 'short' }).format(new Date(v)) : '—';
const states: Record<string, string> = { active: 'Aktywny', afk: 'AFK', editor_open: 'Edytor otwarty', offline: 'Offline' };
const tileHint = 'Suma pól dotkniętych edycjami. Ponowne zmiany tego samego pola są liczone ponownie; Undo i Redo są osobno.';
const ratio = (active: number, idle: number) => (active + idle) > 0 ? Math.round(idle / (active + idle) * 100) : 0;
function Avatar({ name }: {
    name: string;
}) { return <span className="avatar">{name.split(/\s+/).slice(0, 2).map(n => n[0]).join('').toUpperCase()}</span>; }
function ActivityBar({ active, idle }: {
    active: number;
    idle: number;
}) {
    const afk = ratio(active, idle);
    return <div className="time-split" title={`${t("Aktywnie:")} ${duration(active)} · AFK: ${duration(idle)}`}><div className="split-track"><i style={{ width: `${active + idle > 0 ? 100 - afk : 0}%` }}/><i style={{ width: `${afk}%` }}/></div><small>{active + idle > 0 ? `${afk}% AFK` : t("Brak czasu")}</small></div>;
}
function Chart({ rows, field, time = false, hour = false, zone }: {
    rows: any[];
    field: string;
    time?: boolean;
    hour?: boolean;
    zone: string;
}) {
    const [hover, setHover] = useState<number | null>(null);
    if (!rows.length)
        return <div className="empty"><span className="empty-symbol">▥</span>{t("Brak aktywno\u015Bci w tym okresie")}<small>{t("Dane pojawi\u0105 si\u0119 po rozpocz\u0119ciu pracy w DME.")}</small></div>;
    const max = Math.max(1, ...rows.map(r => r[field] ?? 0)), width = 710 / rows.length;
    const label = (r: any) => hour ? new Intl.DateTimeFormat(locale(), { timeZone: zone, day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit' }).format(new Date(r.label)) : r.label;
    return <div className="chart-wrap"><div className="chart-readout">{hover !== null ? <><span>{label(rows[hover])}</span><strong>{time ? duration(rows[hover][field]) : number(rows[hover][field])}</strong></> : <span>{t("Najed\u017A na s\u0142upek, aby zobaczy\u0107 szczeg\u00F3\u0142y")}</span>}</div><svg viewBox="0 0 790 215" role="img" aria-label={time ? t("Wykres czasu aktywnego") : t("Wykres zmian tile\u2019\u00F3w")} className="chart">
 {[0, .25, .5, .75, 1].map(v => <g key={v}><line x1="64" x2="778" y1={170 - v * 135} y2={170 - v * 135} stroke="#30363d" strokeDasharray={v ? '3 5' : undefined}/><text x="54" y={174 - v * 135} textAnchor="end">{time ? number(max * v / 60000) + 'm' : number(max * v)}</text></g>)}
 {rows.map((r, i) => <g key={r.label} onMouseEnter={() => setHover(i)} onMouseLeave={() => setHover(null)}><rect x={67 + i * width} y="30" width={width} height="143" fill="transparent"/><rect x={67 + i * width} y={170 - (r[field] ?? 0) / max * 135} width={Math.max(1, width * .65)} height={(r[field] ?? 0) / max * 135} rx="3" fill={hover === i ? '#81caef' : time ? '#4a9ec7' : '#66bda5'}><title>{label(r)}: {time ? duration(r[field]) : number(r[field])}</title></rect>{(rows.length < 9 || i % Math.ceil(rows.length / 8) === 0) && <text x={67 + i * width} y="196">{hour ? new Intl.DateTimeFormat(locale(), { timeZone: zone, day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit' }).format(new Date(r.label)) : r.label.slice(5).split('-').reverse().join('.')}</text>}</g>)}
 </svg></div>;
}
export function Dashboard({ user, catalog, report, filters, setFilters, applied, setApplied, openSession, refreshed }: {
    user: any;
    catalog: any;
    report: any;
    filters: any;
    setFilters: any;
    applied: any;
    setApplied: any;
    openSession: (id: string) => void;
    refreshed: string;
}) {
    useLanguage();
    const [dailyMetric, setDailyMetric] = useState('tiles'), [showSessions, setShowSessions] = useState(false);
    const total = report?.totals ?? {}, live: any[] = report?.live ?? [], priority: Record<string, number> = { active: 4, afk: 3, editor_open: 2, offline: 1 };
    // Multiple workstations do not create duplicate people in the team overview.
    const people = [...live.reduce((m: Map<string, any>, r: any) => { const old = m.get(r.id); if (!old || priority[r.status] > priority[old.status])
            m.set(r.id, r); return m; }, new Map<string, any>()).values()];
    const mappers: any[] = report?.mappers ?? [];
    const dayMap = new Map((report?.days ?? []).map((d: any) => [d.label, d]));
    const dailyRows: any[] = [];
    if (report?.days?.length) {
        const end = Date.parse(applied.to + 'T00:00:00Z');
        for (let at = Date.parse(applied.from + 'T00:00:00Z'); at <= end; at += 86400000) {
            const label = new Date(at).toISOString().slice(0, 10);
            dailyRows.push(dayMap.get(label) ?? { label, activeMs: 0, tiles: 0 });
        }
    }
    const focusMapper = (id: string) => { setFilters({ ...filters, userId: id }); setApplied({ ...applied, userId: id }); };
    const preset = (days: number) => { const end = new Date(), start = new Date(); start.setDate(end.getDate() - days + 1); const format = (d: Date) => new Intl.DateTimeFormat('sv-SE', { timeZone: filters.zone }).format(d); const next = { ...filters, from: format(start), to: format(end) }; setFilters(next); setApplied(next); };
    return <>
 <div className="page-heading"><div><div className="eyebrow">{t("WORKSPACE / AKTYWNO\u015A\u0106")}</div><h2>{t("Praca nad mapami")}</h2><p>{t("Tw\u00F3j zesp\u00F3\u0142 i jego aktywno\u015B\u0107 w jednym miejscu.")}</p></div><div className="sync-state"><span /> {refreshed ? t("Zaktualizowano ") + refreshed : t("\u0141adowanie danych\u2026")}<small>{t("Od\u015Bwie\u017Canie co 15 sekund")}</small></div></div>
 <section className="filter-panel"><div className="filter-top"><strong>{t("Zakres raportu")}</strong><div className="range-presets">{[[1, t("Dzisiaj")], [7, t("7 dni")], [30, t("30 dni")]].map(([d, l]) => <button key={d} onClick={() => preset(Number(d))}>{l}</button>)}</div></div><div className="filters">
 <label>{t("Od")}<input type="date" value={filters.from} onChange={e => setFilters({ ...filters, from: e.target.value })}/></label><label>{t("Do")}<input type="date" value={filters.to} onChange={e => setFilters({ ...filters, to: e.target.value })}/></label>
 <label>{t("Projekt")}<select value={filters.projectId} onChange={e => setFilters({ ...filters, projectId: e.target.value, mapId: '' })}><option value="">{t("Wszystkie projekty")}</option>{catalog.projects.map((p: any) => <option key={p.id} value={p.id}>{p.name}</option>)}</select></label>
 <label>{t("Mapa")}<select value={filters.mapId} onChange={e => setFilters({ ...filters, mapId: e.target.value })}><option value="">{t("Wszystkie mapy")}</option>{catalog.maps.filter((m: any) => !filters.projectId || m.project_id === filters.projectId).map((m: any) => <option key={m.id} value={m.id}>{m.name}</option>)}</select></label>
 {user.role === 'admin' && <label>Mapper<select value={filters.userId} onChange={e => setFilters({ ...filters, userId: e.target.value })}><option value="">{t("Ca\u0142y zesp\u00F3\u0142")}</option>{catalog.users.map((u: any) => <option key={u.id} value={u.id}>{u.name}</option>)}</select></label>}
 <label>{t("Strefa czasowa")}<select value={filters.zone} onChange={e => setFilters({ ...filters, zone: e.target.value })}>{['Europe/Warsaw', 'UTC', 'Europe/London', 'America/New_York'].map(z => <option key={z}>{z}</option>)}</select></label><button className="primary" onClick={() => setApplied({ ...filters })}>{t("Zastosuj")}</button></div></section>
 <div className="metrics">{[[t("Czas aktywny"), duration(total.activeMs), t("Praca w edytorze"), '◷'], [t("Czas AFK"), duration(total.idleMs), t("Po 5 min bez interakcji"), '◴'], [t("Edycje"), number(total.edits), t("Zatwierdzone operacje"), '✎'], [t("Zmiany tile\u2019\u00F3w"), number(total.tiles), t("Z ponownymi zmianami"), '▦'], [t("Zapisy"), number((total.saves ?? 0) + (total.autosaves ?? 0)), `${number(total.saves)} ${t("ręcznych")} · ${number(total.autosaves)} auto`, '▣'], [t("B\u0142\u0119dy zapisu"), number(total.saveErrors), t("Wymagaj\u0105 sprawdzenia"), '!']].map(([label, value, hint, icon], i) => <section className={'metric metric-' + i} key={label} title={i === 3 ? t(tileHint) : undefined}><div className="metric-label">{label}<span>{icon}</span></div><strong>{value}</strong><small>{hint}</small></section>)}</div>
 <section><div className="section-title"><div><h2>{t("Zesp\u00F3\u0142 na \u017Cywo")}</h2><p>{t("Status teraz \u00B7 czas aktywny dzisiaj")}</p></div><span className="live-label"><i /> {people.filter(p => p.status === 'active').length}{" " + t("aktywnych")}</span></div><div className="team-grid">{people.map(r => <div className="person-card" key={r.id}><div className="person-heading"><Avatar name={r.name}/><button className="text-button person-name" onClick={() => focusMapper(r.id)}>{r.name}</button><span className={'badge ' + r.status}>{t(states[r.status] ?? r.status)}</span></div><div className="person-map">{r.map ?? t("Bez aktywnej mapy")}</div><div className="person-time"><strong>{duration(r.todayActiveMs)}</strong><small>{t("dzisiaj aktywnie")}</small></div><div className="person-events"><span>{t("Ostatnia edycja")}<b>{date(r.last_edit, applied.zone)}</b></span><span>{t("Ostatni zapis")}<b>{date(r.last_save, applied.zone)}</b></span></div></div>)}</div>{!people.length && <div className="empty">{t("Brak mapper\u00F3w dla wybranych filtr\u00F3w.")}</div>}</section>
 <section><div className="section-title"><div><h2>{t("Czas pracy mapper\u00F3w")}</h2><p>{t("Aktywno\u015B\u0107 w wybranym okresie \u00B7 kliknij osob\u0119, aby zaw\u0119zi\u0107 raport")}</p></div><div className="legend"><span><i />{t("Aktywnie")}</span><span><i />AFK</span></div></div><div className="table"><table className="mapper-table"><thead><tr><th>Mapper</th><th>{t("\u0141\u0105czny czas")}</th><th>{t("Aktywnie")}</th><th>AFK</th><th>{t("Podzia\u0142 czasu")}</th><th title={t(tileHint)}>{t("Zmiany tile\u2019\u00F3w")}</th><th>{t("Edycje")}</th><th>{t("Zapisy")}</th><th>{t("Sesje")}</th><th>{t("Najaktywniejszy dzie\u0144")}</th></tr></thead><tbody>{mappers.map(r => <tr key={r.id}><td><button className="mapper-link" onClick={() => focusMapper(r.id)}><Avatar name={r.name}/>{r.name}</button></td><td>{duration(r.activeMs + r.idleMs)}</td><td className="active-number">{duration(r.activeMs)}</td><td>{duration(r.idleMs)}</td><td><ActivityBar active={r.activeMs} idle={r.idleMs}/></td><td>{number(r.tiles)}</td><td>{number(r.edits)}</td><td title={`${r.saves} ${t("ręcznych")} / ${r.autosaves} autosave`}>{number(r.saves + r.autosaves)}</td><td>{r.sessions}</td><td>{r.mostActiveDay ? <>{r.mostActiveDay.label}<small>{duration(r.mostActiveDay.activeMs)}{" " + t("aktywnie")}</small></> : '—'}</td></tr>)}</tbody></table></div>{!mappers.length && <div className="empty">{t("Brak zarejestrowanej pracy w tym okresie.")}</div>}<div className="panel-footnote">{t("Liczby opisuj\u0105 aktywno\u015B\u0107, nie jako\u015B\u0107 mapowania. Zmiany tile\u2019\u00F3w obejmuj\u0105 ponowne edycje tego samego pola.")}</div></section>
 <div className="columns"><section><div className="section-title"><div><h2>{t("Rytm pracy")}</h2><p>{t("Aktywny czas wed\u0142ug godzin")}</p></div><span className="chart-unit">{t("MINUTY")}</span></div><Chart rows={report?.hours ?? []} field="activeMs" time hour zone={applied.zone}/></section><section><div className="section-title"><div><h2>{t("Aktywno\u015B\u0107 dzie\u0144 po dniu")}</h2><p>{dailyMetric === 'tiles' ? t("Zmiany tile\u2019\u00F3w w wybranym okresie") : t("Czas aktywny w wybranym okresie")}</p></div><div className="segmented"><button className={dailyMetric === 'tiles' ? 'selected' : ''} onClick={() => setDailyMetric('tiles')}>{t("Tile\u2019e")}</button><button className={dailyMetric === 'activeMs' ? 'selected' : ''} onClick={() => setDailyMetric('activeMs')}>{t("Czas")}</button></div></div><Chart rows={dailyRows} field={dailyMetric} time={dailyMetric === 'activeMs'} zone={applied.zone}/></section></div>
 <section><div className="section-title"><div><h2>{t("Historia sesji") + " "}<span className="count">{report?.sessions.length ?? 0}</span></h2><p>{t("Operacje, zapisy i notatki z pracy na mapie")}</p></div><button onClick={() => setShowSessions(!showSessions)} aria-expanded={showSessions}>{showSessions ? t("Zwi\u0144 histori\u0119") : t("Poka\u017C sesje")}</button></div>{showSessions && <><div className="table"><table><thead><tr><th>{t("Mapper / mapa")}</th><th>{t("Ostatnia aktywno\u015B\u0107")}</th><th>{t("Aktywnie")}</th><th>AFK</th><th>{t("Edycje")}</th><th>{t("Tile\u2019e")}</th><th>Undo / Redo</th><th>{t("Zapisy")}</th><th /></tr></thead><tbody>{report?.sessions.map((s: any) => <tr key={s.id}><td><strong>{s.user}</strong><small>{s.project} · {s.map}</small></td><td>{date(s.last, applied.zone)}</td><td>{duration(s.activeMs)}</td><td>{duration(s.idleMs)}</td><td>{number(s.edits)}</td><td>{number(s.tiles)}</td><td>{s.undo} / {s.redo}</td><td>{s.saves + s.autosaves}</td><td><button onClick={() => openSession(s.id)}>{t("Raport \u2197")}</button></td></tr>)}</tbody></table></div>{!report?.sessions.length && <div className="empty">{t("Zaloguj si\u0119 w DME i otw\u00F3rz \u015Bledzon\u0105 map\u0119, aby rozpocz\u0105\u0107 prac\u0119.")}</div>}</>}</section>
 </>;
}
