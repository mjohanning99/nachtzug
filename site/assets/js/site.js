// The small things around the page: the departure board in the top bar, the station you're reading in the
// timetable, the conductor's punch on a ticket, and the size, version and date of the downloads.
(() => {
  'use strict';
  const ticker = document.querySelector('.ticker-text');
  const lines = [
    'ABFAHRT 22:58 · GLEIS 3 · NACHTZUG NACH BERLIN SCHOOL',
    'EVERY NOTE IS IN THE SCALE · NOTHING CAN SOUND WRONG',
    'NEXT STOP: ACID JUNCTION · PLEASE MIND THE SQUELCH',
    'HOLD SHIFT FOR THE RIGHT WRONG NOTES',
    'THE DRUMMER NEVER GETS TIRED',
  ];
  let k = 0;
  if (ticker) ticker.addEventListener('animationiteration', () => { k = (k + 1) % lines.length; ticker.textContent = lines[k]; });

  // the timetable: the station you are reading lights up
  const stops = [...document.querySelectorAll('.route .stop')];
  if (stops.length && 'IntersectionObserver' in window) {
    const seen = new Map();
    const io = new IntersectionObserver((es) => {
      es.forEach((e) => seen.set(e.target, e.isIntersecting ? e.intersectionRect.top : null));
      let best = null;
      stops.forEach((s) => { const top = seen.get(s); if (top != null && (best === null || top < seen.get(best))) best = s; });
      stops.forEach((s) => s.classList.toggle('here', s === best));
    }, { rootMargin: '-35% 0px -45% 0px' });
    stops.forEach((s) => io.observe(s));
  }

  document.querySelectorAll('.ticket .punchme').forEach((a) => a.addEventListener('click', () => a.closest('.ticket').classList.add('punched')));

  // downloads/latest.json is written by the build: { "version": "…", "date": "15 November 2026", "files": { "name": { "size": … } } }.
  // Its date goes on the cards, as the dating press would print it (15.11.26); without it, they keep the day the site was built.
  const months = ['january', 'february', 'march', 'april', 'may', 'june', 'july', 'august', 'september', 'october', 'november', 'december'];
  const pressed = (date) => {
    const m = /^(\d{1,2}) (\p{L}+) (\d{4})$/u.exec(String(date || '').trim());
    const mo = m ? months.indexOf(m[2].toLowerCase()) : -1;
    return mo < 0 ? null : `${m[1].padStart(2, '0')}.${String(mo + 1).padStart(2, '0')}.${m[3].slice(2)}`;
  };
  // on GitHub Pages the downloads are the latest GitHub release: its tag, date and file sizes, read the same way
  const api = document.querySelector('meta[name="nz-release-api"]');
  const fromGitHub = (rel) => {
    if (!rel || !rel.assets) return null;
    const d = new Date(rel.published_at || rel.created_at);
    const files = {};
    rel.assets.forEach((a) => { files[a.name] = { size: a.size }; });
    return { version: rel.tag_name, date: `${d.getUTCDate()} ${months[d.getUTCMonth()].replace(/^./, (c) => c.toUpperCase())} ${d.getUTCFullYear()}`, files };
  };
  const source = api ? fetch(api.content, { headers: { Accept: 'application/vnd.github+json' } }).then((r) => r.ok ? r.json() : null).then(fromGitHub)
                     : fetch('downloads/latest.json', { cache: 'no-cache' }).then((r) => r.ok ? r.json() : null);
  source.then((info) => {
    if (!info) return;
    const day = pressed(info.date);
    if (day) document.querySelectorAll('.card [data-issued]').forEach((d) => { d.textContent = day; });
    document.querySelectorAll('.ticket[data-file]').forEach((t) => {
      const f = info.files && info.files[t.dataset.file];
      if (!f) return;
      const mb = (f.size / 1048576).toFixed(f.size > 10485760 ? 0 : 1);
      t.querySelector('[data-meta]').textContent = `${info.version} · ${mb} MB · ${info.date}`;
    });
  }).catch(() => {});
})();
