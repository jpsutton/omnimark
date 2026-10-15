import { expect, test, type Page } from '@playwright/test';

declare global {
  interface Window {
    omnimarkMock: { renders: number; dirty: boolean; headings: { id: string }[]; activeHeading: string; exported: string };
    omnimark: {
      userEdit(text: string): void;
      getText(): string;
      find(q: string, backward: boolean, cs: boolean): { active: number; total: number };
      exportHtml(inline: boolean): void;
      mode(): string;
    };
  }
}

async function open(page: Page, query = '') {
  await page.goto(`/?file=showcase.md${query}`);
  await page.waitForFunction(() => window.omnimarkMock?.renders > 0);
}

test('renders every embedded format without errors', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await open(page);
  const fences = page.locator('.om-fence');
  await expect(fences).toHaveCount(8);
  for (const renderer of ['mermaid', 'graphviz', 'vega-lite', 'abc', 'wavedrom', 'nomnoml']) {
    await expect(page.locator(`.om-fence-rendered[data-renderer="${renderer}"] svg`).first()).toBeVisible();
  }
  await expect(page.locator('.om-fence-rendered[data-renderer="csv"] table.om-csv')).toBeVisible();
  // The deliberately broken mermaid block shows an inline error.
  await expect(page.locator('.om-fence-error')).toHaveCount(1);
  expect(errors).toEqual([]);
});

test('code blocks get headers, copy buttons and line numbers', async ({ page }) => {
  await open(page);
  const block = page.locator('.om-code-block').first();
  await expect(block.locator('.om-code-title')).toHaveText('greet.ts');
  await expect(block.locator('.om-copy')).toHaveCount(1);
  const numbered = await block.locator('.line').first().evaluate((el) => getComputedStyle(el, '::before').content);
  expect(numbered).not.toBe('none');
});

test('theme switch re-renders diagrams', async ({ page }) => {
  await open(page, '&theme=light');
  const before = await page.locator('.om-fence[data-renderer="mermaid"] svg').first().innerHTML();
  await page.evaluate(() => (window as any).omnimarkMock.themeChanged.emit('dark'));
  await expect(page.locator('html')).toHaveAttribute('data-theme', 'dark');
  await expect.poll(() => page.locator('.om-fence[data-renderer="mermaid"] svg').first().innerHTML()).not.toBe(before);
});

test('editing in split view updates the preview and marks dirty', async ({ page }) => {
  await open(page, '&mode=split');
  await expect(page.locator('#editor-pane .cm-content')).toBeVisible();
  await page.evaluate(() => window.omnimark.userEdit('# Changed title\n\nbody'));
  await expect(page.locator('#preview h1')).toHaveText('Changed title');
  expect(await page.evaluate(() => window.omnimarkMock.dirty)).toBe(true);
});

test('typing in the editor works', async ({ page }) => {
  await open(page, '&mode=split');
  await page.locator('#editor-pane .cm-content').click();
  await page.keyboard.press('Control+Home');
  await page.keyboard.type('Typed ');
  await expect(page.locator('#preview')).toContainText('Typed');
});

test('scroll sync follows the editor in split view', async ({ page }) => {
  await open(page, '&mode=split');
  await page.evaluate(() => {
    const scroller = document.querySelector('#editor-pane .cm-scroller') as HTMLElement;
    scroller.scrollTop = scroller.scrollHeight / 2;
  });
  await expect.poll(() => page.locator('#preview-pane').evaluate((el) => el.scrollTop)).toBeGreaterThan(200);
});

test('scroll-spy reports the active heading', async ({ page }) => {
  await open(page);
  await page.evaluate(() => document.getElementById('math')!.scrollIntoView());
  await expect.poll(() => page.evaluate(() => window.omnimarkMock.activeHeading)).toBe('math');
});

test('find in preview highlights and counts matches', async ({ page }) => {
  await open(page);
  const result = await page.evaluate(() => window.omnimark.find('mermaid', false, false));
  expect(result.total).toBeGreaterThan(1);
  expect(await page.evaluate(() => CSS.highlights.has('om-find'))).toBe(true);
});

test('diagram fullscreen viewer opens and closes', async ({ page }) => {
  await open(page);
  const fence = page.locator('.om-fence[data-renderer="graphviz"]');
  await fence.hover();
  await fence.getByTitle('Fullscreen').click();
  await expect(page.locator('.om-viewer')).toBeVisible();
  await page.keyboard.press('Escape');
  await expect(page.locator('.om-viewer')).toHaveCount(0);
});

test('csv tables sort on header click', async ({ page }) => {
  await open(page);
  const header = page.locator('table.om-csv th', { hasText: 'Stars' });
  await header.click();
  await header.click();
  await expect(page.locator('table.om-csv tbody tr').first().locator('td').first()).toHaveText('mermaid');
});

test('standalone HTML export inlines styles and strips app chrome', async ({ page }) => {
  await open(page);
  await page.evaluate(() => window.omnimark.exportHtml(true));
  await expect.poll(() => page.evaluate(() => window.omnimarkMock.exported.length)).toBeGreaterThan(10_000);
  const html = await page.evaluate(() => window.omnimarkMock.exported);
  expect(html.startsWith('<!doctype html>')).toBe(true);
  expect(html).toContain('url("data:font/');
  expect(html).not.toContain('om-fence-toolbar"');
});

test('extra formats: maps, 3D, chemistry, and opt-in PlantUML', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto('/?file=formats-extra.md');
  await page.waitForFunction(() => window.omnimarkMock?.renders > 0);
  await expect(page.locator('.om-fence-rendered[data-renderer="geojson"] .leaflet-container')).toBeVisible();
  await expect(page.locator('.om-fence-rendered[data-renderer="geojson"] path.leaflet-interactive')).toHaveCount(2);
  await expect(page.locator('.om-fence-rendered[data-renderer="topojson"] path.leaflet-interactive')).toHaveCount(1);
  await expect(page.locator('.om-fence-rendered[data-renderer="smiles"] figure svg')).toHaveCount(2);
  // WebGL may be missing in headless CI; then STL must fail gracefully with an inline error.
  await expect(page.locator('.om-fence[data-renderer="stl"]')).toHaveClass(/om-fence-(rendered|error)/);
  // PlantUML is off by default: inline explanation, no network request.
  await expect(page.locator('.om-fence-error[data-renderer="plantuml"] .om-fence-error-message')).toContainText('PlantUML rendering is off');
  expect(errors).toEqual([]);
});

test('find handles characters whose lower case is longer (İ)', async ({ page }) => {
  await open(page);
  await page.evaluate(() => {
    const p = document.createElement('p');
    p.textContent = 'İstanbul and istanbul';
    document.getElementById('preview')!.prepend(p);
  });
  const r = await page.evaluate(() => window.omnimark.find('istanbul', false, false));
  expect(r.total).toBeGreaterThanOrEqual(1);
});

test('exported links to local files never become javascript: URLs', async ({ page }) => {
  await open(page);
  await page.evaluate(() => {
    const a = document.createElement('a');
    a.href = 'omnimark://local/home/me/javascript%3Aalert(1)';
    a.textContent = 'x';
    document.getElementById('preview')!.append(a);
    window.omnimark.exportHtml(false);
  });
  await expect.poll(() => page.evaluate(() => window.omnimarkMock.exported.length)).toBeGreaterThan(1000);
  const html = await page.evaluate(() => window.omnimarkMock.exported);
  expect(html).not.toMatch(/href="javascript:/i);
  expect(html).toContain('href="./javascript%3Aalert(1)"');
});

test('re-renders reuse unchanged blocks and keep diagrams', async ({ page }) => {
  await open(page, '&mode=split');
  await page.evaluate(() => {
    (document.querySelector('.om-fence[data-renderer="graphviz"] svg') as any).__marker = 'kept';
    (document.querySelector('.om-code-block') as any).__marker = 'kept';
  });
  const before = await page.evaluate(() => window.omnimarkMock.renders);
  // Insert two lines above the title: every line number below shifts by 2.
  await page.evaluate(() => {
    const text = window.omnimark.getText();
    const at = text.indexOf('# Omnimark showcase');
    window.omnimark.userEdit(text.slice(0, at) + 'Intro line\n\n' + text.slice(at));
  });
  await expect.poll(() => page.evaluate(() => window.omnimarkMock.renders)).toBeGreaterThan(before);
  const state = await page.evaluate(() => ({
    svg: (document.querySelector('.om-fence[data-renderer="graphviz"] svg') as any)?.__marker,
    code: (document.querySelector('.om-code-block') as any)?.__marker,
    firstText: document.querySelector('#preview > p')?.textContent,
    created: (window.omnimark as any).timings().created,
    blocks: (window.omnimark as any).timings().blocks,
    headingLine: document.querySelector('#preview h1')?.getAttribute('data-source-line'),
    codeLines: [document.querySelector('.om-code-block')!.getAttribute('data-source-line'),
                document.querySelector('.om-code-block pre')!.getAttribute('data-source-line')],
  }));
  expect(state.svg).toBe('kept');
  expect(state.code).toBe('kept');
  expect(state.firstText).toBe('Intro line');
  expect(state.created).toBeLessThan(state.blocks / 4);
  // Front matter is 7 lines; the title moved from line 9 to 11.
  expect(state.headingLine).toBe('11');
  expect(state.codeLines[0]).toBe(state.codeLines[1]);
});

test('editor formatting shortcuts wrap and unwrap the selection', async ({ page }) => {
  await open(page, '&mode=split');
  await page.evaluate(() => window.omnimark.userEdit('hello world'));
  const content = page.locator('#editor-pane .cm-content');
  await content.click();
  await page.keyboard.press('Control+End');
  for (let i = 0; i < 5; i++) await page.keyboard.press('Shift+ArrowLeft');
  await page.keyboard.press('Control+b');
  expect(await page.evaluate(() => window.omnimark.getText())).toBe('hello **world**');
  await page.keyboard.press('Control+b');
  expect(await page.evaluate(() => window.omnimark.getText())).toBe('hello world');
  await page.keyboard.press('Control+k');
  expect(await page.evaluate(() => window.omnimark.getText())).toBe('hello [world](url)');
  await page.keyboard.type('https://example.com');
  expect(await page.evaluate(() => window.omnimark.getText())).toBe('hello [world](https://example.com)');
});

test('pasting an image saves it and inserts a link', async ({ page }) => {
  await open(page, '&mode=split');
  await page.evaluate(() => window.omnimark.userEdit(''));
  await page.locator('#editor-pane .cm-content').click();
  await page.evaluate(() => {
    const bytes = Uint8Array.from(atob('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=='), (c) => c.charCodeAt(0));
    const data = new DataTransfer();
    data.items.add(new File([bytes], 'shot.png', { type: 'image/png' }));
    const event = new ClipboardEvent('paste', { clipboardData: data, bubbles: true, cancelable: true });
    document.querySelector('#editor-pane .cm-content')!.dispatchEvent(event);
  });
  await expect.poll(() => page.evaluate(() => window.omnimark.getText())).toBe('![](images/pasted-1.png)');
  expect(await page.evaluate(() => (window as any).omnimarkMock.savedImages[0])).toBe('iVBORw0KGgoAAAAN');
});
