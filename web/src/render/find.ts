/**
 * Find-in-preview using the CSS Custom Highlight API, so matches are painted
 * without touching the DOM (re-renders and diagram hydration stay intact).
 */

export interface FindResult {
  active: number; // 1-based, 0 when no matches
  total: number;
}

const ALL = 'om-find';
const CURRENT = 'om-find-current';

let ranges: Range[] = [];
let index = -1;
let lastKey = '';

function collect(root: HTMLElement, query: string, caseSensitive: boolean): Range[] {
  const found: Range[] = [];
  if (!query) return found;
  // Match on the original text: lower-casing can change string length
  // (e.g. "İ"), which would make offsets point at the wrong characters.
  const pattern = new RegExp(query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), caseSensitive ? 'gu' : 'giu');
  const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {
    acceptNode(node) {
      const parent = node.parentElement;
      if (!parent || parent.closest('.om-fence-toolbar, .om-copy, script, style')) return NodeFilter.FILTER_REJECT;
      if (parent.closest('[hidden]')) return NodeFilter.FILTER_REJECT;
      return NodeFilter.FILTER_ACCEPT;
    },
  });
  for (let node = walker.nextNode(); node; node = walker.nextNode()) {
    const text = node.nodeValue ?? '';
    for (const match of text.matchAll(pattern)) {
      if (!match[0]) continue;
      const range = document.createRange();
      range.setStart(node, match.index);
      range.setEnd(node, match.index + match[0].length);
      found.push(range);
    }
  }
  return found;
}

function paint() {
  const registry = CSS.highlights;
  registry.delete(ALL);
  registry.delete(CURRENT);
  if (!ranges.length) return;
  registry.set(ALL, new Highlight(...ranges));
  if (index >= 0) registry.set(CURRENT, new Highlight(ranges[index]));
}

function reveal(container: HTMLElement) {
  const range = ranges[index];
  if (!range) return;
  const rect = range.getBoundingClientRect();
  const box = container.getBoundingClientRect();
  if (rect.top < box.top + 40 || rect.bottom > box.bottom - 40) {
    container.scrollTop += rect.top - box.top - box.height / 3;
  }
}

/**
 * Steps to the next/previous match of `query`. A changed query (or `refresh`)
 * recomputes matches and keeps the nearest one to the current position.
 */
export function find(
  root: HTMLElement,
  container: HTMLElement,
  query: string,
  backward: boolean,
  caseSensitive: boolean,
  refresh = false,
): FindResult {
  const key = `${caseSensitive ? 1 : 0}\u0000${query}`;
  if (key !== lastKey || refresh) {
    const previousTop = ranges[index]?.getBoundingClientRect().top;
    ranges = collect(root, query, caseSensitive);
    lastKey = key;
    // Start from the first match at or below the top of the viewport.
    const anchorTop = previousTop ?? container.getBoundingClientRect().top;
    index = ranges.findIndex((r) => r.getBoundingClientRect().top >= anchorTop - 1);
    if (index < 0) index = ranges.length ? 0 : -1;
  } else if (ranges.length) {
    index = (index + (backward ? -1 : 1) + ranges.length) % ranges.length;
  }
  paint();
  if (!refresh) reveal(container);
  return { active: index + 1, total: ranges.length };
}

export function clearFind() {
  ranges = [];
  index = -1;
  lastKey = '';
  CSS.highlights.delete(ALL);
  CSS.highlights.delete(CURRENT);
}
