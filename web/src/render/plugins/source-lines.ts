import type { Root, Element } from 'hast';
import { visit } from 'unist-util-visit';

const BLOCKS = new Set([
  'p', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'li', 'pre', 'blockquote', 'table', 'tr',
  'hr', 'ul', 'ol', 'dl', 'dt', 'dd', 'div', 'details', 'section',
]);

/** Tags block elements with data-source-line so editor and preview can scroll in sync. */
export function rehypeSourceLines() {
  return (tree: Root) => {
    visit(tree, 'element', (node: Element) => {
      const line = node.position?.start.line;
      if (line !== undefined && BLOCKS.has(node.tagName)) {
        node.properties.dataSourceLine = line;
      }
    });
  };
}
