<script>
	import { onMount } from 'svelte';
	import Terrain from '$lib/Terrain.svelte';
	import Rig from '$lib/Rig.svelte';
	import Downloads from '$lib/Downloads.svelte';
	import { REPO_URL, latestRelease } from '$lib/release.js';
	import logo from '../../../packaging/arsandbox.svg';
	import panelCalibration from '../../../docs/images/panel-calibration.png';
	import panelTopography from '../../../docs/images/panel-topography.png';
	import panelWater from '../../../docs/images/panel-water.png';

	/** @type {import('$lib/release.js').Release | null} */
	let release = $state(null);
	let error = $state('');

	onMount(() => {
		latestRelease().then(
			(r) => (release = r),
			(e) => (error = e.message)
		);
	});

	const LEGEND = ['#1F5A70', '#3E7B5A', '#86A85E', '#D9B65A', '#E09F4F', '#C97E45', '#EDE6D6'];
</script>

<svelte:head>
	<title>ARSandbox — install</title>
	<meta
		name="description"
		content="Install the Augmented Reality Sandbox: a Kinect watches a box of sand and a projector paints live elevation colours, contour lines and flowing water back onto it."
	/>
</svelte:head>

{#snippet marker(/** @type {number} */ n, last = false)}
	<svg class="marker" width="56" height="56" viewBox="0 0 56 56" aria-hidden="true">
		<circle cx="28" cy="28" r="26" fill="none" stroke="#2E3934" />
		<circle cx="28" cy="28" r="19" fill="none" stroke="#3A4741" />
		<circle cx="28" cy="28" r="12" fill={last ? '#5EC2C9' : '#E3B25B'} />
		<text x="28" y="33" text-anchor="middle" font-family="IBM Plex Mono, monospace" font-size="14" font-weight="500" fill="#14110A">{n}</text>
	</svg>
{/snippet}

<header>
	<div class="wrap nav">
		<a class="brand" href="#top">
			<img src={logo} alt="" width="34" height="34" />
			<span class="disp">ARSandbox</span>
		</a>
		<nav class="links mono" aria-label="Sections">
			<a href="#rig">The rig</a>
			<a href="#install">Install</a>
			<a href="#setup">Calibrate</a>
		</nav>
		<a class="ghost mono gh" href={REPO_URL}>
			<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="6" cy="6" r="2.2" /><circle cx="6" cy="18" r="2.2" /><circle cx="18" cy="8" r="2.2" /><path d="M6 8.2v7.6M18 10.2c0 4-6 3-11 6" /></svg>
			GitHub{release ? ` · ${release.tag_name}` : ''}
		</a>
	</div>
</header>

<main>
	<section id="top" class="hero">
		<Terrain />
		<div class="wrap hero-inner">
			<div class="shelf">
				<div class="eyebrow mono rise water">
					<svg width="10" height="10" viewBox="0 0 10 10" aria-hidden="true"><circle class="led" cx="5" cy="5" r="4" fill="#5EC2C9" /></svg>
					Sheet 01 · Augmented reality sandbox
				</div>
				<h1 class="disp rise d1">Sculpt the sand.<br /><span>The map follows.</span></h1>
				<p class="lead rise d2">
					A Kinect watches a box of sand. A projector paints live elevation colours, contour lines and
					flowing water back onto it — rebuilt on SARndbox 2.8 and Vrui, with a control panel for
					calibration.
				</p>
				<div class="actions rise d3">
					<a class="btn" href="#install">
						<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 4v11M7 10l5 5 5-5M5 20h14" /></svg>
						Install for Linux
					</a>
					<a class="ghost" href="#setup">Calibration guide</a>
				</div>
			</div>
		</div>
		<div class="legend mono" aria-hidden="true">
			<div class="wrap legend-row">
				<span>ELEV</span>
				<svg width="168" height="10" viewBox="0 0 168 10">
					{#each LEGEND as c, i}<rect x={i * 24} width="24" height="10" fill={c} />{/each}
				</svg>
				<span>WATER → PEAK</span>
			</div>
		</div>
	</section>

	<section id="rig" class="alt">
		<div class="wrap rig">
			<figure><Rig /></figure>
			<div>
				<div class="eyebrow mono">The rig</div>
				<h2 class="disp">Three parts, one loop.</h2>
				<p class="intro">
					The depth camera samples the surface, the sandbox turns it into a height map and runs the
					water simulation, and the projector lays the result back over the exact grains it came from —
					every frame.
				</p>
				<ul class="reqs">
					<li>
						<svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="#E3B25B" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="4" width="18" height="16" rx="2" /><path d="M7 10l3 2-3 2M12 15h5" /></svg>
						<strong>Linux</strong><span>x86-64. Not macOS or Windows.</span>
					</li>
					<li>
						<svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="#5EC2C9" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="2" y="8" width="20" height="7" rx="3.5" /><circle cx="7" cy="11.5" r="1.4" /><circle cx="12" cy="11.5" r="1.4" /><circle cx="17" cy="11.5" r="1.4" /><path d="M12 15v4M8 19h8" /></svg>
						<strong>Kinect v1</strong><span>The Xbox 360 model.</span>
					</li>
					<li>
						<svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="#E3B25B" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="18" height="10" rx="2" /><circle cx="15" cy="12" r="2.6" /><path d="M6 10h4M6 13h4" /></svg>
						<strong>Projector</strong><span>Above the box, aimed down.</span>
					</li>
					<li>
						<svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="#5EC2C9" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="4" width="18" height="16" rx="2" /><path d="M3 8h18M7 12h10M7 16h6" /><circle cx="14" cy="12" r="1.2" fill="#5EC2C9" /></svg>
						<strong>Qt 6.4+</strong><span>Or Qt 5.9+ on older Ubuntu.</span>
					</li>
				</ul>
			</div>
		</div>
	</section>

	<section id="install">
		<div class="wrap">
			<div class="section-head">
				<div>
					<div class="eyebrow mono">Install · latest release{release ? ` ${release.tag_name}` : ''}</div>
					<h2 class="disp">Pick your ground.</h2>
				</div>
				<p class="aside">
					Packages carry the whole app under <code class="mono">/opt/arsandbox</code>. Anything they don't
					cover builds from source.
				</p>
			</div>
			<Downloads {release} {error} />
		</div>
	</section>

	<section id="setup" class="alt">
		<div class="wrap">
			<div class="eyebrow mono">Field procedure</div>
			<h2 class="disp steps-title">From cold start to calibrated.</h2>

			<ol class="steps">
				<li class="step">
					{@render marker(1)}
					<div>
						<h3 class="disp">Start it</h3>
						<p>
							Run <code class="mono">arsandbox</code> from a package, or
							<code class="mono">scripts/run-sandbox.sh</code> from a checkout. The first run says there is no
							projector calibration yet — expected until step 5, and remembered after.
						</p>
					</div>
				</li>
				<li class="step">
					{@render marker(2)}
					<div>
						<h3 class="disp">Open the control panel</h3>
						<p>
							<strong>Right-click</strong> the sandbox view. Every other key and mouse binding is switched off,
							so a visitor leaning on the keyboard can't knock the projection off the sand.
						</p>
					</div>
				</li>
				<li class="step">
					{@render marker(3)}
					<div>
						<h3 class="disp">Camera intrinsics</h3>
						<p>
							Calibration tab, step 1. Downloads the factory calibration from the camera itself — tied to
							that one Kinect's serial number.
						</p>
					</div>
				</li>
				<li class="step">
					{@render marker(4)}
					<div>
						<h3 class="disp">Base plane and sand extents</h3>
						<p>
							Grab a depth image. Lay a <strong>flat plate</strong> on the sand and drag over it, clear of the
							walls. Remove it, grab again, drag over the <strong>bare sand</strong>. Then
							<strong>Write BoxLayout.txt</strong> — the sandbox restarts to pick it up.
						</p>
						<p class="mono small">Checked for you: tilt · corner fit · opposite-side mismatch</p>
					</div>
					<div class="media shot">
						<img src={panelCalibration} alt="Calibration tab showing a depth frame of the box" loading="lazy" />
					</div>
				</li>
				<li class="step">
					{@render marker(5)}
					<div>
						<h3 class="disp">Projector alignment</h3>
						<p>
							Pick the projector, press <strong>Start</strong>, keep the box clear for two seconds. Then hold a
							disc target <strong>above</strong> the sand on the white cross and press
							<strong>Capture this point</strong> when the blue disc appears. Twelve points by default, at
							least 10 cm of height between them.
						</p>
						<p class="mono small">The RMS residual at the end is the honest score.</p>
					</div>
					<div class="media">
						<svg viewBox="0 0 420 220" role="img" aria-label="A CD target with a marked centre held above the sand, lined up with the projected white cross, the detected disc outlined in blue.">
							<rect x="0.5" y="0.5" width="419" height="219" rx="6" fill="#0A0E0C" stroke="#2E3934" />
							<path d="M20 180C80 160 120 170 170 176S260 150 320 158 390 176 400 180" fill="none" stroke="#E3B25B" stroke-opacity=".5" />
							<path d="M20 196C90 182 140 190 190 194S280 174 340 180 392 192 400 196" fill="none" stroke="#E3B25B" stroke-opacity=".3" />
							<path d="M210 60v60M180 90h60" stroke="#F1EDE4" stroke-width="2" />
							<ellipse class="ring" cx="210" cy="90" rx="44" ry="40" fill="none" stroke="#5EC2C9" stroke-width="2" />
							<ellipse cx="210" cy="90" rx="34" ry="31" fill="#C9C4B6" fill-opacity=".12" stroke="#C9C4B6" stroke-width="1.2" />
							<circle cx="210" cy="90" r="4" fill="#E3B25B" />
							<path d="M210 132v36" stroke="#9AA39C" stroke-dasharray="3 4" />
							<text x="222" y="156" font-family="IBM Plex Mono, monospace" font-size="11" fill="#9AA39C">≥ 10 cm spread</text>
							<text x="20" y="30" font-family="IBM Plex Mono, monospace" font-size="11" fill="#5EC2C9" letter-spacing="1">POINT 04 / 12</text>
						</svg>
					</div>
				</li>
				<li class="step">
					{@render marker(6, true)}
					<div>
						<h3 class="disp">Run it</h3>
						<p>
							Contour spacing, relief shading, sun direction, water speed and attenuation are all live, with a
							<strong>Drain water</strong> button. Every slider survives closing the panel and restarting the sandbox.
						</p>
					</div>
					<div class="media pair">
						<div class="shot"><img src={panelTopography} alt="Topography tab of the control panel" loading="lazy" /></div>
						<div class="shot"><img src={panelWater} alt="Water tab of the control panel" loading="lazy" /></div>
					</div>
				</li>
			</ol>
		</div>
	</section>
</main>

<footer>
	<svg class="ridge" viewBox="0 0 1440 60" preserveAspectRatio="none" aria-hidden="true">
		<path d="M0 40C160 20 280 50 420 34S700 10 860 28 1180 52 1440 30" fill="none" stroke="#3A4741" />
		<path d="M0 50C180 34 300 58 440 46S720 24 880 40 1200 60 1440 44" fill="none" stroke="#2A3430" />
		<path class="flow" d="M0 30C140 12 300 40 440 22S720 2 880 18 1180 40 1440 18" fill="none" stroke="#E3B25B" stroke-opacity=".5" />
	</svg>
	<div class="wrap foot mono">
		<span>SARndbox &amp; Vrui © Oliver Kreylos · GPL v2 · control panel under the same licence</span>
		<a href={REPO_URL}>github.com/domeepc/ARSandbox</a>
	</div>
</footer>

<style>
	.wrap {
		max-width: 1200px;
		margin: 0 auto;
		padding: 0 24px;
	}
	section {
		border-top: 1px solid var(--line);
		scroll-margin-top: 68px;
	}
	section > .wrap {
		padding-top: 112px;
		padding-bottom: 112px;
	}
	.alt {
		background: var(--ground-2);
	}
	.eyebrow {
		font-size: 12px;
		letter-spacing: 0.14em;
		text-transform: uppercase;
		color: var(--muted);
	}
	h2 {
		margin: 12px 0 18px;
		font-size: clamp(32px, 5vw, 44px);
		line-height: 1.05;
		letter-spacing: -0.02em;
		font-weight: 700;
		color: var(--text-1);
	}
	strong {
		color: var(--text-1);
	}
	code {
		color: var(--sand);
	}

	/* header */
	header {
		position: sticky;
		top: 0;
		z-index: 10;
		background: rgb(14 19 17 / 0.9);
		backdrop-filter: blur(8px);
		border-bottom: 1px solid var(--line);
	}
	.nav {
		height: 68px;
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 16px;
	}
	.brand {
		display: flex;
		align-items: center;
		gap: 12px;
		color: var(--text-1);
		text-decoration: none;
		font-weight: 700;
		font-size: 19px;
		letter-spacing: -0.01em;
	}
	.links {
		display: flex;
		gap: 32px;
		font-size: 13px;
		letter-spacing: 0.06em;
		text-transform: uppercase;
	}
	.links a {
		color: var(--muted);
		text-decoration: none;
	}
	.links a:hover {
		color: var(--text-1);
	}
	.gh {
		min-height: 44px;
		padding: 0 14px;
		font-size: 13px;
	}

	/* hero */
	.hero {
		position: relative;
		min-height: 760px;
		border-top: 0;
		overflow: hidden;
	}
	.hero-inner {
		position: relative;
		padding-top: 120px;
		padding-bottom: 120px;
	}
	.shelf {
		max-width: 560px;
		background: rgb(14 19 17 / 0.86);
		border: 1px solid var(--line);
		border-radius: 4px;
		padding: 40px 40px 36px;
	}
	.eyebrow.water {
		display: flex;
		align-items: center;
		gap: 10px;
		color: var(--water);
	}
	h1 {
		margin: 18px 0 20px;
		font-size: clamp(42px, 7vw, 64px);
		line-height: 0.98;
		letter-spacing: -0.03em;
		font-weight: 750;
		color: var(--text-1);
	}
	h1 span {
		color: var(--sand);
	}
	.lead {
		margin: 0 0 28px;
		font-size: 18px;
		color: var(--text-2);
		max-width: 46ch;
	}
	.actions {
		display: flex;
		flex-wrap: wrap;
		gap: 12px;
	}
	.legend {
		position: absolute;
		left: 0;
		right: 0;
		bottom: 24px;
		font-size: 11px;
		letter-spacing: 0.08em;
		color: var(--muted);
	}
	.legend-row {
		display: flex;
		align-items: center;
		gap: 10px;
	}
	.rise {
		opacity: 0;
		transform: translateY(10px);
		animation: rise 0.9s cubic-bezier(0.2, 0.7, 0.2, 1) forwards;
	}
	.d1 {
		animation-delay: 0.15s;
	}
	.d2 {
		animation-delay: 0.3s;
	}
	.d3 {
		animation-delay: 0.45s;
	}
	.led {
		animation: blink 2.4s steps(1) infinite;
	}

	/* rig */
	.rig {
		display: grid;
		grid-template-columns: minmax(0, 1.1fr) minmax(0, 1fr);
		gap: 56px;
		align-items: center;
	}
	figure {
		margin: 0;
	}
	.intro {
		margin: 0 0 32px;
		color: var(--text-2);
		max-width: 48ch;
	}
	.reqs {
		list-style: none;
		margin: 0;
		padding: 0;
		display: grid;
		grid-template-columns: repeat(2, minmax(0, 1fr));
		gap: 1px;
		background: var(--line);
		border: 1px solid var(--line);
		border-radius: 4px;
		overflow: hidden;
	}
	.reqs li {
		background: var(--ground-2);
		padding: 20px;
		display: flex;
		flex-direction: column;
	}
	.reqs strong {
		margin-top: 12px;
		font-weight: 600;
	}
	.reqs span {
		font-size: 14px;
		color: var(--muted);
	}

	/* install */
	.section-head {
		display: flex;
		flex-wrap: wrap;
		justify-content: space-between;
		align-items: flex-end;
		gap: 24px;
		margin-bottom: 40px;
	}
	.section-head h2 {
		margin-bottom: 0;
	}
	.aside {
		margin: 0;
		color: var(--muted);
		max-width: 44ch;
		font-size: 15px;
	}
	.aside code {
		color: var(--text-2);
	}

	/* setup */
	.steps-title {
		margin-bottom: 64px;
	}
	.steps {
		list-style: none;
		margin: 0;
		padding: 0;
	}
	.step {
		display: grid;
		grid-template-columns: 72px minmax(0, 1fr) minmax(0, 420px);
		gap: 28px;
		align-items: start;
		padding-bottom: 56px;
	}
	.step:last-child {
		padding-bottom: 0;
	}
	h3 {
		margin: 8px 0 10px;
		font-size: 24px;
		font-weight: 650;
		color: var(--text-1);
	}
	.step p {
		margin: 0 0 14px;
		color: var(--text-2);
	}
	.step p.small {
		font-size: 13px;
		color: var(--muted);
	}
	.media svg {
		display: block;
		width: 100%;
		height: auto;
	}
	.shot {
		border: 1px solid var(--line-2);
		border-radius: 6px;
		overflow: hidden;
		background: #0a0e0c;
	}
	.shot img {
		display: block;
		width: 100%;
		height: 300px;
		object-fit: cover;
		object-position: top;
	}
	.pair {
		display: grid;
		grid-template-columns: repeat(2, minmax(0, 1fr));
		gap: 12px;
	}
	.pair img {
		height: 220px;
	}
	.ring {
		transform-box: fill-box;
		transform-origin: center;
		animation: breathe 7s ease-in-out infinite;
	}

	/* footer */
	footer {
		border-top: 1px solid var(--line);
	}
	.ridge {
		display: block;
		width: 100%;
		height: 48px;
	}
	.flow {
		stroke-dasharray: 2 6;
		animation: flow 2.6s linear infinite reverse;
	}
	.foot {
		padding-top: 24px;
		padding-bottom: 48px;
		display: flex;
		flex-wrap: wrap;
		justify-content: space-between;
		gap: 16px;
		font-size: 12px;
		color: var(--muted);
		letter-spacing: 0.04em;
	}
	.foot a {
		color: var(--text-2);
	}

	@keyframes rise {
		to {
			opacity: 1;
			transform: none;
		}
	}
	@keyframes blink {
		50% {
			opacity: 0.25;
		}
	}
	@keyframes breathe {
		0%,
		100% {
			transform: scale(0.96);
		}
		50% {
			transform: scale(1.04);
		}
	}
	@keyframes flow {
		to {
			stroke-dashoffset: -20;
		}
	}
	@media (prefers-reduced-motion: reduce) {
		.rise,
		.led,
		.ring,
		.flow {
			animation: none;
		}
		.rise {
			opacity: 1;
			transform: none;
		}
	}

	@media (max-width: 900px) {
		.links {
			display: none;
		}
		.rig {
			grid-template-columns: minmax(0, 1fr);
		}
		.hero-inner {
			padding-top: 64px;
			padding-bottom: 96px;
		}
		.shelf {
			padding: 28px 24px;
		}
		section > .wrap {
			padding-top: 80px;
			padding-bottom: 80px;
		}
		.step {
			grid-template-columns: 44px minmax(0, 1fr);
			gap: 16px;
		}
		.step :global(.marker) {
			width: 44px;
			height: 44px;
		}
		.media {
			grid-column: 2;
		}
	}
	@media (max-width: 520px) {
		.reqs,
		.pair {
			grid-template-columns: minmax(0, 1fr);
		}
	}
</style>
