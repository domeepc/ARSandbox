<script>
	import { hillA, hillB, basin } from './terrain.js';

	// The sandbox's own elevation scale, lowland to peak.
	const BANDS = ['#2E5A44', '#3E7B5A', '#5C9460', '#86A85E', '#B5B65E', '#D9B65A', '#E09F4F', '#C97E45', '#EDE6D6'];
	const OPACITY = [0.55, 0.5, 0.5, 0.5, 0.5, 0.55, 0.55, 0.6, 0.7];
	// Every third ring is an index contour, drawn heavier.
	const width = (/** @type {number} */ i) => (i % 3 === 0 ? 2 : 1.2);
</script>

<svg class="terrain" viewBox="-40 20 1440 800" preserveAspectRatio="xMidYMid slice" aria-hidden="true">
	<g class="drift">
		<g class="bands">
			{#each hillA as d, i}<path {d} fill={BANDS[i]} fill-opacity={OPACITY[i]} />{/each}
			{#each hillB as d, i}<path {d} fill={BANDS[i]} fill-opacity={OPACITY[i]} />{/each}
		</g>
		<g class="trace">
			{#each hillA as d, i}<path {d} stroke-width={width(i)} style:animation-delay="{i * 0.12}s" />{/each}
		</g>
		<g class="trace">
			{#each hillB as d, i}<path {d} stroke-width={width(i)} style:animation-delay="{i * 0.12}s" />{/each}
		</g>
		<path class="breathe" d={basin[0]} fill="#2F7F96" fill-opacity=".55" stroke="#5EC2C9" stroke-opacity=".7" stroke-width="1.2" />
		<path class="breathe late" d={basin[1]} fill="#2A6F86" fill-opacity=".7" stroke="#5EC2C9" stroke-opacity=".5" />
		<path d={basin[2]} fill="#1F5A70" fill-opacity=".85" />
		<g class="labels">
			<text x="1240" y="248">120</text>
			<text x="1150" y="300">160</text>
			<text x="955" y="330">220</text>
			<text x="470" y="540">90</text>
			<text x="590" y="690" fill="#BFEDEF">−20</text>
		</g>
	</g>
</svg>

<style>
	.terrain {
		position: absolute;
		inset: 0;
		width: 100%;
		height: 100%;
	}
	.trace path {
		fill: none;
		stroke: var(--ground);
		stroke-opacity: 0.75;
		stroke-dasharray: 2600;
		stroke-dashoffset: 2600;
		animation: trace 3.2s cubic-bezier(0.55, 0, 0.2, 1) forwards;
	}
	.bands path {
		opacity: 0;
		animation: fade 2.4s ease forwards 0.6s;
	}
	.drift {
		animation: drift 22s ease-in-out infinite alternate;
	}
	.breathe {
		transform-box: fill-box;
		transform-origin: center;
		animation: breathe 7s ease-in-out infinite;
	}
	.breathe.late {
		animation-delay: 1.2s;
	}
	.labels {
		font-family: var(--mono);
		font-size: 11px;
		letter-spacing: 1px;
		fill: var(--ground);
		fill-opacity: 0.8;
	}
	@keyframes trace {
		to {
			stroke-dashoffset: 0;
		}
	}
	@keyframes fade {
		to {
			opacity: 1;
		}
	}
	@keyframes drift {
		to {
			transform: translate(-14px, 8px);
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
	@media (prefers-reduced-motion: reduce) {
		.trace path,
		.bands path,
		.drift,
		.breathe {
			animation: none;
		}
		.trace path {
			stroke-dashoffset: 0;
		}
		.bands path {
			opacity: 1;
		}
	}
	@media (max-width: 900px) {
		.terrain {
			opacity: 0.4;
		}
	}
</style>
