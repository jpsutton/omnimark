import L from 'leaflet';
import 'leaflet/dist/leaflet.css';
import { feature } from 'topojson-client';
import type { Topology } from 'topojson-specification';
import type { FenceRenderer, FenceContext } from './registry';

type GeoJsonObject = Parameters<typeof L.geoJSON>[0];

function draw(data: GeoJsonObject, el: HTMLElement, ctx: FenceContext) {
  const container = document.createElement('div');
  container.className = 'om-map';
  el.append(container);
  const map = L.map(container, { attributionControl: ctx.allowNetwork, scrollWheelZoom: false });
  if (ctx.allowNetwork) {
    L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
      maxZoom: 19,
      attribution: '&copy; OpenStreetMap contributors',
    }).addTo(map);
  }
  const color = ctx.theme === 'dark' ? '#58a6ff' : '#0969da';
  const layer = L.geoJSON(data, {
    style: { color, weight: 2, fillOpacity: 0.25 },
    pointToLayer: (_f, latlng) => L.circleMarker(latlng, { radius: 6, color, fillOpacity: 0.8 }),
    onEachFeature: (f, l) => {
      const props = f.properties as Record<string, unknown> | null;
      if (!props || !Object.keys(props).length) return;
      const table = document.createElement('table');
      for (const [k, v] of Object.entries(props)) {
        const row = table.insertRow();
        row.insertCell().textContent = k;
        row.insertCell().textContent = typeof v === 'object' ? JSON.stringify(v) : String(v);
      }
      l.bindPopup(table);
    },
  }).addTo(map);
  const bounds = layer.getBounds();
  if (bounds.isValid()) map.fitBounds(bounds, { padding: [16, 16], maxZoom: 14 });
  else map.setView([0, 0], 1);
  // Leaflet measures its container; hydrate inserts the output after this runs.
  requestAnimationFrame(() => map.invalidateSize());
}

export const geojson: FenceRenderer = {
  async render(code, el, ctx) {
    draw(JSON.parse(code) as GeoJsonObject, el, ctx);
  },
};

export const topojson: FenceRenderer = {
  async render(code, el, ctx) {
    const topology = JSON.parse(code) as Topology;
    const objects = Object.values(topology.objects ?? {});
    if (!objects.length) throw new Error('TopoJSON has no objects.');
    const features = objects.flatMap((object) => {
      const converted = feature(topology, object) as unknown as GeoJSON.Feature | GeoJSON.FeatureCollection;
      return 'features' in converted ? converted.features : [converted];
    });
    draw({ type: 'FeatureCollection', features } as GeoJsonObject, el, ctx);
  },
};
