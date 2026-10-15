import { Graphviz } from '@hpcc-js/wasm-graphviz';
import type { FenceRenderer } from './registry';
import { sanitizeSvg } from './sanitize-svg';

let instance: Promise<Graphviz> | undefined;

/**
 * Graphviz draws a white page background and black ink by default. Make the
 * background transparent and map default black to the text color so graphs
 * follow the theme; explicit colors in the source are left alone.
 */
export function themeSvg(svg: string): string {
  return svg
    .replace(/(<g id="graph0" class="graph"[^>]*>\s*(?:<title>[^<]*<\/title>\s*)?<polygon) fill="white"/, '$1 fill="transparent"')
    .replace(/(fill|stroke)="black"/g, '$1="currentColor"');
}

const renderer: FenceRenderer = {
  cacheable: true,
  async render(code, el) {
    instance ??= Graphviz.load();
    const graphviz = await instance;
    el.innerHTML = sanitizeSvg(themeSvg(graphviz.dot(code)));
  },
};

export default renderer;
