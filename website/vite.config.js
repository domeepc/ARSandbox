import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
	plugins: [sveltekit()],
	// The screenshots live in ../docs/images and are imported from there, so
	// the site never carries a second copy that drifts from the README's.
	server: { fs: { allow: ['..'] } }
});
