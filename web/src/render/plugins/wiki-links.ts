import type { Root, Link, PhrasingContent } from 'mdast';
import { findAndReplace } from 'mdast-util-find-and-replace';

// [[Target]], [[Target|Label]], [[Target#Heading]], [[Target#Heading|Label]]
const WIKI_LINK = /\[\[([^[\]|#\n]+)(?:#([^[\]|\n]+))?(?:\|([^[\]\n]+))?\]\]/g;

function slug(text: string): string {
  return text.trim().toLowerCase().replace(/[^\p{L}\p{N}\s-]/gu, '').replace(/\s+/g, '-');
}

/**
 * Turns wiki links into ordinary relative links to `Target.md`. The app
 * resolves targets that aren't next to the current file by searching the
 * open workspace for a file with the same name.
 */
export function remarkWikiLinks() {
  return (tree: Root) => {
    findAndReplace(tree, [
      WIKI_LINK,
      (_match: string, target: string, heading?: string, label?: string): PhrasingContent => {
        const name = target.trim();
        const file = /\.[a-z0-9]+$/i.test(name) ? name : `${name}.md`;
        const url = encodeURI(file) + (heading ? `#${slug(heading)}` : '');
        const text = label?.trim() || (heading ? `${name} › ${heading.trim()}` : name);
        const link: Link = {
          type: 'link',
          url,
          children: [{ type: 'text', value: text }],
          data: { hProperties: { className: ['wikilink'] } },
        };
        return link;
      },
    ], { ignore: ['link', 'linkReference', 'inlineCode', 'code'] });
  };
}
