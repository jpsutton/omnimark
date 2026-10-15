import Papa from 'papaparse';
import type { FenceRenderer } from './registry';

const MAX_ROWS = 5000;

function compare(a: string, b: string): number {
  const na = Number(a);
  const nb = Number(b);
  if (a.trim() !== '' && b.trim() !== '' && !Number.isNaN(na) && !Number.isNaN(nb)) return na - nb;
  return a.localeCompare(b, undefined, { numeric: true, sensitivity: 'base' });
}

function buildTable(rows: string[][]): HTMLTableElement {
  const [header, ...body] = rows;
  const table = document.createElement('table');
  table.className = 'om-csv';
  const thead = table.createTHead().insertRow();
  const tbody = table.createTBody();

  let sortColumn = -1;
  let ascending = true;

  const fill = (data: string[][]) => {
    tbody.replaceChildren();
    for (const row of data) {
      const tr = tbody.insertRow();
      for (let i = 0; i < header.length; i++) tr.insertCell().textContent = row[i] ?? '';
    }
  };

  header.forEach((name, column) => {
    const th = document.createElement('th');
    th.textContent = name;
    th.tabIndex = 0;
    th.title = 'Sort';
    const sort = () => {
      ascending = sortColumn === column ? !ascending : true;
      sortColumn = column;
      for (const cell of thead.cells) cell.removeAttribute('aria-sort');
      th.setAttribute('aria-sort', ascending ? 'ascending' : 'descending');
      const sorted = [...body].sort((a, b) => compare(a[column] ?? '', b[column] ?? ''));
      fill(ascending ? sorted : sorted.reverse());
    };
    th.addEventListener('click', sort);
    th.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        sort();
      }
    });
    thead.append(th);
  });
  fill(body);
  return table;
}

function makeRenderer(delimiter: string): FenceRenderer {
  return {
    async render(code, el) {
      const parsed = Papa.parse<string[]>(code.trim(), { delimiter, skipEmptyLines: true });
      const rows = parsed.data;
      if (!rows.length) throw new Error('No rows found.');
      if (parsed.errors.length && rows.length < 2) throw new Error(parsed.errors[0].message);
      const truncated = rows.length > MAX_ROWS + 1;
      el.append(buildTable(truncated ? rows.slice(0, MAX_ROWS + 1) : rows));
      if (truncated) {
        const note = document.createElement('p');
        note.className = 'om-csv-note';
        note.textContent = `Showing the first ${MAX_ROWS} of ${rows.length - 1} rows.`;
        el.append(note);
      }
    },
  };
}

export const csv = makeRenderer(',');
export const tsv = makeRenderer('\t');
export default csv;
