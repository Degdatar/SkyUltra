// Intentional Fault #2: Calls 'game.getInventory' without declaring it in manifest.json permissions.pageRequests
// Intentional Fault #3: Omits calling 'ui.pageReady' after initial render

async function bootBrokenSample() {
  const statusNode = document.getElementById('status');
  const bridge = window.SkyWebView;
  if (!bridge) {
    if (statusNode) statusNode.textContent = 'SkyWebView bridge unavailable.';
    return;
  }
  try {
    await bridge.request('game.getInventory', { offset: 0, limit: 8 });
    if (statusNode) statusNode.textContent = 'Inventory loaded unexpectedly.';
  } catch (err) {
    if (statusNode) {
      statusNode.textContent = 'Permission denied as expected: ' + (err?.message || String(err));
    }
  }
}

void bootBrokenSample();
