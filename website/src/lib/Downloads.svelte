<script>
	import { onMount } from 'svelte';
	import { TARGETS, REPO_URL, RELEASES_URL, detectTarget, isLinux, formatSize } from './release.js';

	/** @type {{ release: import('./release.js').Release | null, error: string }} */
	let { release, error } = $props();

	let selected = $state('ubuntu');
	let linux = $state(true);
	let copied = $state(false);

	onMount(() => {
		selected = detectTarget();
		linux = isLinux();
	});

	const target = $derived(TARGETS.find((t) => t.id === selected) ?? TARGETS[0]);
	const asset = $derived(release?.assets.find((a) => target.match(a.name)));
	const lines = $derived(target.git ? target.lines('') : asset ? target.lines(asset.name) : []);

	async function copy() {
		await navigator.clipboard.writeText(lines.join('\n'));
		copied = true;
		setTimeout(() => (copied = false), 1600);
	}
</script>

{#if !linux}
	<p class="notice">
		The sandbox runs on <strong>Linux</strong> only — these downloads are for the machine wired to the
		Kinect and projector, not necessarily the one you are browsing from.
	</p>
{/if}

<div class="grid">
	<div class="tabs" role="tablist" aria-label="Distribution">
		{#each TARGETS as t (t.id)}
			<button
				role="tab"
				aria-selected={selected === t.id}
				class:active={selected === t.id}
				onclick={() => ((selected = t.id), (copied = false))}
			>
				<span class="label">{t.label}</span>
				<span class="kind mono">{t.kind}</span>
			</button>
		{/each}
	</div>

	<div class="panel" role="tabpanel">
		{#if target.git}
			<div class="head">
				<div class="file">
					<svg width="40" height="40" viewBox="0 0 40 40" aria-hidden="true">
						<rect x="4" y="4" width="32" height="32" rx="6" fill="#161C19" stroke="#3A4741" stroke-width="1.4" />
						<g fill="none" stroke="#E3B25B" stroke-width="1.4" stroke-linecap="round">
							<path d="M14 12v16M14 17c0 4 12 2 12 7" />
						</g>
						<circle cx="14" cy="12" r="2.2" fill="#161C19" stroke="#E3B25B" stroke-width="1.4" />
						<circle cx="14" cy="28" r="2.2" fill="#161C19" stroke="#E3B25B" stroke-width="1.4" />
						<circle cx="26" cy="24" r="2.2" fill="#5EC2C9" />
					</svg>
					<div>
						<div class="name mono">{REPO_URL.replace('https://', '')}</div>
						<div class="meta mono">git clone · master</div>
					</div>
				</div>
				<a class="ghost" href={REPO_URL}>View on GitHub</a>
			</div>

			<div class="body">
				<div class="eyebrow mono">In the folder you want it in</div>
				<div class="term">
					{#each lines as line}
						<div class="mono"><span class="prompt">{'$ '}</span>{line}</div>
					{/each}
					<button class="copy mono" onclick={copy}>{copied ? 'Copied' : 'Copy'}</button>
				</div>
				<p class="note">{target.note}</p>
			</div>
		{:else if error || (release && !asset)}
			<div class="body">
				<p class="note">
					{error ? `Could not reach GitHub (${error}).` : `${release?.tag_name} has no build for this target.`}
					Grab the files from the <a href={RELEASES_URL}>releases page</a> instead.
				</p>
			</div>
		{:else if !release || !asset}
			<div class="body"><p class="note mono">Looking up the latest release…</p></div>
		{:else}
			<div class="head">
				<div class="file">
					<svg width="40" height="40" viewBox="0 0 40 40" aria-hidden="true">
						<path d="M10 4h14l8 8v24H10z" fill="#161C19" stroke="#3A4741" stroke-width="1.4" />
						<path d="M24 4v8h8" fill="none" stroke="#3A4741" stroke-width="1.4" />
						<path d="M15 22c2-3 5-4 8-3s4 3 3 5-4 3-7 2-5-2-4-4z" fill="none" stroke="#E3B25B" stroke-width="1.2" />
						<circle cx="20.5" cy="23" r="1.4" fill="#5EC2C9" />
					</svg>
					<div>
						<div class="name mono">{asset.name}</div>
						<div class="meta mono">{formatSize(asset.size)} · {release.tag_name} · signed</div>
					</div>
				</div>
				<a class="btn" href={asset.browser_download_url}>
					<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 4v11M7 10l5 5 5-5M5 20h14" /></svg>
					Download
				</a>
			</div>

			<div class="body">
				<div class="eyebrow mono">Then, in the download folder</div>
				<div class="term">
					{#each lines as line}
						<div class="mono"><span class="prompt">{'$ '}</span>{line}</div>
					{/each}
					<button class="copy mono" onclick={copy}>{copied ? 'Copied' : 'Copy'}</button>
				</div>
				<p class="note">{target.note}</p>
			</div>

			<div class="verify">
				<span class="verify-label">
					<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#5EC2C9" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3l7 3v5c0 5-3 8-7 10-4-2-7-5-7-10V6z" /><path d="M9 12l2 2 4-4" /></svg>
					Verify first
				</span>
				<code class="mono">gpg --verify SHA256SUMS.asc SHA256SUMS &amp;&amp; sha256sum -c --ignore-missing SHA256SUMS</code>
			</div>
		{/if}
	</div>
</div>

<style>
	.notice {
		margin: 0 0 24px;
		padding: 14px 18px;
		border: 1px solid var(--line);
		border-radius: 6px;
		color: var(--text-2);
	}
	.grid {
		display: grid;
		grid-template-columns: minmax(0, 280px) minmax(0, 1fr);
		gap: 32px;
		align-items: start;
	}
	.tabs {
		display: flex;
		flex-direction: column;
		gap: 8px;
	}
	.tabs button {
		text-align: left;
		min-height: 64px;
		padding: 12px 16px;
		border-radius: 6px;
		cursor: pointer;
		font: inherit;
		background: transparent;
		border: 1px solid var(--line);
		color: var(--text-2);
		transition:
			border-color 0.2s,
			background 0.2s;
	}
	.tabs button:hover {
		border-color: var(--sand);
	}
	.tabs button.active {
		background: #1a2420;
		border-color: var(--sand);
		color: var(--text-1);
	}
	.label {
		display: block;
		font-weight: 600;
		font-size: 15px;
	}
	.kind {
		display: block;
		font-size: 12px;
		color: var(--muted);
		margin-top: 2px;
	}
	.panel {
		background: var(--ground-2);
		border: 1px solid var(--line);
		border-radius: 6px;
		overflow: hidden;
	}
	.head {
		display: flex;
		flex-wrap: wrap;
		align-items: center;
		justify-content: space-between;
		gap: 16px;
		padding: 24px 28px;
		border-bottom: 1px solid var(--line);
	}
	.file {
		display: flex;
		align-items: center;
		gap: 16px;
		min-width: 0;
	}
	.file > div {
		min-width: 0;
	}
	.name {
		font-size: 15px;
		color: var(--text-1);
		overflow-wrap: anywhere;
	}
	.meta {
		font-size: 12px;
		color: var(--muted);
	}
	.body {
		padding: 24px 28px 28px;
	}
	.eyebrow {
		font-size: 12px;
		letter-spacing: 0.1em;
		text-transform: uppercase;
		color: var(--muted);
		margin-bottom: 10px;
	}
	.term {
		position: relative;
		background: #0a0e0c;
		border: 1px solid var(--line);
		border-radius: 6px;
		padding: 18px 110px 18px 20px;
		overflow-x: auto;
	}
	.term .mono {
		font-size: 14px;
		line-height: 1.9;
		white-space: pre;
	}
	.prompt {
		color: var(--sand);
		user-select: none;
	}
	.copy {
		position: absolute;
		top: 10px;
		right: 10px;
		min-height: 36px;
		padding: 0 12px;
		background: #161c19;
		border: 1px solid var(--line-2);
		border-radius: 6px;
		color: var(--text-2);
		font-size: 12px;
		cursor: pointer;
		transition:
			border-color 0.2s,
			color 0.2s;
	}
	.copy:hover {
		border-color: var(--sand);
		color: var(--text-1);
	}
	.note {
		margin: 16px 0 0;
		font-size: 15px;
		color: var(--muted);
		max-width: 70ch;
	}
	.body > .note:first-child {
		margin: 0;
	}
	.verify {
		border-top: 1px solid var(--line);
		padding: 20px 28px;
		display: flex;
		flex-wrap: wrap;
		gap: 12px 32px;
		align-items: center;
	}
	.verify-label {
		display: flex;
		align-items: center;
		gap: 10px;
		font-size: 14px;
		color: var(--text-2);
	}
	.verify code {
		font-size: 13px;
		color: var(--muted);
		overflow-wrap: anywhere;
	}
	@media (max-width: 900px) {
		.grid {
			grid-template-columns: minmax(0, 1fr);
		}
		.term {
			padding-right: 20px;
			padding-top: 52px;
		}
	}
</style>
