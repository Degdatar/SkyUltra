import { useEffect, useMemo, useRef, useState } from "react";
import {
  getSkyUltraClient,
  getSkyUltraRuntimeInfo,
  getSkyWebView,
  getSharedGameSnapshot,
  subscribeSkyUltraGame,
  type JsonValue,
  type Player,
  type SkyUltraClient,
  type SkyUltraGameSubscriptionOptions,
  type SkyUltraGameTopic,
  type SkyUltraGameUpdateEvent,
  type SkyUltraRuntimeInfo,
  type SkyWebViewAPI,
} from "@skyultra/sdk";

export type DefaultSkyUltraEvents = Record<string, JsonValue>;
export type DefaultSkyUltraRequests = Record<string, { params?: JsonValue; response: JsonValue }>;

/** Keep one typed client reference for the lifetime of this component. */
export function useSkyUltraClient<
  TEvents extends object = DefaultSkyUltraEvents,
  TRequests extends object = DefaultSkyUltraRequests,
>(): SkyUltraClient<TEvents, TRequests> {
  const client = useRef<SkyUltraClient<TEvents, TRequests> | null>(null);
  if (client.current === null) {
    client.current = getSkyUltraClient<TEvents, TRequests>();
  }
  return client.current;
}

/** Subscribe to a native event and clean up when its client, name, or handler changes. */
export function useSkyUltraEvent<
  TEvents extends object,
  TRequests extends object,
  TName extends Extract<keyof TEvents, string>,
>(
  client: SkyUltraClient<TEvents, TRequests>,
  name: TName,
  callback: (data: TEvents[TName]) => void,
): void {
  useEffect(
    () => client.on(name, (data) => callback(data as unknown as TEvents[TName])),
    [client, name, callback],
  );
}

/** Return the stable typed native-request function for the provided client. */
export function useSkyUltraRequest<
  TEvents extends object,
  TRequests extends object,
>(client: SkyUltraClient<TEvents, TRequests>): SkyUltraClient<TEvents, TRequests>["request"] {
  return useMemo(() => client.request, [client]);
}

/** Read the runtime metadata once for a component. */
export function useSkyUltraRuntimeInfo(): SkyUltraRuntimeInfo {
  return useMemo(() => getSkyUltraRuntimeInfo(), []);
}

/**
 * Subscribe to the host-owned shared game-data feed with topic filtering,
 * incremental delta merging, and shared per-bridge caching.
 */
export function useSkyUltraGameSubscription(
  options: SkyUltraGameSubscriptionOptions = {},
  bridge?: SkyWebViewAPI,
): {
  player: Player | null;
  lastUpdate: SkyUltraGameUpdateEvent | null;
  sequence: number;
  changedTopics: readonly SkyUltraGameTopic[];
} {
  const resolvedBridge = useMemo(() => bridge ?? getSkyWebView(), [bridge]);
  const [player, setPlayer] = useState<Player | null>(() => getSharedGameSnapshot(resolvedBridge));
  const [lastUpdate, setLastUpdate] = useState<SkyUltraGameUpdateEvent | null>(null);
  const topicsKey = options.topics ? options.topics.join(",") : "*";
  const scope = options.scope ?? "full";
  const intervalMs = options.intervalMs ?? 750;

  useEffect(() => {
    const sub = subscribeSkyUltraGame(
      resolvedBridge,
      (event) => {
        setPlayer(event.snapshot);
        setLastUpdate(event);
      },
      {
        topics: options.topics,
        scope,
        intervalMs,
      },
    );
    return () => sub.unsubscribe();
  }, [resolvedBridge, topicsKey, scope, intervalMs]);

  return {
    player,
    lastUpdate,
    sequence: lastUpdate?.sequence ?? player?.sequence ?? 0,
    changedTopics: lastUpdate?.changedTopics ?? [],
  };
}
