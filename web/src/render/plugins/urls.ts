import type { Root, Element } from 'hast';
import { visit } from 'unist-util-visit';

const URL_ATTRS: Record<string, string> = {
  a: 'href',
  img: 'src',
  source: 'src',
  video: 'src',
  audio: 'src',
};

export function isRelativeUrl(value: string): boolean {
  if (!value || value.startsWith('#') || value.startsWith('//')) return false;
  return !/^[a-z][a-z0-9+.-]*:/i.test(value);
}

/** Resolves document-relative links and media against the document's directory. */
export function rehypeResolveUrls(options: { baseUrl: string }) {
  return (tree: Root) => {
    if (!options.baseUrl) return;
    visit(tree, 'element', (node: Element) => {
      const attr = URL_ATTRS[node.tagName];
      if (!attr) return;
      const value = node.properties[attr];
      if (typeof value !== 'string' || !isRelativeUrl(value)) return;
      try {
        node.properties[attr] = new URL(value, options.baseUrl).toString();
      } catch {
        // leave unresolvable values untouched
      }
    });
  };
}

/** Replaces remote images with a link so rendering never contacts other hosts. */
export function rehypeBlockRemoteImages(options: { allow: boolean }) {
  return (tree: Root) => {
    if (options.allow) return;
    visit(tree, 'element', (node: Element, index, parent) => {
      if (node.tagName !== 'img' || index === undefined || !parent) return;
      const src = node.properties.src;
      if (typeof src !== 'string' || !/^https?:/i.test(src)) return;
      const alt = typeof node.properties.alt === 'string' && node.properties.alt ? node.properties.alt : src;
      parent.children[index] = {
        type: 'element',
        tagName: 'a',
        properties: { href: src, className: ['om-remote-image'], title: 'Remote image not loaded' },
        children: [{ type: 'text', value: alt }],
      };
    });
  };
}
