export const REPO = 'domeepc/ARSandbox';
export const REPO_URL = `https://github.com/${REPO}`;
export const RELEASES_URL = `${REPO_URL}/releases/latest`;

/**
 * @typedef {{ name: string, browser_download_url: string, size: number }} Asset
 * @typedef {{ tag_name: string, html_url: string, published_at: string, assets: Asset[] }} Release
 */

const PACKAGE_NOTE =
	'Bundles Vrui, the Kinect package, the sandbox and the control panel under /opt/arsandbox, with arsandbox and sandbox-control on your PATH. Calibration is kept across upgrades.';

/**
 * Each target the release workflow builds for, and how to recognise its asset
 * by file name. Order matters: the Qt 5 .deb also ends in .deb.
 */
export const TARGETS = [
	{
		id: 'ubuntu',
		label: 'Ubuntu 24.04+',
		kind: '.deb · Debian, Qt 6',
		match: (/** @type {string} */ n) => n.endsWith('.deb') && !n.includes('+qt5'),
		lines: (/** @type {string} */ f) => [`sudo apt install ./${f}`],
		note: PACKAGE_NOTE
	},
	{
		id: 'ubuntu-qt5',
		label: 'Ubuntu 18.04 / 20.04',
		kind: '.deb · Mint 19/20, Qt 5',
		match: (/** @type {string} */ n) => n.endsWith('+qt5_amd64.deb'),
		lines: (/** @type {string} */ f) => [`sudo apt install ./${f}`],
		note: `Built on 18.04, so it needs only that release’s glibc and libraries. ${PACKAGE_NOTE}`
	},
	{
		id: 'fedora',
		label: 'Fedora 40+',
		kind: '.rpm · signed',
		match: (/** @type {string} */ n) => n.endsWith('.rpm'),
		lines: (/** @type {string} */ f) => [`sudo dnf install ./${f}`],
		note: PACKAGE_NOTE
	},
	{
		id: 'source',
		label: 'Anything else',
		kind: 'source · incl. Ubuntu 22.04',
		match: (/** @type {string} */ n) => n === 'arsandbox-source.tar.gz',
		// The archive has no top-level folder, so unpack it into one.
		lines: (/** @type {string} */ f) => [
			`mkdir arsandbox && tar xzf ${f} -C arsandbox`,
			'cd arsandbox && ./install.sh'
		],
		note: 'Installs build dependencies, builds Vrui 8.0 and its Kinect package if missing, then the sandbox and panel — adapting to whichever Qt and libraries your distribution ships.'
	}
];

/** @returns {Promise<Release>} */
export async function latestRelease() {
	const res = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`, {
		headers: { Accept: 'application/vnd.github+json' }
	});
	if (!res.ok) throw new Error(`GitHub answered ${res.status}`);
	return res.json();
}

/** Best guess at the visitor's target from the user agent; Ubuntu otherwise. */
export function detectTarget() {
	return /Fedora|Red Hat/i.test(navigator.userAgent) ? 'fedora' : 'ubuntu';
}

export const isLinux = () => /Linux/.test(navigator.userAgent) && !/Android/.test(navigator.userAgent);

/** @param {number} bytes */
export const formatSize = (bytes) => `${(bytes / 1024 / 1024).toFixed(1)} MB`;
