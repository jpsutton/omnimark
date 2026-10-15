import type { Root, Element, ElementContent } from 'hast';
import { visit } from 'unist-util-visit';

const TYPES: Record<string, string> = {
  NOTE: 'Note',
  TIP: 'Tip',
  IMPORTANT: 'Important',
  WARNING: 'Warning',
  CAUTION: 'Caution',
};

const MARKER = /^\s*\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\][ \t]*\n?/i;

/** GitHub alerts: `> [!NOTE]` blockquotes become styled callouts. */
export function rehypeAlerts() {
  return (tree: Root) => {
    visit(tree, 'element', (node: Element) => {
      if (node.tagName !== 'blockquote') return;
      const first = node.children.find(
        (c): c is Element => c.type === 'element',
      );
      if (!first || first.tagName !== 'p') return;
      const text = first.children[0];
      if (!text || text.type !== 'text') return;
      const match = MARKER.exec(text.value);
      if (!match) return;

      const type = match[1].toUpperCase();
      text.value = text.value.slice(match[0].length);
      if (!text.value) first.children.shift();
      if (first.children.length && first.children[0].type === 'element'
        && first.children[0].tagName === 'br') {
        first.children.shift();
      }
      if (!first.children.length) node.children.splice(node.children.indexOf(first), 1);

      const title: ElementContent = {
        type: 'element',
        tagName: 'p',
        properties: { className: ['markdown-alert-title'] },
        children: [{ type: 'text', value: TYPES[type] }],
      };
      node.tagName = 'div';
      node.properties = {
        ...node.properties,
        className: ['markdown-alert', `markdown-alert-${type.toLowerCase()}`],
      };
      node.children = [title, ...node.children];
    });
  };
}
