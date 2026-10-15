/**
 * Fence languages rendered by a client-side renderer (see fences/registry.ts)
 * instead of being syntax highlighted. Kept separate from the registry so the
 * worker can import it without pulling in DOM-dependent renderer code.
 */
export const FENCE_LANGS: Record<string, string> = {
  mermaid: 'mermaid',
  dot: 'graphviz',
  graphviz: 'graphviz',
  gv: 'graphviz',
  vega: 'vega',
  'vega-lite': 'vega-lite',
  vegalite: 'vega-lite',
  csv: 'csv',
  tsv: 'tsv',
  abc: 'abc',
  wavedrom: 'wavedrom',
  nomnoml: 'nomnoml',
  geojson: 'geojson',
  topojson: 'topojson',
  stl: 'stl',
  smiles: 'smiles',
  plantuml: 'plantuml',
  puml: 'plantuml',
};

export function fenceRendererFor(lang: string): string | undefined {
  return FENCE_LANGS[lang.toLowerCase()];
}
