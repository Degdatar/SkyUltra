/** Keep the pointer preview readable at every edge of the game viewport. */
export function itemDragPosition(x: number, y: number, width = 1280, height = 720): { x: number; y: number } {
  const margin = 10, gap = 18, previewWidth = 248, previewHeight = 78;
  const left = x + gap + previewWidth <= width - margin ? x + gap : x - gap - previewWidth;
  const top = y + gap + previewHeight <= height - margin ? y + gap : y - gap - previewHeight;
  return { x: Math.max(margin, Math.min(left, width - previewWidth - margin)), y: Math.max(margin, Math.min(top, height - previewHeight - margin)) };
}
