import mermaid from 'mermaid';
import type { FenceRenderer, Theme } from './registry';

let configuredTheme: Theme | undefined;
let counter = 0;

function configure(theme: Theme) {
  if (configuredTheme === theme) return;
  mermaid.initialize({
    startOnLoad: false,
    securityLevel: 'strict',
    theme: theme === 'dark' ? 'dark' : 'default',
    fontFamily: 'inherit',
  });
  configuredTheme = theme;
}

const renderer: FenceRenderer = {
  cacheable: true,
  async render(code, el, ctx) {
    configure(ctx.theme);
    const id = `om-mermaid-${++counter}`;
    try {
      const { svg, bindFunctions } = await mermaid.render(id, code);
      el.innerHTML = svg; // mermaid sanitizes with DOMPurify in strict mode
      bindFunctions?.(el);
    } finally {
      // mermaid leaves a temp node behind when rendering fails; keep our output.
      for (const leftover of [document.getElementById(id), document.getElementById(`d${id}`)]) {
        if (leftover && !el.contains(leftover)) leftover.remove();
      }
    }
  },
};

export default renderer;
