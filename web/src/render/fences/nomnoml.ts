import { renderSvg } from 'nomnoml';
import type { FenceRenderer } from './registry';
import { sanitizeSvg } from './sanitize-svg';

const DARK = ['#stroke: #d1d7e0', '#fill: #262c36; #2f3742', '#fontColor: #f0f6fc', '#background: transparent'];

const renderer: FenceRenderer = {
  cacheable: true,
  async render(code, el, ctx) {
    // Directives later in the source win, so user styling still applies.
    const source = ctx.theme === 'dark' ? `${DARK.join('\n')}\n${code}` : code;
    el.innerHTML = sanitizeSvg(renderSvg(source));
  },
};

export default renderer;
