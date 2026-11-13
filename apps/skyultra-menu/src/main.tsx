import { createRoot } from "react-dom/client";
import { Component, type ReactNode } from "react";
import { installSkyUltraMock, markSkyUltraPageReady, defaultSkyUltraSettings, type JsonValue, type Player } from "@skyultra/sdk";
import { App } from "@skyultra/ui/menu";
import "@skyultra/ui/styles.css";
import "@skyultra/ui/menu.css";
import "@skyultra/ui/chrome.css";

const previewPlayerSnapshot: JsonValue = {
  available: true,
  playerName: "Dovahkiin",
  level: 24,
  location: "Whiterun · preview fixture",
  heading: 42,
  calendar: { hour: 16, minute: 32, day: 24, month: "Last Seed", year: 201 },
  carryWeight: { current: 142, max: 300 },
  health: { current: 240, max: 310 },
  stamina: { current: 128, max: 180 },
  magicka: { current: 92, max: 140 },
  armorRating: 62,
  gold: 1840,
  resistances: [
    { id: "fire", label: "Fire", value: 20 }, { id: "frost", label: "Frost", value: 60 },
    { id: "shock", label: "Shock", value: -10 }, { id: "magic", label: "Magic", value: 15 },
    { id: "poison", label: "Poison", value: 0 }, { id: "disease", label: "Disease", value: 31 },
  ],
  skills: [
    { id: "oneHanded", label: "One-handed", group: "Combat", value: 55 },
    { id: "block", label: "Block", group: "Combat", value: 40 },
    { id: "destruction", label: "Destruction", group: "Magic", value: 38 },
    { id: "restoration", label: "Restoration", group: "Magic", value: 27 },
    { id: "lightArmor", label: "Light armor", group: "Stealth", value: 42 },
    { id: "sneak", label: "Sneak", group: "Stealth", value: 29 },
  ],
  activeEffects: [
    { id: "1", name: "Fortify Health", magnitude: 20, duration: 0, remaining: 0, hostile: false },
    { id: "2", name: "Resist Frost", magnitude: 15, duration: 300, remaining: 182, hostile: false },
    { id: "3", name: "Weakness to Shock", magnitude: 10, duration: 0, remaining: 0, hostile: true },
  ],
  activeEffectCount: 3,
  equipment: {
    head: { id: "00000001", name: "Circlet of the North", equipSlots: ["head"], category: "armor", count: 1, value: 450, weight: 1, equipped: true, enchanted: true, enchantmentName: "Peerless Magicka", enchantments: [{ name: "Fortify Magicka", description: "Increases your Magicka by 60 points.", magnitude: 60, duration: 0 }] },
    body: { id: "00000002", name: "Wayfarer's Mail", equipSlots: ["body"], category: "armor", count: 1, value: 820, weight: 12, equipped: true, enchanted: true, enchantmentName: "Frostbulwark", enchantments: [{ name: "Resist Frost", description: "Increases Frost Resistance by 40%.", magnitude: 40, duration: 0 }, { name: "Fortify Health", description: "Increases your Health by 50 points.", magnitude: 50, duration: 0 }] },
    hands: { id: "00000003", name: "Leather Bracers", equipSlots: ["hands"], category: "armor", count: 1, value: 90, weight: 2, equipped: true },
    feet: { id: "00000004", name: "Leather Boots", equipSlots: ["feet"], category: "armor", count: 1, value: 75, weight: 2, equipped: true },
    mainHand: { id: "00000005", name: "Wyrmsteel Blade", equipSlots: ["mainHand","offHand"], category: "weapon", count: 1, value: 720, weight: 11, equipped: true, enchanted: true, enchantmentName: "Frostfire Bite", enchantmentCharge: 84, enchantments: [{ name: "Fire Damage", description: "Target takes 25 points of fire damage. Targets on fire take extra damage.", magnitude: 25, duration: 0 }, { name: "Frost Damage", description: "Target takes 20 points of frost damage to Health and Stamina.", magnitude: 20, duration: 0 }] },
    offHand: null,
  },
  inventory: [
    { id: "00000001", name: "Circlet of the North", equipSlots: ["head"], category: "armor", count: 1, value: 450, weight: 1, equipped: true, enchanted: true, enchantmentName: "Peerless Magicka", enchantments: [{ name: "Fortify Magicka", description: "Increases your Magicka by 60 points.", magnitude: 60, duration: 0 }] },
    { id: "00000002", name: "Wayfarer's Mail", equipSlots: ["body"], category: "armor", count: 1, value: 820, weight: 12, equipped: true, enchanted: true, enchantmentName: "Frostbulwark", enchantments: [{ name: "Resist Frost", description: "Increases Frost Resistance by 40%.", magnitude: 40, duration: 0 }, { name: "Fortify Health", description: "Increases your Health by 50 points.", magnitude: 50, duration: 0 }] },
    { id: "00000003", name: "Leather Bracers", equipSlots: ["hands"], category: "armor", count: 1, value: 90, weight: 2, equipped: true },
    { id: "00000004", name: "Leather Boots", equipSlots: ["feet"], category: "armor", count: 1, value: 75, weight: 2, equipped: true },
    { id: "00000005", name: "Wyrmsteel Blade", equipSlots: ["mainHand","offHand"], category: "weapon", count: 1, value: 720, weight: 11, equipped: true, enchanted: true, enchantmentName: "Frostfire Bite", enchantmentCharge: 84, enchantments: [{ name: "Fire Damage", description: "Target takes 25 points of fire damage. Targets on fire take extra damage.", magnitude: 25, duration: 0 }, { name: "Frost Damage", description: "Target takes 20 points of frost damage to Health and Stamina.", magnitude: 20, duration: 0 }] },
    { id: "00000006", name: "Minor Health Potion", category: "potion", count: 4, value: 18, weight: 0.5, equipped: false },
    { id: "00000007", name: "Iron Arrow", equipSlots: ["ammo"], category: "ammo", count: 37, value: 1, weight: 0.1, equipped: false },
    { id: "00000008", name: "Dragon Bone", category: "miscellaneous", count: 2, value: 500, weight: 15, equipped: false },
    { id: "00000009", name: "Frost Salt", category: "ingredient", count: 3, value: 15, weight: 0.3, equipped: false },
    { id: "10", name: "Empty Soul Gem", category: "soul gem", count: 1, value: 60, weight: 0.5, equipped: false },
  ],
  inventoryTypeCount: 10,
  inventoryItemCount: 54,
  inventoryTruncated: false,
};

if (!window.SkyWebView) {
  let previewSettings=JSON.parse(JSON.stringify(defaultSkyUltraSettings)) as JsonValue;
  const previewMock = installSkyUltraMock(window, {
    requestHandler: (name, params): JsonValue => {
      if(name==="ui.getSettings")return JSON.parse(JSON.stringify(previewSettings)) as JsonValue;
      if(name==="ui.updateSettings"){previewSettings=JSON.parse(JSON.stringify(params)) as JsonValue;return previewSettings;}
      if(name==="ui.close")return {closed:false};
      if (name === "game.getPlayerSnapshot") return previewPlayerSnapshot;
      if (name === "game.getInventory") {
        const input = (params ?? {}) as Record<string, JsonValue>;
        const fixture = previewPlayerSnapshot as unknown as Player;
        const query = String(input.query ?? '').toLowerCase(), category = String(input.category ?? 'all');
        const equipment = String(input.equipment ?? 'all'), sort = String(input.sort ?? 'name');
        const offset = Number(input.offset ?? 0), limit = Number(input.limit ?? 64);
        const items = fixture.inventory.filter(item => (category === 'all' || category === item.category) && (equipment === 'all' || item.equipped === (equipment === 'equipped')) && (item.name+' '+item.category+' '+item.id).toLowerCase().includes(query));
        items.sort((a,b) => sort === 'value' ? b.value-a.value : sort === 'weight' ? b.weight-a.weight : sort === 'count' ? b.count-a.count : a.name.localeCompare(b.name));
        return {available:true, results:items.slice(offset,offset+limit), totalTypes:fixture.inventoryTypeCount, totalMatches:items.length, offset, categories:[...new Set(fixture.inventory.map(item => item.category))].sort()} as unknown as JsonValue;
      }
      if (name === "game.getItemThumbnail") return {available:false, message:"Item model thumbnails are available inside Skyrim."};
      if (name === "game.equipItem" || name === "game.unequipItem") return {accepted:false, message:"Equipping is available inside Skyrim. Browser fixtures do not change your character."};
      if (name === "game.getBarterSnapshot") {
        const fixture = previewPlayerSnapshot as unknown as Player;
        return {
          available: true,
          vendorReferenceId: "0001A672",
          vendorName: "Belethor",
          vendorGold: 750,
          vendorItems: fixture.inventory,
          vendorTotalTypes: fixture.inventory.length,
          vendorCategories: [...new Set(fixture.inventory.map(item => item.category))].sort(),
          playerName: fixture.playerName,
          playerGold: fixture.gold,
          playerCarryWeight: fixture.carryWeight,
          playerItems: fixture.inventory,
          playerTotalTypes: fixture.inventory.length,
          playerCategories: [...new Set(fixture.inventory.map(item => item.category))].sort(),
        } as unknown as JsonValue;
      }
      if (name === "game.barterTransaction") return { accepted: false, message: "Barter transactions are available inside Skyrim." };
      if (name === "ui.pageReady" || name === "ui.setInputScope") return { ready: true };
      if (name === "ui.configureWidgets") return { available: false, enabled: false, message: "Gameplay pinning is available inside Skyrim." };
      if (name === "game.getNearbyNpcs") return { available: true, radius: 6000, totalNearby: 2, results: [
        { baseFormId: "8128", referenceId: "66601", name: "Lydia · preview", race: "Nord", sex: "Female", level: 24, follower: true, distance: 220, health: 250, healthMax: 310 },
        { baseFormId: "8135", referenceId: "66602", name: "Whiterun Guard · preview", race: "Nord", sex: "Male", level: 20, distance: 460 },
      ] };
      if (name === "game.searchNpcs") return { available: true, totalMatches: 0, results: [] };
      if (name === "game.setActorPreview" || name === "game.getActorPreviewStatus") return { available: false, ready: false, message: "Native actor rendering is available inside Skyrim. This browser uses preview fixture data." };
      if (name === "game.captureActorAnimations") return { available: false, clips: [], message: "Animation capture is available inside Skyrim." };
      if (name === "game.loadActorPoses") return { available: false, clips: [], message: "Pose loading is available inside Skyrim." };
      if (name === "game.closeActorPreview") return { available: true, ready: false };
      if (name === "game.setItemPreview" || name === "game.getItemPreviewStatus" || name === "game.closeItemPreview") return { available: false, ready: false, message: "Native item rendering is available inside Skyrim." };
      if (name === "game.saveItemPreview") return { saved: false, message: "PNG export is available inside Skyrim." };
      throw new Error("Unsupported preview request: " + name);
    },
  });
  if (import.meta.env.DEV) {
    Object.defineProperty(window, "__SKYULTRA_MOCK__", {
      value: previewMock,
      configurable: true,
    });
  }
}

const rootElement = document.getElementById("root");
if (!rootElement) throw new Error("Missing #root element");

class FieldRecovery extends Component<{children:ReactNode},{failed:boolean}> {
  state={failed:false};
  static getDerivedStateFromError():{failed:boolean} { return {failed:true}; }
  componentDidCatch(error:Error):void {
    console.error("SkyUltra Field Atlas could not render:",error);
    void markSkyUltraPageReady().catch(()=>undefined);
  }
  render():ReactNode {
    return this.state.failed ? <main id="skyultra-boot"><section className="boot-content"><img className="boot-logo" src="./SkyUltraLogo.png" alt="SkyUltra" draggable={false}/><i className="boot-rule"/><h1 className="boot-title">The field interface needs to reload.</h1><p className="boot-status">A page component or resource failed. The browser log contains the error.</p><button className="boot-retry" onClick={()=>location.reload()}>Reload interface</button></section></main> : this.props.children;
  }
}
createRoot(rootElement).render(<FieldRecovery><App /></FieldRecovery>);

