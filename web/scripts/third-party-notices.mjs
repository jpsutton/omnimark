// Writes ../THIRD_PARTY_NOTICES.md: license texts of every production npm
// dependency bundled into the web frontend, plus the native components the
// packaged app ships (Qt, QtWebEngine/Chromium).
//
//   pnpm notices
import { execFileSync } from 'node:child_process';
import { existsSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';

const root = resolve(import.meta.dirname, '..');
const out = resolve(root, '..', 'THIRD_PARTY_NOTICES.md');

const json = execFileSync('pnpm', ['licenses', 'list', '--prod', '--json'], { cwd: root, encoding: 'utf8' });
const byLicense = JSON.parse(json);

const packages = [];
for (const [license, list] of Object.entries(byLicense)) {
  for (const pkg of list) {
    for (const [i, version] of pkg.versions.entries()) {
      packages.push({ name: pkg.name, version, license, path: pkg.paths[i] ?? pkg.paths[0], homepage: pkg.homepage });
    }
  }
}
packages.sort((a, b) => a.name.localeCompare(b.name) || a.version.localeCompare(b.version));

function licenseText(dir) {
  if (!dir || !existsSync(dir)) return undefined;
  const file = readdirSync(dir).find((f) => /^(licen[cs]e|copying|notice)(\.|$)/i.test(f));
  return file ? readFileSync(join(dir, file), 'utf8').trim() : undefined;
}

function declaredLicense(dir) {
  try {
    const meta = JSON.parse(readFileSync(join(dir, 'package.json'), 'utf8'));
    return typeof meta.license === 'string' ? meta.license : meta.license?.type;
  } catch {
    return undefined;
  }
}

const lines = [
  '# Third-party notices',
  '',
  'Omnimark is MIT licensed (see LICENSE). Packaged builds include the components below, under their own licenses.',
  '',
  '## Native components',
  '',
  '- **Qt 6** (Core, Gui, Widgets, Network, Concurrent, PrintSupport, Svg, WebChannel, WebEngine). Used under the GNU Lesser General Public License v3, dynamically linked.',
  '  - Source: https://download.qt.io/official_releases/qt/',
  '  - License text: https://www.gnu.org/licenses/lgpl-3.0.html',
  '  - You may replace the bundled Qt libraries with your own build.',
  '- **QtWebEngine** includes Chromium and its third-party components under BSD-style and other permissive licenses.',
  '  - The complete list is available in a running copy at `chrome://credits` and in the QtWebEngine source.',
  '- **Graphviz**, compiled to WebAssembly by @hpcc-js/wasm-graphviz, is under the Eclipse Public License 2.0.',
  '  - Source: https://gitlab.com/graphviz/graphviz',
  '',
  '## JavaScript packages',
  '',
  `${packages.length} packages. Each is bundled unmodified.`,
  '',
  '| Package | Version | License |',
  '|---|---|---|',
];
function guessFromText(text) {
  if (!text) return undefined;
  if (/^(the )?mit license/i.test(text) || /permission is hereby granted, free of charge/i.test(text)) return 'MIT';
  if (/apache license[\s\S]{0,40}version 2\.0/i.test(text)) return 'Apache-2.0';
  if (/redistribution and use in source and binary forms/i.test(text)) return 'BSD';
  return undefined;
}

for (const p of packages) {
  const license = p.license === 'Unknown'
    ? declaredLicense(p.path) ?? guessFromText(licenseText(p.path)) ?? 'see text below'
    : p.license;
  lines.push(`| ${p.name} | ${p.version} | ${license} |`);
}
lines.push('', '## License texts', '');
for (const p of packages) {
  const text = licenseText(p.path);
  lines.push(`### ${p.name}@${p.version}`, '');
  if (p.homepage) lines.push(`Source: ${p.homepage}`, '');
  lines.push(text ? '```text\n' + text.replace(/```/g, "'''") + '\n```' : `License: ${p.license} (no license file shipped)`, '');
}
writeFileSync(out, lines.join('\n') + '\n');
console.log(`wrote ${out} (${packages.length} packages)`);
