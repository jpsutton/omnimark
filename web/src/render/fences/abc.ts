import abcjs from 'abcjs';
import type { FenceRenderer } from './registry';
import { sanitizeSvg } from './sanitize-svg';

const renderer: FenceRenderer = {
  async render(code, el) {
    const target = document.createElement('div');
    el.append(target);
    const [tune] = abcjs.renderAbc(target, code, {
      responsive: 'resize',
      foregroundColor: 'currentColor',
      add_classes: true,
    });
    const warnings = tune?.warnings;
    if (!tune || (warnings?.length && !target.querySelector('svg'))) {
      throw new Error(warnings?.join('\n') ?? 'Could not parse ABC notation.');
    }
    // Header fields (title, composer…) come from the document.
    target.innerHTML = sanitizeSvg(target.innerHTML);
  },
};

export default renderer;
