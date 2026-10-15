import JSON5 from 'json5';
// @ts-expect-error -- wavedrom ships CommonJS without type declarations
import { renderAny, onml, waveSkin } from 'wavedrom';
import type { FenceRenderer } from './registry';
import { sanitizeSvg } from './sanitize-svg';

let counter = 0;

// WaveDrom's bundled "dark" skin assumes a page background it doesn't draw, so
// the default skin is always used and dark mode inverts it in CSS instead.
const renderer: FenceRenderer = {
  cacheable: true,
  async render(code, el) {
    // WaveJSON is a JS object literal; JSON5 parses it without eval.
    const source = JSON5.parse(code);
    const jsonml = renderAny(counter++, source, waveSkin);
    el.innerHTML = sanitizeSvg(onml.stringify(jsonml));
  },
};

export default renderer;
