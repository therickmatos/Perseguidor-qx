// Guarda a pagina no celular para abrir sem internet (o Bluetooth nao precisa dela).
// Troque a VERSAO a cada mudanca nos arquivos: o celular baixa a nova na proxima
// vez que abrir com internet e passa a usar na seguinte.
const VERSAO = 'pqx-1';
const ARQUIVOS = [
  './', 'index.html', 'estilo.css', 'app.js', 'manifest.webmanifest',
  'icones/icone.svg', 'icones/icone-192.png', 'icones/icone-512.png', 'icones/icone-maskable-512.png',
];

self.addEventListener('install', (ev) => {
  ev.waitUntil(caches.open(VERSAO).then((c) => c.addAll(ARQUIVOS)).then(() => self.skipWaiting()));
});

self.addEventListener('activate', (ev) => {
  ev.waitUntil(caches.keys()
    .then((ks) => Promise.all(ks.filter((k) => k !== VERSAO).map((k) => caches.delete(k))))
    .then(() => self.clients.claim()));
});

// Primeiro o que esta guardado; se nao tiver, a rede
self.addEventListener('fetch', (ev) => {
  if (ev.request.method !== 'GET') return;
  ev.respondWith(caches.match(ev.request, { ignoreSearch: true }).then((r) => r || fetch(ev.request)));
});
