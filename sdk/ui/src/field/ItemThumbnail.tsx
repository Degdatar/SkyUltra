import * as React from 'react';
import { useEffect, useRef, useState } from 'react';
import type { SkyUltraItemThumbnail, SkyWebViewAPI } from '@skyultra/sdk';
import { itemThumbnailQueue } from './itemThumbnails.js';
import { initials, type Item } from './model.js';

export function ItemImage({ item, src, loading = false, title, nodeRef }: {
  item: Pick<Item, 'name'>; src?: string; loading?: boolean; title?: string; nodeRef?: React.Ref<HTMLSpanElement>;
}): React.JSX.Element {
  return <span ref={nodeRef} className="item-art item-thumbnail" title={title ?? item.name}><span className="item-thumbnail__media">
    {src ? <img src={src} alt={item.name} draggable={false} /> : <span className="item-thumbnail__fallback" aria-label={loading ? 'Loading item artwork' : 'Item artwork unavailable'}><b>{initials(item.name)}</b><small>{loading ? 'Preview…' : 'No model'}</small></span>}
  </span></span>;
}

export const ItemThumbnail = React.memo(function ItemThumbnail({ item, bridge, paused = false }: { item: Item; bridge?: SkyWebViewAPI; paused?: boolean }): React.JSX.Element {
  const node = useRef<HTMLSpanElement>(null);
  const [result, setResult] = useState<SkyUltraItemThumbnail | null>(() => (bridge ? itemThumbnailQueue(bridge).peek(item.id) ?? null : null));
  useEffect(() => {
    setResult(bridge ? itemThumbnailQueue(bridge).peek(item.id) ?? null : null);
  }, [item.id, bridge]);
  useEffect(() => {
    if (!bridge) return;
    return itemThumbnailQueue(bridge).subscribe(item.id, dataUrl => {
      setResult({ available: true, dataUrl });
    });
  }, [item.id, bridge]);
  useEffect(() => {
    if (!bridge || paused) return;
    const cached = itemThumbnailQueue(bridge).peek(item.id);
    if (cached) { setResult(cached); return; }
    let controller: AbortController | null = null;
    let visible = false, disposed = false;
    const update = (): void => {
      const hit = itemThumbnailQueue(bridge).peek(item.id);
      if (hit) { setResult(hit); return; }
      const allowed = visible && !document.hidden && (bridge.getVisibility?.().visible ?? true);
      if (!allowed) { controller?.abort(); controller = null; return; }
      if (controller) return;
      const current = new AbortController(); controller = current;
      void itemThumbnailQueue(bridge).request(item.id, current.signal).then(value => { if (!disposed && !current.signal.aborted) setResult(value); });
    };
    const observer = typeof IntersectionObserver === 'undefined' ? null : new IntersectionObserver(entries => { visible = entries.some(entry => entry.isIntersecting); update(); }, { threshold: .01 });
    if (node.current) observer?.observe(node.current);
    if (!observer) { visible = true; update(); }
    const unsubscribe = bridge.on('skyultra.visibility', update);
    document.addEventListener('visibilitychange', update);
    return () => { disposed = true; controller?.abort(); observer?.disconnect(); unsubscribe(); document.removeEventListener('visibilitychange', update); };
  }, [item.id, bridge, paused]);
  return <ItemImage nodeRef={node} item={item} src={result?.available ? result.dataUrl : undefined} loading={!result && !!bridge} title={result?.message} />;
}, (previous, next) => previous.bridge === next.bridge && previous.item.id === next.item.id && previous.item.name === next.item.name &&
  (previous.paused === next.paused || Boolean(next.bridge && itemThumbnailQueue(next.bridge).peek(next.item.id)?.dataUrl)));
