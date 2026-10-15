/** Hover toolbar for rendered fences: toggle source, copy SVG, fullscreen. */

function button(label: string, title: string, onClick: () => void): HTMLButtonElement {
  const b = document.createElement('button');
  b.type = 'button';
  b.textContent = label;
  b.title = title;
  b.addEventListener('click', (e) => {
    e.stopPropagation();
    onClick();
  });
  return b;
}

function flash(b: HTMLButtonElement, text: string) {
  const original = b.textContent;
  b.textContent = text;
  setTimeout(() => (b.textContent = original), 1200);
}

export function addToolbar(fence: HTMLElement, source: string) {
  fence.querySelector(':scope > .om-fence-toolbar')?.remove();
  const output = fence.querySelector<HTMLElement>(':scope > .om-fence-output');
  if (!output) return;

  const bar = document.createElement('div');
  bar.className = 'om-fence-toolbar';

  const sourceView = document.createElement('pre');
  sourceView.className = 'om-fence-source-view';
  sourceView.hidden = true;
  const code = document.createElement('code');
  code.textContent = source;
  sourceView.append(code);

  const toggle = button('Source', 'Show source', () => {
    sourceView.hidden = !sourceView.hidden;
    output.hidden = !sourceView.hidden;
    toggle.textContent = sourceView.hidden ? 'Source' : 'Diagram';
  });
  bar.append(toggle);

  if (output.querySelector('svg')) {
    const copy = button('Copy SVG', 'Copy SVG markup', async () => {
      const svg = output.querySelector('svg')!;
      try {
        await navigator.clipboard.writeText(new XMLSerializer().serializeToString(svg));
        flash(copy, 'Copied');
      } catch {
        flash(copy, 'Failed');
      }
    });
    bar.append(copy, button('⤢', 'Fullscreen', () => openViewer(output.querySelector('svg')!)));
  }

  fence.append(bar, sourceView);
}

/** Fullscreen overlay with wheel zoom, drag to pan, Escape/click backdrop to close. */
function openViewer(svg: SVGSVGElement) {
  const overlay = document.createElement('div');
  overlay.className = 'om-viewer';
  overlay.tabIndex = -1;
  const stage = document.createElement('div');
  stage.className = 'om-viewer-stage';
  const clone = svg.cloneNode(true) as SVGSVGElement;
  clone.removeAttribute('width');
  clone.removeAttribute('height');
  clone.style.maxWidth = 'none';
  stage.append(clone);
  const hint = document.createElement('div');
  hint.className = 'om-viewer-hint';
  hint.textContent = 'Scroll to zoom · drag to pan · 0 to reset · Esc to close';
  overlay.append(stage, hint);
  document.body.append(overlay);

  let scale = 1;
  let x = 0;
  let y = 0;
  const apply = () => (stage.style.transform = `translate(${x}px, ${y}px) scale(${scale})`);
  const reset = () => {
    scale = 1;
    x = 0;
    y = 0;
    apply();
  };

  overlay.addEventListener('wheel', (e) => {
    e.preventDefault();
    const factor = Math.exp(-e.deltaY * 0.0015);
    const next = Math.min(20, Math.max(0.1, scale * factor));
    const rect = overlay.getBoundingClientRect();
    const cx = e.clientX - rect.width / 2 - x;
    const cy = e.clientY - rect.height / 2 - y;
    x -= cx * (next / scale - 1);
    y -= cy * (next / scale - 1);
    scale = next;
    apply();
  }, { passive: false });

  let drag: { px: number; py: number; moved: boolean } | undefined;
  overlay.addEventListener('pointerdown', (e) => {
    drag = { px: e.clientX, py: e.clientY, moved: false };
    overlay.setPointerCapture(e.pointerId);
  });
  overlay.addEventListener('pointermove', (e) => {
    if (!drag) return;
    x += e.clientX - drag.px;
    y += e.clientY - drag.py;
    drag.moved ||= Math.abs(e.clientX - drag.px) + Math.abs(e.clientY - drag.py) > 2;
    drag.px = e.clientX;
    drag.py = e.clientY;
    apply();
  });
  overlay.addEventListener('pointerup', (e) => {
    const clickedBackdrop = drag && !drag.moved && e.target === overlay;
    drag = undefined;
    if (clickedBackdrop) close();
  });

  const onKey = (e: KeyboardEvent) => {
    if (e.key === 'Escape') close();
    else if (e.key === '0') reset();
    else if (e.key === '+' || e.key === '=') { scale = Math.min(20, scale * 1.2); apply(); }
    else if (e.key === '-') { scale = Math.max(0.1, scale / 1.2); apply(); }
  };
  function close() {
    document.removeEventListener('keydown', onKey);
    overlay.remove();
  }
  document.addEventListener('keydown', onKey);
  overlay.focus();
}
