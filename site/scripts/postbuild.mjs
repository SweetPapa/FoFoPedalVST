// postbuild.mjs — copies dist/spa/index.html → 404.html so deep links
// (/pedals/vroom) work on static hosts that serve 404.html for unknown
// paths (GitHub Pages). Netlify/Cloudflare Pages don't need it but it's
// harmless there.
import { copyFileSync, existsSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const dist = join(dirname(fileURLToPath(import.meta.url)), '..', 'dist', 'spa');
if (existsSync(join(dist, 'index.html'))) {
  copyFileSync(join(dist, 'index.html'), join(dist, '404.html'));
  console.log('404.html SPA fallback written');
}
