<script>
	/** @type {{ code: string }} */
	let { code } = $props();
	let copied = $state(false);

	async function copy() {
		await navigator.clipboard.writeText(code);
		copied = true;
		setTimeout(() => (copied = false), 1500);
	}
</script>

<div class="code">
	<pre><code>{code}</code></pre>
	<button onclick={copy} aria-label="Copy to clipboard">{copied ? 'Copied' : 'Copy'}</button>
</div>

<style>
	.code {
		position: relative;
		margin: 0.75rem 0;
	}
	pre {
		margin: 0;
		padding: 1rem 5rem 1rem 1rem;
		background: var(--code-bg);
		border: 1px solid var(--border);
		border-radius: 8px;
		overflow-x: auto;
		font-size: 0.9rem;
		line-height: 1.5;
	}
	button {
		position: absolute;
		top: 0.5rem;
		right: 0.5rem;
		font: inherit;
		font-size: 0.8rem;
		padding: 0.25rem 0.6rem;
		border-radius: 6px;
		border: 1px solid var(--border);
		background: var(--surface);
		color: var(--text);
		cursor: pointer;
	}
	button:hover {
		border-color: var(--accent);
	}
</style>
