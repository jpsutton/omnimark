import type { Heading } from './render/types';

type Handler<T extends unknown[]> = (...args: T) => void;

interface Signal<T extends unknown[]> {
  connect(handler: Handler<T>): void;
}

/** Shape of the C++ Bridge object (src/Bridge.h) as seen through QWebChannel. */
export interface NativeBridge {
  ready(): void;
  renderFinished(elapsedMs: number): void;
  openLink(href: string, newTab: boolean): void;
  reportHeadings(headings: Heading[]): void;
  reportActiveHeading(id: string): void;
  setDirty(dirty: boolean): void;
  exportFinished(html: string, error: string): void;
  reportStats(words: number, characters: number): void;
  /** QWebChannel delivers return values to a trailing callback. */
  saveImage(base64: string, mimeType: string, callback: (relativePath: string) => void): void;
  log(message: string): void;
  documentLoaded: Signal<[path: string, text: string, baseUrl: string]>;
  themeChanged: Signal<[theme: string]>;
  viewModeChanged: Signal<[mode: string]>;
  settingsChanged: Signal<[json: string]>;
  scrollToAnchor: Signal<[id: string]>;
  scrollToLine: Signal<[line: number]>;
}

declare global {
  interface Window {
    qt?: { webChannelTransport: unknown };
    QWebChannel?: new (
      transport: unknown,
      init: (channel: { objects: { bridge: NativeBridge } }) => void,
    ) => unknown;
  }
}

function loadScript(src: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = src;
    script.onload = () => resolve();
    script.onerror = () => reject(new Error(`failed to load ${src}`));
    document.head.append(script);
  });
}

function createSignal<T extends unknown[]>() {
  const handlers: Handler<T>[] = [];
  return {
    connect: (h: Handler<T>) => void handlers.push(h),
    emit: (...args: T) => handlers.forEach((h) => h(...args)),
  };
}

/** Browser-only stand-in so the frontend runs under `vite dev` and in tests. */
function createMockBridge() {
  const documentLoaded = createSignal<[string, string, string]>();
  const themeChanged = createSignal<[string]>();
  const viewModeChanged = createSignal<[string]>();
  const settingsChanged = createSignal<[string]>();
  const scrollToAnchor = createSignal<[string]>();
  const scrollToLine = createSignal<[number]>();
  const params = new URLSearchParams(location.search);
  const file = params.get('file') ?? 'showcase.md';
  const base = new URL(`/samples/${file}`, location.href);
  const mock = {
    dirty: false,
    headings: [] as Heading[],
    activeHeading: '',
    renders: 0,
    ready() {
      if (params.get('theme')) themeChanged.emit(params.get('theme')!);
      if (params.get('mode')) viewModeChanged.emit(params.get('mode')!);
      fetch(base)
        .then((r) => (r.ok ? r.text() : `# Mock bridge\n\nCould not load \`${file}\`.`))
        .then((text) => documentLoaded.emit(file, text, new URL('.', base).toString()));
    },
    renderFinished(ms: number) {
      mock.renders++;
      console.debug(`rendered in ${ms} ms`);
    },
    openLink: (href: string, newTab: boolean) => console.info('openLink', href, newTab),
    reportHeadings: (h: Heading[]) => void (mock.headings = h),
    reportActiveHeading: (id: string) => void (mock.activeHeading = id),
    setDirty: (d: boolean) => void (mock.dirty = d),
    exported: '',
    words: 0,
    reportStats: (words: number) => void (mock.words = words),
    savedImages: [] as string[],
    saveImage(base64: string, mimeType: string, callback: (path: string) => void) {
      const path = `images/pasted-${mock.savedImages.length + 1}.${mimeType.split('/')[1] ?? 'png'}`;
      mock.savedImages.push(base64.slice(0, 16));
      setTimeout(() => callback(path));
    },
    exportFinished: (html: string, error: string) => void (mock.exported = error || html),
    log: (m: string) => console.info(m),
    documentLoaded,
    themeChanged,
    viewModeChanged,
    settingsChanged,
    scrollToAnchor,
    scrollToLine,
  };
  (window as unknown as { omnimarkMock: typeof mock }).omnimarkMock = mock;
  return mock satisfies NativeBridge;
}

export async function connectBridge(): Promise<NativeBridge> {
  if (!window.qt?.webChannelTransport) return createMockBridge();
  if (!window.QWebChannel) await loadScript('omnimark://app/qwebchannel.js');
  return new Promise((resolve) => {
    new window.QWebChannel!(window.qt!.webChannelTransport, (channel) => {
      resolve(channel.objects.bridge);
    });
  });
}
