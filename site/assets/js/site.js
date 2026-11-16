// The small things around the page: the departure board in the top bar, the conductor's punch on a ticket, and the
// size, version and date of the downloads.
(() => {
  'use strict';
  // the departure board: its lines, in the page's language, are in data-lines
  const board = document.querySelector('.ticker');
  const ticker = board && board.querySelector('.ticker-text');
  const lines = board ? (board.dataset.lines || '').split('|').filter(Boolean) : [];
  let k = 0;
  if (ticker && lines.length > 1) ticker.addEventListener('animationiteration', () => { k = (k + 1) % lines.length; ticker.textContent = lines[k]; });


  // your ticket: the one for the computer you are reading this on gets a tag and the lit key, and the key at the top
  // names it. Nothing for a phone or a tablet, where none of them runs; a Mac can't say whether it has Apple Silicon,
  // so its ticket says so itself.
  const ua = navigator.userAgent;
  const pf = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || '';
  let system = null;
  if (/Android|iPhone|iPad|iPod|CrOS/i.test(ua)) system = null;
  else if (/^Win/i.test(pf) || /Windows/.test(ua)) system = 'windows';
  else if (/^Mac/i.test(pf) || /Mac OS X/.test(ua)) system = navigator.maxTouchPoints > 1 ? null : 'mac';   // an iPad says Mac too
  else if (/Linux|X11/i.test(pf + ' ' + ua)) system = 'linux';
  const mine = system && document.getElementById(`ticket-${system}`);
  if (mine) {
    mine.classList.add('mine');
    const key = mine.querySelector('.keycap.punchme');
    if (key) { key.classList.remove('primary'); key.classList.add('lit'); }
    const cta = document.querySelector('.hero .cta a[data-for]');
    if (cta) {
      cta.href = `#ticket-${system}`;
      const sub = cta.querySelector('small');
      if (sub) sub.textContent = cta.dataset.for.replace('%system%', mine.dataset.system);
    }
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
  const base = (document.querySelector('meta[name="nz-base"]') || {}).content || '/';
  const fromGitHub = (rel) => {
    if (!rel || !rel.assets) return null;
    const d = new Date(rel.published_at || rel.created_at);
    const files = {};
    rel.assets.forEach((a) => { files[a.name] = { size: a.size }; });
    return { version: rel.tag_name, date: `${d.getUTCDate()} ${months[d.getUTCMonth()].replace(/^./, (c) => c.toUpperCase())} ${d.getUTCFullYear()}`, files };
  };
  const source = api ? fetch(api.content, { headers: { Accept: 'application/vnd.github+json' } }).then((r) => r.ok ? r.json() : null).then(fromGitHub)
                     : fetch(`${base}downloads/latest.json`, { cache: 'no-cache' }).then((r) => r.ok ? r.json() : null);
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

// the animation of the program playing: still, with its controls, for those whose computer asks for less motion
(() => {
  const v = document.querySelector('.playing video');
  if (v && window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
    v.removeAttribute('autoplay');
    v.pause();
    v.controls = true;
  }
})();

// the licences page: a link straight to a licence text opens it
(function () {
  function openTarget() {
    var el = location.hash && document.getElementById(location.hash.slice(1));
    if (el && el.tagName === 'DETAILS') el.open = true;
  }
  openTarget();
  window.addEventListener('hashchange', openTarget);
})();
