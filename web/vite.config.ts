/// <reference types="vitest/config" />
import { defineConfig, type Plugin } from 'vite';
import { readFile } from 'node:fs/promises';
import { resolve, normalize } from 'node:path';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);

const samplesDir = resolve(import.meta.dirname, '../samples');

// Dev only: serve ../samples so the app can run in a plain browser with the
// mock bridge (http://localhost:5173/?file=showcase.md).
function samples(): Plugin {
  return {
    name: 'omnimark-samples',
    apply: 'serve',
    configureServer(server) {
      server.middlewares.use('/samples/', async (req, res, next) => {
        const rel = decodeURIComponent((req.url ?? '').split('?')[0]);
        const file = normalize(resolve(samplesDir, '.' + rel));
        if (!file.startsWith(samplesDir)) return next();
        try {
          res.end(await readFile(file));
        } catch {
          next();
        }
      });
    },
  };
}

// The CSP allows WebSocket connections only under `vite dev` (for HMR).
function devCsp(): Plugin {
  return {
    name: 'omnimark-dev-csp',
    apply: 'serve',
    transformIndexHtml: (html) => html.replace("connect-src 'self' omnimark:", "connect-src 'self' omnimark: ws:"),
  };
}

export default defineConfig({
  base: '/',
  plugins: [samples(), devCsp()],
  resolve: {
    // These packages ship "browser" builds that need document/DOMParser, which
    // don't exist in the render worker. Their default builds work everywhere.
    alias: Object.fromEntries(
      ['decode-named-character-reference', 'hast-util-from-html-isomorphic'].map((pkg) => [
        pkg,
        require.resolve(pkg, { paths: [require.resolve('rehype-katex')] }),
      ]),
    ),
  },
  worker: { format: 'es' },
  build: {
    target: 'chrome120',
    outDir: 'dist',
    emptyOutDir: true,
    chunkSizeWarningLimit: 4096,
  },
  test: {
    include: ['tests/unit/**/*.test.ts'],
  },
});
