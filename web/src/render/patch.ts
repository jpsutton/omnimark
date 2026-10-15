/**
 * Applies freshly rendered HTML to the preview by reusing top-level blocks
 * whose markup is unchanged. Keeps hydrated diagrams, code block chrome and
 * open <details> intact, and avoids rebuilding a huge DOM on every keystroke.
 *
 * Blocks are matched on their HTML with data-source-line removed, because an
 * edit near the top shifts the line numbers of everything below it. Reused
 * blocks get their line attributes updated in place.
 */

const LINE_ATTR = /\sdata-source-line="\d+"/g;

// Key each inserted node was created from.
const keys = new WeakMap<Node, string>();
// Elements carrying data-source-line, captured before hydration rearranged them.
const lineNodes = new WeakMap<Element, Element[]>();

function collectLines(el: Element): Element[] {
  const list = el.hasAttribute('data-source-line') ? [el] : [];
  list.push(...el.querySelectorAll('[data-source-line]'));
  return list;
}

function keyFor(node: Node): string {
  if (node.nodeType === Node.ELEMENT_NODE) return `E${(node as Element).outerHTML.replace(LINE_ATTR, '')}`;
  return `T${node.textContent ?? ''}`;
}

function syncLines(existing: Element, fresh: Element) {
  const targets = lineNodes.get(existing);
  const sources = collectLines(fresh);
  if (!targets || targets.length !== sources.length) return;
  for (let i = 0; i < targets.length; i++) {
    const line = sources[i].getAttribute('data-source-line')!;
    if (targets[i].getAttribute('data-source-line') === line) continue;
    targets[i].setAttribute('data-source-line', line);
    // hydrate.ts copies the line onto the code block wrapper.
    const wrapper = targets[i].parentElement;
    if (wrapper?.classList.contains('om-code-block')) wrapper.setAttribute('data-source-line', line);
  }
}

/** Returns the number of blocks that were newly created. */
export function patchHtml(container: HTMLElement, html: string): number {
  const template = document.createElement('template');
  template.innerHTML = html;

  const pool = new Map<string, Node[]>();
  for (const node of container.childNodes) {
    const key = keys.get(node);
    if (key === undefined) continue;
    const bucket = pool.get(key);
    if (bucket) bucket.push(node);
    else pool.set(key, [node]);
  }

  const next: Node[] = [];
  let created = 0;
  for (const node of [...template.content.childNodes]) {
    const key = keyFor(node);
    const reuse = pool.get(key)?.shift();
    if (reuse) {
      if (reuse.nodeType === Node.ELEMENT_NODE) syncLines(reuse as Element, node as Element);
      next.push(reuse);
    } else {
      keys.set(node, key);
      if (node.nodeType === Node.ELEMENT_NODE) lineNodes.set(node as Element, collectLines(node as Element));
      next.push(node);
      created++;
    }
  }

  const keep = new Set(next);
  for (const node of [...container.childNodes]) {
    if (!keep.has(node)) node.remove();
  }
  let cursor = container.firstChild;
  for (const node of next) {
    if (node === cursor) {
      cursor = cursor.nextSibling;
      continue;
    }
    container.insertBefore(node, cursor);
  }
  return created;
}

/** Lets `replacement` (e.g. a wrapper hydrate.ts put around `original`) be reused like it. */
export function transferIdentity(original: Element, replacement: Element) {
  const key = keys.get(original);
  if (key === undefined) return;
  keys.set(replacement, key);
  const lines = lineNodes.get(original);
  if (lines) lineNodes.set(replacement, lines);
}

/** Forgets reuse state, e.g. after a theme change that must redraw everything. */
export function resetPatch(container: HTMLElement) {
  container.replaceChildren();
}
