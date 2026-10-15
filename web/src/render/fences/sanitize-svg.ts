import DOMPurify from 'dompurify';

// <use> is needed by WaveDrom (it references glyphs in <defs>), but only
// same-document references are allowed: no external sprite loading.
DOMPurify.addHook('afterSanitizeAttributes', (node) => {
  if (node.nodeName.toLowerCase() !== 'use') return;
  for (const attr of ['href', 'xlink:href']) {
    const value = node.getAttribute(attr);
    if (value !== null && !value.startsWith('#')) node.removeAttribute(attr);
  }
});

/** Strips scripts and event handlers from renderer-produced SVG markup. */
export function sanitizeSvg(svg: string): string {
  return DOMPurify.sanitize(svg, {
    USE_PROFILES: { svg: true, svgFilters: true },
    ADD_TAGS: ['foreignObject', 'use'],
    ADD_ATTR: ['xlink:href', 'href'],
  });
}
