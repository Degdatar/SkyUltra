import * as React from "react";
import { useEffect, useMemo, useState } from "react";
import {
  createSkyUltraGame,
  persistSkyUltraProfileStore,
  readLocalSkyUltraProfileStore,
  resolveEffectiveSkyUltraProfile,
  updateProfileForContext,
  type SkyUltraEquipmentSlot,
  type SkyUltraProfileStoreV2,
  type SkyUltraSavedLoadoutRecord,
  type SkyWebViewAPI,
} from "@skyultra/sdk";
import {
  SkyRadarChart,
  SkyRingMeter,
  SkyStatBar,
  skyStatMaximum,
} from "../core.js";
import { EquipmentGlyph } from "./EquipmentGlyph.js";
import { ItemThumbnail } from "./ItemThumbnail.js";
import { classifyEffectVisual, EffectIcon, Icon, itemIcon, SkillIcon } from "./icons.js";
import { PlayerPortrait } from "./PlayerPortrait.js";
import { Empty, SectionTitle } from "./pieces.js";
import { formId, type Item, type Player } from "./model.js";

type CharacterTab = "overview" | "defenses" | "skills" | "effects" | "loadouts";
type ConcreteEquipmentSlot = Exclude<SkyUltraEquipmentSlot, "auto">;
type EffectCategoryFilter = "all" | "constant" | "timed" | "alchemy" | "arcane" | "combat" | "harmful";

const LOADOUT_SLOT_META: ReadonlyArray<{ id: ConcreteEquipmentSlot; label: string; biped?: string }> = [
  { id: "head", label: "Head" },
  { id: "body", label: "Body" },
  { id: "hands", label: "Hands" },
  { id: "feet", label: "Feet" },
  { id: "neck", label: "Neck" },
  { id: "ring", label: "Ring" },
  { id: "mainHand", label: "Right Hand" },
  { id: "offHand", label: "Left Hand" },
  { id: "ammo", label: "Ammo" },
  { id: "cape", label: "Cape", biped: "46" },
  { id: "lantern", label: "Lantern", biped: "49" },
] as const;

const LOADOUT_SLOTS: readonly ConcreteEquipmentSlot[] = LOADOUT_SLOT_META.map((s) => s.id);

interface SavedLoadoutSlot {
  id: string;
  name: string;
  category: string;
}

export interface SavedCharacterLoadout {
  id: string;
  name: string;
  characterName: string;
  createdAt: string;
  slots: Partial<Record<ConcreteEquipmentSlot, SavedLoadoutSlot | null>>;
}

const LOADOUT_STORAGE_KEY = "skyultra.character.loadouts.v1";

function loadSavedLoadouts(characterName?: string | null): SavedCharacterLoadout[] {
  try {
    const store = readLocalSkyUltraProfileStore();
    const resolved = resolveEffectiveSkyUltraProfile(store, characterName);
    if (resolved.loadouts.length > 0) {
      return resolved.loadouts as unknown as SavedCharacterLoadout[];
    }
    const raw = window.localStorage.getItem(LOADOUT_STORAGE_KEY);
    if (!raw) return [];
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}

function persistSavedLoadouts(
  list: SavedCharacterLoadout[],
  bridge?: SkyWebViewAPI,
  characterName?: string | null,
): void {
  const capped = list.slice(0, 24);
  try {
    window.localStorage.setItem(LOADOUT_STORAGE_KEY, JSON.stringify(capped));
  } catch {
    // Ignore storage quota errors
  }
  const nextStore = updateProfileForContext(readLocalSkyUltraProfileStore(), characterName, {
    loadouts: capped as unknown as SkyUltraSavedLoadoutRecord[],
  });
  if (bridge) {
    void persistSkyUltraProfileStore(nextStore, bridge, characterName);
  }
}

function skillTier(val: number): { label: string; tone: string; nextAt: number } {
  if (val >= 105) return { label: "Legendary", tone: "legendary", nextAt: 150 };
  if (val >= 100) return { label: "Master", tone: "master", nextAt: 105 };
  if (val >= 75) return { label: "Expert", tone: "expert", nextAt: 100 };
  if (val >= 50) return { label: "Adept", tone: "adept", nextAt: 75 };
  if (val >= 25) return { label: "Apprentice", tone: "apprentice", nextAt: 50 };
  return { label: "Novice", tone: "novice", nextAt: 25 };
}

function deriveArchetype(player: Player | null): { title: string; subtitle: string; dominant: string } {
  const skills = player?.skills ?? [];
  if (!skills.length) {
    return { title: "Sovereign Dragonborn", subtitle: "Balanced Adventurer", dominant: "Combat" };
  }
  const totals: Record<string, number> = { Combat: 0, Magic: 0, Stealth: 0 };
  for (const s of skills) {
    totals[s.group] = (totals[s.group] ?? 0) + (Number.isFinite(s.value) ? s.value : 0);
  }
  const sortedGroups = Object.entries(totals).sort((a, b) => b[1] - a[1]);
  const topGroup = sortedGroups[0]?.[0] ?? "Combat";
  const secondGroup = sortedGroups[1]?.[0] ?? "Magic";
  const ratio = (sortedGroups[1]?.[1] ?? 0) / Math.max(1, sortedGroups[0]?.[1] ?? 1);

  if (ratio > 0.86) {
    if (topGroup === "Combat" && secondGroup === "Magic")
      return { title: "Spellsword Paragon", subtitle: "Hybrid Martial & Arcane Savant", dominant: topGroup };
    if (topGroup === "Magic" && secondGroup === "Stealth")
      return { title: "Arcane Nightblade", subtitle: "Illusion & Shadow Specialist", dominant: topGroup };
    if (topGroup === "Combat" && secondGroup === "Stealth")
      return { title: "Shadow Ranger", subtitle: "Precision & Vanguard Tactician", dominant: topGroup };
  }
  if (topGroup === "Magic") return { title: "Arch-Mage Sovereign", subtitle: "Master of the Arcane Arts", dominant: "Magic" };
  if (topGroup === "Stealth") return { title: "Nightingale Phantom", subtitle: "Master of Stealth & Precision", dominant: "Stealth" };
  return { title: "Dragonscale Vanguard", subtitle: "Master of Arms & Armor", dominant: "Combat" };
}

export default function CharacterPage({ player, bridge }: { player: Player | null; bridge?: SkyWebViewAPI }): React.JSX.Element {
  const [tab, setTab] = useState<CharacterTab>("overview");
  const [effectFilter, setEffectFilter] = useState<EffectCategoryFilter>("all");
  const [effectViewMode, setEffectViewMode] = useState<"stacked" | "all">("stacked");
  const [effectQuery, setEffectQuery] = useState("");
  const [skillDiscipline, setSkillDiscipline] = useState<"All" | "Combat" | "Magic" | "Stealth">("All");
  const [skillQuery, setSkillQuery] = useState("");
  const [skillSort, setSkillSort] = useState<"value" | "name">("value");
  const [loadouts, setLoadouts] = useState<SavedCharacterLoadout[]>(() => loadSavedLoadouts(player?.playerName));

  useEffect(() => {
    const onProfileStore = (event: Event): void => {
      const detail = (event as CustomEvent<SkyUltraProfileStoreV2>).detail;
      if (!detail) return;
      const resolved = resolveEffectiveSkyUltraProfile(detail, player?.playerName);
      setLoadouts(resolved.loadouts as unknown as SavedCharacterLoadout[]);
    };
    window.addEventListener("skyultra:profile-store", onProfileStore);
    return () => window.removeEventListener("skyultra:profile-store", onProfileStore);
  }, [player?.playerName]);
  const [loadoutName, setLoadoutName] = useState("");
  const [stagedLoadoutId, setStagedLoadoutId] = useState<string | null>(null);
  const [undoSnapshot, setUndoSnapshot] = useState<Partial<Record<ConcreteEquipmentSlot, SavedLoadoutSlot | null>> | null>(null);
  const [loadoutStatus, setLoadoutStatus] = useState("");
  const [loadoutBusy, setLoadoutBusy] = useState(false);
  const [loadoutJsonText, setLoadoutJsonText] = useState("");
  const [renamingLoadoutId, setRenamingLoadoutId] = useState<string | null>(null);
  const [renameDraft, setRenameDraft] = useState("");

  const defenseMax = skyStatMaximum((player?.resistances ?? []).map((item) => item.value));
  const skillMax = skyStatMaximum((player?.skills ?? []).map((item) => item.value));

  const archetype = useMemo(() => deriveArchetype(player), [player]);

  const armorRating = Math.max(0, Math.round(player?.armorRating ?? 0));
  const physicalMitigation = Math.min(80, Math.round(armorRating * 0.12 * 10) / 10);
  const armorCapProgress = Math.min(100, Math.round((armorRating / 667) * 100));
  const maxHp = Math.max(1, Math.round(player?.health.max ?? 100));
  const effectiveHp = Math.round(maxHp / Math.max(0.2, 1 - physicalMitigation / 100));

  const skills = player?.skills ?? [];
  const topSkills = useMemo(() => [...skills].sort((a, b) => b.value - a.value).slice(0, 6), [skills]);

  const disciplineTotals = useMemo(() => {
    const groups = ["Combat", "Magic", "Stealth"] as const;
    return groups.map((g) => {
      const list = skills.filter((s) => s.group === g);
      const sum = list.reduce((acc, s) => acc + (Number.isFinite(s.value) ? s.value : 0), 0);
      const avg = list.length ? Math.round(sum / list.length) : 0;
      const best = [...list].sort((a, b) => b.value - a.value)[0];
      return { group: g, avg, sum: Math.round(sum), best, count: list.length };
    });
  }, [skills]);

  const filteredSkills = useMemo(() => {
    const q = skillQuery.trim().toLowerCase();
    return skills
      .filter((s) => (skillDiscipline === "All" || s.group === skillDiscipline) && (!q || s.label.toLowerCase().includes(q)))
      .sort((a, b) => (skillSort === "value" ? b.value - a.value : a.label.localeCompare(b.label)));
  }, [skills, skillDiscipline, skillQuery, skillSort]);

  const allEffects = player?.activeEffects ?? [];
  const timedCount = allEffects.filter((e) => e.duration > 0 && !e.hostile).length;
  const constantCount = allEffects.filter((e) => e.duration <= 0 && !e.hostile).length;
  const hostileCount = allEffects.filter((e) => e.hostile).length;

  const filteredEffects = useMemo(() => {
    const q = effectQuery.trim().toLowerCase();
    return allEffects.filter((effect) => {
      const meta = classifyEffectVisual(effect.name, effect.hostile, effect.duration);
      if (q && !effect.name.toLowerCase().includes(q) && !meta.schoolLabel.toLowerCase().includes(q)) return false;
      if (effectFilter === "harmful") return effect.hostile || meta.category === "harmful";
      if (effectFilter === "timed") return effect.duration > 0;
      if (effectFilter === "constant") return effect.duration <= 0;
      if (effectFilter === "alchemy") return meta.category === "alchemy";
      if (effectFilter === "arcane") return meta.category === "arcane" || meta.category === "divine";
      if (effectFilter === "combat") return meta.category === "combat" || meta.category === "stealth";
      return true;
    });
  }, [allEffects, effectFilter, effectQuery]);

  const stackedEffects = useMemo(() => {
    const groups = new Map<
      string,
      {
        id: string;
        name: string;
        hostile: boolean;
        duration: number;
        remaining: number;
        totalMagnitude: number;
        count: number;
        magnitudes: number[];
      }
    >();
    for (const eff of filteredEffects) {
      const key = `${eff.name.trim().toLowerCase()}::${eff.hostile ? "1" : "0"}::${eff.duration > 0 ? "t" : "c"}`;
      const existing = groups.get(key);
      if (existing) {
        existing.totalMagnitude += eff.magnitude;
        existing.count += 1;
        existing.magnitudes.push(eff.magnitude);
        if (eff.duration > existing.duration) existing.duration = eff.duration;
        if (eff.remaining > existing.remaining) existing.remaining = eff.remaining;
      } else {
        groups.set(key, {
          id: eff.id,
          name: eff.name,
          hostile: eff.hostile,
          duration: eff.duration,
          remaining: eff.remaining,
          totalMagnitude: eff.magnitude,
          count: 1,
          magnitudes: [eff.magnitude],
        });
      }
    }
    return Array.from(groups.values()).sort((a, b) => b.totalMagnitude - a.totalMagnitude || a.name.localeCompare(b.name));
  }, [filteredEffects]);

  const peakEffect = useMemo(() => {
    if (!stackedEffects.length) return null;
    return stackedEffects[0] ?? null;
  }, [stackedEffects]);

  const [equipmentOverride, setEquipmentOverride] = useState<Player["equipment"] | null>(null);
  useEffect(() => {
    setEquipmentOverride(null);
  }, [player?.equipment]);
  const liveEquipment = equipmentOverride ?? player?.equipment;

  const equippedCount = useMemo(() => {
    if (!liveEquipment) return 0;
    return Object.values(liveEquipment).filter(Boolean).length;
  }, [liveEquipment]);

  const itemLookup = useMemo(() => {
    const map = new Map<string, Item>();
    for (const item of player?.inventory ?? []) {
      map.set(item.id, item);
    }
    for (const slot of LOADOUT_SLOTS) {
      const eq = liveEquipment?.[slot];
      if (eq) map.set(eq.id, eq);
    }
    return map;
  }, [player?.inventory, liveEquipment]);

  const resolveSavedSlotItem = (slot: SavedLoadoutSlot | null | undefined): Item | null => {
    if (!slot) return null;
    const found = itemLookup.get(slot.id);
    if (found) return found;
    return {
      id: slot.id,
      name: slot.name,
      category: slot.category || "armor",
      count: 1,
      value: 0,
      weight: 0,
      equipped: false,
    };
  };

  const currentEquipmentSlots = useMemo(() => {
    const out: Partial<Record<ConcreteEquipmentSlot, SavedLoadoutSlot | null>> = {};
    for (const slot of LOADOUT_SLOTS) {
      const item: Item | null | undefined = liveEquipment?.[slot];
      out[slot] = item ? { id: item.id, name: item.name, category: item.category } : null;
    }
    return out;
  }, [liveEquipment]);

  const stagedLoadout = useMemo(
    () => loadouts.find((entry) => entry.id === stagedLoadoutId) ?? loadouts[0] ?? null,
    [loadouts, stagedLoadoutId],
  );

  const stagingSummary = useMemo(() => {
    let matches = 0;
    let toEquip = 0;
    let toUnequip = 0;
    for (const slot of LOADOUT_SLOTS) {
      const liveItem = currentEquipmentSlots[slot] ?? null;
      const stagedItem = stagedLoadout ? stagedLoadout.slots[slot] ?? null : liveItem;
      if ((liveItem?.id ?? null) === (stagedItem?.id ?? null)) {
        matches++;
      } else if (stagedItem) {
        toEquip++;
      } else {
        toUnequip++;
      }
    }
    return { matches, toEquip, toUnequip, totalChanges: toEquip + toUnequip };
  }, [currentEquipmentSlots, stagedLoadout]);

  const saveCurrentLoadout = (): void => {
    const title = loadoutName.trim() || `${player?.playerName ?? "Dragonborn"} Set #${loadouts.length + 1}`;
    const entry: SavedCharacterLoadout = {
      id: `loadout-${Date.now()}`,
      name: title,
      characterName: player?.playerName ?? "Dragonborn",
      createdAt: new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }),
      slots: { ...currentEquipmentSlots },
    };
    const next = [entry, ...loadouts].slice(0, 24);
    setLoadouts(next);
    persistSavedLoadouts(next, bridge, player?.playerName);
    setStagedLoadoutId(entry.id);
    setLoadoutName("");
    setLoadoutStatus(`Saved loadout "${entry.name}" (${equippedCount}/11 slots).`);
  };

  const deleteLoadout = (id: string): void => {
    const next = loadouts.filter((entry) => entry.id !== id);
    setLoadouts(next);
    persistSavedLoadouts(next, bridge, player?.playerName);
    if (stagedLoadoutId === id) {
      setStagedLoadoutId(next[0]?.id ?? null);
    }
    if (renamingLoadoutId === id) {
      setRenamingLoadoutId(null);
      setRenameDraft("");
    }
    setLoadoutStatus("Removed saved loadout.");
  };

  const startRenameLoadout = (entry: SavedCharacterLoadout): void => {
    setRenamingLoadoutId(entry.id);
    setRenameDraft(entry.name);
  };

  const commitRenameLoadout = (id: string): void => {
    const trimmed = renameDraft.trim().slice(0, 64);
    if (!trimmed) {
      setRenamingLoadoutId(null);
      setRenameDraft("");
      return;
    }
    const next = loadouts.map((entry) => (entry.id === id ? { ...entry, name: trimmed } : entry));
    setLoadouts(next);
    persistSavedLoadouts(next, bridge, player?.playerName);
    setRenamingLoadoutId(null);
    setRenameDraft("");
    setLoadoutStatus(`Renamed loadout to "${trimmed}".`);
  };

  const applySingleSlot = async (slot: ConcreteEquipmentSlot, desired: SavedLoadoutSlot | null, current: SavedLoadoutSlot | null): Promise<void> => {
    if (!bridge || loadoutBusy) return;
    const game = createSkyUltraGame(bridge);
    setLoadoutBusy(true);
    setUndoSnapshot({ ...currentEquipmentSlots });
    try {
      if (desired) {
        const res = await game.equipItem(desired.id, slot);
        if (res.equipment) {
          setEquipmentOverride(res.equipment as Player["equipment"]);
        }
        setLoadoutStatus(res.accepted ? `Equipped ${desired.name} in ${slot}.` : res.message ?? `Could not equip ${desired.name}.`);
      } else if (current) {
        const res = await game.unequipItem(current.id, slot);
        if (res.equipment) {
          setEquipmentOverride(res.equipment as Player["equipment"]);
        }
        setLoadoutStatus(res.accepted ? `Unequipped ${current.name} from ${slot}.` : res.message ?? `Could not unequip ${current.name}.`);
      }
    } catch {
      setLoadoutStatus(`Could not update ${slot} slot.`);
    } finally {
      setLoadoutBusy(false);
    }
  };

  const applyEquipmentMap = async (
    targetSlots: Partial<Record<ConcreteEquipmentSlot, SavedLoadoutSlot | null>>,
    label: string,
    recordUndo: boolean,
  ): Promise<void> => {
    if (!bridge || loadoutBusy) return;
    const game = createSkyUltraGame(bridge);
    setLoadoutBusy(true);
    if (recordUndo) {
      setUndoSnapshot({ ...currentEquipmentSlots });
    }
    let changed = 0;
    let lastError = "";
    const workingSlots: Partial<Record<ConcreteEquipmentSlot, SavedLoadoutSlot | null>> = { ...currentEquipmentSlots };
    try {
      for (const slot of LOADOUT_SLOTS) {
        const desired = targetSlots[slot] ?? null;
        const current = workingSlots[slot] ?? null;
        if ((desired?.id ?? null) === (current?.id ?? null)) continue;
        if (desired) {
          const res = await game.equipItem(desired.id, slot);
          if (res.equipment) {
            const nextEq = res.equipment as Player["equipment"];
            setEquipmentOverride(nextEq);
            for (const s of LOADOUT_SLOTS) {
              const item = nextEq?.[s];
              workingSlots[s] = item ? { id: item.id, name: item.name, category: item.category } : null;
            }
          }
          if (res.accepted) changed++;
          else if (res.message) lastError = res.message;
        } else if (current) {
          const res = await game.unequipItem(current.id, slot);
          if (res.equipment) {
            const nextEq = res.equipment as Player["equipment"];
            setEquipmentOverride(nextEq);
            for (const s of LOADOUT_SLOTS) {
              const item = nextEq?.[s];
              workingSlots[s] = item ? { id: item.id, name: item.name, category: item.category } : null;
            }
          }
          if (res.accepted) changed++;
          else if (res.message) lastError = res.message;
        }
      }
      setLoadoutStatus(
        changed > 0
          ? `Applied "${label}" (${changed} slot change${changed === 1 ? "" : "s"} synchronized).`
          : lastError || `"${label}" already matches live gear.`,
      );
    } catch {
      setLoadoutStatus(`Could not complete "${label}" equipment sync.`);
    } finally {
      setLoadoutBusy(false);
    }
  };

  const exportLoadoutsJson = (): void => {
    setLoadoutJsonText(JSON.stringify(loadouts, null, 2));
    setLoadoutStatus(`Exported ${loadouts.length} saved loadout(s) to JSON.`);
  };

  const importLoadoutsJson = (): void => {
    try {
      const parsed = JSON.parse(loadoutJsonText);
      if (!Array.isArray(parsed)) throw new Error("Expected JSON array");
      const valid = parsed.filter((item) => item && typeof item.name === "string" && item.slots && typeof item.slots === "object");
      setLoadouts(valid);
      persistSavedLoadouts(valid, bridge, player?.playerName);
      setStagedLoadoutId(valid[0]?.id ?? null);
      setLoadoutStatus(`Imported ${valid.length} loadout preset(s).`);
    } catch {
      setLoadoutStatus("Invalid loadout JSON format.");
    }
  };

  return (
    <div className="page character-page">
      <section className="char-hero-banner">
        <div className="char-hero-banner__identity">
          <PlayerPortrait
            player={player}
            bridge={bridge}
            size="lg"
            badge={`LVL ${player?.level ?? 1}`}
            className="char-hero-banner__portrait"
          />
          <div className="char-hero-banner__copy">
            <div className="char-hero-banner__tags">
              <span className="char-tag char-tag--accent">{archetype.title}</span>
              <span className="char-tag">LVL {player?.level ?? 1}</span>
              <span className="char-tag">{player?.location || "Skyrim"}</span>
            </div>
            <h1>
              {player?.playerName ?? "Dragonborn"}
              <span className="title-period">.</span>
            </h1>
            <p>{archetype.subtitle} · Real-time combat mitigation, skill constellations, and active arcane influences.</p>
          </div>
        </div>

        <div className="char-hero-banner__kpis">
          <div className="char-kpi-tile">
            <small>PHYSICAL MITIGATION</small>
            <strong>{physicalMitigation.toFixed(0)}%</strong>
            <span>{armorRating.toLocaleString()} Armor Rating</span>
          </div>
          <div className="char-kpi-tile">
            <small>EFFECTIVE HEALTH (EHP)</small>
            <strong>{effectiveHp.toLocaleString()}</strong>
            <span>{Math.round(player?.health.current ?? 0).toLocaleString()} Base HP</span>
          </div>
          <div className="char-kpi-tile">
            <small>EQUIPPED LOADOUT</small>
            <strong>{equippedCount} / 11</strong>
            <span>Active Gear Slots</span>
          </div>
          <div className="char-kpi-tile">
            <small>ACTIVE INFLUENCES</small>
            <strong>{(player?.activeEffectCount ?? allEffects.length).toLocaleString()}</strong>
            <span>{hostileCount > 0 ? `${hostileCount} Hostile` : "All Beneficial"}</span>
          </div>
        </div>
      </section>

      <nav className="char-tab-rail" role="tablist" aria-label="Character sheet sections">
        {(
          [
            { id: "overview", label: "Overview & Build", sub: "Archetype, Vitals & Top Masteries", icon: "actors" },
            { id: "defenses", label: "Defenses & Wards", sub: `${physicalMitigation.toFixed(0)}% Mitigation & Resistances`, icon: "shield" },
            { id: "skills", label: "Skill Constellations", sub: `${skills.length || 18} Disciplines & Ranks`, icon: "book" },
            {
              id: "effects",
              label: "Active Magic & Buffs",
              sub: `${player?.activeEffectCount ?? allEffects.length} Spell & Gear Effects`,
              icon: "rune",
            },
            {
              id: "loadouts",
              label: "Loadouts & Staging",
              sub: `${loadouts.length} Saved Outfit Presets`,
              icon: "inventory",
            },
          ] as const
        ).map((item) => (
          <button
            key={item.id}
            type="button"
            role="tab"
            aria-selected={tab === item.id}
            className={"char-tab-card" + (tab === item.id ? " is-active" : "")}
            onClick={() => setTab(item.id)}
          >
            <span className="char-tab-card__icon">
              <Icon name={item.icon} size={18} />
            </span>
            <span className="char-tab-card__text">
              <strong>{item.label}</strong>
              <small>{item.sub}</small>
            </span>
          </button>
        ))}
      </nav>

      {tab === "overview" && (
        <div className="char-overview-grid">
          <section className="surface char-vitals-surface">
            <SectionTitle eyebrow="PRIMARY ATTRIBUTE POOLS" title="Vitality, Magicka & Stamina Resonance">
              <span className="quiet-pill">LIVE SNAPSHOT</span>
            </SectionTitle>
            <div className="character-rings">
              <SkyRingMeter
                compact
                label="Health"
                tone="danger"
                value={player?.health.current ?? 0}
                max={player?.health.max ?? 0}
                detail={`EHP ${effectiveHp.toLocaleString()}`}
              />
              <SkyRingMeter
                compact
                label="Magicka"
                value={player?.magicka.current ?? 0}
                max={player?.magicka.max ?? 0}
                detail="Arcane Pool"
              />
              <SkyRingMeter
                compact
                label="Stamina"
                tone="success"
                value={player?.stamina.current ?? 0}
                max={player?.stamina.max ?? 0}
                detail={`${Math.round(player?.carryWeight?.max ?? 300)} Max Carry`}
              />
            </div>

            <div className="char-combat-breakdown">
              <div className="char-breakdown-card">
                <div className="char-breakdown-card__head">
                  <span>SKYRIM ARMOR CAP PROGRESSION (667 CAP = 80% DR)</span>
                  <strong>{armorCapProgress}% OF CAP</strong>
                </div>
                <div className="char-progress-track">
                  <i style={{ width: `${armorCapProgress}%` }} />
                </div>
                <small>
                  {armorRating >= 667
                    ? "Maximum 80% physical damage mitigation achieved."
                    : `${Math.max(0, 667 - armorRating)} more Armor Rating needed for the 80% physical mitigation cap.`}
                </small>
              </div>

              <div className="char-discipline-split">
                {disciplineTotals.map((d) => (
                  <div key={d.group} className="char-discipline-mini" data-group={d.group.toLowerCase()}>
                    <span>{d.group.toUpperCase()}</span>
                    <strong>AVG {d.avg}</strong>
                    <small>{d.best ? `Peak: ${d.best.label} (${Math.round(d.best.value)})` : "6 Skills"}</small>
                  </div>
                ))}
              </div>
            </div>
          </section>

          <section className="surface char-top-skills-surface">
            <SectionTitle eyebrow="SIGNATURE PROFICIENCIES" title="Highest Mastered Skills">
              <button type="button" className="control-button" onClick={() => setTab("skills")}>
                All 18 Skills
              </button>
            </SectionTitle>
            {topSkills.length > 0 ? (
              <div className="char-signature-skills">
                {topSkills.map((s, idx) => {
                  const tier = skillTier(s.value);
                  const pct = Math.max(0, Math.min(100, (s.value / Math.max(100, skillMax)) * 100));
                  return (
                    <article key={s.id} className="char-sig-skill-card" data-group={s.group.toLowerCase()}>
                      <div className="char-sig-skill-card__top">
                        <span className="char-sig-skill-card__rank">#{idx + 1}</span>
                        <span className="char-sig-skill-card__icon" aria-hidden="true">
                          <SkillIcon skill={s.id || s.label} size={18} />
                        </span>
                        <div>
                          <strong>{s.label}</strong>
                          <small>{s.group} Constellation</small>
                        </div>
                        <span className={"char-tier-badge is-" + tier.tone}>{tier.label}</span>
                        <b className="char-sig-skill-card__val">{Math.round(s.value)}</b>
                      </div>
                      <div className="char-progress-track">
                        <i style={{ width: `${pct}%` }} />
                      </div>
                    </article>
                  );
                })}
              </div>
            ) : (
              <Empty title="Awaiting skill telemetry" detail="Connect to Skyrim to inspect your signature skills." icon="book" />
            )}
          </section>
        </div>
      )}

      {tab === "defenses" && (
        <div className="character-columns">
          <section className="surface">
            <SectionTitle eyebrow="ELEMENTAL & ARCANE WARDS" title="Resistance Matrix & Radar">
              <span className="quiet-pill">{Math.round(player?.armorRating ?? 0).toLocaleString()} ARMOR</span>
            </SectionTitle>
            {player?.resistances?.length ? (
              <>
                <div className="defense-display">
                  <SkyRadarChart label="Character resistance percentages" metrics={player.resistances} max={defenseMax} />
                  <div className="defense-chart">
                    {player.resistances.map((resistance) => (
                      <SkyStatBar
                        key={resistance.id}
                        label={resistance.label}
                        value={resistance.value}
                        maximum={defenseMax}
                        unit="%"
                      />
                    ))}
                  </div>
                </div>
                <p className="inspector-note">
                  Shared linear scale: 0–{defenseMax.toLocaleString()}%. Skyrim caps standard elemental & magic resistance at 85%.
                </p>
              </>
            ) : (
              <Empty title="Defense data unavailable" detail="Waiting for the native character snapshot." icon="shield" />
            )}
          </section>

          <section className="surface">
            <SectionTitle eyebrow="WARD BREAKDOWN" title="Elemental Cap & Vulnerability Audit" />
            {player?.resistances?.length ? (
              <div className="char-ward-cards">
                {player.resistances.map((res) => {
                  const val = Math.round(res.value);
                  const status =
                    val >= 85 ? "CAPPED (85%+)" : val >= 50 ? "FORTIFIED" : val > 0 ? "WARDED" : val < 0 ? "VULNERABLE" : "UNWARDED";
                  const pct = Math.max(0, Math.min(100, Math.round((Math.max(0, val) / 85) * 100)));
                  return (
                    <article
                      key={res.id}
                      className={"char-ward-card" + (val < 0 ? " is-vulnerable" : val >= 85 ? " is-capped" : "")}
                    >
                      <div className="char-ward-card__head">
                        <strong>{res.label}</strong>
                        <span className="char-ward-card__status">{status}</span>
                        <b>{val}%</b>
                      </div>
                      <div className="char-progress-track">
                        <i style={{ width: `${pct}%` }} />
                      </div>
                    </article>
                  );
                })}
              </div>
            ) : (
              <Empty title="No ward data" detail="Connect to Skyrim to inspect elemental wards." icon="shield" />
            )}
          </section>
        </div>
      )}

      {tab === "skills" && (
        <div className="char-skills-workspace">
          <div className="char-skills-toolbar surface">
            <label className="page-search char-skills-search">
              <Icon name="search" size={16} />
              <input
                aria-label="Filter skills by name"
                placeholder="Search 18 skills (e.g. One-Handed, Destruction, Sneak)…"
                value={skillQuery}
                onChange={(e) => setSkillQuery(e.currentTarget.value)}
              />
              {skillQuery && (
                <button type="button" className="search-clear-btn" onClick={() => setSkillQuery("")} aria-label="Clear search">
                  <Icon name="close" size={12} />
                </button>
              )}
            </label>
            <div className="char-skills-filters">
              {(["All", "Combat", "Magic", "Stealth"] as const).map((grp) => (
                <button
                  key={grp}
                  type="button"
                  className={"hud-category-pill" + (skillDiscipline === grp ? " is-active" : "")}
                  onClick={() => setSkillDiscipline(grp)}
                >
                  {grp === "All" ? "All 18 Skills" : grp}
                </button>
              ))}
              <button
                type="button"
                className="control-button"
                onClick={() => setSkillSort((s) => (s === "value" ? "name" : "value"))}
              >
                Sort: {skillSort === "value" ? "Highest Level" : "A–Z"}
              </button>
            </div>
          </div>

          {filteredSkills.length ? (
            <div className="char-constellation-grid">
              {filteredSkills.map((skill) => {
                const tier = skillTier(skill.value);
                const pct = Math.max(0, Math.min(100, (skill.value / Math.max(100, skillMax)) * 100));
                return (
                  <article key={skill.id} className="char-skill-card surface" data-group={skill.group.toLowerCase()}>
                    <div className="char-skill-card__top">
                      <div className="char-skill-card__identity">
                        <span className="char-skill-card__icon" aria-hidden="true">
                          <SkillIcon skill={skill.id || skill.label} size={22} />
                        </span>
                        <div>
                          <span className="char-skill-card__group">{skill.group.toUpperCase()}</span>
                          <h3>{skill.label}</h3>
                        </div>
                      </div>
                      <div className="char-skill-card__right">
                        <strong>{skill.value.toFixed(0)}</strong>
                        <span className={"char-tier-badge is-" + tier.tone}>{tier.label}</span>
                      </div>
                    </div>
                    <div className="char-progress-track">
                      <i style={{ width: `${pct}%` }} />
                    </div>
                    <div className="char-skill-card__foot">
                      <span>Mastery Rank: {tier.label}</span>
                      <small>{skill.value < 100 ? `${Math.max(0, Math.ceil(tier.nextAt - skill.value))} pts to next tier` : "Peak Mastery"}</small>
                    </div>
                  </article>
                );
              })}
            </div>
          ) : (
            <section className="surface">
              <Empty title="No matching skills" detail="Try clearing your search filter or choosing another discipline." icon="book" />
            </section>
          )}
        </div>
      )}

      {tab === "effects" && (
        <div className="char-effects-workspace">
          <div className="char-effects-kpi-strip">
            <div className="char-effect-kpi">
              <small>ACTIVE INFLUENCES</small>
              <strong>{(player?.activeEffectCount ?? allEffects.length).toLocaleString()}</strong>
              <span>{stackedEffects.length} Distinct Archetypes</span>
            </div>
            <div className="char-effect-kpi">
              <small>CONSTANT ENCHANTMENTS</small>
              <strong>{constantCount.toLocaleString()}</strong>
              <span>Worn Gear & Standing Stones</span>
            </div>
            <div className="char-effect-kpi">
              <small>TIMED SPELLS & ELIXIRS</small>
              <strong>{timedCount.toLocaleString()}</strong>
              <span>{hostileCount > 0 ? `${hostileCount} Hostile Active` : "Zero Afflictions"}</span>
            </div>
            <div className="char-effect-kpi">
              <small>DOMINANT RESONANCE</small>
              <strong>
                {peakEffect ? `${peakEffect.totalMagnitude > 0 ? "+" : ""}${Math.round(peakEffect.totalMagnitude).toLocaleString()}` : "—"}
              </strong>
              <span>{peakEffect ? peakEffect.name : "No Active Magic"}</span>
            </div>
          </div>

          <section className="surface effects-surface">
            <SectionTitle eyebrow="CURRENT ARCANE & ALCHEMICAL INFLUENCES" title="Active Effects & Enchantments">
              <div className="char-effects-mode-switch" role="group" aria-label="Effect grouping mode">
                <button
                  type="button"
                  className={"char-stage-btn" + (effectViewMode === "stacked" ? " char-stage-btn--active" : "")}
                  onClick={() => setEffectViewMode("stacked")}
                >
                  Stacked Resonance ({stackedEffects.length})
                </button>
                <button
                  type="button"
                  className={"char-stage-btn" + (effectViewMode === "all" ? " char-stage-btn--active" : "")}
                  onClick={() => setEffectViewMode("all")}
                >
                  All Instances ({filteredEffects.length})
                </button>
              </div>
            </SectionTitle>

            <div className="char-effects-toolbar">
              <label className="page-search char-effects-search">
                <Icon name="search" size={16} />
                <input
                  aria-label="Search active effects"
                  placeholder="Filter active effects, blessings, schools, or enchantments…"
                  value={effectQuery}
                  onChange={(e) => setEffectQuery(e.currentTarget.value)}
                />
                {effectQuery && (
                  <button type="button" className="search-clear-btn" onClick={() => setEffectQuery("")} aria-label="Clear search">
                    <Icon name="close" size={12} />
                  </button>
                )}
              </label>

              <div className="char-effects-filter-pills" role="group" aria-label="Effect category filter">
                {(
                  [
                    { id: "all", label: `All (${allEffects.length})` },
                    { id: "constant", label: `Constant (${constantCount})` },
                    { id: "timed", label: `Timed (${timedCount})` },
                    { id: "alchemy", label: "Alchemical" },
                    { id: "arcane", label: "Arcane & Divine" },
                    { id: "combat", label: "Martial & Shadow" },
                    { id: "harmful", label: `Harmful (${hostileCount})` },
                  ] as const
                ).map((pill) => (
                  <button
                    key={pill.id}
                    type="button"
                    className={"hud-category-pill" + (effectFilter === pill.id ? " is-active" : "")}
                    onClick={() => setEffectFilter(pill.id)}
                  >
                    {pill.label}
                  </button>
                ))}
              </div>
            </div>

            {effectViewMode === "stacked" ? (
              <div className="char-effects-grid">
                {stackedEffects.map((group) => {
                  const meta = classifyEffectVisual(group.name, group.hostile, group.duration);
                  const remPct =
                    group.duration > 0 ? Math.max(0, Math.min(100, Math.round((group.remaining / group.duration) * 100))) : 100;
                  const magCounts = new Map<string, number>();
                  for (const m of group.magnitudes) {
                    const fmt = `${m > 0 ? "+" : ""}${Number.isInteger(m) ? m.toString() : m.toFixed(1)}`;
                    magCounts.set(fmt, (magCounts.get(fmt) ?? 0) + 1);
                  }
                  const magEntries = Array.from(magCounts.entries());
                  const breakdown =
                    magEntries
                      .slice(0, 4)
                      .map(([val, cnt]) => (cnt > 1 ? `${cnt}× ${val}` : val))
                      .join(" · ") + (magEntries.length > 4 ? ` · +${magEntries.length - 4} more` : "");

                  return (
                    <article
                      key={group.id + "::stacked"}
                      className={"char-effect-card" + (group.hostile ? " is-harmful" : "")}
                      style={{ "--effect-tone": meta.accent } as React.CSSProperties}
                    >
                      <div className="char-effect-card__emblem" aria-hidden="true">
                        <EffectIcon name={group.name} hostile={group.hostile} duration={group.duration} size={24} />
                      </div>
                      <div className="char-effect-card__body">
                        <div className="char-effect-card__meta">
                          <span className="char-effect-card__school">{meta.schoolLabel}</span>
                          {group.count > 1 && <span className="char-effect-card__stack">×{group.count} STACKED</span>}
                          <span className="char-effect-card__state">
                            {group.duration > 0 ? `${Math.max(0, Math.ceil(group.remaining))}s LEFT` : "CONSTANT"}
                          </span>
                        </div>
                        <div className="char-effect-card__main">
                          <h3>{group.name}</h3>
                          <strong className="char-effect-card__mag">
                            {group.totalMagnitude > 0 ? "+" : ""}
                            {group.totalMagnitude >= 100
                              ? Math.round(group.totalMagnitude).toLocaleString()
                              : group.totalMagnitude.toFixed(1)}
                          </strong>
                        </div>
                        <div className="char-effect-card__foot">
                          <small>
                            {group.count > 1
                              ? `Combined from ${group.count} enchanted sources (${breakdown})`
                              : group.duration > 0
                                ? "Active timed spell or alchemical elixir"
                                : "Permanent gear enchantment or standing stone blessing"}
                          </small>
                        </div>
                        {group.duration > 0 && (
                          <div className="char-effect-card__bar">
                            <i style={{ width: `${remPct}%` }} />
                          </div>
                        )}
                      </div>
                    </article>
                  );
                })}
              </div>
            ) : (
              <div className="char-effects-grid">
                {filteredEffects.map((effect) => {
                  const meta = classifyEffectVisual(effect.name, effect.hostile, effect.duration);
                  const remPct =
                    effect.duration > 0 ? Math.max(0, Math.min(100, Math.round((effect.remaining / effect.duration) * 100))) : 100;
                  return (
                    <article
                      key={effect.id}
                      className={"char-effect-card" + (effect.hostile ? " is-harmful" : "")}
                      style={{ "--effect-tone": meta.accent } as React.CSSProperties}
                    >
                      <div className="char-effect-card__emblem" aria-hidden="true">
                        <EffectIcon name={effect.name} hostile={effect.hostile} duration={effect.duration} size={24} />
                      </div>
                      <div className="char-effect-card__body">
                        <div className="char-effect-card__meta">
                          <span className="char-effect-card__school">{meta.schoolLabel}</span>
                          <span className="char-effect-card__state">
                            {effect.duration > 0 ? `${Math.max(0, Math.ceil(effect.remaining))}s LEFT` : "CONSTANT"}
                          </span>
                        </div>
                        <div className="char-effect-card__main">
                          <h3>{effect.name}</h3>
                          <strong className="char-effect-card__mag">
                            {effect.magnitude > 0 ? "+" : ""}
                            {effect.magnitude >= 100 ? Math.round(effect.magnitude).toLocaleString() : effect.magnitude.toFixed(1)}
                          </strong>
                        </div>
                        <div className="char-effect-card__foot">
                          <small>
                            {effect.hostile
                              ? "Harmful affliction"
                              : effect.duration > 0
                                ? "Timed spell or potion instance"
                                : "Constant enchantment or blessing instance"}
                          </small>
                        </div>
                        {effect.duration > 0 && (
                          <div className="char-effect-card__bar">
                            <i style={{ width: `${remPct}%` }} />
                          </div>
                        )}
                      </div>
                    </article>
                  );
                })}
              </div>
            )}

            {!filteredEffects.length && (
              <Empty
                title="No matching effects"
                detail={
                  player?.activeEffects?.length
                    ? "Choose a different effect filter or clear your search."
                    : "Active magic appears here when effects are present on your character."
                }
                icon="rune"
              />
            )}
            {(player?.activeEffectCount ?? 0) > (player?.activeEffects?.length ?? 0) && (
              <p className="inspector-note">
                Showing {player?.activeEffects?.length ?? 0} named effects ({stackedEffects.length} distinct archetypes) from{" "}
                {player?.activeEffectCount} total active engine entries.
              </p>
            )}
          </section>
        </div>
      )}

      {tab === "loadouts" && (
        <div className="char-loadouts-studio">
          <div className="char-loadout-left-col">
            <section className="surface char-loadout-presets-surface">
              <SectionTitle eyebrow="PER-CHARACTER OUTFIT PROFILES" title="Saved Loadout Presets">
                <span className="quiet-pill">{loadouts.length} SAVED</span>
              </SectionTitle>

              <div className="char-loadout-capture">
                <label className="page-search">
                  <Icon name="inventory" size={16} />
                  <input
                    aria-label="New loadout preset name"
                    placeholder={`Name current ${equippedCount}/11 slot outfit (e.g. Battleplate, Stealth)…`}
                    value={loadoutName}
                    onChange={(e) => setLoadoutName(e.currentTarget.value)}
                  />
                </label>
                <button type="button" className="char-stage-btn char-stage-btn--primary" onClick={saveCurrentLoadout}>
                  <Icon name="plus" size={14} /> Save Current Outfit
                </button>
              </div>

              {loadouts.length > 0 ? (
                <div className="char-loadout-preset-list">
                  {loadouts.map((entry) => {
                    const slotEntries = LOADOUT_SLOT_META.map((meta) => ({
                      meta,
                      saved: entry.slots[meta.id] ?? null,
                    })).filter((x) => x.saved !== null);
                    const slotCount = slotEntries.length;
                    const active = stagedLoadout?.id === entry.id;
                    return (
                      <article key={entry.id} className={"char-loadout-preset-card" + (active ? " is-active" : "")}>
                        <div className="char-loadout-preset-card__top">
                          <div className="char-loadout-preset-card__title">
                            <div className="char-loadout-preset-card__badges">
                              <span className="char-preset-pill">{slotCount} / 11 SLOTS</span>
                              {active && <span className="char-preset-pill char-preset-pill--active">STAGED FOR DIFF</span>}
                            </div>
                            {renamingLoadoutId === entry.id ? (
                              <div className="char-loadout-rename-row">
                                <input
                                  className="char-loadout-rename-input"
                                  aria-label={`Rename loadout ${entry.name}`}
                                  value={renameDraft}
                                  autoFocus
                                  onChange={(e) => setRenameDraft(e.currentTarget.value)}
                                  onKeyDown={(e) => {
                                    if (e.key === "Enter") {
                                      e.preventDefault();
                                      commitRenameLoadout(entry.id);
                                    } else if (e.key === "Escape") {
                                      e.preventDefault();
                                      setRenamingLoadoutId(null);
                                      setRenameDraft("");
                                    }
                                  }}
                                />
                                <button
                                  type="button"
                                  className="char-stage-btn char-stage-btn--sm char-stage-btn--primary"
                                  onClick={() => commitRenameLoadout(entry.id)}
                                >
                                  Save
                                </button>
                                <button
                                  type="button"
                                  className="char-stage-btn char-stage-btn--sm"
                                  onClick={() => {
                                    setRenamingLoadoutId(null);
                                    setRenameDraft("");
                                  }}
                                >
                                  Cancel
                                </button>
                              </div>
                            ) : (
                              <strong>{entry.name}</strong>
                            )}
                            <small>
                              {entry.characterName} · Saved {entry.createdAt}
                            </small>
                          </div>
                          <div className="char-loadout-preset-card__actions">
                            <button
                              type="button"
                              className={"char-stage-btn" + (active ? " char-stage-btn--active" : "")}
                              onClick={() => setStagedLoadoutId(entry.id)}
                            >
                              {active ? "Staged" : "Stage Diff"}
                            </button>
                            <button
                              type="button"
                              className="char-stage-btn"
                              onClick={() => startRenameLoadout(entry)}
                            >
                              Rename
                            </button>
                            <button
                              type="button"
                              className="char-stage-btn char-stage-btn--primary"
                              disabled={!bridge || loadoutBusy}
                              onClick={() => void applyEquipmentMap(entry.slots, entry.name, true)}
                            >
                              Equip Set
                            </button>
                            <button
                              type="button"
                              className="char-stage-btn char-stage-btn--danger"
                              onClick={() => deleteLoadout(entry.id)}
                            >
                              Remove
                            </button>
                          </div>
                        </div>

                        {slotEntries.length > 0 && (
                          <div className="char-loadout-preset-card__strip">
                            {slotEntries.slice(0, 7).map(({ meta, saved }) => {
                              const resolved = resolveSavedSlotItem(saved);
                              return (
                                <span key={meta.id} className="char-preset-mini-item" title={`${meta.label}: ${saved?.name ?? ""}`}>
                                  <span className="char-preset-mini-item__art">
                                    {resolved ? (
                                      <ItemThumbnail item={resolved} bridge={bridge} />
                                    ) : (
                                      <Icon name={itemIcon(saved?.category ?? "armor")} size={14} />
                                    )}
                                  </span>
                                  <small>{saved?.name}</small>
                                </span>
                              );
                            })}
                            {slotEntries.length > 7 && (
                              <span className="char-preset-mini-more">+{slotEntries.length - 7} more</span>
                            )}
                          </div>
                        )}
                      </article>
                    );
                  })}
                </div>
              ) : (
                <Empty
                  title="No saved loadouts yet"
                  detail="Save your current 11-slot equipment above to stage diffs, switch outfits, or export profile JSON."
                  icon="inventory"
                />
              )}

              <div className="char-loadout-json">
                <div className="char-loadout-json__actions">
                  <button type="button" className="char-stage-btn" onClick={exportLoadoutsJson}>
                    Export JSON
                  </button>
                  <button
                    type="button"
                    className="char-stage-btn"
                    onClick={importLoadoutsJson}
                    disabled={!loadoutJsonText.trim()}
                  >
                    Import JSON
                  </button>
                  {undoSnapshot && (
                    <button
                      type="button"
                      className="char-stage-btn"
                      disabled={!bridge || loadoutBusy}
                      onClick={() => void applyEquipmentMap(undoSnapshot, "Undo Snapshot", false)}
                    >
                      <Icon name="rotate" size={13} /> Undo Last Outfit Change
                    </button>
                  )}
                </div>
                {loadoutJsonText && (
                  <textarea
                    className="char-loadout-textarea"
                    aria-label="Loadouts JSON exchange"
                    value={loadoutJsonText}
                    onChange={(e) => setLoadoutJsonText(e.currentTarget.value)}
                    rows={4}
                  />
                )}
                {loadoutStatus && <p className="char-loadout-status-banner">{loadoutStatus}</p>}
              </div>
            </section>

            <section className="surface char-live-gear-surface">
              <SectionTitle eyebrow="CURRENTLY WORN IN SKYRIM" title="Equipped Gear Snapshot">
                <span className="quiet-pill">
                  {equippedCount} / 11 EQUIPPED · {armorRating.toLocaleString()} ARMOR
                </span>
              </SectionTitle>
              <div className="char-live-equipped-grid">
                {LOADOUT_SLOT_META.map((slotMeta) => {
                  const liveItem = liveEquipment?.[slotMeta.id] ?? null;
                  return (
                    <article
                      key={slotMeta.id}
                      className={"char-live-slot-tile" + (liveItem ? " is-occupied" : " is-empty")}
                    >
                      <div className="char-live-slot-tile__art" aria-hidden="true">
                        {liveItem ? (
                          <ItemThumbnail item={liveItem} bridge={bridge} />
                        ) : (
                          <EquipmentGlyph slot={slotMeta.id} />
                        )}
                      </div>
                      <div className="char-live-slot-tile__copy">
                        <span className="char-live-slot-tile__slot">
                          {slotMeta.label}
                          {slotMeta.biped ? <em>#{slotMeta.biped}</em> : null}
                        </span>
                        <strong title={liveItem?.name ?? "Unequipped"}>
                          {liveItem ? liveItem.name : "Not Equipped"}
                        </strong>
                        <small>{liveItem ? `${formId(liveItem.id)} · ${liveItem.category}` : "Open Slot"}</small>
                      </div>
                    </article>
                  );
                })}
              </div>
            </section>
          </div>

          <section className="surface char-staging-matrix-surface">
            <SectionTitle
              eyebrow="11-SLOT VISUAL STAGING MATRIX"
              title={stagedLoadout ? `Staging Diff: ${stagedLoadout.name}` : "Live 11-Slot Equipment Matrix"}
            >
              <div className="char-staging-matrix-actions">
                <div className="char-staging-diff-pills">
                  <span className="char-diff-pill is-match">{stagingSummary.matches} Match</span>
                  {stagingSummary.toEquip > 0 && (
                    <span className="char-diff-pill is-equip">+{stagingSummary.toEquip} Equip</span>
                  )}
                  {stagingSummary.toUnequip > 0 && (
                    <span className="char-diff-pill is-unequip">−{stagingSummary.toUnequip} Unequip</span>
                  )}
                </div>
                {stagedLoadout && (
                  <button
                    type="button"
                    className="char-stage-btn char-stage-btn--primary"
                    disabled={!bridge || loadoutBusy}
                    onClick={() => void applyEquipmentMap(stagedLoadout.slots, stagedLoadout.name, true)}
                  >
                    <Icon name="check" size={14} /> Apply Staged Outfit
                  </button>
                )}
              </div>
            </SectionTitle>

            <div className="char-staging-slot-grid">
              {LOADOUT_SLOT_META.map((slotMeta) => {
                const slot = slotMeta.id;
                const liveSlot = currentEquipmentSlots[slot] ?? null;
                const stagedSlot = stagedLoadout ? stagedLoadout.slots[slot] ?? null : liveSlot;
                const changed = stagedLoadout ? (liveSlot?.id ?? null) !== (stagedSlot?.id ?? null) : false;
                const displayItem = resolveSavedSlotItem(stagedSlot ?? liveSlot);
                const statusLabel = !changed
                  ? stagedSlot
                    ? "MATCHES LIVE"
                    : "EMPTY SLOT"
                  : stagedSlot
                    ? "WILL EQUIP"
                    : "WILL UNEQUIP";
                const statusClass = !changed
                  ? stagedSlot
                    ? "is-synced"
                    : "is-empty"
                  : stagedSlot
                    ? "is-will-equip"
                    : "is-will-unequip";

                return (
                  <article key={slot} className={`char-staging-slot-card ${statusClass}`}>
                    <div className="char-staging-slot-card__art" aria-hidden="true">
                      {displayItem ? (
                        <ItemThumbnail item={displayItem} bridge={bridge} />
                      ) : (
                        <EquipmentGlyph slot={slot} />
                      )}
                    </div>

                    <div className="char-staging-slot-card__body">
                      <div className="char-staging-slot-card__top">
                        <span className="char-staging-slot-card__label">
                          {slotMeta.label}
                          {slotMeta.biped && <em>BIPED {slotMeta.biped}</em>}
                        </span>
                        <span className="char-staging-slot-card__badge">{statusLabel}</span>
                      </div>

                      <strong className="char-staging-slot-card__item" title={stagedSlot?.name ?? "Unequipped"}>
                        {stagedSlot ? stagedSlot.name : "Empty Slot"}
                      </strong>

                      <div className="char-staging-slot-card__meta">
                        <span>
                          Live: <b>{liveSlot ? `${liveSlot.name} (${formId(liveSlot.id)})` : "Unequipped"}</b>
                        </span>
                        {changed && stagedLoadout && (
                          <span className="char-staging-slot-card__delta">
                            Target: <b>{stagedSlot ? `${stagedSlot.name} (${formId(stagedSlot.id)})` : "Unequip Slot"}</b>
                          </span>
                        )}
                      </div>
                    </div>

                    {(changed || liveSlot) && (
                      <div className="char-staging-slot-card__Quick">
                        {changed ? (
                          <button
                            type="button"
                            className="char-stage-btn char-stage-btn--sm char-stage-btn--primary"
                            disabled={!bridge || loadoutBusy}
                            onClick={() => void applySingleSlot(slot, stagedSlot, liveSlot)}
                            title={stagedSlot ? `Equip ${stagedSlot.name}` : `Unequip ${liveSlot?.name ?? slotMeta.label}`}
                          >
                            {stagedSlot ? "Equip" : "Unequip"}
                          </button>
                        ) : liveSlot ? (
                          <button
                            type="button"
                            className="char-stage-btn char-stage-btn--sm"
                            disabled={!bridge || loadoutBusy}
                            onClick={() => void applySingleSlot(slot, null, liveSlot)}
                            title={`Unequip ${liveSlot.name}`}
                          >
                            Unequip
                          </button>
                        ) : null}
                      </div>
                    )}
                  </article>
                );
              })}
            </div>
          </section>
        </div>
      )}
    </div>
  );
}
