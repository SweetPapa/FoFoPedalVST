# Sweet Papa Pedals — website

Static marketing + download site for the pedal suite. Quasar (Vue 3) SPA with
per-page SEO meta, JSON-LD structured data, sitemap and OpenGraph baked in.

## Everything lives in one file

**`src/data/catalog.json`** is the entire catalog. To…

- **Change download links**: paste your Google Drive (or any) URLs into
  `bundle.downloads.mac` / `bundle.downloads.win`. While a link still says
  `PASTE_…`, the site shows a disabled "coming right up" button instead.
- **Add a pedal**: copy any entry in `pedals[]`, change `slug` (becomes the
  URL `/pedals/<slug>`), name, tagline, copy, controls, modes, keywords,
  accent color. That's it — card, detail page, sitemap and structured data
  are all generated from it.
- **Remove a pedal**: delete its entry.
- **Mark one premium later**: set `"status": "premium"` (badge changes from
  FREE automatically; wire pricing/links when that day comes).
- **Change the domain**: edit `site.url` (canonical URLs, sitemap and robots
  all follow).

Then rebuild (below) and re-upload `dist/spa/`.

## Develop / build

```bash
cd site
pnpm install
pnpm dev      # live dev server
pnpm build    # → dist/spa/  (regenerates sitemap.xml/robots.txt, adds 404.html fallback)
```

## Deploy (any static host)

Upload the contents of `dist/spa/` to Netlify, Cloudflare Pages, GitHub Pages,
Vercel, or any static bucket. The site uses clean history-mode URLs
(`/pedals/vroom`), so the host needs an SPA fallback:

- **Netlify / Cloudflare Pages / Vercel** — automatic, nothing to do.
- **GitHub Pages** — automatic via the generated `404.html`.
- **Plain nginx/S3** — point 404/fallback at `index.html`.

## SEO checklist (already wired)

- Unique `<title>`/description/canonical per page, OG + Twitter cards,
  `SoftwareApplication` JSON-LD per pedal (free-price offers — eligible for
  rich results), `sitemap.xml` + `robots.txt` regenerated from the catalog
  on every build.
- After deploying: submit the sitemap in [Google Search Console](https://search.google.com/search-console)
  and [Bing Webmaster Tools](https://www.bing.com/webmasters) once — biggest
  single findability win.
- Real-world discovery for free plugins is dominated by directories and
  communities, not search alone: list the suite on **KVR Audio** (free
  developer account), and post launch threads to r/WeAreTheMusicMakers,
  r/edmproduction, Gearspace free-plugins threads, and Audio Plugin Guy.
  Every one of those is also a backlink that lifts the site's ranking.
- `public/og.png` is the social-share card (1200×630). Replace it any time.
