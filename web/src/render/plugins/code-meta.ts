import type { Root, Code } from 'mdast';
import { visit } from 'unist-util-visit';

/**
 * Carries the fence info string after the language (```ts title="a.ts" {1,3})
 * through to hast as data-meta, which survives sanitizing.
 */
export function remarkCodeMeta() {
  return (tree: Root) => {
    visit(tree, 'code', (node: Code) => {
      if (!node.meta) return;
      node.data ??= {};
      node.data.hProperties = { ...(node.data.hProperties ?? {}), dataMeta: node.meta };
    });
  };
}
