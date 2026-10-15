import { describe, expect, it } from 'vitest';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { renderMarkdown } from '../../src/render/pipeline';
import { isRelativeUrl } from '../../src/render/plugins/urls';
import { parseMeta } from '../../src/render/plugins/shiki';

const base = 'omnimark://local/home/me/notes/';
const render = (md: string) => renderMarkdown(md, { baseUrl: base });

describe('sanitizing', () => {
  it('strips scripts and event handlers from raw HTML', async () => {
    const { html } = await render(
      '<script>alert(1)</script>\n\n<img src="x.png" onerror="alert(2)">\n\n<a href="javascript:alert(3)">x</a>',
    );
    expect(html).not.toContain('<script');
    expect(html).not.toContain('onerror');
    expect(html).not.toContain('javascript:');
  });

  it('keeps safe HTML like details and kbd', async () => {
    const { html } = await render('<details><summary>S</summary>\n\nbody\n\n</details>\n\n<kbd>K</kbd>');
    expect(html).toContain('<details');
    expect(html).toContain('<summary>S</summary>');
    expect(html).toContain('<kbd>K</kbd>');
  });

  it('drops absolute omnimark:// URLs written by the user', async () => {
    const { html } = await render('![x](omnimark://local/etc/passwd)');
    expect(html).not.toContain('/etc/passwd');
  });
});

describe('urls', () => {
  it('resolves relative images and links against the document directory', async () => {
    const { html } = await render('![a](img/a.png) [b](../b.md#top) [c](https://x.org) [d](#sec)');
    expect(html).toContain('src="omnimark://local/home/me/notes/img/a.png"');
    expect(html).toContain('href="omnimark://local/home/me/b.md#top"');
    expect(html).toContain('href="https://x.org"');
    expect(html).toContain('href="#sec"');
  });

  it('classifies relative urls', () => {
    expect(isRelativeUrl('a/b.png')).toBe(true);
    expect(isRelativeUrl('../b.md')).toBe(true);
    expect(isRelativeUrl('#x')).toBe(false);
    expect(isRelativeUrl('https://x')).toBe(false);
    expect(isRelativeUrl('mailto:a@b')).toBe(false);
    expect(isRelativeUrl('//cdn/x')).toBe(false);
  });
});

describe('markdown features', () => {
  it('renders GFM tables, task lists, strikethrough and footnotes', async () => {
    const { html } = await render('| a | b |\n|---|---|\n| 1 | 2 |\n\n- [x] done\n\n~~gone~~ text[^1]\n\n[^1]: note');
    expect(html).toContain('<table');
    expect(html).toContain('type="checkbox"');
    expect(html).toContain('<del>gone</del>');
    expect(html).toContain('data-footnotes');
  });

  it('renders inline, display and fenced math with KaTeX', async () => {
    const { html } = await render('$a^2$\n\n$$\nb^2\n$$\n\n```math\nc^2\n```');
    expect(html.match(/class="katex"/g)?.length).toBeGreaterThanOrEqual(3);
    expect(html).toContain('katex-display');
  });

  it('turns GitHub alerts into callouts', async () => {
    const { html } = await render('> [!WARNING]\n> Careful.');
    expect(html).toContain('markdown-alert markdown-alert-warning');
    expect(html).toContain('<p class="markdown-alert-title">Warning</p>');
    expect(html).toContain('Careful.');
    expect(html).not.toContain('[!WARNING]');
  });

  it('renders YAML front matter as a table and removes it from the body', async () => {
    const { html } = await render('---\ntitle: Hi\ntags: [a, b]\n---\n\n# Body');
    expect(html).toContain('om-frontmatter');
    expect(html).toContain('<th>title</th>');
    expect(html).not.toMatch(/<hr>/);
  });

  it('collects headings with slugs and source lines', async () => {
    const { headings } = await render('# One\n\ntext\n\n## Two words');
    expect(headings).toEqual([
      { level: 1, id: 'one', text: 'One', line: 1 },
      { level: 2, id: 'two-words', text: 'Two words', line: 5 },
    ]);
  });

  it('tags blocks with source lines', async () => {
    const { html } = await render('para\n\n- item');
    expect(html).toContain('<p data-source-line="1">');
    expect(html).toContain('<ul data-source-line="3">');
  });
});

describe('code blocks', () => {
  it('highlights with dual themes and keeps meta', async () => {
    const { html } = await render('```ts title="a.ts" {1}\nconst a = 1;\nconst b = 2;\n```');
    expect(html).toContain('class="shiki shiki-themes github-light github-dark');
    expect(html).toMatch(/<pre [^>]*class="[^"]*\bom-code\b/);
    expect(html).toContain('--shiki-dark');
    expect(html).toContain('data-title="a.ts"');
    expect(html).toContain('data-lang="ts"');
    expect(html).toMatch(/class="line highlighted"/);
  });

  it('falls back to plain text for unknown languages', async () => {
    const { html } = await render('```nosuchlang\nhello\n```');
    expect(html).toContain('data-lang="nosuchlang"');
    expect(html).toContain('hello');
  });

  it('turns diagram fences into placeholders', async () => {
    const { html } = await render('```mermaid\ngraph TD; A-->B\n```\n\n```dot\ndigraph{a->b}\n```');
    expect(html).toContain('class="om-fence" data-renderer="mermaid"');
    expect(html).toContain('data-renderer="graphviz"');
    expect(html).toContain('<code>graph TD; A-->B</code>');
  });

  it('parses fence meta', () => {
    expect(parseMeta('title="x.ts" {1,3} showLineNumbers')).toMatchObject({ title: 'x.ts', lineNumbers: true });
    expect(parseMeta("filename='y.py' noLineNumbers")).toMatchObject({ title: 'y.py', lineNumbers: false });
    expect(parseMeta('startLine=10')).toMatchObject({ lineNumbers: true, startLine: 10 });
  });
});

describe('showcase sample', () => {
  it('renders without throwing', async () => {
    const md = await readFile(resolve(import.meta.dirname, '../../../samples/showcase.md'), 'utf8');
    const { html, headings } = await render(md);
    expect(html.length).toBeGreaterThan(1000);
    expect(headings[0]).toMatchObject({ level: 1, text: 'Omnimark showcase' });
  });
});

describe('wiki links', () => {
  it('turns [[links]] into relative markdown links', async () => {
    const { html } = await render('See [[Other Page]], [[notes/todo|my todos]] and [[Guide#Getting Started]].');
    expect(html).toContain('<a href="omnimark://local/home/me/notes/Other%20Page.md" class="wikilink">Other Page</a>');
    expect(html).toContain('href="omnimark://local/home/me/notes/notes/todo.md" class="wikilink">my todos</a>');
    expect(html).toContain('href="omnimark://local/home/me/notes/Guide.md#getting-started"');
  });

  it('leaves code alone', async () => {
    const { html } = await render('`[[not a link]]`');
    expect(html).not.toContain('wikilink');
  });
});

describe('remote images', () => {
  it('loads remote images by default', async () => {
    const { html } = await render('![cat](https://example.com/cat.png)');
    expect(html).toContain('<img src="https://example.com/cat.png"');
  });

  it('turns remote images into links when blocked, keeping local ones', async () => {
    const { html } = await renderMarkdown('![cat](https://example.com/cat.png) ![me](me.png)', {
      baseUrl: base,
      remoteImages: false,
    });
    expect(html).not.toContain('<img src="https://');
    expect(html).toContain('class="om-remote-image"');
    expect(html).toContain('src="omnimark://local/home/me/notes/me.png"');
  });
});

describe('id clobbering', () => {
  it('prefixes ids from raw HTML so they cannot shadow heading slugs', async () => {
    const { html } = await render('<a id="intro"></a>\n\n## Intro');
    expect(html).toContain('id="user-content-intro"');
    expect(html).toMatch(/<h2 [^>]*id="intro"/);
  });

  it('keeps footnote links resolvable through the user-content- fallback', async () => {
    const { html } = await render('Text[^1]\n\n[^1]: Note');
    const href = /href="#([^"]+)"[^>]*data-footnote-ref/.exec(html)?.[1];
    expect(href).toBeTruthy();
    // main.ts looks up `id`, then `user-content-${id}`.
    expect(html.includes(`id="${href}"`) || html.includes(`id="user-content-${href}"`)).toBe(true);
  });
});

describe('word count', () => {
  it('counts prose, not code, math markup or front matter', async () => {
    const { words } = await render('---\ntitle: Ignore me please\n---\n\nOne two three.\n\n```js\nconst not = counted;\n```\n\n$x$ four');
    // "One two three" + "four" + the visible KaTeX glyph for x.
    expect(words).toBeGreaterThanOrEqual(4);
    expect(words).toBeLessThanOrEqual(6);
  });
});

describe('extended inline syntax', () => {
  it('renders emoji shortcodes', async () => {
    const { html } = await render('Ship it :rocket: :tada:');
    expect(html).toContain('🚀');
    expect(html).toContain('🎉');
  });

  it('renders ==mark==', async () => {
    const { html } = await render('This is ==important== text.');
    expect(html).toMatch(/<mark[^>]*>important<\/mark>/);
  });

  it('renders ~sub~ and ^sup^ while keeping ~~strike~~', async () => {
    const { html } = await render('H~2~O, E=mc^2^, ~~gone~~, and `~code~`');
    expect(html).toContain('H<sub>2</sub>O');
    expect(html).toContain('mc<sup>2</sup>');
    expect(html).toContain('<del>gone</del>');
    expect(html).toContain('<code>~code~</code>');
  });

  it('renders definition lists', async () => {
    const { html } = await render('Term\n: The definition\n\nOther\n: Another one');
    expect(html).toContain('<dl');
    expect(html).toMatch(/<dt[^>]*>Term<\/dt>/);
    expect(html).toMatch(/<dd[^>]*>.*The definition/s);
  });
});

describe('toml front matter', () => {
  it('renders TOML front matter as a table', async () => {
    const { html } = await render('+++\ntitle = "Hi"\ntags = ["a", "b"]\n+++\n\n# Body');
    expect(html).toContain('<th>title</th>');
    expect(html).toContain('<td>Hi</td>');
  });
});
