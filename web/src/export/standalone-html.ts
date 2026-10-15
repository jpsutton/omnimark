/**
 * Builds a self-contained HTML file from the rendered preview: all CSS is
 * inlined, KaTeX fonts become data URIs, and (optionally) local images too.
 */

async function toDataUri(url: string): Promise<string | undefined> {
  try {
    const response = await fetch(url);
    if (!response.ok) return undefined;
    const blob = await response.blob();
    return await new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(reader.result as string);
      reader.onerror = () => reject(reader.error);
      reader.readAsDataURL(blob);
    });
  } catch {
    return undefined;
  }
}

/** Inlines url(...) references (fonts) in a stylesheet's text. */
async function inlineCssUrls(css: string, baseHref: string): Promise<string> {
  const pattern = /url\((['"]?)([^'")]+)\1\)/g;
  const urls = new Set<string>();
  for (const match of css.matchAll(pattern)) {
    if (!match[2].startsWith('data:')) urls.add(match[2]);
  }
  const replacements = new Map<string, string>();
  await Promise.all([...urls].map(async (raw) => {
    const absolute = new URL(raw, baseHref).toString();
    const data = await toDataUri(absolute);
    if (data) replacements.set(raw, data);
  }));
  return css.replace(pattern, (whole, _q, raw: string) => {
    const data = replacements.get(raw);
    return data ? `url("${data}")` : whole;
  });
}

async function collectCss(): Promise<string> {
  const parts: string[] = [];
  for (const sheet of [...document.styleSheets]) {
    let text = '';
    try {
      text = [...sheet.cssRules].map((r) => r.cssText).join('\n');
    } catch {
      continue; // cross-origin sheet
    }
    // App chrome (editor, split layout) isn't part of the document.
    if (/#editor-pane|\.cm-editor/.test(text) && !/markdown-body/.test(text)) continue;
    parts.push(await inlineCssUrls(text, sheet.href ?? location.href));
  }
  return parts.join('\n');
}

function escapeHtml(text: string): string {
  return text.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!);
}

export async function buildStandaloneHtml(preview: HTMLElement, options: { inlineImages: boolean; title: string }): Promise<string> {
  const clone = preview.cloneNode(true) as HTMLElement;
  clone.querySelectorAll('.om-fence-toolbar, .om-copy, .om-fence-source-view').forEach((el) => el.remove());
  clone.querySelectorAll('[hidden]').forEach((el) => el.removeAttribute('hidden'));
  clone.removeAttribute('aria-live');

  if (options.inlineImages) {
    await Promise.all([...clone.querySelectorAll<HTMLImageElement>('img[src^="omnimark:"]')].map(async (img) => {
      const data = await toDataUri(img.src);
      if (data) img.src = data;
    }));
  }
  // Links into the local filesystem can't work from a standalone file.
  // Keep the (still percent-encoded) file name so no scheme like
  // `javascript:` can appear after rewriting.
  clone.querySelectorAll<HTMLAnchorElement>('a[href^="omnimark:"]').forEach((a) => {
    const url = new URL(a.href);
    const name = url.pathname.split('/').pop() ?? '';
    a.setAttribute('href', `./${name.replace(/:/g, '%3A')}${url.hash}`);
  });

  const theme = document.documentElement.dataset.theme ?? 'light';
  const style = document.documentElement.dataset.style ?? 'github';
  const css = await collectCss();
  return `<!doctype html>
<html lang="en" data-theme="${theme}" data-style="${style}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="generator" content="Omnimark">
<title>${escapeHtml(options.title)}</title>
<style>
${css}
html, body { height: auto; overflow: visible; }
</style>
</head>
<body>
<main class="markdown-body">
${clone.innerHTML}
</main>
</body>
</html>
`;
}
