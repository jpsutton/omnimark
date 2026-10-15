import SmilesDrawer from 'smiles-drawer';
import type { FenceRenderer } from './registry';
import { sanitizeSvg } from './sanitize-svg';

function parse(smiles: string): Promise<unknown> {
  return new Promise((resolve, reject) => SmilesDrawer.parse(smiles, resolve, reject));
}

const renderer: FenceRenderer = {
  cacheable: true,
  async render(code, el, ctx) {
    const lines = code.split('\n').map((l) => l.trim()).filter(Boolean);
    if (!lines.length) throw new Error('No SMILES string.');
    for (const smiles of lines) {
      const tree = await parse(smiles).catch((err: unknown) => {
        throw new Error(`Invalid SMILES "${smiles}": ${err instanceof Error ? err.message : String(err)}`);
      });
      const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
      const drawer = new SmilesDrawer.SvgDrawer({ width: 360, height: 240, padding: 12 });
      drawer.draw(tree, svg, ctx.theme === 'dark' ? 'dark' : 'light');
      const holder = document.createElement('figure');
      holder.className = 'om-smiles';
      holder.innerHTML = sanitizeSvg(svg.outerHTML);
      const caption = document.createElement('figcaption');
      caption.textContent = smiles;
      holder.append(caption);
      el.append(holder);
    }
  },
};

export default renderer;
