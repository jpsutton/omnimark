import type { Root, Element } from 'hast';
import { visit } from 'unist-util-visit';
import { toString } from 'hast-util-to-string';
import {
  bundledLanguages,
  bundledLanguagesAlias,
  createHighlighter,
  createJavaScriptRegexEngine,
  type Highlighter,
} from 'shiki';
import {
  transformerMetaHighlight,
  transformerNotationDiff,
  transformerNotationFocus,
  transformerNotationHighlight,
  transformerNotationErrorLevel,
} from '@shikijs/transformers';
import { fenceRendererFor } from '../fence-langs';

export const THEMES = { light: 'github-light', dark: 'github-dark' } as const;

let highlighter: Promise<Highlighter> | undefined;

// Highlighting dominates re-render time for big documents; most code blocks
// are unchanged between keystrokes, so reuse their output.
const cache = new Map<string, Element>();
const CACHE_LIMIT = 1000;

function cached(key: string, build: () => Element): Element {
  let hit = cache.get(key);
  if (hit) {
    cache.delete(key); // refresh LRU position
  } else {
    hit = build();
    if (cache.size >= CACHE_LIMIT) cache.delete(cache.keys().next().value!);
  }
  cache.set(key, hit);
  return structuredClone(hit);
}

function getHighlighter(): Promise<Highlighter> {
  highlighter ??= createHighlighter({
    themes: [THEMES.light, THEMES.dark],
    langs: [],
    engine: createJavaScriptRegexEngine({ forgiving: true }),
  });
  return highlighter;
}

function isKnownLanguage(lang: string): boolean {
  return lang in bundledLanguages || lang in bundledLanguagesAlias;
}

async function ensureLanguage(hl: Highlighter, lang: string): Promise<string> {
  if (lang === 'text' || lang === 'plaintext' || lang === 'txt') return 'text';
  if (!isKnownLanguage(lang)) return 'text';
  if (!hl.getLoadedLanguages().includes(lang)) {
    try {
      await hl.loadLanguage(lang as keyof typeof bundledLanguages);
    } catch {
      return 'text';
    }
  }
  return lang;
}

export interface CodeMeta {
  title?: string;
  lineNumbers?: boolean;
  startLine?: number;
  raw: string;
}

export function parseMeta(raw: string | undefined): CodeMeta {
  const meta: CodeMeta = { raw: raw ?? '' };
  if (!raw) return meta;
  const title = /(?:title|filename)=(?:"([^"]*)"|'([^']*)'|(\S+))/.exec(raw);
  if (title) meta.title = title[1] ?? title[2] ?? title[3];
  if (/\bshowLineNumbers\b|\blineNumbers\b/.test(raw)) meta.lineNumbers = true;
  if (/\bnoLineNumbers\b/.test(raw)) meta.lineNumbers = false;
  const start = /\b(?:showLineNumbers|startLine)=(\d+)/.exec(raw);
  if (start) {
    meta.lineNumbers = true;
    meta.startLine = Number(start[1]);
  }
  return meta;
}

function languageOf(code: Element): string | undefined {
  const classes = code.properties.className;
  if (!Array.isArray(classes)) return undefined;
  for (const c of classes) {
    if (typeof c === 'string' && c.startsWith('language-')) return c.slice('language-'.length);
  }
  return undefined;
}

/**
 * Highlights fenced code with Shiki (dual light/dark themes as CSS variables)
 * and turns renderer fences (mermaid, dot, …) into placeholders for hydrate.ts.
 */
export function rehypeShiki() {
  return async (tree: Root) => {
    const jobs: Array<{ pre: Element; parent: Element | Root; index: number; code: Element }> = [];
    visit(tree, 'element', (node: Element, index, parent) => {
      if (node.tagName !== 'pre' || index === undefined || !parent) return;
      const code = node.children.find(
        (c): c is Element => c.type === 'element' && c.tagName === 'code',
      );
      if (!code) return;
      jobs.push({ pre: node, parent: parent as Element | Root, index, code });
    });
    if (!jobs.length) return;

    const hl = await getHighlighter();
    for (const { pre, parent, index, code } of jobs) {
      const rawLang = (languageOf(code) ?? 'text').toLowerCase();
      if (rawLang === 'math') continue; // rehype-katex owns these
      const source = toString(code).replace(/\n$/, '');
      const meta = parseMeta(code.properties.dataMeta as string | undefined);
      const sourceLine = pre.properties.dataSourceLine;

      const renderer = fenceRendererFor(rawLang);
      if (renderer) {
        parent.children[index] = {
          type: 'element',
          tagName: 'div',
          properties: {
            className: ['om-fence'],
            dataRenderer: renderer,
            dataLang: rawLang,
            dataSourceLine: sourceLine,
          },
          children: [{
            type: 'element',
            tagName: 'pre',
            properties: { className: ['om-fence-source'] },
            children: [{ type: 'element', tagName: 'code', properties: {}, children: [{ type: 'text', value: source }] }],
          }],
        };
        continue;
      }

      const lang = await ensureLanguage(hl, rawLang);
      const highlighted = cached(`${lang}\u0000${meta.raw}\u0000${source}`, () => {
        const out = hl.codeToHast(source, {
          lang,
          themes: THEMES,
          defaultColor: false,
          meta: { __raw: meta.raw },
          transformers: [
            transformerMetaHighlight(),
            transformerNotationDiff(),
            transformerNotationHighlight(),
            transformerNotationFocus(),
            transformerNotationErrorLevel(),
          ],
        });
        const pre = out.children[0] as Element;
        const classes = ['om-code'];
        if (meta.lineNumbers === true) classes.push('om-line-numbers');
        if (meta.lineNumbers === false) classes.push('om-no-line-numbers');
        // Shiki sets `class` as a string; normalize to hast's `className` list.
        const existing = pre.properties.class ?? pre.properties.className;
        delete pre.properties.class;
        pre.properties.className = [
          ...(Array.isArray(existing) ? existing.map(String) : String(existing ?? '').split(/\s+/)),
          ...classes,
        ].filter(Boolean);
        return pre;
      });
      highlighted.properties.dataLang = rawLang;
      highlighted.properties.dataSourceLine = sourceLine;
      if (meta.title) highlighted.properties.dataTitle = meta.title;
      if (meta.startLine && meta.startLine > 1) {
        const style = typeof highlighted.properties.style === 'string' ? highlighted.properties.style : '';
        highlighted.properties.style = `${style};--om-start-line:${meta.startLine - 1}`;
      }
      parent.children[index] = highlighted;
    }
  };
}
