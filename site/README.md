# The Nachtzug website

A one-page site in the program's own look and the manual's type.
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
| `index.html` | the page: the cover, the journey (drawn as the manual's timetable), the manual, the tickets, the questions |
| `_data/stations.yml` | the stops of the journey (one per part of the program) |
| `_data/tickets.yml` | the download tickets |
| `_data/faq.yml` | the questions |
| `assets/css/style.scss` | the look: Bakelite colours, screwed wells, keys; Ember on dark systems |
| `assets/js/site.js` | the ticker, the station you're reading, the tickets' punch, the download sizes |
| `_includes/flourish.svg` | the manual's chapter flourish (from `docs/manual/build.py`), drawn once and reused |
| `assets/img/shots/` | pictures of the program, taken by `tools/site_shots.py` |
| `assets/img/cover-*.jpg` | the manuals' front pages, drawn from the PDFs by `tools/site_covers.sh` |
| `assets/img/social-preview.png` | the card for shared links, drawn by `tools/logo/social_preview.py` |
| `assets/fonts/` | the Nachtzug fonts as WOFF2, made by `tools/site_fonts.py` |
| `downloads/` | filled by the builds: the manuals, the AppImage, the source code and `latest.json`; not in git |

On GitHub the downloads live on the releases page, put there by `.github/workflows/release.yml` for every version
tag, so a change to the site never touches them.
