// The small things around the page: the departure board in the top bar, the conductor's punch on a ticket, the
// size, version and date of the downloads, and the demo song's player.
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

  // the demo song's clip (_includes/player.html): the key plays and pauses; the groove shows how far the song has
  // got, and a click, a drag or the arrow keys move in it. The song is only fetched once it is asked for.
  const groove = document.querySelector('.groove-player');
  if (groove) {
    const audio = groove.querySelector('audio');
    const key = groove.querySelector('.keycap');
    const use = key.querySelector('use');
    const wave = groove.querySelector('.wave');
    const bars = [...wave.children];
    const now = groove.querySelector('.now');
    const length = parseFloat(groove.dataset.seconds) || 1;
    const clock = (s) => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, '0')}`;
    let shown = -1;
    let wanted = null;   // a place chosen before the song has loaded
    const show = (s) => {
      const lit = Math.round(s / length * bars.length);
      if (lit !== shown) { bars.forEach((b, i) => b.classList.toggle('done', i < lit)); shown = lit; }
      now.textContent = clock(s);
      wave.setAttribute('aria-valuenow', Math.round(s));
      wave.setAttribute('aria-valuetext', clock(s));
    };
    const go = (s) => {
      s = Math.min(Math.max(s, 0), length);
      if (audio.readyState >= 1) audio.currentTime = s; else wanted = s;
      show(s);
    };
    const pressed = (on) => {
      key.classList.toggle('on', on);
      use.setAttribute('href', on ? '#i-pause' : '#i-play');
      const label = on ? key.dataset.pause : key.dataset.play;
      key.setAttribute('aria-label', label);
      key.title = label;
    };
    audio.addEventListener('loadedmetadata', () => { if (wanted !== null) { audio.currentTime = wanted; wanted = null; } });
    // the video goes with the sound: playing starts it, pausing stops it (until then it loops silently)
    const video = groove.closest('.clip') && groove.closest('.clip').querySelector('video');
    audio.addEventListener('play', () => { pressed(true); if (video) video.play().catch(() => {}); });
    audio.addEventListener('pause', () => { pressed(false); if (video) video.pause(); });
    audio.addEventListener('timeupdate', () => show(audio.currentTime));
    audio.addEventListener('ended', () => { pressed(false); go(0); });
    key.addEventListener('click', () => { if (audio.paused) audio.play(); else audio.pause(); });
    const at = (e) => { const r = wave.getBoundingClientRect(); return (e.clientX - r.left) / r.width * length; };
    wave.addEventListener('pointerdown', (e) => { wave.setPointerCapture(e.pointerId); go(at(e)); });
    wave.addEventListener('pointermove', (e) => { if (wave.hasPointerCapture(e.pointerId)) go(at(e)); });
    wave.addEventListener('keydown', (e) => {
      const here = wanted ?? audio.currentTime;
      const to = { ArrowLeft: here - 5, ArrowRight: here + 5, Home: 0, End: length }[e.key];
      if (to !== undefined) { e.preventDefault(); go(to); }
      else if (e.key === ' ' || e.key === 'Enter') { e.preventDefault(); key.click(); }
    });
  }
})();
