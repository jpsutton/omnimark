# Omnimark: cross-platform Markdown viewer/editor (desktop, Qt 6)

> **Status (2026-10-15):** M0–M7 are implemented, along with the v1.1 renderers (maps, STL, SMILES, opt-in PlantUML).
>
> Changes from the plan:
> - The web frontend is plain TypeScript, not Svelte.
> - Web assets are served from disk (`share/omnimark/web`, `<app>/web`, or `Contents/Resources/web`) instead of being compiled into the binary.
> - Preview find uses the CSS Custom Highlight API, so matches stay inside the preview pane.
>
> Verified locally:
> - Vitest unit tests, Playwright UI tests and Qt Test unit/GUI tests pass.
> - The Arch package builds.
> - The Flatpak builds and renders a document in its sandbox.
> - The AppImage builds (in an Arch container) and runs natively on Wayland.
> - A full code review was done; every finding is fixed and covered by a regression test.
>
> Windows/macOS packaging and CI have not been run yet.

## Context

`/home/jpsutton/Projects/Personal/omnimark` is empty (no git repo yet). Goal: a desktop-only (Linux, Windows, macOS) Markdown app that renders as many Markdown features as possible, including embedded formats in code fences (Mermaid etc.), syntax highlighting with line numbers, plus a split editor.

Decisions made with the user:
- **C++ / Qt 6** shell (no Electron, no Rust, no GTK webview).
- **Hybrid UI.** Window, menus, toolbar, folder tree, tabs, TOC dock and find bar are native Qt widgets. Each tab holds one `QWebEngineView` containing the CodeMirror editor and the HTML preview, so scroll sync stays inside one page.
- Scope: viewer + split editor, live reload, folder sidebar + tabs, export HTML/PDF, search + TOC, themes.

Why a webview at all: Qt's `QTextDocument` Markdown support cannot render Mermaid, KaTeX, Graphviz etc. QtWebEngine (Chromium) gives identical rendering on every OS and native `printToPdf()`.

Toolchain: Qt ≥ 6.8 LTS (modules: Core, Gui, Widgets, WebEngineWidgets, WebChannel), CMake ≥ 3.24, Ninja, C++20, Node + pnpm for the web part. Arch packages: `qt6-base qt6-webengine qt6-webchannel cmake ninja`. node, npm and pnpm are already installed.

## Architecture

```
QMainWindow (C++)
 ├─ Menu/toolbar, StatusBar
 ├─ WorkspaceDock: QTreeView + QFileSystemModel (+ proxy filter *.md/*.markdown/*.mdx)
 ├─ TocDock: QTreeView fed with headings from the page via the bridge
 ├─ FindBar: QWebEnginePage::findText (preview) / JS call into CodeMirror search (editor)
 └─ QTabWidget ─ DocumentTab (one per open file, view created lazily)
                   ├─ QWebEngineView ── loads omnimark://app/index.html
                   │     web app: CodeMirror 6 editor | preview  (Preview / Split / Source modes)
                   │     render Web Worker: unified + Shiki → sanitized HTML
                   │     hydrate.ts: lazy fence renderers (Mermaid, Graphviz, Vega…)
                   └─ Bridge (QObject over QWebChannel)
                         slots: readFile, writeFile, resolveLink, openExternal, setDirty, reportHeadings, reportScroll
                         signals: fileChanged, themeChanged, settingsChanged, viewModeChanged, scrollToHeading
C++ services: FileWatcher (QFileSystemWatcher), SchemeHandler (omnimark://), Settings (QSettings),
              SingleInstance (QLocalServer), Exporter (printToPdf / HTML), ThemeManager (QStyleHints::colorScheme)
```

Key choices:
- **Web part is vanilla TypeScript + Vite (Svelte dropped).** The native widgets now own the sidebar, tabs and docks, so the page only holds editor + preview. No framework needed.
- **Markdown engine: unified (remark/rehype).** The mdast keeps source positions, which drive editor↔preview scroll sync. Largest plugin ecosystem.
- **Parse + highlight runs in a Web Worker.** It returns sanitized HTML, so typing never blocks the UI. DOM-dependent renderers run afterwards on the main thread.
- **Fence renderer registry.** Each embedded format is a lazily imported module:
  `interface FenceRenderer { langs: string[]; render(code: string, el: HTMLElement, ctx: { theme: 'light'|'dark'; baseUrl: string }): Promise<void> }`.
  Output is cached by `hash(lang + code + theme)`. A failure shows an inline error box plus the raw source.
- **Custom scheme `omnimark://`**, registered with `QWebEngineUrlScheme` before `QApplication` and flagged `SecureScheme | LocalAccessAllowed | CorsEnabled` so module workers load. It serves two hosts:
  - `app/…` serves the Vite build, embedded via `qt_add_resources`.
  - `local/<path>` serves images and other assets, only from inside allowed roots (the open document's directory and opened workspace folders).
- **Navigation:** `QWebEnginePage::acceptNavigationRequest` is overridden. External links go to `QDesktopServices::openUrl`. Links to `.md` files open or focus a tab. All other navigation is blocked.
- **Security:** strict CSP in `index.html`, `rehype-sanitize` with an extended schema (`details`, `summary`, `kbd`, `sup`, `sub`, renderer SVG), Mermaid `securityLevel: 'strict'`. In `QWebEngineSettings`: `LocalContentCanAccessFileUrls=false`, `JavascriptCanOpenWindows=false`, and remote images behind a setting.
- **Dev loop:** when `OMNIMARK_DEV_URL=http://localhost:5173` is set, views load the Vite dev server (HMR). `QTWEBENGINE_REMOTE_DEBUGGING=9222` enables Chrome DevTools. The web app also runs in a plain browser with a mock bridge.

## Feature matrix

**Markdown syntax:** CommonMark, GFM (tables, task lists, strikethrough, autolinks, footnotes), YAML/TOML front matter (collapsible table), GitHub alerts (`> [!NOTE]`), KaTeX math (`$…$`, `$$…$$`, ```` ```math ````), emoji shortcodes, heading slugs + anchors, definition lists, `==mark==`, sub/superscript, wiki links `[[page]]`, raw HTML (sanitized). Packages: `remark-gfm`, `remark-frontmatter`, `remark-math`, `rehype-katex`, `remark-emoji`, `rehype-slug`, `rehype-autolink-headings`, `remark-github-blockquote-alert`, `remark-wiki-link`, `remark-definition-list`, and a micromark extension for mark/sub/sup (write a small one if none is maintained).

**Code blocks (Shiki via `@shikijs/rehype`, dual light/dark themes through CSS vars):**
- Line numbers via CSS counters (global setting plus per-block `showLineNumbers`)
- Meta options: `title="file.ts"`, line highlight `{1,3-5}`, diff/focus notation (`@shikijs/transformers`)
- Copy button, wrap toggle, language label
- Languages load lazily; unknown languages fall back to plaintext

**Embedded formats in fences (each lazy-loaded):**
| Fence lang | Library | Tier |
|---|---|---|
| `mermaid` | mermaid | v1 |
| `math`, `latex`, `katex` | KaTeX | v1 |
| `dot`, `graphviz` | @hpcc-js/wasm-graphviz | v1 |
| `vega`, `vega-lite` | vega-embed | v1 |
| `csv`, `tsv` | papaparse, rendered as a sortable table | v1 |
| `abc` | abcjs | v1 |
| `wavedrom` | wavedrom | v1 |
| `nomnoml` | nomnoml | v1 |
| `geojson`, `topojson` | leaflet (tiles need network, opt-in) | v1.1 |
| `stl` | three.js viewer | v1.1 |
| `smiles` | smiles-drawer | v1.1 |
| `plantuml` | remote server, opt-in (off by default) | v1.1 |

Each diagram gets a hover toolbar: view source, copy SVG, open fullscreen with pan-zoom.

## Project layout

```
omnimark/
  CMakeLists.txt            # builds web/ via pnpm (add_custom_command), embeds web/dist with qt_add_resources
  CMakePresets.json         # debug / release presets
  src/
    main.cpp                # scheme registration, SingleInstance, QFileOpenEvent (macOS), arg parsing
    MainWindow.{h,cpp}      # menus, toolbar, docks, tab management, session restore, recent files
    DocumentTab.{h,cpp}     # QWebEngineView + QWebChannel + Bridge, dirty state, view mode
    Bridge.{h,cpp}          # Q_INVOKABLE slots / signals exposed to JS
    WebPage.{h,cpp}         # QWebEnginePage subclass: acceptNavigationRequest
    SchemeHandler.{h,cpp}   # omnimark://app and omnimark://local with root allow-list
    FileWatcher.{h,cpp}     # QFileSystemWatcher wrapper: debounce, re-add after atomic save
    WorkspaceDock.{h,cpp}   TocDock.{h,cpp}   FindBar.{h,cpp}   SettingsDialog.{h,cpp}
    Exporter.{h,cpp}        # printToPdf(QPageLayout), standalone HTML write-out
    ThemeManager.{h,cpp}    # light/dark/system via QStyleHints, pushes theme + custom CSS to pages
    SingleInstance.{h,cpp}  # QLocalServer/QLocalSocket arg forwarding
  web/
    package.json  vite.config.ts  tsconfig.json  index.html
    src/
      main.ts  bridge.ts (qwebchannel.js glue + browser mock)
      editor/   codemirror.ts (lang-markdown, language-data nested highlighting, search, line numbers, optional vim)
      render/   pipeline.ts  worker.ts  hydrate.ts  scroll-sync.ts
                plugins/ (source-line attrs, fence tagging, link/image URL rewriting to omnimark://local)
                fences/  registry.ts  mermaid.ts  graphviz.ts  vega.ts  csv.ts  abc.ts  wavedrom.ts  nomnoml.ts
      export/   standalone-html.ts (inline CSS/KaTeX/SVG, optional data-URI images)
      styles/   markdown.css  print.css  themes/{github-light,github-dark,…}.css
  samples/    showcase.md + one file per feature (manual corpus + test fixtures)
  tests/      cpp/ (Qt Test)   web/unit (Vitest)   web/e2e (Playwright)
  packaging/  linux/ (flatpak manifest, AppImage, PKGBUILD, .desktop, mime xml)  windows/ (NSIS)  macos/ (Info.plist)
```

## Milestones

**M0 – Scaffold + spikes**
- `git init`, CMake + Qt skeleton window, Vite web project, CMake target that builds `web/` and embeds it.
- Spike the riskiest pieces:
  - module Web Worker + WASM (Graphviz) loading over the `omnimark://` scheme
  - QWebChannel round-trip
  - `printToPdf`
  - Mermaid render
- GitHub Actions matrix (ubuntu, windows, macos) with `jurplel/install-qt-action` (Qt 6.8 + qtwebengine, qtwebchannel, qtpositioning).

**M1 – Core viewer**
- Open via File→Open (`QFileDialog`), drag-and-drop, and CLI args. `SingleInstance` forwards args to the running window.
- Worker pipeline with all Markdown syntax above, plus Shiki with line numbers, copy, title and highlight.
- Relative images through `omnimark://local`, link handling.
- Live reload via `FileWatcher` (debounced, keeps scroll position).

**M2 – Embedded formats**
- Fence registry and hydrate step, v1-tier renderers, render cache, error boxes, diagram toolbar, re-render on theme change.

**M3 – Workspace + tabs**
- Open Folder, `WorkspaceDock` with filtered `QFileSystemModel`.
- `QTabWidget` with lazily created views, dirty marker, close guard.
- Session restore + recent files (`QSettings`). Wiki-link and relative `.md` navigation, back/forward per tab.

**M4 – Split editor**
- CodeMirror pane. Preview / Split / Source toggle on the native toolbar, set per tab.
- Live preview at about 150 ms debounce.
- Bidirectional scroll sync via `data-source-line`.
- Ctrl/Cmd+S → `Bridge::writeFile` (atomic `QSaveFile`).
- External-change conflict prompt when the file changes on disk while dirty. Optional vim keymap (`@replit/codemirror-vim`).

**M5 – Navigation + search**
- `TocDock` from reported headings, with scroll-spy highlighting the current section.
- `FindBar`: `findText` with match count in preview, CodeMirror search in the editor.
- Find in Folder: C++ `QDirIterator` + `QtConcurrent` scan, results in a dock list.
- Keyboard shortcut map (`QAction` shortcuts).

**M6 – Themes + export**
- Light/dark/system, following `QStyleHints::colorScheme` live. GitHub light/dark plus 2–3 presets.
- User custom CSS from `QStandardPaths::AppConfigLocation` (watched, hot-reloaded). Font and zoom settings (`QWebEngineView::setZoomFactor`).
- Export PDF via `QWebEnginePage::printToPdf` with page-size/margin options and `print.css`.
- Export standalone HTML (JS serializes, C++ writes). Native Print dialog via `QWebEngineView::print(QPrinter*)`.

**M7 – Packaging**
- Linux:
  - Flatpak (KDE runtime + `io.qt.qtwebengine.BaseApp`), the primary target
  - AppImage (linuxdeploy-plugin-qt)
  - Arch PKGBUILD
  - `.desktop` + MIME `text/markdown` association
- Windows: `windeployqt` + NSIS, with a file association.
- macOS: `macdeployqt` + dmg, `CFBundleDocumentTypes`, `QFileOpenEvent` handling.
- Code signing and auto-update are deferred.
- License: Qt and QtWebEngine are used under LGPLv3 with dynamic linking.

## Verification
- **Web unit (Vitest):** golden snapshots of `pipeline.ts`. Each `samples/*.md` file maps to expected HTML. Includes XSS payload sanitizer tests and fence registry dispatch.
- **Web UI (Playwright, Vite dev server, mock bridge):** sample loads; Mermaid/Graphviz SVG appears; theme switch re-renders; editor typing updates the preview; scroll sync works.
- **C++ (Qt Test):** `SchemeHandler` root allow-list (path traversal `..` rejected), `FileWatcher` re-arm after atomic replace, `SingleInstance` arg forwarding, `Bridge` read/write round-trip.
- **Manual:**
  1. Run `cmake --preset debug && cmake --build --preset debug && ./build/debug/omnimark samples/showcase.md`.
  2. Check every feature in light and dark mode.
  3. Edit `showcase.md` externally and confirm live reload.
  4. Export PDF and HTML, then inspect the output.
- **CI:** web lint, typecheck, Vitest and Playwright, plus C++ build + Qt Test on all three OSes, and package artifacts on tags.
