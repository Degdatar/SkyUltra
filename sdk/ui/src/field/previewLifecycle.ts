export const previewNavigationEvent = 'skyultra:preview-navigation';

export function leaveMenuPage(): void {
  // Cancel outgoing controllers before React can suspend on the next page.
  window.dispatchEvent(new Event(previewNavigationEvent));
}
