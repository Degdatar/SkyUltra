export const skyUltraEquipmentSlots = ['head','body','hands','feet','neck','ring','mainHand','offHand','ammo','cape','lantern','auto'] as const;
export type SkyUltraEquipmentSlot = typeof skyUltraEquipmentSlots[number];
export const skyUltraGameTopics = ['vitals','equipment','inventory','effects','quests','followers'] as const;
export type SkyUltraGameTopic = typeof skyUltraGameTopics[number];
export const skyUltraGameSubscriptionMethods = ['game.subscribe','game.unsubscribe'] as const;

export const skyUltraItemActions = ['drop','activate','read','learn','charge','drink','eat'] as const;
export type SkyUltraItemAction = typeof skyUltraItemActions[number];
export type SkyUltraArmorWeightClass = 'heavy' | 'light' | 'clothing' | 'shield';
export interface ItemEnchantment { name: string; description?: string; magnitude?: number; duration?: number; }
export interface Item {
  id: string;
  name: string;
  category: string;
  count: number;
  value: number;
  baseValue?: number;
  weight: number;
  equipped: boolean;
  equipSlots?: SkyUltraEquipmentSlot[];
  /** Weapon or ammo damage rating (tempered & skill-adjusted when available). */
  damage?: number;
  baseDamage?: number;
  /** Human-readable weapon / ammo class (e.g., "One-Handed Sword", "Bow", "Greatsword", "Arrow"). */
  weaponType?: string;
  speed?: number;
  reach?: number;
  critDamage?: number;
  /** Armor rating (tempered & skill-adjusted when available). */
  armorRating?: number;
  baseArmorRating?: number;
  /** Armor weight classification ("heavy", "light", "clothing", or "shield"). */
  armorWeightClass?: SkyUltraArmorWeightClass;
  /** Human-readable armor classification ("Heavy Armor", "Light Armor", "Clothing", "Shield"). */
  armorTypeLabel?: string;
  enchanted?: boolean;
  enchantmentName?: string;
  enchantmentCharge?: number;
  enchantments?: ItemEnchantment[];
  useAction?: SkyUltraItemAction;
  canCharge?: boolean;
  canDrop?: boolean;
  teachesSpell?: string;
  favorite?: boolean;
  stolen?: boolean;
}
export interface QuestObjective { index: number; text: string; completed: boolean; failed: boolean; }
export interface QuestSummary { id: string; formId: string; name: string; stage: number; active: boolean; type: string; objectives: QuestObjective[]; }
export interface Npc { baseFormId: string; name: string; race: string; sex: string; referenceId?: string; distance?: number; level?: number; follower?: boolean; dead?: boolean; health?: number; healthMax?: number; armorRating?: number; equipment?: Player['equipment']; }
export type SkyUltraCompassMarkerKind =
  | 'home'
  | 'inn'
  | 'castle'
  | 'blacksmith'
  | 'vendor'
  | 'quest'
  | 'questDoor'
  | 'customMarker'
  | 'follower'
  | 'city'
  | 'temple'
  | 'dungeon'
  | 'cave'
  | 'ruin'
  | 'dragon'
  | 'stone'
  | 'camp'
  | 'door'
  | 'enemy';
export interface SkyUltraCompassMarker {
  id: string;
  name: string;
  subtitle?: string;
  kind: SkyUltraCompassMarkerKind;
  categoryLabel?: string;
  angle: number;
  distance: number;
  discovered?: boolean;
  hostile?: boolean;
  elevation?: 'above' | 'below' | 'level';
  referenceId?: string;
  baseFormId?: string;
  level?: number;
  healthPct?: number;
}
export interface Player {
  available: boolean; playerName: string; level: number; location?: string; carryWeight?: { current: number; max: number };
  sequence?: number; snapshotScope?: 'full' | 'hud';
  heading?: number; compassMarkers?: SkyUltraCompassMarker[]; calendar?: { hour: number; minute: number; day: number; month: string; year: number } | null;
  health: { current: number; max: number }; stamina: { current: number; max: number }; magicka: { current: number; max: number }; armorRating: number; gold: number;
  equipment: { head: Item | null; body: Item | null; hands: Item | null; feet: Item | null; mainHand: Item | null; offHand: Item | null; neck?: Item | null; ring?: Item | null; ammo?: Item | null; cape?: Item | null; lantern?: Item | null };
  inventory: Item[]; inventoryTypeCount: number; inventoryItemCount: number; inventoryTruncated: boolean;
  resistances?: Array<{ id: string; label: string; value: number }>;
  skills?: Array<{ id: string; label: string; group: string; value: number }>;
  activeEffects?: Array<{ id: string; name: string; magnitude: number; duration: number; remaining: number; hostile: boolean }>; activeEffectCount?: number;
  quests?: QuestSummary[]; questCount?: number;
  followers?: Npc[]; followerCount?: number;
}
export interface ActorResponse { available: boolean; radius: number; totalNearby: number; results: Npc[]; }
export interface SearchResponse { available: boolean; totalMatches: number; results: Npc[]; }

import { getSkyWebView, type JsonValue, type SkyUltraRequestOptions, type SkyWebViewAPI } from './index.js';
export interface SkyUltraInventoryOptions { query?: string; category?: string; equipment?: 'all'|'equipped'|'carried'; sort?: 'name'|'value'|'weight'|'count'; offset?: number; limit?: number; }
export interface SkyUltraInventoryPage { available: boolean; results: Item[]; totalTypes: number; totalMatches: number; offset: number; categories: string[]; }
export interface SkyUltraEquipmentResult { accepted: boolean; formId?: string; slot?: SkyUltraEquipmentSlot; equipment?: Player['equipment']; message?: string; }
export interface SkyUltraItemActionOptions { formId: number | string; action: SkyUltraItemAction; count?: number; openNative?: boolean; }
export interface SkyUltraItemActionResult { accepted: boolean; action?: SkyUltraItemAction; formId?: string; equipment?: Player['equipment']; message?: string; bookTitle?: string; bookText?: string; bookPages?: string[]; bookImages?: string[]; bookStyle?: 'book' | 'note' | 'map'; }
export interface SkyUltraItemThumbnail { available: boolean; dataUrl?: string; message?: string; }
export interface SkyUltraBarterSnapshot {
  available: boolean;
  vendorReferenceId?: string;
  vendorBaseFormId?: string;
  vendorName?: string;
  vendorGold?: number;
  vendorItems: Item[];
  vendorTotalTypes?: number;
  vendorCategories?: string[];
  playerName?: string;
  playerGold?: number;
  playerCarryWeight?: { current: number; max: number };
  playerItems: Item[];
  playerTotalTypes?: number;
  playerCategories?: string[];
  message?: string;
}
export interface SkyUltraBarterTransactionOptions {
  action: 'buy' | 'sell';
  formId: number | string;
  count?: number;
  vendorReferenceId?: number | string;
  allowPartialGold?: boolean;
}
export interface SkyUltraBarterTransactionResult {
  accepted: boolean;
  requiresConfirmation?: boolean;
  availableVendorGold?: number;
  action?: 'buy' | 'sell';
  formId?: string;
  count?: number;
  unitPrice?: number;
  totalGold?: number;
  playerGold?: number;
  vendorGold?: number;
  message?: string;
  barter?: SkyUltraBarterSnapshot;
}
export interface SkyUltraContainerSnapshot {
  available: boolean;
  isMannequin?: boolean;
  containerRefId?: string;
  containerBaseFormId?: string;
  containerName?: string;
  containerType?: 'mannequin' | 'actor' | 'corpse' | 'container';
  containerMode?: 'loot' | 'steal' | 'pickpocket' | 'npc';
  containerItems: Item[];
  containerTotalTypes?: number;
  containerCategories?: string[];
  playerName?: string;
  playerGold?: number;
  playerCarryWeight?: { current: number; max: number };
  playerItems: Item[];
  playerTotalTypes?: number;
  playerCategories?: string[];
  equipment?: Player['equipment'] | null;
  armorRating?: number;
  actor?: Npc | null;
  message?: string;
}
export interface SkyUltraContainerSnapshotOptions {
  containerRefId?: number | string;
  query?: string;
  category?: string;
  sort?: 'name' | 'value' | 'weight' | 'count';
}
export type SkyUltraContainerTransferMode = 'take' | 'store' | 'takeAll' | 'storeAll' | 'steal' | 'plant' | 'stealAll' | 'plantAll';
export interface SkyUltraContainerTransferOptions {
  mode?: SkyUltraContainerTransferMode;
  action?: SkyUltraContainerTransferMode;
  formId?: number | string;
  count?: number;
  containerRefId?: number | string;
  includeEquipped?: boolean;
  category?: string;
  equip?: boolean;
  slot?: SkyUltraEquipmentSlot;
}
export interface SkyUltraContainerTransferResult {
  accepted: boolean;
  caught?: boolean;
  closeMenu?: boolean;
  mode?: SkyUltraContainerTransferMode;
  formId?: string;
  count?: number;
  movedTypes?: number;
  message?: string;
  container?: SkyUltraContainerSnapshot;
}
export const skyUltraInventoryMethods = ['game.getInventory','game.getItemThumbnail','game.equipItem','game.unequipItem','game.itemAction','game.getBarterSnapshot','game.barterTransaction','game.getContainerSnapshot','game.containerTransfer'] as const;

/** Spell book categories, matching Skyrim's Magic menu tabs. */
export const skyUltraSpellCategories = ['alteration','conjuration','destruction','illusion','restoration','powers','shouts','other'] as const;
export type SkyUltraSpellCategory = typeof skyUltraSpellCategories[number];
export type SkyUltraSpellHand = 'left' | 'right' | 'both' | 'voice';
export interface SkyUltraSpellEffect { name: string; description?: string; magnitude?: number; duration?: number; area?: number; }
export interface SkyUltraShoutWord { word: string; translation?: string; unlocked: boolean; }
export interface SkyUltraSpell {
  id: string;
  name: string;
  /** spell = castable in hands, power / lesserPower / shout = voice slot. */
  kind: 'spell' | 'power' | 'lesserPower' | 'shout';
  category: SkyUltraSpellCategory;
  school: 'alteration' | 'conjuration' | 'destruction' | 'illusion' | 'restoration' | 'none';
  /** Novice / Apprentice / Adept / Expert / Master, when the spell has a skill requirement. */
  level?: string;
  skillLevel?: number;
  /** Magicka cost (per second for concentration spells). */
  cost: number;
  castingType: 'fireAndForget' | 'concentration' | 'constantEffect' | 'scroll';
  delivery: 'self' | 'touch' | 'aimed' | 'targetActor' | 'targetLocation';
  /** Visual element hint derived from the costliest effect (fire, frost, shock, heal, ward, summon, ...). */
  element?: string;
  description?: string;
  effects: SkyUltraSpellEffect[];
  equippedLeft: boolean;
  equippedRight: boolean;
  equippedVoice: boolean;
  favorite: boolean;
  /** Hands this spell can be equipped to. Voice powers/shouts return ['voice']. */
  hands: SkyUltraSpellHand[];
  twoHanded?: boolean;
  chargeTime?: number;
  cooldown?: number;
  words?: SkyUltraShoutWord[];
}
export interface SkyUltraSpellBook {
  available: boolean;
  magicka?: { current: number; max: number };
  equipped: { left: string | null; right: string | null; voice: string | null };
  categories: Array<{ id: SkyUltraSpellCategory; label: string; count: number }>;
  spells: SkyUltraSpell[];
  message?: string;
}
export interface SkyUltraSpellEquipResult { accepted: boolean; formId?: string; hand?: SkyUltraSpellHand; message?: string; equipped?: SkyUltraSpellBook['equipped']; }
export interface SkyUltraSpellRenameResult { accepted: boolean; formId?: string; oldName?: string; name?: string; message?: string; }
export type SkyUltraDeletableMagicKind = 'spell' | 'power' | 'lesserPower' | 'shout' | 'effect';
export interface SkyUltraSpellDeleteResult { accepted: boolean; formId?: string; kind?: SkyUltraDeletableMagicKind; message?: string; equipped?: SkyUltraSpellBook['equipped']; }
export type SkyUltraFavoriteKind = 'item' | 'spell';
export interface SkyUltraFavoriteResult { accepted: boolean; formId?: string; kind?: SkyUltraFavoriteKind; favorite?: boolean; message?: string; }
export interface SkyUltraFavoriteItem extends Item { favorite: true; }
export interface SkyUltraFavorites {
  available: boolean;
  items: SkyUltraFavoriteItem[];
  spells: SkyUltraSpell[];
  equipped: SkyUltraSpellBook['equipped'];
  message?: string;
}
export const skyUltraMagicMethods = ['game.getSpells','game.equipSpell','game.renameSpell','game.deleteSpell','game.getFavorites','game.setFavorite'] as const;

export interface SkyUltraGameUpdateEvent {
  available: boolean;
  sequence: number;
  snapshotScope: 'full' | 'hud';
  changedTopics: SkyUltraGameTopic[];
  delta: Partial<Player>;
  snapshot: Player;
}

export interface SkyUltraGameSubscriptionOptions {
  topics?: readonly SkyUltraGameTopic[];
  scope?: 'full' | 'hud';
  intervalMs?: number;
  emitInitial?: boolean;
}

export interface SkyUltraGameSubscription {
  unsubscribe(): void;
  getSnapshot(): Player | null;
  refresh(): Promise<Player | null>;
  readonly topics: readonly SkyUltraGameTopic[];
  readonly sequence: number;
}

interface RawHostGameUpdate {
  available?: boolean;
  sequence?: number;
  snapshotScope?: 'full' | 'hud';
  changedTopics?: string[];
  delta?: Partial<Player>;
  snapshot?: Player;
}

interface RawHostSubscribeResponse {
  subscribed?: boolean;
  sequence?: number;
  snapshotScope?: 'full' | 'hud';
  topics?: string[];
  intervalMs?: number;
  update?: RawHostGameUpdate;
  snapshot?: Player;
}

const EMPTY_EQUIPMENT: Player['equipment'] = {
  head: null, body: null, hands: null, feet: null, neck: null, ring: null,
  cape: null, lantern: null, ammo: null, mainHand: null, offHand: null,
};

function createEmptyPlayerSnapshot(scope: 'full' | 'hud' = 'full'): Player {
  return {
    available: false,
    snapshotScope: scope,
    playerName: 'Dragonborn',
    level: 1,
    health: { current: 0, max: 0 },
    stamina: { current: 0, max: 0 },
    magicka: { current: 0, max: 0 },
    armorRating: 0,
    gold: 0,
    equipment: { ...EMPTY_EQUIPMENT },
    inventory: [],
    inventoryTypeCount: 0,
    inventoryItemCount: 0,
    inventoryTruncated: false,
    resistances: [],
    skills: [],
    activeEffects: [],
    activeEffectCount: 0,
    quests: [],
    questCount: 0,
    followers: [],
    followerCount: 0,
  };
}

function normalizeTopics(topics?: readonly SkyUltraGameTopic[]): SkyUltraGameTopic[] {
  if (!topics || topics.length === 0) return [...skyUltraGameTopics];
  const unique: SkyUltraGameTopic[] = [];
  for (const topic of topics) {
    if ((skyUltraGameTopics as readonly string[]).includes(topic) && !unique.includes(topic)) {
      unique.push(topic);
    }
  }
  return unique.length > 0 ? unique : [...skyUltraGameTopics];
}

function topicSliceForPlayer(player: Player, topic: SkyUltraGameTopic): Partial<Player> {
  switch (topic) {
    case 'vitals':
      return {
        playerName: player.playerName,
        level: player.level,
        heading: player.heading,
        compassMarkers: player.compassMarkers,
        calendar: player.calendar,
        health: player.health,
        stamina: player.stamina,
        magicka: player.magicka,
        armorRating: player.armorRating,
        gold: player.gold,
        location: player.location,
        carryWeight: player.carryWeight,
        resistances: player.resistances,
        skills: player.skills,
      };
    case 'equipment':
      return { equipment: player.equipment };
    case 'inventory':
      return {
        inventory: player.inventory,
        inventoryTypeCount: player.inventoryTypeCount,
        inventoryItemCount: player.inventoryItemCount,
        inventoryTruncated: player.inventoryTruncated,
      };
    case 'effects':
      return {
        activeEffects: player.activeEffects,
        activeEffectCount: player.activeEffectCount,
      };
    case 'quests':
      return {
        quests: player.quests,
        questCount: player.questCount,
      };
    case 'followers':
      return {
        followers: player.followers,
        followerCount: player.followerCount,
      };
  }
}

export function applySkyUltraGameUpdate(
  previous: Player | null,
  rawUpdate: RawHostGameUpdate | Player,
): {
  snapshot: Player;
  changedTopics: SkyUltraGameTopic[];
  delta: Partial<Player>;
  sequence: number;
  snapshotScope: 'full' | 'hud';
} {
  const isEnvelope =
    typeof rawUpdate === 'object' &&
    rawUpdate !== null &&
    ('changedTopics' in rawUpdate || 'delta' in rawUpdate || 'snapshot' in rawUpdate);

  const envelope: RawHostGameUpdate = isEnvelope
    ? (rawUpdate as RawHostGameUpdate)
    : {
        available: (rawUpdate as Player).available,
        sequence: (rawUpdate as Player).sequence,
        snapshotScope: (rawUpdate as Player).snapshotScope ?? 'full',
        snapshot: rawUpdate as Player,
      };

  const scope: 'full' | 'hud' =
    envelope.snapshotScope === 'hud' || envelope.snapshot?.snapshotScope === 'hud'
      ? 'hud'
      : previous?.snapshotScope ?? 'full';
  const base = previous ? { ...previous, equipment: { ...previous.equipment } } : createEmptyPlayerSnapshot(scope);
  const nextSequence =
    typeof envelope.sequence === 'number' && Number.isFinite(envelope.sequence)
      ? envelope.sequence
      : (previous?.sequence ?? 0) + 1;

  if (envelope.available === false || (envelope.snapshot && envelope.snapshot.available === false)) {
    const unavailable: Player = {
      ...base,
      available: false,
      sequence: nextSequence,
      snapshotScope: scope,
    };
    const changed: SkyUltraGameTopic[] = previous?.available === false ? [] : [...skyUltraGameTopics];
    return {
      snapshot: unavailable,
      changedTopics: changed,
      delta: { available: false },
      sequence: nextSequence,
      snapshotScope: scope,
    };
  }

  let next: Player = {
    ...base,
    ...(envelope.snapshot ?? {}),
    ...(envelope.delta ?? {}),
    available: true,
    sequence: nextSequence,
    snapshotScope: scope,
  };
  if (envelope.snapshot?.equipment || envelope.delta?.equipment) {
    next.equipment = {
      ...EMPTY_EQUIPMENT,
      ...(base.equipment ?? {}),
      ...(envelope.snapshot?.equipment ?? {}),
      ...(envelope.delta?.equipment ?? {}),
    };
  }

  let changedTopics: SkyUltraGameTopic[] = [];
  if (Array.isArray(envelope.changedTopics) && envelope.changedTopics.length > 0) {
    changedTopics = normalizeTopics(
      envelope.changedTopics.filter((t): t is SkyUltraGameTopic =>
        (skyUltraGameTopics as readonly string[]).includes(t),
      ),
    );
  } else if (!previous || !previous.available) {
    changedTopics = [...skyUltraGameTopics];
  } else {
    for (const topic of skyUltraGameTopics) {
      const beforeSig = JSON.stringify(topicSliceForPlayer(previous, topic));
      const afterSig = JSON.stringify(topicSliceForPlayer(next, topic));
      if (beforeSig !== afterSig) {
        changedTopics.push(topic);
      }
    }
  }

  const delta: Partial<Player> = envelope.delta ? { ...envelope.delta } : {};
  if (!envelope.delta) {
    for (const topic of changedTopics) {
      Object.assign(delta, topicSliceForPlayer(next, topic));
    }
  }

  return {
    snapshot: next,
    changedTopics,
    delta,
    sequence: nextSequence,
    snapshotScope: scope,
  };
}

interface SubscriberEntry {
  id: number;
  topics: SkyUltraGameTopic[];
  scope: 'full' | 'hud';
  intervalMs: number;
  listener: (event: SkyUltraGameUpdateEvent) => void;
}

class SharedGameFeedHub {
  private readonly bridge: SkyWebViewAPI;
  private readonly subscribers = new Map<number, SubscriberEntry>();
  private nextSubscriberId = 1;
  private snapshot: Player | null = null;
  private sequence = 0;
  private activeScope: 'full' | 'hud' = 'hud';
  private activeTopics: SkyUltraGameTopic[] = [];
  private activeIntervalMs = 750;
  private hostSubscribed = false;
  private hostUnsupported = false;
  private subscribeInFlight: Promise<void> | null = null;
  private refreshInFlight: Promise<Player | null> | null = null;
  private offUpdateEvent: (() => void) | null = null;
  private offVisibilityEvent: (() => void) | null = null;
  private removeDocVisibility: (() => void) | null = null;
  private fallbackTimer: ReturnType<typeof setInterval> | null = null;
  private visible = true;
  private needsCatchup = false;

  constructor(bridge: SkyWebViewAPI) {
    this.bridge = bridge;
  }

  getSnapshot(): Player | null {
    return this.snapshot;
  }

  getSequence(): number {
    return this.sequence;
  }

  applyOptimisticEquipment(equipment: Player['equipment']): void {
    if (!this.snapshot) return;
    this.ingestUpdate({
      available: this.snapshot.available,
      sequence: this.sequence + 1,
      snapshotScope: this.snapshot.snapshotScope ?? this.activeScope,
      changedTopics: ['equipment'],
      delta: { equipment },
    });
  }

  subscribe(
    listener: (event: SkyUltraGameUpdateEvent) => void,
    options: SkyUltraGameSubscriptionOptions = {},
  ): SkyUltraGameSubscription {
    const id = this.nextSubscriberId++;
    const topics = normalizeTopics(options.topics);
    const scope: 'full' | 'hud' = options.scope === 'hud' ? 'hud' : 'full';
    const intervalMs = Math.max(16, Math.min(5000, Math.round(options.intervalMs ?? (scope === 'hud' ? 16 : 750))));
    const emitInitial = options.emitInitial !== false;

    const wasEmpty = this.subscribers.size === 0;
    this.subscribers.set(id, { id, topics, scope, intervalMs, listener });

    if (wasEmpty) {
      this.attachBridgeListeners();
    }

    const requiresReconfigure = this.updateDesiredConfiguration();

    if (emitInitial && this.snapshot && (!requiresReconfigure || this.snapshot.snapshotScope === 'full' || scope === 'hud')) {
      const initialDelta: Partial<Player> = {};
      for (const topic of topics) {
        Object.assign(initialDelta, topicSliceForPlayer(this.snapshot, topic));
      }
      queueMicrotask(() => {
        if (!this.subscribers.has(id) || !this.snapshot) return;
        listener({
          available: this.snapshot.available,
          sequence: this.sequence,
          snapshotScope: this.snapshot.snapshotScope ?? this.activeScope,
          changedTopics: [...topics],
          delta: initialDelta,
          snapshot: this.snapshot,
        });
      });
    }

    if (wasEmpty || requiresReconfigure || !this.snapshot) {
      void this.syncHostSubscription();
    }

    return {
      unsubscribe: () => {
        if (!this.subscribers.delete(id)) return;
        if (this.subscribers.size === 0) {
          this.teardown();
        } else if (this.updateDesiredConfiguration()) {
          void this.syncHostSubscription();
        }
      },
      getSnapshot: () => this.snapshot,
      refresh: () => this.refresh(),
      topics,
      get sequence() {
        return this.getSnapshot()?.sequence ?? 0;
      },
    };
  }

  async refresh(): Promise<Player | null> {
    if (this.refreshInFlight) return this.refreshInFlight;
    const scope = this.activeScope;
    this.refreshInFlight = this.bridge
      .request<Player>('game.getPlayerSnapshot', { scope })
      .then((player) => {
        if (player && typeof player === 'object') {
          this.ingestUpdate(player);
        }
        return this.snapshot;
      })
      .catch(() => this.snapshot)
      .finally(() => {
        this.refreshInFlight = null;
      });
    return this.refreshInFlight;
  }

  private computeVisibility(): boolean {
    const bridgeVis = this.bridge.getVisibility?.();
    if (bridgeVis && bridgeVis.visible === false) return false;
    if (typeof document !== 'undefined' && document.visibilityState === 'hidden') return false;
    return true;
  }

  private attachBridgeListeners(): void {
    this.visible = this.computeVisibility();
    this.offUpdateEvent = this.bridge.on<RawHostGameUpdate>('skyultra.game.update', (payload) => {
      if (!this.visible) {
        this.needsCatchup = true;
        return;
      }
      if (payload && typeof payload === 'object') {
        this.ingestUpdate(payload);
      }
    });
    this.offVisibilityEvent = this.bridge.on<{ visible?: boolean }>('skyultra.visibility', (payload) => {
      const nextVisible = payload?.visible !== false && this.computeVisibility();
      this.handleVisibilityChange(nextVisible);
    });
    if (typeof document !== 'undefined' && typeof document.addEventListener === 'function') {
      const onVis = () => this.handleVisibilityChange(this.computeVisibility());
      document.addEventListener('visibilitychange', onVis);
      this.removeDocVisibility = () => document.removeEventListener('visibilitychange', onVis);
    }
  }

  private handleVisibilityChange(nextVisible: boolean): void {
    if (this.visible === nextVisible) return;
    this.visible = nextVisible;
    if (!nextVisible) {
      this.needsCatchup = true;
      this.stopFallbackTimer();
      return;
    }
    if (this.subscribers.size === 0) return;
    if (this.hostUnsupported) {
      this.startFallbackTimer();
    }
    if (this.needsCatchup) {
      this.needsCatchup = false;
      void this.refresh();
    }
  }

  private updateDesiredConfiguration(): boolean {
    let desiredScope: 'full' | 'hud' = 'hud';
    let desiredInterval = 5000;
    const topicSet = new Set<SkyUltraGameTopic>();

    for (const sub of this.subscribers.values()) {
      if (sub.scope === 'full') desiredScope = 'full';
      if (sub.intervalMs < desiredInterval) desiredInterval = sub.intervalMs;
      for (const t of sub.topics) topicSet.add(t);
    }
    const desiredTopics = skyUltraGameTopics.filter((t) => topicSet.has(t));
    const changed =
      desiredScope !== this.activeScope ||
      desiredInterval !== this.activeIntervalMs ||
      desiredTopics.length !== this.activeTopics.length ||
      desiredTopics.some((t, idx) => t !== this.activeTopics[idx]);

    this.activeScope = desiredScope;
    this.activeIntervalMs = desiredInterval;
    this.activeTopics = desiredTopics;
    return changed;
  }

  private async syncHostSubscription(): Promise<void> {
    if (this.subscribers.size === 0) return;
    if (this.hostUnsupported) {
      this.startFallbackTimer();
      await this.refresh();
      return;
    }
    if (this.subscribeInFlight) return this.subscribeInFlight;

    const params: JsonValue = {
      topics: [...this.activeTopics],
      scope: this.activeScope,
      intervalMs: this.activeIntervalMs,
    };

    this.subscribeInFlight = this.bridge
      .request<RawHostSubscribeResponse>('game.subscribe', params)
      .then((res) => {
        if (res && res.subscribed) {
          this.hostSubscribed = true;
          this.stopFallbackTimer();
          if (res.update && typeof res.update === 'object') {
            this.ingestUpdate(res.update);
          } else if (res.snapshot && typeof res.snapshot === 'object') {
            this.ingestUpdate(res.snapshot);
          }
          return;
        }
        this.hostUnsupported = true;
        this.startFallbackTimer();
        return this.refresh().then(() => undefined);
      })
      .catch(() => {
        this.hostUnsupported = true;
        this.startFallbackTimer();
        return this.refresh().then(() => undefined);
      })
      .finally(() => {
        this.subscribeInFlight = null;
      });

    return this.subscribeInFlight;
  }

  private ingestUpdate(rawUpdate: RawHostGameUpdate | Player): void {
    const hadPrevious = this.snapshot !== null;
    const applied = applySkyUltraGameUpdate(this.snapshot, rawUpdate);
    this.snapshot = applied.snapshot;
    this.sequence = applied.sequence;

    if (applied.changedTopics.length === 0 && hadPrevious) {
      return;
    }

    for (const sub of [...this.subscribers.values()]) {
      const relevant = applied.changedTopics.filter((topic) => sub.topics.includes(topic));
      if (relevant.length === 0 && hadPrevious) continue;
      const effectiveTopics = relevant.length > 0 ? relevant : sub.topics;
      const subDelta: Partial<Player> = {};
      for (const topic of effectiveTopics) {
        Object.assign(subDelta, topicSliceForPlayer(applied.snapshot, topic));
      }
      try {
        sub.listener({
          available: applied.snapshot.available,
          sequence: applied.sequence,
          snapshotScope: applied.snapshotScope,
          changedTopics: effectiveTopics,
          delta: subDelta,
          snapshot: applied.snapshot,
        });
      } catch (err) {
        console.error('SkyUltra game subscription listener failed:', err);
      }
    }
  }

  private startFallbackTimer(): void {
    this.stopFallbackTimer();
    if (!this.visible || this.subscribers.size === 0) return;
    this.fallbackTimer = setInterval(() => {
      if (!this.visible || this.subscribers.size === 0) return;
      void this.refresh();
    }, this.activeIntervalMs);
  }

  private stopFallbackTimer(): void {
    if (this.fallbackTimer !== null) {
      clearInterval(this.fallbackTimer);
      this.fallbackTimer = null;
    }
  }

  private teardown(): void {
    this.stopFallbackTimer();
    this.offUpdateEvent?.();
    this.offUpdateEvent = null;
    this.offVisibilityEvent?.();
    this.offVisibilityEvent = null;
    this.removeDocVisibility?.();
    this.removeDocVisibility = null;
    if (this.hostSubscribed) {
      this.hostSubscribed = false;
      void this.bridge.request('game.unsubscribe', {}).catch(() => undefined);
    }
    this.activeTopics = [];
  }
}

const sharedFeedHubs = new WeakMap<SkyWebViewAPI, SharedGameFeedHub>();

function getSharedFeedHub(bridge: SkyWebViewAPI): SharedGameFeedHub {
  let hub = sharedFeedHubs.get(bridge);
  if (!hub) {
    hub = new SharedGameFeedHub(bridge);
    sharedFeedHubs.set(bridge, hub);
  }
  return hub;
}

export function subscribeSkyUltraGame(
  bridge: SkyWebViewAPI,
  listener: (event: SkyUltraGameUpdateEvent) => void,
  options?: SkyUltraGameSubscriptionOptions,
): SkyUltraGameSubscription {
  return getSharedFeedHub(bridge).subscribe(listener, options);
}

export function getSharedGameSnapshot(bridge: SkyWebViewAPI = getSkyWebView()): Player | null {
  return sharedFeedHubs.get(bridge)?.getSnapshot() ?? null;
}

function inventoryFormId(value:number|string):number|string {
  const base=typeof value==='string'?value.split(':')[0]!:value;
  const parsed=typeof base==='number'?base:/^(?:0[xX][0-9a-fA-F]+|[0-9]+)$/.test(base)?Number(base):NaN;
  if(!Number.isInteger(parsed)||parsed<=0||parsed>0xffffffff)throw new TypeError('A positive 32-bit item form ID is required.');
  return value;
}
export function createSkyUltraGame(bridge:SkyWebViewAPI=getSkyWebView()) {
  const equipment=(method:string,formId:number|string,slot:SkyUltraEquipmentSlot,options?:SkyUltraRequestOptions)=>{
    if(!skyUltraEquipmentSlots.includes(slot))throw new TypeError('Unknown equipment slot.');
    const checkedFormId = inventoryFormId(formId);
    return bridge.request<SkyUltraEquipmentResult>(method,{formId:checkedFormId,slot},options).then((res)=>{
      if (res?.accepted && res.equipment) {
        sharedFeedHubs.get(bridge)?.applyOptimisticEquipment(res.equipment);
      }
      return res;
    });
  };
  return {
    player: (scope:'full'|'hud'='full', options?:SkyUltraRequestOptions)=>bridge.request<Player>('game.getPlayerSnapshot',{scope},options),
    subscribe: (listener:(event:SkyUltraGameUpdateEvent)=>void, options?:SkyUltraGameSubscriptionOptions)=>subscribeSkyUltraGame(bridge, listener, options),
    getCachedPlayer: ()=>getSharedGameSnapshot(bridge),
    nearby: (radius=6000,options?:SkyUltraRequestOptions)=>bridge.request<ActorResponse>('game.getNearbyNpcs',{radius},options),
    search: (query:string,options?:SkyUltraRequestOptions)=>bridge.request<SearchResponse>('game.searchNpcs',{query,limit:50},options),
    inventory: (input:SkyUltraInventoryOptions={},options?:SkyUltraRequestOptions)=>{
      const {query='',category='all',equipment='all',sort='name',offset=0,limit=64}=input;
      if(query.length>256||!['all','equipped','carried'].includes(equipment)||!['name','value','weight','count'].includes(sort)||!Number.isInteger(offset)||offset<0||offset>1000000||!Number.isInteger(limit)||limit<1||limit>64)throw new TypeError('Invalid inventory page options.');
      return bridge.request<SkyUltraInventoryPage>('game.getInventory',{query,category,equipment,sort,offset,limit},options);
    },
    thumbnail: (formId:number|string,options?:SkyUltraRequestOptions&{usePreview?:boolean})=>{
      const payload:{formId:number|string;usePreview?:boolean}={formId:inventoryFormId(formId)};
      if(options?.usePreview===true)payload.usePreview=true;
      return bridge.request<SkyUltraItemThumbnail>('game.getItemThumbnail',payload,options);
    },
    actorThumbnail: (actor:{referenceId?:number|string;baseFormId?:number|string;usePreview?:boolean},options?:SkyUltraRequestOptions)=>{
      const payload:{referenceId?:number|string;baseFormId?:number|string;usePreview?:boolean}={};
      if(actor?.referenceId!==undefined&&actor.referenceId!=='')payload.referenceId=inventoryFormId(actor.referenceId);
      if(actor?.baseFormId!==undefined&&actor.baseFormId!=='')payload.baseFormId=inventoryFormId(actor.baseFormId);
      if(actor?.usePreview===true)payload.usePreview=true;
      if(payload.referenceId===undefined&&payload.baseFormId===undefined)throw new TypeError('An actor referenceId or baseFormId is required.');
      return bridge.request<SkyUltraItemThumbnail>('game.getActorThumbnail',payload,options);
    },
    equipItem: (formId:number|string,slot:SkyUltraEquipmentSlot='auto',options?:SkyUltraRequestOptions)=>equipment('game.equipItem',formId,slot,options),
    unequipItem: (formId:number|string,slot:SkyUltraEquipmentSlot='auto',options?:SkyUltraRequestOptions)=>equipment('game.unequipItem',formId,slot,options),
    itemAction: (input:SkyUltraItemActionOptions,options?:SkyUltraRequestOptions)=>{
      if(!input||typeof input!=='object')throw new TypeError('Item action options are required.');
      const checkedFormId=inventoryFormId(input.formId);
      if(!skyUltraItemActions.includes(input.action))throw new TypeError('Unknown inventory item action.');
      const count=input.count===undefined?1:input.count;
      if(!Number.isInteger(count)||count<1||count>100000)throw new TypeError('Item action count must be a positive integer.');
      const payload:{formId:number|string;action:SkyUltraItemAction;count:number;openNative?:boolean}={formId:checkedFormId,action:input.action,count};
      if(input.openNative===true)payload.openNative=true;
      return bridge.request<SkyUltraItemActionResult>('game.itemAction',payload,options).then((res)=>{
        if(res?.accepted&&res.equipment){
          sharedFeedHubs.get(bridge)?.applyOptimisticEquipment(res.equipment);
        }
        return res;
      });
    },
    barterSnapshot: (vendorReferenceId?:number|string,options?:SkyUltraRequestOptions)=>{
      const payload:{vendorReferenceId?:number|string}={};
      if(vendorReferenceId!==undefined&&vendorReferenceId!=='')payload.vendorReferenceId=inventoryFormId(vendorReferenceId);
      return bridge.request<SkyUltraBarterSnapshot>('game.getBarterSnapshot',payload,options);
    },
    barterTransaction: (input:SkyUltraBarterTransactionOptions,options?:SkyUltraRequestOptions)=>{
      if(!input||typeof input!=='object')throw new TypeError('Barter transaction options are required.');
      if(input.action!=='buy'&&input.action!=='sell')throw new TypeError('Barter action must be "buy" or "sell".');
      const checkedFormId=inventoryFormId(input.formId);
      const count=input.count===undefined?1:input.count;
      if(!Number.isInteger(count)||count<1||count>100000)throw new TypeError('Barter count must be a positive integer.');
      const payload:{action:'buy'|'sell';formId:number|string;count:number;vendorReferenceId?:number|string;allowPartialGold?:boolean}={action:input.action,formId:checkedFormId,count};
      if(input.vendorReferenceId!==undefined&&input.vendorReferenceId!=='')payload.vendorReferenceId=inventoryFormId(input.vendorReferenceId);
      if(input.allowPartialGold===true)payload.allowPartialGold=true;
      return bridge.request<SkyUltraBarterTransactionResult>('game.barterTransaction',payload,options);
    },
    containerSnapshot: (input:SkyUltraContainerSnapshotOptions={},options?:SkyUltraRequestOptions)=>{
      const payload:Record<string,JsonValue>={};
      if(input.containerRefId!==undefined&&input.containerRefId!=='')payload.containerRefId=inventoryFormId(input.containerRefId);
      if(typeof input.query==='string')payload.query=input.query.slice(0,256);
      if(typeof input.category==='string')payload.category=input.category;
      if(input.sort&&['name','value','weight','count'].includes(input.sort))payload.sort=input.sort;
      return bridge.request<SkyUltraContainerSnapshot>('game.getContainerSnapshot',payload,options);
    },
    containerTransfer: (input:SkyUltraContainerTransferOptions,options?:SkyUltraRequestOptions)=>{
      if(!input||typeof input!=='object')throw new TypeError('Container transfer options are required.');
      const resolvedMode=input.mode??input.action;
      if(!resolvedMode||!['take','store','takeAll','storeAll','steal','plant','stealAll','plantAll'].includes(resolvedMode))throw new TypeError('Container transfer mode must be take, store, takeAll, storeAll, steal, plant, stealAll, or plantAll.');
      const payload:Record<string,JsonValue>={mode:resolvedMode};
      if(resolvedMode==='take'||resolvedMode==='store'||resolvedMode==='steal'||resolvedMode==='plant'){
        if(input.formId===undefined||input.formId==='')throw new TypeError('formId is required for take/store/steal/plant.');
        payload.formId=inventoryFormId(input.formId);
        const count=input.count===undefined?1:input.count;
        if(!Number.isInteger(count)||count<1||count>10000000)throw new TypeError('Container transfer count must be a positive integer.');
        payload.count=count;
      }
      if(input.containerRefId!==undefined&&input.containerRefId!=='')payload.containerRefId=inventoryFormId(input.containerRefId);
      if(typeof input.includeEquipped==='boolean')payload.includeEquipped=input.includeEquipped;
      if(typeof input.category==='string')payload.category=input.category;
      if(typeof input.equip==='boolean')payload.equip=input.equip;
      if(input.slot&&skyUltraEquipmentSlots.includes(input.slot))payload.slot=input.slot;
      return bridge.request<SkyUltraContainerTransferResult>('game.containerTransfer',payload,options);
    },
    spells: (options?:SkyUltraRequestOptions)=>bridge.request<SkyUltraSpellBook>('game.getSpells',{},options),
    equipSpell: (formId:number|string,hand:SkyUltraSpellHand,options?:SkyUltraRequestOptions)=>{
      if(!['left','right','both','voice'].includes(hand))throw new TypeError('Spell hand must be left, right, both or voice.');
      return bridge.request<SkyUltraSpellEquipResult>('game.equipSpell',{formId:inventoryFormId(formId),hand},options);
    },
    renameSpell: (formId:number|string,name:string,options?:SkyUltraRequestOptions)=>{
      const trimmed=typeof name==='string'?name.trim():'';
      if(!trimmed)throw new TypeError('A non-empty spell, shout, or power name is required.');
      return bridge.request<SkyUltraSpellRenameResult>('game.renameSpell',{formId:inventoryFormId(formId),name:trimmed.slice(0,96)},options);
    },
    deleteSpell: (formId:number|string,kind:SkyUltraDeletableMagicKind='spell',options?:SkyUltraRequestOptions)=>{
      if(!['spell','power','lesserPower','shout','effect'].includes(kind))throw new TypeError('Delete kind must be spell, power, lesserPower, shout, or effect.');
      const resolvedId=kind==='effect'&&typeof formId==='string'&&formId.includes(':')?formId:inventoryFormId(formId);
      return bridge.request<SkyUltraSpellDeleteResult>('game.deleteSpell',{formId:resolvedId,kind},options);
    },
    favorites: (options?:SkyUltraRequestOptions)=>bridge.request<SkyUltraFavorites>('game.getFavorites',{},options),
    setFavorite: (formId:number|string,kind:SkyUltraFavoriteKind,favorite:boolean,options?:SkyUltraRequestOptions)=>{
      if(kind!=='item'&&kind!=='spell')throw new TypeError('Favorite kind must be "item" or "spell".');
      return bridge.request<SkyUltraFavoriteResult>('game.setFavorite',{formId:inventoryFormId(formId),kind,favorite:favorite===true},options);
    },
    equipActorItem: (referenceId:number|string,formId:number|string,slot:SkyUltraEquipmentSlot='auto',options?:SkyUltraRequestOptions)=>{
      if(!skyUltraEquipmentSlots.includes(slot))throw new TypeError('Unknown equipment slot.');
      return bridge.request<SkyUltraEquipmentResult>('game.equipItem',{referenceId:inventoryFormId(referenceId),formId:inventoryFormId(formId),slot},options);
    },
    unequipActorItem: (referenceId:number|string,formId:number|string,slot:SkyUltraEquipmentSlot='auto',returnToPlayerOrOptions?:boolean|SkyUltraRequestOptions,maybeOptions?:SkyUltraRequestOptions)=>{
      if(!skyUltraEquipmentSlots.includes(slot))throw new TypeError('Unknown equipment slot.');
      const returnToPlayer=typeof returnToPlayerOrOptions==='boolean'?returnToPlayerOrOptions:undefined;
      const options=typeof returnToPlayerOrOptions==='object'&&returnToPlayerOrOptions!==null?returnToPlayerOrOptions:maybeOptions;
      const payload:Record<string,JsonValue>={referenceId:inventoryFormId(referenceId),formId:inventoryFormId(formId),slot};
      if(typeof returnToPlayer==='boolean')payload.returnToPlayer=returnToPlayer;
      return bridge.request<SkyUltraEquipmentResult>('game.unequipItem',payload,options);
    },
  };
}
