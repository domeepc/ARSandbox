<script>
	import { onMount } from 'svelte';
	import Code from './Code.svelte';
	import { TARGETS, RELEASES_URL, latestRelease, detectTarget, isLinux, formatSize } from './release.js';

	let release = $state(/** @type {import('./release.js').Release | null} */ (null));
	let error = $state('');
	let selected = $state('ubuntu');
	let linux = $state(true);

	onMount(() => {
		selected = detectTarget();
		linux = isLinux();
		latestRelease().then(
			(r) => (release = r),
			(e) => (error = e.message)
		);
	});

	const target = $derived(TARGETS.find((t) => t.id === selected) ?? TARGETS[0]);
	const asset = $derived(release?.assets.find((a) => target.match(a.name)));
</script>

{#if !linux}
	<p class="note">
		The sandbox runs on <strong>Linux</strong> only — these downloads are for the machine wired
		to the Kinect and projector, not necessarily the one you are browsing from.
	</p>
{/if}

<div class="tabs" role="tablist">
	{#each TARGETS as t (t.id)}
		<button
			role="tab"
			aria-selected={selected === t.id}
			class:active={selected === t.id}
			onclick={() => (selected = t.id)}>{t.label}</button
		>
	{/each}
</div>

<div class="panel" role="tabpanel">
	{#if error}
		<p>
			Could not reach GitHub ({error}). Grab the files from the
			<a href={RELEASES_URL}>releases page</a> instead.
		</p>
	{:else if !release}
		<p class="muted">Looking up the latest release…</p>
	{:else if !asset}
		<p>
			{release.tag_name} has no build for this target. See the
			<a href={release.html_url}>release page</a>.
		</p>
	{:else}
		<div class="asset">
			<a class="download" href={asset.browser_download_url}>
				Download {asset.name}
			</a>
			<span class="muted">{release.tag_name} · {formatSize(asset.size)}</span>
		</div>
		<p>Then, from the folder you downloaded it to:</p>
		<Code code={target.install(asset.name)} />
		{#if target.id === 'source'}
			<p class="muted">
				Installs the build dependencies, builds Vrui 8.0 and its Kinect package if they are not
				already present, then the sandbox and the control panel. Adapts to whichever Qt and library
				versions your distribution ships — the path to take on Ubuntu 22.04, which falls between
				the two prebuilt <code>.deb</code>s.
			</p>
		{:else}
			<p class="muted">
				Bundles the whole app — Vrui, the Kinect package, the sandbox and the control panel —
				under <code>/opt/arsandbox</code>, with <code>arsandbox</code> and
				<code>sandbox-control</code> on your PATH. It will not install on a release older than
				the one listed.
			</p>
		{/if}
	{/if}
</div>

<style>
	.note {
		padding: 0.75rem 1rem;
		border-left: 3px solid var(--accent);
		background: var(--surface);
		border-radius: 4px;
	}
	.tabs {
		display: flex;
		flex-wrap: wrap;
		gap: 0.5rem;
		margin-top: 1rem;
	}
	.tabs button {
		font: inherit;
		font-size: 0.9rem;
		padding: 0.5rem 0.9rem;
		border-radius: 999px;
		border: 1px solid var(--border);
		background: transparent;
		color: var(--text);
		cursor: pointer;
	}
	.tabs button.active {
		background: var(--accent);
		border-color: var(--accent);
		color: var(--on-accent);
	}
	.panel {
		margin-top: 1rem;
		padding: 1.25rem;
		border: 1px solid var(--border);
		border-radius: 12px;
		background: var(--surface);
	}
	.asset {
		display: flex;
		flex-wrap: wrap;
		align-items: center;
		gap: 0.75rem;
	}
	.download {
		display: inline-block;
		padding: 0.7rem 1.1rem;
		border-radius: 8px;
		background: var(--accent);
		color: var(--on-accent);
		font-weight: 600;
		text-decoration: none;
		word-break: break-all;
	}
	.download:hover {
		filter: brightness(1.08);
	}
	.muted {
		color: var(--muted);
		font-size: 0.9rem;
	}
</style>
