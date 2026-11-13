import * as React from "react";
import { useEffect, useRef, useState } from "react";
import type { SkyUltraItemThumbnail, SkyWebViewAPI } from "@skyultra/sdk";
import { actorThumbnailQueue } from "./itemThumbnails.js";
import type { Player } from "./model.js";

const PLAYER_PORTRAIT_KEY = "0x00000014:0x00000007";

function DragonbornPortraitSvg(): React.JSX.Element {
  return (
    <svg
      className="player-portrait__svg"
      viewBox="0 0 96 96"
      fill="none"
      stroke="currentColor"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <circle
        className="player-portrait__halo"
        cx="48"
        cy="46"
        r="36"
        fill="currentColor"
        fillOpacity="0.1"
        stroke="currentColor"
        strokeOpacity="0.32"
        strokeWidth="1.2"
      />
      <path
        className="player-portrait__ring"
        d="M16 46a32 32 0 1 1 64 0"
        stroke="currentColor"
        strokeOpacity="0.45"
        strokeWidth="1.4"
      />
      {/* Shoulders & Cuirass */}
      <path
        className="player-portrait__mantle"
        d="M14 90c4-16 15-24 34-24s30 8 34 24"
        fill="currentColor"
        fillOpacity="0.18"
        stroke="currentColor"
        strokeWidth="1.8"
      />
      <path
        className="player-portrait__armor"
        d="M24 72l10-8h28l10 8M34 64l14 14 14-14"
        stroke="currentColor"
        strokeOpacity="0.78"
        strokeWidth="1.5"
      />
      {/* Horned Dragonborn Helm & Visage */}
      <path
        className="player-portrait__horns"
        d="M28 34c-10-2-16-10-15-20 4 7 10 11 18 12M68 34c10-2 16-10 15-20-4 7-10 11-18 12"
        fill="currentColor"
        fillOpacity="0.24"
        stroke="currentColor"
        strokeWidth="1.85"
      />
      <path
        className="player-portrait__head"
        d="M31 32c2-10 32-10 34 0 2 11 1 22-6 29-4 4-7 6-11 6s-7-2-11-6c-7-7-8-18-6-29Z"
        fill="currentColor"
        fillOpacity="0.16"
        stroke="currentColor"
        strokeWidth="1.85"
      />
      {/* Brow plate, nose guard & eyes */}
      <path
        className="player-portrait__detail"
        d="M30 36h36M48 26v22M36 41l7 1.5M60 41l-7 1.5M42 56h12"
        stroke="currentColor"
        strokeOpacity="0.88"
        strokeWidth="1.5"
      />
      <circle className="player-portrait__eye" cx="39.5" cy="43.5" r="2" fill="currentColor" stroke="none" />
      <circle className="player-portrait__eye" cx="56.5" cy="43.5" r="2" fill="currentColor" stroke="none" />
    </svg>
  );
}

export function PlayerPortrait({
  player,
  bridge,
  size = "md",
  badge,
  className = "",
}: {
  player: Player | null;
  bridge?: SkyWebViewAPI;
  size?: "sm" | "md" | "lg";
  badge?: React.ReactNode;
  className?: string;
}): React.JSX.Element {
  const ref = useRef<HTMLSpanElement>(null);
  const requestKey = PLAYER_PORTRAIT_KEY;
  const [thumbnail, setThumbnail] = useState<SkyUltraItemThumbnail | null>(() =>
    bridge ? actorThumbnailQueue(bridge).peek(requestKey) ?? null : null,
  );
  const lastSignatureRef = useRef<string | undefined>(undefined);

  // Load cached portrait from Overwrite/SkyUltra/Cache/Portraits/ immediately;
  // only reset when switching to a different character or when the user updates the portrait in-game.
  const playerAvailable = player ? player.available !== false : false;
  const playerSignature = playerAvailable && player ? player.playerName : "";

  useEffect(() => {
    if (!bridge) return;
    const cached = actorThumbnailQueue(bridge).peek(requestKey);
    if (cached) {
      setThumbnail(cached);
    }
    return actorThumbnailQueue(bridge).subscribe(requestKey, (dataUrl) => {
      setThumbnail({ available: true, dataUrl });
    });
  }, [bridge, requestKey]);

  useEffect(() => {
    if (!bridge || !playerSignature) return;
    if (lastSignatureRef.current !== undefined && lastSignatureRef.current !== playerSignature) {
      actorThumbnailQueue(bridge).clear(requestKey);
      setThumbnail(null);
    }
    lastSignatureRef.current = playerSignature;

    let active = true;
    let controller: AbortController | null = null;
    let retryTimer = 0;
    let attempts = 0;

    const start = (): void => {
      if (!active) return;
      const hit = actorThumbnailQueue(bridge).peek(requestKey);
      if (hit) {
        setThumbnail(hit);
        return;
      }
      if (controller) return;
      const current = new AbortController();
      controller = current;
      void actorThumbnailQueue(bridge)
        .request(requestKey, current.signal)
        .then((value) => {
          if (!active || current.signal.aborted) return;
          controller = null;
          if (value.available && value.dataUrl) {
            setThumbnail(value);
          } else if (attempts < 16) {
            attempts += 1;
            retryTimer = window.setTimeout(start, 220 + attempts * 180);
          }
        });
    };

    start();
    const unsubscribe = bridge.on?.("skyultra.visibility", () => {
      attempts = 0;
      start();
    });
    document.addEventListener("visibilitychange", start);

    return () => {
      active = false;
      if (retryTimer) window.clearTimeout(retryTimer);
      controller?.abort();
      unsubscribe?.();
      document.removeEventListener("visibilitychange", start);
    };
  }, [bridge, requestKey, playerSignature]);

  const hasPhoto = Boolean(thumbnail?.available && thumbnail.dataUrl);

  return (
    <span
      ref={ref}
      className={
        `player-portrait player-portrait--${size}` +
        (hasPhoto ? " is-photo" : " is-svg") +
        (className ? ` ${className}` : "")
      }
      title={player?.playerName ? `${player.playerName} (Live 3D Portrait)` : "Dragonborn Portrait"}
    >
      <span className="player-portrait__frame">
        {hasPhoto ? (
          <img
            className="player-portrait__img"
            src={thumbnail!.dataUrl}
            alt={player?.playerName ?? "Dragonborn"}
            draggable={false}
          />
        ) : (
          <DragonbornPortraitSvg />
        )}
      </span>
      {badge !== undefined && <small className="player-portrait__badge">{badge}</small>}
    </span>
  );
}
