export const REPO = 'domeepc/ARSandbox';
export const REPO_URL = `https://github.com/${REPO}`;
export const RELEASES_URL = `${REPO_URL}/releases/latest`;

/**
 * @typedef {{ name: string, browser_download_url: string, size: number }} Asset
 * @typedef {{ tag_name: string, html_url: string, published_at: string, assets: Asset[] }} Release
 */

/**
 * Each target the release workflow builds for, and how to recognise its asset
 * by file name. Order matters: the Qt 5 .deb also ends in .deb.
 */
export const TARGETS = [
	{
		id: 'ubuntu',
		label: 'Ubuntu 24.04+ / Debian',
		match: (/** @type {string} */ n) => n.endsWith('.deb') && !n.includes('+qt5'),
		install: (/** @type {string} */ f) => `sudo apt install ./${f}`
	},
	{
		id: 'ubuntu-qt5',
		label: 'Ubuntu 18.04 / 20.04, Mint 19 / 20',
		match: (/** @type {string} */ n) => n.endsWith('+qt5_amd64.deb'),
		install: (/** @type {string} */ f) => `sudo apt install ./${f}`
	},
	{
		id: 'fedora',
		label: 'Fedora 40+',
		match: (/** @type {string} */ n) => n.endsWith('.rpm'),
		install: (/** @type {string} */ f) => `sudo dnf install ./${f}`
	},
	{
		id: 'source',
		label: 'Anything else (incl. Ubuntu 22.04)',
		match: (/** @type {string} */ n) => n === 'arsandbox-source.tar.gz',
		install: (/** @type {string} */ f) =>
			`tar xzf ${f}\n./install.sh   # PREFIX, SRCDIR, JOBS, QT_PREFIX override the defaults`
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
	const ua = navigator.userAgent;
	if (/Fedora|Red Hat/i.test(ua)) return 'fedora';
	return 'ubuntu';
}

export const isLinux = () => /Linux/.test(navigator.userAgent) && !/Android/.test(navigator.userAgent);

/** @param {number} bytes */
export const formatSize = (bytes) => `${(bytes / 1024 / 1024).toFixed(1)} MB`;
