<script>
	import Code from '$lib/Code.svelte';
	import Downloads from '$lib/Downloads.svelte';
	import { REPO_URL } from '$lib/release.js';
	import sandboxView from '../../../docs/images/sandbox-view.png';
	import panelMenu from '../../../docs/images/panel-menu.png';
	import panelCalibration from '../../../docs/images/panel-calibration.png';
	import panelTopography from '../../../docs/images/panel-topography.png';
	import panelWater from '../../../docs/images/panel-water.png';
	import logo from '../../../packaging/arsandbox.svg';
</script>

<svelte:head>
	<title>ARSandbox</title>
	<meta
		name="description"
		content="Install the Augmented Reality Sandbox: a Kinect watches a box of sand and a projector paints a live topographic map and water simulation back onto it."
	/>
</svelte:head>

<header>
	<nav>
		<a class="brand" href="#top"><img src={logo} alt="" width="28" height="28" /> ARSandbox</a>
		<div class="links">
			<a href="#install">Install</a>
			<a href="#setup">Setup</a>
			<a href={REPO_URL}>GitHub</a>
		</div>
	</nav>
</header>

<main id="top">
	<section class="hero">
		<div>
			<h1>Augmented Reality Sandbox</h1>
			<p class="lead">
				A Kinect depth camera watches a box of sand, and a projector paints a live topographic map,
				contour lines and a water-flow simulation back onto it.
			</p>
			<p class="muted">
				Built on Oliver Kreylos's SARndbox 2.8 and the Vrui VR toolkit, with a Qt Quick control
				panel for calibration and live tuning.
			</p>
			<a class="cta" href="#install">Install it</a>
		</div>
		<img class="shot" src={sandboxView} alt="The sandbox rendering contour lines and water" />
	</section>

	<section id="requirements">
		<h2>What you need</h2>
		<ul class="reqs">
			<li><strong>Linux</strong><span>The sandbox does not run on macOS or Windows.</span></li>
			<li><strong>Kinect v1</strong><span>The Xbox 360 model.</span></li>
			<li><strong>A projector</strong><span>Mounted above the box, pointing down.</span></li>
			<li><strong>Qt 6.4+</strong><span>Or Qt 5.9+ on Ubuntu 20.04 and older.</span></li>
		</ul>
	</section>

	<section id="install">
		<h2>Install</h2>
		<p>
			Pick your distribution. The prebuilt packages carry the whole app; building from source
			works everywhere else.
		</p>
		<Downloads />
		<details>
			<summary>Building from a git checkout instead</summary>
			<Code code={`git clone ${REPO_URL}.git\ncd ARSandbox\n./install.sh`} />
		</details>
		<p class="muted">
			Once installed, <code>sandbox-control --version</code> prints the release a build came from.
			A machine that once ran <code>install.sh</code> keeps an older <code>sandbox-control</code> in
			<code>/usr/local/bin</code>, ahead of the package's on PATH — worth checking.
		</p>
	</section>

	<section id="setup">
		<h2>Setup, step by step</h2>
		<ol class="steps">
			<li>
				<h3>Start it</h3>
				<p>Installed from a package:</p>
				<Code code="arsandbox" />
				<p>From a source checkout:</p>
				<Code code="scripts/run-sandbox.sh" />
				<p>
					On first run it reports <code>No projector calibration yet; using the default projection.</code>
					That is expected — the projection lines up with the sand after step 5, and the saved calibration
					is applied automatically on every later run.
				</p>
			</li>
			<li>
				<h3>Open the control panel</h3>
				<p>
					<strong>Right click</strong> anywhere on the sandbox view. The panel opens if it is not running;
					if it is, you get a menu on the panel's screen. Every other mouse and keyboard binding is
					disabled, so a visitor leaning on the keyboard cannot knock the projection off the sand.
				</p>
				<img src={panelMenu} alt="Right-click menu offering to open the control panel" loading="lazy" />
			</li>
			<li>
				<h3>Camera intrinsics</h3>
				<p>
					Open the <strong>Calibration</strong> tab and run step 1. It downloads the factory calibration
					from the camera itself — specific to that one Kinect.
				</p>
			</li>
			<li>
				<h3>Base plane and sand extents</h3>
				<img src={panelCalibration} alt="Calibration tab showing a depth frame of the box" loading="lazy" />
				<ol>
					<li>Press <strong>Grab depth image</strong>.</li>
					<li>
						Lay a <strong>flat plate</strong> on the sand and drag a rectangle over it, clear of the box walls.
					</li>
					<li>
						Remove the plate, grab again, and drag over the <strong>bare sand</strong> to set its extents.
						Click a corner afterwards to re-pick it by hand.
					</li>
					<li>Press <strong>Write BoxLayout.txt</strong>. The sandbox restarts itself to pick it up.</li>
				</ol>
				<p class="muted">
					The panel then checks tilt, corner fit and opposite-side mismatch, and says plainly if a
					measurement came out wrong.
				</p>
			</li>
			<li>
				<h3>Projector alignment</h3>
				<p>
					Pick your projector from the dropdown (check it against its specification) and press
					<strong>Start</strong>. Keep the box clear for the first two seconds while it records the
					background.
				</p>
				<p>
					Hold a flat circular target with a marked centre — a CD with a paper disk glued on works —
					<strong>above</strong> the sand, centred on the white cross. Press
					<strong>Capture this point</strong> once the blue disk appears. Repeat for every point (12 by
					default), varying the height by at least 10&nbsp;cm between them.
				</p>
				<p class="muted">The RMS residual reported at the end is the honest measure of the result.</p>
			</li>
			<li>
				<h3>Run it</h3>
				<p>
					Contour spacing, relief shading, sun direction, water speed and attenuation are all live, with
					a <strong>Drain water</strong> button on the Water tab. Every slider survives closing the panel
					and restarting the sandbox.
				</p>
				<div class="pair">
					<img src={panelTopography} alt="Topography tab" loading="lazy" />
					<img src={panelWater} alt="Water tab" loading="lazy" />
				</div>
			</li>
		</ol>
	</section>
</main>

<footer>
	<p>
		SARndbox and Vrui are GPL v2; the control panel carries the same licence.
		<a href={REPO_URL}>Source on GitHub</a>
	</p>
</footer>

<style>
	:global(:root) {
		--bg: #fbfaf7;
		--surface: #ffffff;
		--code-bg: #f3f1ec;
		--text: #1f2328;
		--muted: #5b6168;
		--border: #e2ded6;
		--accent: #b5651d;
		--on-accent: #ffffff;
		color-scheme: light dark;
	}
	@media (prefers-color-scheme: dark) {
		:global(:root) {
			--bg: #14161a;
			--surface: #1c1f24;
			--code-bg: #23272e;
			--text: #e6e6e6;
			--muted: #9aa1aa;
			--border: #30353d;
			--accent: #e0954f;
			--on-accent: #14161a;
		}
	}
	:global(body) {
		margin: 0;
		background: var(--bg);
		color: var(--text);
		font-family: system-ui, -apple-system, 'Segoe UI', Roboto, sans-serif;
		line-height: 1.6;
	}
	:global(a) {
		color: var(--accent);
	}
	:global(code) {
		font-family: ui-monospace, 'SFMono-Regular', Menlo, Consolas, monospace;
		font-size: 0.9em;
	}
	:global(html) {
		scroll-behavior: smooth;
	}

	header {
		position: sticky;
		top: 0;
		z-index: 1;
		background: color-mix(in srgb, var(--bg) 88%, transparent);
		backdrop-filter: blur(8px);
		border-bottom: 1px solid var(--border);
	}
	nav,
	main,
	footer {
		max-width: 960px;
		margin: 0 auto;
		padding: 0 16px;
	}
	nav {
		display: flex;
		justify-content: space-between;
		align-items: center;
		height: 56px;
	}
	.brand {
		display: flex;
		align-items: center;
		gap: 0.5rem;
		font-weight: 700;
		color: var(--text);
		text-decoration: none;
	}
	.links {
		display: flex;
		gap: 1.25rem;
	}
	.links a {
		color: var(--muted);
		text-decoration: none;
	}
	.links a:hover {
		color: var(--text);
	}

	section {
		padding: 3rem 0 1rem;
		scroll-margin-top: 56px;
	}
	h1 {
		font-size: clamp(2rem, 5vw, 3rem);
		line-height: 1.1;
		margin: 0 0 1rem;
	}
	h2 {
		font-size: 1.6rem;
		margin: 0 0 1rem;
	}
	.hero {
		display: grid;
		grid-template-columns: 1fr 1fr;
		gap: 2rem;
		align-items: center;
	}
	.lead {
		font-size: 1.15rem;
	}
	.muted {
		color: var(--muted);
		font-size: 0.9rem;
	}
	.cta {
		display: inline-block;
		margin-top: 0.5rem;
		padding: 0.75rem 1.4rem;
		border-radius: 8px;
		background: var(--accent);
		color: var(--on-accent);
		font-weight: 600;
		text-decoration: none;
	}
	img {
		max-width: 100%;
		height: auto;
		border-radius: 10px;
		border: 1px solid var(--border);
	}
	.shot {
		box-shadow: 0 10px 30px rgb(0 0 0 / 0.15);
	}

	.reqs {
		list-style: none;
		padding: 0;
		margin: 0;
		display: grid;
		grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
		gap: 0.75rem;
	}
	.reqs li {
		padding: 1rem;
		border: 1px solid var(--border);
		border-radius: 10px;
		background: var(--surface);
		display: flex;
		flex-direction: column;
	}
	.reqs span {
		color: var(--muted);
		font-size: 0.9rem;
	}

	details {
		margin: 1.25rem 0 0.5rem;
	}
	summary {
		cursor: pointer;
		font-weight: 600;
	}

	.steps {
		list-style: none;
		padding: 0;
		margin: 0;
		counter-reset: step;
	}
	.steps > li {
		counter-increment: step;
		position: relative;
		padding: 0 0 2rem 3.25rem;
		border-left: 2px solid var(--border);
		margin-left: 1rem;
	}
	.steps > li:last-child {
		border-left-color: transparent;
	}
	.steps > li::before {
		content: counter(step);
		position: absolute;
		left: -1.05rem;
		top: -0.1rem;
		width: 2rem;
		height: 2rem;
		border-radius: 50%;
		display: grid;
		place-items: center;
		background: var(--accent);
		color: var(--on-accent);
		font-weight: 700;
	}
	h3 {
		margin: 0 0 0.5rem;
		font-size: 1.2rem;
	}
	.steps img {
		display: block;
		max-width: min(100%, 460px);
		margin: 0.75rem 0;
	}
	.pair {
		display: grid;
		grid-template-columns: 1fr 1fr;
		gap: 0.75rem;
		max-width: 720px;
	}
	.pair img {
		margin: 0;
	}

	footer {
		padding-top: 2rem;
		padding-bottom: 3rem;
		color: var(--muted);
		font-size: 0.9rem;
		border-top: 1px solid var(--border);
		margin-top: 2rem;
	}

	@media (max-width: 720px) {
		.hero,
		.pair {
			grid-template-columns: 1fr;
		}
		.steps > li {
			padding-left: 1.75rem;
		}
		.links {
			gap: 0.9rem;
		}
	}
</style>
