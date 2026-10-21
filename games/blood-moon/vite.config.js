import { defineConfig } from 'vite';

// Strict CSP is injected for production builds only (dev server needs inline HMR).
const csp = {
  name: 'inject-csp',
  apply: 'build',
  transformIndexHtml: (html) =>
    html.replace(
      '<!--csp-->',
      `<meta http-equiv="Content-Security-Policy" content="default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data: blob:; connect-src 'self'; media-src 'self' blob:; object-src 'none'; base-uri 'none'; form-action 'none'">`
    ),
};

export default defineConfig({
  base: './',
  plugins: [csp],
  build: { target: 'es2020', chunkSizeWarningLimit: 800 },
  test: { include: ['test/**/*.test.js'] },
});
