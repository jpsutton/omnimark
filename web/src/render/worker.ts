/// <reference lib="webworker" />
import { renderMarkdown } from './pipeline';
import type { RenderRequest, RenderResult } from './types';

declare const self: DedicatedWorkerGlobalScope;

self.onmessage = async (event: MessageEvent<RenderRequest>) => {
  const { id, markdown, baseUrl, remoteImages } = event.data;
  const started = performance.now();
  let result: RenderResult;
  try {
    const out = await renderMarkdown(markdown, { baseUrl, remoteImages });
    result = { id, ...out, elapsedMs: Math.round(performance.now() - started) };
  } catch (err) {
    result = {
      id,
      html: '',
      headings: [],
      words: 0,
      characters: 0,
      elapsedMs: Math.round(performance.now() - started),
      error: err instanceof Error ? `${err.message}\n${err.stack ?? ''}` : String(err),
    };
  }
  self.postMessage(result);
};
