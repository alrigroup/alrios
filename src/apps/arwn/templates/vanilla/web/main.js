// app ARWN - front-end JS (bundled by esbuild)
window.ARWN && ARWN.ready(function (bridge) {
  const el = document.getElementById('app');
  if (el) el.textContent = 'ARWN pronto (' + bridge.version + ')';
});