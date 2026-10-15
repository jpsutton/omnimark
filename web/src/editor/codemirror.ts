import { Compartment, EditorState, type Extension } from '@codemirror/state';
import {
  EditorView,
  crosshairCursor,
  drawSelection,
  dropCursor,
  highlightActiveLine,
  highlightActiveLineGutter,
  highlightSpecialChars,
  keymap,
  lineNumbers,
  rectangularSelection,
} from '@codemirror/view';
import { defaultKeymap, history, historyKeymap, indentWithTab } from '@codemirror/commands';
import {
  HighlightStyle,
  bracketMatching,
  foldGutter,
  foldKeymap,
  indentOnInput,
  syntaxHighlighting,
} from '@codemirror/language';
import { highlightSelectionMatches, search, searchKeymap } from '@codemirror/search';
import { closeBrackets, closeBracketsKeymap } from '@codemirror/autocomplete';
import { markdown, markdownLanguage } from '@codemirror/lang-markdown';
import { languages } from '@codemirror/language-data';
import { tags as t } from '@lezer/highlight';
import type { Theme } from '../render/fences/registry';
import { formattingKeymap } from './formatting';

// Colors come from CSS variables (see styles/editor.css), so a theme switch
// only needs the `dark` flag to change.
const highlight = HighlightStyle.define([
  { tag: t.heading1, class: 'cm-md-h1' },
  { tag: t.heading2, class: 'cm-md-h2' },
  { tag: [t.heading3, t.heading4, t.heading5, t.heading6], class: 'cm-md-h' },
  { tag: t.strong, fontWeight: '600' },
  { tag: t.emphasis, fontStyle: 'italic' },
  { tag: t.strikethrough, textDecoration: 'line-through' },
  { tag: [t.link, t.url], class: 'cm-md-link' },
  { tag: t.monospace, class: 'cm-md-code' },
  { tag: t.quote, class: 'cm-md-quote' },
  { tag: [t.processingInstruction, t.contentSeparator, t.meta], class: 'cm-md-mark' },
  { tag: [t.keyword, t.operatorKeyword, t.modifier], class: 'cm-tok-keyword' },
  { tag: [t.string, t.special(t.string), t.regexp], class: 'cm-tok-string' },
  { tag: [t.number, t.bool, t.null, t.atom], class: 'cm-tok-number' },
  { tag: [t.comment, t.lineComment, t.blockComment], class: 'cm-tok-comment' },
  { tag: [t.function(t.variableName), t.function(t.propertyName)], class: 'cm-tok-function' },
  { tag: [t.typeName, t.className, t.namespace], class: 'cm-tok-type' },
  { tag: [t.propertyName, t.attributeName], class: 'cm-tok-property' },
  { tag: [t.tagName, t.angleBracket], class: 'cm-tok-tag' },
  { tag: t.invalid, class: 'cm-tok-invalid' },
]);

export interface EditorOptions {
  onChange(text: string): void;
  onScroll(): void;
  /** Called for pasted image files; return true if handled. */
  onPasteImage?(file: File): boolean;
}

export interface Editor {
  view: EditorView;
  getText(): string;
  setText(text: string): void;
  /** Replaces the document as if the user typed it (fires onChange). */
  replaceAsUser(text: string): void;
  /** Replaces the selection with `text`, like typing it. */
  insertText(text: string): void;
  setTheme(theme: Theme): void;
  setVim(enabled: boolean): Promise<void>;
  setLineWrapping(enabled: boolean): void;
  /** 1-based line at the top of the viewport, with fractional progress into it. */
  topLine(): number;
  scrollToLine(line: number): void;
  focus(): void;
}

export function createEditor(parent: HTMLElement, options: EditorOptions): Editor {
  const themeCompartment = new Compartment();
  const vimCompartment = new Compartment();
  const wrapCompartment = new Compartment();
  let applyingExternal = false;

  const extensions: Extension[] = [
    vimCompartment.of([]),
    lineNumbers(),
    highlightActiveLineGutter(),
    highlightSpecialChars(),
    history(),
    foldGutter(),
    drawSelection(),
    dropCursor(),
    EditorState.allowMultipleSelections.of(true),
    indentOnInput(),
    syntaxHighlighting(highlight),
    bracketMatching(),
    closeBrackets(),
    rectangularSelection(),
    crosshairCursor(),
    highlightActiveLine(),
    highlightSelectionMatches(),
    search({ top: true }),
    keymap.of([
      ...formattingKeymap,
      ...closeBracketsKeymap,
      ...defaultKeymap,
      ...searchKeymap,
      ...historyKeymap,
      ...foldKeymap,
      indentWithTab,
    ]),
    markdown({ base: markdownLanguage, codeLanguages: languages, addKeymap: true }),
    wrapCompartment.of(EditorView.lineWrapping),
    themeCompartment.of(EditorView.theme({}, { dark: false })),
    EditorView.updateListener.of((update) => {
      if (update.docChanged && !applyingExternal) options.onChange(update.state.doc.toString());
    }),
    EditorView.domEventHandlers({
      scroll: () => {
        options.onScroll();
      },
      paste: (event) => {
        const file = [...(event.clipboardData?.files ?? [])].find((f) => f.type.startsWith('image/'));
        if (!file || !options.onPasteImage) return false;
        if (!options.onPasteImage(file)) return false;
        event.preventDefault();
        return true;
      },
    }),
  ];

  const view = new EditorView({
    parent,
    state: EditorState.create({ doc: '', extensions }),
  });

  return {
    view,
    getText: () => view.state.doc.toString(),
    setText(text) {
      const current = view.state.doc.toString();
      if (current === text) return;
      // Replace only the changed middle so the cursor and scroll stay put
      // when the file is reloaded after an external edit.
      let start = 0;
      while (start < current.length && start < text.length && current[start] === text[start]) start++;
      let endA = current.length;
      let endB = text.length;
      while (endA > start && endB > start && current[endA - 1] === text[endB - 1]) {
        endA--;
        endB--;
      }
      applyingExternal = true;
      try {
        view.dispatch({ changes: { from: start, to: endA, insert: text.slice(start, endB) } });
      } finally {
        applyingExternal = false;
      }
    },
    insertText(text) {
      view.dispatch(view.state.replaceSelection(text), { scrollIntoView: true, userEvent: 'input.paste' });
      view.focus();
    },
    replaceAsUser(text) {
      view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: text }, userEvent: 'input' });
    },
    setTheme(theme) {
      view.dispatch({ effects: themeCompartment.reconfigure(EditorView.theme({}, { dark: theme === 'dark' })) });
    },
    async setVim(enabled) {
      const ext = enabled ? (await import('@replit/codemirror-vim')).vim() : [];
      view.dispatch({ effects: vimCompartment.reconfigure(ext) });
    },
    setLineWrapping(enabled) {
      view.dispatch({ effects: wrapCompartment.reconfigure(enabled ? EditorView.lineWrapping : []) });
    },
    topLine() {
      const top = view.scrollDOM.scrollTop;
      const block = view.lineBlockAtHeight(top);
      const line = view.state.doc.lineAt(block.from).number;
      const progress = block.height > 0 ? (top - block.top) / block.height : 0;
      return line + Math.min(Math.max(progress, 0), 1);
    },
    scrollToLine(line) {
      const doc = view.state.doc;
      const whole = Math.min(Math.max(Math.floor(line), 1), doc.lines);
      const block = view.lineBlockAt(doc.line(whole).from);
      view.scrollDOM.scrollTop = block.top + (line - whole) * block.height;
    },
    focus: () => view.focus(),
  };
}
