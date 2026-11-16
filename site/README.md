# The Nachtzug website

A one-page site in the program's own look and the manual's type, in English (`/`) and German (`/de/`).
It's built with Jekyll and put on GitHub Pages by `.github/workflows/pages.yml`, with `_config.github.yml` on top of
`_config.yml` (it sets the address and points the download tickets at the latest GitHub release).

## Trying it locally

```sh
cd site
sh prepare.sh                 # the manuals, copied from docs/
bundle install
bundle exec jekyll serve      # then open http://localhost:4000
```

## What lives where

| Where | What |
|---|---|
| `_includes/home.html` | the front page: the opening, the stops as cards, pictures of the program, the manual, the tickets, the licences, the questions |
| `index.html`, `de/index.html` | the front page in English and in German: three lines each, the language and `home.html` |
| `_data/en.yml`, `_data/de.yml` | every word of the site, one file per language with the same keys in the same order |
| `_layouts/default.html` | the frame of every page; `page.lang` (`en` unless a page says `de`) picks the words |
| `_includes/topbar.html`, `footer.html` | the top bar with the language switch, the footer with its link columns |
| `_includes/icons.svg` | the cards' icons, drawn once on a 24 grid and reused |
| `impressum.html`, `datenschutz.html` | the legal pages, in German only; their switch goes to the English front page |
| `assets/css/style.scss` | the look: Bakelite colours, screwed wells, keys; Ember on dark systems |
| `assets/js/site.js` | the ticker, the tickets' punch, the download sizes, the still animation for reduced motion |
| `_includes/flourish.svg` | the manual's chapter flourish (from `docs/manual/build.py`), drawn once and reused |
| `assets/img/shots/` | pictures of the program, taken by `tools/site_shots.py` |
| `assets/img/cover-*.jpg` | the manuals' front pages, drawn from the PDFs by `tools/site_covers.sh` |
| `assets/img/social-preview.png` | the card for shared links, drawn by `tools/logo/social_preview.py` |
| `assets/fonts/` | the Nachtzug fonts as WOFF2, made by `tools/site_fonts.py` |
| `downloads/` | filled by the builds: the manuals, the AppImage, the source code and `latest.json`; not in git |

On GitHub the downloads live on the releases page, put there by `.github/workflows/release.yml` for every version
tag, so a change to the site never touches them.
