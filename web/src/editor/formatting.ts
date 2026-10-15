import { EditorSelection, type StateCommand } from '@codemirror/state';
import type { KeyBinding } from '@codemirror/view';

/**
 * Toggles `marker` around each selection: wraps it, or unwraps it if the
 * selection (or the text right around it) is already wrapped. An empty
 * selection inserts a pair of markers with the cursor between them.
 */
function toggleWrap(marker: string): StateCommand {
  return ({ state, dispatch }) => {
    const len = marker.length;
    const changes = state.changeByRange((range) => {
      const text = state.sliceDoc(range.from, range.to);
      const before = state.sliceDoc(range.from - len, range.from);
      const after = state.sliceDoc(range.to, range.to + len);
      if (before === marker && after === marker) {
        return {
          changes: [
            { from: range.from - len, to: range.from },
            { from: range.to, to: range.to + len },
          ],
          range: EditorSelection.range(range.from - len, range.to - len),
        };
      }
      if (text.length >= 2 * len && text.startsWith(marker) && text.endsWith(marker)) {
        return {
          changes: { from: range.from, to: range.to, insert: text.slice(len, -len) },
          range: EditorSelection.range(range.from, range.to - 2 * len),
        };
      }
      return {
        changes: { from: range.from, to: range.to, insert: marker + text + marker },
        range: EditorSelection.range(range.from + len, range.to + len),
      };
    });
    dispatch(state.update(changes, { scrollIntoView: true, userEvent: 'input' }));
    return true;
  };
}

/** Turns the selection into [text](url), selecting "url" for typing. */
const insertLink: StateCommand = ({ state, dispatch }) => {
  const changes = state.changeByRange((range) => {
    const text = state.sliceDoc(range.from, range.to);
    const isUrl = /^[a-z][a-z0-9+.-]*:\/\/\S+$/i.test(text);
    const label = isUrl ? '' : text;
    const url = isUrl ? text : 'url';
    const insert = `[${label}](${url})`;
    // Select the part the user most likely wants to type next.
    const selectFrom = isUrl ? range.from + 1 : range.from + label.length + 3;
    const selectTo = isUrl ? range.from + 1 : selectFrom + url.length;
    return { changes: { from: range.from, to: range.to, insert }, range: EditorSelection.range(selectFrom, selectTo) };
  });
  dispatch(state.update(changes, { scrollIntoView: true, userEvent: 'input' }));
  return true;
};

export const formattingKeymap: KeyBinding[] = [
  { key: 'Mod-b', run: toggleWrap('**') },
  { key: 'Mod-i', run: toggleWrap('*') },
  { key: 'Mod-Shift-x', run: toggleWrap('~~') },
  { key: 'Mod-Shift-c', run: toggleWrap('`') },
  { key: 'Mod-k', run: insertLink },
];
