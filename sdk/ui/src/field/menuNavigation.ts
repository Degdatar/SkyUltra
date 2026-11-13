import type { Route } from './model.js';

// React.lazy and intent preloads share one promise, including while a download
// is in flight. Keep the imports explicit so Vite can discover each chunk.
const pages = {
  actors: () => import('./ActorsPage.js'),
  inventory: () => import('./InventoryPage.js'),
  character: () => import('./CharacterPage.js'),
  settings: () => import('./SettingsPage.js'),
  performance: () => import('./PerformancePage.js'),
  widgets: () => import('./WidgetsPage.js'),
  devtools: () => import('./DevStudioPage.js'),
  guide: () => import('./GuidePage.js'),
};
type Page = keyof typeof pages;
const pending = new Map<Page, Promise<unknown>>();
export function loadMenuPage<K extends Page>(page: K): ReturnType<(typeof pages)[K]> {
  let promise = pending.get(page);
  if (!promise) {
    promise = pages[page]();
    pending.set(page, promise);
    void promise.catch(() => pending.delete(page));
  }
  return promise as ReturnType<(typeof pages)[K]>;
}
export function preloadMenuPage(page: Route): void {
  if (page !== 'overview' && page !== 'components') void loadMenuPage(page).catch(() => undefined);
}

/** Text fields keep normal spaces, including selects and hotkey capture scopes. */
export function isMenuSearchKey(event: KeyboardEvent): boolean {
  return !event.defaultPrevented && !event.isComposing &&
    !event.ctrlKey && !event.metaKey && !event.altKey && !event.shiftKey &&
    (event.key === ' ' || event.code === 'Space') &&
    !(event.target as HTMLElement | null)?.closest?.('input,textarea,select,[contenteditable]:not([contenteditable="false"]),[role="textbox"]');
}
export function focusMenuSearch(root: ParentNode = document): boolean {
  const inputs = root.querySelectorAll<HTMLInputElement>('.page-search input');
  for (const input of Array.from(inputs)) {
    if (input.disabled || input.hidden || !input.getClientRects().length) continue;
    input.focus();
    return true;
  }
  return false;
}
