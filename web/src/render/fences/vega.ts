import embed, { type VisualizationSpec } from 'vega-embed';
import { expressionInterpreter } from 'vega-interpreter';
import JSON5 from 'json5';
import type { FenceRenderer } from './registry';

function parseSpec(code: string): VisualizationSpec {
  const spec = JSON5.parse(code) as VisualizationSpec & { data?: { url?: string } };
  // Remote data URLs would be fetched from the page; keep documents self-contained.
  if (typeof spec.data?.url === 'string' && /^[a-z]+:/i.test(spec.data.url)) {
    throw new Error('Remote data URLs are not loaded; inline the data with "values".');
  }
  return spec;
}

function makeRenderer(mode: 'vega' | 'vega-lite'): FenceRenderer {
  return {
    async render(code, el, ctx) {
      const spec = parseSpec(code);
      await embed(el, spec, {
        mode,
        renderer: 'svg',
        actions: false,
        theme: ctx.theme === 'dark' ? 'dark' : undefined,
        // vega's default expression engine uses `new Function`, which the CSP forbids.
        ast: true,
        expr: expressionInterpreter,
        config: { background: 'transparent' },
      });
    },
  };
}

export const vega = makeRenderer('vega');
export const vegaLite = makeRenderer('vega-lite');
export default vegaLite;
