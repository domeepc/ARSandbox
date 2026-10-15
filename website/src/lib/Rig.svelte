<svg
	viewBox="0 0 560 400"
	role="img"
	aria-label="Side view of the rig: a Kinect and a projector mounted above a box of sand, the Kinect reading depth and the projector casting the map down onto the sand."
>
	<!-- ceiling mount -->
	<path d="M40 28H520" stroke="#3A4741" stroke-width="2" />
	<path
		d="M60 28l-8-10M90 28l-8-10M120 28l-8-10M150 28l-8-10M180 28l-8-10M210 28l-8-10M240 28l-8-10M270 28l-8-10M300 28l-8-10M330 28l-8-10M360 28l-8-10M390 28l-8-10M420 28l-8-10M450 28l-8-10M480 28l-8-10M510 28l-8-10"
		stroke="#2A3430"
		stroke-width="1.2"
	/>
	<path d="M205 28v22M345 28v18" stroke="#3A4741" stroke-width="2" />

	<!-- projector cone and Kinect field of view -->
	<path d="M345 92L72 300H488Z" fill="#E3B25B" fill-opacity=".07" />
	<path class="beam" d="M345 92L72 300M345 92L488 300" stroke="#E3B25B" stroke-width="1.3" fill="none" />
	<path class="beam slow" d="M205 72L92 300M205 72L462 300" stroke="#5EC2C9" stroke-width="1.3" fill="none" />

	<!-- projector -->
	<rect x="306" y="46" width="78" height="44" rx="6" fill="#161C19" stroke="#C9C4B6" stroke-width="1.5" />
	<circle cx="345" cy="90" r="9" fill="#0E1311" stroke="#E3B25B" stroke-width="1.5" />
	<path d="M316 58h22M316 64h22" stroke="#3A4741" stroke-width="1.5" />

	<!-- Kinect -->
	<rect x="158" y="50" width="94" height="20" rx="10" fill="#161C19" stroke="#C9C4B6" stroke-width="1.5" />
	<circle cx="182" cy="60" r="4" fill="none" stroke="#5EC2C9" stroke-width="1.4" />
	<circle cx="205" cy="60" r="4" fill="none" stroke="#C9C4B6" stroke-width="1.4" />
	<circle cx="228" cy="60" r="4" fill="none" stroke="#5EC2C9" stroke-width="1.4" />
	<circle class="led" cx="244" cy="60" r="1.6" fill="#7BD88F" />

	<!-- sand box -->
	<path
		d="M72 300C120 268 150 250 190 258S250 300 290 296 360 248 410 252 470 286 488 300V352H72Z"
		fill="#B88A45"
		fill-opacity=".25"
	/>
	<path
		d="M60 300L72 300C120 268 150 250 190 258S250 300 290 296 360 248 410 252 470 286 488 300H500"
		fill="none"
		stroke="#E3B25B"
		stroke-width="1.8"
	/>
	<path d="M248 290C262 298 276 300 290 296S316 284 326 280" stroke="#5EC2C9" stroke-width="3" fill="none" stroke-linecap="round" />
	<path d="M60 300V352H500V300" fill="none" stroke="#C9C4B6" stroke-width="1.6" />

	<!-- callouts -->
	<g class="callouts">
		<path d="M158 60H118V112H40" fill="none" stroke="#5EC2C9" stroke-opacity=".6" />
		<text x="40" y="104" fill="#5EC2C9">01 KINECT v1</text>
		<text x="40" y="128" fill="#9AA39C">reads depth</text>
		<path d="M384 68H430V112H520" fill="none" stroke="#E3B25B" stroke-opacity=".6" />
		<text x="520" y="104" fill="#E3B25B" text-anchor="end">03 PROJECTOR</text>
		<text x="520" y="128" fill="#9AA39C" text-anchor="end">paints it back</text>
		<path d="M280 352V384" stroke="#9AA39C" stroke-opacity=".5" />
		<text x="292" y="380" fill="#C9C4B6">02 SAND · water pools in the dip</text>
	</g>
</svg>

<style>
	svg {
		display: block;
		width: 100%;
		height: auto;
	}
	.callouts {
		font-family: var(--mono);
		font-size: 11px;
		letter-spacing: 1px;
	}
	.beam {
		stroke-dasharray: 3 7;
		animation: flow 1.4s linear infinite;
	}
	.beam.slow {
		stroke-dasharray: 2 6;
		animation-duration: 2.6s;
		animation-direction: reverse;
	}
	.led {
		animation: blink 2.4s steps(1) infinite;
	}
	@keyframes flow {
		to {
			stroke-dashoffset: -20;
		}
	}
	@keyframes blink {
		50% {
			opacity: 0.25;
		}
	}
	@media (prefers-reduced-motion: reduce) {
		.beam,
		.led {
			animation: none;
		}
	}
</style>
