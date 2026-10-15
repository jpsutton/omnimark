import type { Root, PhrasingContent } from 'mdast';
import { findAndReplace } from 'mdast-util-find-and-replace';

// Pandoc-style H~2~O and E=mc^2^: no whitespace inside the markers.
// remark-gfm runs with singleTilde: false so ~x~ isn't strikethrough.
const SUB = /~([^~\s]+)~/g;
const SUP = /\^([^^\s]+)\^/g;

function wrap(tag: 'sub' | 'sup') {
  // An emphasis node renamed via hName: to-hast applies hName to element nodes.
  return (_match: string, value: string): PhrasingContent => ({
    type: 'emphasis',
    children: [{ type: 'text', value }],
    data: { hName: tag },
  });
}

export function remarkSubSuper() {
  return (tree: Root) => {
    findAndReplace(tree, [[SUB, wrap('sub')], [SUP, wrap('sup')]], {
      ignore: ['link', 'linkReference', 'inlineCode', 'code', 'math', 'inlineMath'],
    });
  };
}
