/**
 * Maps between editor source lines and preview scroll offsets using the
 * data-source-line attributes the render pipeline puts on block elements.
 */

interface Anchor {
  line: number;
  top: number;
}

function anchors(preview: HTMLElement, container: HTMLElement): Anchor[] {
  const base = container.getBoundingClientRect().top - container.scrollTop;
  const list: Anchor[] = [];
  for (const el of preview.querySelectorAll<HTMLElement>('[data-source-line]')) {
    const line = Number(el.dataset.sourceLine);
    if (!line || el.offsetParent === null) continue;
    list.push({ line, top: el.getBoundingClientRect().top - base });
  }
  list.sort((a, b) => a.line - b.line || a.top - b.top);
  // Keep tops monotonic so interpolation never runs backwards.
  const clean: Anchor[] = [];
  for (const a of list) {
    const last = clean[clean.length - 1];
    if (last && (a.line === last.line || a.top < last.top)) continue;
    clean.push(a);
  }
  return clean;
}

/** Preview scrollTop that shows source `line` at the top. */
export function previewOffsetForLine(preview: HTMLElement, container: HTMLElement, line: number): number {
  const list = anchors(preview, container);
  if (!list.length) return 0;
  let before = list[0];
  let after: Anchor | undefined;
  for (const a of list) {
    if (a.line <= line) before = a;
    else {
      after = a;
      break;
    }
  }
  if (line < before.line) return 0;
  const nextTop = after?.top ?? preview.scrollHeight;
  const nextLine = after?.line ?? before.line + 1;
  const ratio = (line - before.line) / Math.max(nextLine - before.line, 1);
  return before.top + ratio * (nextTop - before.top);
}

/** Source line (fractional) shown at the top of the preview. */
export function lineForPreviewOffset(preview: HTMLElement, container: HTMLElement, offset: number): number {
  const list = anchors(preview, container);
  if (!list.length) return 1;
  let before = list[0];
  let after: Anchor | undefined;
  for (const a of list) {
    if (a.top <= offset) before = a;
    else {
      after = a;
      break;
    }
  }
  if (offset < before.top) return 1;
  if (!after) return before.line;
  const ratio = (offset - before.top) / Math.max(after.top - before.top, 1);
  return before.line + ratio * (after.line - before.line);
}
