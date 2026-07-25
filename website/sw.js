/* DigiFrame PWA — cache the app shell so it opens offline (Bluetooth needs
 * no network). Bump CACHE when any shell file changes. */
const CACHE = "digiframe-v1";
const SHELL = [
  "./",
  "./index.html",
  "./style.css",
  "./ble.js",
  "./app.js",
  "./manifest.webmanifest",
];

self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL)).then(() => self.skipWaiting()));
});

self.addEventListener("activate", (e) => {
  e.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k)))
    ).then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", (e) => {
  if (e.request.method !== "GET") return;
  e.respondWith(
    caches.match(e.request).then((hit) => hit || fetch(e.request).catch(() => hit))
  );
});
