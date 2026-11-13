import { createSkyUltraGame, type SkyUltraItemThumbnail, type SkyWebViewAPI } from '@skyultra/sdk';

/** One GPU readback at a time, shared by the inventory grid and equipment slots. */
export function createItemThumbnailQueue(
  load: (id: string, signal?: AbortSignal) => Promise<SkyUltraItemThumbnail>,
  options?: { cacheFailures?: boolean },
) {
  const cacheFailures = options?.cacheFailures ?? true;
  const cache = new Map<string, SkyUltraItemThumbnail>();
  const jobs = new Map<string, Array<{ signal: AbortSignal; resolve(value: SkyUltraItemThumbnail): void }>>();
  const listeners = new Map<string, Set<(dataUrl: string) => void>>();
  const normalizeKey = (rawId: string): string => (rawId.endsWith(':stolen') ? rawId.slice(0, -7) : rawId);
  let running = false;
  function notify(id: string, dataUrl?: string): void {
    if (!dataUrl?.startsWith('data:image/png;base64,')) return;
    listeners.get(id)?.forEach(listener => listener(dataUrl));
  }
  async function pump(): Promise<void> {
    if (running) return;
    running = true;
    try {
      while (jobs.size) {
        const [id, waiters] = jobs.entries().next().value!;
        const activeWaiter = waiters.find(waiter => !waiter.signal.aborted);
        if (!activeWaiter) {
          jobs.delete(id); waiters.forEach(waiter => waiter.resolve({ available: false })); continue;
        }
        let result: SkyUltraItemThumbnail;
        try { result = await load(id, activeWaiter.signal); }
        catch (error) { result = { available: false, message: error instanceof Error ? error.message : 'Thumbnail unavailable.' }; }
        // Never poison the cache or fail sibling live waiters when an in-flight signal is aborted.
        if (activeWaiter.signal.aborted && !result.available) {
          const latest = jobs.get(id) ?? waiters;
          const aborted = latest.filter(w => w.signal.aborted);
          const remaining = latest.filter(w => !w.signal.aborted);
          if (remaining.length > 0) jobs.set(id, remaining);
          else jobs.delete(id);
          aborted.forEach(w => w.resolve({ available: false }));
          continue;
        }
        // Invalid replies never become image sources. A transient busy response
        // is retried when the tile next becomes visible.
        if (result.available && !result.dataUrl?.startsWith('data:image/png;base64,')) result = { available: false, message: 'Invalid thumbnail image source.' };
        const transient =
          !result.available &&
          (!result.message || /busy|timed? out|abort|cancel|no loaded actor|no usable|not ready|not loaded|could not render|pipeline/i.test(result.message));
        if (result.available || (cacheFailures && !transient)) {
          cache.delete(id); cache.set(id, result);
          while (cache.size > 16384) cache.delete(cache.keys().next().value!);
          if (result.available) notify(id, result.dataUrl);
        }
        const settledWaiters = jobs.get(id) ?? waiters;
        jobs.delete(id); settledWaiters.forEach(waiter => waiter.resolve(result));
      }
    } finally { running = false; }
  }
  return {
    peek(rawId: string): SkyUltraItemThumbnail | undefined {
      const id = normalizeKey(rawId);
      const hit = cache.get(id);
      return hit?.available && hit.dataUrl ? hit : undefined;
    },
    request(rawId: string, signal: AbortSignal): Promise<SkyUltraItemThumbnail> {
      const id = normalizeKey(rawId);
      const hit = cache.get(id);
      if (hit) { cache.delete(id); cache.set(id, hit); return Promise.resolve(hit); }
      return new Promise(resolve => {
        const waiters = jobs.get(id) ?? []; waiters.push({ signal, resolve }); jobs.set(id, waiters); void pump();
      });
    },
    set(rawId: string, dataUrl: string): void {
      if (!dataUrl?.startsWith('data:image/png;base64,')) return;
      const id = normalizeKey(rawId);
      const entry: SkyUltraItemThumbnail = { available: true, dataUrl };
      cache.delete(id);
      cache.set(id, entry);
      while (cache.size > 16384) cache.delete(cache.keys().next().value!);
      notify(id, dataUrl);
    },
    clear(rawId?: string): void {
      if (rawId !== undefined) cache.delete(normalizeKey(rawId));
      else cache.clear();
    },
    subscribe(rawId: string, listener: (dataUrl: string) => void): () => void {
      const id = normalizeKey(rawId);
      const setForId = listeners.get(id) ?? new Set<(dataUrl: string) => void>();
      setForId.add(listener);
      listeners.set(id, setForId);
      return () => {
        const current = listeners.get(id);
        if (!current) return;
        current.delete(listener);
        if (!current.size) listeners.delete(id);
      };
    },
  };
}
const gpuLocks = new WeakMap<SkyWebViewAPI, Promise<unknown>>();
function runWithBridgeLock<T>(bridge: SkyWebViewAPI, signal: AbortSignal | undefined, task: () => Promise<T>, fallback: T): Promise<T> {
  const prev = gpuLocks.get(bridge) ?? Promise.resolve();
  const next = prev.catch(() => undefined).then(() => {
    if (signal?.aborted) return fallback;
    return task();
  });
  gpuLocks.set(bridge, next.catch(() => undefined));
  return next;
}
const queues = new WeakMap<SkyWebViewAPI, ReturnType<typeof createItemThumbnailQueue>>();
export function itemThumbnailQueue(bridge: SkyWebViewAPI) {
  let queue = queues.get(bridge);
  if (!queue) {
    const game = createSkyUltraGame(bridge);
    queue = createItemThumbnailQueue((id, signal) =>
      runWithBridgeLock(bridge, signal, () => game.thumbnail(id, signal ? { signal } : undefined), { available: false }),
    );
    queues.set(bridge, queue);
  }
  return queue;
}
const actorQueues = new WeakMap<SkyWebViewAPI, ReturnType<typeof createItemThumbnailQueue>>();
export function actorThumbnailQueue(bridge: SkyWebViewAPI) {
  let queue = actorQueues.get(bridge);
  if (!queue) {
    const game = createSkyUltraGame(bridge);
    queue = createItemThumbnailQueue(
      (id, signal) =>
        runWithBridgeLock(
          bridge,
          signal,
          () => {
            const [referenceId, baseFormId] = id.split(':');
            return game.actorThumbnail(
              { ...(referenceId ? { referenceId } : {}), ...(baseFormId ? { baseFormId } : {}) },
              signal ? { signal } : undefined,
            );
          },
          { available: false },
        ),
      { cacheFailures: false },
    );
    actorQueues.set(bridge, queue);
    // Studio camera captures (inventory / actor observatory) broadcast a new portrait
    // to every view so HUD widgets, the overview card and character sheets refresh.
    const target = queue;
    const hex = (value: string): string => '0x' + (Number(value) >>> 0).toString(16).toUpperCase().padStart(8, '0');
    bridge.on?.<{ referenceId?: string; baseFormId?: string; isPlayer?: boolean; dataUrl?: string }>('skyultra.portraitUpdated', event => {
      if (!event?.dataUrl) return;
      const ref = event.referenceId && event.referenceId !== '0' ? event.referenceId : '';
      const base = event.baseFormId && event.baseFormId !== '0' ? event.baseFormId : '';
      const keys = new Set<string>([`${ref}:${base}`, `${ref ? hex(ref) : ''}:${base ? hex(base) : ''}`, `${ref}:`, `${ref ? hex(ref) : ''}:`, `:${base}`, `:${base ? hex(base) : ''}`]);
      if (event.isPlayer) { keys.add(PLAYER_PORTRAIT_QUEUE_KEY); keys.add('20:7'); keys.add('20:'); keys.add(':7'); keys.add(':0x00000007'); }
      keys.forEach(key => { if (key !== ':') target.set(key, event.dataUrl!); });
    });
  }
  return queue;
}
/** Queue key used by every player portrait (HUD, overview, player card, sheets). */
export const PLAYER_PORTRAIT_QUEUE_KEY = '0x00000014:0x00000007';
