export type Theme = 'light' | 'dark';

export interface FenceContext {
  theme: Theme;
  /** Network content allowed (map tiles); follows the "remote images" setting. */
  allowNetwork: boolean;
  /** PlantUML server base URL, e.g. https://www.plantuml.com/plantuml; empty = disabled. */
  plantumlServer: string;
}

export interface FenceRenderer {
  /** Renders `code` into `el`. Throwing shows an error box with the source. */
  render(code: string, el: HTMLElement, ctx: FenceContext): Promise<void>;
  /**
   * Output is static markup that can be restored from cache with innerHTML.
   * Interactive renderers (event listeners, live views) must leave this unset.
   */
  cacheable?: boolean;
}

type Loader = () => Promise<{ default: FenceRenderer }>;

// Keys match the renderer ids in ../fence-langs.ts. Each loader is a dynamic
// import, so a renderer's library is fetched only when a document uses it.
const loaders: Record<string, Loader> = {
  mermaid: () => import('./mermaid'),
  graphviz: () => import('./graphviz'),
  vega: () => import('./vega').then((m) => ({ default: m.vega })),
  'vega-lite': () => import('./vega').then((m) => ({ default: m.vegaLite })),
  csv: () => import('./csv').then((m) => ({ default: m.csv })),
  tsv: () => import('./csv').then((m) => ({ default: m.tsv })),
  abc: () => import('./abc'),
  wavedrom: () => import('./wavedrom'),
  nomnoml: () => import('./nomnoml'),
  geojson: () => import('./map').then((m) => ({ default: m.geojson })),
  topojson: () => import('./map').then((m) => ({ default: m.topojson })),
  stl: () => import('./stl'),
  smiles: () => import('./smiles'),
  plantuml: () => import('./plantuml'),
};

const loaded = new Map<string, Promise<FenceRenderer>>();

export function getRenderer(id: string): Promise<FenceRenderer> | undefined {
  const loader = loaders[id];
  if (!loader) return undefined;
  let renderer = loaded.get(id);
  if (!renderer) {
    renderer = loader().then((m) => m.default);
    loaded.set(id, renderer);
  }
  return renderer;
}
