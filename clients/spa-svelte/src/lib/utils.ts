import { createToaster } from '@skeletonlabs/skeleton-svelte';

export const toaster = createToaster();

export function escapeHtml(s: string) {
  return s
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

export function bytesToSize(bytes: number) {
  const sizes = ["Bytes", "KB", "MB", "GB", "TB"];
  if (bytes === 0) return "0 Byte";
  const i = Math.floor(Math.log(bytes) / Math.log(1024));
  return Math.round(bytes / Math.pow(1024, i)) + " " + sizes[i];
}

export function hash(str: string) {
  let hash = 0;
  if (str.length === 0) return hash;
  for (let i = 0; i < str.length; i++) {
    const chr = str.charCodeAt(i);
    hash = (hash << 5) - hash + chr;
    hash |= 0; // Convert to 32bit integer
  }
  return hash;
}

export function nextRandomId(len: number): string {
  // https://github.com/ai/nanoid/blob/main/nanoid.js
  return crypto.getRandomValues(new Uint8Array(len)).reduce(
    (len, e) => len += (e &= 63) < 36 ? e.toString(36) : e < 62 ? (e - 26).toString(36).toUpperCase() : e > 62 ? "-" : "_", "")
}


const _lastLogs: { date: string, data: string }[] = [];

export function clog(...args: any[]) {
  console.log(...args);
  if (0 < args.length) {
    const key = args.map(a => JSON.stringify(a)).join(' ');
    const now = Date.now();
    _lastLogs.push({ date: new Date(now).toLocaleTimeString(), data: key });
    while (100 < _lastLogs.length) {
      _lastLogs.shift();
    }
  }
}

export function getLastLogs() {
  return _lastLogs;
}

export function apiUrl(path: string) {
  const base = window.apiServerUrl || '';
  // clog('apiUrl ', base + path);
  return base + path;
}