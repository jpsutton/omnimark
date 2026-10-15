import type { Root as MdRoot } from 'mdast';
import type { Root, Element, ElementContent } from 'hast';
import { parse as parseYaml } from 'yaml';
import { parse as parseToml } from 'smol-toml';
import type { VFile } from 'vfile';

interface FrontMatter {
  kind: 'yaml' | 'toml';
  raw: string;
}

/** Pulls the front matter node out of mdast and stashes it on the file. */
export function remarkExtractFrontMatter() {
  return (tree: MdRoot, file: VFile) => {
    const first = tree.children[0];
    if (first && (first.type === 'yaml' || (first.type as string) === 'toml')) {
      file.data.frontMatter = {
        kind: first.type as 'yaml' | 'toml',
        raw: (first as { value: string }).value,
      } satisfies FrontMatter;
      tree.children.shift();
    }
  };
}

function text(value: string): ElementContent {
  return { type: 'text', value };
}

function el(tagName: string, className: string[] | null, children: ElementContent[]): Element {
  return { type: 'element', tagName, properties: className ? { className } : {}, children };
}

function renderValue(value: unknown): ElementContent {
  if (value === null || value === undefined) return text('');
  if (Array.isArray(value)) {
    return el('ul', null, value.map((v) => el('li', null, [renderValue(v)])));
  }
  if (value instanceof Date) return text(value.toISOString().slice(0, 10));
  if (typeof value === 'object') return renderTable(value as Record<string, unknown>);
  return text(String(value));
}

function renderTable(data: Record<string, unknown>): Element {
  const rows = Object.entries(data).map(([key, value]) =>
    el('tr', null, [el('th', null, [text(key)]), el('td', null, [renderValue(value)])]),
  );
  return el('table', null, [el('tbody', null, rows)]);
}

/** Renders stashed front matter as a collapsible table at the top of the document. */
export function rehypeFrontMatter() {
  return (tree: Root, file: VFile) => {
    const fm = file.data.frontMatter as FrontMatter | undefined;
    if (!fm) return;
    let body: ElementContent;
    try {
      const data = fm.kind === 'yaml' ? parseYaml(fm.raw) : parseToml(fm.raw);
      body = data && typeof data === 'object' && !Array.isArray(data)
        ? renderTable(data as Record<string, unknown>)
        : el('pre', null, [el('code', null, [text(fm.raw)])]);
    } catch {
      body = el('pre', null, [el('code', null, [text(fm.raw)])]);
    }
    const details = el('details', ['om-frontmatter'], [
      el('summary', null, [text('Front matter')]),
      body,
    ]);
    tree.children.unshift(details);
  };
}

declare module 'vfile' {
  interface DataMap {
    frontMatter: FrontMatter;
  }
}
