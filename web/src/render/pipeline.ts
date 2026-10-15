import { unified, type Processor } from 'unified';
import remarkParse from 'remark-parse';
import remarkGfm from 'remark-gfm';
import remarkFrontmatter from 'remark-frontmatter';
import remarkMath from 'remark-math';
import remarkEmoji from 'remark-emoji';
import { remarkDefinitionList, defListHastHandlers } from 'remark-definition-list';
import remarkFlexibleMarkers from 'remark-flexible-markers';
import remarkRehype from 'remark-rehype';
import rehypeRaw from 'rehype-raw';
import rehypeSanitize, { defaultSchema } from 'rehype-sanitize';
import rehypeSlug from 'rehype-slug';
import rehypeKatex from 'rehype-katex';
import rehypeStringify from 'rehype-stringify';
import type { Root, Element } from 'hast';
import { visit } from 'unist-util-visit';
import { toString } from 'hast-util-to-string';
import type { Schema } from 'hast-util-sanitize';
import { VFile } from 'vfile';

import { remarkCodeMeta } from './plugins/code-meta';
import { remarkWikiLinks } from './plugins/wiki-links';
import { remarkSubSuper } from './plugins/sub-super';
import { remarkExtractFrontMatter, rehypeFrontMatter } from './plugins/frontmatter';
import { rehypeSourceLines } from './plugins/source-lines';
import { rehypeBlockRemoteImages, rehypeResolveUrls } from './plugins/urls';
import { rehypeAlerts } from './plugins/alerts';
import { rehypeShiki } from './plugins/shiki';
import type { Heading } from './types';

/**
 * User HTML is sanitized right after parsing. Everything after the sanitize
 * step (slugs, alerts, KaTeX, Shiki, front matter) is our own trusted output.
 */
export const sanitizeSchema: Schema = {
  ...defaultSchema,
  // Default clobbering: ids/names from user HTML get a `user-content-`
  // prefix so they can't collide with heading slugs (added after sanitize)
  // or app globals. Anchor lookups fall back to the prefixed id.
  tagNames: [
    ...(defaultSchema.tagNames ?? []),
    'mark', 'abbr', 'figure', 'figcaption', 'video', 'audio', 'source', 'u', 'center',
  ],
  attributes: {
    ...defaultSchema.attributes,
    '*': [...(defaultSchema.attributes?.['*'] ?? []), 'dataSourceLine', 'align', 'title'],
    code: [
      ...(defaultSchema.attributes?.code ?? []),
      ['className', /^language-./, 'math-inline', 'math-display'],
      'dataMeta',
    ],
    span: [...(defaultSchema.attributes?.span ?? []), ['className', 'math-inline', 'math-display']],
    div: [...(defaultSchema.attributes?.div ?? []), ['className', 'math-display']],
    mark: [['className', 'om-mark']],
    img: [...(defaultSchema.attributes?.img ?? []), 'width', 'height', 'loading'],
    video: ['src', 'controls', 'width', 'height', 'poster', 'loop', 'muted'],
    audio: ['src', 'controls', 'loop', 'muted'],
    source: ['src', 'type'],
    a: [
      ...(defaultSchema.attributes?.a ?? []).filter((a) => !(Array.isArray(a) && a[0] === 'className')),
      'title',
      ['className', 'data-footnote-backref', 'wikilink'],
    ],
  },
  protocols: {
    ...defaultSchema.protocols,
    src: ['http', 'https', 'data'],
  },
};

export interface PipelineOptions {
  baseUrl: string;
  remoteImages?: boolean;
}

export function createProcessor(options: PipelineOptions): Processor<any, any, any, any, string> {
  return unified()
    .use(remarkParse)
    .use(remarkFrontmatter, ['yaml', 'toml'])
    .use(remarkExtractFrontMatter)
    .use(remarkGfm, { singleTilde: false })
    .use(remarkMath)
    .use(remarkDefinitionList)
    .use(remarkFlexibleMarkers, { markerClassName: () => ['om-mark'] })
    .use(remarkEmoji, { accessible: false })
    .use(remarkSubSuper)
    .use(remarkCodeMeta)
    .use(remarkWikiLinks)
    .use(remarkRehype, { allowDangerousHtml: true, handlers: { ...defListHastHandlers } })
    .use(rehypeSourceLines)
    .use(rehypeRaw)
    .use(rehypeSanitize, sanitizeSchema)
    .use(rehypeResolveUrls, { baseUrl: options.baseUrl })
    .use(rehypeBlockRemoteImages, { allow: options.remoteImages !== false })
    .use(rehypeSlug)
    .use(rehypeAlerts)
    .use(rehypeKatex)
    .use(rehypeShiki)
    .use(rehypeFrontMatter)
    .use(rehypeStringify) as unknown as Processor<any, any, any, any, string>;
}

const HEADING = /^h([1-6])$/;

export function collectHeadings(tree: Root): Heading[] {
  const headings: Heading[] = [];
  visit(tree, 'element', (node: Element) => {
    const match = HEADING.exec(node.tagName);
    if (!match || typeof node.properties.id !== 'string') return;
    headings.push({
      level: Number(match[1]),
      id: node.properties.id,
      text: toString(node).trim(),
      line: Number(node.properties.dataSourceLine ?? 0),
    });
  });
  return headings;
}

export interface RenderOutput {
  html: string;
  headings: Heading[];
  words: number;
  characters: number;
}

const WORD = /[\p{L}\p{N}][\p{L}\p{N}'’-]*/gu;

/** Counts words in the rendered text, ignoring front matter and code blocks. */
export function countText(tree: Root): { words: number; characters: number } {
  let words = 0;
  let characters = 0;
  const walk = (node: Root | Element) => {
    for (const child of node.children) {
      if (child.type === 'text') {
        characters += child.value.length;
        words += child.value.match(WORD)?.length ?? 0;
      } else if (child.type === 'element') {
        const classes = child.properties.className;
        const skip = child.tagName === 'pre' || child.tagName === 'details'
          && Array.isArray(classes) && classes.includes('om-frontmatter');
        if (!skip && !(Array.isArray(classes) && classes.includes('katex-mathml'))) walk(child);
      }
    }
  };
  walk(tree);
  return { words, characters };
}

export async function renderMarkdown(markdown: string, options: PipelineOptions): Promise<RenderOutput> {
  const processor = createProcessor(options);
  const vfile = new VFile({ value: markdown });
  const mdast = processor.parse(vfile);
  const hast = (await processor.run(mdast, vfile)) as Root;
  const headings = collectHeadings(hast);
  const html = processor.stringify(hast, vfile);
  return { html, headings, ...countText(hast) };
}
