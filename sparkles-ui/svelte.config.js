import adapter from '@sveltejs/adapter-static';

/** @type {import('@sveltejs/kit').Config} */
const config = {
  kit: {
    adapter: adapter({
      pages: 'build',
      assets: 'build',
      fallback: 'index.html',
      precompress: false,
      strict: true
    }),
    prerender: {
      entries: [
        '/',
        '/battery',
        '/settings',
        '/animations',
        '/midi',
        '/darkroom',
        '/calibration'
      ]
    }
  }
};

export default config;
