// gen-static.mjs — regenerates SEO files from catalog.json.
// Runs automatically as part of `pnpm build` (see package.json).
//   public/sitemap.xml  — homepage + one URL per pedal
//   public/robots.txt   — allow all + sitemap pointer
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const catalog = JSON.parse(readFileSync(join(root, 'src/data/catalog.json'), 'utf8'));
const site = catalog.site.url.replace(/\/$/, '');
const today = new Date().toISOString().slice(0, 10);

const urls = [
  { loc: `${site}/`, priority: '1.0' },
  ...catalog.pedals.map((p) => ({ loc: `${site}/pedals/${p.slug}`, priority: '0.8' }))
];

const sitemap = `<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
${urls.map((u) => `  <url><loc>${u.loc}</loc><lastmod>${today}</lastmod><priority>${u.priority}</priority></url>`).join('\n')}
</urlset>
`;

const robots = `User-agent: *
Allow: /

Sitemap: ${site}/sitemap.xml
`;

mkdirSync(join(root, 'public'), { recursive: true });
writeFileSync(join(root, 'public/sitemap.xml'), sitemap);
writeFileSync(join(root, 'public/robots.txt'), robots);
console.log(`sitemap.xml (${urls.length} urls) + robots.txt regenerated for ${site}`);
