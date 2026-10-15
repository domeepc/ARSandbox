import adapter from '@sveltejs/adapter-static';

/** @type {import('@sveltejs/kit').Config} */
export default {
	kit: {
		adapter: adapter({ fallback: '404.html' }),
		// GitHub Pages serves a project site under /<repo>; BASE_PATH is set by
		// the Pages workflow and left empty for local dev.
		paths: { base: process.env.BASE_PATH ?? '' }
	}
};
