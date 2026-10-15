# Omnimark

A cross-platform desktop Markdown viewer and editor for Linux, Windows and macOS, built with Qt 6.

The window, menus, tabs, docks and dialogs are native Qt widgets. Each document runs in a QtWebEngine (Chromium) view: a TypeScript pipeline renders the preview, and a CodeMirror editor sits beside it.

## Features

**Markdown**
- CommonMark and GFM: tables, task lists, strikethrough, autolinks, footnotes
- YAML/TOML front matter, shown as a collapsible table
- GitHub alerts (`> [!NOTE]`, `[!TIP]`, `[!IMPORTANT]`, `[!WARNING]`, `[!CAUTION]`)
- KaTeX math: `$inline$`, `$$display$$` and ```` ```math ```` blocks
- Extended inline syntax: `==mark==`, `H~2~O` subscript, `x^2^` superscript, `:emoji:` shortcodes, definition lists (`Term` followed by `: definition`)
- Wiki links: `[[Page]]`, `[[Page|label]]`, `[[Page#Heading]]`. A link resolves to any file with that name in the open folder.
- Raw HTML, sanitized: `<details>`, `<kbd>`, `<sub>`/`<sup>`, `<mark>`, images and video are kept; scripts and event handlers are stripped

**Code blocks**
- Shiki syntax highlighting for 200+ languages, with light and dark themes
- Line numbers: on by default; turn off per block with `noLineNumbers`, or start at another number with `startLine=10`
- Titles (`title="file.ts"`), line highlights (`{1,3-5}`), and `// [!code ++]`, `[!code --]`, `[!code focus]`, `[!code error]` notation
- Copy button

**Embedded formats in fenced blocks**

| Fence | Renders as |
|---|---|
| `mermaid` | Mermaid diagrams |
| `dot` / `graphviz` | Graphviz (WASM) |
| `vega` / `vega-lite` | Vega charts (CSP-safe interpreter) |
| `csv` / `tsv` | Sortable tables |
| `abc` | Sheet music (abcjs) |
| `wavedrom` | Timing diagrams |
| `nomnoml` | UML-style diagrams |
| `geojson` / `topojson` | Interactive maps (Leaflet; OpenStreetMap tiles only when remote content is allowed) |
| `stl` | 3D model viewer (three.js, orbit controls) |
| `smiles` | Chemical structures (one SMILES string per line) |
| `plantuml` / `puml` | PlantUML. Opt-in: diagrams are sent to the server you configure in Settings. |
| `math` | KaTeX display math |

Every diagram follows the light/dark theme. Each one has a toolbar: view source, copy SVG, and fullscreen with zoom and pan. A broken diagram shows the error inline next to its source.

**App**
- **Views:** Preview, Split and Source per tab (Ctrl+1/2/3).
  - The toolbar's **Editor** button (Ctrl+E) shows or hides the editor; its drop-down picks any mode.
  - Split view syncs scrolling and has a resizable divider.
  - Toolbar labels can be icons only, text beside or text under (View → Toolbar Labels, or Settings).
- **Editing:** CodeMirror 6 with Markdown and fenced-code highlighting, search/replace, and optional Vim bindings.
  - Formatting shortcuts: Ctrl+B, Ctrl+I, Ctrl+K link, Ctrl+Shift+C code, Ctrl+Shift+X strike.
  - Pasting or dropping an image saves it to `images/` next to the document and inserts a link.
  - Save, Save As, New and Revert, with unsaved-change guards on close.
- **Live reload:** the view updates when a file changes on disk, including editors that save atomically. If you have unsaved edits, Omnimark asks whether to keep your version or reload from disk.
- **Folders:** Open Folder adds a file sidebar with a filter.
  - Tabs with back/forward history per tab. Links to other `.md` files open in the same tab; Ctrl-click or middle-click opens a new tab.
  - The session (tabs, view modes, folder, window layout) and recent files are restored at startup.
- **Navigation:** Outline dock with scroll-spy, Find in document (Ctrl+F), Find in Folder (Ctrl+Shift+F)
- **Appearance:**
  - Light, dark or follow-system themes, with GitHub, Serif or Compact preview styles
  - Font size and width settings, zoom (Ctrl+±)
  - A user `custom.css` that reloads live
- **Export:** PDF (always light theme), standalone HTML (CSS, fonts, diagrams and local images inlined), and printing
- **Single instance:** opening a file from the file manager adds a tab to the running window

## Building

Requirements: Qt ≥ 6.8 (Widgets, WebEngine, WebChannel, PrintSupport, Concurrent), CMake ≥ 3.24, Ninja, a C++20 compiler, Node ≥ 20 and pnpm.

Arch Linux:

```bash
sudo pacman -S --needed qt6-base qt6-webengine qt6-webchannel cmake ninja nodejs pnpm
```

Build and run:

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/omnimark samples/showcase.md
```

The CMake build runs `pnpm install` and `pnpm build` in `web/`. To build the web part yourself, configure with `-DOMNIMARK_BUILD_WEB=OFF`.

### Command line

```
omnimark [files or folders...]
  --view preview|split|source   initial view for the files opened
  --new-instance                don't hand files to an already running window
  --render-pdf out.pdf          render the first file to PDF and exit
  --screenshot out.png          save a window screenshot after rendering and exit
```

## Packaging

| Target | How |
|---|---|
| Arch Linux | `cd packaging/linux/arch && makepkg -si` |
| Flatpak | Build `web/` first, then `flatpak-builder --user --install build-dir packaging/linux/flatpak/io.github.omnimark.Omnimark.yml` |
| AppImage | `packaging/linux/appimage/build-appimage.sh` after a release build. Needs `qt6-serialport` and `qt6-wayland`. The AppImage only runs on distros at least as new as the build host. |
| Windows (best effort, unsigned) | `cpack --config build/release/CPackConfig.cmake -G NSIS`. Bundles Qt via windeployqt and registers "Open with" for `.md`. |
| macOS (best effort, unsigned) | `cpack --config build/release/CPackConfig.cmake -G DragNDrop`. Bundles Qt via macdeployqt; the `.app` declares Markdown document types. |

### Releases and CI

Changes flow from feature branches into `staging`, then from `staging` into `release`.

- `checks.yml` runs on every pull request: static checks, web unit and browser tests, and the Linux C++/GUI tests.
- `build.yml` runs on pull requests into `release`. It builds the AppImage, Arch package and Flatpak, which a release requires, and the Windows and macOS packages, which are best effort and unsigned.
- `release.yml` runs when `staging` is merged into `release` with a merge commit. It publishes that pull request's build as a GitHub Release tagged `YYYY.MM.DD`, signs every file with the release key, and includes `SHA256SUMS` and a signed pacman repo.

Verify a download:

```bash
curl -sL https://raw.githubusercontent.com/jpsutton/omnimark/staging/packaging/omnimark-signing-key.asc | gpg --import
gpg --verify SHA256SUMS.sig SHA256SUMS && sha256sum --check --ignore-missing SHA256SUMS
```

Release key: `5F2F BC3C 175D 49D9 F491  2198 97A9 E841 85E0 2236` (Ed25519, also in [packaging/omnimark-signing-key.asc](packaging/omnimark-signing-key.asc)).

Arch Linux users can install from the signed pacman repo:

```ini
# /etc/pacman.conf
[omnimark]
Server = https://github.com/jpsutton/omnimark/releases/latest/download
```

Then trust the key and install:

```bash
curl -sLO https://github.com/jpsutton/omnimark/releases/latest/download/omnimark-signing-key.asc
sudo pacman-key --add omnimark-signing-key.asc
sudo pacman-key --lsign-key 5F2FBC3C175D49D9F491219897A9E84185E02236
sudo pacman -Sy omnimark
```

## Development

Frontend with hot reload:

```bash
cd web && pnpm dev
```

- **In a browser:** open `http://localhost:5173/?file=showcase.md&mode=split&theme=dark`. A mock bridge loads files from `samples/`.
- **In the app:** run `OMNIMARK_DEV_URL=http://localhost:5173 ./build/debug/omnimark file.md`.
- **DevTools:** set `QTWEBENGINE_REMOTE_DEBUGGING=9222` to inspect the embedded pages.

Tests:

```bash
cd web && pnpm test        # render pipeline unit tests (Vitest)
cd web && pnpm test:e2e    # frontend UI tests in Chromium (Playwright, mock bridge)
ctest --preset debug       # C++ unit tests + GUI tests driving real QtWebEngine pages
```

To run the Playwright tests without installing browsers locally:

```bash
docker run --rm --ipc=host -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$PWD":/src -w /src/web mcr.microsoft.com/playwright:v1.64.1-noble npx playwright test
```

## Layout

```
src/            C++ Qt app: main window, tabs, docks, omnimark:// scheme handler,
                file watcher, QWebChannel bridge, settings, export
web/src/        TypeScript frontend
  render/       unified pipeline (runs in a Web Worker), fence renderers, hydrate, find, scroll sync
  editor/       CodeMirror setup
  export/       standalone HTML builder
samples/        Feature showcase documents (also test fixtures)
tests/cpp/      Qt Test unit and GUI tests
packaging/      Icons, desktop/AppStream/MIME files, PKGBUILD, Flatpak, AppImage, macOS/Windows resources
```

## License

MIT, see [LICENSE](LICENSE).

Packaged builds bundle Qt (LGPLv3), Chromium (via QtWebEngine) and many JavaScript libraries. Their licenses are collected in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); the app shows it under Help → Third-Party Notices.

After changing web dependencies, regenerate the file with `pnpm --dir web notices`. CI fails if it is out of date.

## Security model

Documents are treated as untrusted:

- **Sanitizing:** user HTML is sanitized right after parsing. Diagram SVG is sanitized again with DOMPurify. The page runs under a strict CSP with no inline scripts and no `eval`.
- **Local files:** the page reaches them only through `omnimark://local/`. That scheme serves files only from inside the open document's folder or the opened workspace. Symlinks that point outside are refused.
- **Navigation:** the page cannot navigate away. External links open in the system browser.
- **Network:** remote images can be turned off in Settings. Renderer data must be inlined: Vega `data.url` pointing to remote hosts is refused.
