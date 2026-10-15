export interface Heading {
  level: number;
  id: string;
  text: string;
  line: number;
}

export interface RenderRequest {
  id: number;
  markdown: string;
  /** Directory URL of the document, e.g. omnimark://local/home/me/notes/ */
  baseUrl: string;
  /** When false, http(s) images become links instead of loading. */
  remoteImages?: boolean;
}

export interface RenderResult {
  id: number;
  html: string;
  headings: Heading[];
  words: number;
  characters: number;
  elapsedMs: number;
  error?: string;
}
