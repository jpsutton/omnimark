import 'katex/dist/katex.min.css';
import './styles/markdown.css';
import './styles/app.css';
import './styles/editor.css';

import { connectBridge, type NativeBridge } from './bridge';
import { createEditor } from './editor/codemirror';
import { hydrate } from './render/hydrate';
import { patchHtml, resetPatch } from './render/patch';
import { clearFind, find } from './render/find';
import { buildStandaloneHtml } from './export/standalone-html';
import { openSearchPanel } from '@codemirror/search';
import { lineForPreviewOffset, previewOffsetForLine } from './render/scroll-sync';
import type { Theme } from './render/fences/registry';
import type { Heading, RenderRequest, RenderResult } from './render/types';

type Mode = 'preview' | 'split' | 'source';

interface Settings {
  previewStyle?: string;
  remoteImages?: boolean;
  customCss?: string;
  plantumlServer?: string;
  vim?: boolean;
  wrap?: boolean;
  lineNumbers?: boolean;
  editorFontSize?: number;
  previewFontSize?: number;
  previewWidth?: number;
  syncScroll?: boolean;
}

const app = document.getElementById('app') as HTMLElement;
const preview = document.getElementById('preview') as HTMLElement;
const previewPane = document.getElementById('preview-pane') as HTMLElement;
const editorPane = document.getElementById('editor-pane') as HTMLElement;
const divider = document.getElementById('divider') as HTMLElement;
const worker = new Worker(new URL('./render/worker.ts', import.meta.url), { type: 'module' });

let bridge: NativeBridge;
let theme: Theme = matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
let mode: Mode = 'preview';
let settings: Settings = { syncScroll: true, wrap: true };
let baseUrl = '';
let currentPath: string | undefined;
let savedText = '';
// Text handed to C++ per save request, so markSaved() records exactly what was written.
const pendingSaves = new Map<number, string>();
let nextSaveId = 0;
let dirty = false;
let requestId = 0;
let renderStarted = 0;
let renderTimer: number | undefined;
let headings: Heading[] = [];
let timings = { worker: 0, transfer: 0, apply: 0, hydrate: 0, created: 0, blocks: 0 };

// Scroll sync: whichever pane the user scrolled last drives the other one.
// Scroll events caused by our own writes are recognized by their position.
let scrollDriver: 'editor' | 'preview' = 'preview';
let expectedEditorTop: number | undefined;
let expectedPreviewTop: number | undefined;

function isEcho(actual: number, expected: number | undefined): boolean {
  return expected !== undefined && Math.abs(actual - expected) <= 1;
}

const editor = createEditor(editorPane, {
  onChange(text) {
    setDirty(text !== savedText);
    window.clearTimeout(renderTimer);
    renderTimer = window.setTimeout(() => requestRender(text), 150);
  },
  onPasteImage(file) {
    if (!currentPath) {
      bridge.log('paste image: save the document first');
      return false;
    }
    const reader = new FileReader();
    reader.onload = () => {
      const base64 = String(reader.result).replace(/^data:[^,]*,/, '');
      bridge.saveImage(base64, file.type, (relative) => {
        if (!relative) {
          bridge.log('paste image: could not save the image');
          return;
        }
        editor.insertText(`![](${encodeURI(relative)})`);
      });
    };
    reader.readAsDataURL(file);
    return true;
  },
  onScroll() {
    const top = editor.view.scrollDOM.scrollTop;
    if (isEcho(top, expectedEditorTop)) return;
    expectedEditorTop = undefined;
    scrollDriver = 'editor';
    if (mode === 'split' && settings.syncScroll !== false) syncPreviewToEditor();
  },
});

function setDirty(next: boolean) {
  if (next === dirty) return;
  dirty = next;
  bridge?.setDirty(next);
}

function applyTheme(next: Theme) {
  theme = next;
  document.documentElement.dataset.theme = next;
  editor.setTheme(next);
}

// One render in flight at a time; while it runs, only the newest text is kept.
// Otherwise fast typing on a big document queues stale renders in the worker.
let inFlight = false;
let queuedText: string | undefined;

function requestRender(text = editor.getText()) {
  if (inFlight) {
    queuedText = text;
    return;
  }
  inFlight = true;
  renderStarted = performance.now();
  const request: RenderRequest = {
    id: ++requestId,
    markdown: text,
    baseUrl,
    remoteImages: settings.remoteImages !== false,
  };
  worker.postMessage(request);
}

function renderSettled() {
  inFlight = false;
  if (queuedText !== undefined) {
    const text = queuedText;
    queuedText = undefined;
    requestRender(text);
  }
}

function showRenderError(message: string) {
  bridge.log(`render failed: ${message}`);
  preview.replaceChildren();
  const pre = document.createElement('pre');
  pre.className = 'om-render-error';
  pre.textContent = message;
  preview.append(pre);
  bridge.renderFinished(-1);
}

worker.onerror = (event) => {
  showRenderError(`render worker crashed: ${event.message}`);
  renderSettled();
};

worker.onmessage = async (event: MessageEvent<RenderResult>) => {
  const result = event.data;
  if (result.error) {
    showRenderError(result.error);
    renderSettled();
    return;
  }
  try {
    await applyResult(result);
  } finally {
    renderSettled();
  }
};

async function applyResult(result: RenderResult) {
  const received = performance.now();
  const scrollTop = previewPane.scrollTop;
  const created = patchHtml(preview, result.html);
  previewPane.scrollTop = scrollTop;
  const applied = performance.now();
  await hydrate(preview, {
    theme,
    allowNetwork: settings.remoteImages !== false,
    plantumlServer: settings.plantumlServer ?? '',
  });
  timings = {
    worker: result.elapsedMs,
    transfer: Math.round(received - renderStarted - result.elapsedMs),
    apply: Math.round(applied - received),
    hydrate: Math.round(performance.now() - applied),
    created,
    blocks: preview.childNodes.length,
  };
  if (mode === 'split' && scrollDriver === 'editor' && settings.syncScroll !== false) syncPreviewToEditor();
  else previewPane.scrollTop = scrollTop;
  headings = result.headings;
  bridge.reportHeadings(result.headings);
  bridge.reportStats(result.words, result.characters);
  updateActiveHeading();
  bridge.renderFinished(Math.round(performance.now() - renderStarted));
}

// ---- scroll sync -----------------------------------------------------------

function syncPreviewToEditor() {
  previewPane.scrollTop = previewOffsetForLine(preview, previewPane, editor.topLine());
  expectedPreviewTop = previewPane.scrollTop;
}

function syncEditorToPreview() {
  editor.scrollToLine(lineForPreviewOffset(preview, previewPane, previewPane.scrollTop));
  expectedEditorTop = editor.view.scrollDOM.scrollTop;
}

let activeHeading = '';
function updateActiveHeading() {
  let current = '';
  const limit = previewPane.getBoundingClientRect().top + 80;
  for (const h of headings) {
    const el = document.getElementById(h.id);
    if (!el) continue;
    if (el.getBoundingClientRect().top <= limit) current = h.id;
    else break;
  }
  if (!current && headings.length) current = headings[0].id;
  if (current !== activeHeading) {
    activeHeading = current;
    bridge.reportActiveHeading(current);
  }
}

let spyFrame = 0;
previewPane.addEventListener('scroll', () => {
  if (!spyFrame) spyFrame = requestAnimationFrame(() => {
    spyFrame = 0;
    updateActiveHeading();
  });
  if (isEcho(previewPane.scrollTop, expectedPreviewTop)) return;
  expectedPreviewTop = undefined;
  scrollDriver = 'preview';
  if (mode === 'split' && settings.syncScroll !== false) syncEditorToPreview();
});

function scrollToId(id: string, smooth = true) {
  const target = document.getElementById(id) ?? document.getElementById(`user-content-${id}`);
  if (!target) return;
  if (mode === 'source') {
    const line = Number(target.dataset.sourceLine ?? 0);
    if (line) editor.scrollToLine(line);
    return;
  }
  scrollDriver = 'preview';
  target.scrollIntoView({ behavior: smooth ? 'smooth' : 'auto', block: 'start' });
}

function scrollToSourceLine(line: number) {
  if (mode !== 'preview') editor.scrollToLine(line);
  if (mode !== 'source') {
    scrollDriver = 'preview';
    previewPane.scrollTop = previewOffsetForLine(preview, previewPane, line);
  }
}

// ---- modes and layout --------------------------------------------------------

function setMode(next: Mode) {
  if (next === mode) return;
  // Carry the reading position across the switch.
  const line = mode === 'source' ? editor.topLine() : lineForPreviewOffset(preview, previewPane, previewPane.scrollTop);
  mode = next;
  app.dataset.mode = next;
  requestAnimationFrame(() => {
    editor.view.requestMeasure();
    if (next !== 'preview') editor.scrollToLine(line);
    if (next !== 'source') previewPane.scrollTop = previewOffsetForLine(preview, previewPane, line);
    if (next !== 'preview') editor.focus();
  });
}

divider.addEventListener('pointerdown', (event) => {
  divider.setPointerCapture(event.pointerId);
  divider.classList.add('dragging');
  const move = (e: PointerEvent) => {
    const ratio = Math.min(0.85, Math.max(0.15, e.clientX / app.clientWidth));
    app.style.setProperty('--om-split', `${(ratio * 100).toFixed(2)}%`);
  };
  const up = () => {
    divider.classList.remove('dragging');
    divider.removeEventListener('pointermove', move);
    divider.removeEventListener('pointerup', up);
    editor.view.requestMeasure();
  };
  divider.addEventListener('pointermove', move);
  divider.addEventListener('pointerup', up);
});
divider.addEventListener('dblclick', () => app.style.removeProperty('--om-split'));

function applySettings(next: Settings) {
  const previous = settings;
  settings = { ...settings, ...next };
  document.documentElement.dataset.style = settings.previewStyle ?? 'github';
  let custom = document.getElementById('om-custom-css') as HTMLStyleElement | null;
  if (settings.customCss) {
    if (!custom) {
      custom = document.createElement('style');
      custom.id = 'om-custom-css';
      document.head.append(custom); // last, so it wins over built-in styles
    }
    custom.textContent = settings.customCss;
  } else {
    custom?.remove();
  }
  const needsRender = (previous.remoteImages !== settings.remoteImages && previous.remoteImages !== undefined)
    || (previous.plantumlServer !== settings.plantumlServer && previous.plantumlServer !== undefined);
  if (settings.vim !== previous.vim) void editor.setVim(!!settings.vim);
  editor.setLineWrapping(settings.wrap !== false);
  document.body.classList.toggle('om-line-numbers-off', settings.lineNumbers === false);
  const root = document.documentElement.style;
  if (settings.editorFontSize) root.setProperty('--om-editor-font-size', `${settings.editorFontSize}px`);
  if (settings.previewFontSize) root.setProperty('--om-preview-font-size', `${settings.previewFontSize}px`);
  if (settings.previewWidth !== undefined) {
    root.setProperty('--om-preview-width', settings.previewWidth > 0 ? `${settings.previewWidth}px` : 'none');
  }
  if (needsRender || (previous.remoteImages === undefined && settings.remoteImages === false)) {
    resetPatch(preview); // fences depend on these settings
    requestRender();
  }
}

// ---- links -----------------------------------------------------------------

function onLinkClick(event: MouseEvent) {
  const anchor = (event.target as Element).closest('a');
  if (!anchor || !preview.contains(anchor)) return;
  const href = anchor.getAttribute('href') ?? '';
  event.preventDefault();
  if (href.startsWith('#')) {
    scrollToId(decodeURIComponent(href.slice(1)));
    return;
  }
  const newTab = event.ctrlKey || event.metaKey || event.button === 1;
  bridge.openLink(anchor.href, newTab);
}

preview.addEventListener('click', onLinkClick);
preview.addEventListener('auxclick', (event) => {
  if (event.button === 1) onLinkClick(event);
});

// Task list checkboxes are read-only in the preview.
preview.addEventListener('change', (event) => {
  const input = event.target as HTMLInputElement;
  if (input.type === 'checkbox') input.checked = !input.checked;
});

// ---- API for the C++ side (QWebEnginePage::runJavaScript) -------------------

const api = {
  /** [id, text] to write on save; markSaved(id) confirms that write succeeded. */
  snapshot(): [number, string] {
    const id = ++nextSaveId;
    const text = editor.getText();
    pendingSaves.set(id, text);
    return [id, text];
  },
  markSaved(id: number) {
    const text = pendingSaves.get(id);
    if (text === undefined) return;
    // Older snapshots are superseded by this successful write.
    for (const key of [...pendingSaves.keys()]) if (key <= id) pendingSaves.delete(key);
    savedText = text;
    setDirty(editor.getText() !== savedText);
  },
  revert() {
    editor.setText(savedText);
    setDirty(false);
    requestRender(savedText);
  },
  getText: () => editor.getText(),
  isDirty: () => dirty,
  topLine: () => (mode === 'source' ? editor.topLine() : lineForPreviewOffset(preview, previewPane, previewPane.scrollTop)),
  previewHtml: () => preview.innerHTML,
  focusEditor: () => editor.focus(),
  /**
   * Where Ctrl+F should go: opens CodeMirror's search panel and returns
   * 'editor' when the editor has focus (or is the only pane), else 'preview'.
   */
  findTarget(): 'editor' | 'preview' {
    if (mode === 'source' || (mode === 'split' && editor.view.hasFocus)) {
      openSearchPanel(editor.view);
      return 'editor';
    }
    return 'preview';
  },
  find: (query: string, backward: boolean, caseSensitive: boolean, refresh = false) =>
    find(preview, previewPane, query, backward, caseSensitive, refresh),
  clearFind: () => clearFind(),
  selectedText: () => window.getSelection()?.toString() ?? '',
  /** Builds standalone HTML and hands it to bridge.exportFinished. */
  exportHtml(inlineImages: boolean) {
    const title = preview.querySelector('h1')?.textContent?.trim() || document.title;
    buildStandaloneHtml(preview, { inlineImages, title })
      .then((html) => bridge.exportFinished(html, ''))
      .catch((err) => bridge.exportFinished('', String(err)));
  },
  /** Inserts text at the editor's cursor (used for dropped images). */
  insertText: (text: string) => editor.insertText(text),
  /** Test hook: edit the document the way typing would. */
  userEdit: (text: string) => editor.replaceAsUser(text),
  mode: () => mode,
  /** Last render's phase durations in ms (debugging aid). */
  timings: () => timings,
};
(window as unknown as { omnimark: typeof api }).omnimark = api;

// ---- startup -----------------------------------------------------------------

applyTheme(theme);
bridge = await connectBridge();
bridge.documentLoaded.connect((path, text, nextBaseUrl) => {
  baseUrl = nextBaseUrl;
  savedText = text;
  pendingSaves.clear();
  const sameDocument = path === currentPath;
  currentPath = path;
  editor.setText(text);
  setDirty(false);
  window.clearTimeout(renderTimer);
  // Reloads of the same file keep reusable blocks; another file starts clean.
  if (!sameDocument) resetPatch(preview);
  requestRender(text);
});
bridge.themeChanged.connect((next) => {
  const resolved: Theme = next === 'dark' ? 'dark' : 'light';
  if (resolved === theme) return;
  applyTheme(resolved);
  resetPatch(preview); // diagrams are drawn per theme
  requestRender();
});
bridge.viewModeChanged.connect((next) => {
  if (next === 'preview' || next === 'split' || next === 'source') setMode(next);
});
bridge.settingsChanged.connect((json) => {
  try {
    applySettings(JSON.parse(json) as Settings);
  } catch (err) {
    bridge.log(`bad settings: ${String(err)}`);
  }
});
bridge.scrollToAnchor.connect((id) => scrollToId(id, false));
bridge.scrollToLine.connect((line) => scrollToSourceLine(line));
bridge.ready();
