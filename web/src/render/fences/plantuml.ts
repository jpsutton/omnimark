import { encode } from 'plantuml-encoder';
import type { FenceRenderer } from './registry';

/**
 * PlantUML needs a server to render. Sending a document's diagrams to a
 * third party is opt-in (Settings → PlantUML server).
 */
const renderer: FenceRenderer = {
  async render(code, el, ctx) {
    if (!ctx.plantumlServer) {
      throw new Error('PlantUML rendering is off. Set a PlantUML server in Settings to render this diagram.');
    }
    if (!ctx.allowNetwork) {
      throw new Error('PlantUML needs network access, which is off (Settings → Load remote images).');
    }
    const base = ctx.plantumlServer.replace(/\/+$/, '');
    const img = document.createElement('img');
    img.alt = 'PlantUML diagram';
    img.className = 'om-plantuml';
    img.referrerPolicy = 'no-referrer';
    img.src = `${base}/svg/${encode(code)}`;
    await new Promise<void>((resolve, reject) => {
      img.onload = () => resolve();
      img.onerror = () => reject(new Error(`Could not load the diagram from ${base}.`));
    });
    el.append(img);
  },
};

export default renderer;
