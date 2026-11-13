import * as React from "react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  Backpack,
  BookOpen,
  Check,
  ChevronDown,
  CircleHelp,
  Coins,
  Compass,
  Heart,
  MapPin,
  Map,
  Palette,
  Search,
  Settings2,
  Shield,
  Sparkles,
  Star,
  Swords,
  Weight,
  Users,
  X,
} from "@skyultra/ui";
import {
  useSkyUltraClient,
  useSkyUltraEvent,
  useSkyUltraRuntimeInfo,
} from "@skyultra/react";
import {
  SkyBadge,
  SkyButton,
  SkyDialogClose,
  SkyDialogContent,
  SkyDialogDescription,
  SkyDialogRoot,
  SkyDialogTitle,
  SkyDialogTrigger,
  SkyPanel,
  SkyPanelContent,
  SkyProgress,
  SkyThemeProvider,
  type SkyTheme,
} from "@skyultra/ui";
import { useWayfinderStore } from "./wayfinderStore";

interface CompanionProfile {
  name: string;
  title: string;
  race: string;
  role: string;
  level: number;
  location: string;
  portraitUrl?: string;
  health: number;
  healthMax: number;
  stamina: number;
  staminaMax: number;
  magicka: number;
  magickaMax: number;
}

interface Events {
  inventoryUpdated: { count: number };
  companionUpdated: Partial<CompanionProfile>;
}

type Category = "all" | "weapons" | "apparel" | "alchemy";
type LoadoutSlot = Exclude<Category, "all">;
type ArchivePage = "armory" | "party" | "chronicle" | "wayfinder";
type ChronicleKind = "gear" | "journey" | "party" | "quest";
type SortOrder = "recent" | "value" | "name" | "weight";
type Artwork = "sword" | "shield" | "potion" | "ring" | "bow";

interface ChronicleEntry {
  id: string;
  kind: ChronicleKind;
  time: string;
  title: string;
  detail: string;
  itemId?: string;
  page?: ArchivePage;
  live?: boolean;
}

interface EventToast {
  id: number;
  title: string;
  detail: string;
  page: ArchivePage;
}

interface SampleQuest {
  id: string;
  title: string;
  giver: string;
  region: string;
  reward: string;
  objectives: string[];
}
interface LoadoutPreset {
  id: string;
  name: string;
  itemIds: string[];
  starter?: boolean;
}
type Palette = "aurora" | "ember" | "glacier" | "verdant" | "aether" | "dwemer" | "sunspire";

interface InventoryItem {
  id: string;
  name: string;
  source: string;
  kind: string;
  category: Exclude<Category, "all">;
  weight: number;
  value: number;
  rating: string;
  description: string;
  artwork: Artwork;
  rarity: "COMMON" | "UNCOMMON" | "RARE" | "RELIC";
}

const SAMPLE_PORTRAIT = "./companion-portrait.svg";
const themeBase: Record<Palette, SkyTheme> = {
  aurora: "aurora",
  ember: "ember",
  glacier: "glacier",
  verdant: "verdant",
  aether: "aether",
  dwemer: "dwemer",
  sunspire: "sunspire",
};
const themes: { value: Palette; label: string; swatch: string }[] = [
  { value: "aurora", label: "Aurora", swatch: "#52e6d1" },
  { value: "ember", label: "Emberfall", swatch: "#ff765f" },
  { value: "glacier", label: "Glacier", swatch: "#70caff" },
  { value: "verdant", label: "Verdant", swatch: "#a5df67" },
  { value: "aether", label: "Aether", swatch: "#be8cff" },
  { value: "dwemer", label: "Dwemer", swatch: "#dfa75d" },
  { value: "sunspire", label: "Sunspire", swatch: "#ffb95c" },
];
const categories: { value: Category; label: string }[] = [
  { value: "all", label: "All gear" },
  { value: "weapons", label: "Weapons" },
  { value: "apparel", label: "Apparel" },
  { value: "alchemy", label: "Alchemy" },
];
const archivePages: { value: ArchivePage; label: string; icon: React.ReactNode }[] = [
  { value: "armory", label: "Armory", icon: <Backpack /> },
  { value: "party", label: "Party", icon: <Users /> },
  { value: "chronicle", label: "Chronicle", icon: <BookOpen /> },
  { value: "wayfinder", label: "Wayfinder", icon: <Map /> },
];
const inventory: InventoryItem[] = [
  { id: "wyrmsteel", name: "Wyrmsteel Greatsword", source: "Forged in Whiterun", kind: "TWO-HANDED", category: "weapons", weight: 18, value: 640, rating: "31 DMG", description: "A broad blade of pale steel, its fuller etched with the old dragon tongue.", artwork: "sword", rarity: "RELIC" },
  { id: "frostguard", name: "Frostguard Shield", source: "Nordic · Heavy armor", kind: "SHIELD", category: "apparel", weight: 14, value: 520, rating: "48 ARM", description: "Ironwood and hammered steel, scarred by a hundred northern winters.", artwork: "shield", rarity: "RARE" },
  { id: "moon-sugar", name: "Moon-Sugar Tonic", source: "Rare alchemical draught", kind: "POTION", category: "alchemy", weight: 0.5, value: 75, rating: "RESTORE", description: "A silvery distillate that catches the light like frost under a full moon.", artwork: "potion", rarity: "UNCOMMON" },
  { id: "aether-ring", name: "Aetherial Signet", source: "Dwemer relic · Jewelry", kind: "RING", category: "apparel", weight: 0.2, value: 1120, rating: "ENCHANTED", description: "A warm pulse travels through this impossible brasswork when held near a flame.", artwork: "ring", rarity: "RELIC" },
  { id: "glassbow", name: "Glenmoril Longbow", source: "Glass · Hunting bow", kind: "BOW", category: "weapons", weight: 12, value: 890, rating: "27 DMG", description: "Green glass limbs flex silently, balanced for a patient hunter.", artwork: "bow", rarity: "RARE" },
  { id: "dawnbreaker", name: "Dawn-Touched Edge", source: "A relic from the western barrow", kind: "SWORD", category: "weapons", weight: 10, value: 740, rating: "24 DMG", description: "A bright, sun-warmed blade that hums softly when the dead draw near.", artwork: "sword", rarity: "RARE" },
  { id: "nightward", name: "Nightward Buckler", source: "Leather · Light armor", kind: "SHIELD", category: "apparel", weight: 5, value: 260, rating: "21 ARM", description: "A compact guard stitched with a pattern of stars used by old scouts.", artwork: "shield", rarity: "UNCOMMON" },
  { id: "ember-elixir", name: "Emberheart Elixir", source: "Alchemist's reserve", kind: "POTION", category: "alchemy", weight: 0.5, value: 145, rating: "FORTIFY", description: "A glowing red cordial, warm enough to thaw a flask of winter air.", artwork: "potion", rarity: "UNCOMMON" },
];
const sampleLoadouts: LoadoutPreset[] = [
  { id: "wayfarer", name: "Wayfarer", itemIds: ["wyrmsteel", "frostguard", "moon-sugar"], starter: true },
  { id: "witchlight", name: "Witchlight", itemIds: ["aether-ring", "ember-elixir", "glassbow"], starter: true },
  { id: "nightglass", name: "Nightglass", itemIds: ["nightward", "glassbow", "moon-sugar"], starter: true },
];
const sampleQuests: SampleQuest[] = [
  { id: "ashbound", title: "Ashes over Whiterun", giver: "Proventus Avenicci", region: "Whiterun Hold", reward: "750 SEPTIMS", objectives: ["Speak with the Jarl's steward", "Find the ash mark at the western watchtower", "Return with proof of the attack"] },
  { id: "starfall", title: "A Star in the Snow", giver: "Faralda", region: "Winterhold", reward: "ENCHANTED FOCUS", objectives: ["Recover the observatory lens", "Follow the aurora to the frozen ridge", "Bring the lens back to the College"] },
];
const sampleCompanion: CompanionProfile = {
  name: "Mira Vey",
  title: "The Ashen Hunter",
  race: "Dunmer",
  role: "Ranger",
  level: 28,
  location: "Whiterun",
  portraitUrl: SAMPLE_PORTRAIT,
  health: 820,
  healthMax: 820,
  stamina: 314,
  staminaMax: 380,
  magicka: 96,
  magickaMax: 240,
};

function isSafeLocalAssetPath(path: string): boolean {
  const value = path.trim();
  return value.length > 0 && value.length <= 512 &&
    !value.startsWith("/") && !value.startsWith("\\") &&
    !value.includes(":") && !value.split(/[\\/]/).includes("..");
}

function mergeCompanion(current: CompanionProfile, update: Partial<CompanionProfile>): CompanionProfile {
  const next = { ...current };
  for (const field of ["name", "title", "race", "role", "location"] as const) {
    const value = update[field];
    if (typeof value === "string" && value.trim().length > 0 && value.length <= 80) {
      next[field] = value.trim();
    }
  }
  const nameChanged = typeof update.name === "string" && update.name !== current.name;
  if (update.portraitUrl !== undefined) {
    next.portraitUrl = typeof update.portraitUrl === "string" && isSafeLocalAssetPath(update.portraitUrl)
      ? update.portraitUrl.trim()
      : undefined;
  } else if (nameChanged) {
    next.portraitUrl = undefined;
  }
  for (const field of ["level", "health", "healthMax", "stamina", "staminaMax", "magicka", "magickaMax"] as const) {
    if (typeof update[field] === "number" && Number.isFinite(update[field])) {
      next[field] = Math.max(0, update[field]!);
    }
  }
  return next;
}

export function App(): React.JSX.Element {
  const sky = useSkyUltraClient<Events>();
  const runtime = useSkyUltraRuntimeInfo();
  const searchRef = useRef<HTMLInputElement>(null);
  const [page, setPage] = useState<ArchivePage>("armory");
  const [palette, setPalette] = useState<Palette>("aurora");
  const [category, setCategory] = useState<Category>("all");
  const [sortOrder, setSortOrder] = useState<SortOrder>("recent");
  const [query, setQuery] = useState("");
  const [selectedId, setSelectedId] = useState("wyrmsteel");
  const [itemCount, setItemCount] = useState(42);
  const [favorites, setFavorites] = useState<Set<string>>(() => new Set());
  const [favoriteOnly, setFavoriteOnly] = useState(false);
  const [equippedIds, setEquippedIds] = useState<Set<string>>(() => new Set(["wyrmsteel", "frostguard", "moon-sugar"]));
  const [loadouts, setLoadouts] = useState(sampleLoadouts);
  const [loadoutName, setLoadoutName] = useState("");
  const [activeLoadoutId, setActiveLoadoutId] = useState<string | null>("wayfarer");
  const [compareOpen, setCompareOpen] = useState(false);
  const [slotPicker, setSlotPicker] = useState<LoadoutSlot | null>(null);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [commandOpen, setCommandOpen] = useState(false);
  const [commandQuery, setCommandQuery] = useState("");
  const [companion, setCompanion] = useState(sampleCompanion);
  const [portraitFailed, setPortraitFailed] = useState(false);
  const [liveChronicles, setLiveChronicles] = useState<ChronicleEntry[]>([]);
  const [eventToast, setEventToast] = useState<EventToast | null>(null);
  const toastSequence = useRef(0);
  const trackedQuestId = useWayfinderStore((state) => state.trackedQuestId);
  const questProgress = useWayfinderStore((state) => state.questProgress);
  const mapLocation = useWayfinderStore((state) => state.mapLocation);
  const setTrackedQuestId = useWayfinderStore((state) => state.setTrackedQuestId);
  const setMapLocation = useWayfinderStore((state) => state.setMapLocation);
  const toggleObjective = useWayfinderStore((state) => state.toggleObjective);
  const commandInputRef = useRef<HTMLInputElement>(null);

  const recordChronicle = useCallback((entry: Omit<ChronicleEntry, "id" | "time" | "live">) => {
    const time = new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    setLiveChronicles((current) => [
      { ...entry, id: `event-${Date.now()}-${current.length}`, time: `JUST NOW · ${time}`, live: true },
      ...current,
    ].slice(0, 20));
  }, []);
  const openCommandPalette = useCallback(() => {
    setSettingsOpen(false);
    setCommandQuery("");
    setCommandOpen(true);
  }, []);

  const onInventoryUpdated = useCallback(
    (data: Events["inventoryUpdated"]) => {
      if (!Number.isFinite(data?.count)) return;
      const count = Math.max(0, data.count);
      setItemCount(count);
      recordChronicle({
        kind: "gear",
        title: "Pack count synchronized",
        detail: `The bridge reports ${count} objects in the player's inventory.`,
        page: "armory",
      });
      setEventToast({ id: ++toastSequence.current, title: "Pack refreshed", detail: `${count} objects reported by the bridge.`, page: "armory" });
    },
    [recordChronicle],
  );
  const onCompanionUpdated = useCallback((data: Events["companionUpdated"]) => {
    if (!data || typeof data !== "object") return;
    setCompanion((current) => mergeCompanion(current, data));
    setPortraitFailed(false);
    const name = typeof data.name === "string" && data.name.trim() ? data.name.trim() : "Companion";
    const changed = [data.health !== undefined || data.healthMax !== undefined ? "vitals" : "", data.location ? "location" : "", data.portraitUrl ? "portrait" : ""]
      .filter(Boolean).join(", ");
    recordChronicle({
      kind: "party",
      title: `${name} synchronized`,
      detail: changed ? `The bridge updated companion ${changed}.` : "Companion details arrived through the native event bridge.",
      page: "party",
    });
    setEventToast({ id: ++toastSequence.current, title: `${name} updated`, detail: changed ? `New ${changed} received.` : "Companion data received from the bridge.", page: "party" });
  }, [recordChronicle]);
  const toggleQuestObjective = (questId: string, objectiveIndex: number) => {
    const quest = sampleQuests.find((item) => item.id === questId);
    if (!quest) return;
    const { wasComplete, completedObjectives } = toggleObjective(questId, objectiveIndex);
    const completed = !wasComplete && completedObjectives.length === quest.objectives.length;
    const objective = quest.objectives[objectiveIndex];
    recordChronicle({
      kind: "quest",
      title: completed ? `${quest.title} completed` : `${wasComplete ? "Objective reopened" : "Objective recorded"}`,
      detail: `${objective} · ${quest.title}`,
      page: "wayfinder",
    });
    if (completed) {
      setEventToast({ id: ++toastSequence.current, title: "Quest completed", detail: `${quest.title} · ${quest.reward}`, page: "wayfinder" });
    }
  };
  const selectMapLocation = (location: string) => {
    if (location === mapLocation) return;
    setMapLocation(location);
    recordChronicle({ kind: "journey", title: `Map destination changed to ${location}`, detail: "The active destination was updated on the Skyrim hold map.", page: "wayfinder" });
    setEventToast({ id: ++toastSequence.current, title: "Destination marked", detail: `The map is now focused on ${location}.`, page: "wayfinder" });
  };
  useSkyUltraEvent(sky, "inventoryUpdated", onInventoryUpdated);
  useSkyUltraEvent(sky, "companionUpdated", onCompanionUpdated);

  useEffect(() => {
    if (!eventToast) return;
    const timeout = window.setTimeout(() => {
      setEventToast((current) => current?.id === eventToast.id ? null : current);
    }, 5600);
    return () => window.clearTimeout(timeout);
  }, [eventToast]);

  useEffect(() => {
    if (!commandOpen) return;
    const timeout = window.setTimeout(() => commandInputRef.current?.focus(), 50);
    return () => window.clearTimeout(timeout);
  }, [commandOpen]);

  useEffect(() => {
    const onShortcut = (event: KeyboardEvent) => {
      const target = event.target;
      const isTyping = target instanceof HTMLElement &&
        (target.isContentEditable || ["INPUT", "TEXTAREA", "SELECT"].includes(target.tagName));
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k") {
        event.preventDefault();
        openCommandPalette();
        return;
      }
      const pageShortcuts: Record<string, ArchivePage> = { "1": "armory", "2": "party", "3": "chronicle", "4": "wayfinder" };
      if (!isTyping && pageShortcuts[event.key]) {
        setPage(pageShortcuts[event.key]);
        return;
      }
      if (event.key === "/" && !isTyping) {
        event.preventDefault();
        searchRef.current?.focus();
      }
      if (event.key === "Escape" && query && !commandOpen) setQuery("");
    };
    window.addEventListener("keydown", onShortcut);
    return () => window.removeEventListener("keydown", onShortcut);
  }, [commandOpen, openCommandPalette, query]);

  const visibleItems = useMemo(() => {
    const normalizedQuery = query.trim().toLocaleLowerCase();
    const filtered = inventory.filter((item) => {
      const matchesCategory = category === "all" || item.category === category;
      const matchesQuery = !normalizedQuery ||
        `${item.name} ${item.source} ${item.kind} ${item.rarity}`.toLocaleLowerCase().includes(normalizedQuery);
      const matchesFavorite = !favoriteOnly || favorites.has(item.id);
      return matchesCategory && matchesQuery && matchesFavorite;
    });
    if (sortOrder === "value") return filtered.sort((left, right) => right.value - left.value);
    if (sortOrder === "name") return filtered.sort((left, right) => left.name.localeCompare(right.name));
    if (sortOrder === "weight") return filtered.sort((left, right) => left.weight - right.weight);
    return filtered;
  }, [category, favoriteOnly, favorites, query, sortOrder]);

  const selectedItem = inventory.find((item) => item.id === selectedId) ?? inventory[0];
  const normalizedCommandQuery = commandQuery.trim().toLocaleLowerCase();
  const matchingPages = archivePages.filter((item) => item.label.toLocaleLowerCase().includes(normalizedCommandQuery));
  const matchingItems = inventory.filter((item) => `${item.name} ${item.kind} ${item.rarity}`.toLocaleLowerCase().includes(normalizedCommandQuery));
  const matchingThemes = themes.filter((item) => item.label.toLocaleLowerCase().includes(normalizedCommandQuery));
  const hasCommandResults = matchingPages.length + matchingItems.length + matchingThemes.length > 0;
  const runCommand = (action: () => void) => {
    action();
    setCommandOpen(false);
    setCommandQuery("");
  };
  const navigateCommandResults = (event: React.KeyboardEvent<HTMLDivElement>) => {
    if (event.key === "Enter" && event.target instanceof HTMLInputElement) {
      event.preventDefault();
      event.currentTarget.querySelector<HTMLButtonElement>(".command-results .command-row")?.click();
      return;
    }
    if (event.key !== "ArrowDown" && event.key !== "ArrowUp") return;
    const results = Array.from(event.currentTarget.querySelectorAll<HTMLButtonElement>(".command-results .command-row"));
    if (!results.length) return;
    event.preventDefault();
    const activeIndex = results.indexOf(document.activeElement as HTMLButtonElement);
    const nextIndex = activeIndex < 0
      ? event.key === "ArrowDown" ? 0 : results.length - 1
      : (activeIndex + (event.key === "ArrowDown" ? 1 : -1) + results.length) % results.length;
    results[nextIndex]?.focus();
  };
  const categoryCounts = useMemo(() => ({
    all: inventory.length,
    weapons: inventory.filter((item) => item.category === "weapons").length,
    apparel: inventory.filter((item) => item.category === "apparel").length,
    alchemy: inventory.filter((item) => item.category === "alchemy").length,
  }), []);
  const cycleSortOrder = () => setSortOrder((current) =>
    current === "recent" ? "value" : current === "value" ? "name" : current === "name" ? "weight" : "recent",
  );
  const toggleFavorite = (id: string) => setFavorites((current) => {
    const next = new Set(current);
    if (next.has(id)) next.delete(id);
    else next.add(id);
    return next;
  });
  const toggleEquipped = (id: string) => {
    const item = inventory.find((candidate) => candidate.id === id);
    if (!item) return;
    const wasEquipped = equippedIds.has(id);
    setActiveLoadoutId(null);
    setEquippedIds((current) => {
      const next = new Set(current);
      if (next.has(id)) next.delete(id);
      else {
        const item = inventory.find((candidate) => candidate.id === id);
        if (item) {
          for (const equippedId of next) {
            if (inventory.find((candidate) => candidate.id === equippedId)?.category === item.category) {
              next.delete(equippedId);
            }
          }
        }
        next.add(id);
      }
      return next;
    });
    recordChronicle({
      kind: "gear",
      title: wasEquipped ? `${item.name} removed from the quick kit` : `${item.name} staged in the quick kit`,
      detail: `${item.kind} · ${item.category === "weapons" ? "weapon" : item.category === "apparel" ? "armor" : "supply"} slot`,
      itemId: item.id,
      page: "armory",
    });
  };
  const saveCurrentLoadout = () => {
    if (equippedIds.size === 0 || loadouts.length >= 8) return;
    const name = loadoutName.trim() || `Field kit ${loadouts.length - sampleLoadouts.length + 1}`;
    const preset: LoadoutPreset = { id: `custom-${Date.now()}`, name, itemIds: Array.from(equippedIds) };
    setLoadouts((current) => [...current, preset]);
    setActiveLoadoutId(preset.id);
    setLoadoutName("");
    recordChronicle({ kind: "gear", title: `Loadout saved: ${name}`, detail: `${preset.itemIds.length} equipped objects saved as a local preview kit.`, page: "armory" });
    setEventToast({ id: ++toastSequence.current, title: "Kit recorded", detail: `${name} · ${preset.itemIds.length} objects`, page: "armory" });
  };
  const applyLoadout = (preset: LoadoutPreset) => {
    const itemIds = preset.itemIds.filter((id) => inventory.some((item) => item.id === id));
    setEquippedIds(new Set(itemIds));
    setActiveLoadoutId(preset.id);
    recordChronicle({ kind: "gear", title: `Loadout equipped: ${preset.name}`, detail: `${itemIds.length} sample objects staged in the quick kit.`, page: "armory" });
    setEventToast({ id: ++toastSequence.current, title: "Kit ready", detail: `${preset.name} · ${itemIds.length} objects staged`, page: "armory" });
  };
  const deleteLoadout = (preset: LoadoutPreset) => {
    if (preset.starter) return;
    setLoadouts((current) => current.filter((item) => item.id !== preset.id));
    if (activeLoadoutId === preset.id) setActiveLoadoutId(null);
  };
  const healthPercent = percentage(companion.health, companion.healthMax);
  const equippedItems = inventory.filter((item) => equippedIds.has(item.id));
  const equippedWeapon = equippedItems.find((item) => item.category === "weapons");
  const equippedArmor = equippedItems.find((item) => item.category === "apparel");
  const equippedSupply = equippedItems.find((item) => item.category === "alchemy");
  const compareItem = inventory.find((item) => item.id !== selectedItem.id && item.category === selectedItem.category && equippedIds.has(item.id))
    ?? inventory.find((item) => item.id !== selectedItem.id && item.category === selectedItem.category);
  const selectedRating = parseRating(selectedItem.rating);
  const compareRating = compareItem ? parseRating(compareItem.rating) : undefined;
  const ratingDelta = selectedRating && compareRating && selectedRating.unit.toLowerCase() === compareRating.unit.toLowerCase()
    ? selectedRating.value - compareRating.value
    : undefined;
  const slotItems = slotPicker ? inventory.filter((item) => item.category === slotPicker) : [];
  const slotPickerLabel = slotPicker === "weapons" ? "Weapon" : slotPicker === "apparel" ? "Armor" : "Supply";
  const portraitUrl = companion.portraitUrl && !portraitFailed ? companion.portraitUrl : undefined;
  const portraitCredit = portraitUrl === SAMPLE_PORTRAIT ? "SAMPLE ART · REPLACE WITH YOUR MOD ASSET" :
    portraitUrl ? "MOD-SUPPLIED PORTRAIT" : "NO PORTRAIT ASSET · INITIALS SHOWN";

  return (
    <SkyThemeProvider theme={themeBase[palette]} data-sky-palette={palette} className="mod-shell">
      <header className="topbar">
        <a className="brand-lockup" href="#top" aria-label="SkyUltra field archive">
          <span className="brand-seal"><Compass size={23} strokeWidth={1.35} /></span>
          <span className="brand-copy"><strong>SKYULTRA</strong><small>FIELD ARCHIVE <i /> 04E</small></span>
        </a>

        <nav className="page-nav" aria-label="Field archive sections">
          {archivePages.map((item) => (
            <button key={item.value} type="button" className={page === item.value ? "page-choice page-choice--active" : "page-choice"} onClick={() => setPage(item.value)} aria-current={page === item.value ? "page" : undefined}>
              <span className="page-choice-icon">{item.icon}</span>{item.label}
            </button>
          ))}
        </nav>

        <div className="topbar-actions">
          <button type="button" className="command-trigger" onClick={openCommandPalette} aria-label="Open quick navigation"><Search size={14} /><span>QUICK OPEN</span><kbd>CTRL K</kbd></button>
          <div className="septim-pill"><Coins size={16} /><span>2,072</span><small>G</small></div>
          <label className="theme-picker"><Palette size={15} aria-hidden="true" /><select aria-label="Choose a color theme" value={palette} onChange={(event) => setPalette(event.currentTarget.value as Palette)}>{themes.map((option) => <option key={option.value} value={option.value}>{option.label}</option>)}</select><ChevronDown size={13} aria-hidden="true" /></label>
          <SkyDialogRoot open={settingsOpen} onOpenChange={setSettingsOpen}>
            <SkyDialogTrigger asChild><SkyButton className="settings-trigger" variant="ghost" size="sm" aria-label="Open interface settings"><Settings2 size={18} /></SkyButton></SkyDialogTrigger>
            <SkyDialogContent data-sky-palette={palette} aria-describedby="settings-copy">
              <SkyDialogTitle>Archive settings</SkyDialogTitle>
              <SkyDialogDescription id="settings-copy">Color themes and game data are controlled by this mod page. The portrait shown here is local sample artwork.</SkyDialogDescription>
              <div className="settings-state"><span>Bridge connection</span><SkyBadge tone={runtime.capabilities.events ? "success" : "danger"}>{runtime.host.toUpperCase()} · {runtime.capabilities.events ? "EVENTS READY" : "OFFLINE"}</SkyBadge></div>
              <SkyDialogClose asChild><SkyButton className="settings-done">Back to archive</SkyButton></SkyDialogClose>
            </SkyDialogContent>
          </SkyDialogRoot>
        </div>
      </header>

      <SkyDialogRoot open={commandOpen} onOpenChange={(open) => { setCommandOpen(open); if (!open) setCommandQuery(""); }}>
        <SkyDialogContent data-sky-palette={palette} className="command-dialog" aria-describedby="command-copy" onKeyDown={navigateCommandResults}>
          <div className="command-heading"><SkyDialogTitle>Quick open</SkyDialogTitle><kbd>ESC</kbd></div>
          <SkyDialogDescription id="command-copy">Jump to a section, open an item, or change the archive palette.</SkyDialogDescription>
          <label className="command-search"><Search size={17} /><input ref={commandInputRef} value={commandQuery} onChange={(event) => setCommandQuery(event.currentTarget.value)} placeholder="Search sections, items, themes..." aria-label="Search quick actions" /><kbd>CTRL K</kbd></label>
          <div className="command-results" role="group" aria-label="Quick navigation results">
            {matchingPages.length > 0 && <div className="command-group"><small>SECTIONS</small>{matchingPages.map((item) => <button key={`page-${item.value}`} type="button" className="command-row" onClick={() => runCommand(() => setPage(item.value))}><span className="command-row-icon">{item.icon}</span><span><strong>{item.label}</strong><small>Open archive section</small></span><kbd>{archivePages.indexOf(item) + 1}</kbd></button>)}</div>}
            {matchingItems.length > 0 && <div className="command-group"><small>ITEMS</small>{matchingItems.map((item) => <button key={`item-${item.id}`} type="button" className="command-row" onClick={() => runCommand(() => { setSelectedId(item.id); setCategory("all"); setFavoriteOnly(false); setQuery(""); setPage("armory"); })}><span className="command-row-icon">{item.category === "weapons" ? <Swords /> : item.category === "apparel" ? <Shield /> : <Sparkles />}</span><span><strong>{item.name}</strong><small>{item.rarity} · {item.kind}</small></span><span className="command-result-arrow">↗</span></button>)}</div>}
            {matchingThemes.length > 0 && <div className="command-group"><small>PALETTES</small>{matchingThemes.map((item) => <button key={`theme-${item.value}`} type="button" className="command-row" onClick={() => runCommand(() => setPalette(item.value))}><span className="command-swatch" style={{ background: item.swatch }} /><span><strong>{item.label}</strong><small>Change archive colors</small></span><span className="command-result-arrow">↗</span></button>)}</div>}
            {!hasCommandResults && <div className="command-empty"><Sparkles size={18} /><strong>No matching records</strong><span>Try a section, item, or palette name.</span></div>}
          </div>
          <div className="command-footer"><span><kbd>CTRL</kbd> + <kbd>K</kbd> TO OPEN</span><span>LOCAL ARCHIVE SEARCH</span></div>
        </SkyDialogContent>
      </SkyDialogRoot>

      <SkyDialogRoot open={compareOpen} onOpenChange={setCompareOpen}>
        <SkyDialogContent data-sky-palette={palette} className="compare-dialog" aria-describedby="compare-copy">
          <div className="compare-heading">
            <div><span className="section-eyebrow">FIELD BENCHMARK</span><SkyDialogTitle>Compare <em>relics</em></SkyDialogTitle></div>
            <SkyDialogClose aria-label="Close item comparison"><X size={16} /></SkyDialogClose>
          </div>
          <SkyDialogDescription id="compare-copy">Review the selected item against your equipped piece or a collection reference. Values are sample data.</SkyDialogDescription>
          {compareItem ? <>
            <div className="compare-items">
              <CompareCard item={compareItem} label={equippedIds.has(compareItem.id) ? "CURRENT KIT" : "COLLECTION REFERENCE"} />
              <span className="compare-versus" aria-hidden="true">VS</span>
              <CompareCard item={selectedItem} label="YOUR PICK" featured />
            </div>
            <section className="compare-deltas" aria-label="Item differences">
              <div><small>{selectedRating && compareRating && selectedRating.unit.toLowerCase() === compareRating.unit.toLowerCase() ? selectedRating.unit.toUpperCase() : "RATING"}</small><strong>{ratingDelta === undefined ? "Different properties" : `${formatSigned(ratingDelta)} ${selectedRating?.unit ?? ""}`}</strong></div>
              <div><small>WEIGHT CHANGE</small><strong>{formatSigned(selectedItem.weight - compareItem.weight, 1)} WT</strong></div>
              <div><small>VALUE CHANGE</small><strong>{formatSigned(selectedItem.value - compareItem.value)} G</strong></div>
            </section>
          </> : <div className="compare-empty"><Sparkles /><strong>No matching reference</strong><span>Choose another item to compare its category.</span></div>}
          <div className="compare-footer">
            <small>PREVIEW ONLY · DOES NOT CHANGE SKYRIM</small>
            {compareItem && <button type="button" className="compare-stage" onClick={() => { if (!equippedIds.has(selectedItem.id)) toggleEquipped(selectedItem.id); setCompareOpen(false); }}>
              {equippedIds.has(selectedItem.id) ? "DONE" : <><Swords size={13} /> STAGE THIS ITEM</>}
            </button>}
          </div>
        </SkyDialogContent>
      </SkyDialogRoot>

      <SkyDialogRoot open={slotPicker !== null} onOpenChange={(open) => { if (!open) setSlotPicker(null); }}>
        <SkyDialogContent data-sky-palette={palette} className="slot-picker-dialog" aria-describedby="slot-picker-copy">
          <div className="slot-picker-heading">
            <div><span className="section-eyebrow">QUICK KIT · SLOT SELECT</span><SkyDialogTitle>Choose your <em>{slotPickerLabel.toLowerCase()}</em></SkyDialogTitle></div>
            <SkyDialogClose aria-label="Close slot picker"><X size={16} /></SkyDialogClose>
          </div>
          <SkyDialogDescription id="slot-picker-copy">Select a sample item to stage it in this slot. The current item is replaced.</SkyDialogDescription>
          <div className="slot-picker-list">
            {slotItems.map((item, index) => {
              const equipped = equippedIds.has(item.id);
              return (
                <button
                  key={item.id}
                  type="button"
                  className={equipped ? "slot-option slot-option--active" : "slot-option"}
                  style={{ "--card-index": index } as React.CSSProperties}
                  aria-pressed={equipped}
                  onClick={() => {
                    setSelectedId(item.id);
                    if (!equipped) toggleEquipped(item.id);
                    setSlotPicker(null);
                  }}
                >
                  <span className="slot-option-art"><ItemArtwork kind={item.artwork} /></span>
                  <span className="slot-option-copy"><strong>{item.name}</strong><small>{item.rarity} · {item.kind}</small><span>{item.source}</span></span>
                  <span className="slot-option-stats"><strong>{item.rating}</strong><small>{item.weight.toFixed(1)} WT · {item.value.toLocaleString()} G</small></span>
                  <span className="slot-option-state">{equipped ? <><Check size={13} /> IN KIT</> : <><span>+</span> STAGE</>}</span>
                </button>
              );
            })}
          </div>
          <div className="slot-picker-footer"><small>PREVIEW LOADOUT · NO GAME EQUIPMENT CHANGES</small></div>
        </SkyDialogContent>
      </SkyDialogRoot>

      {eventToast && <aside key={eventToast.id} className="event-toast" role="status" aria-live="polite">
        <span className="toast-rune"><Sparkles size={17} /></span>
        <div className="toast-copy"><small>FIELD UPDATE · JUST NOW</small><strong>{eventToast.title}</strong><p>{eventToast.detail}</p></div>
        <button className="toast-open" type="button" onClick={() => { setPage(eventToast.page); setEventToast(null); }}>OPEN</button>
        <button className="toast-dismiss" type="button" aria-label="Dismiss field update" onClick={() => setEventToast(null)}><X size={14} /></button>
      </aside>}

      <main id="top" className="archive-shell">
        <section className="hero-stage" aria-label="Field archive overview">
          <div className="hero-grid" aria-hidden="true" />
          <div className="hero-orbit hero-orbit--one" aria-hidden="true" />
          <div className="hero-orbit hero-orbit--two" aria-hidden="true" />
          <svg className="hero-constellation" viewBox="0 0 500 250" aria-hidden="true">
            <path d="m24 175 63-76 66 39 61-85 69 57 72-43 99 54" />
            <path d="m153 138 44 83 86-111 76 111 95-100" />
            {[24, 87, 153, 214, 283, 355, 454, 197, 369].map((x, index) => <circle key={`${x}-${index}`} cx={x} cy={[175, 99, 138, 53, 110, 67, 121, 221, 221][index]} r={index % 3 === 0 ? 3 : 2} />)}
          </svg>

          <div className="hero-copy">
            <div className="hero-kicker"><span className="signal-dot" /> FIELD ARCHIVE <span className="hero-kicker-line" /> SKYRIM · 4E 201</div>
            <h1>Carry a legend.<br /><em>Make it yours.</em></h1>
            <p>A living field archive for the gear, people, and places that shape your story.</p>
            <button type="button" className="hero-location" onClick={() => setPage("wayfinder")} aria-label={`Open ${companion.location} on the field map`}><MapPin size={15} /><span>ACTIVE ROUTE</span><i /><small>{companion.location.toUpperCase()}</small><b>OPEN MAP ↗</b></button>
            <div className="hero-signals" aria-label="Archive overview">
              <div><span className="hero-signal-icon"><Backpack size={14} /></span><span><small>PACK RECORD</small><strong>{itemCount.toString().padStart(2, "0")} <i>OBJECTS</i></strong></span></div>
              <div><span className="hero-signal-icon hero-signal-icon--violet"><Users size={14} /></span><span><small>TRAVELING ALLY</small><strong>01 <i>COMPANION</i></strong></span></div>
              <div><span className="hero-signal-icon hero-signal-icon--gold"><BookOpen size={14} /></span><span><small>FIELD CONTRACTS</small><strong>{sampleQuests.length.toString().padStart(2, "0")} <i>SAMPLE QUESTS</i></strong></span></div>
            </div>
          </div>

          <figure className="hero-companion">
            <div className="hero-companion-portrait">
              <div className="portrait-frame">
                {portraitUrl ? <img src={portraitUrl} alt={`Portrait of ${companion.name}`} onError={() => setPortraitFailed(true)} /> : <div className="portrait-initials" aria-label={`No portrait asset for ${companion.name}`}>{initials(companion.name)}</div>}
                <span className="portrait-glass" />
                <span className="portrait-level">LVL <strong>{companion.level}</strong></span>
                <span className="portrait-credit">{portraitCredit}</span>
              </div>
              <span className="hero-portrait-caption">FIELD COMPANION <i /> 01</span>
            </div>
            <figcaption>
              <div className="hero-companion-status"><i /> ACTIVE ALLY <span>LVL {companion.level}</span></div>
              <span className="hero-companion-title">{companion.title}</span>
              <strong>{companion.name}</strong>
              <small>{companion.race} <i /> {companion.role}</small>
              <div className="hero-companion-bottom"><span><MapPin size={13} /> {companion.location}</span><button type="button" onClick={() => setPage("party")}>OPEN DOSSIER <b>↗</b></button></div>
            </figcaption>
          </figure>
        </section>

        <section className="vitals-grid" aria-label="Current expedition status">
          <MetricCard icon={<Heart />} label="HEALTH" value={`${companion.health} / ${companion.healthMax}`} detail={healthPercent >= 75 ? "READY FOR THE ROAD" : healthPercent >= 35 ? "TAKE CARE OUT THERE" : "NEEDS ATTENTION"} percent={healthPercent} tone="coral" />
          <MetricCard icon={<Sparkles />} label="STAMINA" value={`${companion.stamina} / ${companion.staminaMax}`} detail="COMPANION VITALS" percent={percentage(companion.stamina, companion.staminaMax)} tone="lime" />
          <MetricCard icon={<Compass />} label="MAGICKA" value={`${companion.magicka} / ${companion.magickaMax}`} detail="COMPANION VITALS" percent={percentage(companion.magicka, companion.magickaMax)} tone="violet" />
          <MetricCard icon={<Weight />} label="PACK LOAD" value="142 / 300" detail={`${itemCount} OBJECTS · 47% CAPACITY`} percent={47} tone="gold" />
        </section>

        {page === "armory" && <div key={page} className="archive-layout page-enter">
          <section className="collection-panel" aria-label="Pack contents">
            <div className="section-heading">
              <div><span className="section-eyebrow">THE THINGS YOU CARRY</span><h2>Pack <em>contents</em></h2></div>
              <span className="collection-count">{visibleItems.length} <small>OF {inventory.length} OBJECTS</small></span>
            </div>

            <div className="category-chips" aria-label="Filter by item category">
              {categories.map((item) => (
                <button key={item.value} type="button" className={category === item.value ? "category-chip category-chip--active" : "category-chip"} onClick={() => setCategory(item.value)} aria-pressed={category === item.value}>
                  {item.label}<span>{categoryCounts[item.value]}</span>
                </button>
              ))}
            </div>

            <div className="collection-tools">
              <label className="search-box"><Search size={17} /><input ref={searchRef} value={query} onChange={(event) => setQuery(event.currentTarget.value)} placeholder="Find a relic, material, or enchantment..." aria-label="Search inventory" /><kbd>/</kbd></label>
              <button type="button" className={favoriteOnly ? "filter-button filter-button--active" : "filter-button"} onClick={() => setFavoriteOnly((active) => !active)} aria-pressed={favoriteOnly}><Star size={15} fill={favoriteOnly ? "currentColor" : "none"} /> FAVOURITES <span>{favorites.size}</span></button>
              <button type="button" className="sort-button" onClick={cycleSortOrder} aria-label={`Sort inventory: ${sortOrder}`}><span>SORT</span> {sortOrder.toUpperCase()} <ChevronDown size={14} /></button>
            </div>

            <div className="collection-grid">
              {visibleItems.length ? visibleItems.map((item, index) => (
                <ItemCard
                  key={item.id}
                  item={item}
                  index={index}
                  selected={selectedId === item.id}
                  favorite={favorites.has(item.id)}
                  onSelect={() => setSelectedId(item.id)}
                  onFavorite={() => toggleFavorite(item.id)}
                />
              )) : <div className="empty-result"><Sparkles /><strong>No relics found</strong><span>Clear your search or change the active filters.</span><button type="button" onClick={() => { setQuery(""); setFavoriteOnly(false); setCategory("all"); }}>RESET FILTERS</button></div>}
            </div>
          </section>

          <aside className="detail-column" aria-label="Selected item and equipment">
            <SkyPanel className="relic-detail">
              <div className={`relic-art relic-art--${selectedItem.artwork}`}><span className="detail-orbit" /><ItemArtwork kind={selectedItem.artwork} /><span className={`rarity-chip rarity-chip--${selectedItem.rarity.toLowerCase()}`}>{selectedItem.rarity}</span></div>
              <SkyPanelContent className="relic-copy">
                <div className="relic-title-row"><span className="section-eyebrow">SELECTED RELIC</span><button className={`favorite-button ${favorites.has(selectedItem.id) ? "is-favorite" : ""}`} type="button" aria-label={favorites.has(selectedItem.id) ? "Remove favorite" : "Add favorite"} onClick={() => toggleFavorite(selectedItem.id)}><Star size={17} fill={favorites.has(selectedItem.id) ? "currentColor" : "none"} /></button></div>
                <h3>{selectedItem.name}</h3><p className="relic-origin">{selectedItem.source}</p><p className="relic-description">{selectedItem.description}</p>
                <div className="relic-measures">
                  <span><small>{selectedItem.rating.includes("DMG") ? "DAMAGE" : selectedItem.rating.includes("ARM") ? "ARMOR" : "PROPERTY"}</small><strong>{selectedItem.rating}</strong></span>
                  <span><small>WEIGHT</small><strong>{selectedItem.weight.toFixed(1)} <i>WT</i></strong></span>
                  <span><small>VALUE</small><strong>{selectedItem.value.toLocaleString()} <i>G</i></strong></span>
                </div>
                <div className="relic-actions">
                  <SkyButton className="equip-button" onClick={() => toggleEquipped(selectedItem.id)}>{equippedIds.has(selectedItem.id) ? <Check size={15} /> : <Swords size={15} />}{equippedIds.has(selectedItem.id) ? "EQUIPPED" : "EQUIP IN SAMPLE"}</SkyButton>
                  <button type="button" className="compare-trigger" onClick={() => setCompareOpen(true)}><Swords size={14} /> COMPARE</button>
                </div>
                <p className="sample-action-note">Preview state only · connect a native request to equip in game</p>
              </SkyPanelContent>
            </SkyPanel>

            <section className="quick-kit">
              <div className="mini-heading"><span>QUICK KIT</span><span>{equippedItems.length} STAGED</span></div>
              <GearRow icon={<Swords />} slot="WEAPON" item={equippedWeapon?.name ?? "No weapon slotted"} color="coral" filled={Boolean(equippedWeapon)} onClick={() => setSlotPicker("weapons")} />
              <GearRow icon={<Shield />} slot="ARMOR" item={equippedArmor?.name ?? "No armor slotted"} color="cyan" filled={Boolean(equippedArmor)} onClick={() => setSlotPicker("apparel")} />
              <GearRow icon={<Sparkles />} slot="SUPPLY" item={equippedSupply?.name ?? "No supply slotted"} color="violet" filled={Boolean(equippedSupply)} onClick={() => setSlotPicker("alchemy")} />
            </section>

            <section className="loadout-shelf" aria-label="Saved loadouts">
              <div className="mini-heading"><span>LOADOUT SHELF</span><span>{loadouts.length} / 8 KITS</span></div>
              <form className="loadout-save-row" onSubmit={(event) => { event.preventDefault(); saveCurrentLoadout(); }}>
                <input
                  value={loadoutName}
                  onChange={(event) => setLoadoutName(event.currentTarget.value)}
                  maxLength={24}
                  placeholder="Name this kit"
                  aria-label="Name this loadout"
                />
                <button type="submit" disabled={equippedIds.size === 0 || loadouts.length >= 8}><BookOpen size={13} /> SAVE</button>
              </form>
              <div className="loadout-list">
                {loadouts.map((preset, index) => {
                  const names = preset.itemIds
                    .map((id) => inventory.find((item) => item.id === id)?.name)
                    .filter((name): name is string => Boolean(name));
                  const active = activeLoadoutId === preset.id;
                  return (
                    <article
                      key={preset.id}
                      className={active ? "loadout-card loadout-card--active" : "loadout-card"}
                      style={{ "--card-index": index } as React.CSSProperties}
                    >
                      <div className="loadout-card-heading">
                        <span><strong>{preset.name}</strong><small>{names.length} OBJECTS · {preset.starter ? "FIELD SET" : "CUSTOM"}</small></span>
                        {!preset.starter && <button type="button" className="loadout-delete" onClick={() => deleteLoadout(preset)} aria-label={`Delete ${preset.name}`}><X size={13} /></button>}
                      </div>
                      <p title={names.join(" · ")}>{names.join(" · ") || "Empty kit"}</p>
                      <button type="button" className="loadout-apply" onClick={() => applyLoadout(preset)} aria-pressed={active}>
                        {active ? <><Check size={12} /> ACTIVE KIT</> : <>EQUIP KIT <span>↗</span></>}
                      </button>
                    </article>
                  );
                })}
              </div>
              <small className="loadout-note">LOCAL PREVIEW PRESETS · NO GAME EQUIPMENT CHANGES</small>
            </section>

            <section className="bridge-card">
              <div className="bridge-pulse"><span /></div><div><strong>{runtime.host === "preview" ? "PREVIEW BRIDGE" : "SKYULTRA BRIDGE"}</strong><small>{runtime.capabilities.events ? "Companion + inventory events listening" : "Waiting for native bridge"}</small></div><CircleHelp size={17} />
            </section>
          </aside>
        </div>}

        {page === "party" && <PartyPage key={page} companion={companion} portraitUrl={portraitUrl} portraitCredit={portraitCredit} eventsReady={runtime.capabilities.events} onNavigate={setPage} />}
        {page === "chronicle" && <ChroniclePage key={page} liveEvents={liveChronicles} setPage={setPage} selectItem={(id) => { setSelectedId(id); setPage("armory"); }} />}
        {page === "wayfinder" && <WayfinderPage key={page} location={mapLocation} trackedQuestId={trackedQuestId} questProgress={questProgress} onSelect={selectMapLocation} onTrackQuest={setTrackedQuestId} onToggleObjective={toggleQuestObjective} />}

        <footer className="archive-footer"><span><span className="footer-seal">✦</span> SKYULTRA <i /> FIELD ARCHIVE <i /> {(runtime.browserEngine ?? "unknown").toUpperCase()}</span><span><kbd>1–4</kbd> SECTIONS <b>·</b> <kbd>/</kbd> SEARCH <b>·</b> LOCAL PREVIEW DATA</span></footer>
      </main>
    </SkyThemeProvider>
  );
}

function PageHeading({ kicker, title, description, mark }: { kicker: string; title: React.ReactNode; description: string; mark: string }): React.JSX.Element {
  return <div className="feature-heading"><div><span className="section-eyebrow">{kicker}</span><h2>{title}</h2><p>{description}</p></div><span className="feature-mark">{mark}</span></div>;
}

function PartyPage({ companion, portraitUrl, portraitCredit, eventsReady, onNavigate }: { companion: CompanionProfile; portraitUrl?: string; portraitCredit: string; eventsReady: boolean; onNavigate: (page: ArchivePage) => void }): React.JSX.Element {
  return (
    <section className="feature-page party-page" aria-label="Companion details">
      <PageHeading kicker="THE PEOPLE BEHIND THE STORY" title={<>Your <em>party</em></>} description="A field dossier for the allies who travel beside you." mark="02" />
      <div className="party-layout">
        <article className="party-identity-card">
          <div className="party-portrait">
            {portraitUrl ? <img src={portraitUrl} alt={`Portrait of ${companion.name}`} /> : <span>{initials(companion.name)}</span>}
            <span className="party-portrait-level">LVL <strong>{companion.level}</strong></span>
            <small>{portraitCredit}</small>
          </div>
          <div className="party-identity-copy">
            <div className="party-identity-topline"><span><i /> ACTIVE COMPANION DOSSIER</span><small>BRIDGE PROFILE</small></div>
            <h3>{companion.name}</h3>
            <p>{companion.title}</p>
            <div className="party-calling"><span><Compass size={19} /></span><div><small>FIELD CALLING</small><strong>{companion.role}</strong></div></div>
            <div className="party-profile-grid">
              <div><small>ANCESTRY</small><strong>{companion.race}</strong></div>
              <div><small>COMPANION LEVEL</small><strong>LEVEL {companion.level}</strong></div>
            </div>
            <div className="party-actions">
              <button type="button" onClick={() => onNavigate("armory")}><Backpack size={14} /> OPEN PACK <span>↗</span></button>
              <button type="button" onClick={() => onNavigate("wayfinder")}><MapPin size={14} /> FIELD MAP <span>↗</span></button>
            </div>
          </div>
        </article>
        <aside className="party-stats-card" aria-label="Companion vitals">
          <div className="party-vitals-heading"><div><span className="section-eyebrow">COMPANION SIGNAL</span><h3>Field condition</h3></div><span className={eventsReady ? "party-sync-state" : "party-sync-state party-sync-state--waiting"}><i /> {eventsReady ? "EVENT READY" : "AWAITING EVENT"}</span></div>
          <p className="party-vitals-copy">Current readings from the companion event bridge.</p>
          <PartyVital icon={<Heart />} label="HEALTH" value={companion.health} max={companion.healthMax} tone="coral" />
          <PartyVital icon={<Sparkles />} label="STAMINA" value={companion.stamina} max={companion.staminaMax} tone="lime" />
          <PartyVital icon={<Compass />} label="MAGICKA" value={companion.magicka} max={companion.magickaMax} tone="violet" />
          <button type="button" className="party-location-card" onClick={() => onNavigate("wayfinder")}>
            <span className="party-location-icon"><MapPin size={17} /></span>
            <span><small>LAST REPORTED LOCATION</small><strong>{companion.location}</strong></span>
            <span className="party-location-link">OPEN MAP <b>↗</b></span>
          </button>
          <div className="party-vitals-foot"><span><i /> {eventsReady ? "EVENT STREAM AVAILABLE" : "SAMPLE PROFILE DATA"}</span><span>HEALTH · STAMINA · MAGICKA</span></div>
        </aside>
        <article className="portrait-guide-card">
          <div className="guide-art"><span className="guide-art-orbit" /><Users size={29} /><span>MOD PORTRAIT</span><b>01</b></div>
          <div className="guide-copy">
            <div className="portrait-guide-heading"><div><span className="section-eyebrow">MAKE IT YOURS</span><h3>Bring your own portrait</h3></div><span className="portrait-guide-format">PNG <i /> WEBP <i /> SVG</span></div>
            <p>Ship portrait art with your mod and send its relative path with <code>companionUpdated</code>. SkyUltra displays supplied artwork; live 3D actor capture is not available yet.</p>
            <div className="portrait-guide-example"><span><small>EVENT FIELD</small><code>portraitUrl: "./portraits/companion.webp"</code></span><span><small>MOD ASSET</small><code>Data\SkyUltra\ModName\Files\portraits\companion.webp</code></span></div>
          </div>
        </article>
      </div>
    </section>
  );
}

function PartyVital({ icon, label, value, max, tone }: { icon: React.ReactNode; label: string; value: number; max: number; tone: string }): React.JSX.Element {
  const percent = percentage(value, max);
  return (
    <article className={`party-vital party-vital--${tone}`}>
      <div className="party-vital-top"><span className="party-vital-icon">{icon}</span><span className="party-vital-name">{label}<small>CORE VITAL</small></span><strong>{value.toLocaleString()}<i> / {max.toLocaleString()}</i></strong></div>
      <div className="party-vital-track"><span style={{ width: `${percent}%` }} /></div>
      <div className="party-vital-meta"><small>{Math.round(percent)}% CAPACITY</small><span>{percent >= 85 ? "FULL RESERVE" : percent >= 45 ? "STEADY" : "LOW RESERVE"}</span></div>
    </article>
  );
}

type ChronicleFilter = "all" | ChronicleKind;

const chronicles: ChronicleEntry[] = [
  { id: "found", kind: "gear", time: "TODAY · 18:42", title: "Wyrmsteel Greatsword entered the archive", detail: "A road-worn relic from a smith in Whiterun.", itemId: "wyrmsteel" },
  { id: "route", kind: "journey", time: "TODAY · 16:10", title: "A route was marked toward Riften", detail: "The eastern road is open. A storm is gathering over the pass.", page: "wayfinder" },
  { id: "companion", kind: "party", time: "TODAY · 14:26", title: "Mira's field condition was synchronized", detail: "Health, stamina, and magicka received from the companion event.", page: "party" },
  { id: "quest", kind: "quest", time: "YESTERDAY · 19:14", title: "A new contract arrived from Whiterun", detail: "The western watchtower bears a mark no local guard can explain.", page: "wayfinder" },
  { id: "tonic", kind: "gear", time: "YESTERDAY · 21:03", title: "Moon-Sugar Tonic added to your pack", detail: "A rare draught with a silver sheen.", itemId: "moon-sugar" },
  { id: "discovery", kind: "journey", time: "YESTERDAY · 17:51", title: "A marker appeared on the field map", detail: "Local map data in this starter is illustrative preview content.", page: "wayfinder" },
];

function ChroniclePage({ liveEvents, setPage, selectItem }: { liveEvents: ChronicleEntry[]; setPage: (page: ArchivePage) => void; selectItem: (id: string) => void }): React.JSX.Element {
  const [filter, setFilter] = useState<ChronicleFilter>("all");
  const [focusedId, setFocusedId] = useState<string | null>(null);
  const entries = [...liveEvents, ...chronicles];
  const visible = filter === "all" ? entries : entries.filter((entry) => entry.kind === filter);
  const focused = visible.find((entry) => entry.id === focusedId) ?? visible[0];
  const focusedItem = focused?.itemId ? inventory.find((item) => item.id === focused.itemId) : undefined;
  const focusedIndex = focused ? visible.findIndex((entry) => entry.id === focused.id) + 1 : 0;
  const filters: { value: ChronicleFilter; label: string }[] = [
    { value: "all", label: "All entries" },
    { value: "gear", label: "Gear" },
    { value: "journey", label: "Journey" },
    { value: "party", label: "Party" },
    { value: "quest", label: "Quests" },
  ];
  const chapterFilters = filters.filter((item): item is { value: ChronicleKind; label: string } => item.value !== "all");
  return (
    <section className="feature-page chronicle-page" aria-label="Chronicle timeline">
      <PageHeading kicker="YOUR STORY, IN SMALL MOMENTS" title={<>Field <em>chronicle</em></>} description="A readable event history for discoveries, companion updates, and gear changes." mark="03" />
      <div className="chronicle-layout">
        <div className="chronicle-stream">
          <div className="chronicle-toolbar"><span>STORY THREAD <small>{liveEvents.length ? `${liveEvents.length} LIVE SIGNALS · ${chronicles.length} ARCHIVE MEMORIES` : `${chronicles.length} ARCHIVE MEMORIES · READY FOR GAME EVENTS`}</small></span><div>{filters.map((item) => { const count = item.value === "all" ? entries.length : entries.filter((entry) => entry.kind === item.value).length; return <button key={item.value} type="button" className={filter === item.value ? "chronicle-filter chronicle-filter--active" : "chronicle-filter"} onClick={() => setFilter(item.value)} aria-pressed={filter === item.value}>{item.label}<span>{count}</span></button>; })}</div></div>
          <div className="timeline-list">
            {visible.map((entry, index) => <article key={entry.id} className={`timeline-entry timeline-entry--${entry.kind} ${focused?.id === entry.id ? "timeline-entry--focused" : ""}`} style={{ "--entry-index": Math.min(index, 10) } as React.CSSProperties}>
              <span className="timeline-orb"><ChronicleGlyph kind={entry.kind} /></span>
              <button type="button" className="timeline-select" onClick={() => setFocusedId(entry.id)} aria-pressed={focused?.id === entry.id}>
                <span className="timeline-meta"><small>{entry.time}</small><i /> <b>{entry.kind.toUpperCase()}</b>{entry.live && <strong className="timeline-live-tag">LIVE</strong>}</span>
                <span className="timeline-copy"><strong>{entry.title}</strong><small>{entry.detail}</small></span>
              </button>
              <button type="button" className="timeline-open" onClick={() => entry.itemId ? selectItem(entry.itemId) : setPage(entry.page ?? "armory")}>{entry.itemId ? "VIEW ITEM" : "OPEN"}<span>↗</span></button>
            </article>)}
            {visible.length === 0 && <div className="empty-result"><BookOpen /><strong>No entries in this chapter</strong><span>Choose another filter to see more records.</span></div>}
          </div>
        </div>
        <aside className={`chapter-aside ${focused ? `chapter-aside--${focused.kind}` : ""}`}>
          <div className="chapter-visual" key={focused?.id ?? "empty"}>
            <span className="chapter-visual-ring chapter-visual-ring--one" />
            <span className="chapter-visual-ring chapter-visual-ring--two" />
            {focusedItem ? <ItemArtwork kind={focusedItem.artwork} /> : <span className="chapter-visual-glyph"><ChronicleGlyph kind={focused?.kind ?? "journey"} size={42} /></span>}
            <small>MEMORY {focusedIndex.toString().padStart(2, "0")} <i /> {visible.length.toString().padStart(2, "0")}</small>
          </div>
          <div className="chapter-focus-copy">
            <span className="section-eyebrow">{focused ? `${focused.live ? "LIVE MEMORY" : "ARCHIVE MEMORY"} · ${focused.kind.toUpperCase()}` : "NO MEMORY SELECTED"}</span>
            <h3>{focused?.title ?? "Your next story starts here."}</h3>
            <p>{focused?.detail ?? "Connect your mod's game events to record discoveries, journeys, allies, and quests."}</p>
            {focused && <small className="chapter-focus-time">{focused.time}</small>}
          </div>
          <div className="chapter-metrics">
            <div><small>THIS SESSION</small><strong>{liveEvents.length.toString().padStart(2, "0")}</strong><span>LIVE SIGNALS</span></div>
            <div><small>FIELD ARCHIVE</small><strong>{entries.length.toString().padStart(2, "0")}</strong><span>TOTAL RECORDS</span></div>
          </div>
          <div className="chapter-index">
            <div className="mini-heading"><span>CHAPTER INDEX</span><span>FILTER</span></div>
            {chapterFilters.map((item) => {
              const count = entries.filter((entry) => entry.kind === item.value).length;
              return <button key={item.value} type="button" className={filter === item.value ? "chapter-index-row chapter-index-row--active" : "chapter-index-row"} onClick={() => setFilter(item.value)} aria-pressed={filter === item.value}>
                <span className={`chapter-index-glyph chapter-index-glyph--${item.value}`}><ChronicleGlyph kind={item.value} size={13} /></span><strong>{item.label}</strong><small>{count.toString().padStart(2, "0")}</small><span className="chapter-index-arrow">↗</span>
              </button>;
            })}
          </div>
          <div className="chapter-aside-foot"><span><i /> FIELD ARCHIVE</span><span>{focused?.live ? "SYNCED JUST NOW" : "PREVIEW RECORD"}</span></div>
        </aside>
      </div>
    </section>
  );
}

function ChronicleGlyph({ kind, size }: { kind: ChronicleKind; size?: number }): React.JSX.Element {
  const iconProps = { size: size ?? 15, strokeWidth: 1.7 };
  if (kind === "gear") return <Swords {...iconProps} />;
  if (kind === "journey") return <MapPin {...iconProps} />;
  if (kind === "quest") return <Sparkles {...iconProps} />;
  return <Users {...iconProps} />;
}

const waypoints = [
  { name: "Solitude", region: "Haafingar", left: "18.3%", top: "25.8%", x: 183, y: 160 },
  { name: "Morthal", region: "Hjaalmarch", left: "35.5%", top: "34.7%", x: 355, y: 215 },
  { name: "Dawnstar", region: "The Pale", left: "57%", top: "22.9%", x: 570, y: 142 },
  { name: "Winterhold", region: "Winterhold", left: "80.5%", top: "24.7%", x: 805, y: 153 },
  { name: "Whiterun", region: "Whiterun Hold", left: "49.5%", top: "50%", x: 495, y: 310 },
  { name: "Windhelm", region: "Eastmarch", left: "81%", top: "50%", x: 810, y: 310 },
  { name: "Markarth", region: "The Reach", left: "18.5%", top: "62.9%", x: 185, y: 390 },
  { name: "Falkreath", region: "Falkreath Hold", left: "38.5%", top: "78.2%", x: 385, y: 485 },
  { name: "Riften", region: "The Rift", left: "79%", top: "75.8%", x: 790, y: 470 },
];

function WayfinderPage({ location, trackedQuestId, questProgress, onSelect, onTrackQuest, onToggleObjective }: { location: string; trackedQuestId: string; questProgress: Record<string, number[]>; onSelect: (location: string) => void; onTrackQuest: (questId: string) => void; onToggleObjective: (questId: string, objectiveIndex: number) => void }): React.JSX.Element {
  const activeWaypoint = waypoints.find((point) => point.name.toLowerCase() === location.toLowerCase()) ?? waypoints.find((point) => point.name === "Whiterun")!;
  const activeQuest = sampleQuests.find((quest) => quest.id === trackedQuestId) ?? sampleQuests[0];
  const completed = questProgress[activeQuest.id] ?? [];
  const questPercent = Math.round((completed.length / activeQuest.objectives.length) * 100);
  const [mapZoom, setMapZoom] = useState(1);
  const [mapLayers, setMapLayers] = useState({ relief: true, borders: true, roads: true });
  const [mapQuery, setMapQuery] = useState("");
  const toggleMapLayer = (layer: keyof typeof mapLayers) => setMapLayers((current) => ({ ...current, [layer]: !current[layer] }));
  const matchingWaypoints = waypoints.filter((point) => `${point.name} ${point.region}`.toLowerCase().includes(mapQuery.trim().toLowerCase()));
  const jumpToWaypoint = (name: string) => { onSelect(name); setMapQuery(""); };
  return (
    <section className="feature-page wayfinder-page" aria-label="Wayfinder map">
      <PageHeading kicker="THE WORLD IS WIDER THAN THE ROAD" title={<>Choose your <em>next horizon</em></>} description="An illustrated Skyrim atlas with all nine hold capitals and an interactive destination marker." mark="04" />
      <div className="wayfinder-layout">
        <div className="map-board">
          <div className="map-topline"><span><span className="map-live-dot" /> SKYRIM · ALL NINE HOLDS</span><span>ILLUSTRATIVE ATLAS · PREVIEW DATA</span></div>
          <div className="map-layer-tools" role="group" aria-label="Map layers">
            <span>LAYERS</span>
            {([ ["relief", "Relief"], ["borders", "Hold borders"], ["roads", "Roads"] ] as const).map(([layer, label]) => <button key={layer} type="button" className={mapLayers[layer] ? "map-layer-toggle map-layer-toggle--on" : "map-layer-toggle"} onClick={() => toggleMapLayer(layer)} aria-pressed={mapLayers[layer]}>{label}</button>)}
          </div>
          <div className="map-zoom-tools" role="group" aria-label="Map zoom controls">
            <button type="button" aria-label="Zoom out" title="Zoom out" disabled={mapZoom <= 1} onClick={() => setMapZoom((zoom) => Math.max(1, Number((zoom - .15).toFixed(2))))}>−</button>
            <span>{Math.round(mapZoom * 100)}%</span>
            <button type="button" aria-label="Zoom in" title="Zoom in" disabled={mapZoom >= 1.6} onClick={() => setMapZoom((zoom) => Math.min(1.6, Number((zoom + .15).toFixed(2))))}>+</button>
            <button type="button" aria-label="Reset map zoom" title="Reset zoom" disabled={mapZoom === 1} onClick={() => setMapZoom(1)}>↺</button>
          </div>
          <div className="map-canvas" style={{ transform: `scale(${mapZoom})` }}>
          <svg className="map-drawing" viewBox="0 0 1000 620" preserveAspectRatio="none" role="img" aria-label="Illustrated map of Skyrim showing all nine hold capitals">
            <title>Skyrim atlas: all nine holds</title>
            <desc>Original illustrated map with hold borders, mountain ranges, roads, rivers, and selectable capital markers.</desc>
            <defs>
              <linearGradient id="map-sea" x1="0" y1="0" x2="1" y2="1"><stop stopColor="#173846" /><stop offset=".52" stopColor="#102b35" /><stop offset="1" stopColor="#0c202a" /></linearGradient>
              <linearGradient id="map-land" x1="0" y1="0" x2=".8" y2="1"><stop stopColor="#bba57e" /><stop offset=".48" stopColor="#948365" /><stop offset="1" stopColor="#6d705d" /></linearGradient>
              <linearGradient id="map-route" x1="0" x2="1"><stop stopColor="var(--accent)" /><stop offset="1" stopColor="var(--accent-2)" /></linearGradient>
              <pattern id="map-grid" width="40" height="40" patternUnits="userSpaceOnUse"><path d="M40 0H0V40" fill="none" stroke="#a5d4cc" strokeOpacity=".06" strokeWidth=".7" /></pattern>
              <pattern id="map-grain" width="36" height="34" patternUnits="userSpaceOnUse"><circle cx="5" cy="7" r=".8" fill="#382f26" opacity=".22" /><circle cx="24" cy="17" r=".65" fill="#f5dfae" opacity=".2" /><circle cx="13" cy="29" r=".7" fill="#302c24" opacity=".17" /></pattern>
              <filter id="map-glow"><feGaussianBlur stdDeviation="4" result="blur" /><feMerge><feMergeNode in="blur" /><feMergeNode in="SourceGraphic" /></feMerge></filter>
            </defs>
            <rect width="1000" height="620" fill="url(#map-sea)" />
            <rect width="1000" height="620" fill="url(#map-grid)" />
            <path className="map-sea-label" d="M390 34h220" />
            <text className="map-sea-title" x="500" y="31" textAnchor="middle">SEA OF GHOSTS</text>
            <path className="map-landmass" d="M105 144 140 116 183 105 218 79 276 90 318 75 357 91 400 67 448 82 486 62 535 77 579 61 623 77 671 70 715 93 758 86 798 111 841 119 869 148 889 183 920 207 910 245 929 276 902 312 917 346 891 373 905 407 875 432 873 463 846 490 824 531 789 548 742 546 704 566 657 551 618 577 571 567 531 590 480 572 438 588 391 564 347 570 309 548 267 552 239 526 207 515 187 483 159 462 168 427 141 398 151 360 126 332 137 295 111 266 126 231 96 202Z" />
            <path className="map-land-edge" d="M105 144 140 116 183 105 218 79 276 90 318 75 357 91 400 67 448 82 486 62 535 77 579 61 623 77 671 70 715 93 758 86 798 111 841 119 869 148 889 183 920 207 910 245 929 276 902 312 917 346 891 373 905 407 875 432 873 463 846 490 824 531 789 548 742 546 704 566 657 551 618 577 571 567 531 590 480 572 438 588 391 564 347 570 309 548 267 552 239 526 207 515 187 483 159 462 168 427 141 398 151 360 126 332 137 295 111 266 126 231 96 202Z" />
            <path className="map-paper-grain" d="M105 144 140 116 183 105 218 79 276 90 318 75 357 91 400 67 448 82 486 62 535 77 579 61 623 77 671 70 715 93 758 86 798 111 841 119 869 148 889 183 920 207 910 245 929 276 902 312 917 346 891 373 905 407 875 432 873 463 846 490 824 531 789 548 742 546 704 566 657 551 618 577 571 567 531 590 480 572 438 588 391 564 347 570 309 548 267 552 239 526 207 515 187 483 159 462 168 427 141 398 151 360 126 332 137 295 111 266 126 231 96 202Z" />
            {mapLayers.borders && <path className="map-hold-border" d="M299 105 318 168 300 233 340 284M343 282 390 326 355 389 365 477M342 284 431 256 506 276 536 333M432 256 455 187 555 177 606 224M555 177 657 159 731 197 744 257M731 197 835 209 856 278M535 333 630 306 711 338 744 408M365 389 464 400 495 474 480 552M464 400 535 333 630 362 659 450 744 408 795 463" />}
            {mapLayers.relief && <>
              <path className="map-terrain" d="M130 169c53-51 96-39 145-3s81 31 129 10 85-33 127-6 92 26 141 3 99-15 145 17M143 208c46-30 83-21 122 9s83 39 127 17 89-49 128-22 89 47 139 21 94-25 143 7M150 438c52-41 91-34 133-2s83 30 126 10 82-22 124 6 84 31 129 11 84-29 132-2M206 486c43-27 73-26 114-2s75 33 116 15 76-14 113 10 81 31 116 17 59-26 98-13" />
              <path className="map-mountains" d="m222 286 18-30 18 30zm22 12 16-26 17 26zm23-33 21-36 21 36zm22 14 17-29 18 29zm15 52 21-38 22 38zm22-45 18-32 18 32zm358 39 19-33 19 33zm24-28 21-38 22 38zm-11-50 18-31 19 31zm-386 166 19-32 19 32zm21-29 20-36 20 36zm184-77 26-48 27 48zm19-38 19-34 20 34zm27 29 18-33 18 33zm202 179 19-34 19 34zm-27 17 17-31 17 31zm-117-77 15-27 16 27z" />
              <path className="map-forest" d="M400 407 417 382m-4 32 17-28m-4 37 18-28m-2 39 17-27m-17 39 19-30m260-22 17-26m-3 36 18-27m-2 38 17-27m-12 38 18-27m-15 39 18-29" />
              <path className="map-lake" d="M516 350c17-14 47-13 57 3 7 12-7 26-25 28-21 2-45-13-32-31zm187 39c13-9 37-6 42 5 6 13-12 23-28 21-17-2-26-16-14-26z" />
              <path className="map-river" d="M551 366c-38 25-47 47-74 63s-54 18-73 43m142-107c40-27 69-32 95-55s39-43 77-50m-246 169c-18 25-31 43-60 53" />
            </>}
            {mapLayers.roads && <>
              <path className="map-road" d="M183 160Q250 177 355 215T495 310Q545 236 570 142Q694 120 805 153L810 310 790 470Q596 499 385 485L185 390Q323 339 495 310" />
              <path className="map-route-base" d="M183 160 355 215 495 310 570 142 805 153 810 310 790 470 385 485 185 390" />
            </>}
            <path className="map-route-highlight" d={`M${waypoints[0].x} ${waypoints[0].y} Q${(waypoints[0].x + activeWaypoint.x) / 2} ${(waypoints[0].y + activeWaypoint.y) / 2 - 35} ${activeWaypoint.x} ${activeWaypoint.y}`} />
            <text className="map-hold-label" x="221" y="231">HAAFINGAR</text><text className="map-hold-label" x="348" y="264">HJAALMARCH</text>
            <text className="map-hold-label" x="573" y="217">THE PALE</text><text className="map-hold-label" x="779" y="207">WINTERHOLD</text>
            <text className="map-hold-label" x="507" y="389">WHITERUN</text><text className="map-hold-label" x="752" y="380">EASTMARCH</text>
            <text className="map-hold-label" x="222" y="337">THE REACH</text><text className="map-hold-label" x="384" y="540">FALKREATH</text>
            <text className="map-hold-label" x="755" y="529">THE RIFT</text>
            <circle className="map-route-pulse" cx={activeWaypoint.x} cy={activeWaypoint.y} r="20" filter="url(#map-glow)" />
          </svg>
          {waypoints.map((point) => <button key={point.name} type="button" className={point.name === activeWaypoint.name ? "map-stop map-stop--active" : "map-stop"} style={{ left: point.left, top: point.top }} onClick={() => onSelect(point.name)} aria-pressed={point.name === activeWaypoint.name} aria-label={`Mark ${point.name}, ${point.region}`}><span className="map-stop-pip" /><span className="map-stop-label"><strong>{point.name}</strong><small>{point.region}</small></span></button>)}
          </div>
          <div className="map-compass" aria-hidden="true"><span>N</span><Compass size={26} /><small>FIELD<br />NORTH</small></div>
          <span className="map-scale">0 ━━━━━ 50 ━━━━━ 100 mi</span>
        </div>
        <aside className="route-briefing"><span className="section-eyebrow">ACTIVE DESTINATION</span><h3>{activeWaypoint.name}</h3><p>Capital of {activeWaypoint.region}</p><div className="route-line"><span /><span /></div><div className="route-endpoints"><span><small>ROUTE ORIGIN</small><strong>Solitude</strong></span><span><small>MARKED DESTINATION</small><strong>{activeWaypoint.name}</strong></span></div><div className="route-note"><MapPin size={15} /><span>Select any capital to move the route marker. This self-contained atlas keeps map navigation independent from character data.</span></div><SkyBadge tone="accent">LOCAL PREVIEW ONLY</SkyBadge>
          <details className="hold-directory">
            <summary><span>HOLD DIRECTORY</span><small>09 CAPITALS <b>＋</b></small></summary>
            <label className="hold-search"><span>FIND A HOLD OR CAPITAL</span><input type="search" value={mapQuery} onChange={(event) => setMapQuery(event.currentTarget.value)} onKeyDown={(event) => { if (event.key === "Enter" && matchingWaypoints[0]) { event.preventDefault(); jumpToWaypoint(matchingWaypoints[0].name); } }} placeholder="e.g. The Rift or Riften" /></label>
            <div className="hold-directory-results" aria-live="polite">
              {matchingWaypoints.map((point) => <button key={point.name} type="button" className={point.name === activeWaypoint.name ? "hold-directory-entry hold-directory-entry--active" : "hold-directory-entry"} onClick={() => jumpToWaypoint(point.name)} aria-pressed={point.name === activeWaypoint.name}><span><strong>{point.name}</strong><small>{point.region}</small></span><b>{point.name === activeWaypoint.name ? "HERE" : "↗"}</b></button>)}
              {matchingWaypoints.length === 0 && <p className="hold-directory-empty">No hold or capital matches “{mapQuery}”.</p>}
            </div>
          </details>
          <div className="quest-tracker">
            <div className="quest-head"><span className="section-eyebrow">FIELD CONTRACT</span><span className="quest-live"><i /> TRACKING</span></div>
            <div className="quest-switcher" aria-label="Choose tracked sample quest">{sampleQuests.map((quest) => <button key={quest.id} type="button" className={activeQuest.id === quest.id ? "quest-switch quest-switch--active" : "quest-switch"} onClick={() => onTrackQuest(quest.id)} aria-pressed={activeQuest.id === quest.id}>{quest.region}</button>)}</div>
            <h4>{activeQuest.title}</h4><p className="quest-giver">A contract from {activeQuest.giver}</p>
            <div className="quest-progress-copy"><span>{completed.length} / {activeQuest.objectives.length} OBJECTIVES</span><strong>{questPercent}%</strong></div>
            <SkyProgress className="quest-progress-track" label={`${activeQuest.title} objectives completed`} value={completed.length} max={activeQuest.objectives.length} size="sm" />
            <div className="quest-objectives">{activeQuest.objectives.map((objective, index) => { const checked = completed.includes(index); return <button key={`${activeQuest.id}-${index}`} type="button" className={checked ? "quest-objective quest-objective--complete" : "quest-objective"} onClick={() => onToggleObjective(activeQuest.id, index)} aria-checked={checked} role="checkbox"><span className="quest-check">{checked && <Check size={11} />}</span><span>{objective}</span></button>; })}</div>
            <div className="quest-reward"><Sparkles size={13} /><span>FIELD REWARD</span><strong>{activeQuest.reward}</strong></div>
          </div>
        </aside>
      </div>
    </section>
  );
}

function percentage(value: number, max: number): number {
  if (!Number.isFinite(value) || !Number.isFinite(max) || max <= 0) return 0;
  return Math.max(0, Math.min(100, (value / max) * 100));
}

function parseRating(rating: string): { value: number; unit: string } | undefined {
  const match = /^(\d+(?:\.\d+)?)\s*(.*)$/.exec(rating.trim());
  return match ? { value: Number(match[1]), unit: match[2] || "RATING" } : undefined;
}

function formatSigned(value: number, digits = 0): string {
  const formatted = value.toFixed(digits);
  return value > 0 ? `+${formatted}` : formatted;
}

function initials(name: string): string {
  return name.trim().split(/\s+/).slice(0, 2).map((part) => part.charAt(0).toUpperCase()).join("") || "?";
}

function MetricCard({ icon, label, value, detail, percent, tone }: { icon: React.ReactNode; label: string; value: string; detail: string; percent: number; tone: string }): React.JSX.Element {
  return <article className={`metric-card metric-card--${tone}`}><div className="metric-top"><span className="metric-icon">{icon}</span><span>{label}</span></div><strong className="metric-value">{value}</strong><div className="metric-track"><span style={{ width: `${percent}%` }} /></div><small>{detail}</small></article>;
}

function ItemCard({ item, index, selected, favorite, onSelect, onFavorite }: { item: InventoryItem; index: number; selected: boolean; favorite: boolean; onSelect: () => void; onFavorite: () => void }): React.JSX.Element {
  return (
    <article className={`loot-card loot-card--${item.rarity.toLowerCase()} ${selected ? "loot-card--selected" : ""}`} style={{ "--card-index": index } as React.CSSProperties}>
      <button type="button" className="loot-card__select" onClick={onSelect} aria-pressed={selected} aria-label={`Select ${item.name}`}>
        <span className="loot-card__art"><ItemArtwork kind={item.artwork} /></span>
        <span className="loot-card__details"><span className="loot-card__rarity">{item.rarity} <i /> {item.kind}</span><strong>{item.name}</strong><small>{item.source}</small></span>
        <span className="loot-card__value"><strong>{item.value.toLocaleString()}</strong><small>SEPTIMS</small></span>
        <span className="loot-card__foot"><span><Weight size={12} /> {item.weight.toFixed(1)} WT</span><span>{item.rating}</span><span className="loot-card__arrow">↗</span></span>
      </button>
      <button type="button" className={`loot-card__favorite ${favorite ? "is-favorite" : ""}`} aria-label={favorite ? `Remove ${item.name} from favourites` : `Add ${item.name} to favourites`} aria-pressed={favorite} onClick={onFavorite}><Star size={16} fill={favorite ? "currentColor" : "none"} /></button>
    </article>
  );
}

function CompareCard({ item, label, featured = false }: { item: InventoryItem; label: string; featured?: boolean }): React.JSX.Element {
  return (
    <article className={featured ? "compare-side compare-side--featured" : "compare-side"}>
      <div className="compare-side-label"><span>{label}</span><span className={`rarity-chip rarity-chip--${item.rarity.toLowerCase()}`}>{item.rarity}</span></div>
      <div className={`compare-art relic-art--${item.artwork}`}><ItemArtwork kind={item.artwork} /></div>
      <h3>{item.name}</h3>
      <p>{item.source}</p>
      <div className="compare-side-rating"><small>{item.category === "weapons" ? "WEAPON" : item.category === "apparel" ? "APPAREL" : "ALCHEMY"}</small><strong>{item.rating}</strong></div>
    </article>
  );
}

function GearRow({ icon, slot, item, color, filled, onClick }: { icon: React.ReactNode; slot: string; item: string; color: string; filled: boolean; onClick: () => void }): React.JSX.Element {
  return <button type="button" className={`gear-row gear-row--${color}`} onClick={onClick} aria-label={`Change ${slot.toLowerCase()} slot, currently ${item}`}><span className="gear-icon">{icon}</span><span className="gear-slot">{slot}</span><strong>{item}</strong><span className={filled ? "gear-state" : "gear-state gear-state--empty"}>{filled ? <Check size={12} /> : <span>+</span>}</span></button>;
}

function ItemArtwork({ kind }: { kind: Artwork }): React.JSX.Element {
  return (
    <svg className={`item-artwork item-artwork--${kind}`} viewBox="0 0 80 80" role="img" aria-label={`${kind} illustration`}>
      <defs>
        <linearGradient id={`metal-${kind}`} x1="0" x2="1" y1="0" y2="1"><stop stopColor="#f4e5bd" /><stop offset=".42" stopColor="#a8a18d" /><stop offset="1" stopColor="#4a4842" /></linearGradient>
        <linearGradient id={`brass-${kind}`} x1="0" x2="1"><stop stopColor="#f1cd78" /><stop offset="1" stopColor="#8a572c" /></linearGradient>
        <radialGradient id={`glow-${kind}`}><stop stopColor="#f6d179" stopOpacity=".35" /><stop offset="1" stopColor="#f6d179" stopOpacity="0" /></radialGradient>
      </defs>
      <circle cx="40" cy="40" r="36" fill={`url(#glow-${kind})`} /><circle cx="40" cy="40" r="31" fill="none" stroke="#c69c58" strokeOpacity=".26" />
      {kind === "sword" && <g strokeLinecap="round" strokeLinejoin="round"><path d="M48 12 58 22 35 49 29 43Z" fill={`url(#metal-${kind})`} stroke="#f0d596" strokeWidth="1.2"/><path d="m28 41 11 11M23 52l8 8M21 59l-4 4M43 32l5 5" fill="none" stroke={`url(#brass-${kind})`} strokeWidth="3"/><path d="m51 17 4 4-17 20" fill="none" stroke="#fff4d7" strokeOpacity=".55"/></g>}
      {kind === "shield" && <g strokeLinejoin="round"><path d="M40 12 59 19v15c0 14-9 24-19 31-10-7-19-17-19-31V19Z" fill={`url(#metal-${kind})`} stroke={`url(#brass-${kind})`} strokeWidth="2"/><path d="M40 19 52 24v10c0 9-5 16-12 22-7-6-12-13-12-22V24Z" fill="#30251b" stroke="#f1d394" strokeOpacity=".8"/><path d="M40 23v28m-8-14h16" stroke="#e2bd70" strokeWidth="1.5"/></g>}
      {kind === "potion" && <g strokeLinejoin="round"><path d="M32 15h16v9l-3 5v5c9 5 14 11 13 21-1 9-8 12-18 12s-17-3-18-12c-1-10 4-16 13-21v-5l-3-5Z" fill="#512e24" stroke={`url(#brass-${kind})`} strokeWidth="2"/><path d="M25 49c8-5 22-5 30 0l1 7c0 5-6 8-15 8s-15-3-15-8Z" fill="#d48243" fillOpacity=".85"/><path d="M34 15h12v4H34Z" fill={`url(#metal-${kind})`} stroke="#e8c57e"/><path d="m36 39 8 7m2-9 4 4" stroke="#f5dfa0" strokeWidth="1.2" strokeLinecap="round"/></g>}
      {kind === "ring" && <g><ellipse cx="40" cy="45" rx="19" ry="12" transform="rotate(-24 40 45)" fill="none" stroke={`url(#brass-${kind})`} strokeWidth="7"/><ellipse cx="40" cy="45" rx="14" ry="8" transform="rotate(-24 40 45)" fill="none" stroke="#372c20" strokeWidth="2"/><path d="m39 24 6-10 8 8-9 7Z" fill={`url(#metal-${kind})`} stroke="#f2d88f" strokeWidth="1.5"/><path d="m44 24 4-5" stroke="#fff2c8" strokeWidth="1.3"/></g>}
      {kind === "bow" && <g fill="none" strokeLinecap="round"><path d="M25 15c18 8 31 22 30 38-.2 6-3 11-8 14" stroke={`url(#brass-${kind})`} strokeWidth="6"/><path d="M25 15c-4 20 2 40 22 52" stroke={`url(#metal-${kind})`} strokeWidth="3"/><path d="m22 18 38 43M40 39h25m-7-6 7 6-7 6" stroke="#e9dfc5" strokeWidth="1.5"/><path d="M28 23 50 54" stroke="#fff1cd" strokeOpacity=".55"/></g>}
      <path d="M40 5v4m0 62v4M5 40h4m62 0h4" stroke="#e7c780" strokeOpacity=".65" />
    </svg>
  );
}
