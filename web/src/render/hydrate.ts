import { getRenderer, type FenceContext, type Theme } from './fences/registry';
import { addToolbar } from './diagram-toolbar';
import { transferIdentity } from './patch';

// Cache rendered fence output by renderer + theme + source, so re-rendering
// the document (live reload, typing) doesn't redraw unchanged diagrams.
const cache = new Map<string, string>();
const CACHE_LIMIT = 200;

function cacheKey(renderer: string, theme: Theme, source: string) {
  return `${renderer}\u0000${theme}\u0000${source}`;
}

function remember(key: string, html: string) {
  if (cache.size >= CACHE_LIMIT) cache.delete(cache.keys().next().value!);
  cache.set(key, html);
}

function showError(el: HTMLElement, source: string, error: unknown) {
  el.classList.add('om-fence-error');
  el.replaceChildren();
  const message = document.createElement('div');
  message.className = 'om-fence-error-message';
  message.textContent = error instanceof Error ? error.message : String(error);
  const pre = document.createElement('pre');
  const code = document.createElement('code');
  code.textContent = source;
  pre.append(code);
  el.append(message, pre);
}

async function hydrateFence(el: HTMLElement, ctx: FenceContext) {
  const theme = ctx.theme;
  const id = el.dataset.renderer!;
  const source = el.querySelector('code')?.textContent ?? '';
  const key = cacheKey(id, theme, source);
  const cached = cache.get(key);
  const target = document.createElement('div');
  target.className = 'om-fence-output';

  if (cached !== undefined) {
    target.innerHTML = cached;
  } else {
    const renderer = getRenderer(id);
    if (!renderer) return;
    try {
      const r = await renderer;
      // Some renderers measure their container, so render while attached.
      el.append(target);
      await r.render(source, target, ctx);
      if (r.cacheable) remember(key, target.innerHTML);
    } catch (err) {
      showError(el, source, err);
      return;
    }
  }
  el.replaceChildren(target);
  el.classList.add('om-fence-rendered');
  addToolbar(el, source);
}

function decorateCodeBlocks(root: HTMLElement) {
  for (const pre of root.querySelectorAll<HTMLPreElement>('pre.om-code')) {
    if (pre.parentElement?.classList.contains('om-code-block')) continue;
    const wrapper = document.createElement('div');
    wrapper.className = 'om-code-block';
    const header = document.createElement('div');
    header.className = 'om-code-header';
    const label = document.createElement('span');
    label.className = 'om-code-title';
    label.textContent = pre.dataset.title ?? (pre.dataset.lang === 'text' ? '' : pre.dataset.lang ?? '');
    const copy = document.createElement('button');
    copy.type = 'button';
    copy.className = 'om-copy';
    copy.textContent = 'Copy';
    copy.addEventListener('click', async () => {
      const text = pre.querySelector('code')?.textContent ?? '';
      try {
        await navigator.clipboard.writeText(text);
        copy.textContent = 'Copied';
      } catch {
        copy.textContent = 'Failed';
      }
      setTimeout(() => (copy.textContent = 'Copy'), 1200);
    });
    header.append(label, copy);
    pre.replaceWith(wrapper);
    transferIdentity(pre, wrapper);
    wrapper.append(header, pre);
    if (pre.dataset.sourceLine) wrapper.dataset.sourceLine = pre.dataset.sourceLine;
  }
}

/** Post-processes freshly rendered HTML: code block chrome and fence renderers. */
export async function hydrate(root: HTMLElement, ctx: FenceContext): Promise<void> {
  decorateCodeBlocks(root);
  const fences = [...root.querySelectorAll<HTMLElement>('.om-fence:not(.om-fence-rendered)')];
  await Promise.all(fences.map((el) => hydrateFence(el, ctx)));
}
