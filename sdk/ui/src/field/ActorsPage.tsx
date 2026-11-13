import * as React from "react";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  createSkyUltraGame,
  type SkyUltraEquipmentSlot,
  type SkyUltraInventoryPage,
  type SkyUltraItemThumbnail,
  type SkyWebViewAPI,
} from "@skyultra/sdk";
import { SkySegmentedControl } from "../core.js";
import { ModelStage } from "./ActorStage.js";
import { EquipmentGlyph } from "./EquipmentGlyph.js";
import { equipmentSlots, fitsEquipmentSlot } from "./EquipmentPanel.js";
import { Icon } from "./icons.js";
import { itemDragPosition } from "./itemDrag.js";
import { ItemImage, ItemThumbnail } from "./ItemThumbnail.js";
import { actorThumbnailQueue } from "./itemThumbnails.js";
import { formId, type Item, type Npc, type Player } from "./model.js";
import { Empty } from "./pieces.js";
import { useActors } from "./useFieldData.js";

type ActorLineage =
  | "human"
  | "elf"
  | "orc"
  | "khajiit"
  | "argonian"
  | "hooved"
  | "avian"
  | "leporine"
  | "predator"
  | "draconic";

type DragState = { item: Item; pointer: number; x: number; y: number; active: boolean };

const wardrobePageSize = 24;

export function actorRaceName(actor: Npc): string {
  const rawRace = (actor.race ?? "").trim();
  const lowerRace = rawRace.toLowerCase();
  const lowerName = (actor.name ?? "").toLowerCase();
  if (
    lowerRace === "rabbit" &&
    (lowerName.includes("chicken") || lowerName.includes("hen") || lowerName.includes("rooster"))
  ) {
    return "Chicken";
  }
  return rawRace || "Unknown";
}

function classifyLineage(actor: Npc): ActorLineage {
  const text = (actorRaceName(actor) + " " + actor.name).toLowerCase();
  if (/khajiit|senche|alphiq/.test(text)) return "khajiit";
  if (/argonian|saxhleel/.test(text)) return "argonian";
  if (/orc|orsimer/.test(text)) return "orc";
  if (/elf|mer\b|altmer|bosmer|dunmer|falmer|dremora/.test(text)) return "elf";
  if (/chicken|hen\b|rooster|hawk|bird|crow|hagrav/.test(text)) return "avian";
  if (/rabbit|hare|skeever|mudcrab/.test(text)) return "leporine";
  if (/goat|cow|deer|elk|vale deer|horse|mammoth|horker/.test(text)) return "hooved";
  if (/dragon|draugr|skeleton|lich|priest|dwarven|automaton|atronach|wisp|ghost|vampire lord|boneman|wrathman|mistman/.test(text))
    return "draconic";
  if (/wolf|dog|husky|hound|fox|bear|sabre|troll|werewolf|spider|chaurus|spriggan|giant/.test(text)) return "predator";
  return "human";
}

function hashSeed(value: string): number {
  let hash = 2166136261;
  for (let i = 0; i < value.length; i++) {
    hash ^= value.charCodeAt(i);
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

function ActorFaceSvg({ actor }: { actor: Npc }): React.JSX.Element {
  const lineage = classifyLineage(actor);
  const female = /female/i.test(actor.sex);
  const elder = /old|elder/i.test(actor.race);
  const variant = hashSeed(actor.name + ":" + actor.baseFormId) % 3;

  return (
    <svg className={"actor-face-svg actor-face-svg--" + lineage} viewBox="0 0 96 96" fill="none" aria-hidden="true">
      <circle className="actor-face-svg__halo" cx="48" cy="42" r="31" />
      <path className="actor-face-svg__ring" d="M20 42a28 28 0 1 1 56 0" />

      {lineage === "khajiit" && (
        <>
          <path className="actor-face-svg__mantle" d="M16 90c3-16 14-24 32-24s29 8 32 24" />
          <path className="actor-face-svg__hood" d="M24 66l8-10h32l8 10" />
          <path className="actor-face-svg__head" d="M27 38l-5-18 15 9c7-3 15-3 22 0l15-9-5 18c4 8 3 19-5 25-6 5-11 7-16 7s-10-2-16-7c-8-6-9-17-5-25Z" />
          <path className="actor-face-svg__detail" d="M22 20l-3-5M74 20l3-5M36 42l6 2M60 42l-6 2M48 44v9l-4 3h8l-4-3M28 52l-9 2M29 56l-8 4M68 52l9 2M67 56l8 4" />
          <circle className="actor-face-svg__eye" cx="39" cy="43" r="1.8" />
          <circle className="actor-face-svg__eye" cx="57" cy="43" r="1.8" />
        </>
      )}

      {lineage === "argonian" && (
        <>
          <path className="actor-face-svg__mantle" d="M17 90c4-15 15-23 31-23s27 8 31 23" />
          <path className="actor-face-svg__detail" d="M32 26l-10-10 4 14M64 26l10-10-4 14M40 21l-4-9M56 21l4-9M48 18v-8" />
          <path className="actor-face-svg__head" d="M31 32c5-7 29-7 34 0 4 7 4 17-2 25l-9 10c-4 3-8 3-12 0l-9-10c-6-8-6-18-2-25Z" />
          <path className="actor-face-svg__detail" d="M48 27v27M37 40l6 2M59 40l-6 2M43 58h10" />
          <circle className="actor-face-svg__eye" cx="39" cy="42" r="1.7" />
          <circle className="actor-face-svg__eye" cx="57" cy="42" r="1.7" />
        </>
      )}

      {lineage === "orc" && (
        <>
          <path className="actor-face-svg__mantle" d="M14 90c3-17 15-25 34-25s31 8 34 25" />
          <path className="actor-face-svg__hair" d="M30 30c3-11 33-11 36 0" />
          <path className="actor-face-svg__detail" d="M29 36l-9-5 5 12M67 36l9-5-5 12" />
          <path className="actor-face-svg__head" d="M30 31c4-8 32-8 36 0 3 9 2 22-5 30-5 5-9 7-13 7s-8-2-13-7c-7-8-8-21-5-30Z" />
          <path className="actor-face-svg__detail" d="M36 39l8 3M60 39l-8 3M48 41v10M41 58h14M39 59l2-7 3 6M57 59l-2-7-3 6" />
          <circle className="actor-face-svg__eye" cx="40" cy="43" r="1.7" />
          <circle className="actor-face-svg__eye" cx="56" cy="43" r="1.7" />
        </>
      )}

      {lineage === "elf" && (
        <>
          <path className="actor-face-svg__mantle" d="M18 90c3-16 14-24 30-24s27 8 30 24" />
          <path className="actor-face-svg__hair" d={female ? "M27 56c-2-18 3-36 21-36s23 18 21 36" : "M30 36c2-13 34-13 36 0"} />
          <path className="actor-face-svg__detail" d="M31 39l-14-7 9 14M65 39l14-7-9 14" />
          <path className="actor-face-svg__head" d="M32 33c3-9 29-9 32 0 2 10 0 21-7 29-4 4-6 6-9 6s-5-2-9-6c-7-8-9-19-7-29Z" />
          <path className="actor-face-svg__detail" d="M37 39l8-2M59 39l-8-2M48 41v11M44 57h8" />
          <circle className="actor-face-svg__eye" cx="41" cy="42" r="1.7" />
          <circle className="actor-face-svg__eye" cx="55" cy="42" r="1.7" />
        </>
      )}

      {lineage === "hooved" && (
        <>
          <path className="actor-face-svg__mantle" d="M24 90c3-14 11-22 24-22s21 8 24 22" />
          <path className="actor-face-svg__detail" d="M38 28c-6-12-18-13-22-4-2 5 2 10 8 9M58 28c6-12 18-13 22-4 2 5-2 10-8 9M31 35l-10 3 8 6M65 35l10 3-8 6" />
          <path className="actor-face-svg__head" d="M35 28h26l5 16-9 21H39l-9-21 5-16Z" />
          <path className="actor-face-svg__detail" d="M44 57h8M48 65l-3 7M48 65l3 7" />
          <circle className="actor-face-svg__eye" cx="39" cy="42" r="1.8" />
          <circle className="actor-face-svg__eye" cx="57" cy="42" r="1.8" />
        </>
      )}

      {lineage === "avian" && (
        <>
          <path className="actor-face-svg__mantle" d="M24 90c3-14 11-22 24-22s21 8 24 22" />
          <path className="actor-face-svg__hair" d="M41 25c1-6 6-8 8-3 3-5 8-3 7 4" />
          <path className="actor-face-svg__head" d="M34 42c0-10 6-17 14-17s14 7 14 17c0 12-5 24-14 26-9-2-14-14-14-26Z" />
          <path className="actor-face-svg__detail" d="M48 43l9 5-9 4-4-4 4-5ZM46 54c0 5 4 7 6 3" />
          <circle className="actor-face-svg__eye" cx="43" cy="39" r="1.9" />
        </>
      )}

      {lineage === "leporine" && (
        <>
          <path className="actor-face-svg__mantle" d="M24 90c3-13 11-20 24-20s21 7 24 20" />
          <path className="actor-face-svg__detail" d="M41 34c-5-14-4-22 0-22s5 8 4 21M55 34c5-14 4-22 0-22s-5 8-4 21" />
          <path className="actor-face-svg__head" d="M34 45c0-8 6-14 14-14s14 6 14 14c0 9-6 17-14 17s-14-8-14-17Z" />
          <path className="actor-face-svg__detail" d="M46 51l2 2 2-2M48 53v4" />
          <circle className="actor-face-svg__eye" cx="42" cy="45" r="1.8" />
          <circle className="actor-face-svg__eye" cx="54" cy="45" r="1.8" />
        </>
      )}

      {lineage === "predator" && (
        <>
          <path className="actor-face-svg__mantle" d="M18 90c4-15 14-23 30-23s26 8 30 23" />
          <path className="actor-face-svg__head" d="M28 36l-4-16 14 9c6-2 14-2 20 0l14-9-4 16c5 8 4 19-4 26l-10 7h-12l-10-7c-8-7-9-18-4-26Z" />
          <path className="actor-face-svg__detail" d="M36 41l7 3M60 41l-7 3M44 55h8l-4 5-4-5ZM42 63h12" />
          <circle className="actor-face-svg__eye" cx="39" cy="44" r="1.9" />
          <circle className="actor-face-svg__eye" cx="57" cy="44" r="1.9" />
        </>
      )}

      {lineage === "draconic" && (
        <>
          <path className="actor-face-svg__mantle" d="M16 90c4-16 15-24 32-24s28 8 32 24" />
          <path className="actor-face-svg__detail" d="M31 28l-13-14 6 18M65 28l13-14-6 18M48 22v-9" />
          <path className="actor-face-svg__head" d="M30 31l18-8 18 8 4 17-12 20H38L26 48l4-17Z" />
          <path className="actor-face-svg__detail" d="M37 41l8 3M59 41l-8 3M42 58h12M45 54v8M51 54v8" />
          <circle className="actor-face-svg__eye" cx="40" cy="44" r="2" />
          <circle className="actor-face-svg__eye" cx="56" cy="44" r="2" />
        </>
      )}

      {lineage === "human" && (
        <>
          <path className="actor-face-svg__mantle" d="M16 90c3-16 14-24 32-24s29 8 32 24" />
          <path className="actor-face-svg__hood" d="M35 66l13 10 13-10" />
          {female ? (
            <path
              className="actor-face-svg__hair"
              d={
                variant === 0
                  ? "M26 58c-2-20 4-38 22-38s24 18 22 38c-4-5-6-14-6-22-5-5-11-7-16-7s-11 2-16 7c0 8-2 17-6 22Z"
                  : variant === 1
                    ? "M28 53c-1-19 5-34 20-34s21 15 20 34"
                    : "M27 60c-3-21 4-39 21-39s24 18 21 39"
              }
            />
          ) : (
            <path
              className="actor-face-svg__hair"
              d={
                variant === 0
                  ? "M29 39c1-14 8-20 19-20s18 6 19 20"
                  : variant === 1
                    ? "M28 46c0-17 7-26 20-26s20 9 20 26"
                    : "M30 36c2-12 9-17 18-17s16 5 18 17"
              }
            />
          )}
          <path
            className="actor-face-svg__head"
            d={
              female
                ? "M32 35c2-9 30-9 32 0 2 10 0 20-7 27-3 3-6 5-9 5s-6-2-9-5c-7-7-9-17-7-27Z"
                : "M31 33c3-9 31-9 34 0 2 10 1 21-6 29-4 4-7 6-11 6s-7-2-11-6c-7-8-8-19-6-29Z"
            }
          />
          <path
            className="actor-face-svg__detail"
            d={
              elder
                ? "M37 39l7 1M59 39l-7 1M48 41v10M43 56h10M35 49l3 4M61 49l-3 4"
                : !female && variant !== 2
                  ? "M37 39l7 1M59 39l-7 1M48 41v10M42 56h12M36 55c3 8 9 12 12 12s9-4 12-12"
                  : "M38 39l6-1M58 39l-6-1M48 41v10M44 56h8"
            }
          />
          <circle className="actor-face-svg__eye" cx="41" cy="43" r="1.7" />
          <circle className="actor-face-svg__eye" cx="55" cy="43" r="1.7" />
        </>
      )}
    </svg>
  );
}

const ActorPortrait = React.memo(function ActorPortrait({
  actor,
  bridge,
  paused = false,
  refreshToken = 0,
}: {
  actor: Npc;
  bridge: SkyWebViewAPI;
  paused?: boolean;
  refreshToken?: number;
}): React.JSX.Element {
  const ref = useRef<HTMLSpanElement>(null);
  const portraitKey = (actor.referenceId ?? "") + ":" + (actor.baseFormId ?? "");
  const [state, setState] = useState<{ key: string; value: SkyUltraItemThumbnail } | null>(() => {
    const cached = actorThumbnailQueue(bridge).peek(portraitKey);
    return cached ? { key: portraitKey, value: cached } : null;
  });
  const current = state?.key === portraitKey ? state.value : (actorThumbnailQueue(bridge).peek(portraitKey) ?? null);
  const lineage = classifyLineage(actor);

  useEffect(() => {
    return actorThumbnailQueue(bridge).subscribe(portraitKey, (dataUrl) => {
      setState({ key: portraitKey, value: { available: true, dataUrl } });
    });
  }, [bridge, portraitKey]);

  useEffect(() => {
    const node = ref.current;
    if (!node || paused || (!actor.referenceId && !actor.baseFormId)) return;
    if (refreshToken === 0) {
      const hit = actorThumbnailQueue(bridge).peek(portraitKey);
      if (hit?.available && hit.dataUrl) {
        setState({ key: portraitKey, value: hit });
        return;
      }
    }
    let active = true;
    let controller: AbortController | null = null;
    let startTimer = 0;
    let retryTimer = 0;
    let attempts = 0;
    const start = () => {
      startTimer = 0;
      if (controller || !active) return;
      controller = new AbortController();
      void actorThumbnailQueue(bridge)
        .request(portraitKey, controller.signal)
        .then((value) => {
          if (!active || controller?.signal.aborted) return;
          if (!value.available && /busy|timed? out/i.test(value.message ?? "") && attempts < 6) {
            attempts += 1;
            controller = null;
            retryTimer = window.setTimeout(start, 260 * attempts);
            return;
          }
          setState({ key: portraitKey, value });
        });
    };
    if (typeof IntersectionObserver === "undefined") {
      start();
      return () => {
        active = false;
        window.clearTimeout(startTimer);
        window.clearTimeout(retryTimer);
        controller?.abort();
      };
    }
    const observer = new IntersectionObserver(
      (entries) => {
        if (entries.some((entry) => entry.isIntersecting)) {
          if (!controller && !startTimer) {
            startTimer = window.setTimeout(start, 75);
          }
        } else {
          window.clearTimeout(startTimer);
          startTimer = 0;
          window.clearTimeout(retryTimer);
          controller?.abort();
          controller = null;
        }
      },
      { rootMargin: "96px" },
    );
    observer.observe(node);
    return () => {
      active = false;
      window.clearTimeout(startTimer);
      window.clearTimeout(retryTimer);
      observer.disconnect();
      controller?.abort();
    };
  }, [portraitKey, bridge, paused, actor.referenceId, actor.baseFormId, refreshToken]);

  return (
    <span ref={ref} className={"actor-portrait actor-portrait--" + lineage + (current?.available && current.dataUrl ? " has-render" : "")}>
      {!(current?.available && current.dataUrl) && (
        <span className="actor-portrait__backdrop">
          <ActorFaceSvg actor={actor} />
        </span>
      )}
      {current?.available && current.dataUrl && (
        <span className="actor-portrait__media">
          <img src={current.dataUrl} alt="" loading="lazy" decoding="async" draggable={false} />
        </span>
      )}
      <i className="actor-portrait__sheen" aria-hidden="true" />
    </span>
  );
}, (previous, next) => previous.bridge === next.bridge &&
  (previous.paused === next.paused || (next.refreshToken === 0 && Boolean(actorThumbnailQueue(next.bridge).peek((next.actor.referenceId ?? "") + ":" + (next.actor.baseFormId ?? ""))?.dataUrl))) &&
  previous.refreshToken === next.refreshToken && previous.actor.referenceId === next.actor.referenceId &&
  previous.actor.baseFormId === next.actor.baseFormId && previous.actor.name === next.actor.name &&
  previous.actor.race === next.actor.race && previous.actor.sex === next.actor.sex);

export default function ActorsPage({
  player = null,
  bridge,
  obscured,
}: {
  player?: Player | null;
  bridge: SkyWebViewAPI;
  obscured: boolean;
}): React.JSX.Element {
  const game = useMemo(() => createSkyUltraGame(bridge), [bridge]);
  const [mode, setMode] = useState<"nearby" | "followers" | "search">("nearby");
  const [layout, setLayout] = useState<"grid" | "list">("grid");
  const [radius, setRadius] = useState(6000);
  const [query, setQuery] = useState("");
  const [selectedId, setSelectedId] = useState("");
  const [poseDrawer, setPoseDrawer] = useState(false);

  // Wardrobe / Inventory state for dressing up actors
  const [gearCategory, setGearCategory] = useState<"all" | "armor" | "weapon" | "ammo">("all");
  const [gearQuery, setGearQuery] = useState("");
  const [gearOffset, setGearOffset] = useState(0);
  const [remoteGear, setRemoteGear] = useState<SkyUltraInventoryPage | null>(null);
  const [gearLoading, setGearLoading] = useState(false);
  const [selectedGearId, setSelectedGearId] = useState("");
  const [actorEquipmentOverrides, setActorEquipmentOverrides] = useState<Record<string, Player["equipment"]>>({});
  const [equipBusy, setEquipBusy] = useState(false);
  const [equipMessage, setEquipMessage] = useState("");
  const [revision, setRevision] = useState(0);

  const actionRef = useRef<AbortController | null>(null);
  const lockedRef = useRef(false);
  const dragRef = useRef<DragState | null>(null);
  const suppressClickRef = useRef(false);
  const [dragging, setDragging] = useState<Item | null>(null);
  const [hoverSlot, setHoverSlot] = useState("");
  const ghostRef = useRef<HTMLDivElement>(null);
  const ghostPoint = useRef({ x: 0, y: 0 });
  const dragImage = useRef<string | undefined>(undefined);
  const [thumbnailsDeferred, setThumbnailsDeferred] = useState(false);
  const thumbnailResumeTimer = useRef(0);
  const deferThumbnails = (): void => {
    window.clearTimeout(thumbnailResumeTimer.current);
    setThumbnailsDeferred(true);
    thumbnailResumeTimer.current = window.setTimeout(() => setThumbnailsDeferred(false), 180);
  };

  const data = useActors(bridge, "actors", radius, mode, query);
  const followerCount = data.actors.filter((actor) => actor.follower).length;
  const rows = (mode === "search" ? data.results : data.actors.filter((actor) => mode !== "followers" || actor.follower)).filter(
    (actor) =>
      mode === "search" ||
      !query.trim() ||
      (actor.name + " " + actorRaceName(actor) + " " + actor.sex).toLowerCase().includes(query.trim().toLowerCase()),
  );
  const selected = rows.find((actor) => (actor.referenceId ?? actor.baseFormId) === selectedId) ?? rows[0] ?? null;
  const selectedKey = selected ? selected.referenceId ?? selected.baseFormId : "";
  const totalCount = mode === "search" ? data.searchCount : mode === "followers" ? followerCount : data.count;
  const totalLabel =
    mode === "search" ? "MATCHING NPC RECORDS" : mode === "followers" ? "FOLLOWERS IN RANGE" : "ACTORS IN RANGE";

  useEffect(() => {
    deferThumbnails();
    return () => window.clearTimeout(thumbnailResumeTimer.current);
  }, [selectedKey]);
  const thumbnailsPaused = obscured || thumbnailsDeferred;

  useEffect(() => {
    setGearOffset(0);
  }, [gearCategory, gearQuery]);

  useEffect(() => () => actionRef.current?.abort(), [bridge]);

  useEffect(() => {
    if (obscured) {
      dragRef.current = null;
      setDragging(null);
      setHoverSlot("");
    }
  }, [obscured]);

  useEffect(() => {
    if (obscured) return;
    let active = true;
    let pending = false;
    const controller = new AbortController();
    const refreshGear = async (): Promise<void> => {
      if (pending || document.hidden || !(bridge.getVisibility?.().visible ?? true)) return;
      pending = true;
      setGearLoading(true);
      try {
        const page = await game.inventory(
          {
            query: gearQuery.trim(),
            category: gearCategory,
            equipment: "equippable" as unknown as "all",
            sort: "name",
            offset: gearOffset,
            limit: wardrobePageSize,
          },
          { signal: controller.signal },
        );
        if (active) {
          setRemoteGear(page);
          if (page.available && gearOffset >= page.totalMatches && gearOffset > 0) {
            setGearOffset(Math.max(0, Math.floor((page.totalMatches - 1) / wardrobePageSize) * wardrobePageSize));
          }
        }
      } catch {
        // Fallback to player snapshot inventory when offline or in preview mode
      } finally {
        pending = false;
        if (active) setGearLoading(false);
      }
    };
    const timer = window.setTimeout(() => void refreshGear(), gearQuery ? 200 : 0);
    return () => {
      active = false;
      controller.abort();
      window.clearTimeout(timer);
    };
  }, [game, bridge, obscured, gearCategory, gearQuery, gearOffset, revision]);

  const fallbackGear = useMemo(
    () =>
      (player?.inventory ?? [])
        .filter((item) => !!item.equipSlots?.length)
        .filter((item) => gearCategory === "all" || item.category === gearCategory)
        .filter(
          (item) =>
            !gearQuery.trim() ||
            (item.name + " " + item.category + " " + item.id + " " + formId(item.id))
              .toLowerCase()
              .includes(gearQuery.trim().toLowerCase()),
        ),
    [player, gearCategory, gearQuery],
  );

  const rawGearItems = remoteGear?.available ? remoteGear.results : fallbackGear.slice(gearOffset, gearOffset + wardrobePageSize);
  const gearItems = useMemo(() => rawGearItems.filter((item) => !!item.equipSlots?.length), [rawGearItems]);
  const gearTotal = remoteGear?.available ? remoteGear.totalMatches : fallbackGear.length;
  const selectedGear = gearItems.find((item) => item.id === selectedGearId) ?? gearItems[0] ?? null;

  const actorEquipment: Player["equipment"] = useMemo(() => {
    const empty: Player["equipment"] = {
      head: null,
      body: null,
      hands: null,
      feet: null,
      neck: null,
      ring: null,
      mainHand: null,
      offHand: null,
      ammo: null,
      cape: null,
      lantern: null,
    };
    if (!selected) return empty;
    return {
      ...empty,
      ...(selected.equipment ?? {}),
      ...(actorEquipmentOverrides[selectedKey] ?? {}),
    };
  }, [selected, selectedKey, actorEquipmentOverrides]);

  const occupiedCount = equipmentSlots.filter((slot) => !!actorEquipment[slot.id]).length;

  async function changeActorEquipment(item: Item, slot: SkyUltraEquipmentSlot, unequip = false): Promise<void> {
    if (!selected || obscured || lockedRef.current) return;
    if (!selected.referenceId) {
      setEquipMessage("Select a loaded actor in range to equip or unequip items on their 3D model.");
      return;
    }
    lockedRef.current = true;
    setEquipBusy(true);
    setEquipMessage((unequip ? "Unequipping " : "Equipping ") + item.name + (unequip ? " from " : " on ") + selected.name + "…");
    const controller = new AbortController();
    actionRef.current = controller;
    try {
      const result = await (unequip
        ? game.unequipActorItem(selected.referenceId, item.id, slot, { signal: controller.signal })
        : game.equipActorItem(selected.referenceId, item.id, slot, { signal: controller.signal }));
      const destination = (result.slot ?? slot) as keyof Player["equipment"];
      if (!result.accepted) {
        if (unequip && destination && destination !== ("auto" as keyof Player["equipment"])) {
          setActorEquipmentOverrides((prev) => ({
            ...prev,
            [selectedKey]: { ...(prev[selectedKey] ?? actorEquipment), [destination]: null },
          }));
        }
        setEquipMessage(result.message ?? "Skyrim declined the actor equipment change.");
        return;
      }
      if (result.equipment) {
        const nextEquip = { ...result.equipment };
        if (unequip && destination && destination !== ("auto" as keyof Player["equipment"])) {
          nextEquip[destination] = null;
        }
        setActorEquipmentOverrides((prev) => ({ ...prev, [selectedKey]: nextEquip }));
      }
      for (let attempt = 0; attempt < 6 && !controller.signal.aborted; attempt++) {
        await new Promise((resolve) => window.setTimeout(resolve, 180));
        if (controller.signal.aborted) return;
        try {
          const nearby = await game.nearby(Math.max(radius, 6000), { signal: controller.signal });
          if (controller.signal.aborted) return;
          const refreshed = nearby.results.find((candidate) => candidate.referenceId === selected.referenceId);
          if (refreshed?.equipment) {
            const worn =
              destination && destination !== ("auto" as keyof Player["equipment"])
                ? refreshed.equipment[destination]?.id === item.id
                : false;
            if (unequip ? !worn : worn) {
              setActorEquipmentOverrides((prev) => ({ ...prev, [selectedKey]: refreshed.equipment! }));
              break;
            }
          }
        } catch {
          break;
        }
      }
      setRevision((value) => value + 1);
      setEquipMessage(item.name + (unequip ? " unequipped from " : " equipped on ") + selected.name + ".");
    } catch (error) {
      if (!controller.signal.aborted) {
        setEquipMessage(error instanceof Error ? error.message : "Could not change actor equipment.");
      }
    } finally {
      lockedRef.current = false;
      if (!controller.signal.aborted) setEquipBusy(false);
    }
  }

  const endDragAtPoint = (
    pointerId: number,
    clientX: number,
    clientY: number,
    cancel = false,
    targetEl?: HTMLButtonElement | null,
  ): void => {
    const current = dragRef.current;
    if (!current || current.pointer !== pointerId) return;
    dragRef.current = null;
    setDragging(null);
    setHoverSlot("");
    try {
      if (targetEl?.hasPointerCapture?.(pointerId)) {
        targetEl.releasePointerCapture(pointerId);
      }
    } catch {
      // ignore
    }
    if (!current.active) return;
    suppressClickRef.current = true;
    const target = cancel
      ? ""
      : document.elementFromPoint(clientX, clientY)?.closest<HTMLElement>("[data-equip-slot]")?.dataset.equipSlot ?? "";
    if (fitsEquipmentSlot(current.item, target)) {
      void changeActorEquipment(current.item, target as SkyUltraEquipmentSlot);
    } else {
      setEquipMessage(
        target ? "This item does not fit that equipment slot." : "Drop the item onto a matching slot on the actor.",
      );
    }
  };

  const endDrag = (event: React.PointerEvent<HTMLButtonElement>, cancel = false): void => {
    endDragAtPoint(event.pointerId, event.clientX, event.clientY, cancel, event.currentTarget);
  };
  const endDragAtPointRef = useRef(endDragAtPoint);
  endDragAtPointRef.current = endDragAtPoint;

  useEffect(() => {
    const onWindowPointerMove = (event: PointerEvent): void => {
      const current = dragRef.current;
      if (!current || current.pointer !== event.pointerId) return;
      if (!current.active && Math.hypot(event.clientX - current.x, event.clientY - current.y) < 6) return;
      const starting = !current.active;
      current.active = true;
      if (starting) {
        setDragging(current.item);
        setSelectedGearId(current.item.id);
      }
      ghostPoint.current = itemDragPosition(event.clientX, event.clientY, window.innerWidth, window.innerHeight);
      if (ghostRef.current) {
        ghostRef.current.style.transform =
          "translate3d(" + ghostPoint.current.x + "px," + ghostPoint.current.y + "px,0)";
      }
      setHoverSlot(
        document.elementFromPoint(event.clientX, event.clientY)?.closest<HTMLElement>("[data-equip-slot]")?.dataset
          .equipSlot ?? "",
      );
    };
    const onWindowPointerUp = (event: PointerEvent): void => {
      if (dragRef.current && dragRef.current.pointer === event.pointerId) {
        endDragAtPointRef.current(event.pointerId, event.clientX, event.clientY, false);
      }
    };
    const onWindowPointerCancel = (event: PointerEvent): void => {
      if (dragRef.current && dragRef.current.pointer === event.pointerId) {
        endDragAtPointRef.current(event.pointerId, event.clientX, event.clientY, true);
      }
    };
    window.addEventListener("pointermove", onWindowPointerMove, { passive: true });
    window.addEventListener("pointerup", onWindowPointerUp);
    window.addEventListener("pointercancel", onWindowPointerCancel);
    return () => {
      window.removeEventListener("pointermove", onWindowPointerMove);
      window.removeEventListener("pointerup", onWindowPointerUp);
      window.removeEventListener("pointercancel", onWindowPointerCancel);
    };
  }, []);

  const renderSlotButton = ({ id, label, biped }: (typeof equipmentSlots)[number]): React.JSX.Element => {
    const item = actorEquipment[id] ?? null;
    const candidate = dragging ?? selectedGear;
    const fits = fitsEquipmentSlot(candidate, id);
    return (
      <div
        key={id}
        className={
          "equipment-target" +
          (item ? " is-occupied" : "") +
          (dragging && fits ? " can-drop" : "") +
          (hoverSlot === id ? (fits ? " is-over" : " is-invalid") : "")
        }
      >
        <button
          type="button"
          className="equipment-slot"
          data-equip-slot={id}
          disabled={equipBusy || obscured || !selected?.referenceId}
          aria-label={label + ": " + (item?.name ?? "Empty") + (fits && candidate ? ". Equip " + candidate.name : "")}
          title={
            label +
            (biped ? " · slot " + biped : "") +
            "\n" +
            (item?.name ?? "Empty") +
            (fits && candidate ? "\nClick or drop to equip " + candidate.name : "")
          }
          onClick={() => {
            if (selectedGear && fits) void changeActorEquipment(selectedGear, id);
          }}
        >
          <span className="equipment-slot__label">
            {label}
            {biped && <em>{biped}</em>}
          </span>
          <span className="equipment-slot__bezel">
            <span className="equipment-slot__art">
              <EquipmentGlyph slot={id} />
              {item && <ItemThumbnail item={item} bridge={bridge} paused={obscured} />}
            </span>
          </span>
          <strong>{item?.name ?? "Not equipped"}</strong>
        </button>
        {item && !!item.equipSlots?.length && (
          <button
            type="button"
            className="equipment-unequip"
            disabled={equipBusy || obscured || !selected?.referenceId}
            onClick={() => void changeActorEquipment(item, id, true)}
            aria-label={"Unequip " + item.name + " from " + label}
            title={"Unequip " + item.name}
          >
            ×
          </button>
        )}
      </div>
    );
  };

  return (
    <div className="page actors-page actors-page--observatory">
      <header className="observatory-masthead">
        <div className="observatory-heading">
          <span className="eyebrow">THE PEOPLE OF SKYRIM</span>
          <h1>
            Actor observatory<span className="title-period">.</span>
          </h1>
        </div>
        <div className="observatory-workspaces">
          <SkySegmentedControl
            label="Actor source"
            value={mode}
            onValueChange={(value) => {
              setMode(value);
              setQuery("");
              setSelectedId("");
            }}
            options={[
              { value: "nearby", label: "Nearby", count: data.count },
              { value: "followers", label: "Followers", count: followerCount },
              { value: "search", label: "Find NPC" },
            ]}
          />
        </div>
        <div className="observatory-totals">
          {mode !== "search" && (
            <div className="observatory-radar-pill">
              <i className="status-dot" />
              <span>
                RADAR <b>{radius / 1000}K u</b>
              </span>
            </div>
          )}
          <div className="observatory-total">
            <strong>{totalCount.toLocaleString()}</strong>
            <span>{totalLabel}</span>
          </div>
        </div>
      </header>

      <div className="actor-workbench">
        <div className="actor-left-stack" onScrollCapture={deferThumbnails}>
          <section className="surface roster-surface" aria-label="Actor portrait directory">
            <div className="roster-toolbar">
              <label className="page-search">
                <Icon name="search" size={16} />
                <input
                  aria-label={mode === "search" ? "Search NPC records" : "Filter nearby actors"}
                  value={query}
                  onChange={(event) => setQuery(event.currentTarget.value)}
                  placeholder={
                    mode === "search" ? "Search every NPC record in Skyrim…" : "Filter by name, race, or lineage…"
                  }
                />
                {query ? (
                  <button
                    type="button"
                    className="search-clear"
                    aria-label="Clear actor search"
                    onClick={() => setQuery("")}
                  >
                    <Icon name="close" size={12} />
                  </button>
                ) : (
                  <kbd>Space</kbd>
                )}
              </label>
              <SkySegmentedControl
                label="Actor layout"
                value={layout}
                onValueChange={setLayout}
                options={[
                  { value: "grid", label: <Icon name="grid" size={14} />, ariaLabel: "Grid view" },
                  { value: "list", label: <Icon name="list" size={14} />, ariaLabel: "List view" },
                ]}
              />
            </div>

            {mode !== "search" ? (
              <div className="roster-radius">
                <span>SCAN RANGE</span>
                <div className="roster-radius__pills">
                  {[1500, 3000, 6000, 12000].map((value) => (
                    <button key={value} type="button" aria-pressed={radius === value} onClick={() => setRadius(value)}>
                      {value / 1000}K
                    </button>
                  ))}
                </div>
                <small>
                  <b>{rows.length}</b> SHOWN
                  {followerCount > 0 ? " · " + followerCount + " FOLLOWER" + (followerCount === 1 ? "" : "S") : ""}
                </small>
              </div>
            ) : (
              <div className="roster-radius roster-radius--search">
                <span>BASE RECORD ARCHIVE</span>
                <small>
                  <b>{rows.length}</b> SHOWN · 40 RESULT LIMIT
                </small>
              </div>
            )}

            <div className={"actor-roster actor-roster--" + layout}>
              {rows.length ? (
                rows.map((actor) => {
                  const active = actor === selected;
                  const lineage = classifyLineage(actor);
                  const raceLabel = actorRaceName(actor);
                  const distanceText =
                    actor.distance !== undefined
                      ? Math.round(actor.distance).toLocaleString() + " u"
                      : formId(actor.baseFormId);
                  return (
                    <button
                      type="button"
                      className={
                        "actor-row actor-row--" +
                        layout +
                        (active ? " is-active" : "") +
                        (actor.follower ? " is-follower" : "") +
                        (actor.dead ? " is-dead" : "")
                      }
                      data-lineage={lineage}
                      key={actor.referenceId ?? actor.baseFormId}
                      onClick={() => setSelectedId(actor.referenceId ?? actor.baseFormId)}
                    >
                      <div className="actor-row__art">
                        <ActorPortrait
                          actor={actor}
                          bridge={bridge}
                          paused={thumbnailsPaused}
                          refreshToken={active ? revision : 0}
                        />
                        {layout === "grid" && (
                          <>
                            <div className="actor-card__top">
                              {actor.follower ? (
                                <span className="actor-badge actor-badge--follower">FOLLOWER</span>
                              ) : actor.dead ? (
                                <span className="actor-badge actor-badge--dead">DECEASED</span>
                              ) : (
                                <span className="actor-badge actor-badge--lineage">{raceLabel}</span>
                              )}
                              {actor.level !== undefined && (
                                <span className="actor-badge actor-badge--level">LVL {actor.level}</span>
                              )}
                            </div>
                            <span className="actor-card__distance">{distanceText}</span>
                          </>
                        )}
                      </div>
                      <span className="actor-row__body">
                        <strong>{actor.name}</strong>
                        <small>
                          {raceLabel} · {actor.sex}
                          {layout === "list" && actor.level !== undefined ? " · Lvl " + actor.level : ""}
                        </small>
                      </span>
                      {layout === "list" && (
                        <span className="actor-row__end">
                          {actor.follower ? (
                            <b className="follower-label">FOLLOWER</b>
                          ) : actor.dead ? (
                            <b className="dead-label">DECEASED</b>
                          ) : null}
                          <small>{distanceText}</small>
                        </span>
                      )}
                    </button>
                  );
                })
              ) : (
                <Empty
                  title={mode === "search" ? "Find someone in Skyrim" : "No actors in this scan"}
                  detail={mode === "search" ? data.searchStatus : data.status}
                  icon="actors"
                />
              )}
            </div>

            <div className="roster-footer">
              <span>{mode === "search" ? data.searchCount + " matching base records · 40 result limit" : data.status}</span>
              <span>{layout === "grid" ? "PORTRAIT GALLERY" : "DOSSIER ROSTER"}</span>
            </div>
          </section>

          <section className="surface actor-wardrobe-surface" aria-label="Equipment wardrobe for dressing up actors">
            <div className="wardrobe-toolbar">
              <div className="wardrobe-toolbar__title">
                <span className="eyebrow">WARDROBE &amp; ARMORY</span>
                <strong>Dress up {selected ? selected.name : "actor"}</strong>
              </div>
              <div className="wardrobe-categories" role="group" aria-label="Wardrobe categories">
                {(
                  [
                    { id: "all", label: "All gear" },
                    { id: "armor", label: "Armor" },
                    { id: "weapon", label: "Weapons" },
                    { id: "ammo", label: "Ammo" },
                  ] as const
                ).map((cat) => (
                  <button
                    key={cat.id}
                    type="button"
                    aria-pressed={gearCategory === cat.id}
                    onClick={() => setGearCategory(cat.id)}
                  >
                    {cat.label}
                  </button>
                ))}
              </div>
              <label className="page-search wardrobe-search">
                <Icon name="search" size={14} />
                <input
                  aria-label="Search wardrobe equipment"
                  value={gearQuery}
                  onChange={(event) => setGearQuery(event.currentTarget.value)}
                  placeholder="Filter equipment…"
                />
                {gearQuery && (
                  <button
                    type="button"
                    className="search-clear"
                    aria-label="Clear wardrobe search"
                    onClick={() => setGearQuery("")}
                  >
                    <Icon name="close" size={11} />
                  </button>
                )}
              </label>
            </div>

            <div className="wardrobe-grid">
              {gearItems.length ? (
                gearItems.map((item) => {
                  const slotTag = item.equipSlots?.[0] ?? "auto";
                  const wornByActor = equipmentSlots.some((slot) => actorEquipment[slot.id]?.id === item.id);
                  return (
                    <button
                      type="button"
                      key={item.id}
                      title={item.name + " — Drag to a matching slot on " + (selected?.name ?? "the actor")}
                      data-category={item.category}
                      aria-pressed={item.id === selectedGear?.id}
                      className={
                        "item-tile wardrobe-tile can-equip" +
                        (item.id === selectedGear?.id ? " is-active" : "") +
                        (wornByActor ? " is-equipped" : "")
                      }
                      onClick={() => {
                        if (suppressClickRef.current) {
                          suppressClickRef.current = false;
                          return;
                        }
                        setSelectedGearId(item.id);
                      }}
                      onDoubleClick={() => {
                        if (selected?.referenceId && item.equipSlots?.length) {
                          void changeActorEquipment(item, item.equipSlots[0] ?? "auto");
                        }
                      }}
                      onDragStart={(event) => event.preventDefault()}
                      onPointerDown={(event) => {
                        suppressClickRef.current = false;
                        if (event.button !== 0 || equipBusy || obscured || !item.equipSlots?.length) return;
                        try {
                          event.currentTarget.setPointerCapture(event.pointerId);
                        } catch {
                          // ignore
                        }
                        const image = event.currentTarget.querySelector?.<HTMLImageElement>(".item-thumbnail img");
                        dragImage.current = image?.getAttribute("src") || undefined;
                        ghostPoint.current = itemDragPosition(
                          event.clientX,
                          event.clientY,
                          window.innerWidth,
                          window.innerHeight,
                        );
                        dragRef.current = {
                          item,
                          pointer: event.pointerId,
                          x: event.clientX,
                          y: event.clientY,
                          active: false,
                        };
                      }}
                      onPointerMove={(event) => {
                        const current = dragRef.current;
                        if (!current || current.pointer !== event.pointerId) return;
                        if (!current.active && Math.hypot(event.clientX - current.x, event.clientY - current.y) < 6) return;
                        const starting = !current.active;
                        current.active = true;
                        event.preventDefault();
                        if (starting) {
                          setDragging(item);
                          setSelectedGearId(item.id);
                        }
                        ghostPoint.current = itemDragPosition(
                          event.clientX,
                          event.clientY,
                          window.innerWidth,
                          window.innerHeight,
                        );
                        if (ghostRef.current) {
                          ghostRef.current.style.transform =
                            "translate3d(" + ghostPoint.current.x + "px," + ghostPoint.current.y + "px,0)";
                        }
                        setHoverSlot(
                          document
                            .elementFromPoint(event.clientX, event.clientY)
                            ?.closest<HTMLElement>("[data-equip-slot]")?.dataset.equipSlot ?? "",
                        );
                      }}
                      onPointerUp={(event) => endDrag(event)}
                      onPointerCancel={(event) => endDrag(event, true)}
                    >
                      <span className="item-tile__top">
                        <small>
                          {wornByActor ? (
                            <>
                              <Icon name="check" size={9} />
                              WORN
                            </>
                          ) : (
                            slotTag
                          )}
                        </small>
                        <span>
                          {item.value.toLocaleString()}
                          <Icon name="gold" size={9} />
                        </span>
                      </span>
                      <span className="item-tile__image">
                        <ItemThumbnail item={item} bridge={bridge} paused={thumbnailsPaused} />
                      </span>
                      <span className="item-tile__copy">
                        <strong>{item.name}</strong>
                        <small>
                          {item.category}
                          <span>{item.weight} wt</span>
                        </small>
                      </span>
                    </button>
                  );
                })
              ) : (
                <Empty
                  title={gearLoading ? "Reading wardrobe" : "No equippable gear found"}
                  detail="Armor, weapons, capes, lanterns, jewelry, and ammo from your inventory appear here."
                  icon="inventory"
                />
              )}
            </div>

            <div className="roster-footer wardrobe-footer">
              <span role="status">
                {equipMessage ||
                  (selectedGear && selected?.referenceId
                    ? `Drag ${selectedGear.name} to a matching slot on ${selected.name}, or double-click to equip.`
                    : "Drag any item from your wardrobe onto an actor equipment slot.")}
              </span>
              <div className="wardrobe-footer__actions">
                {selectedGear && selected?.referenceId && (
                  <button
                    type="button"
                    className="control-button is-active"
                    disabled={equipBusy || obscured}
                    onClick={() => void changeActorEquipment(selectedGear, selectedGear.equipSlots?.[0] ?? "auto")}
                  >
                    Equip on {selected.name.split(" ")[0]}
                  </button>
                )}
                <button
                  type="button"
                  className="control-button"
                  disabled={gearOffset === 0 || gearLoading}
                  onClick={() => setGearOffset((value) => Math.max(0, value - wardrobePageSize))}
                >
                  Prev
                </button>
                <button
                  type="button"
                  className="control-button"
                  disabled={gearOffset + rawGearItems.length >= gearTotal || gearLoading}
                  onClick={() => setGearOffset((value) => value + wardrobePageSize)}
                >
                  Next
                </button>
              </div>
            </div>
          </section>
        </div>

        {selected ? (
          <section className="surface outfit-panel actor-inspector" aria-label="Selected actor equipment and 3D studio">
            <div className="outfit-heading">
              <div>
                <div className="actor-dossier-eyebrow">
                  <span className="eyebrow">{selected.referenceId ? "CHARACTER STUDIO & EQUIPMENT" : "NPC BASE RECORD"}</span>
                  {selected.follower && <span className="actor-status-pill actor-status-pill--follower">FOLLOWER</span>}
                  {selected.dead && <span className="actor-status-pill actor-status-pill--dead">DECEASED</span>}
                </div>
                <h2 title={selected.name}>{selected.name}</h2>
                <p>
                  {actorRaceName(selected)}
                  <span>·</span>
                  {selected.sex}
                  {selected.distance !== undefined && (
                    <>
                      <span>·</span>
                      {Math.round(selected.distance).toLocaleString()} u away
                    </>
                  )}
                  <span>·</span>
                  {occupiedCount} / {equipmentSlots.length} slots equipped
                </p>
              </div>
              <div className="actor-studio-head-tools">
                <button
                  type="button"
                  className={"control-button actor-pose-toggle" + (poseDrawer ? " is-active" : "")}
                  aria-pressed={poseDrawer}
                  onClick={() => setPoseDrawer((value) => !value)}
                >
                  <Icon name="camera" size={13} />
                  {poseDrawer ? "Outfit studio" : "Poses & frames"}
                </button>
                <span className="outfit-level">
                  <small>LEVEL</small>
                  <strong>{selected.level ?? "—"}</strong>
                </span>
              </div>
            </div>

            <div className="outfit-vitals actor-outfit-vitals" aria-label="Actor vital signs">
              <div className="outfit-vital outfit-vital--health" role="meter" aria-label="Health">
                <span>
                  Vitality / Health
                  <b>
                    {selected.health !== undefined
                      ? `${Math.round(selected.health).toLocaleString()} / ${Math.round(selected.healthMax ?? selected.health).toLocaleString()}`
                      : "Base Record"}
                  </b>
                </span>
                <div>
                  <i
                    style={{
                      width:
                        (selected.health !== undefined
                          ? Math.max(0, Math.min(100, (selected.health / Math.max(1, selected.healthMax ?? 1)) * 100))
                          : 100) + "%",
                    }}
                  />
                </div>
              </div>
            </div>

            <div className="outfit-doll">
              <div className="equipment-rail equipment-rail--armor">{equipmentSlots.slice(0, 5).map(renderSlotButton)}</div>
              <div className="outfit-model">
                <ModelStage
                  subject={{ kind: "actor", actor: selected }}
                  bridge={bridge}
                  obscured={obscured}
                  equipment={!poseDrawer}
                  refreshToken={revision}
                  onCapturePortrait={async () => {
                    const result = await game.actorThumbnail({
                      ...(selected.referenceId ? { referenceId: selected.referenceId } : {}),
                      ...(selected.baseFormId ? { baseFormId: selected.baseFormId } : {}),
                      usePreview: true,
                    });
                    if (!result.available || !result.dataUrl) {
                      throw new Error(result.message ?? "Could not capture portrait from current view.");
                    }
                    const portraitKey = (selected.referenceId ?? "") + ":" + (selected.baseFormId ?? "");
                    actorThumbnailQueue(bridge).set(portraitKey, result.dataUrl);
                    setEquipMessage(`Updated ${selected.name}'s portrait and saved to Overwrite cache.`);
                    return `Updated ${selected.name}'s portrait (cached in Overwrite).`;
                  }}
                />
              </div>
              <div className="equipment-rail equipment-rail--accessories">{equipmentSlots.slice(5).map(renderSlotButton)}</div>
            </div>

            <div className="outfit-stats">
              <span>
                <Icon name="shield" size={17} />
                <small>ARMOR RATING</small>
                <strong>{Math.round(selected.armorRating ?? 0).toLocaleString()}</strong>
              </span>
              <span>
                <Icon name="actors" size={17} />
                <small>BASE FORM</small>
                <strong>{formId(selected.baseFormId)}</strong>
              </span>
              <span>
                <Icon name="rune" size={17} />
                <small>REFERENCE ID</small>
                <strong>{selected.referenceId ? formId(selected.referenceId) : "BASE RECORD"}</strong>
              </span>
            </div>
          </section>
        ) : (
          <section className="surface outfit-panel actor-inspector actor-inspector--empty">
            <Empty
              title="Select a person"
              detail="Choose an actor from the portrait gallery to inspect their equipment, dress them up, and view their 3D model."
              icon="actors"
            />
          </section>
        )}
      </div>

      {dragging && (
        <div
          ref={ghostRef}
          className="equipment-drag-ghost"
          style={{ transform: "translate3d(" + ghostPoint.current.x + "px," + ghostPoint.current.y + "px,0)" }}
          aria-hidden="true"
        >
          <span className="equipment-drag-ghost__art">
            {dragImage.current ? (
              <ItemImage item={dragging} src={dragImage.current} />
            ) : (
              <ItemThumbnail item={dragging} bridge={bridge} paused={obscured} />
            )}
          </span>
          <span className="equipment-drag-ghost__copy">
            <strong>{dragging.name}</strong>
            <small>
              {hoverSlot
                ? fitsEquipmentSlot(dragging, hoverSlot)
                  ? "Release to equip on " + (selected?.name ?? "actor")
                  : "Incompatible slot"
                : "Drag to an actor equipment slot"}
            </small>
          </span>
        </div>
      )}
    </div>
  );
}
