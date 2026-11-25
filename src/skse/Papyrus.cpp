#include "SkyUltra/ActorPreview.h"
#include "SkyUltra/D3D11Overlay.h"
#include "SkyUltra/FrameworkServices.h"
#include <SKSE/SKSE.h>
#include <RE/A/AIFormulas.h>
#include <RE/A/ActorValues.h>
#include <RE/A/ActorEquipManager.h>
#include <RE/A/AlchemyItem.h>
#include <RE/B/BGSAddToPlayerInventoryEvent.h>
#include <RE/B/BGSCraftItemEvent.h>
#include <RE/B/BGSCreatedObjectManager.h>
#include <RE/B/BGSDefaultObjectManager.h>
#include <RE/B/BGSEntryPoint.h>
#include <RE/B/BGSEquipSlot.h>
#include <RE/B/BGSConstructibleObject.h>
#include <RE/B/BGSListForm.h>
#include <RE/B/BGSPerk.h>
#include <RE/B/BGSRelationship.h>
#include <RE/B/BGSStoryEventManager.h>
#include <RE/C/ChestsLooted.h>
#include <RE/I/ItemCrafted.h>
#include <RE/I/ItemsPickpocketed.h>
#include <RE/S/SendHUDMessage.h>
#include <RE/S/SendUIMessage.h>
#include <RE/A/ActiveEffect.h>
#include <RE/B/BGSArtObject.h>
#include <RE/B/BGSBaseAlias.h>
#include <RE/B/BarterMenu.h>
#include <RE/B/BookMenu.h>
#include <RE/C/ConstructibleObjectMenu.h>
#include <RE/C/ContainerMenu.h>
#include <RE/M/MenuEventHandler.h>
#include <RE/M/MenuOpenCloseEvent.h>
#include <RE/C/CraftingMenu.h>
#include <RE/C/CraftingSubMenu.h>
#include <RE/E/EnchantConstructMenu.h>
#include <RE/E/ExtraAliasInstanceArray.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSLight.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BGSLocation.h>
#include <RE/B/BSModelDB.h>
#include <RE/B/BSShaderManager.h>
#include <RE/B/BSShaderProperty.h>
#include <RE/B/BSShadowLight.h>
#include <RE/B/BGSTextureSet.h>
#include <RE/B/BSResourceNiBinaryStream.h>
#include <RE/C/Calendar.h>
#include <RE/B/BSKeyboardDevice.h>
#include <RE/B/BSWin32GamepadDevice.h>
#include <RE/B/BSWin32MouseDevice.h>
#include <RE/C/ControlMap.h>
#include <RE/U/UserEvents.h>
#include <RE/E/Effect.h>
#include <RE/E/EffectSetting.h>
#include <RE/E/EnchantmentItem.h>
#include <RE/E/ExtraCharge.h>
#include <RE/E/ExtraContainerChanges.h>
#include <RE/E/ExtraDataList.h>
#include <RE/E/ExtraEnchantment.h>
#include <RE/E/ExtraHealth.h>
#include <RE/E/ExtraLight.h>
#include <RE/E/ExtraMagicLight.h>
#include <RE/E/ExtraMapMarker.h>
#include <RE/E/ExtraOwnership.h>
#include <RE/E/ExtraPoison.h>
#include <RE/E/ExtraSoul.h>
#include <RE/E/ExtraTeleport.h>
#include <RE/E/ExtraTextDisplayData.h>
#include <RE/E/ExtraWorn.h>
#include <RE/E/ExtraWornLeft.h>
#include <RE/F/FormTraits.h>
#include <RE/G/GameSettingCollection.h>
#include <RE/I/IngredientItem.h>
#include <RE/I/InventoryEntryData.h>
#include <RE/I/ItemRemoveReason.h>
#include <RE/L/LightEffect.h>
#include <RE/M/MagicTarget.h>
#include <RE/M/MenuTopicManager.h>
#include <RE/M/Misc.h>
#include <RE/M/ModelReferenceEffect.h>
#include <RE/N/NativeFunction.h>
#include <RE/N/NiLight.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiPointLight.h>
#include <RE/N/NiRTTI.h>
#include <RE/N/NiSourceTexture.h>
#include <RE/P/PlayerCamera.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/ProcessLists.h>
#include <RE/R/ReferenceEffect.h>
#include <RE/R/Renderer.h>
#include <RE/RTTI.h>
#include <RE/S/ScriptEventSourceHolder.h>
#include <RE/S/ScrollItem.h>
#include <RE/S/ShadowSceneNode.h>
#include <RE/S/SmithingMenu.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESFurniture.h>
#include <RE/T/TESFurnitureEvent.h>
#include <RE/T/TESGlobal.h>
#include <RE/T/TempEffectTraits.h>
#include <RE/I/InventoryChanges.h>
#include <RE/M/MagicFavorites.h>
#include <RE/T/TESShout.h>
#include <RE/T/TESSpellList.h>
#include <RE/T/TESWordOfPower.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESDescription.h>
#include <RE/T/TESEnchantableForm.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESFile.h>
#include <RE/T/TESForm.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectBOOK.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESObjectLIGH.h>
#include <RE/T/TESObjectSTAT.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/T/TESWorldSpace.h>
#include <RE/T/TES.h>
#include <RE/T/TESAmmo.h>
#include <RE/T/TESClass.h>
#include <RE/T/TESSoulGem.h>
#include <RE/T/TESQuest.h>
#include <RE/T/TESRace.h>
#include <RE/U/UI.h>
#include <RE/Q/QuestObjectiveStates.h>

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#undef GetObject
#undef min
#undef max
#include <nlohmann/json.hpp>

#include "SkyUltra/ModManifest.h"
#include "SkyUltra/Papyrus.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view kPapyrusClass = "SkyUltraNative";
constexpr std::string_view kRequestEvent = "SkyUltraPageRequest";
constexpr std::string_view kCancelledEvent = "SkyUltraPageRequestCancelled";
constexpr std::int32_t kMaxPapyrusToken = 0x00FFFFFF;
constexpr std::size_t kMaxPapyrusPendingRequests = 2048;
constexpr std::uint32_t kMaxPixelDimension = 8192;
constexpr std::size_t kMaxPlayerSnapshotItems = 512;

using Json = nlohmann::json;

std::recursive_mutex g_shared_game_data_mutex;

std::string_view ItemCategory(RE::FormType form_type) noexcept
{
    switch (form_type) {
    case RE::FormType::Weapon: return "weapon";
    case RE::FormType::Armor: return "armor";
    case RE::FormType::AlchemyItem: return "potion";
    case RE::FormType::Book: return "book";
    case RE::FormType::Ingredient: return "ingredient";
    case RE::FormType::Ammo: return "ammo";
    case RE::FormType::Scroll: return "scroll";
    case RE::FormType::SoulGem: return "soul gem";
    case RE::FormType::KeyMaster: return "key";
    case RE::FormType::Misc: return "miscellaneous";
    case RE::FormType::Spell: return "spell";
    case RE::FormType::Light: return "light";
    default: return "item";
    }
}

std::string LowerAscii(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

std::string_view NpcSexName(RE::SEX sex) noexcept
{
    switch (sex) {
    case RE::SEX::kMale: return "Male";
    case RE::SEX::kFemale: return "Female";
    default: return "Unknown";
    }
}

std::string NpcRaceName(RE::TESNPC* npc)
{
    const auto* race = npc != nullptr ? npc->GetRace() : nullptr;
    const auto* raw_race = race != nullptr ? race->GetName() : nullptr;
    std::string race_name = raw_race != nullptr && raw_race[0] != '\0' ? raw_race : "";
    const auto* raw_npc = npc != nullptr ? npc->GetName() : nullptr;
    const auto lower_npc = LowerAscii(raw_npc != nullptr ? raw_npc : "");
    const auto lower_race = LowerAscii(race_name);
    if ((race != nullptr && (race->GetFormID() & 0x00FFFFFFU) == 0x000A919DU) ||
        (lower_race == "rabbit" && (lower_npc.find("chicken") != std::string::npos ||
            lower_npc.find("hen") != std::string::npos || lower_npc.find("rooster") != std::string::npos))) {
        return "Chicken";
    }
    if (race_name.empty()) {
        return raw_npc != nullptr && raw_npc[0] != '\0' ? raw_npc : "Unknown";
    }
    return race_name;
}

Json ItemEquipSlots(RE::TESForm* form)
{
    Json slots=Json::array();
    if(auto* armor=form->As<RE::TESObjectARMO>()){
        using Slot=RE::BGSBipedObjectForm::BipedObjectSlot;
        static_assert(static_cast<std::uint32_t>(Slot::kModChestPrimary)==(1U<<(46-30)));
        static_assert(static_cast<std::uint32_t>(Slot::kModPelvisPrimary)==(1U<<(49-30)));
        const auto mask=armor->GetSlotMask();
        if(mask.any(Slot::kHead,Slot::kHair,Slot::kLongHair,Slot::kCirclet))slots.push_back("head");
        if(mask.any(Slot::kBody))slots.push_back("body");
        if(mask.any(Slot::kHands,Slot::kForearms))slots.push_back("hands");
        if(mask.any(Slot::kFeet,Slot::kCalves))slots.push_back("feet");
        if(mask.any(Slot::kAmulet))slots.push_back("neck");
        if(mask.any(Slot::kRing))slots.push_back("ring");
        if(mask.any(Slot::kShield))slots.push_back("offHand");
        if(mask.any(Slot::kModChestPrimary))slots.push_back("cape"); // biped slot 46
        if(mask.any(Slot::kModPelvisPrimary))slots.push_back("lantern"); // biped slot 49
        if(slots.empty())slots.push_back("auto"); // custom armor slots
    }else if(auto* weapon=form->As<RE::TESObjectWEAP>();weapon&&!weapon->IsHandToHandMelee()){
        slots.push_back("mainHand");
        if(!weapon->IsTwoHandedSword()&&!weapon->IsTwoHandedAxe()&&!weapon->IsBow()&&!weapon->IsCrossbow())slots.push_back("offHand");
    }else if(form->Is(RE::FormType::Ammo))slots.push_back("ammo");
    else if(form->Is(RE::FormType::Scroll)){slots.push_back("mainHand");slots.push_back("offHand");}
    else if(form->Is(RE::FormType::Light)){slots.push_back("offHand");slots.push_back("mainHand");}
    else if(auto* spell=form->As<RE::SpellItem>()){
        const auto spell_type=spell->GetSpellType();
        if(spell_type!=RE::MagicSystem::SpellType::kPower&&spell_type!=RE::MagicSystem::SpellType::kLesserPower){
            slots.push_back("mainHand");
            if(!spell->IsTwoHanded())slots.push_back("offHand");
        }
    }
    return slots;
}

bool IsBeverageItemName(std::string_view raw_name)
{
    const auto lower = LowerAscii(raw_name);
    constexpr std::array<std::string_view, 23> kWords{
        "mead", "wine", "ale", "brandy", "beer", "cider", "drink", "milk", "water",
        "juice", "tea", "coffee", "skooma", "sujamma", "flin", "mazte", "shein",
        "rum", "whiskey", "vodka", "liquor", "brew", "draught"
    };
    for (const auto word : kWords) {
        if (lower.find(word) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string StripLeadingDollar(std::string_view raw)
{
    while (!raw.empty() && (raw.front() == '$' || raw.front() == ' ' || raw.front() == '\t')) {
        raw.remove_prefix(1);
    }
    while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t')) {
        raw.remove_suffix(1);
    }
    return std::string(raw);
}

bool IsMissingOrUnnamedDisplayName(std::string_view raw) noexcept
{
    while (!raw.empty() && (raw.front() == '$' || raw.front() == ' ' || raw.front() == '\t')) {
        raw.remove_prefix(1);
    }
    while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t')) {
        raw.remove_suffix(1);
    }
    if (raw.empty()) {
        return true;
    }
    const auto lower = LowerAscii(raw);
    return lower == "<missing name>" || lower == "missing name" ||
           lower == "unnamed item" || lower.starts_with("<missing");
}

bool IsHiddenInternalInventoryItem(std::string_view raw) noexcept
{
    const auto lower = LowerAscii(raw);
    return lower.find("genitalcover") != std::string::npos ||
           lower.find("genital cover") != std::string::npos ||
           (lower.starts_with("tng ") && lower.find("genital") != std::string::npos);
}

[[nodiscard]] bool IsLiveInventoryForm(const RE::TESForm* form) noexcept
{
    if (form == nullptr) {
        return false;
    }
    const auto fid = form->GetFormID();
    if (fid == 0U) {
        return false;
    }
    // Dynamic forms (0xFFxxxxxx — brewed potions/poisons, runtime clones, mod temp forms)
    // can be freed while stale InventoryChanges entries remain; also re-verify uninitialized forms.
    // Static ESM/ESP/ESL forms pay only two integer checks (zero lock contention).
    if ((fid >> 24U) == 0xFFU || !form->IsInitialized()) {
        return RE::TESForm::LookupByID(fid) == form;
    }
    return true;
}

bool IsPlayableNamedInventoryItem(RE::TESForm* form, const RE::InventoryEntryData* entry_data = nullptr, bool allow_equipped = false)
{
    (void)allow_equipped;
    if (!IsLiveInventoryForm(form) || !form->IsInventoryObject() || form->Is(RE::FormType::LeveledItem)) {
        return false;
    }
    if (form->GetFormID() == 0x0000000FU || form->IsGold()) {
        return true;
    }
    if (!form->GetPlayable()) {
        return false;
    }
    if (const auto* armor = form->As<RE::TESObjectARMO>(); armor != nullptr && !armor->GetPlayable()) {
        return false;
    }
    if (const auto* weap = form->As<RE::TESObjectWEAP>(); weap != nullptr &&
        (weap->weaponData.flags.any(RE::TESObjectWEAP::Data::Flag::kNonPlayable) || !weap->GetPlayable())) {
        return false;
    }
    const char* raw_name = entry_data != nullptr ? const_cast<RE::InventoryEntryData*>(entry_data)->GetDisplayName() : nullptr;
    if (raw_name == nullptr || raw_name[0] == '\0' || IsMissingOrUnnamedDisplayName(raw_name)) {
        raw_name = form->GetName();
    }
    if (raw_name == nullptr || raw_name[0] == '\0' || IsMissingOrUnnamedDisplayName(raw_name)) {
        return false;
    }
    if (IsHiddenInternalInventoryItem(raw_name)) {
        return false;
    }
    return true;
}

std::unordered_map<std::uint32_t, std::int32_t> g_stolen_player_counts;

std::int32_t ComputePlayerStolenCount(
    RE::TESForm* form,
    std::int32_t total_count,
    const RE::InventoryEntryData* entry_data = nullptr)
{
    if (form == nullptr || total_count <= 0 || form->GetFormID() == 0x0000000FU || form->IsGold()) {
        return 0;
    }
    if (const auto it = g_stolen_player_counts.find(form->GetFormID()); it != g_stolen_player_counts.end()) {
        const std::int32_t tracked = std::clamp(it->second, 0, total_count);
        it->second = tracked;
        return tracked;
    }
    std::int32_t native_stolen = 0;
    if (entry_data != nullptr && entry_data->extraLists != nullptr) {
        auto* mut_entry = const_cast<RE::InventoryEntryData*>(entry_data);
        if (auto* player = RE::PlayerCharacter::GetSingleton(); player != nullptr) {
            for (auto* x_list : *entry_data->extraLists) {
                if (x_list == nullptr) continue;
                if (auto* owner = x_list->GetOwner(); owner != nullptr && !mut_entry->IsOwnedBy(player, owner, true)) {
                    native_stolen += std::max<std::int32_t>(1, x_list->GetCount());
                }
            }
        }
    }
    return std::clamp(native_stolen, 0, total_count);
}

void RecordPlayerStolenCount(
    std::uint32_t form_id,
    std::int32_t prior_stolen,
    std::int32_t stolen_delta,
    std::int32_t max_total_count)
{
    if (form_id == 0 || form_id == 0x0000000FU || stolen_delta <= 0) return;
    if (max_total_count <= 0) {
        g_stolen_player_counts.erase(form_id);
        return;
    }
    g_stolen_player_counts[form_id] = std::clamp(prior_stolen + stolen_delta, 0, max_total_count);
}

void ConsumePlayerStolenCount(
    RE::TESForm* form,
    const RE::InventoryEntryData* entry_data,
    std::int32_t before_total,
    std::int32_t used_count,
    std::int32_t remaining_total,
    bool from_stolen_stack)
{
    if (form == nullptr || form->GetFormID() == 0 || form->GetFormID() == 0x0000000FU || form->IsGold()) return;
    const auto form_id = form->GetFormID();
    if (remaining_total <= 0) {
        g_stolen_player_counts.erase(form_id);
        return;
    }
    const std::int32_t prior_stolen = ComputePlayerStolenCount(form, before_total, entry_data);
    if (prior_stolen <= 0 && !g_stolen_player_counts.contains(form_id)) {
        return;
    }
    if (from_stolen_stack) {
        g_stolen_player_counts[form_id] = std::clamp(prior_stolen - std::max(0, used_count), 0, remaining_total);
    } else {
        g_stolen_player_counts[form_id] = std::clamp(prior_stolen, 0, remaining_total);
    }
}

std::mutex g_custom_item_names_mutex;
std::unordered_map<RE::FormID, std::string> g_custom_item_names;

std::string CustomItemDisplayName(RE::FormID form_id)
{
    const std::scoped_lock lock(g_custom_item_names_mutex);
    if (const auto it = g_custom_item_names.find(form_id); it != g_custom_item_names.end()) {
        return it->second;
    }
    return {};
}

const char* TemperTierLabel(float health) noexcept
{
    if (!std::isfinite(health) || health <= 1.01F) return nullptr;
    if (health >= 1.595F) return "Legendary";
    if (health >= 1.495F) return "Epic";
    if (health >= 1.395F) return "Flawless";
    if (health >= 1.295F) return "Exquisite";
    if (health >= 1.195F) return "Superior";
    return "Fine";
}

const char* SoulLevelLabel(RE::SOUL_LEVEL soul) noexcept
{
    switch (soul) {
    case RE::SOUL_LEVEL::kPetty: return "Petty";
    case RE::SOUL_LEVEL::kLesser: return "Lesser";
    case RE::SOUL_LEVEL::kCommon: return "Common";
    case RE::SOUL_LEVEL::kGreater: return "Greater";
    case RE::SOUL_LEVEL::kGrand: return "Grand";
    default: return nullptr;
    }
}

const char* SkillLabelForActorValue(RE::ActorValue av) noexcept
{
    switch (av) {
    case RE::ActorValue::kOneHanded: return "One-handed";
    case RE::ActorValue::kTwoHanded: return "Two-handed";
    case RE::ActorValue::kArchery: return "Archery";
    case RE::ActorValue::kBlock: return "Block";
    case RE::ActorValue::kSmithing: return "Smithing";
    case RE::ActorValue::kHeavyArmor: return "Heavy armor";
    case RE::ActorValue::kLightArmor: return "Light armor";
    case RE::ActorValue::kPickpocket: return "Pickpocket";
    case RE::ActorValue::kLockpicking: return "Lockpicking";
    case RE::ActorValue::kSneak: return "Sneak";
    case RE::ActorValue::kAlchemy: return "Alchemy";
    case RE::ActorValue::kSpeech: return "Speech";
    case RE::ActorValue::kAlteration: return "Alteration";
    case RE::ActorValue::kConjuration: return "Conjuration";
    case RE::ActorValue::kDestruction: return "Destruction";
    case RE::ActorValue::kIllusion: return "Illusion";
    case RE::ActorValue::kRestoration: return "Restoration";
    case RE::ActorValue::kEnchanting: return "Enchanting";
    default: return nullptr;
    }
}

double ItemValuePerWeight(const Json& item) noexcept
{
    const double val = item.value("value", 0.0);
    const double wt = item.value("weight", 0.0);
    return val / std::max(wt, 0.05);
}

void AppendActorDossierFields(RE::Actor* actor, RE::TESNPC* base, RE::PlayerCharacter* player, Json& record)
{
    if (actor == nullptr) return;
    const auto* av = actor->AsActorValueOwner();
    const float magicka = av == nullptr ? 0.0F : std::max(av->GetActorValue(RE::ActorValue::kMagicka), 0.0F);
    const float magicka_max = std::max(actor->GetActorValueMax(RE::ActorValue::kMagicka), 0.0F);
    const float stamina = av == nullptr ? 0.0F : std::max(av->GetActorValue(RE::ActorValue::kStamina), 0.0F);
    const float stamina_max = std::max(actor->GetActorValueMax(RE::ActorValue::kStamina), 0.0F);
    record["magicka"] = { { "current", magicka }, { "max", magicka_max } };
    record["stamina"] = { { "current", stamina }, { "max", stamina_max } };
    if (base != nullptr && base->npcClass != nullptr) {
        const auto* class_name = base->npcClass->GetFullName();
        if (class_name != nullptr && class_name[0] != '\0') {
            record["className"] = class_name;
        }
    }
    record["isEssential"] = actor->IsEssential();
    record["isUnique"] = base != nullptr && base->IsUnique();
    std::string combat_state = "idle";
    if (!actor->IsDead()) {
        if (actor->IsInCombat()) {
            combat_state = "combat";
        } else if (player != nullptr && actor->IsHostileToActor(player) && actor->RequestDetectionLevel(player) > 0) {
            combat_state = "searching";
        }
    }
    record["combatState"] = std::move(combat_state);
    int relationship_rank = 0;
    if (base != nullptr && player != nullptr && player->GetActorBase() != nullptr) {
        if (const auto* rel = RE::BGSRelationship::GetRelationship(base, player->GetActorBase()); rel != nullptr) {
            relationship_rank = std::clamp(4 - static_cast<int>(rel->level.get()), -4, 4);
        }
    }
    record["relationshipRank"] = relationship_rank;
}

std::uint32_t ParseItemFormIdField(const Json& options, const char* key, bool* out_from_stolen_stack = nullptr)
{
    if (out_from_stolen_stack != nullptr) {
        *out_from_stolen_stack = options.is_object() && options.value("stolen", false);
    }
    if (!options.is_object()) return 0;
    const auto field = options.find(key);
    if (field == options.end()) return 0;
    std::uint32_t form_id = 0;
    if (field->is_number_integer()) {
        form_id = static_cast<std::uint32_t>(field->get<std::int64_t>());
    } else if (field->is_string()) {
        const auto text = field->get<std::string>();
        if (out_from_stolen_stack != nullptr && text.find(":stolen") != std::string::npos) {
            *out_from_stolen_stack = true;
        }
        const bool hex = text.starts_with("0x") || text.starts_with("0X");
        (void)std::from_chars(text.data() + (hex ? 2 : 0), text.data() + text.size(), form_id, hex ? 16 : 10);
    }
    return form_id;
}

Json PlayerItemJson(
    RE::TESForm* form,
    std::int32_t count,
    bool equipped,
    const RE::InventoryEntryData* entry_data = nullptr,
    bool is_player_inventory = true,
    bool is_stolen_stack = false)
{
    if (!IsLiveInventoryForm(form) || form->Is(RE::FormType::LeveledItem)) {
        return nullptr;
    }
    const auto* raw_name = entry_data != nullptr ? const_cast<RE::InventoryEntryData*>(entry_data)->GetDisplayName() : nullptr;
    if (raw_name == nullptr || raw_name[0] == '\0' || IsMissingOrUnnamedDisplayName(raw_name)) {
        raw_name = form->GetName();
    }
    const std::string stripped_name = raw_name != nullptr && raw_name[0] != '\0' ? StripLeadingDollar(raw_name) : std::string{};
    if (IsMissingOrUnnamedDisplayName(stripped_name)) {
        return nullptr;
    }
    const std::string custom_name = CustomItemDisplayName(form->GetFormID());
    const std::string name = !custom_name.empty() ? custom_name : stripped_name;

    RE::EnchantmentItem* ench = entry_data != nullptr ? entry_data->GetEnchantment() : nullptr;
    if (ench == nullptr && entry_data != nullptr && entry_data->extraLists != nullptr) {
        for (const auto* x_list : *entry_data->extraLists) {
            if (x_list == nullptr) continue;
            if (const auto* ex = x_list->GetByType<RE::ExtraEnchantment>(); ex != nullptr && ex->enchantment != nullptr) {
                ench = ex->enchantment;
                break;
            }
        }
    }
    if (ench == nullptr) {
        if (const auto* ench_form = form->As<RE::TESEnchantableForm>(); ench_form != nullptr) {
            ench = ench_form->formEnchanting;
        }
    }

    Json enchantments = Json::array();
    if (ench != nullptr) {
        for (const auto* eff : ench->effects) {
            if (eff == nullptr || eff->baseEffect == nullptr) continue;
            if (eff->baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) continue;
            const auto* eff_name = eff->baseEffect->GetFullName();
            if (eff_name == nullptr || eff_name[0] == '\0') eff_name = eff->baseEffect->GetName();
            if (eff_name == nullptr || eff_name[0] == '\0') continue;
            const float mag = std::isfinite(eff->GetMagnitude()) ? std::max(0.0F, eff->GetMagnitude()) : 0.0F;
            const std::uint32_t dur = eff->GetDuration();
            const std::string mag_str = std::to_string(static_cast<int>(std::round(mag)));
            std::string desc = eff->baseEffect->magicItemDescription.c_str() ? eff->baseEffect->magicItemDescription.c_str() : "";
            for (std::size_t pos = 0; (pos = desc.find('<', pos)) != std::string::npos;) {
                const auto end = desc.find('>', pos);
                if (end == std::string::npos) break;
                const auto token = LowerAscii(desc.substr(pos + 1, end - pos - 1));
                const std::string repl = token == "mag" ? mag_str : token == "dur" ? std::to_string(dur) : token == "area" ? std::to_string(eff->GetArea()) : "";
                desc.replace(pos, end - pos + 1, repl);
                pos += repl.size();
            }
            if (desc.empty()) {
                if (mag > 0.0F && dur > 0U) desc = mag_str + " pts for " + std::to_string(dur) + "s";
                else if (mag > 0.0F) desc = "+" + mag_str + " pts";
                else if (dur > 0U) desc = std::to_string(dur) + "s";
            }
            enchantments.push_back({
                { "name", eff_name },
                { "description", desc },
                { "magnitude", mag },
                { "duration", dur }
            });
            if (enchantments.size() >= 32) break;
        }
    }

    const bool is_enchanted = ench != nullptr || !enchantments.empty() || (entry_data != nullptr && entry_data->IsEnchanted());
    const bool is_quest_item = entry_data != nullptr && entry_data->IsQuestObject();
    const bool is_stolen = is_player_inventory && is_stolen_stack && form->GetFormID() != 0x0000000FU && !form->IsGold();
    const std::string base_id_str = std::to_string(form->GetFormID());
    Json item{
        { "id", is_stolen ? (base_id_str + ":stolen") : base_id_str },
        { "name", name },
        { "category", ItemCategory(form->GetFormType()) },
        { "count", std::max(count, 1) },
        { "value", std::max(entry_data != nullptr ? entry_data->GetValue() : form->GetGoldValue(), 0) },
        { "weight", std::max(form->GetWeight(), 0.0F) },
        { "equipped", equipped },
        { "equipSlots", ItemEquipSlots(form) },
        { "enchanted", is_enchanted },
        { "canDrop", !is_quest_item },
        { "favorite", entry_data != nullptr && entry_data->IsFavorited() },
        { "stolen", is_stolen }
    };
    const RE::ExtraHealth* ex_health = nullptr;
    const RE::ExtraPoison* ex_poison = nullptr;
    const RE::ExtraSoul* ex_soul = nullptr;
    const RE::ExtraOwnership* ex_owner = nullptr;
    if (entry_data != nullptr && entry_data->extraLists != nullptr) {
        for (const auto* x_list : *entry_data->extraLists) {
            if (x_list == nullptr) continue;
            if (ex_health == nullptr) ex_health = x_list->GetByType<RE::ExtraHealth>();
            if (ex_poison == nullptr) ex_poison = x_list->GetByType<RE::ExtraPoison>();
            if (ex_soul == nullptr) ex_soul = x_list->GetByType<RE::ExtraSoul>();
            if (ex_owner == nullptr) ex_owner = x_list->GetByType<RE::ExtraOwnership>();
        }
    }
    if (ex_health != nullptr) {
        if (const char* tier = TemperTierLabel(ex_health->health); tier != nullptr) {
            item["temperTier"] = tier;
        }
    }
    if (ex_poison != nullptr && ex_poison->poison != nullptr) {
        const auto* p_name = ex_poison->poison->GetFullName();
        if (p_name == nullptr || p_name[0] == '\0') p_name = ex_poison->poison->GetName();
        if (p_name != nullptr && p_name[0] != '\0') {
            item["poisonName"] = p_name;
        }
    }
    if (ex_soul != nullptr) {
        if (const char* s_label = SoulLevelLabel(ex_soul->GetContainedSoul()); s_label != nullptr) {
            item["soulSize"] = s_label;
        }
    } else if (const auto* gem = form->As<RE::TESSoulGem>(); gem != nullptr) {
        if (const char* s_label = SoulLevelLabel(gem->GetContainedSoul()); s_label != nullptr) {
            item["soulSize"] = s_label;
        }
    }
    if (ex_owner != nullptr && ex_owner->owner != nullptr) {
        if (auto* owner_check_player = RE::PlayerCharacter::GetSingleton();
            owner_check_player != nullptr && ex_owner->owner != owner_check_player &&
            ex_owner->owner != owner_check_player->GetActorBase()) {
            item["isOwned"] = true;
        }
    }
    if (is_enchanted) {
        const auto* ench_name = ench != nullptr ? ench->GetFullName() : nullptr;
        if (ench_name != nullptr && ench_name[0] != '\0') {
            item["enchantmentName"] = ench_name;
        }
        if (entry_data != nullptr) {
            if (const auto charge = entry_data->GetEnchantmentCharge(); charge.has_value() && std::isfinite(*charge)) {
                item["enchantmentCharge"] = std::clamp(static_cast<int>(std::round(*charge)), 0, 100);
            }
        }
        if (!enchantments.empty()) {
            item["enchantments"] = std::move(enchantments);
        }
        if (form->Is(RE::FormType::Weapon) || item.contains("enchantmentCharge")) {
            item["canCharge"] = true;
        }
    }
    if (const auto* weap = form->As<RE::TESObjectWEAP>(); weap != nullptr) {
        const std::int32_t base_dmg = std::max<std::int32_t>(0, weap->GetAttackDamage());
        std::int32_t eff_dmg = base_dmg;
        if (auto* player = RE::PlayerCharacter::GetSingleton(); player != nullptr) {
            if (const auto* av = player->AsActorValueOwner(); av != nullptr) {
                const auto skill_av = (weap->IsBow() || weap->IsCrossbow())
                    ? RE::ActorValue::kArchery
                    : (weap->IsTwoHandedSword() || weap->IsTwoHandedAxe())
                        ? RE::ActorValue::kTwoHanded
                        : RE::ActorValue::kOneHanded;
                const float skill = std::clamp(av->GetActorValue(skill_av), 0.0F, 200.0F);
                const float scaled = static_cast<float>(base_dmg) * (1.0F + 0.005F * skill);
                if (std::isfinite(scaled) && scaled > 0.0F) {
                    eff_dmg = std::max<std::int32_t>(base_dmg, static_cast<std::int32_t>(std::lround(scaled)));
                }
            }
        }
        item["damage"] = eff_dmg;
        item["baseDamage"] = base_dmg;
        std::string weap_type = "Weapon";
        if (weap->IsOneHandedSword()) weap_type = "One-Handed Sword";
        else if (weap->IsOneHandedDagger()) weap_type = "Dagger";
        else if (weap->IsOneHandedAxe()) weap_type = "War Axe";
        else if (weap->IsOneHandedMace()) weap_type = "Mace";
        else if (weap->IsTwoHandedSword()) weap_type = "Greatsword";
        else if (weap->IsTwoHandedAxe()) {
            const auto lower_weap_name = LowerAscii(name);
            weap_type = (lower_weap_name.find("hammer") != std::string::npos ||
                         lower_weap_name.find("maul") != std::string::npos ||
                         lower_weap_name.find("volendrung") != std::string::npos)
                ? "Warhammer"
                : "Battleaxe";
        } else if (weap->IsBow()) weap_type = "Bow";
        else if (weap->IsStaff()) weap_type = "Staff";
        else if (weap->IsCrossbow()) weap_type = "Crossbow";
        item["weaponType"] = std::move(weap_type);
        const float spd = weap->GetSpeed();
        const float rch = weap->GetReach();
        if (std::isfinite(spd) && spd > 0.0F) item["speed"] = std::round(spd * 100.0F) / 100.0F;
        if (std::isfinite(rch) && rch > 0.0F) item["reach"] = std::round(rch * 100.0F) / 100.0F;
        if (weap->GetCritDamage() > 0) item["critDamage"] = static_cast<std::int32_t>(weap->GetCritDamage());
    } else if (auto* armor = form->As<RE::TESObjectARMO>(); armor != nullptr) {
        const float raw_ar = armor->GetArmorRating();
        const std::int32_t base_ar = std::isfinite(raw_ar) ? std::max<std::int32_t>(0, static_cast<std::int32_t>(std::lround(raw_ar))) : 0;
        std::int32_t eff_ar = base_ar;
        if (base_ar > 0 && (armor->IsHeavyArmor() || armor->IsLightArmor())) {
            if (auto* player = RE::PlayerCharacter::GetSingleton(); player != nullptr) {
                if (const auto* av = player->AsActorValueOwner(); av != nullptr) {
                    const auto skill_av = armor->IsHeavyArmor() ? RE::ActorValue::kHeavyArmor : RE::ActorValue::kLightArmor;
                    const float skill = std::clamp(av->GetActorValue(skill_av), 0.0F, 200.0F);
                    const float scaled = static_cast<float>(base_ar) * (1.0F + 0.004F * skill);
                    if (std::isfinite(scaled) && scaled > 0.0F) {
                        eff_ar = std::max<std::int32_t>(base_ar, static_cast<std::int32_t>(std::lround(scaled)));
                    }
                }
            }
        }
        item["armorRating"] = eff_ar;
        item["baseArmorRating"] = base_ar;
        if (armor->IsHeavyArmor()) {
            item["armorWeightClass"] = "heavy";
            item["armorTypeLabel"] = armor->IsShield() ? "Heavy Shield" : "Heavy Armor";
        } else if (armor->IsLightArmor()) {
            item["armorWeightClass"] = "light";
            item["armorTypeLabel"] = armor->IsShield() ? "Light Shield" : "Light Armor";
        } else if (armor->IsShield()) {
            item["armorWeightClass"] = "shield";
            item["armorTypeLabel"] = "Shield";
        } else {
            item["armorWeightClass"] = "clothing";
            const bool is_jewelry = armor->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kAmulet) ||
                armor->HasPartOf(RE::BGSBipedObjectForm::BipedObjectSlot::kRing) ||
                armor->IsCirclet();
            item["armorTypeLabel"] = is_jewelry ? "Jewelry" : "Clothing";
        }
    } else if (auto* ammo = form->As<RE::TESAmmo>(); ammo != nullptr) {
        const float ammo_dmg = ammo->GetRuntimeData().data.damage;
        item["damage"] = std::isfinite(ammo_dmg) ? std::max<std::int32_t>(0, static_cast<std::int32_t>(std::lround(ammo_dmg))) : 0;
        item["weaponType"] = ammo->IsBolt() ? "Bolt" : "Arrow";
    }
    if (const auto* alch = form->As<RE::AlchemyItem>(); alch != nullptr) {
        const bool drink_sound = alch->data.consumptionSound != nullptr &&
            alch->data.consumptionSound->GetFormID() == 0x000B6435;
        const bool is_drink = !alch->IsFood() || drink_sound || IsBeverageItemName(name);
        item["useAction"] = is_drink ? "drink" : "eat";
    } else if (const auto* ing = form->As<RE::IngredientItem>(); ing != nullptr) {
        item["useAction"] = "eat";
        Json ing_effects = Json::array();
        std::int32_t known_count = 0;
        const std::uint32_t max_slots = std::min<std::uint32_t>(4U, ing->effects.size());
        for (std::uint32_t slot = 0; slot < max_slots; ++slot) {
            const auto* eff = ing->effects[slot];
            const bool known = (ing->gamedata.knownEffectFlags & (static_cast<std::uint16_t>(1U) << slot)) != 0U;
            if (!known || eff == nullptr || eff->baseEffect == nullptr) {
                ing_effects.push_back({
                    { "slot", static_cast<int>(slot) },
                    { "known", false }
                });
                continue;
            }
            ++known_count;
            const auto* base = eff->baseEffect;
            const char* raw_eff_name = base->GetFullName();
            if (raw_eff_name == nullptr || raw_eff_name[0] == '\0') {
                raw_eff_name = base->GetName();
            }
            const std::string eff_name = (raw_eff_name != nullptr && raw_eff_name[0] != '\0')
                ? StripLeadingDollar(raw_eff_name)
                : std::string("Alchemical Effect");
            ing_effects.push_back({
                { "slot", static_cast<int>(slot) },
                { "known", true },
                { "name", eff_name },
                { "isHostile", base->IsHostile() },
                { "isDetrimental", base->IsDetrimental() }
            });
        }
        item["ingredientEffects"] = std::move(ing_effects);
        item["knownEffectCount"] = known_count;
        item["totalEffectCount"] = static_cast<std::int32_t>(max_slots);
    } else if (auto* book = form->As<RE::TESObjectBOOK>(); book != nullptr) {
        item["isRead"] = book->IsRead();
        if (book->TeachesSpell()) {
            item["useAction"] = "learn";
            if (const auto* spell = book->GetSpell(); spell != nullptr) {
                const auto* spell_name = spell->GetFullName();
                if (spell_name != nullptr && spell_name[0] != '\0') {
                    item["teachesSpell"] = spell_name;
                }
            }
        } else {
            item["useAction"] = "read";
            if (book->TeachesSkill()) {
                if (const char* skill_label = SkillLabelForActorValue(book->GetSkill()); skill_label != nullptr) {
                    item["teachesSkill"] = skill_label;
                }
            }
        }
    } else if (form->Is(RE::FormType::Misc) || form->Is(RE::FormType::SoulGem) ||
               form->Is(RE::FormType::KeyMaster) || form->Is(RE::FormType::Scroll) ||
               form->Is(RE::FormType::Activator) || form->Is(RE::FormType::Apparatus) ||
               form->Is(RE::FormType::Light) || form->Is(RE::FormType::TalkingActivator)) {
        item["useAction"] = "activate";
    }
    return item;
}

template <class EmitFn>
void AppendPlayerInventoryItems(
    RE::TESForm* form,
    std::int32_t total_count,
    bool equipped,
    const RE::InventoryEntryData* entry_data,
    EmitFn&& emit)
{
    if (form == nullptr || total_count <= 0) {
        return;
    }
    const std::int32_t stolen_count = ComputePlayerStolenCount(form, total_count, entry_data);
    const std::int32_t clean_count = std::max(0, total_count - stolen_count);
    if (clean_count > 0) {
        auto clean_item = PlayerItemJson(form, clean_count, equipped, entry_data, true, false);
        if (clean_item.is_object()) {
            emit(std::move(clean_item));
        }
    }
    if (stolen_count > 0) {
        const bool stolen_equipped = (clean_count == 0) ? equipped : false;
        auto stolen_item = PlayerItemJson(form, stolen_count, stolen_equipped, entry_data, true, true);
        if (stolen_item.is_object()) {
            emit(std::move(stolen_item));
        }
    }
}

std::uint64_t SharedSnapshotNowMs() noexcept;

Json ReadActorEquipment(RE::Actor* actor, bool scan_inventory = true)
{
    const bool is_player = actor != nullptr && actor == RE::PlayerCharacter::GetSingleton();
    Json equipment{
        { "head", nullptr },
        { "body", nullptr },
        { "hands", nullptr },
        { "feet", nullptr },
        { "neck", nullptr },
        { "ring", nullptr },
        { "cape", nullptr },
        { "lantern", nullptr },
        { "ammo", actor ? PlayerItemJson(actor->GetCurrentAmmo(), 1, true, nullptr, is_player) : nullptr },
        { "mainHand", actor ? PlayerItemJson(actor->GetEquippedObject(false), 1, true, actor->GetEquippedEntryData(false), is_player) : nullptr },
        { "offHand", actor ? PlayerItemJson(actor->GetEquippedObject(true), 1, true, actor->GetEquippedEntryData(true), is_player) : nullptr }
    };
    if (actor == nullptr) {
        return equipment;
    }
    bool scanned_live_changes = false;
    if (scan_inventory) {
        for (const auto& [form, inventory_entry] : actor->GetInventory()) {
            if (form == nullptr || inventory_entry.first <= 0) {
                continue;
            }
            auto* entry_data = inventory_entry.second.get();
            const bool worn = entry_data != nullptr && entry_data->IsWorn();
            auto* armor = form->As<RE::TESObjectARMO>();
            if (armor == nullptr || !worn || !IsPlayableNamedInventoryItem(armor, entry_data, true)) {
                continue;
            }
            const auto worn_item = PlayerItemJson(form, inventory_entry.first, true, entry_data, is_player);
            for (const auto& slot : ItemEquipSlots(armor)) {
                if (slot != "auto") {
                    equipment[slot.get<std::string>()] = worn_item;
                }
            }
        }
        scanned_live_changes = true;
    } else if (!is_player) {
        // Single-pass scan over existing ExtraContainerChanges without calling GetWornArmor 8 times per actor.
        if (auto* container_changes = actor->extraList.GetByType<RE::ExtraContainerChanges>();
            container_changes != nullptr && container_changes->changes != nullptr &&
            container_changes->changes->entryList != nullptr) {
            for (const auto* entry_data : *container_changes->changes->entryList) {
                if (entry_data == nullptr || entry_data->object == nullptr || !entry_data->IsWorn()) {
                    continue;
                }
                auto* armor = entry_data->object->As<RE::TESObjectARMO>();
                if (armor == nullptr || !IsPlayableNamedInventoryItem(armor, entry_data, true)) {
                    continue;
                }
                const auto worn_item = PlayerItemJson(armor, std::max<std::int32_t>(entry_data->countDelta, 1), true, entry_data, false);
                if (!worn_item.is_object()) {
                    continue;
                }
                for (const auto& slot : ItemEquipSlots(armor)) {
                    if (slot != "auto") {
                        equipment[slot.get<std::string>()] = worn_item;
                    }
                }
                scanned_live_changes = true;
            }
        }
    }
    if (!scanned_live_changes) {
        using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
        const auto fill_slot = [&](const char* key, Slot mask) {
            if (!equipment[key].is_null()) {
                return;
            }
            if (auto* armor = actor->GetWornArmor(mask, !scan_inventory);
                armor != nullptr && IsPlayableNamedInventoryItem(armor, nullptr, true)) {
                equipment[key] = PlayerItemJson(armor, 1, true, nullptr, is_player);
            }
        };
        fill_slot("head", static_cast<Slot>(static_cast<std::uint32_t>(Slot::kHead) | static_cast<std::uint32_t>(Slot::kHair) | static_cast<std::uint32_t>(Slot::kLongHair) | static_cast<std::uint32_t>(Slot::kCirclet)));
        fill_slot("body", Slot::kBody);
        fill_slot("hands", static_cast<Slot>(static_cast<std::uint32_t>(Slot::kHands) | static_cast<std::uint32_t>(Slot::kForearms)));
        fill_slot("feet", static_cast<Slot>(static_cast<std::uint32_t>(Slot::kFeet) | static_cast<std::uint32_t>(Slot::kCalves)));
        fill_slot("neck", Slot::kAmulet);
        fill_slot("ring", Slot::kRing);
        fill_slot("cape", Slot::kModChestPrimary);
        fill_slot("lantern", Slot::kModPelvisPrimary);
    }
    return equipment;
}

Json ParsePageParams(const RE::BSFixedString& params)
{
    if (params.empty()) {
        return Json::object();
    }
    auto parsed = Json::parse(params.data(), params.data() + params.size(), nullptr, false);
    return parsed.is_discarded() || !parsed.is_object() ? Json::object() : std::move(parsed);
}

struct NpcDirectoryEntry {
    RE::TESNPC* form{};
    std::string normalized_name;
};

std::vector<NpcDirectoryEntry>& NpcFormDirectory()
{
    static std::vector<NpcDirectoryEntry> forms;
    static bool initialized{};
    if (!initialized) {
        auto* data_handler = RE::TESDataHandler::GetSingleton();
        if (data_handler == nullptr) {
            return forms;
        }
        auto& loaded_forms = data_handler->GetFormArray<RE::TESNPC>();
        forms.reserve(loaded_forms.size());
        for (auto* form : loaded_forms) {
            if (form != nullptr && form->GetName() != nullptr && form->GetName()[0] != '\0') {
                forms.push_back({ form, LowerAscii(form->GetName()) });
            }
        }
        std::sort(forms.begin(), forms.end(), [](const auto& left, const auto& right) {
            return left.normalized_name < right.normalized_name;
        });
        initialized = true;
    }
    return forms;
}

RE::BSFixedString PapyrusSearchNpcsJSON(
    RE::StaticFunctionTag*, RE::BSFixedString params)
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    const auto request = ParsePageParams(params);
    std::string query;
    if (const auto field = request.find("query"); field != request.end() && field->is_string()) {
        query = LowerAscii(field->get_ref<const std::string&>());
    }
    std::int64_t requested_limit = 30;
    if (const auto field = request.find("limit"); field != request.end()) {
        if (field->is_number_unsigned()) {
            requested_limit = static_cast<std::int64_t>(
                std::min(field->get<std::uint64_t>(), std::uint64_t{50}));
        } else if (field->is_number_integer()) {
            requested_limit = field->get<std::int64_t>();
        }
    }
    const auto limit = static_cast<std::int32_t>(std::clamp<std::int64_t>(requested_limit, 1, 50));
    Json matches = Json::array();
    if (query.size() < 3) {
        return RE::BSFixedString(Json{
            { "available", true }, { "query", query }, { "totalMatches", 0 },
            { "results", std::move(matches) }, { "message", "Enter at least 3 characters." }
        }.dump());
    }

    std::size_t total_matches{};
    for (const auto& entry : NpcFormDirectory()) {
        auto* form = entry.form;
        if (entry.normalized_name.find(query) == std::string::npos) {
            continue;
        }
        ++total_matches;
        if (matches.size() < static_cast<std::size_t>(limit)) {
            const auto* raw_name = form->GetName();
            RE::Actor* loaded_actor{};
            if (auto* lists = RE::ProcessLists::GetSingleton()) {
                lists->ForAllActors([&](RE::Actor* candidate) {
                    if (candidate != nullptr && !candidate->IsDisabled() && !candidate->IsDeleted() &&
                        candidate->Get3D(false) != nullptr && candidate->GetActorBase() == form) {
                        loaded_actor = candidate;
                        return RE::BSContainer::ForEachResult::kStop;
                    }
                    return RE::BSContainer::ForEachResult::kContinue;
                });
            }
            Json record{
                { "baseFormId", std::to_string(form->GetFormID()) },
                { "name", raw_name != nullptr ? raw_name : "Unnamed NPC" },
                { "race", NpcRaceName(form) },
                { "sex", NpcSexName(form->GetSex()) },
                { "source", "loaded NPC record" }
            };
            if (loaded_actor != nullptr) {
                const auto* actor_values = loaded_actor->AsActorValueOwner();
                record["referenceId"] = std::to_string(loaded_actor->GetFormID());
                record["level"] = loaded_actor->GetLevel();
                record["follower"] = loaded_actor->IsPlayerTeammate();
                record["dead"] = loaded_actor->IsDead();
                record["health"] = actor_values == nullptr ? 0.0F :
                    std::max(actor_values->GetActorValue(RE::ActorValue::kHealth), 0.0F);
                record["healthMax"] = std::max(loaded_actor->GetActorValueMax(RE::ActorValue::kHealth), 0.0F);
                record["armorRating"] = actor_values == nullptr ? 0.0F :
                    std::max(actor_values->GetActorValue(RE::ActorValue::kDamageResist), 0.0F);
                AppendActorDossierFields(loaded_actor, form, RE::PlayerCharacter::GetSingleton(), record);
                record["equipment"] = ReadActorEquipment(loaded_actor);
            }
            matches.push_back(std::move(record));
        }
    }
    std::sort(matches.begin(), matches.end(), [](const Json& left, const Json& right) {
        const auto left_name = left.value("name", std::string{});
        const auto right_name = right.value("name", std::string{});
        return left_name == right_name ?
            left.value("baseFormId", std::string{}) < right.value("baseFormId", std::string{}) :
            left_name < right_name;
    });
    return RE::BSFixedString(Json{
        { "available", true }, { "query", query }, { "totalMatches", total_matches },
        { "results", std::move(matches) }, { "limit", limit }
    }.dump());
}

struct NearbyNpcsCacheEntry {
    bool valid{ false };
    std::uint64_t timestamp_ms{ 0 };
    std::uint32_t cell_id{ 0 };
    float radius{ 0.0F };
    std::size_t limit{ 0 };
    std::string response;
};
NearbyNpcsCacheEntry g_nearby_npcs_cache;

RE::BSFixedString PapyrusGetNearbyNpcsJSON(
    RE::StaticFunctionTag*, RE::BSFixedString params)
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    const auto request = ParsePageParams(params);
    auto requested_radius = 6000.0;
    if (const auto field = request.find("radius"); field != request.end() && field->is_number()) {
        requested_radius = field->get<double>();
    }
    if (!std::isfinite(requested_radius)) {
        requested_radius = 6000.0;
    }
    const auto radius = static_cast<float>(std::clamp(requested_radius, 500.0, 20000.0));
    constexpr std::size_t kMaxNearbyActors = 40;
    std::size_t limit = kMaxNearbyActors;
    if (const auto field = request.find("limit"); field != request.end()) {
        if (field->is_number_unsigned()) {
            limit = static_cast<std::size_t>(std::clamp<std::uint64_t>(field->get<std::uint64_t>(), 1ULL, kMaxNearbyActors));
        } else if (field->is_number_integer()) {
            limit = static_cast<std::size_t>(std::clamp<std::int64_t>(field->get<std::int64_t>(), 1LL, static_cast<std::int64_t>(kMaxNearbyActors)));
        }
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* process_lists = RE::ProcessLists::GetSingleton();
    auto* parent_cell = player != nullptr ? player->GetParentCell() : nullptr;
    if (player == nullptr || process_lists == nullptr || !player->Is3DLoaded() ||
        parent_cell == nullptr || !parent_cell->IsAttached()) {
        return RE::BSFixedString(R"({"available":false,"results":[]})");
    }
    if (!REL::Module::IsVR()) {
        const auto& rt = player->GetPlayerRuntimeData();
        if (rt.queuedTargetLoc.isValid || rt.playerFlags.isLoading ||
            rt.playerFlags.travelUseDoor || rt.playerFlags.fastTraveling) {
            return RE::BSFixedString(R"({"available":false,"results":[]})");
        }
    }
    if (auto* ui = RE::UI::GetSingleton();
        ui != nullptr && (ui->IsMenuOpen(std::string_view("Main Menu")) ||
                          ui->IsMenuOpen(std::string_view("Loading Menu")) ||
                          ui->IsMenuOpen(std::string_view("Fader Menu")) ||
                          ui->IsMenuOpen(std::string_view("Mist Menu")))) {
        return RE::BSFixedString(R"({"available":false,"results":[]})");
    }

    constexpr std::uint64_t kNearbyNpcsCacheTtlMs = 1000;
    const auto now_ms = SharedSnapshotNowMs();
    const auto cell_id = parent_cell->GetFormID();
    if (g_nearby_npcs_cache.valid &&
        g_nearby_npcs_cache.cell_id == cell_id &&
        std::abs(g_nearby_npcs_cache.radius - radius) < 0.5F &&
        g_nearby_npcs_cache.limit == limit &&
        (now_ms - g_nearby_npcs_cache.timestamp_ms) < kNearbyNpcsCacheTtlMs) {
        return RE::BSFixedString(g_nearby_npcs_cache.response.c_str());
    }

    const auto player_position = player->GetPosition();
    const auto radius_squared = radius * radius;
    struct NearbyCandidate {
        RE::Actor* actor{};
        RE::TESNPC* base{};
        float distance_squared{};
    };
    std::vector<NearbyCandidate> candidates;
    candidates.reserve(64);
    process_lists->ForAllActors([&](RE::Actor* actor) {
        if (actor == nullptr || actor == player || actor->IsDisabled() || actor->IsDeleted() || actor->Get3D(false) == nullptr) {
            return RE::BSContainer::ForEachResult::kContinue;
        }
        auto* base = actor->GetActorBase();
        if (base == nullptr || base->GetName() == nullptr || base->GetName()[0] == '\0') {
            return RE::BSContainer::ForEachResult::kContinue;
        }
        const auto position = actor->GetPosition();
        const auto dx = position.x - player_position.x;
        const auto dy = position.y - player_position.y;
        const auto dz = position.z - player_position.z;
        const auto distance_squared = dx * dx + dy * dy + dz * dz;
        if (distance_squared > radius_squared) {
            return RE::BSContainer::ForEachResult::kContinue;
        }
        candidates.push_back(NearbyCandidate{ actor, base, distance_squared });
        return RE::BSContainer::ForEachResult::kContinue;
    });
    std::sort(candidates.begin(), candidates.end(), [](const NearbyCandidate& left, const NearbyCandidate& right) {
        return left.distance_squared < right.distance_squared;
    });
    const auto total = candidates.size();
    if (candidates.size() > limit) {
        candidates.erase(candidates.begin() + limit, candidates.end());
    }
    Json actors = Json::array();
    for (const auto& cand : candidates) {
        auto* actor = cand.actor;
        auto* base = cand.base;
        const auto* actor_values = actor->AsActorValueOwner();
        const auto health = actor_values == nullptr ? 0.0F :
            std::max(actor_values->GetActorValue(RE::ActorValue::kHealth), 0.0F);
        const auto health_max = std::max(actor->GetActorValueMax(RE::ActorValue::kHealth), 0.0F);
        const auto armor_rating = actor_values == nullptr ? 0.0F :
            std::max(actor_values->GetActorValue(RE::ActorValue::kDamageResist), 0.0F);
        Json actor_record{
            { "referenceId", std::to_string(actor->GetFormID()) },
            { "baseFormId", std::to_string(base->GetFormID()) },
            { "name", base->GetName() },
            { "race", NpcRaceName(base) },
            { "sex", NpcSexName(base->GetSex()) },
            { "distance", std::sqrt(cand.distance_squared) },
            { "level", actor->GetLevel() },
            { "follower", actor->IsPlayerTeammate() },
            { "dead", actor->IsDead() },
            { "health", health },
            { "healthMax", health_max },
            { "armorRating", armor_rating },
            { "equipment", ReadActorEquipment(actor, false) }
        };
        AppendActorDossierFields(actor, base, player, actor_record);
        actors.push_back(std::move(actor_record));
    }
    g_nearby_npcs_cache.response = Json{
        { "available", true }, { "radius", radius }, { "totalNearby", total },
        { "results", std::move(actors) }
    }.dump();
    g_nearby_npcs_cache.cell_id = cell_id;
    g_nearby_npcs_cache.radius = radius;
    g_nearby_npcs_cache.limit = limit;
    g_nearby_npcs_cache.timestamp_ms = now_ms;
    g_nearby_npcs_cache.valid = true;
    return RE::BSFixedString(g_nearby_npcs_cache.response.c_str());
}

struct CachedPlayerInventoryItem {
    Json item;
    std::string lower_name;
    std::string category;
    std::string search_blob;
    std::string id_str;
    RE::FormID form_id{};
    double value{};
    double weight{};
    double count{};
    double vw{};
    bool equipped{};
    bool equippable{};
    bool first_stack_for_form{};
};
struct PlayerInventoryPageCache {
    bool valid{ false };
    std::uint64_t timestamp_ms{ 0 };
    std::vector<CachedPlayerInventoryItem> items;
};
PlayerInventoryPageCache g_inventory_page_cache;

Json ReadInventoryPage(const Json& options)
{
    auto* player=RE::PlayerCharacter::GetSingleton();
    if(!player)return {{"available",false},{"results",Json::array()}};
    if(!options.is_object())throw std::invalid_argument("Inventory options must be an object.");
    const auto query=LowerAscii(options.value("query",std::string{}));
    const auto category=options.value("category",std::string("all"));
    const auto wear=options.value("equipment",std::string("all"));
    const auto sort=options.value("sort",std::string("name"));
    const auto offset=std::clamp(options.value("offset",0),0,1000000);
    const auto limit=std::clamp(options.value("limit",64),1,512);
    if(query.size()>256)throw std::invalid_argument("Inventory search is too long.");

    constexpr std::uint64_t kInventoryPageCacheTtlMs = 15000;
    const auto now_ms = SharedSnapshotNowMs();
    if (!g_inventory_page_cache.valid || (now_ms - g_inventory_page_cache.timestamp_ms) >= kInventoryPageCacheTtlMs) {
        g_inventory_page_cache.items.clear();
        const auto inventory = player->GetInventory();
        g_inventory_page_cache.items.reserve(inventory.size());
        const auto* right_hand_obj = player->GetEquippedObject(false);
        const auto* left_hand_obj = player->GetEquippedObject(true);
        const auto* current_ammo_obj = player->GetCurrentAmmo();
        for (const auto& [form, entry] : inventory) {
            if (!form || entry.first <= 0) continue;
            const bool equipped = (entry.second && entry.second->IsWorn()) ||
                right_hand_obj == form || left_hand_obj == form || current_ammo_obj == form;
            if (!IsPlayableNamedInventoryItem(form, entry.second.get(), equipped)) continue;
            const bool equippable = !(ItemEquipSlots(form).empty() || form->Is(RE::FormType::Scroll));
            const auto kind = std::string(ItemCategory(form->GetFormType()));
            const auto lower_kind = LowerAscii(kind);
            char hex[11]{};
            std::snprintf(hex, sizeof(hex), "0x%08x", form->GetFormID());
            bool is_first = true;
            AppendPlayerInventoryItems(form, entry.first, equipped, entry.second.get(), [&](Json&& item) {
                CachedPlayerInventoryItem cached;
                cached.id_str = item.value("id", std::string{});
                cached.lower_name = LowerAscii(item.value("name", std::string{}));
                cached.search_blob = cached.lower_name + " " + lower_kind + " " + LowerAscii(cached.id_str) +
                    " " + hex + (item.value("stolen", false) ? " stolen" : "");
                cached.category = kind;
                cached.form_id = form->GetFormID();
                cached.value = item.value("value", 0.0);
                cached.weight = item.value("weight", 0.0);
                cached.count = item.value("count", 0.0);
                cached.vw = cached.value / std::max(cached.weight, 0.05);
                cached.equipped = item.value("equipped", false);
                cached.equippable = equippable;
                cached.first_stack_for_form = std::exchange(is_first, false);
                cached.item = std::move(item);
                g_inventory_page_cache.items.push_back(std::move(cached));
            });
        }
        g_inventory_page_cache.timestamp_ms = now_ms;
        g_inventory_page_cache.valid = true;
    }

    const bool equippable_only=(wear=="equippable"||category=="equippable");
    std::vector<const CachedPlayerInventoryItem*> filtered;
    filtered.reserve(g_inventory_page_cache.items.size());
    std::unordered_set<std::string> categories;
    std::size_t total{};
    for (const auto& cached : g_inventory_page_cache.items) {
        if (equippable_only && !cached.equippable) continue;
        if (cached.first_stack_for_form) {
            ++total;
            categories.insert(cached.category);
        }
        if (category != "all" && category != "equippable" && category != cached.category) continue;
        if ((wear == "equipped" && !cached.equipped) || (wear == "carried" && cached.equipped)) continue;
        if (!query.empty() && cached.search_blob.find(query) == std::string::npos) continue;
        filtered.push_back(&cached);
    }
    std::sort(filtered.begin(), filtered.end(), [&](const CachedPlayerInventoryItem* a, const CachedPlayerInventoryItem* b) {
        if (sort == "vw") {
            if (std::abs(a->vw - b->vw) > 1e-6) return a->vw > b->vw;
        } else if (sort == "value") {
            if (a->value != b->value) return a->value > b->value;
        } else if (sort == "weight") {
            if (a->weight != b->weight) return a->weight > b->weight;
        } else if (sort == "count") {
            if (a->count != b->count) return a->count > b->count;
        }
        return a->lower_name != b->lower_name ? a->lower_name < b->lower_name : a->id_str < b->id_str;
    });
    Json results=Json::array();
    for(auto i=static_cast<std::size_t>(offset);i<filtered.size()&&results.size()<static_cast<std::size_t>(limit);++i){
        results.push_back(filtered[i]->item);
    }
    std::vector<std::string> sorted_categories(categories.begin(),categories.end());std::sort(sorted_categories.begin(),sorted_categories.end());
    return {{"available",true},{"results",std::move(results)},{"totalTypes",total},{"totalMatches",filtered.size()},
        {"offset",offset},{"categories",sorted_categories}};
}

bool IsMannequinReference(RE::TESObjectREFR* refr) noexcept
{
    if (refr == nullptr) return false;
    auto* actor = refr->As<RE::Actor>();
    if (actor == nullptr || actor == RE::PlayerCharacter::GetSingleton() || actor->IsDead()) {
        return false;
    }
    if (const auto* race = actor->GetRace(); race != nullptr) {
        if ((race->GetFormID() & 0x00FFFFFFU) == 0x0010760AU) return true;
        if (const auto* rname = race->GetFullName(); rname != nullptr && LowerAscii(rname).find("mannequin") != std::string::npos) return true;
        if (const auto* redid = race->GetFormEditorID(); redid != nullptr && LowerAscii(redid).find("mannequin") != std::string::npos) return true;
    }
    if (const auto* base = actor->GetActorBase(); base != nullptr) {
        if (const auto* bname = base->GetName(); bname != nullptr && LowerAscii(bname).find("mannequin") != std::string::npos) return true;
        if (const auto* bedid = base->GetFormEditorID(); bedid != nullptr && LowerAscii(bedid).find("mannequin") != std::string::npos) return true;
    }
    if (const auto* dname = actor->GetDisplayFullName(); dname != nullptr && LowerAscii(dname).find("mannequin") != std::string::npos) {
        return true;
    }
    return false;
}

RE::EnchantmentItem* ResolveItemEnchantment(RE::TESBoundObject* form, RE::ExtraDataList* extra, RE::InventoryEntryData* entry = nullptr) noexcept
{
    if (extra != nullptr) {
        if (const auto* ex_ench = extra->GetByType<RE::ExtraEnchantment>(); ex_ench != nullptr && ex_ench->enchantment != nullptr) {
            return ex_ench->enchantment;
        }
    }
    if (entry != nullptr) {
        if (auto* entry_ench = entry->GetEnchantment(); entry_ench != nullptr) {
            return entry_ench;
        }
    }
    if (form != nullptr) {
        if (const auto* ench_form = form->As<RE::TESEnchantableForm>(); ench_form != nullptr) {
            return ench_form->formEnchanting;
        }
    }
    return nullptr;
}

bool HasLightArchetypeEnchantment(RE::TESBoundObject* form, RE::ExtraDataList* extra, RE::InventoryEntryData* entry = nullptr) noexcept
{
    const auto* ench = ResolveItemEnchantment(form, extra, entry);
    if (ench == nullptr) {
        return false;
    }
    for (const auto* eff : ench->effects) {
        if (eff != nullptr && eff->baseEffect != nullptr &&
            eff->baseEffect->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kLight) {
            return true;
        }
    }
    return false;
}

Json ChangePlayerEquipment(const Json& options,bool unequip)
{
    const auto rejected=[](std::string message)->Json{return {{"accepted",false},{"message",std::move(message)}};};
    if(!options.is_object())return rejected("Equipment options must be an object.");
    const auto form_id=ParseItemFormIdField(options,"formId");
    const auto ref_id=ParseItemFormIdField(options,"referenceId");
    auto* player=RE::PlayerCharacter::GetSingleton();auto* manager=RE::ActorEquipManager::GetSingleton();
    auto* target=ref_id!=0?RE::TESForm::LookupByID<RE::Actor>(ref_id):static_cast<RE::Actor*>(player);
    auto* raw_form=form_id!=0?RE::TESForm::LookupByID(form_id):nullptr;
    if(!player||!target||!manager||!raw_form||target->IsDead())return rejected("Character equipment is unavailable.");
    auto slot=options.value("slot",std::string("auto"));const auto slots=ItemEquipSlots(raw_form);
    if(slots.empty())return rejected("Only carried armor, weapons, spells and ammunition can be equipped here.");
    if(slot=="auto")slot=slots[0].get<std::string>();
    if(std::find(slots.begin(),slots.end(),Json(slot))==slots.end())return rejected("This item does not fit that equipment slot.");
    if(auto* spell=raw_form->As<RE::SpellItem>()){
        const bool left_hand=(slot=="offHand");
        const auto* defaults=RE::BGSDefaultObjectManager::GetSingleton();
        const auto* hand_slot=defaults?defaults->GetObject<RE::BGSEquipSlot>(left_hand?RE::DEFAULT_OBJECT::kLeftHandEquip:RE::DEFAULT_OBJECT::kRightHandEquip):nullptr;
        if(unequip){
            const auto hand_idx=left_hand?RE::Actor::SlotTypes::kLeftHand:RE::Actor::SlotTypes::kRightHand;
            target->DeselectSpell(spell);
            if(target->GetActorRuntimeData().selectedSpells[hand_idx]==spell){
                target->GetActorRuntimeData().selectedSpells[hand_idx]=nullptr;
            }
        }else{
            if(spell->IsTwoHanded())manager->EquipSpell(target,spell,nullptr);
            else manager->EquipSpell(target,spell,hand_slot);
        }
        RE::PlaySound(unequip?"UIMenuCancel":"UIMenuOK");
        skyultra::InvalidateSharedGameDataCache();
        auto updated_equipment=ReadActorEquipment(target,true);
        if(unequip&&slot!="auto"&&updated_equipment.contains(slot))updated_equipment[slot]=nullptr;
        return {{"accepted",true},{"formId",std::to_string(form_id)},{"slot",slot},{"equipment",std::move(updated_equipment)},{"message",unequip?"Unequipped spell.":"Equipped spell."}};
    }
    auto* form=raw_form->As<RE::TESBoundObject>();
    if(!form)return rejected("Character equipment is unavailable.");
    const bool is_mannequin = target != player && IsMannequinReference(target);
    auto inventory=target->GetInventory();auto found=inventory.find(form);
    if(target!=player&&!unequip&&(found==inventory.end()||found->second.first<=0)){
        if(is_mannequin){
            auto player_inv=player->GetInventory();
            auto pfound=player_inv.find(form);
            if(pfound!=player_inv.end()&&pfound->second.first>0){
                auto* pentry=pfound->second.second.get();
                if(pentry&&pentry->IsQuestObject())return rejected("Quest items cannot be placed on a mannequin.");
                RE::ExtraDataList* pextra=nullptr;
                if(pentry&&pentry->extraLists){
                    for(auto* cand:*pentry->extraLists){if(cand){pextra=cand;break;}}
                }
                const bool pworn=(pentry&&pentry->IsWorn())||player->GetEquippedObject(false)==form||player->GetEquippedObject(true)==form||player->GetCurrentAmmo()==form;
                if(pworn&&pfound->second.first<=1){
                    (void)manager->UnequipObject(player,form,pextra,1,nullptr,false,false,true,false,nullptr);
                }
                const std::int32_t xfer_count=form->Is(RE::FormType::Ammo)?pfound->second.first:1;
                player->RemoveItem(form,xfer_count,RE::ITEM_REMOVE_REASON::kStoreInContainer,pextra,target);
            }else{
                return rejected("Neither you nor the mannequin has this item.");
            }
        }else{
            target->AddObjectToContainer(form,nullptr,form->Is(RE::FormType::Ammo)?24:1,nullptr);
        }
        inventory=target->GetInventory();found=inventory.find(form);
    }
    if(target==player&&(found==inventory.end()||found->second.first<=0))return rejected("The player no longer carries this item.");
    auto* entry=found!=inventory.end()?found->second.second.get():nullptr;
    const bool is_worn=(entry&&entry->IsWorn())||target!=player;
    const bool in_hand=slot=="mainHand"?target->GetEquippedObject(false)==form:slot=="offHand"?target->GetEquippedObject(true)==form:false;
    if(unequip&&target==player&&!(form->Is(RE::FormType::Weapon)?in_hand:is_worn||in_hand||target->GetCurrentAmmo()==form))return rejected("That item is no longer equipped in this slot.");
    const RE::BGSEquipSlot* hand{};
    const auto* weapon=form->As<RE::TESObjectWEAP>();
    const bool both_hands=weapon&&(weapon->IsTwoHandedSword()||weapon->IsTwoHandedAxe()||weapon->IsBow()||weapon->IsCrossbow());
    if((weapon&&!both_hands)||form->Is(RE::FormType::Scroll)||form->Is(RE::FormType::Light)){
        const auto* defaults=RE::BGSDefaultObjectManager::GetSingleton();
        if(defaults)hand=defaults->GetObject<RE::BGSEquipSlot>(slot=="offHand"?RE::DEFAULT_OBJECT::kLeftHandEquip:RE::DEFAULT_OBJECT::kRightHandEquip);
        if(!hand)return rejected("The hand equipment slot is unavailable.");
    }
    RE::ExtraDataList* extra{};
    if(entry&&entry->extraLists)for(auto* candidate:*entry->extraLists){
        if(!candidate)continue;
        const bool worn=slot=="offHand"&&weapon?candidate->HasType<RE::ExtraWornLeft>():candidate->HasType<RE::ExtraWorn>()||candidate->HasType<RE::ExtraWornLeft>();
        if(unequip?worn:!candidate->HasType<RE::ExtraWorn>()&&!candidate->HasType<RE::ExtraWornLeft>()){extra=candidate;break;}
    }
    const auto count=form->Is(RE::FormType::Ammo)?static_cast<std::uint32_t>(found!=inventory.end()&&found->second.first>0?found->second.first:1):1U;
    auto* ui = RE::UI::GetSingleton();
    const std::uint32_t saved_pause_count = (ui != nullptr) ? ui->numPausesGame : 0U;
    const bool is_paused = (saved_pause_count > 0U);
    // Match Skyrim's InventoryMenu equip path: execute immediately on the game thread
    // while paused (a_queueEquip=false, matching InventoryMenu::EquipOrUnequipEntry),
    // and queue while the world is running (a_queueEquip=true) to avoid actor-job races.
    // Mannequins (non-living container refs) always execute immediately with Update3DModel.
    const bool queue_equip = !is_mannequin && !is_paused;
    const bool apply_now = false;
    if(unequip){
        const bool unequipped = manager->UnequipObject(target,form,extra,count,hand,queue_equip,false,target==player,apply_now);
        if(extra!=nullptr && (!unequipped || (!queue_equip && entry && entry->IsWorn()))){
            (void)manager->UnequipObject(target,form,nullptr,count,hand,queue_equip,false,target==player,apply_now);
        }
        if (target == player && HasLightArchetypeEnchantment(form, extra, entry)) {
            std::size_t remaining_light_fx = 0U;
            if (auto* magic_target = player->AsMagicTarget(); magic_target != nullptr) {
                RE::MagicTarget::EffectVisitor trace_visitor([&](RE::ActiveEffect* ae) {
                    if (ae != nullptr) {
                        const auto* base = ae->GetBaseObject();
                        if (base != nullptr && base->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kLight) {
                            ++remaining_light_fx;
                        }
                    }
                    return RE::BSContainer::ForEachResult::kContinue;
                });
                magic_target->VisitEffects(trace_visitor);
            }
            const bool has_extra_magic = (player->extraList.GetByType<RE::ExtraMagicLight>() != nullptr);
            SKSE::log::info(
                "SkyUltra unequip light-effect trace {:08X}: paused={}, queueEquip={}, unequipped={}, remainingLightFx={}, extraMagicLight={}.",
                form->GetFormID(),
                saved_pause_count > 0U,
                queue_equip,
                unequipped,
                remaining_light_fx,
                has_extra_magic);
        }
        if(is_mannequin&&options.value("returnToPlayer",false)){
            auto post_inv=target->GetInventory();
            auto mfound=post_inv.find(form);
            if(mfound!=post_inv.end()&&mfound->second.first>0){
                RE::ExtraDataList* mextra=nullptr;
                if(mfound->second.second&&mfound->second.second->extraLists){
                    for(auto* cand:*mfound->second.second->extraLists){if(cand){mextra=cand;break;}}
                }
                target->RemoveItem(form,static_cast<std::int32_t>(count),RE::ITEM_REMOVE_REASON::kRemove,mextra,player);
            }
        }
    }else{
        manager->EquipObject(target,form,extra,count,hand,queue_equip,false,target==player,apply_now);
        if(target==player && HasLightArchetypeEnchantment(form, extra, entry)){
            bool found_effect = false;
            std::uint32_t effect_flags = 0U;
            std::size_t hit_effect_count = 0U;
            bool any_attached = false;
            bool native_light = false;
            if(auto* magic_target = player->AsMagicTarget(); magic_target != nullptr){
                RE::MagicTarget::EffectVisitor trace_visitor([&](RE::ActiveEffect* ae) {
                    if(ae == nullptr) return RE::BSContainer::ForEachResult::kContinue;
                    const auto* base = ae->GetBaseObject();
                    if(base == nullptr || base->GetArchetype() != RE::EffectArchetypes::ArchetypeID::kLight){
                        return RE::BSContainer::ForEachResult::kContinue;
                    }
                    const bool source_match = (ae->source == form);
                    if(!source_match && found_effect){
                        return RE::BSContainer::ForEachResult::kContinue;
                    }
                    const bool is_active = !ae->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled);
                    if(found_effect && !is_active){
                        return RE::BSContainer::ForEachResult::kContinue;
                    }
                    found_effect = true;
                    effect_flags = ae->flags.underlying();
                    hit_effect_count = 0U;
                    any_attached = false;
                    if(ae->hitEffects != nullptr){
                        for(auto* ref_fx : *ae->hitEffects){
                            if(ref_fx != nullptr){
                                ++hit_effect_count;
                                if(ref_fx->GetAttached()){
                                    any_attached = true;
                                }
                            }
                        }
                    }
                    const auto* light_fx = skyrim_cast<RE::LightEffect*>(ae);
                    native_light = (light_fx != nullptr && light_fx->light != nullptr);
                    return (source_match && is_active) ? RE::BSContainer::ForEachResult::kStop : RE::BSContainer::ForEachResult::kContinue;
                });
                magic_target->VisitEffects(trace_visitor);
            }
            const auto* cam = RE::PlayerCamera::GetSingleton();
            SKSE::log::info(
                "SkyUltra equip light-effect trace {:08X}: paused={}, queueEquip={}, found={}, flags=0x{:X}, hitEffects={}, anyAttached={}, nativeLight={}, is3rdPerson={}, has3D={}.",
                form->GetFormID(),
                saved_pause_count > 0U,
                queue_equip,
                found_effect,
                effect_flags,
                hit_effect_count,
                any_attached,
                native_light,
                cam != nullptr && cam->IsInThirdPerson(),
                player->Is3DLoaded());
        }
    }
    if(is_mannequin || (is_paused && target->Is3DLoaded())){
        target->Update3DModel();
    }else if(queue_equip&&target==player){
        if(const auto* task_if = SKSE::GetTaskInterface(); task_if != nullptr){
            task_if->AddTask([]() {
                skyultra::InvalidateSharedGameDataCache();
            });
        }
    }
    skyultra::InvalidateSharedGameDataCache();
    auto updated_equipment=ReadActorEquipment(target,true);
    if(unequip){
        if(slot!="auto"&&updated_equipment.contains(slot))updated_equipment[slot]=nullptr;
        for(auto& [equip_key, equip_val] : updated_equipment.items()){
            if(equip_val.is_object() && ParseItemFormIdField(equip_val, "id") == form_id){
                equip_val = nullptr;
            }
        }
    }else{
        auto equipped_item = PlayerItemJson(form, static_cast<std::int32_t>(count), true, entry, target == player);
        if(equipped_item.is_object()){
            if(form->As<RE::TESObjectARMO>() != nullptr){
                for(const auto& s : slots){
                    if(s.is_string() && s != "auto") updated_equipment[s.get<std::string>()] = equipped_item;
                }
            }else if(both_hands){
                updated_equipment["mainHand"] = equipped_item;
                updated_equipment["offHand"] = equipped_item;
            }else if(slot != "auto"){
                updated_equipment[slot] = equipped_item;
            }
        }
    }
    return {{"accepted",true},{"formId",std::to_string(form_id)},{"slot",slot},{"equipment",std::move(updated_equipment)},{"message",unequip?"Unequipped item.":"Equipped item."}};
}

std::string EncodeBytesBase64WithMime(const std::uint8_t* data, std::size_t size, std::string_view mime)
{
    if (data == nullptr || size == 0) return {};
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result = "data:";
    result.append(mime);
    result.append(";base64,");
    result.reserve(result.size() + ((size + 2) / 3) * 4);
    for (std::size_t i = 0; i < size; i += 3) {
        const unsigned value =
            (static_cast<unsigned>(data[i]) << 16U) |
            (i + 1 < size ? (static_cast<unsigned>(data[i + 1]) << 8U) : 0U) |
            (i + 2 < size ? static_cast<unsigned>(data[i + 2]) : 0U);
        result.push_back(kAlphabet[(value >> 18U) & 63U]);
        result.push_back(kAlphabet[(value >> 12U) & 63U]);
        result.push_back(i + 1 < size ? kAlphabet[(value >> 6U) & 63U] : '=');
        result.push_back(i + 2 < size ? kAlphabet[value & 63U] : '=');
    }
    return result;
}

std::string EncodeBytesBase64(const std::uint8_t* data, std::size_t size)
{
    return EncodeBytesBase64WithMime(data, size, "image/png");
}

std::string EncodeBgraPngDataUrl(const std::vector<std::uint8_t>& bgra, std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0 || bgra.size() < static_cast<std::size_t>(width) * height * 4U) {
        return {};
    }
    std::uint32_t out_w = width;
    std::uint32_t out_h = height;
    std::vector<std::uint8_t> scaled;
    const std::uint8_t* src_pixels = bgra.data();
    constexpr std::uint32_t kMaxBookImgEdge = 1024U;
    if (width > kMaxBookImgEdge || height > kMaxBookImgEdge) {
        if (width >= height) {
            out_w = kMaxBookImgEdge;
            out_h = std::max(1U, static_cast<std::uint32_t>(std::lround(static_cast<double>(height) * kMaxBookImgEdge / width)));
        } else {
            out_h = kMaxBookImgEdge;
            out_w = std::max(1U, static_cast<std::uint32_t>(std::lround(static_cast<double>(width) * kMaxBookImgEdge / height)));
        }
        scaled.resize(static_cast<std::size_t>(out_w) * out_h * 4U);
        for (std::uint32_t y = 0; y < out_h; ++y) {
            const std::uint32_t sy = std::min(height - 1U, (y * height) / out_h);
            for (std::uint32_t x = 0; x < out_w; ++x) {
                const std::uint32_t sx = std::min(width - 1U, (x * width) / out_w);
                const std::size_t si = (static_cast<std::size_t>(sy) * width + sx) * 4U;
                const std::size_t di = (static_cast<std::size_t>(y) * out_w + x) * 4U;
                scaled[di + 0] = bgra[si + 0];
                scaled[di + 1] = bgra[si + 1];
                scaled[di + 2] = bgra[si + 2];
                scaled[di + 3] = bgra[si + 3];
            }
        }
        src_pixels = scaled.data();
    }

    struct ComInit {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ~ComInit() { if (SUCCEEDED(hr)) CoUninitialize(); }
    } com_init;

    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) || !factory) {
        return {};
    }

    const auto encode_with_container = [&](const GUID& container_guid, WICPixelFormatGUID desired_fmt, const std::uint8_t* pixels, std::uint32_t stride, std::uint32_t total_bytes, std::uint64_t max_bytes) -> std::vector<std::uint8_t> {
        ComPtr<IStream> memory;
        if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &memory)) || !memory) {
            return {};
        }
        ComPtr<IWICBitmapEncoder> encoder;
        ComPtr<IWICBitmapFrameEncode> frame;
        if (FAILED(factory->CreateEncoder(container_guid, nullptr, &encoder)) ||
            FAILED(encoder->Initialize(memory.Get(), WICBitmapEncoderNoCache)) ||
            FAILED(encoder->CreateNewFrame(&frame, nullptr)) ||
            FAILED(frame->Initialize(nullptr)) ||
            FAILED(frame->SetSize(out_w, out_h))) {
            return {};
        }
        WICPixelFormatGUID fmt = desired_fmt;
        if (FAILED(frame->SetPixelFormat(&fmt)) ||
            FAILED(frame->WritePixels(out_h, stride, total_bytes, const_cast<BYTE*>(pixels))) ||
            FAILED(frame->Commit()) ||
            FAILED(encoder->Commit())) {
            return {};
        }
        STATSTG stat{};
        if (FAILED(memory->Stat(&stat, STATFLAG_NONAME)) || stat.cbSize.QuadPart == 0 || stat.cbSize.QuadPart > max_bytes) {
            return {};
        }
        if (FAILED(memory->Seek({}, STREAM_SEEK_SET, nullptr))) {
            return {};
        }
        std::vector<std::uint8_t> out_bytes(static_cast<std::size_t>(stat.cbSize.QuadPart));
        ULONG read_bytes = 0;
        if (FAILED(memory->Read(out_bytes.data(), static_cast<ULONG>(out_bytes.size()), &read_bytes)) || read_bytes != out_bytes.size()) {
            return {};
        }
        return out_bytes;
    };

    if (auto png_bytes = encode_with_container(GUID_ContainerFormatPng, GUID_WICPixelFormat32bppBGRA, src_pixels, out_w * 4U, out_w * out_h * 4U, 380000ULL); !png_bytes.empty()) {
        return EncodeBytesBase64WithMime(png_bytes.data(), png_bytes.size(), "image/png");
    }

    std::vector<std::uint8_t> bgr24(static_cast<std::size_t>(out_w) * out_h * 3U);
    const std::size_t total_px = static_cast<std::size_t>(out_w) * out_h;
    for (std::size_t i = 0; i < total_px; ++i) {
        bgr24[i * 3 + 0] = src_pixels[i * 4 + 0];
        bgr24[i * 3 + 1] = src_pixels[i * 4 + 1];
        bgr24[i * 3 + 2] = src_pixels[i * 4 + 2];
    }
    if (auto jpg_bytes = encode_with_container(GUID_ContainerFormatJpeg, GUID_WICPixelFormat24bppBGR, bgr24.data(), out_w * 3U, out_w * out_h * 3U, 1800000ULL); !jpg_bytes.empty()) {
        return EncodeBytesBase64WithMime(jpg_bytes.data(), jpg_bytes.size(), "image/jpeg");
    }

    if (auto png_large = encode_with_container(GUID_ContainerFormatPng, GUID_WICPixelFormat32bppBGRA, src_pixels, out_w * 4U, out_w * out_h * 4U, 2000000ULL); !png_large.empty()) {
        return EncodeBytesBase64WithMime(png_large.data(), png_large.size(), "image/png");
    }
    return {};
}

void DecodeRgb565(std::uint16_t c, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) noexcept
{
    r = static_cast<std::uint8_t>((((c >> 11U) & 0x1FU) * 255U + 15U) / 31U);
    g = static_cast<std::uint8_t>((((c >> 5U) & 0x3FU) * 255U + 31U) / 63U);
    b = static_cast<std::uint8_t>(((c & 0x1FU) * 255U + 15U) / 31U);
}

void DecodeBc4Channel(const std::uint8_t* blk, std::uint8_t out_vals[16]) noexcept
{
    const std::uint8_t c0 = blk[0];
    const std::uint8_t c1 = blk[1];
    std::uint8_t pal[8]{ c0, c1, 0, 0, 0, 0, 0, 255 };
    if (c0 > c1) {
        for (int i = 1; i <= 6; ++i) {
            pal[i + 1] = static_cast<std::uint8_t>(((7 - i) * c0 + i * c1 + 3) / 7);
        }
    } else {
        for (int i = 1; i <= 4; ++i) {
            pal[i + 1] = static_cast<std::uint8_t>(((5 - i) * c0 + i * c1 + 2) / 5);
        }
        pal[6] = 0;
        pal[7] = 255;
    }
    std::uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) {
        bits |= (static_cast<std::uint64_t>(blk[2 + i]) << (8 * i));
    }
    for (int i = 0; i < 16; ++i) {
        out_vals[i] = pal[(bits >> (3 * i)) & 0x07U];
    }
}

void DecodeBc7Block(const std::uint8_t* blk, std::uint8_t out_rgba[16][4]) noexcept
{
    static constexpr std::uint8_t kWeights2[4] = { 0, 21, 43, 64 };
    static constexpr std::uint8_t kWeights3[8] = { 0, 9, 18, 27, 37, 46, 55, 64 };
    static constexpr std::uint8_t kWeights4[16] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

    static constexpr std::uint8_t kPart2[64][16] = {
        {0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1}, {0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1},
        {0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1}, {0,0,0,1,0,0,1,1,0,0,1,1,0,1,1,1},
        {0,0,0,0,0,0,0,1,0,0,0,1,0,0,1,1}, {0,0,1,1,0,1,1,1,0,1,1,1,1,1,1,1},
        {0,0,0,1,0,0,1,1,0,1,1,1,1,1,1,1}, {0,0,0,0,0,0,0,1,0,0,1,1,0,1,1,1},
        {0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,1}, {0,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1},
        {0,0,0,0,0,0,0,1,0,1,1,1,1,1,1,1}, {0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,1},
        {0,0,0,1,0,1,1,1,1,1,1,1,1,1,1,1}, {0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1},
        {0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1}, {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1},
        {0,0,0,0,1,0,0,0,1,1,1,0,1,1,1,1}, {0,1,1,1,0,0,0,1,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,1,0,0,0,1,1,1,0}, {0,1,1,1,0,0,1,1,0,0,0,1,0,0,0,0},
        {0,0,1,1,0,0,0,1,0,0,0,0,0,0,0,0}, {0,0,0,0,1,0,0,0,1,1,0,0,1,1,1,0},
        {0,0,0,0,0,0,0,0,1,0,0,0,1,1,0,0}, {0,1,1,1,0,0,1,1,0,0,1,1,0,0,0,1},
        {0,0,1,1,0,0,0,1,0,0,0,1,0,0,0,0}, {0,0,0,0,1,0,0,0,1,0,0,0,1,1,0,0},
        {0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0}, {0,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0},
        {0,0,0,1,0,1,1,1,1,1,1,0,1,0,0,0}, {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,1,1,1,0,0,0,1,1,0,0,0,1,1,1,0}, {0,0,1,1,1,0,0,1,1,0,0,1,1,1,0,0},
        {0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1}, {0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1},
        {0,1,0,1,1,0,1,0,0,1,0,1,1,0,1,0}, {0,0,1,1,0,0,1,1,1,1,0,0,1,1,0,0},
        {0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0}, {0,1,0,1,0,1,0,1,1,0,1,0,1,0,1,0},
        {0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1}, {0,1,0,1,1,0,1,0,1,0,1,0,0,1,0,1},
        {0,1,1,1,0,0,1,1,1,1,0,0,1,1,1,0}, {0,0,0,1,0,0,1,1,1,1,0,0,1,0,0,0},
        {0,0,1,1,0,0,1,0,0,1,0,0,1,1,0,0}, {0,0,1,1,1,0,1,1,1,1,0,1,1,1,0,0},
        {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0}, {0,0,1,1,1,1,0,0,1,1,0,0,0,0,1,1},
        {0,1,1,0,0,1,1,0,1,0,0,1,1,0,0,1}, {0,0,0,0,0,1,1,0,0,1,1,0,0,0,0,0},
        {0,1,0,0,1,1,1,0,0,1,0,0,0,0,0,0}, {0,0,1,0,0,1,1,1,0,0,1,0,0,0,0,0},
        {0,0,0,0,0,0,1,0,0,1,1,1,0,0,1,0}, {0,0,0,0,0,1,0,0,1,1,1,0,0,1,0,0},
        {0,1,1,0,1,1,0,0,1,0,0,1,0,0,1,1}, {0,0,1,1,0,1,1,0,1,1,0,0,1,0,0,1},
        {0,1,1,0,0,0,1,1,1,0,0,1,1,1,0,0}, {0,0,1,1,1,0,0,1,1,1,0,0,0,1,1,0},
        {0,1,1,0,1,1,0,0,1,1,0,0,1,0,0,1}, {0,1,1,0,0,0,1,1,0,0,1,1,1,0,0,1},
        {0,1,1,1,1,1,1,0,1,0,0,0,0,0,0,1}, {0,0,0,1,1,0,0,0,1,1,1,0,0,1,1,1},
        {0,0,0,0,1,1,1,1,0,0,1,1,0,0,1,1}, {0,0,1,1,0,0,1,1,1,1,1,1,0,0,0,0},
        {0,0,1,0,0,0,1,0,1,1,1,0,1,1,1,0}, {0,1,0,0,0,1,0,0,0,1,1,1,0,1,1,1}
    };

    static constexpr std::uint8_t kPart3[64][16] = {
        {0,0,1,1,0,0,1,1,0,2,2,1,2,2,2,2}, {0,0,0,1,0,0,1,1,2,2,1,1,2,2,2,1},
        {0,0,0,0,2,0,0,1,2,2,1,1,2,2,1,1}, {0,2,2,2,0,0,2,2,0,0,1,1,0,1,1,1},
        {0,0,0,0,0,0,0,0,1,1,2,2,1,1,2,2}, {0,0,1,1,0,0,1,1,0,0,2,2,0,0,2,2},
        {0,0,2,2,0,0,2,2,1,1,1,1,1,1,1,1}, {0,0,1,1,0,0,1,1,2,2,1,1,2,2,1,1},
        {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2}, {0,0,0,0,1,1,1,1,1,1,1,1,2,2,2,2},
        {0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2}, {0,0,1,2,0,0,1,2,0,0,1,2,0,0,1,2},
        {0,1,1,2,0,1,1,2,0,1,1,2,0,1,1,2}, {0,1,2,2,0,1,2,2,0,1,2,2,0,1,2,2},
        {0,0,1,1,0,1,1,2,1,1,2,2,1,2,2,2}, {0,0,1,1,2,0,0,1,2,2,0,0,2,2,2,0},
        {0,0,0,1,0,0,1,1,0,1,1,2,1,1,2,2}, {0,1,1,1,0,0,1,1,2,0,0,1,2,2,0,0},
        {0,0,0,0,1,1,2,2,1,1,2,2,1,1,2,2}, {0,0,2,2,0,0,2,2,0,0,2,2,1,1,1,1},
        {0,1,1,1,0,1,1,1,0,2,2,2,0,2,2,2}, {0,0,0,1,0,0,0,1,2,2,2,1,2,2,2,1},
        {0,0,0,0,0,0,1,1,0,1,2,2,0,1,2,2}, {0,0,0,0,1,1,0,0,2,2,1,0,2,2,1,0},
        {0,1,2,2,0,1,2,2,0,0,1,1,0,0,0,0}, {0,0,1,2,0,0,1,2,1,1,2,2,2,2,2,2},
        {0,1,1,0,1,2,2,1,1,2,2,1,0,1,1,0}, {0,0,0,0,0,1,1,0,1,2,2,1,1,2,2,1},
        {0,0,2,2,1,1,0,2,1,1,0,2,0,0,2,2}, {0,1,1,0,0,1,1,0,2,0,0,2,2,2,2,2},
        {0,0,1,1,0,1,2,2,0,1,2,2,0,0,1,1}, {0,0,0,0,2,0,0,0,2,2,1,1,2,2,2,1},
        {0,0,0,0,0,0,0,2,1,1,2,2,1,2,2,2}, {0,2,2,2,0,0,2,2,0,0,1,2,0,0,1,1},
        {0,0,1,1,0,0,1,2,0,0,2,2,0,2,2,2}, {0,1,2,0,0,1,2,0,0,1,2,0,0,1,2,0},
        {0,0,0,0,1,1,1,1,2,2,2,2,0,0,0,0}, {0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0},
        {0,1,2,0,2,0,1,2,1,2,0,1,0,1,2,0}, {0,0,1,1,2,2,0,0,1,1,2,2,0,0,1,1},
        {0,0,1,1,1,1,2,2,2,2,0,0,0,0,1,1}, {0,1,0,1,0,1,0,1,2,2,2,2,2,2,2,2},
        {0,0,0,0,0,0,0,0,2,1,2,1,2,1,2,1}, {0,0,2,2,1,1,2,2,0,0,2,2,1,1,2,2},
        {0,0,2,2,0,0,1,1,0,0,2,2,0,0,1,1}, {0,2,2,0,1,2,2,1,0,2,2,0,1,2,2,1},
        {0,1,0,1,2,2,2,2,2,2,2,2,0,1,0,1}, {0,0,0,0,2,1,2,1,2,1,2,1,2,1,2,1},
        {0,1,0,1,0,1,0,1,0,1,0,1,2,2,2,2}, {0,2,2,2,0,1,1,1,0,2,2,2,0,1,1,1},
        {0,0,0,2,1,1,1,2,0,0,0,2,1,1,1,2}, {0,0,0,0,2,1,1,2,2,1,1,2,2,1,1,2},
        {0,2,2,2,0,1,1,1,0,1,1,1,0,2,2,2}, {0,0,0,2,1,1,1,2,1,1,1,2,0,0,0,2},
        {0,1,1,0,0,1,1,0,0,1,1,0,2,2,2,2}, {0,0,0,0,0,0,0,0,2,1,1,2,2,1,1,2},
        {0,1,1,0,0,1,1,0,2,2,2,2,2,2,2,2}, {0,0,2,2,0,0,1,1,0,0,1,1,0,0,2,2},
        {0,0,2,2,1,1,2,2,1,1,2,2,0,0,2,2}, {0,0,0,0,0,0,0,0,0,0,0,0,2,1,1,2},
        {0,0,0,2,0,0,0,1,0,0,0,2,0,0,0,1}, {0,2,2,2,1,2,2,2,0,2,2,2,1,2,2,2},
        {0,1,0,1,2,2,2,2,2,2,2,2,2,2,2,2}, {0,1,1,1,2,0,1,1,2,2,0,1,2,2,2,0}
    };

    static constexpr std::uint8_t kAnchor2[64] = {
        15,15,15,15,15,15,15,15, 15,15,15,15,15,15,15,15,
        15, 2, 8, 2, 2, 8, 8,15,  2, 8, 2, 2, 8, 8, 2, 2,
        15,15, 6, 8, 2, 8,15,15,  2, 8, 2, 2, 2,15,15, 6,
         6, 2, 6, 8,15,15, 2, 2, 15,15,15,15,15, 2, 2,15
    };

    static constexpr std::uint8_t kAnchor3A[64] = {
         3, 3,15,15, 8, 3,15,15,  8, 8, 6, 6, 6, 5, 3, 3,
         3, 3, 8,15, 3, 3, 6,10,  5, 8, 8, 6, 8, 5,15,15,
         8,15, 3, 5, 6,10, 8,15, 15, 3,15, 5,15,15,15,15,
         3,15, 5, 5, 5, 8, 5,10,  5,10, 8,13,15,12, 3, 3
    };

    static constexpr std::uint8_t kAnchor3B[64] = {
        15, 8, 8, 3,15,15, 3, 8, 15,15,15,15,15,15,15, 8,
        15, 8,15, 3,15, 8,15, 8,  3,15, 6,10,15,15,10, 8,
        15, 3,15,10,10, 8, 9,10,  6,15, 8,15, 3, 6, 6, 8,
        15, 3,15,15,15,15,15,15, 15,15,15,15, 3,15,15, 8
    };

    struct ModeInfo {
        int ns;
        int pb;
        int rb;
        int isb;
        int cb;
        int ab;
        int epb;
        int spb;
        int ib;
        int ib2;
    };
    static constexpr ModeInfo kModes[8] = {
        { 3, 4, 0, 0, 4, 0, 1, 0, 3, 0 },
        { 2, 6, 0, 0, 6, 0, 0, 1, 3, 0 },
        { 3, 6, 0, 0, 5, 0, 0, 0, 2, 0 },
        { 2, 6, 0, 0, 7, 0, 1, 0, 2, 0 },
        { 1, 0, 2, 1, 5, 6, 0, 0, 2, 3 },
        { 1, 0, 2, 0, 7, 8, 0, 0, 2, 2 },
        { 1, 0, 0, 0, 7, 7, 1, 0, 4, 0 },
        { 2, 6, 0, 0, 5, 5, 1, 0, 2, 0 }
    };

    std::uint64_t lo = 0, hi = 0;
    std::memcpy(&lo, blk, 8U);
    std::memcpy(&hi, blk + 8, 8U);

    int bit_pos = 0;
    const auto read_bits = [&](int count) noexcept -> std::uint32_t {
        if (count <= 0) return 0U;
        std::uint32_t val = 0U;
        for (int i = 0; i < count; ++i) {
            const int p = bit_pos++;
            const std::uint64_t word = (p < 64) ? lo : hi;
            const int shift = (p < 64) ? p : (p - 64);
            val |= static_cast<std::uint32_t>((word >> shift) & 1ULL) << i;
        }
        return val;
    };

    int mode = -1;
    for (int m = 0; m < 8; ++m) {
        if (read_bits(1) != 0U) {
            mode = m;
            break;
        }
    }
    if (mode < 0 || mode > 7) {
        for (int i = 0; i < 16; ++i) {
            out_rgba[i][0] = out_rgba[i][1] = out_rgba[i][2] = 0;
            out_rgba[i][3] = 255;
        }
        return;
    }

    const ModeInfo& mi = kModes[mode];
    const int partition = static_cast<int>(read_bits(mi.pb));
    const int rotation = static_cast<int>(read_bits(mi.rb));
    const int idx_mode = static_cast<int>(read_bits(mi.isb));

    std::uint8_t ep[6][4]{};
    const int num_ep = mi.ns * 2;
    for (int ch = 0; ch < 3; ++ch) {
        for (int e = 0; e < num_ep; ++e) {
            ep[e][ch] = static_cast<std::uint8_t>(read_bits(mi.cb));
        }
    }
    if (mi.ab > 0) {
        for (int e = 0; e < num_ep; ++e) {
            ep[e][3] = static_cast<std::uint8_t>(read_bits(mi.ab));
        }
    } else {
        for (int e = 0; e < num_ep; ++e) {
            ep[e][3] = 255;
        }
    }

    std::uint8_t pbits[6]{};
    if (mi.epb > 0) {
        for (int e = 0; e < num_ep; ++e) {
            pbits[e] = static_cast<std::uint8_t>(read_bits(1));
        }
    } else if (mi.spb > 0) {
        for (int s = 0; s < mi.ns; ++s) {
            const auto pb = static_cast<std::uint8_t>(read_bits(1));
            pbits[s * 2 + 0] = pb;
            pbits[s * 2 + 1] = pb;
        }
    }

    const int color_prec = mi.cb + (mi.epb + mi.spb);
    const int alpha_prec = mi.ab + (mi.ab > 0 ? (mi.epb + mi.spb) : 0);
    for (int e = 0; e < num_ep; ++e) {
        for (int ch = 0; ch < 3; ++ch) {
            std::uint32_t v = ep[e][ch];
            if (mi.epb > 0 || mi.spb > 0) {
                v = (v << 1U) | pbits[e];
            }
            v = (v << (8 - color_prec)) | (v >> (2 * color_prec - 8));
            ep[e][ch] = static_cast<std::uint8_t>(v);
        }
        if (mi.ab > 0) {
            std::uint32_t v = ep[e][3];
            if (mi.epb > 0 || mi.spb > 0) {
                v = (v << 1U) | pbits[e];
            }
            v = (v << (8 - alpha_prec)) | (v >> (2 * alpha_prec - 8));
            ep[e][3] = static_cast<std::uint8_t>(v);
        }
    }

    const auto get_weights = [](int bits) noexcept -> const std::uint8_t* {
        if (bits == 2) return kWeights2;
        if (bits == 3) return kWeights3;
        return kWeights4;
    };

    std::uint8_t idx1[16]{};
    std::uint8_t idx2[16]{};
    for (int i = 0; i < 16; ++i) {
        bool is_anchor = (i == 0);
        if (mi.ns == 2 && i == kAnchor2[partition]) is_anchor = true;
        else if (mi.ns == 3 && (i == kAnchor3A[partition] || i == kAnchor3B[partition])) is_anchor = true;
        idx1[i] = static_cast<std::uint8_t>(read_bits(is_anchor ? (mi.ib - 1) : mi.ib));
    }
    if (mi.ib2 > 0) {
        for (int i = 0; i < 16; ++i) {
            const bool is_anchor = (i == 0);
            idx2[i] = static_cast<std::uint8_t>(read_bits(is_anchor ? (mi.ib2 - 1) : mi.ib2));
        }
    }

    const std::uint8_t* w1 = get_weights(mi.ib);
    const std::uint8_t* w2 = mi.ib2 > 0 ? get_weights(mi.ib2) : w1;

    for (int i = 0; i < 16; ++i) {
        int subset = 0;
        if (mi.ns == 2) subset = kPart2[partition][i];
        else if (mi.ns == 3) subset = kPart3[partition][i];

        const std::uint8_t* e0 = ep[subset * 2 + 0];
        const std::uint8_t* e1 = ep[subset * 2 + 1];

        const std::uint8_t wc = (mi.ib2 > 0 && idx_mode == 1) ? w2[idx2[i]] : w1[idx1[i]];
        const std::uint8_t wa = (mi.ib2 > 0) ? (idx_mode == 1 ? w1[idx1[i]] : w2[idx2[i]]) : wc;

        std::uint8_t r = static_cast<std::uint8_t>(((64 - wc) * e0[0] + wc * e1[0] + 32) >> 6);
        std::uint8_t g = static_cast<std::uint8_t>(((64 - wc) * e0[1] + wc * e1[1] + 32) >> 6);
        std::uint8_t b = static_cast<std::uint8_t>(((64 - wc) * e0[2] + wc * e1[2] + 32) >> 6);
        std::uint8_t a = (mi.ab > 0)
            ? static_cast<std::uint8_t>(((64 - wa) * e0[3] + wa * e1[3] + 32) >> 6)
            : 255;

        if (rotation == 1) std::swap(a, r);
        else if (rotation == 2) std::swap(a, g);
        else if (rotation == 3) std::swap(a, b);

        out_rgba[i][0] = r;
        out_rgba[i][1] = g;
        out_rgba[i][2] = b;
        out_rgba[i][3] = a;
    }
}

bool DecodeDdsToBgra(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out_bgra, std::uint32_t& out_w, std::uint32_t& out_h)
{
    if (data == nullptr || size < 128U || std::memcmp(data, "DDS ", 4) != 0) {
        return false;
    }
    const auto read_u32 = [&](std::size_t off) -> std::uint32_t {
        std::uint32_t v = 0;
        if (off + 4U <= size) std::memcpy(&v, data + off, 4U);
        return v;
    };
    const std::uint32_t height = read_u32(12U);
    const std::uint32_t width = read_u32(16U);
    if (width == 0 || height == 0 || width > 4096U || height > 4096U) {
        return false;
    }
    const std::uint32_t pf_flags = read_u32(80U);
    const std::uint32_t four_cc = read_u32(84U);
    std::uint32_t rgb_bits = read_u32(88U);
    std::uint32_t r_mask = read_u32(92U);
    std::size_t offset = 128U;

    constexpr auto make_fourcc = [](char a, char b, char c, char d) constexpr -> std::uint32_t {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8U) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16U) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24U);
    };

    // bc_mode: 1=BC1, 2=BC2, 3=BC3, 4=BC4, 5=BC5, 7=BC7
    int bc_mode = 0;
    if ((pf_flags & 0x4U) != 0) {
        if (four_cc == make_fourcc('D', 'X', 'T', '1')) bc_mode = 1;
        else if (four_cc == make_fourcc('D', 'X', 'T', '3')) bc_mode = 2;
        else if (four_cc == make_fourcc('D', 'X', 'T', '5')) bc_mode = 3;
        else if (four_cc == make_fourcc('A', 'T', 'I', '1') || four_cc == make_fourcc('B', 'C', '4', 'U') || four_cc == make_fourcc('B', 'C', '4', 'S')) bc_mode = 4;
        else if (four_cc == make_fourcc('A', 'T', 'I', '2') || four_cc == make_fourcc('B', 'C', '5', 'U') || four_cc == make_fourcc('B', 'C', '5', 'S')) bc_mode = 5;
        else if (four_cc == make_fourcc('D', 'X', '1', '0')) {
            if (size < 148U) return false;
            const std::uint32_t dxgi_fmt = read_u32(128U);
            offset = 148U;
            if (dxgi_fmt == 70U || dxgi_fmt == 71U || dxgi_fmt == 72U) bc_mode = 1;
            else if (dxgi_fmt == 73U || dxgi_fmt == 74U || dxgi_fmt == 75U) bc_mode = 2;
            else if (dxgi_fmt == 76U || dxgi_fmt == 77U || dxgi_fmt == 78U) bc_mode = 3;
            else if (dxgi_fmt == 79U || dxgi_fmt == 80U || dxgi_fmt == 81U) bc_mode = 4;
            else if (dxgi_fmt == 82U || dxgi_fmt == 83U || dxgi_fmt == 84U) bc_mode = 5;
            else if (dxgi_fmt == 97U || dxgi_fmt == 98U || dxgi_fmt == 99U) bc_mode = 7;
            else if (dxgi_fmt == 27U || dxgi_fmt == 28U || dxgi_fmt == 29U) {
                rgb_bits = 32U;
                r_mask = 0x000000FFU; // RGBA8
            } else if (dxgi_fmt == 87U || dxgi_fmt == 88U || dxgi_fmt == 90U || dxgi_fmt == 91U || dxgi_fmt == 92U || dxgi_fmt == 93U) {
                rgb_bits = 32U;
                r_mask = 0x00FF0000U; // BGRA8 / BGRX8
            } else if (dxgi_fmt == 61U) {
                rgb_bits = 8U;
            }
        }
    }

    out_w = width;
    out_h = height;
    out_bgra.assign(static_cast<std::size_t>(width) * height * 4U, 255U);

    if (bc_mode == 7) {
        const std::uint32_t bw = (width + 3U) / 4U;
        const std::uint32_t bh = (height + 3U) / 4U;
        if (offset + static_cast<std::size_t>(bw) * bh * 16U > size) {
            return false;
        }
        std::uint8_t rgba[16][4]{};
        for (std::uint32_t by = 0; by < bh; ++by) {
            for (std::uint32_t bx = 0; bx < bw; ++bx) {
                const std::uint8_t* blk = data + offset + (static_cast<std::size_t>(by) * bw + bx) * 16U;
                DecodeBc7Block(blk, rgba);
                for (std::uint32_t py = 0; py < 4U; ++py) {
                    const std::uint32_t dy = by * 4U + py;
                    if (dy >= height) continue;
                    for (std::uint32_t px = 0; px < 4U; ++px) {
                        const std::uint32_t dx = bx * 4U + px;
                        if (dx >= width) continue;
                        const int idx = static_cast<int>(py * 4U + px);
                        const std::size_t dst = (static_cast<std::size_t>(dy) * width + dx) * 4U;
                        out_bgra[dst + 0] = rgba[idx][2];
                        out_bgra[dst + 1] = rgba[idx][1];
                        out_bgra[dst + 2] = rgba[idx][0];
                        out_bgra[dst + 3] = rgba[idx][3];
                    }
                }
            }
        }
        return true;
    }

    if (bc_mode == 4 || bc_mode == 5) {
        const std::uint32_t bw = (width + 3U) / 4U;
        const std::uint32_t bh = (height + 3U) / 4U;
        const std::size_t block_bytes = bc_mode == 4 ? 8U : 16U;
        if (offset + static_cast<std::size_t>(bw) * bh * block_bytes > size) {
            return false;
        }
        std::uint8_t ch0[16]{};
        std::uint8_t ch1[16]{};
        for (std::uint32_t by = 0; by < bh; ++by) {
            for (std::uint32_t bx = 0; bx < bw; ++bx) {
                const std::uint8_t* blk = data + offset + (static_cast<std::size_t>(by) * bw + bx) * block_bytes;
                DecodeBc4Channel(blk, ch0);
                if (bc_mode == 5) {
                    DecodeBc4Channel(blk + 8, ch1);
                }
                for (std::uint32_t py = 0; py < 4U; ++py) {
                    const std::uint32_t dy = by * 4U + py;
                    if (dy >= height) continue;
                    for (std::uint32_t px = 0; px < 4U; ++px) {
                        const std::uint32_t dx = bx * 4U + px;
                        if (dx >= width) continue;
                        const int idx = static_cast<int>(py * 4U + px);
                        const std::size_t dst = (static_cast<std::size_t>(dy) * width + dx) * 4U;
                        const std::uint8_t r_val = ch0[idx];
                        const std::uint8_t g_val = bc_mode == 5 ? ch1[idx] : r_val;
                        const std::uint8_t b_val = bc_mode == 5 ? r_val : r_val;
                        out_bgra[dst + 0] = b_val;
                        out_bgra[dst + 1] = g_val;
                        out_bgra[dst + 2] = r_val;
                        out_bgra[dst + 3] = 255U;
                    }
                }
            }
        }
        return true;
    }

    if (bc_mode >= 1 && bc_mode <= 3) {
        const std::uint32_t bw = (width + 3U) / 4U;
        const std::uint32_t bh = (height + 3U) / 4U;
        const std::size_t block_bytes = bc_mode == 1 ? 8U : 16U;
        if (offset + static_cast<std::size_t>(bw) * bh * block_bytes > size) {
            return false;
        }
        for (std::uint32_t by = 0; by < bh; ++by) {
            for (std::uint32_t bx = 0; bx < bw; ++bx) {
                const std::uint8_t* blk = data + offset + (static_cast<std::size_t>(by) * bw + bx) * block_bytes;
                std::uint8_t alphas[16];
                std::memset(alphas, 255, sizeof(alphas));
                const std::uint8_t* color_blk = blk;
                if (bc_mode == 2) {
                    for (int i = 0; i < 8; ++i) {
                        const std::uint8_t byte_val = blk[i];
                        alphas[i * 2 + 0] = static_cast<std::uint8_t>((byte_val & 0x0FU) * 17U);
                        alphas[i * 2 + 1] = static_cast<std::uint8_t>((byte_val >> 4U) * 17U);
                    }
                    color_blk = blk + 8;
                } else if (bc_mode == 3) {
                    DecodeBc4Channel(blk, alphas);
                    color_blk = blk + 8;
                }
                std::uint16_t c0 = static_cast<std::uint16_t>(color_blk[0] | (color_blk[1] << 8U));
                std::uint16_t c1 = static_cast<std::uint16_t>(color_blk[2] | (color_blk[3] << 8U));
                std::uint8_t r[4]{}, g[4]{}, b[4]{}, a[4]{ 255, 255, 255, 255 };
                DecodeRgb565(c0, r[0], g[0], b[0]);
                DecodeRgb565(c1, r[1], g[1], b[1]);
                if (c0 > c1 || bc_mode != 1) {
                    r[2] = static_cast<std::uint8_t>((2U * r[0] + r[1] + 1U) / 3U);
                    g[2] = static_cast<std::uint8_t>((2U * g[0] + g[1] + 1U) / 3U);
                    b[2] = static_cast<std::uint8_t>((2U * b[0] + b[1] + 1U) / 3U);
                    r[3] = static_cast<std::uint8_t>((r[0] + 2U * r[1] + 1U) / 3U);
                    g[3] = static_cast<std::uint8_t>((g[0] + 2U * g[1] + 1U) / 3U);
                    b[3] = static_cast<std::uint8_t>((b[0] + 2U * b[1] + 1U) / 3U);
                } else {
                    r[2] = static_cast<std::uint8_t>((r[0] + r[1]) / 2U);
                    g[2] = static_cast<std::uint8_t>((g[0] + g[1]) / 2U);
                    b[2] = static_cast<std::uint8_t>((b[0] + b[1]) / 2U);
                    r[3] = g[3] = b[3] = 0;
                    a[3] = 0;
                }
                std::uint32_t c_bits = 0;
                std::memcpy(&c_bits, color_blk + 4, 4U);
                for (std::uint32_t py = 0; py < 4U; ++py) {
                    const std::uint32_t dy = by * 4U + py;
                    if (dy >= height) continue;
                    for (std::uint32_t px = 0; px < 4U; ++px) {
                        const std::uint32_t dx = bx * 4U + px;
                        if (dx >= width) continue;
                        const int idx = static_cast<int>(py * 4U + px);
                        const int ci = static_cast<int>((c_bits >> (2 * idx)) & 0x03U);
                        const std::size_t dst = (static_cast<std::size_t>(dy) * width + dx) * 4U;
                        out_bgra[dst + 0] = b[ci];
                        out_bgra[dst + 1] = g[ci];
                        out_bgra[dst + 2] = r[ci];
                        out_bgra[dst + 3] = bc_mode == 1 ? a[ci] : alphas[idx];
                    }
                }
            }
        }
        return true;
    }

    if (rgb_bits == 32U && offset + static_cast<std::size_t>(width) * height * 4U <= size) {
        const bool is_rgba = (r_mask == 0x000000FFU);
        const std::uint8_t* src = data + offset;
        const std::size_t count = static_cast<std::size_t>(width) * height;
        for (std::size_t i = 0; i < count; ++i) {
            out_bgra[i * 4 + 0] = is_rgba ? src[i * 4 + 2] : src[i * 4 + 0];
            out_bgra[i * 4 + 1] = src[i * 4 + 1];
            out_bgra[i * 4 + 2] = is_rgba ? src[i * 4 + 0] : src[i * 4 + 2];
            out_bgra[i * 4 + 3] = src[i * 4 + 3];
        }
        return true;
    }
    if (rgb_bits == 24U && offset + static_cast<std::size_t>(width) * height * 3U <= size) {
        const bool is_rgb = (r_mask == 0x0000FFU);
        const std::uint8_t* src = data + offset;
        const std::size_t count = static_cast<std::size_t>(width) * height;
        for (std::size_t i = 0; i < count; ++i) {
            out_bgra[i * 4 + 0] = is_rgb ? src[i * 3 + 2] : src[i * 3 + 0];
            out_bgra[i * 4 + 1] = src[i * 3 + 1];
            out_bgra[i * 4 + 2] = is_rgb ? src[i * 3 + 0] : src[i * 3 + 2];
            out_bgra[i * 4 + 3] = 255U;
        }
        return true;
    }
    if (rgb_bits == 8U && offset + static_cast<std::size_t>(width) * height <= size) {
        const std::uint8_t* src = data + offset;
        const std::size_t count = static_cast<std::size_t>(width) * height;
        for (std::size_t i = 0; i < count; ++i) {
            out_bgra[i * 4 + 0] = src[i];
            out_bgra[i * 4 + 1] = src[i];
            out_bgra[i * 4 + 2] = src[i];
            out_bgra[i * 4 + 3] = 255U;
        }
        return true;
    }
    return false;
}

bool DecodeImageBytesViaWic(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out_bgra, std::uint32_t& out_w, std::uint32_t& out_h)
{
    if (data == nullptr || size < 16U || size > 32U * 1024U * 1024U) {
        return false;
    }
    struct ComInit {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ~ComInit() { if (SUCCEEDED(hr)) CoUninitialize(); }
    } com_init;

    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) || !factory) {
        return false;
    }
    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream)) || !stream ||
        FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(data), static_cast<DWORD>(size)))) {
        return false;
    }
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) || !decoder) {
        return false;
    }
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame)) || !frame) {
        return false;
    }
    UINT w = 0, h = 0;
    if (FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0 || w > 4096U || h > 4096U) {
        return false;
    }
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)) || !converter ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        return false;
    }
    out_w = w;
    out_h = h;
    out_bgra.resize(static_cast<std::size_t>(w) * h * 4U);
    if (FAILED(converter->CopyPixels(nullptr, w * 4U, static_cast<UINT>(out_bgra.size()), out_bgra.data()))) {
        out_bgra.clear();
        return false;
    }
    return true;
}

std::string LoadBookTextureDataUrl(std::string_view raw_src)
{
    std::string path(raw_src);
    while (!path.empty() && (path.front() == ' ' || path.front() == '\'' || path.front() == '"')) path.erase(path.begin());
    while (!path.empty() && (path.back() == ' ' || path.back() == '\'' || path.back() == '"')) path.pop_back();
    const auto lower_prefix = LowerAscii(path);
    if (lower_prefix.starts_with("img:///")) path.erase(0, 7);
    else if (lower_prefix.starts_with("img://")) path.erase(0, 6);
    for (char& ch : path) {
        if (ch == '/') ch = '\\';
    }
    while (!path.empty() && path.front() == '\\') path.erase(path.begin());
    if (LowerAscii(path).starts_with("data\\")) path.erase(0, 5);
    if (path.empty()) return {};

    std::vector<std::string> candidates;
    candidates.push_back(path);
    if (!LowerAscii(path).starts_with("textures\\")) {
        candidates.push_back("Textures\\" + path);
    }
    const std::size_t base_count = candidates.size();
    for (std::size_t i = 0; i < base_count; ++i) {
        const auto c = candidates[i];
        const auto lc = LowerAscii(c);
        if (lc.ends_with(".png")) {
            candidates.push_back(c.substr(0, c.size() - 4) + ".dds");
        } else if (lc.ends_with(".dds")) {
            candidates.push_back(c.substr(0, c.size() - 4) + ".png");
        } else {
            candidates.push_back(c + ".dds");
            candidates.push_back(c + ".png");
        }
    }

    for (const auto& candidate : candidates) {
        RE::BSResourceNiBinaryStream stream(candidate);
        if (!stream.good() || !stream.stream) continue;
        const auto total_size = stream.stream->totalSize;
        if (total_size < 16U || total_size > 16U * 1024U * 1024U) continue;
        std::vector<std::uint8_t> bytes(total_size);
        if (!stream.read(reinterpret_cast<char*>(bytes.data()), total_size)) continue;

        if (bytes.size() >= 8U && bytes[0] == 0x89U && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') {
            if (bytes.size() <= 550000U) {
                return EncodeBytesBase64(bytes.data(), bytes.size());
            }
        }
        std::vector<std::uint8_t> bgra;
        std::uint32_t w = 0, h = 0;
        if (DecodeDdsToBgra(bytes.data(), bytes.size(), bgra, w, h) ||
            DecodeImageBytesViaWic(bytes.data(), bytes.size(), bgra, w, h)) {
            if (auto encoded = EncodeBgraPngDataUrl(bgra, w, h); !encoded.empty()) {
                return encoded;
            }
        }
    }
    return {};
}

struct ParsedBookContent {
    std::string text;
    std::vector<std::string> pages;
    std::vector<std::string> images;
};

std::string TrimBookBlock(std::string s)
{
    while (!s.empty() && (s.front() == '\n' || s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    return s;
}

ParsedBookContent ParseBookContent(std::string_view raw, std::string_view book_title, std::string_view player_name)
{
    std::string utf8;
    utf8.reserve(raw.size() + 32);
    for (std::size_t i = 0; i < raw.size();) {
        const unsigned char b = static_cast<unsigned char>(raw[i]);
        if (b < 0x80U) {
            if (b != '\r') utf8.push_back(static_cast<char>(b));
            ++i;
        } else if ((b & 0xE0U) == 0xC0U && i + 1 < raw.size() &&
                   (static_cast<unsigned char>(raw[i + 1]) & 0xC0U) == 0x80U) {
            utf8.append(raw.data() + i, 2);
            i += 2;
        } else if ((b & 0xF0U) == 0xE0U && i + 2 < raw.size() &&
                   (static_cast<unsigned char>(raw[i + 1]) & 0xC0U) == 0x80U &&
                   (static_cast<unsigned char>(raw[i + 2]) & 0xC0U) == 0x80U) {
            utf8.append(raw.data() + i, 3);
            i += 3;
        } else if ((b & 0xF8U) == 0xF0U && i + 3 < raw.size() &&
                   (static_cast<unsigned char>(raw[i + 1]) & 0xC0U) == 0x80U &&
                   (static_cast<unsigned char>(raw[i + 2]) & 0xC0U) == 0x80U &&
                   (static_cast<unsigned char>(raw[i + 3]) & 0xC0U) == 0x80U) {
            utf8.append(raw.data() + i, 4);
            i += 4;
        } else {
            if (b == 0x91U || b == 0x92U) utf8.push_back('\'');
            else if (b == 0x93U || b == 0x94U) utf8.push_back('"');
            else if (b == 0x96U || b == 0x97U) utf8.append("—");
            else if (b >= 0xA0U) {
                utf8.push_back(static_cast<char>(0xC0U | (b >> 6)));
                utf8.push_back(static_cast<char>(0x80U | (b & 0x3FU)));
            }
            ++i;
        }
    }

    ParsedBookContent out;
    std::string cleaned;
    std::string current_page;
    cleaned.reserve(utf8.size());
    current_page.reserve(utf8.size());

    for (std::size_t i = 0; i < utf8.size();) {
        if (utf8[i] == '[') {
            const auto close = utf8.find(']', i + 1);
            if (close != std::string::npos && close - i <= 24) {
                const auto tag = LowerAscii(std::string_view(utf8.data() + i + 1, close - i - 1));
                if (tag == "pagebreak") {
                    cleaned.append("\n\n───\n\n");
                    auto trimmed_page = TrimBookBlock(std::move(current_page));
                    current_page.clear();
                    if (!trimmed_page.empty()) {
                        out.pages.push_back(std::move(trimmed_page));
                    }
                    i = close + 1;
                    continue;
                }
            }
        }
        if (utf8[i] == '<') {
            const auto close = utf8.find('>', i + 1);
            if (close != std::string::npos && close - i <= 320) {
                const std::string_view raw_tag(utf8.data() + i + 1, close - i - 1);
                const auto tag = LowerAscii(raw_tag);
                if (tag == "br" || tag == "br/" || tag == "br /") {
                    cleaned.push_back('\n');
                    current_page.push_back('\n');
                    i = close + 1;
                    continue;
                }
                if (tag == "/p" || tag == "/div") {
                    cleaned.append("\n\n");
                    current_page.append("\n\n");
                    i = close + 1;
                    continue;
                }
                if (tag.starts_with("alias=")) {
                    const auto alias_target = tag.substr(6);
                    if (alias_target == "player") {
                        const std::string_view pname = !player_name.empty() ? player_name : "Dragonborn";
                        cleaned.append(pname);
                        current_page.append(pname);
                    }
                    i = close + 1;
                    continue;
                }
                if (tag.starts_with("img")) {
                    const auto src_pos = tag.find("src=");
                    if (src_pos != std::string::npos) {
                        std::size_t val_start = src_pos + 4;
                        while (val_start < raw_tag.size() && raw_tag[val_start] == ' ') ++val_start;
                        if (val_start < raw_tag.size()) {
                            const char quote = (raw_tag[val_start] == '\'' || raw_tag[val_start] == '"') ? raw_tag[val_start++] : ' ';
                            const auto val_end = raw_tag.find(quote, val_start);
                            const auto src_str = raw_tag.substr(val_start, val_end == std::string_view::npos ? raw_tag.size() - val_start : val_end - val_start);
                            const auto lower_src = LowerAscii(src_str);
                            const auto letter_pos = lower_src.find("_letter");
                            if (letter_pos != std::string::npos && letter_pos > 0) {
                                const char drop_ch = static_cast<char>(std::toupper(static_cast<unsigned char>(src_str[letter_pos - 1])));
                                if (drop_ch >= 'A' && drop_ch <= 'Z') {
                                    cleaned.push_back(drop_ch);
                                    current_page.push_back(drop_ch);
                                }
                            } else if (out.images.size() < 4) {
                                if (auto data_url = LoadBookTextureDataUrl(src_str); !data_url.empty()) {
                                    out.images.push_back(std::move(data_url));
                                }
                            }
                        }
                    }
                    i = close + 1;
                    continue;
                }
                if (tag.starts_with("font") || tag == "/font" || tag.starts_with("p ") || tag == "p" ||
                    tag.starts_with("div") || tag == "b" || tag == "/b" ||
                    tag == "i" || tag == "/i" || tag == "u" || tag == "/u" ||
                    tag.starts_with("global=") || tag.starts_with("quest=")) {
                    i = close + 1;
                    continue;
                }
            }
        }
        cleaned.push_back(utf8[i]);
        current_page.push_back(utf8[i]);
        ++i;
    }

    auto last_page = TrimBookBlock(std::move(current_page));
    if (!last_page.empty()) {
        out.pages.push_back(std::move(last_page));
    }
    out.text = TrimBookBlock(std::move(cleaned));

    if (out.images.empty()) {
        const auto lower_title = LowerAscii(book_title);
        if (lower_title.find("treasure map") != std::string::npos) {
            std::string map_tex;
            if (lower_title.find("fort neugrad") != std::string::npos) map_tex = "Textures\\Interface\\Books\\TreasureMapFortNeugrad.dds";
            else if (lower_title.find("velehk") != std::string::npos) map_tex = "Textures\\Interface\\Books\\TreasureMapVelehkSain.dds";
            else if (lower_title.ends_with(" x") || lower_title.ends_with(" 10")) map_tex = "Textures\\Interface\\Books\\TreasureMap10.dds";
            else if (lower_title.ends_with(" ix") || lower_title.ends_with(" 9")) map_tex = "Textures\\Interface\\Books\\TreasureMap09.dds";
            else if (lower_title.ends_with(" viii") || lower_title.ends_with(" 8")) map_tex = "Textures\\Interface\\Books\\TreasureMap08.dds";
            else if (lower_title.ends_with(" vii") || lower_title.ends_with(" 7")) map_tex = "Textures\\Interface\\Books\\TreasureMap07.dds";
            else if (lower_title.ends_with(" vi") || lower_title.ends_with(" 6")) map_tex = "Textures\\Interface\\Books\\TreasureMap06.dds";
            else if (lower_title.ends_with(" v") || lower_title.ends_with(" 5")) map_tex = "Textures\\Interface\\Books\\TreasureMap05.dds";
            else if (lower_title.ends_with(" iv") || lower_title.ends_with(" 4")) map_tex = "Textures\\Interface\\Books\\TreasureMap04.dds";
            else if (lower_title.ends_with(" iii") || lower_title.ends_with(" 3")) map_tex = "Textures\\Interface\\Books\\TreasureMap03.dds";
            else if (lower_title.ends_with(" ii") || lower_title.ends_with(" 2")) map_tex = "Textures\\Interface\\Books\\TreasureMap02.dds";
            else if (lower_title.ends_with(" i") || lower_title.ends_with(" 1")) map_tex = "Textures\\Interface\\Books\\TreasureMap01.dds";
            if (!map_tex.empty()) {
                if (auto data_url = LoadBookTextureDataUrl(map_tex); !data_url.empty()) {
                    out.images.push_back(std::move(data_url));
                }
            }
        }
    }
    return out;
}

bool IsNonDiffuseOrGenericBookTexture(std::string_view raw_path, bool& out_is_generic_paper) noexcept
{
    out_is_generic_paper = false;
    const auto lower = LowerAscii(raw_path);
    if (lower.size() < 5 || !lower.ends_with(".dds")) {
        return true;
    }
    if (lower.ends_with("_n.dds") || lower.ends_with("_msn.dds") || lower.ends_with("_s.dds") ||
        lower.ends_with("_sk.dds") || lower.ends_with("_g.dds") || lower.ends_with("_p.dds") ||
        lower.ends_with("_m.dds") || lower.ends_with("_em.dds") || lower.ends_with("_e.dds") ||
        lower.find("cubemap") != std::string::npos || lower.find("effects\\") != std::string::npos ||
        lower.find("effects/") != std::string::npos) {
        return true;
    }
    const auto slash = lower.find_last_of("\\/");
    const std::string_view fname = slash == std::string::npos ? std::string_view(lower) : std::string_view(lower).substr(slash + 1);
    if (fname == "note01.dds" || fname == "note02.dds" || fname == "letter01.dds" ||
        fname == "scroll01.dds" || fname == "scroll02.dds" || fname == "paper01.dds" ||
        fname == "paper02.dds" || fname == "book01.dds" || fname == "book02.dds" ||
        fname == "book03.dds" || fname == "book04.dds" || fname == "highpolynote01.dds" ||
        fname.find("bookgeneric") != std::string_view::npos ||
        fname.find("burnedbook") != std::string_view::npos ||
        fname.find("ruinedbook") != std::string_view::npos) {
        out_is_generic_paper = true;
    }
    return false;
}

void ScanBinaryBufferForDiffuseTextures(
    const std::uint8_t* bytes,
    std::size_t size,
    std::vector<std::string>& primary_paths,
    std::vector<std::string>& fallback_paths,
    std::vector<std::string>* out_bgsm_paths = nullptr)
{
    if (bytes == nullptr || size < 8U) return;
    const auto is_path_char = [](std::uint8_t c) noexcept -> bool {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '\\' || c == '/' ||
               c == '_' || c == '-' || c == '.' || c == ' ' || c == '\'';
    };
    for (std::size_t i = 0; i + 4 <= size; ++i) {
        const bool is_dds =
            bytes[i] == '.' &&
            (bytes[i + 1] == 'd' || bytes[i + 1] == 'D') &&
            (bytes[i + 2] == 'd' || bytes[i + 2] == 'D') &&
            (bytes[i + 3] == 's' || bytes[i + 3] == 'S');
        const bool is_bgsm =
            out_bgsm_paths != nullptr && i + 5 <= size &&
            bytes[i] == '.' &&
            (bytes[i + 1] == 'b' || bytes[i + 1] == 'B') &&
            (bytes[i + 2] == 'g' || bytes[i + 2] == 'G') &&
            (bytes[i + 3] == 's' || bytes[i + 3] == 'S') &&
            (bytes[i + 4] == 'm' || bytes[i + 4] == 'M');
        if (!is_dds && !is_bgsm) {
            continue;
        }
        const std::size_t ext_len = is_bgsm ? 5U : 4U;
        std::size_t start = i;
        while (start > 0 && (i - start) < 250U && is_path_char(bytes[start - 1])) {
            --start;
        }
        if (i - start >= 2U) {
            std::string extracted(reinterpret_cast<const char*>(bytes + start), (i + ext_len) - start);
            const auto lower_ext = LowerAscii(extracted);
            if (is_bgsm) {
                const auto mat_pos = lower_ext.find("materials");
                if (mat_pos != std::string::npos && mat_pos + 10 < extracted.size() &&
                    (extracted[mat_pos + 9] == '\\' || extracted[mat_pos + 9] == '/')) {
                    extracted.erase(0, mat_pos);
                }
                if (std::find(out_bgsm_paths->begin(), out_bgsm_paths->end(), extracted) == out_bgsm_paths->end()) {
                    out_bgsm_paths->push_back(std::move(extracted));
                }
            } else {
                const auto tex_pos = lower_ext.find("textures");
                if (tex_pos != std::string::npos && tex_pos + 9 < extracted.size() &&
                    (extracted[tex_pos + 8] == '\\' || extracted[tex_pos + 8] == '/')) {
                    extracted.erase(0, tex_pos);
                }
                bool is_generic = false;
                if (!IsNonDiffuseOrGenericBookTexture(extracted, is_generic)) {
                    auto& bucket = is_generic ? fallback_paths : primary_paths;
                    if (std::find(bucket.begin(), bucket.end(), extracted) == bucket.end()) {
                        bucket.push_back(std::move(extracted));
                    }
                }
            }
        }
        i += ext_len - 1U;
    }
}

void ScanNifForDiffuseTextures(std::string_view raw_model_path, std::vector<std::string>& primary_paths, std::vector<std::string>& fallback_paths)
{
    std::string path(raw_model_path);
    for (char& ch : path) {
        if (ch == '/') ch = '\\';
    }
    while (!path.empty() && (path.front() == '\\' || path.front() == ' ')) path.erase(path.begin());
    if (LowerAscii(path).starts_with("data\\")) path.erase(0, 5);
    if (path.empty()) return;

    std::vector<std::string> nif_candidates;
    if (!LowerAscii(path).starts_with("meshes\\")) {
        nif_candidates.push_back("Meshes\\" + path);
    }
    nif_candidates.push_back(path);

    std::vector<std::string> bgsm_paths;
    for (const auto& candidate : nif_candidates) {
        RE::BSResourceNiBinaryStream stream(candidate);
        if (!stream.good() || !stream.stream) continue;
        const auto total_size = stream.stream->totalSize;
        if (total_size < 32U || total_size > 8U * 1024U * 1024U) continue;
        std::vector<std::uint8_t> bytes(total_size);
        if (!stream.read(reinterpret_cast<char*>(bytes.data()), total_size)) continue;
        ScanBinaryBufferForDiffuseTextures(bytes.data(), bytes.size(), primary_paths, fallback_paths, &bgsm_paths);
        break;
    }

    for (const auto& bgsm_raw : bgsm_paths) {
        std::string bpath = bgsm_raw;
        for (char& ch : bpath) {
            if (ch == '/') ch = '\\';
        }
        std::vector<std::string> bgsm_candidates;
        if (!LowerAscii(bpath).starts_with("materials\\")) {
            bgsm_candidates.push_back("Materials\\" + bpath);
        }
        bgsm_candidates.push_back(bpath);
        for (const auto& bcand : bgsm_candidates) {
            RE::BSResourceNiBinaryStream bstream(bcand);
            if (!bstream.good() || !bstream.stream) continue;
            const auto bsize = bstream.stream->totalSize;
            if (bsize < 24U || bsize > 512U * 1024U) continue;
            std::vector<std::uint8_t> bbytes(bsize);
            if (!bstream.read(reinterpret_cast<char*>(bbytes.data()), bsize)) continue;
            ScanBinaryBufferForDiffuseTextures(bbytes.data(), bbytes.size(), primary_paths, fallback_paths, nullptr);
            break;
        }
    }
}

void CollectNodeDiffuseTextures(RE::NiAVObject* obj, const std::function<void(const char*)>& add_candidate_path)
{
    if (obj == nullptr) return;
    if (auto* geom = obj->AsGeometry(); geom != nullptr) {
        if (auto* shader_prop = geom->lightingShaderProp_cast(); shader_prop != nullptr && shader_prop->material != nullptr) {
            if (shader_prop->material->GetType() == RE::BSShaderMaterial::Type::kLighting) {
                auto* mat = static_cast<RE::BSLightingShaderMaterialBase*>(shader_prop->material);
                if (mat->textureSet != nullptr) {
                    add_candidate_path(mat->textureSet->GetTexturePath(RE::BSTextureSet::Textures::kDiffuse));
                }
                if (mat->diffuseTexture != nullptr && mat->diffuseTexture->name.c_str() != nullptr) {
                    add_candidate_path(mat->diffuseTexture->name.c_str());
                }
            }
        }
    }
    if (auto* node = obj->AsNode(); node != nullptr) {
        for (auto& child : node->GetChildren()) {
            if (child != nullptr) {
                CollectNodeDiffuseTextures(child.get(), add_candidate_path);
            }
        }
    }
}

void ExtractBookModelTextures(RE::TESObjectBOOK* book, std::string_view book_title, std::vector<std::string>& out_images, bool is_empty_text)
{
    if (book == nullptr || !out_images.empty()) {
        return;
    }
    const auto lower_title = LowerAscii(book_title);
    const bool is_visual_title =
        lower_title.find("map") != std::string::npos ||
        lower_title.find("drawing") != std::string::npos ||
        lower_title.find("sketch") != std::string::npos ||
        lower_title.find("painting") != std::string::npos ||
        lower_title.find("diagram") != std::string::npos ||
        lower_title.find("chart") != std::string::npos ||
        lower_title.find("blueprint") != std::string::npos;
    if (!is_empty_text && !is_visual_title) {
        return;
    }

    std::vector<std::string> primary_paths;
    std::vector<std::string> fallback_paths;
    const auto add_candidate_path = [&](const char* raw_path) {
        if (raw_path == nullptr || raw_path[0] == '\0') return;
        bool is_generic = false;
        if (!IsNonDiffuseOrGenericBookTexture(raw_path, is_generic)) {
            std::string s(raw_path);
            auto& bucket = is_generic ? fallback_paths : primary_paths;
            if (std::find(bucket.begin(), bucket.end(), s) == bucket.end()) {
                bucket.push_back(std::move(s));
            }
        }
    };

    const std::array<const RE::TESModelTextureSwap*, 2> swap_sources{
        book->inventoryModel != nullptr ? static_cast<const RE::TESModelTextureSwap*>(book->inventoryModel) : nullptr,
        static_cast<const RE::TESModelTextureSwap*>(book)
    };
    for (const auto* swap_src : swap_sources) {
        if (swap_src != nullptr && swap_src->alternateTextures != nullptr &&
            swap_src->numAlternateTextures > 0 && swap_src->numAlternateTextures <= 64) {
            for (std::uint32_t i = 0; i < swap_src->numAlternateTextures; ++i) {
                const auto& alt = swap_src->alternateTextures[i];
                if (alt.textureSet != nullptr) {
                    add_candidate_path(alt.textureSet->GetTexturePath(RE::BSTextureSet::Textures::kDiffuse));
                    add_candidate_path(alt.textureSet->textures[RE::BSTextureSet::Textures::kDiffuse].textureName.c_str());
                }
            }
        }
    }

    const std::array<const char*, 2> model_paths{
        (book->inventoryModel != nullptr && book->inventoryModel->GetModel() != nullptr && book->inventoryModel->GetModel()[0] != '\0')
            ? book->inventoryModel->GetModel()
            : nullptr,
        (book->GetModel() != nullptr && book->GetModel()[0] != '\0')
            ? book->GetModel()
            : nullptr
    };
    for (const char* model_path : model_paths) {
        if (model_path == nullptr) continue;
        ScanNifForDiffuseTextures(model_path, primary_paths, fallback_paths);
        RE::NiPointer<RE::NiNode> loaded_model;
        RE::BSModelDB::DBTraits::ArgsType model_args{};
        if (RE::BSModelDB::Demand(model_path, loaded_model, model_args) == RE::BSResource::ErrorCode::kNone && loaded_model != nullptr) {
            CollectNodeDiffuseTextures(loaded_model.get(), add_candidate_path);
        }
    }
    if (book->textureName.c_str() != nullptr && book->textureName.c_str()[0] != '\0') {
        add_candidate_path(book->textureName.c_str());
    }
    if (book->icon.textureName.c_str() != nullptr && book->icon.textureName.c_str()[0] != '\0') {
        add_candidate_path(book->icon.textureName.c_str());
    }

    // Maps/drawings: only the printed front face is wanted. Drop the reverse side of the
    // sheet (e.g. "*back*.dds", "*_b.dds", "*reverse*", blank parchment) when a front exists.
    if (is_visual_title && primary_paths.size() > 1) {
        const auto is_reverse_face = [](const std::string& p) {
            const auto lower = LowerAscii(p);
            const auto slash = lower.find_last_of("\\/");
            const std::string fname = slash == std::string::npos ? lower : lower.substr(slash + 1);
            return fname.find("back") != std::string::npos || fname.ends_with("_b.dds") ||
                   fname.find("reverse") != std::string::npos || fname.find("rear") != std::string::npos ||
                   fname.find("blank") != std::string::npos || fname.find("parchment") != std::string::npos ||
                   fname.find("paper") != std::string::npos;
        };
        std::vector<std::string> fronts;
        for (const auto& p : primary_paths) {
            if (!is_reverse_face(p)) fronts.push_back(p);
        }
        if (!fronts.empty()) primary_paths = std::move(fronts);
    }

    const std::size_t max_images = is_visual_title ? 1U : 4U;
    for (const auto& tex_path : primary_paths) {
        if (out_images.size() >= max_images) break;
        if (auto data_url = LoadBookTextureDataUrl(tex_path); !data_url.empty()) {
            out_images.push_back(std::move(data_url));
        }
    }
}

Json PerformInventoryItemAction(const Json& options)
{
    const auto rejected = [](std::string message) -> Json {
        return { { "accepted", false }, { "message", std::move(message) } };
    };
    if (!options.is_object()) {
        return rejected("Item action options must be an object.");
    }
    bool from_stolen_stack = false;
    const std::uint32_t form_id = ParseItemFormIdField(options, "formId", &from_stolen_stack);
    const auto action = LowerAscii(options.value("action", std::string{}));
    const std::int32_t requested_count = std::clamp(options.value("count", 1), 1, 100000);
    const bool open_native = options.value("openNative", false);
    if (form_id == 0 || action.empty()) {
        return rejected("A valid item form ID and action are required.");
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* manager = RE::ActorEquipManager::GetSingleton();
    auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(form_id);
    if (player == nullptr || manager == nullptr || form == nullptr || player->IsDead()) {
        return rejected("Player inventory is unavailable.");
    }

    auto inventory = player->GetInventory();
    auto found = inventory.find(form);
    if (found == inventory.end() || found->second.first <= 0) {
        return rejected("The player no longer carries this item.");
    }
    const std::int32_t carried_count = found->second.first;
    auto* entry = found->second.second.get();
    const std::int32_t stolen_count = ComputePlayerStolenCount(form, carried_count, entry);
    const std::int32_t clean_count = std::max(0, carried_count - stolen_count);
    const std::int32_t stack_avail = from_stolen_stack
        ? (stolen_count > 0 ? stolen_count : carried_count)
        : (clean_count > 0 ? clean_count : carried_count);
    const auto* raw_name = entry != nullptr ? entry->GetDisplayName() : nullptr;
    if (raw_name == nullptr || raw_name[0] == '\0') {
        raw_name = form->GetName();
    }
    const std::string stripped_item_name = raw_name != nullptr && raw_name[0] != '\0' ? StripLeadingDollar(raw_name) : std::string{};
    const std::string item_name = !stripped_item_name.empty() ? stripped_item_name : "Item";

    RE::ExtraDataList* extra = nullptr;
    if (entry != nullptr && entry->extraLists != nullptr) {
        for (auto* candidate : *entry->extraLists) {
            if (candidate != nullptr) {
                extra = candidate;
                break;
            }
        }
    }

    const bool game_paused = RE::UI::GetSingleton() != nullptr && RE::UI::GetSingleton()->GameIsPaused();
    const auto finish_ok = [&](std::string message, Json extra_fields = Json::object()) -> Json {
        skyultra::InvalidateSharedGameDataCache();
        Json res{
            { "accepted", true },
            { "action", action },
            { "formId", std::to_string(form_id) },
            { "equipment", ReadActorEquipment(player, true) },
            { "message", std::move(message) }
        };
        for (auto& [k, v] : extra_fields.items()) {
            res[k] = std::move(v);
        }
        return res;
    };

    if (action == "drop") {
        if (entry != nullptr && entry->IsQuestObject()) {
            return rejected("Quest items cannot be dropped.");
        }
        const std::int32_t drop_count = std::clamp(requested_count, 1, stack_avail);
        player->DropObject(form, extra, drop_count, nullptr, nullptr);
        ConsumePlayerStolenCount(form, entry, carried_count, drop_count, std::max(0, carried_count - drop_count), from_stolen_stack || clean_count == 0);
        return finish_ok("Dropped " + (drop_count > 1 ? std::to_string(drop_count) + "x " : std::string{}) + item_name + ".");
    }

    if (action == "drink" || action == "eat") {
        if (auto* alch = form->As<RE::AlchemyItem>(); alch != nullptr) {
            if (alch->IsPoison()) {
                manager->EquipObject(player, alch, extra, 1, nullptr, false, false, true, false);
                ConsumePlayerStolenCount(form, entry, carried_count, 1, std::max(0, carried_count - 1), from_stolen_stack || clean_count == 0);
                return finish_ok("Applied " + item_name + ".");
            }
            if (!player->DrinkPotion(alch, extra)) {
                manager->EquipObject(player, alch, extra, 1, nullptr, false, false, true, false);
            }
            ConsumePlayerStolenCount(form, entry, carried_count, 1, std::max(0, carried_count - 1), from_stolen_stack || clean_count == 0);
            return finish_ok((action == "eat" ? "Ate " : "Drank ") + item_name + ".");
        }
        if (auto* ingr = form->As<RE::IngredientItem>(); ingr != nullptr) {
            std::uint32_t max_reveal = 1U;
            const auto has_perk_id = [player](RE::FormID id) noexcept -> bool {
                if (auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(id); perk != nullptr) {
                    return player->HasPerk(perk);
                }
                return false;
            };
            if (has_perk_id(0x00105F2BU)) max_reveal = 4U;
            else if (has_perk_id(0x00105F2AU)) max_reveal = 3U;
            else if (has_perk_id(0x00058218U)) max_reveal = 2U;
            const std::uint32_t slot_limit = std::min<std::uint32_t>(max_reveal, ingr->effects.size());
            std::vector<std::string> learned_names;
            for (std::uint32_t slot = 0; slot < slot_limit; ++slot) {
                const bool was_known = (ingr->gamedata.knownEffectFlags & (static_cast<std::uint16_t>(1U) << slot)) != 0U;
                const auto* eff = ingr->effects[slot];
                if (!was_known && eff != nullptr && eff->baseEffect != nullptr) {
                    if (ingr->LearnEffect(slot)) {
                        const char* raw_eff_name = eff->baseEffect->GetFullName();
                        if (raw_eff_name == nullptr || raw_eff_name[0] == '\0') {
                            raw_eff_name = eff->baseEffect->GetName();
                        }
                        if (raw_eff_name != nullptr && raw_eff_name[0] != '\0') {
                            learned_names.push_back(StripLeadingDollar(raw_eff_name));
                        }
                    }
                }
            }
            manager->EquipObject(player, ingr, extra, 1, nullptr, false, false, true, false);
            ConsumePlayerStolenCount(form, entry, carried_count, 1, std::max(0, carried_count - 1), from_stolen_stack || clean_count == 0);
            std::string ate_msg = "Ate " + item_name;
            if (!learned_names.empty()) {
                ate_msg += " - Learned: ";
                for (std::size_t i = 0; i < learned_names.size(); ++i) {
                    if (i > 0) ate_msg += ", ";
                    ate_msg += learned_names[i];
                }
            }
            ate_msg += ".";
            return finish_ok(std::move(ate_msg));
        }
        return rejected("This item cannot be consumed.");
    }

    if (action == "learn") {
        auto* book = form->As<RE::TESObjectBOOK>();
        if (book == nullptr || !book->TeachesSpell()) {
            return rejected("This item is not a Spell Tome.");
        }
        auto* spell = book->GetSpell();
        const std::string spell_name = (spell != nullptr && spell->GetFullName() && spell->GetFullName()[0] != '\0')
            ? spell->GetFullName()
            : item_name;
        if (spell != nullptr && player->HasSpell(spell)) {
            return rejected("You already know " + spell_name + ".");
        }
        (void)book->Read(player);
        if (spell != nullptr && !player->HasSpell(spell)) {
            player->AddSpell(spell);
        }
        const auto after_inv = player->GetInventory();
        const auto after_it = after_inv.find(form);
        if (after_it != after_inv.end() && after_it->second.first >= carried_count) {
            player->RemoveItem(form, 1, RE::ITEM_REMOVE_REASON::kRemove, extra, nullptr);
        }
        ConsumePlayerStolenCount(form, entry, carried_count, 1, std::max(0, carried_count - 1), from_stolen_stack || clean_count == 0);
        RE::PlaySound("UISkillIncrease");
        return finish_ok("Learned " + spell_name + ".");
    }

    if (action == "read") {
        auto* book = form->As<RE::TESObjectBOOK>();
        if (book == nullptr) {
            return rejected("This item cannot be read.");
        }
        if (book->TeachesSpell()) {
            return PerformInventoryItemAction({ { "formId", std::to_string(form_id) }, { "action", "learn" } });
        }
        RE::BSString raw_desc;
        book->GetDescription(raw_desc, nullptr);
        if (raw_desc.empty()) {
            book->GetDescription(raw_desc, book);
        }
        if (raw_desc.empty()) {
            book->itemCardDescription.GetDescription(raw_desc, nullptr);
        }
        const char* p_name = player->GetName();
        auto parsed_book = ParseBookContent(
            raw_desc.c_str() ? raw_desc.c_str() : "",
            item_name,
            p_name && p_name[0] != '\0' ? p_name : "Dragonborn");
        if (parsed_book.images.empty()) {
            ExtractBookModelTextures(book, item_name, parsed_book.images, parsed_book.text.empty());
        }
        const bool was_read = book->IsRead();
        (void)book->Read(player);
        if (open_native) {
            manager->EquipObject(player, book, extra, 1, nullptr, false, false, true, false);
        } else {
            RE::PlaySound("ITMBookOpenSD");
        }
        std::string msg = "Reading " + item_name + ".";
        if (!was_read && book->TeachesSkill()) {
            msg = "Read " + item_name + " and increased your skill!";
        }
        const auto lower_title = LowerAscii(item_name);
        std::string book_style = "book";
        if (lower_title.find("map") != std::string::npos || (!parsed_book.images.empty() && parsed_book.text.size() < 120)) {
            book_style = "map";
        } else if (lower_title.find("note") != std::string::npos || lower_title.find("letter") != std::string::npos ||
                   lower_title.find("recipe") != std::string::npos || lower_title.find("bounty") != std::string::npos ||
                   lower_title.find("orders") != std::string::npos || lower_title.find("missives") != std::string::npos ||
                   (parsed_book.pages.size() <= 1 && parsed_book.text.size() < 900)) {
            book_style = "note";
        }
        return finish_ok(std::move(msg), {
            { "bookTitle", item_name },
            { "bookText", parsed_book.text.empty()
                ? (!parsed_book.images.empty() ? std::string{} : ("[" + item_name + "]\n\nThis note or book has no inscription text."))
                : parsed_book.text },
            { "bookPages", parsed_book.pages },
            { "bookImages", parsed_book.images },
            { "bookStyle", book_style }
        });
    }

    if (action == "charge") {
        RE::TESSoulGem* chosen_gem = nullptr;
        RE::ExtraDataList* chosen_gem_extra = nullptr;
        RE::SOUL_LEVEL best_soul = RE::SOUL_LEVEL::kNone;
        for (const auto& [cand_form, cand_entry] : inventory) {
            if (cand_form == nullptr || cand_entry.first <= 0 || !cand_form->Is(RE::FormType::SoulGem)) {
                continue;
            }
            auto* gem = cand_form->As<RE::TESSoulGem>();
            auto* gem_data = cand_entry.second.get();
            RE::SOUL_LEVEL soul = gem_data != nullptr ? gem_data->GetSoulLevel() : RE::SOUL_LEVEL::kNone;
            RE::ExtraDataList* gem_extra = nullptr;
            if (gem_data != nullptr && gem_data->extraLists != nullptr) {
                for (auto* x_list : *gem_data->extraLists) {
                    if (x_list == nullptr) continue;
                    const auto list_soul = x_list->GetSoulLevel();
                    if (list_soul != RE::SOUL_LEVEL::kNone) {
                        soul = list_soul;
                        gem_extra = x_list;
                        break;
                    }
                    if (const auto* ex_soul = x_list->GetByType<RE::ExtraSoul>(); ex_soul != nullptr && ex_soul->GetContainedSoul() != RE::SOUL_LEVEL::kNone) {
                        soul = ex_soul->GetContainedSoul();
                        gem_extra = x_list;
                        break;
                    }
                }
            }
            if (soul == RE::SOUL_LEVEL::kNone && gem != nullptr) {
                soul = gem->GetContainedSoul();
            }
            if (soul != RE::SOUL_LEVEL::kNone) {
                if (chosen_gem == nullptr || static_cast<int>(soul) < static_cast<int>(best_soul)) {
                    chosen_gem = gem;
                    chosen_gem_extra = gem_extra;
                    best_soul = soul;
                }
            }
        }
        if (chosen_gem == nullptr || best_soul == RE::SOUL_LEVEL::kNone) {
            return rejected("You have no filled Soul Gems in your inventory to charge this item.");
        }

        const auto current_pct = entry != nullptr ? entry->GetEnchantmentCharge() : std::nullopt;
        if (current_pct.has_value() && *current_pct >= 99.5) {
            return rejected(item_name + " is already fully charged.");
        }

        float soul_points = 500.0F;
        switch (best_soul) {
        case RE::SOUL_LEVEL::kPetty: soul_points = 250.0F; break;
        case RE::SOUL_LEVEL::kLesser: soul_points = 500.0F; break;
        case RE::SOUL_LEVEL::kCommon: soul_points = 1000.0F; break;
        case RE::SOUL_LEVEL::kGreater: soul_points = 1500.0F; break;
        case RE::SOUL_LEVEL::kGrand: soul_points = 3000.0F; break;
        default: break;
        }

        float max_charge = 1000.0F;
        if (const auto* ench_form = form->As<RE::TESEnchantableForm>(); ench_form != nullptr && ench_form->amountofEnchantment > 0) {
            max_charge = static_cast<float>(ench_form->amountofEnchantment);
        }
        if (entry != nullptr && entry->extraLists != nullptr) {
            for (auto* x_list : *entry->extraLists) {
                if (x_list == nullptr) continue;
                if (const auto* ex_ench = x_list->GetByType<RE::ExtraEnchantment>(); ex_ench != nullptr && ex_ench->charge > 0) {
                    max_charge = static_cast<float>(ex_ench->charge);
                }
                if (auto* ex_charge = x_list->GetByType<RE::ExtraCharge>(); ex_charge != nullptr) {
                    ex_charge->charge = std::min(max_charge, ex_charge->charge + soul_points);
                }
            }
        }
        if (auto* av_owner = player->AsActorValueOwner(); av_owner != nullptr) {
            if (player->GetEquippedObject(false) == form) {
                av_owner->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kRightItemCharge, soul_points);
            }
            if (player->GetEquippedObject(true) == form) {
                av_owner->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kLeftItemCharge, soul_points);
            }
        }

        const auto gem_id = chosen_gem->GetFormID();
        const std::string gem_name = (chosen_gem->GetName() && chosen_gem->GetName()[0] != '\0') ? chosen_gem->GetName() : "Soul Gem";
        const bool reusable_star = (gem_id == 0x00063B27 || gem_id == 0x00063B29) || chosen_gem->linkedSoulGem != nullptr;
        if (reusable_star && chosen_gem_extra != nullptr) {
            if (auto* ex_soul = chosen_gem_extra->GetByType<RE::ExtraSoul>(); ex_soul != nullptr) {
                ex_soul->soul = RE::SOUL_LEVEL::kNone;
            }
        } else {
            player->RemoveItem(chosen_gem, 1, RE::ITEM_REMOVE_REASON::kRemove, chosen_gem_extra, nullptr);
        }
        player->UseSkill(RE::ActorValue::kEnchanting, soul_points * 0.02F, nullptr);
        RE::PlaySound("UIMenuOK");
        return finish_ok("Charged " + item_name + " with " + gem_name + ".");
    }

    if (action == "activate") {
        // Mirror Skyrim's own InventoryMenu "use" path: ActorEquipManager::EquipObject.
        // For MISC / soul gem / key items this does not equip anything; it fires the
        // item's OnEquipped Papyrus event, which is how storage gems, backpacks and
        // other scripted items open their container or menu.
        //
        // NEVER call TESBoundObject::Activate(player, player, ...) here: the first
        // argument is the *reference being activated*, so the engine treats the
        // PlayerCharacter as a world item being picked up and flags it kDeleted with
        // no parent cell. Actor job threads then dereference the player's null cell
        // data and crash (SkyrimSE.exe+06EB4E0, movzx eax, byte ptr [rcx+0x13A]).
        //
        // Queue the equip while the world is running (applying immediately outside the
        // equip queue is only safe while the game is paused), matching
        // ChangePlayerEquipment.
        //
        // MISC / soul gem / key items are the exception: they have no 3D or equip slot, so
        // the deferred equip queue is not a reliable way to deliver their OnEquipped event.
        // Apply them immediately (we are on the game thread), like the paused vanilla menu.
        const bool scripted_use_item = form->Is(RE::FormType::Misc) || form->Is(RE::FormType::SoulGem) ||
            form->Is(RE::FormType::KeyMaster);
        const bool queue_equip = !scripted_use_item;
        const bool apply_now = false;
        manager->EquipObject(player, form, extra, 1, nullptr, queue_equip, false, true, apply_now);
        if (scripted_use_item) {
            // Scripts on these items typically call Game.DisablePlayerControls() (which closes
            // Skyrim's InventoryMenu) and Utility.Wait(), which only advances once no
            // game-pausing menu is open, before opening their container. Close our inventory
            // replacement the same way so the script can continue and its menu can appear.
            skyultra::RequestCloseInventoryReplacement("inventory item activated");
        }
        return finish_ok("Activated " + item_name + ".");
    }

    return rejected("Unsupported item action '" + action + "'.");
}

// ---------------------------------------------------------------------------
// Spellbook (Magic menu replacement) and Favorites (Favorites menu replacement)
// ---------------------------------------------------------------------------

std::string_view SpellSchoolId(RE::ActorValue skill) noexcept
{
    switch (skill) {
    case RE::ActorValue::kAlteration: return "alteration";
    case RE::ActorValue::kConjuration: return "conjuration";
    case RE::ActorValue::kDestruction: return "destruction";
    case RE::ActorValue::kIllusion: return "illusion";
    case RE::ActorValue::kRestoration: return "restoration";
    default: return "none";
    }
}

std::string_view SpellLevelName(std::int32_t minimum_skill) noexcept
{
    if (minimum_skill >= 100) return "Master";
    if (minimum_skill >= 75) return "Expert";
    if (minimum_skill >= 50) return "Adept";
    if (minimum_skill >= 25) return "Apprentice";
    return "Novice";
}

std::string_view SpellCastingTypeId(RE::MagicSystem::CastingType type) noexcept
{
    switch (type) {
    case RE::MagicSystem::CastingType::kConstantEffect: return "constantEffect";
    case RE::MagicSystem::CastingType::kConcentration: return "concentration";
    case RE::MagicSystem::CastingType::kScroll: return "scroll";
    default: return "fireAndForget";
    }
}

std::string_view SpellDeliveryId(RE::MagicSystem::Delivery delivery) noexcept
{
    switch (delivery) {
    case RE::MagicSystem::Delivery::kSelf: return "self";
    case RE::MagicSystem::Delivery::kTouch: return "touch";
    case RE::MagicSystem::Delivery::kTargetActor: return "targetActor";
    case RE::MagicSystem::Delivery::kTargetLocation: return "targetLocation";
    default: return "aimed";
    }
}

// Visual hint for the UI's animated spell sigil, derived from the costliest effect.
std::string SpellElementHint(const RE::EffectSetting* base)
{
    if (base == nullptr) return "arcane";
    switch (base->data.resistVariable) {
    case RE::ActorValue::kResistFire: return "fire";
    case RE::ActorValue::kResistFrost: return "frost";
    case RE::ActorValue::kResistShock: return "shock";
    default: break;
    }
    using A = RE::EffectSetting::Archetype;
    switch (base->GetArchetype()) {
    case A::kSummonCreature:
    case A::kReanimate:
    case A::kCommandSummoned: return "summon";
    case A::kBoundWeapon: return "bound";
    case A::kLight:
    case A::kGuide: return "light";
    case A::kTurnUndead:
    case A::kBanish: return "sun";
    case A::kCalm:
    case A::kFrenzy:
    case A::kDemoralize:
    case A::kRally: return "mind";
    case A::kInvisibility:
    case A::kEtherealize: return "shadow";
    case A::kParalysis:
    case A::kSlowTime: return "time";
    case A::kDetectLife: return "detect";
    case A::kTelekinesis:
    case A::kGrabActor: return "telekinesis";
    case A::kSoulTrap: return "soul";
    case A::kCureDisease:
    case A::kCurePoison:
    case A::kCureParalysis: return "heal";
    default: break;
    }
    if (base->data.primaryAV == RE::ActorValue::kWardPower) return "ward";
    if (base->data.primaryAV == RE::ActorValue::kHealth && !base->IsHostile()) return "heal";
    if (base->data.primaryAV == RE::ActorValue::kDamageResist) return "armor";
    if (base->IsHostile()) return "harm";
    return "arcane";
}

std::string FormatEffectDescription(const RE::Effect* eff)
{
    if (eff == nullptr || eff->baseEffect == nullptr) return {};
    const float mag = std::isfinite(eff->GetMagnitude()) ? std::max(0.0F, eff->GetMagnitude()) : 0.0F;
    const auto dur = eff->GetDuration();
    const std::string mag_str = std::to_string(static_cast<int>(std::round(mag)));
    std::string desc = eff->baseEffect->magicItemDescription.c_str() ? eff->baseEffect->magicItemDescription.c_str() : "";
    for (std::size_t pos = 0; (pos = desc.find('<', pos)) != std::string::npos;) {
        const auto end = desc.find('>', pos);
        if (end == std::string::npos) break;
        const auto token = LowerAscii(desc.substr(pos + 1, end - pos - 1));
        const std::string repl = token == "mag" ? mag_str : token == "dur" ? std::to_string(dur) :
            token == "area" ? std::to_string(eff->GetArea()) : "";
        desc.replace(pos, end - pos + 1, repl);
        pos += repl.size();
    }
    if (desc.empty()) {
        if (mag > 0.0F && dur > 0U) desc = mag_str + " points for " + std::to_string(dur) + " seconds";
        else if (mag > 0.0F) desc = mag_str + " points";
        else if (dur > 0U) desc = std::to_string(dur) + " seconds";
    }
    return desc;
}

Json SpellEffectsJson(const RE::MagicItem* item, std::string& out_summary)
{
    Json effects = Json::array();
    if (item == nullptr) return effects;
    for (const auto* eff : item->effects) {
        if (eff == nullptr || eff->baseEffect == nullptr) continue;
        if (eff->baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) continue;
        const auto* name = eff->baseEffect->GetFullName();
        if (name == nullptr || name[0] == '\0') continue;
        auto desc = FormatEffectDescription(eff);
        if (!desc.empty()) {
            if (!out_summary.empty()) out_summary += " ";
            out_summary += desc;
        }
        effects.push_back({
            { "name", StripLeadingDollar(name) },
            { "description", std::move(desc) },
            { "magnitude", std::isfinite(eff->GetMagnitude()) ? eff->GetMagnitude() : 0.0F },
            { "duration", eff->GetDuration() },
            { "area", eff->GetArea() }
        });
        if (effects.size() >= 16) break;
    }
    return effects;
}

std::unordered_map<RE::FormID, std::string> g_custom_spell_names;
std::unordered_set<RE::FormID> g_deleted_spell_form_ids;
std::unordered_set<std::string> g_deleted_active_effect_ids;

std::string SpellOrShoutDisplayName(const RE::TESForm* form)
{
    if (form == nullptr) return {};
    if (const auto it = g_custom_spell_names.find(form->GetFormID()); it != g_custom_spell_names.end() && !it->second.empty()) {
        return it->second;
    }
    const auto* raw = form->GetName();
    return raw != nullptr ? StripLeadingDollar(raw) : std::string{};
}

struct PlayerSpellSources {
    std::vector<RE::SpellItem*> spells;
    std::vector<RE::TESShout*> shouts;
};

PlayerSpellSources CollectPlayerSpells(RE::PlayerCharacter* player)
{
    PlayerSpellSources out;
    std::unordered_set<RE::FormID> seen_spells;
    std::unordered_set<RE::FormID> seen_shouts;
    const auto add_spell = [&](RE::SpellItem* spell) {
        if (spell == nullptr || g_deleted_spell_form_ids.contains(spell->GetFormID()) || !seen_spells.insert(spell->GetFormID()).second) return;
        const auto type = spell->GetSpellType();
        if (type != RE::MagicSystem::SpellType::kSpell && type != RE::MagicSystem::SpellType::kPower &&
            type != RE::MagicSystem::SpellType::kLesserPower) return;
        const auto name = SpellOrShoutDisplayName(spell);
        if (name.empty()) return;
        out.spells.push_back(spell);
    };
    const auto add_list = [&](RE::TESSpellList::SpellData* data) {
        if (data == nullptr) return;
        if (data->spells != nullptr) {
            for (std::uint32_t i = 0; i < data->numSpells; ++i) add_spell(data->spells[i]);
        }
        if (data->shouts != nullptr) {
            for (std::uint32_t i = 0; i < data->numShouts; ++i) {
                auto* shout = data->shouts[i];
                if (shout != nullptr && !g_deleted_spell_form_ids.contains(shout->GetFormID()) && seen_shouts.insert(shout->GetFormID()).second) out.shouts.push_back(shout);
            }
        }
    };
    for (auto* spell : player->GetActorRuntimeData().addedSpells) add_spell(spell);
    if (auto* base = player->GetActorBase(); base != nullptr) add_list(base->actorEffects);
    if (auto* race = player->GetRace(); race != nullptr) add_list(race->actorEffects);
    return out;
}

bool IsMagicFavorite(const RE::TESForm* form)
{
    auto* favorites = RE::MagicFavorites::GetSingleton();
    if (favorites == nullptr || form == nullptr) return false;
    for (const auto* entry : favorites->spells) {
        if (entry == form) return true;
    }
    return false;
}

Json SpellEquippedJson(RE::PlayerCharacter* player)
{
    auto& rt = player->GetActorRuntimeData();
    const auto id_of = [](const RE::TESForm* form) -> Json { return form ? Json(std::to_string(form->GetFormID())) : Json(nullptr); };
    return {
        { "left", id_of(rt.selectedSpells[RE::Actor::SlotTypes::kLeftHand]) },
        { "right", id_of(rt.selectedSpells[RE::Actor::SlotTypes::kRightHand]) },
        { "voice", id_of(rt.selectedPower) }
    };
}

Json PlayerSpellJson(RE::PlayerCharacter* player, RE::SpellItem* spell)
{
    auto& rt = player->GetActorRuntimeData();
    const auto type = spell->GetSpellType();
    const bool is_voice = type == RE::MagicSystem::SpellType::kPower || type == RE::MagicSystem::SpellType::kLesserPower;
    const auto* costliest = spell->GetCostliestEffectItem();
    const auto* base = costliest != nullptr ? costliest->baseEffect : nullptr;
    const auto school = SpellSchoolId(base != nullptr ? base->GetMagickSkill() : RE::ActorValue::kNone);
    std::string summary;
    auto effects = SpellEffectsJson(spell, summary);
    const float cost = std::max(0.0F, spell->CalculateMagickaCost(player));
    Json json{
        { "id", std::to_string(spell->GetFormID()) },
        { "name", SpellOrShoutDisplayName(spell) },
        { "kind", type == RE::MagicSystem::SpellType::kPower ? "power" : type == RE::MagicSystem::SpellType::kLesserPower ? "lesserPower" : "spell" },
        { "category", is_voice ? std::string_view("powers") : (school == "none" ? std::string_view("other") : school) },
        { "school", school },
        { "cost", std::isfinite(cost) ? static_cast<int>(std::round(cost)) : 0 },
        { "castingType", SpellCastingTypeId(spell->GetCastingType()) },
        { "delivery", SpellDeliveryId(spell->GetDelivery()) },
        { "element", SpellElementHint(base) },
        { "description", summary },
        { "effects", std::move(effects) },
        { "equippedLeft", rt.selectedSpells[RE::Actor::SlotTypes::kLeftHand] == spell },
        { "equippedRight", rt.selectedSpells[RE::Actor::SlotTypes::kRightHand] == spell },
        { "equippedVoice", rt.selectedPower == spell },
        { "favorite", IsMagicFavorite(spell) },
        { "twoHanded", spell->IsTwoHanded() },
        { "chargeTime", std::isfinite(spell->GetChargeTime()) ? spell->GetChargeTime() : 0.0F }
    };
    if (is_voice) {
        json["hands"] = Json::array({ "voice" });
    } else if (spell->IsTwoHanded()) {
        json["hands"] = Json::array({ "both" });
    } else {
        json["hands"] = Json::array({ "left", "right", "both" });
    }
    if (base != nullptr && school != "none" && !is_voice) {
        const auto minimum = base->GetMinimumSkillLevel();
        json["skillLevel"] = minimum;
        json["level"] = SpellLevelName(minimum);
    }
    return json;
}

Json PlayerShoutJson(RE::PlayerCharacter* player, RE::TESShout* shout)
{
    auto& rt = player->GetActorRuntimeData();
    Json words = Json::array();
    RE::SpellItem* strongest = nullptr;
    float cooldown = 0.0F;
    for (const auto& variation : shout->variations) {
        if (variation.word == nullptr) continue;
        const bool known = variation.word->GetKnown();
        const auto* word_name = variation.word->GetFullName();
        words.push_back({
            { "word", word_name != nullptr ? StripLeadingDollar(word_name) : std::string{} },
            { "translation", variation.word->translation.c_str() ? variation.word->translation.c_str() : "" },
            { "unlocked", known }
        });
        if (known && variation.spell != nullptr) {
            strongest = variation.spell;
            cooldown = variation.recoveryTime;
        }
    }
    if (strongest == nullptr && shout->variations[0].spell != nullptr) {
        strongest = shout->variations[0].spell;
        cooldown = shout->variations[0].recoveryTime;
    }
    std::string summary;
    auto effects = SpellEffectsJson(strongest, summary);
    const auto* costliest = strongest != nullptr ? strongest->GetCostliestEffectItem() : nullptr;
    return {
        { "id", std::to_string(shout->GetFormID()) },
        { "name", SpellOrShoutDisplayName(shout) },
        { "kind", "shout" },
        { "category", "shouts" },
        { "school", "none" },
        { "cost", 0 },
        { "castingType", "fireAndForget" },
        { "delivery", strongest != nullptr ? SpellDeliveryId(strongest->GetDelivery()) : std::string_view("aimed") },
        { "element", costliest != nullptr ? SpellElementHint(costliest->baseEffect) : std::string("voice") },
        { "description", summary },
        { "effects", std::move(effects) },
        { "equippedLeft", false },
        { "equippedRight", false },
        { "equippedVoice", rt.selectedPower == shout },
        { "favorite", IsMagicFavorite(shout) },
        { "hands", Json::array({ "voice" }) },
        { "cooldown", std::isfinite(cooldown) ? cooldown : 0.0F },
        { "words", std::move(words) }
    };
}

std::vector<Json> ReadPlayerSpellList(RE::PlayerCharacter* player)
{
    std::vector<Json> list;
    const auto sources = CollectPlayerSpells(player);
    list.reserve(sources.spells.size() + sources.shouts.size());
    for (auto* spell : sources.spells) list.push_back(PlayerSpellJson(player, spell));
    for (auto* shout : sources.shouts) {
        const auto* name = shout->GetFullName();
        if (name == nullptr || name[0] == '\0') continue;
        bool any_known = false;
        for (const auto& variation : shout->variations) any_known = any_known || (variation.word && variation.word->GetKnown());
        if (!any_known) continue;
        list.push_back(PlayerShoutJson(player, shout));
    }
    std::sort(list.begin(), list.end(), [](const Json& a, const Json& b) {
        return LowerAscii(a.value("name", std::string{})) < LowerAscii(b.value("name", std::string{}));
    });
    return list;
}

Json ReadSpellBook()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return { { "available", false }, { "equipped", Json::object() }, { "categories", Json::array() }, { "spells", Json::array() }, { "message", "The player is unavailable." } };
    }
    auto spells = ReadPlayerSpellList(player);
    constexpr std::array<std::pair<std::string_view, std::string_view>, 8> kCategories{ {
        { "alteration", "Alteration" }, { "conjuration", "Conjuration" }, { "destruction", "Destruction" },
        { "illusion", "Illusion" }, { "restoration", "Restoration" }, { "powers", "Powers" },
        { "shouts", "Shouts" }, { "other", "Other" } } };
    Json categories = Json::array();
    for (const auto& [id, label] : kCategories) {
        const auto count = std::count_if(spells.begin(), spells.end(), [&](const Json& s) { return s.value("category", std::string{}) == id; });
        if (count > 0 || id != "other") categories.push_back({ { "id", id }, { "label", label }, { "count", count } });
    }
    Json magicka = Json::object();
    if (auto* av = player->AsActorValueOwner(); av != nullptr) {
        const float current = av->GetActorValue(RE::ActorValue::kMagicka);
        const float max = av->GetPermanentActorValue(RE::ActorValue::kMagicka) +
            player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka);
        magicka = { { "current", std::max(0, static_cast<int>(std::round(current))) }, { "max", std::max(1, static_cast<int>(std::round(max))) } };
    }
    return {
        { "available", true },
        { "magicka", std::move(magicka) },
        { "equipped", SpellEquippedJson(player) },
        { "categories", std::move(categories) },
        { "spells", std::move(spells) }
    };
}

Json EquipPlayerSpell(const Json& options)
{
    const auto rejected = [](std::string message) -> Json { return { { "accepted", false }, { "message", std::move(message) } }; };
    if (!options.is_object()) return rejected("Spell options must be an object.");
    std::uint32_t form_id = 0;
    if (const auto field = options.find("formId"); field != options.end()) {
        if (field->is_number_integer()) form_id = static_cast<std::uint32_t>(field->get<std::int64_t>());
        else if (field->is_string()) {
            const auto text = field->get<std::string>();
            const bool hex = text.starts_with("0x") || text.starts_with("0X");
            (void)std::from_chars(text.data() + (hex ? 2 : 0), text.data() + text.size(), form_id, hex ? 16 : 10);
        }
    }
    const auto hand = options.value("hand", std::string("right"));
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* manager = RE::ActorEquipManager::GetSingleton();
    auto* form = form_id != 0 ? RE::TESForm::LookupByID(form_id) : nullptr;
    if (player == nullptr || manager == nullptr || form == nullptr || player->IsDead()) return rejected("That spell is unavailable.");
    auto* defaults = RE::BGSDefaultObjectManager::GetSingleton();
    const auto equip_slot = [&](RE::DEFAULT_OBJECT object) -> const RE::BGSEquipSlot* {
        return defaults != nullptr ? defaults->GetObject<RE::BGSEquipSlot>(object) : nullptr;
    };
    const auto done = [&](std::string message) -> Json {
        RE::PlaySound("UIMenuOK");
        skyultra::InvalidateSharedLightGameDataCache();
        return { { "accepted", true }, { "formId", std::to_string(form_id) }, { "hand", hand }, { "message", std::move(message) }, { "equipped", SpellEquippedJson(player) } };
    };
    const std::string name = form->GetName() != nullptr ? StripLeadingDollar(form->GetName()) : std::string("Spell");

    if (auto* shout = form->As<RE::TESShout>(); shout != nullptr) {
        const auto sources = CollectPlayerSpells(player);
        if (std::find(sources.shouts.begin(), sources.shouts.end(), shout) == sources.shouts.end()) return rejected("You have not learned " + name + ".");
        manager->EquipShout(player, shout);
        return done(name + " readied as your shout.");
    }
    auto* spell = form->As<RE::SpellItem>();
    if (spell == nullptr) return rejected("That form is not a spell.");
    const auto sources = CollectPlayerSpells(player);
    if (std::find(sources.spells.begin(), sources.spells.end(), spell) == sources.spells.end()) return rejected("You do not know " + name + ".");
    const auto type = spell->GetSpellType();
    if (type == RE::MagicSystem::SpellType::kPower || type == RE::MagicSystem::SpellType::kLesserPower) {
        manager->EquipSpell(player, spell, equip_slot(RE::DEFAULT_OBJECT::kVoiceEquip));
        return done(name + " readied as your power.");
    }
    if (hand == "voice") return rejected(name + " cannot be equipped to the voice slot.");
    if (hand == "both" || spell->IsTwoHanded()) {
        if (spell->IsTwoHanded()) {
            manager->EquipSpell(player, spell, nullptr);
        } else {
            manager->EquipSpell(player, spell, equip_slot(RE::DEFAULT_OBJECT::kLeftHandEquip));
            manager->EquipSpell(player, spell, equip_slot(RE::DEFAULT_OBJECT::kRightHandEquip));
        }
        return done(name + " equipped in both hands.");
    }
    const bool left = hand == "left";
    manager->EquipSpell(player, spell, equip_slot(left ? RE::DEFAULT_OBJECT::kLeftHandEquip : RE::DEFAULT_OBJECT::kRightHandEquip));
    return done(name + (left ? " equipped in your left hand." : " equipped in your right hand."));
}

RE::InventoryEntryData* FindLiveInventoryEntry(RE::PlayerCharacter* player, RE::TESBoundObject* object);

Json RenamePlayerItem(const Json& options)
{
    const auto rejected = [](std::string message) -> Json {
        return { { "updated", false }, { "accepted", false }, { "message", std::move(message) } };
    };
    if (!options.is_object()) return rejected("Item rename options must be an object.");
    bool from_stolen_stack = false;
    std::uint32_t form_id = ParseItemFormIdField(options, "id", &from_stolen_stack);
    if (form_id == 0) {
        form_id = ParseItemFormIdField(options, "formId", &from_stolen_stack);
    }
    const bool clear_custom = options.value("clear", false);
    auto new_name = options.value("name", std::string{});
    while (!new_name.empty() && std::isspace(static_cast<unsigned char>(new_name.front()))) new_name.erase(new_name.begin());
    while (!new_name.empty() && std::isspace(static_cast<unsigned char>(new_name.back()))) new_name.pop_back();
    if (!clear_custom && new_name.empty()) return rejected("Enter a non-empty item name.");
    if (new_name.size() > 96) new_name.resize(96);

    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* form = form_id != 0 ? RE::TESForm::LookupByID(form_id) : nullptr;
    if (player == nullptr || form == nullptr) return rejected("That inventory item is unavailable.");
    const std::string base_name = form->GetName() != nullptr && form->GetName()[0] != '\0'
        ? StripLeadingDollar(form->GetName())
        : std::string("Item");

    std::string final_name = clear_custom ? base_name : new_name;
    {
        const std::scoped_lock lock(g_custom_item_names_mutex);
        if (clear_custom) {
            g_custom_item_names.erase(form_id);
        } else {
            g_custom_item_names[form_id] = final_name;
        }
    }
    if (auto* object = form->As<RE::TESBoundObject>(); object != nullptr) {
        if (auto* live_entry = FindLiveInventoryEntry(player, object);
            live_entry != nullptr && live_entry->extraLists != nullptr) {
            for (auto* x_list : *live_entry->extraLists) {
                if (x_list == nullptr) continue;
                if (auto* ex_text = x_list->GetByType<RE::ExtraTextDisplayData>(); ex_text != nullptr) {
                    ex_text->SetName(final_name.c_str());
                }
            }
        }
    }
    RE::PlaySound("UIMenuOK");
    skyultra::InvalidateSharedGameDataCache();
    const std::string item_id = from_stolen_stack ? (std::to_string(form_id) + ":stolen") : std::to_string(form_id);
    return {
        { "updated", true },
        { "accepted", true },
        { "cleared", clear_custom },
        { "id", item_id },
        { "formId", std::to_string(form_id) },
        { "name", final_name },
        { "message", clear_custom ? ("Restored name to \"" + final_name + "\".") : ("Renamed item to \"" + final_name + "\".") }
    };
}

Json RenamePlayerSpell(const Json& options)
{
    const auto rejected = [](std::string message) -> Json { return { { "accepted", false }, { "message", std::move(message) } }; };
    if (!options.is_object()) return rejected("Spell rename options must be an object.");
    std::uint32_t form_id = 0;
    if (const auto field = options.find("formId"); field != options.end()) {
        if (field->is_number_integer()) form_id = static_cast<std::uint32_t>(field->get<std::int64_t>());
        else if (field->is_string()) {
            const auto text = field->get<std::string>();
            const bool hex = text.starts_with("0x") || text.starts_with("0X");
            (void)std::from_chars(text.data() + (hex ? 2 : 0), text.data() + text.size(), form_id, hex ? 16 : 10);
        }
    }
    auto new_name = options.value("name", std::string{});
    while (!new_name.empty() && std::isspace(static_cast<unsigned char>(new_name.front()))) new_name.erase(new_name.begin());
    while (!new_name.empty() && std::isspace(static_cast<unsigned char>(new_name.back()))) new_name.pop_back();
    if (new_name.empty()) return rejected("Enter a non-empty name.");
    if (new_name.size() > 96) new_name.resize(96);

    auto* form = form_id != 0 ? RE::TESForm::LookupByID(form_id) : nullptr;
    if (form == nullptr) return rejected("That spell, shout, or power is unavailable.");
    auto* spell = form->As<RE::SpellItem>();
    auto* shout = form->As<RE::TESShout>();
    if (spell == nullptr && shout == nullptr) return rejected("Only spells, shouts, and powers can be renamed.");

    const std::string old_name = SpellOrShoutDisplayName(form);
    g_custom_spell_names[form_id] = new_name;
    if (auto* full_name = form->As<RE::TESFullName>(); full_name != nullptr) {
        full_name->SetFullName(new_name.c_str());
    }
    RE::PlaySound("UIMenuOK");
    skyultra::InvalidateSharedLightGameDataCache();
    return {
        { "accepted", true },
        { "formId", std::to_string(form_id) },
        { "oldName", old_name },
        { "name", new_name },
        { "message", "Renamed \"" + (old_name.empty() ? std::string("Spell") : old_name) + "\" to \"" + new_name + "\"." }
    };
}

Json DeletePlayerSpellOrEffect(const Json& options)
{
    const auto rejected = [](std::string message) -> Json { return { { "accepted", false }, { "message", std::move(message) } }; };
    if (!options.is_object()) return rejected("Delete options must be an object.");
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr || player->IsDead()) return rejected("The player is unavailable.");

    const auto kind = LowerAscii(options.value("kind", std::string("spell")));
    std::string raw_id;
    std::uint32_t form_id = 0;
    if (const auto field = options.find("formId"); field != options.end()) {
        if (field->is_number_integer()) {
            form_id = static_cast<std::uint32_t>(field->get<std::int64_t>());
            raw_id = std::to_string(form_id);
        } else if (field->is_string()) {
            raw_id = field->get<std::string>();
            const auto colon = raw_id.find(':');
            const auto id_part = colon != std::string::npos ? raw_id.substr(0, colon) : raw_id;
            const bool hex = id_part.starts_with("0x") || id_part.starts_with("0X");
            (void)std::from_chars(id_part.data() + (hex ? 2 : 0), id_part.data() + id_part.size(), form_id, hex ? 16 : 10);
        }
    }

    if (kind == "effect" || raw_id.find(':') != std::string::npos) {
        std::uint16_t target_uid = 0;
        bool has_uid = false;
        if (const auto colon = raw_id.find(':'); colon != std::string::npos) {
            const auto uid_part = raw_id.substr(colon + 1);
            const auto parsed = std::from_chars(uid_part.data(), uid_part.data() + uid_part.size(), target_uid);
            has_uid = (parsed.ec == std::errc{});
        }
        std::string removed_name;
        bool dispelled_any = false;
        if (auto* magic_target = player->AsMagicTarget(); magic_target != nullptr) {
            RE::MagicTarget::EffectVisitor visitor([&](RE::ActiveEffect* active_effect) {
                if (active_effect == nullptr) return RE::BSContainer::ForEachResult::kContinue;
                const auto* base_effect = active_effect->GetBaseObject();
                const auto base_id = base_effect != nullptr ? base_effect->GetFormID() : 0U;
                const auto comp_id = std::to_string(base_id) + ":" + std::to_string(active_effect->usUniqueID);
                const bool match = (!raw_id.empty() && comp_id == raw_id) ||
                    (has_uid && active_effect->usUniqueID == target_uid && (form_id == 0 || base_id == form_id)) ||
                    (!has_uid && form_id != 0 && base_id == form_id);
                if (!match) return RE::BSContainer::ForEachResult::kContinue;
                if (removed_name.empty() && base_effect != nullptr && base_effect->GetName() != nullptr) {
                    removed_name = StripLeadingDollar(base_effect->GetName());
                }
                active_effect->Dispel(true);
                active_effect->flags.set(RE::ActiveEffect::Flag::kDispelled);
                active_effect->flags.set(RE::ActiveEffect::Flag::kInactive);
                g_deleted_active_effect_ids.insert(comp_id);
                dispelled_any = true;
                return has_uid ? RE::BSContainer::ForEachResult::kStop : RE::BSContainer::ForEachResult::kContinue;
            });
            magic_target->VisitEffects(visitor);
        }
        if (!raw_id.empty()) {
            g_deleted_active_effect_ids.insert(raw_id);
        }
        if (!dispelled_any && raw_id.empty()) {
            return rejected("Active effect could not be found.");
        }
        if (removed_name.empty()) removed_name = "Active Effect";
        RE::PlaySound("UIMenuCancel");
        skyultra::InvalidateSharedLightGameDataCache(true);
        return {
            { "accepted", true },
            { "formId", raw_id },
            { "kind", "effect" },
            { "message", "Dispelled & removed " + removed_name + "." }
        };
    }

    auto* form = form_id != 0 ? RE::TESForm::LookupByID(form_id) : nullptr;
    if (form == nullptr) return rejected("That spell, shout, or power is unavailable.");
    const std::string display_name = SpellOrShoutDisplayName(form).empty() ? std::string("Spell") : SpellOrShoutDisplayName(form);

    if (auto* favorites = RE::MagicFavorites::GetSingleton(); favorites != nullptr) {
        auto& list = favorites->spells;
        if (const auto it = std::find(list.begin(), list.end(), form); it != list.end()) list.erase(it);
        for (auto& slot : favorites->hotkeys) {
            if (slot == form) slot = nullptr;
        }
    }

    auto& rt = player->GetActorRuntimeData();
    if (auto* shout = form->As<RE::TESShout>(); shout != nullptr) {
        if (rt.selectedPower == shout) {
            rt.selectedPower = nullptr;
        }
        if (auto* base = player->GetActorBase(); base != nullptr && base->actorEffects != nullptr) {
            (void)base->actorEffects->RemoveShout(shout);
        }
        if (auto* race = player->GetRace(); race != nullptr && race->actorEffects != nullptr) {
            (void)race->actorEffects->RemoveShout(shout);
        }
        for (auto& variation : shout->variations) {
            if (variation.word != nullptr) {
                variation.word->formFlags &= ~(static_cast<std::uint32_t>(RE::TESForm::RecordFlags::kKnown) |
                                               static_cast<std::uint32_t>(RE::TESForm::RecordFlags::kUnlocked));
            }
        }
        g_deleted_spell_form_ids.insert(form_id);
        g_custom_spell_names.erase(form_id);
        RE::PlaySound("UIMenuCancel");
        skyultra::InvalidateSharedLightGameDataCache();
        return {
            { "accepted", true },
            { "formId", std::to_string(form_id) },
            { "kind", "shout" },
            { "message", "Deleted shout \"" + display_name + "\"." },
            { "equipped", SpellEquippedJson(player) }
        };
    }

    auto* spell = form->As<RE::SpellItem>();
    if (spell == nullptr) return rejected("That form is not a spell, power, or shout.");
    if (rt.selectedSpells[RE::Actor::SlotTypes::kLeftHand] == spell ||
        rt.selectedSpells[RE::Actor::SlotTypes::kRightHand] == spell) {
        player->DeselectSpell(spell);
        if (rt.selectedSpells[RE::Actor::SlotTypes::kLeftHand] == spell) {
            rt.selectedSpells[RE::Actor::SlotTypes::kLeftHand] = nullptr;
        }
        if (rt.selectedSpells[RE::Actor::SlotTypes::kRightHand] == spell) {
            rt.selectedSpells[RE::Actor::SlotTypes::kRightHand] = nullptr;
        }
    }
    if (rt.selectedPower == spell) {
        rt.selectedPower = nullptr;
    }
    (void)player->RemoveSpell(spell);
    if (auto* base = player->GetActorBase(); base != nullptr && base->actorEffects != nullptr) {
        (void)base->actorEffects->RemoveSpell(spell);
    }
    if (auto* race = player->GetRace(); race != nullptr && race->actorEffects != nullptr) {
        (void)race->actorEffects->RemoveSpell(spell);
    }
    g_deleted_spell_form_ids.insert(form_id);
    g_custom_spell_names.erase(form_id);
    const auto type = spell->GetSpellType();
    const std::string resolved_kind = type == RE::MagicSystem::SpellType::kPower ? "power" :
        (type == RE::MagicSystem::SpellType::kLesserPower ? "lesserPower" : "spell");
    RE::PlaySound("UIMenuCancel");
    skyultra::InvalidateSharedLightGameDataCache();
    return {
        { "accepted", true },
        { "formId", std::to_string(form_id) },
        { "kind", resolved_kind },
        { "message", "Deleted \"" + display_name + "\"." },
        { "equipped", SpellEquippedJson(player) }
    };
}

// Inventory favorites must be changed on the live InventoryChanges entry (GetInventory returns copies).
RE::InventoryEntryData* FindLiveInventoryEntry(RE::PlayerCharacter* player, RE::TESBoundObject* object)
{
    auto* changes = player != nullptr ? player->GetInventoryChanges() : nullptr;
    if (changes == nullptr || changes->entryList == nullptr || object == nullptr) return nullptr;
    for (auto* entry : *changes->entryList) {
        if (entry != nullptr && entry->object == object) return entry;
    }
    return nullptr;
}

Json SetPlayerFavorite(const Json& options)
{
    const auto rejected = [](std::string message) -> Json { return { { "accepted", false }, { "message", std::move(message) } }; };
    if (!options.is_object()) return rejected("Favorite options must be an object.");
    const std::uint32_t form_id = ParseItemFormIdField(options, "formId");
    const auto kind = options.value("kind", std::string("item"));
    const bool favorite = options.value("favorite", true);
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* form = form_id != 0 ? RE::TESForm::LookupByID(form_id) : nullptr;
    if (player == nullptr || form == nullptr) return rejected("That entry is unavailable.");
    const std::string name = form->GetName() != nullptr ? StripLeadingDollar(form->GetName()) : std::string("Entry");
    const auto done = [&](bool now_favorite) -> Json {
        RE::PlaySound(now_favorite ? "UIFavorite" : "UIMenuCancel");
        if (kind == "spell") {
            skyultra::InvalidateSharedLightGameDataCache();
        } else {
            skyultra::InvalidateSharedGameDataCache();
        }
        return { { "accepted", true }, { "formId", std::to_string(form_id) }, { "kind", kind }, { "favorite", now_favorite },
                 { "message", name + (now_favorite ? " added to favorites." : " removed from favorites.") } };
    };

    if (kind == "spell") {
        auto* favorites = RE::MagicFavorites::GetSingleton();
        if (favorites == nullptr) return rejected("Magic favorites are unavailable.");
        if (!form->Is(RE::FormType::Spell) && !form->Is(RE::FormType::Shout)) return rejected("That form is not a spell or shout.");
        if (IsMagicFavorite(form) == favorite) return done(favorite);
        if (favorite) favorites->SetFavorite(form);
        else favorites->RemoveFavorite(form);
        return done(IsMagicFavorite(form));
    }

    auto* object = form->As<RE::TESBoundObject>();
    auto* entry = FindLiveInventoryEntry(player, object);
    auto* changes = player->GetInventoryChanges();
    if (entry == nullptr || changes == nullptr) return rejected("You no longer carry " + name + ".");
    if (entry->IsFavorited() == favorite) return done(favorite);
    if (favorite) {
        RE::ExtraDataList* list = nullptr;
        if (entry->extraLists != nullptr) {
            for (auto* candidate : *entry->extraLists) {
                if (candidate != nullptr) { list = candidate; break; }
            }
        }
        changes->SetFavorite(entry, list);
    } else {
        changes->RemoveFavorite(entry, entry->GetFavoriteExtraList());
    }
    return done(entry->IsFavorited());
}

Json ReadPlayerFavorites()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return { { "available", false }, { "items", Json::array() }, { "spells", Json::array() }, { "equipped", Json::object() } };
    }
    Json items = Json::array();
    for (const auto& [form, entry] : player->GetInventory()) {
        if (form == nullptr || entry.first <= 0 || !entry.second || !entry.second->IsFavorited()) continue;
        if (!IsPlayableNamedInventoryItem(form, entry.second.get(), true)) continue;
        const bool equipped = entry.second->IsWorn() || player->GetEquippedObject(false) == form ||
            player->GetEquippedObject(true) == form || player->GetCurrentAmmo() == form;
        AppendPlayerInventoryItems(form, entry.first, equipped, entry.second.get(), [&](Json&& item) {
            item["favorite"] = true;
            items.push_back(std::move(item));
        });
    }
    Json spells = Json::array();
    for (auto& spell : ReadPlayerSpellList(player)) {
        if (spell.value("favorite", false)) spells.push_back(std::move(spell));
    }
    return { { "available", true }, { "items", std::move(items) }, { "spells", std::move(spells) }, { "equipped", SpellEquippedJson(player) } };
}

std::uint32_t g_active_barter_vendor_form_id = 0;

RE::TESFaction* FindActorVendorFaction(RE::Actor* actor)
{
    if (actor == nullptr) return nullptr;
    (void)actor->GetVendorFaction();
    if (auto* fac = actor->GetActorRuntimeData().vendorFaction;
        fac != nullptr && fac->vendorData.merchantContainer != nullptr) {
        return fac;
    }
    RE::TESFaction* found = nullptr;
    actor->VisitFactions([&](RE::TESFaction* fac, std::int8_t rank) -> bool {
        if (fac == nullptr || rank < 0) return false;
        if (fac->vendorData.merchantContainer != nullptr) {
            found = fac;
            if (fac->IsVendor()) return true;
        }
        return false;
    });
    if (found != nullptr) return found;
    if (auto* base = actor->GetActorBase(); base != nullptr) {
        for (const auto& rank_entry : base->factions) {
            if (rank_entry.faction != nullptr && rank_entry.rank >= 0 &&
                rank_entry.faction->vendorData.merchantContainer != nullptr) {
                found = rank_entry.faction;
                if (found->IsVendor()) break;
            }
        }
    }
    return found;
}

RE::Actor* ResolveActiveBarterVendor()
{
    RE::NiPointer<RE::TESObjectREFR> refr;
    if (const auto handle = RE::BarterMenu::GetTargetRefHandle(); handle != 0) {
        RE::LookupReferenceByHandle(handle, refr);
        if (auto* actor = refr ? refr->As<RE::Actor>() : nullptr; actor != nullptr) {
            g_active_barter_vendor_form_id = actor->GetFormID();
            return actor;
        }
    }
    if (auto* topic_mgr = RE::MenuTopicManager::GetSingleton(); topic_mgr != nullptr) {
        if (auto spk = topic_mgr->speaker.get(); spk != nullptr) {
            if (auto* actor = spk->As<RE::Actor>(); actor != nullptr) {
                g_active_barter_vendor_form_id = actor->GetFormID();
                return actor;
            }
        }
        if (auto last_spk = topic_mgr->lastSpeaker.get(); last_spk != nullptr) {
            if (auto* actor = last_spk->As<RE::Actor>(); actor != nullptr) {
                g_active_barter_vendor_form_id = actor->GetFormID();
                return actor;
            }
        }
    }
    if (g_active_barter_vendor_form_id != 0) {
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(g_active_barter_vendor_form_id);
            actor != nullptr && !actor->IsDead() && !actor->IsDeleted()) {
            return actor;
        }
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* process_lists = RE::ProcessLists::GetSingleton();
    if (player != nullptr && process_lists != nullptr) {
        const auto ppos = player->GetPosition();
        float best_dist_sq = 1200.0F * 1200.0F;
        RE::Actor* best_vendor = nullptr;
        float best_npc_dist_sq = 450.0F * 450.0F;
        RE::Actor* best_npc = nullptr;
        process_lists->ForAllActors([&](RE::Actor* cand) {
            if (cand == nullptr || cand == player || cand->IsDead() || cand->IsDisabled() || cand->IsDeleted()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            const auto pos = cand->GetPosition();
            const float dx = pos.x - ppos.x;
            const float dy = pos.y - ppos.y;
            const float dz = pos.z - ppos.z;
            const float dist_sq = dx * dx + dy * dy + dz * dz;
            if (FindActorVendorFaction(cand) != nullptr) {
                if (dist_sq < best_dist_sq) {
                    best_dist_sq = dist_sq;
                    best_vendor = cand;
                }
            } else if (dist_sq < best_npc_dist_sq) {
                best_npc_dist_sq = dist_sq;
                best_npc = cand;
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        if (best_vendor == nullptr) {
            best_vendor = best_npc;
        }
        if (best_vendor != nullptr) {
            g_active_barter_vendor_form_id = best_vendor->GetFormID();
            return best_vendor;
        }
    }
    return nullptr;
}

void ComputeBarterMultipliers(RE::PlayerCharacter* player, float& out_buy_mult, float& out_sell_mult) noexcept
{
    float speech = 20.0F;
    if (player != nullptr) {
        if (auto* av = player->AsActorValueOwner(); av != nullptr) {
            speech = std::clamp(av->GetActorValue(RE::ActorValue::kSpeech), 0.0F, 120.0F);
        }
    }
    out_buy_mult = std::clamp(3.30F - 0.019F * speech, 1.05F, 3.30F);
    out_sell_mult = std::clamp(0.30F + 0.0045F * speech, 0.15F, 0.95F);
}

std::int32_t ComputeBarterUnitPrice(std::int32_t base_value, float mult, bool is_buy) noexcept
{
    if (base_value <= 0) {
        return is_buy ? 1 : 0;
    }
    return std::max(1, static_cast<std::int32_t>(std::lround(static_cast<double>(base_value) * mult)));
}

Json ReadBarterSnapshot(const Json& /*options*/)
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return { { "available", false }, { "vendorItems", Json::array() }, { "playerItems", Json::array() } };
    }
    auto* vendor = ResolveActiveBarterVendor();
    auto* faction = FindActorVendorFaction(vendor);
    RE::TESObjectREFR* merchant_container = (faction != nullptr && faction->vendorData.merchantContainer != nullptr)
        ? faction->vendorData.merchantContainer
        : static_cast<RE::TESObjectREFR*>(vendor);

    std::string vendor_name = "Merchant";
    if (vendor != nullptr) {
        const char* disp = vendor->GetDisplayFullName();
        if (disp == nullptr || disp[0] == '\0') {
            if (auto* base = vendor->GetActorBase(); base != nullptr) disp = base->GetName();
        }
        if (disp != nullptr && disp[0] != '\0') vendor_name = StripLeadingDollar(disp);
    }
    std::string vendor_title = "Merchant";
    if (faction != nullptr && faction->GetFullName() != nullptr && faction->GetFullName()[0] != '\0') {
        vendor_title = StripLeadingDollar(faction->GetFullName());
    }
    const char* raw_player_name = player->GetName();
    const std::string player_name = (raw_player_name != nullptr && raw_player_name[0] != '\0')
        ? StripLeadingDollar(raw_player_name)
        : "Dragonborn";

    float buy_mult = 2.0F, sell_mult = 0.5F;
    ComputeBarterMultipliers(player, buy_mult, sell_mult);

    std::int32_t vendor_gold = 0;
    std::vector<Json> vendor_items;
    if (merchant_container != nullptr) {
        for (const auto& [form, entry] : merchant_container->GetInventory()) {
            if (form == nullptr || entry.first <= 0) continue;
            if (form->GetFormID() == 0x0000000FU || form->IsGold()) {
                vendor_gold += entry.first;
                continue;
            }
            if (form->Is(RE::FormType::KeyMaster)) continue;
            if (merchant_container == vendor && entry.second && entry.second->IsWorn()) continue;
            if (!IsPlayableNamedInventoryItem(form, entry.second.get())) continue;
            auto item = PlayerItemJson(form, entry.first, false, entry.second.get(), false);
            if (!item.is_object()) continue;
            const std::int32_t base_val = item.value("value", 0);
            item["baseValue"] = base_val;
            item["value"] = ComputeBarterUnitPrice(base_val, buy_mult, true);
            vendor_items.push_back(std::move(item));
        }
    }
    if (vendor != nullptr && merchant_container != vendor) {
        vendor_gold += std::max(0, vendor->GetGoldAmount());
    }

    std::int32_t player_gold = std::max(0, player->GetGoldAmount());
    std::vector<Json> player_items;
    for (const auto& [form, entry] : player->GetInventory()) {
        if (form == nullptr || entry.first <= 0) continue;
        if (form->GetFormID() == 0x0000000FU || form->IsGold() || form->Is(RE::FormType::KeyMaster)) continue;
        if (entry.second && entry.second->IsQuestObject()) continue;
        if (!IsPlayableNamedInventoryItem(form, entry.second.get())) continue;
        const bool equipped = (entry.second && entry.second->IsWorn()) ||
            player->GetEquippedObject(false) == form ||
            player->GetEquippedObject(true) == form ||
            player->GetCurrentAmmo() == form;
        AppendPlayerInventoryItems(form, entry.first, equipped, entry.second.get(), [&](Json&& item) {
            const std::int32_t base_val = item.value("value", 0);
            item["baseValue"] = base_val;
            item["value"] = ComputeBarterUnitPrice(base_val, sell_mult, false);
            player_items.push_back(std::move(item));
        });
    }

    const auto by_name = [](const Json& a, const Json& b) {
        const auto an = LowerAscii(a.value("name", std::string{}));
        const auto bn = LowerAscii(b.value("name", std::string{}));
        return an != bn ? an < bn : a.value("id", std::string{}) < b.value("id", std::string{});
    };
    std::sort(vendor_items.begin(), vendor_items.end(), by_name);
    std::sort(player_items.begin(), player_items.end(), by_name);

    const auto collect_cats = [](const std::vector<Json>& list) -> Json {
        Json arr = Json::array();
        std::vector<std::string> seen;
        for (const auto& it : list) {
            const auto cat = it.value("category", std::string{});
            if (!cat.empty() && std::find(seen.begin(), seen.end(), cat) == seen.end()) {
                seen.push_back(cat);
                arr.push_back(cat);
            }
        }
        return arr;
    };

    const auto cap_items = [](std::vector<Json>& list) -> Json {
        Json arr = Json::array();
        for (auto& it : list) {
            if (arr.size() >= 2048U) break;
            arr.push_back(std::move(it));
        }
        return arr;
    };

    float carry_curr = 0.0F;
    float carry_max = 300.0F;
    if (auto* av = player->AsActorValueOwner(); av != nullptr) {
        carry_curr = std::max(0.0F, av->GetActorValue(RE::ActorValue::kInventoryWeight));
        carry_max = std::max(1.0F, av->GetActorValue(RE::ActorValue::kCarryWeight));
    }

    const auto vendor_total_types = static_cast<std::int32_t>(vendor_items.size());
    const auto player_total_types = static_cast<std::int32_t>(player_items.size());
    auto vendor_cats = collect_cats(vendor_items);
    auto player_cats = collect_cats(player_items);
    const std::string vendor_ref_id = vendor != nullptr ? std::to_string(vendor->GetFormID()) : std::string{};
    const std::string vendor_base_id = (vendor != nullptr && vendor->GetActorBase() != nullptr)
        ? std::to_string(vendor->GetActorBase()->GetFormID())
        : std::string{};

    return {
        { "available", vendor != nullptr },
        { "vendorName", vendor_name },
        { "vendorTitle", vendor_title },
        { "vendorRefId", vendor_ref_id },
        { "vendorReferenceId", vendor_ref_id },
        { "vendorBaseFormId", vendor_base_id },
        { "vendorGold", vendor_gold },
        { "vendorTotalTypes", vendor_total_types },
        { "vendorCategories", std::move(vendor_cats) },
        { "playerName", player_name },
        { "playerGold", player_gold },
        { "playerCarryWeight", { { "current", carry_curr }, { "max", carry_max } } },
        { "playerTotalTypes", player_total_types },
        { "playerCategories", std::move(player_cats) },
        { "vendorItems", cap_items(vendor_items) },
        { "playerItems", cap_items(player_items) }
    };
}

void NotifyNativeBarterContext(RE::Actor* vendor) noexcept
{
    if (vendor == nullptr) {
        return;
    }
    if (!REL::Module::IsVR()) {
        static REL::Relocation<RE::RefHandle*> barter_target{ RELOCATION_ID(519283, 405823) };
        *barter_target = vendor->CreateRefHandle().native_handle();
    }
    if (auto* ui = RE::UI::GetSingleton(); ui != nullptr) {
        if (auto* src = ui->GetEventSource<RE::MenuOpenCloseEvent>(); src != nullptr) {
            RE::MenuOpenCloseEvent ev{ RE::BarterMenu::MENU_NAME, false };
            src->SendEvent(&ev);
        }
    }
}

Json PerformBarterTransaction(const Json& options)
{
    const auto rejected = [](std::string message) -> Json {
        return { { "accepted", false }, { "message", std::move(message) } };
    };
    if (!options.is_object()) {
        return rejected("Barter transaction options must be an object.");
    }
    const auto mode = LowerAscii(options.value("action", options.value("mode", std::string{})));
    if (mode != "buy" && mode != "sell") {
        return rejected("Barter action must be 'buy' or 'sell'.");
    }
    bool from_stolen_stack = false;
    const std::uint32_t form_id = ParseItemFormIdField(options, "formId", &from_stolen_stack);
    const std::int32_t requested_count = std::clamp(options.value("count", 1), 1, 100000);
    if (form_id == 0) {
        return rejected("A valid item form ID is required.");
    }

    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* vendor = ResolveActiveBarterVendor();
    auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(form_id);
    auto* gold_form = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000FU);
    if (player == nullptr || vendor == nullptr || form == nullptr || gold_form == nullptr || player->IsDead()) {
        return rejected("Merchant trade is currently unavailable.");
    }
    auto* faction = FindActorVendorFaction(vendor);
    RE::TESObjectREFR* merchant_container = (faction != nullptr && faction->vendorData.merchantContainer != nullptr)
        ? faction->vendorData.merchantContainer
        : static_cast<RE::TESObjectREFR*>(vendor);

    float buy_mult = 2.0F, sell_mult = 0.5F;
    ComputeBarterMultipliers(player, buy_mult, sell_mult);
    NotifyNativeBarterContext(vendor);

    if (mode == "buy") {
        auto vendor_inv = merchant_container->GetInventory();
        auto found = vendor_inv.find(form);
        if (found == vendor_inv.end() || found->second.first <= 0) {
            return rejected("The merchant no longer has this item in stock.");
        }
        const std::int32_t avail_count = found->second.first;
        const std::int32_t tx_count = std::clamp(requested_count, 1, avail_count);
        auto* entry = found->second.second.get();
        const auto item_json = PlayerItemJson(form, tx_count, false, entry, false);
        const std::string item_name = item_json.is_object() ? item_json.value("name", std::string("Item")) : "Item";
        const std::int32_t base_val = item_json.is_object() ? item_json.value("value", 0) : std::max(0, form->GetGoldValue());
        const std::int32_t unit_price = ComputeBarterUnitPrice(base_val, buy_mult, true);
        const std::int64_t total_gold_64 = static_cast<std::int64_t>(unit_price) * tx_count;
        const std::int32_t total_gold = static_cast<std::int32_t>(std::min<std::int64_t>(total_gold_64, INT32_MAX));
        const std::int32_t player_gold = std::max(0, player->GetGoldAmount());
        if (player_gold < total_gold) {
            return rejected("You need " + std::to_string(total_gold) + " gold to buy " +
                (tx_count > 1 ? std::to_string(tx_count) + "x " : std::string{}) + item_name + ".");
        }
        RE::ExtraDataList* extra = nullptr;
        if (entry != nullptr && entry->extraLists != nullptr) {
            for (auto* cand : *entry->extraLists) {
                if (cand != nullptr) { extra = cand; break; }
            }
        }
        if (total_gold > 0) {
            player->RemoveItem(gold_form, total_gold, RE::ITEM_REMOVE_REASON::kRemove, nullptr, merchant_container);
        }
        merchant_container->RemoveItem(form, tx_count, RE::ITEM_REMOVE_REASON::kRemove, extra, player);
        player->AddPlayerAddItemEvent(form, vendor, merchant_container, RE::AQUIRE_TYPE::kBuy);
        RE::SendUIMessage::SendInventoryUpdateMessage(player, form);
        RE::SendUIMessage::SendInventoryUpdateMessage(merchant_container, form);
        player->UseSkill(RE::ActorValue::kSpeech, static_cast<float>(total_gold) * 0.45F, nullptr);
        RE::PlaySound("ITMGoldDownSD");
        skyultra::InvalidateSharedGameDataCache();
        auto updated_barter = ReadBarterSnapshot(Json::object());
        return {
            { "accepted", true },
            { "action", "buy" },
            { "mode", "buy" },
            { "formId", std::to_string(form_id) },
            { "count", tx_count },
            { "unitPrice", unit_price },
            { "totalGold", total_gold },
            { "playerGold", updated_barter.value("playerGold", 0) },
            { "vendorGold", updated_barter.value("vendorGold", 0) },
            { "message", "Purchased " + (tx_count > 1 ? std::to_string(tx_count) + "x " : std::string{}) + item_name + " for " + std::to_string(total_gold) + " gold." },
            { "barter", std::move(updated_barter) }
        };
    }

    // mode == "sell"
    auto player_inv = player->GetInventory();
    auto found = player_inv.find(form);
    if (found == player_inv.end() || found->second.first <= 0) {
        return rejected("You no longer carry this item.");
    }
    auto* entry = found->second.second.get();
    if (entry != nullptr && entry->IsQuestObject()) {
        return rejected("Quest items cannot be sold.");
    }
    const std::int32_t total_carried = found->second.first;
    const std::int32_t stolen_count = ComputePlayerStolenCount(form, total_carried, entry);
    const std::int32_t clean_count = std::max(0, total_carried - stolen_count);
    const std::int32_t avail_count = from_stolen_stack
        ? (stolen_count > 0 ? stolen_count : total_carried)
        : (clean_count > 0 ? clean_count : total_carried);
    const std::int32_t tx_count = std::clamp(requested_count, 1, avail_count);
    const auto item_json = PlayerItemJson(form, tx_count, false, entry, true, from_stolen_stack);
    const std::string item_name = item_json.is_object() ? item_json.value("name", std::string("Item")) : "Item";
    const std::int32_t base_val = item_json.is_object() ? item_json.value("value", 0) : std::max(0, form->GetGoldValue());
    const std::int32_t unit_price = ComputeBarterUnitPrice(base_val, sell_mult, false);
    const std::int64_t total_gold_64 = static_cast<std::int64_t>(unit_price) * tx_count;
    const std::int32_t total_gold = static_cast<std::int32_t>(std::min<std::int64_t>(total_gold_64, INT32_MAX));

    std::int32_t container_gold = 0;
    for (const auto& [vf, ve] : merchant_container->GetInventory()) {
        if (vf != nullptr && ve.first > 0 && (vf->GetFormID() == 0x0000000FU || vf->IsGold())) {
            container_gold += ve.first;
        }
    }
    const std::int32_t actor_gold = (merchant_container != vendor) ? std::max(0, vendor->GetGoldAmount()) : 0;
    const std::int32_t vendor_gold = container_gold + actor_gold;
    const bool allow_partial_gold = options.value("allowPartialGold", false);
    if (vendor_gold < total_gold && !allow_partial_gold) {
        return {
            { "accepted", false },
            { "requiresConfirmation", true },
            { "action", "sell" },
            { "mode", "sell" },
            { "formId", from_stolen_stack ? (std::to_string(form_id) + ":stolen") : std::to_string(form_id) },
            { "count", tx_count },
            { "unitPrice", unit_price },
            { "totalGold", total_gold },
            { "availableVendorGold", vendor_gold },
            { "vendorGold", vendor_gold },
            { "message", "The merchant only has " + std::to_string(vendor_gold) + " gold available (worth " + std::to_string(total_gold) + " gold). Confirm to sell anyway." }
        };
    }
    const std::int32_t payout_gold = std::min(total_gold, vendor_gold);

    RE::ExtraDataList* extra = nullptr;
    if (entry != nullptr && entry->extraLists != nullptr) {
        for (auto* cand : *entry->extraLists) {
            if (cand != nullptr) { extra = cand; break; }
        }
    }
    if (total_carried - tx_count <= 0) {
        if (auto* eq_mgr = RE::ActorEquipManager::GetSingleton(); eq_mgr != nullptr) {
            const bool is_worn = (entry != nullptr && entry->IsWorn()) ||
                player->GetEquippedObject(false) == form ||
                player->GetEquippedObject(true) == form ||
                player->GetCurrentAmmo() == form;
            if (is_worn) {
                (void)eq_mgr->UnequipObject(player, form, extra, 1, nullptr, false, false, true, false, nullptr);
            }
        }
    }
    player->RemoveItem(form, tx_count, RE::ITEM_REMOVE_REASON::kSelling, extra, merchant_container);
    ConsumePlayerStolenCount(form, entry, total_carried, tx_count, std::max(0, total_carried - tx_count), from_stolen_stack || clean_count == 0);
    if (payout_gold > 0) {
        const std::int32_t from_container = std::min(payout_gold, container_gold);
        if (from_container > 0) {
            merchant_container->RemoveItem(gold_form, from_container, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player);
        }
        const std::int32_t rem_gold = payout_gold - from_container;
        if (rem_gold > 0 && merchant_container != vendor) {
            vendor->RemoveItem(gold_form, rem_gold, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player);
        }
    }
    RE::SendUIMessage::SendInventoryUpdateMessage(player, form);
    RE::SendUIMessage::SendInventoryUpdateMessage(merchant_container, form);
    player->UseSkill(RE::ActorValue::kSpeech, static_cast<float>(total_gold) * 0.45F, nullptr);
    RE::PlaySound("ITMGoldUpSD");
    skyultra::InvalidateSharedGameDataCache();
    auto updated_barter = ReadBarterSnapshot(Json::object());
    return {
        { "accepted", true },
        { "action", "sell" },
        { "mode", "sell" },
        { "formId", std::to_string(form_id) },
        { "count", tx_count },
        { "unitPrice", unit_price },
        { "totalGold", payout_gold },
        { "playerGold", updated_barter.value("playerGold", 0) },
        { "vendorGold", updated_barter.value("vendorGold", 0) },
        { "message", "Sold " + (tx_count > 1 ? std::to_string(tx_count) + "x " : std::string{}) + item_name + " for " + std::to_string(payout_gold) + " gold." },
        { "barter", std::move(updated_barter) }
    };
}

std::uint32_t g_active_container_ref_id = 0;
RE::RefHandle g_active_container_handle = 0;
RE::ContainerMenu::ContainerMode g_active_container_mode = RE::ContainerMenu::ContainerMode::kLoot;
bool g_active_container_looted_notified = false;

RE::TESObjectREFR* LookupContainerRefById(std::uint32_t form_id) noexcept
{
    if (form_id == 0) {
        return nullptr;
    }
    auto* form = RE::TESForm::LookupByID(form_id);
    if (form == nullptr) {
        return nullptr;
    }
    if (auto* ref = form->As<RE::TESObjectREFR>(); ref != nullptr && !ref->IsDeleted()) {
        return ref;
    }
    if (auto* actor = form->As<RE::Actor>(); actor != nullptr && !actor->IsDeleted()) {
        return actor;
    }
    return nullptr;
}

RE::ContainerMenu::ContainerMode ActiveContainerMode() noexcept
{
    if (RE::ContainerMenu::GetTargetRefHandle() != 0) {
        g_active_container_mode = RE::ContainerMenu::GetContainerMode();
    }
    return g_active_container_mode;
}

RE::TESObjectREFR* ResolveActiveContainerRef()
{
    const auto handle = RE::ContainerMenu::GetTargetRefHandle();
    if (handle != 0) {
        g_active_container_handle = handle;
        g_active_container_mode = RE::ContainerMenu::GetContainerMode();
        if (auto ref_ptr = RE::TESObjectREFR::LookupByHandle(handle); ref_ptr != nullptr) {
            g_active_container_ref_id = ref_ptr->GetFormID();
            return ref_ptr.get();
        }
    }
    if (g_active_container_ref_id != 0) {
        if (auto* ref = LookupContainerRefById(g_active_container_ref_id); ref != nullptr) {
            return ref;
        }
    }
    if (g_active_container_handle != 0) {
        if (auto ref_ptr = RE::TESObjectREFR::LookupByHandle(g_active_container_handle); ref_ptr != nullptr && !ref_ptr->IsDeleted()) {
            g_active_container_ref_id = ref_ptr->GetFormID();
            return ref_ptr.get();
        }
    }
    return nullptr;
}

std::string_view ContainerModeLabel(RE::ContainerMenu::ContainerMode mode) noexcept
{
    switch (mode) {
    case RE::ContainerMenu::ContainerMode::kSteal: return "steal";
    case RE::ContainerMenu::ContainerMode::kPickpocket: return "pickpocket";
    case RE::ContainerMenu::ContainerMode::kNPCMode: return "npc";
    case RE::ContainerMenu::ContainerMode::kLoot:
    default: return "loot";
    }
}

Json ReadContainerSnapshot(const Json& options)
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return { { "available", false }, { "message", "The player is unavailable." } };
    }

    RE::TESObjectREFR* container = nullptr;
    if (options.is_object()) {
        if (const auto field = options.find("containerRefId"); field != options.end()) {
            std::uint32_t ref_id = 0;
            if (field->is_number_integer()) ref_id = static_cast<std::uint32_t>(field->get<std::int64_t>());
            else if (field->is_string()) {
                const auto text = field->get<std::string>();
                const bool hex = text.starts_with("0x") || text.starts_with("0X");
                (void)std::from_chars(text.data() + (hex ? 2 : 0), text.data() + text.size(), ref_id, hex ? 16 : 10);
            }
            if (ref_id != 0) {
                container = LookupContainerRefById(ref_id);
                if (container != nullptr) g_active_container_ref_id = ref_id;
            }
        }
    }
    if (container == nullptr) {
        container = ResolveActiveContainerRef();
    }
    if (container == nullptr) {
        return { { "available", false }, { "message", "No active container or mannequin is currently open." } };
    }

    const bool is_mannequin = IsMannequinReference(container);
    auto* actor = container->As<RE::Actor>();
    const auto* base = container->GetBaseObject();
    std::string container_name = container->GetDisplayFullName() != nullptr ? StripLeadingDollar(container->GetDisplayFullName()) : std::string{};
    if (container_name.empty() && base != nullptr && base->GetName() != nullptr) {
        container_name = StripLeadingDollar(base->GetName());
    }
    if (container_name.empty()) {
        container_name = is_mannequin ? "Armor Mannequin" : "Storage Container";
    }

    const std::string query = options.is_object() ? LowerAscii(options.value("query", std::string{})) : std::string{};
    const std::string category = options.is_object() ? options.value("category", std::string("all")) : std::string("all");
    const std::string sort = options.is_object() ? options.value("sort", std::string("name")) : std::string("name");

    const auto matches_filter = [&](const Json& item, std::string_view kind, std::uint32_t form_id) -> bool {
        if (category != "all" && category != kind) return false;
        if (query.empty()) return true;
        char hex[11]{};
        std::snprintf(hex, sizeof(hex), "0x%08X", form_id);
        const auto hay = LowerAscii(item.value("name", std::string{}) + " " + std::string(kind) + " " +
                                    item.value("id", std::string{}) + " " + hex);
        return hay.find(query) != std::string::npos;
    };

    const auto sort_items = [&](std::vector<Json>& list) {
        std::sort(list.begin(), list.end(), [&](const Json& a, const Json& b) {
            if (sort == "vw") {
                const double av = ItemValuePerWeight(a);
                const double bv = ItemValuePerWeight(b);
                if (std::abs(av - bv) > 1e-6) return av > bv;
            } else if (sort == "value" || sort == "weight" || sort == "count") {
                const double av = a.value(sort, 0.0);
                const double bv = b.value(sort, 0.0);
                if (av != bv) return av > bv;
            }
            const auto an = LowerAscii(a.value("name", std::string{}));
            const auto bn = LowerAscii(b.value("name", std::string{}));
            return an != bn ? an < bn : a.value("id", std::string{}) < b.value("id", std::string{});
        });
    };

    const auto active_mode = ActiveContainerMode();
    const bool is_corpse = actor != nullptr && !is_mannequin && actor->IsDead();
    const bool is_pickpocket = !is_mannequin && !is_corpse && actor != nullptr &&
        (active_mode == RE::ContainerMenu::ContainerMode::kPickpocket || skyultra::IsActiveContainerPickpocket());
    const bool container_steal_owned = !is_mannequin && !is_corpse &&
        (is_pickpocket || active_mode == RE::ContainerMenu::ContainerMode::kSteal);
    const std::string variant = is_mannequin ? "mannequin" :
        (is_corpse ? "loot" : (is_pickpocket ? "pickpocket" : "container"));
    const float thief_pickpocket_skill = player->AsActorValueOwner() != nullptr
        ? std::max(0.0F, player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kPickpocket))
        : 15.0F;
    const float target_pickpocket_skill = (actor != nullptr && actor->AsActorValueOwner() != nullptr)
        ? std::max(0.0F, actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kPickpocket))
        : 15.0F;
    const bool target_detected = is_pickpocket && actor != nullptr && actor->RequestDetectionLevel(player) > 0;
    const auto attach_container_item_meta = [&](Json& item, RE::TESForm* item_form) {
        if (!item.is_object()) return;
        const bool is_gold = item_form != nullptr && (item_form->GetFormID() == 0x0000000FU || item_form->IsGold());
        if (container_steal_owned && !is_gold) {
            item["isOwned"] = true;
            item["stolen"] = true;
        }
        if (is_pickpocket && actor != nullptr && item_form != nullptr) {
            const auto raw_val = item.value("value", 0);
            const auto value_u32 = static_cast<std::uint32_t>(std::max(0, raw_val));
            const float wt = std::max(0.0F, item.value("weight", 0.0F));
            const auto chance = RE::AIFormulas::ComputePickpocketSuccess(
                thief_pickpocket_skill, target_pickpocket_skill, value_u32, wt,
                player, actor, target_detected, item_form);
            item["pickpocketChance"] = std::clamp(chance, 0, 100);
        }
    };

    std::vector<Json> container_items_vec;
    std::unordered_set<std::string> container_categories_set;
    std::size_t container_total_types = 0;
    bool has_container_gold = false;

    for (const auto& [form, entry] : container->GetInventory()) {
        if (form == nullptr || entry.first <= 0) continue;
        if (form->GetFormID() == 0x0000000FU || form->IsGold()) {
            has_container_gold = true;
        }
        const bool worn = (entry.second && entry.second->IsWorn()) ||
            (actor != nullptr && (actor->GetEquippedObject(false) == form || actor->GetEquippedObject(true) == form || actor->GetCurrentAmmo() == form));
        if (!IsPlayableNamedInventoryItem(form, entry.second.get(), worn)) continue;
        ++container_total_types;
        const auto kind = std::string(ItemCategory(form->GetFormType()));
        container_categories_set.insert(kind);
        auto item = PlayerItemJson(form, entry.first, worn, entry.second.get(), false);
        if (!item.is_object()) continue;
        attach_container_item_meta(item, form);
        if (!matches_filter(item, kind, form->GetFormID())) continue;
        container_items_vec.push_back(std::move(item));
    }
    if (!is_mannequin && !has_container_gold && actor != nullptr) {
        const std::int32_t actor_gold = actor->GetGoldAmount();
        if (actor_gold > 0) {
            if (auto* gold_form = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000FU); gold_form != nullptr) {
                ++container_total_types;
                const auto kind = std::string(ItemCategory(gold_form->GetFormType()));
                container_categories_set.insert(kind);
                auto gold_item = PlayerItemJson(gold_form, actor_gold, false, nullptr, false);
                attach_container_item_meta(gold_item, gold_form);
                if (gold_item.is_object() && matches_filter(gold_item, kind, 0x0000000FU)) {
                    container_items_vec.push_back(std::move(gold_item));
                }
            }
        }
    }
    sort_items(container_items_vec);

    Json container_items = Json::array();
    for (auto& item : container_items_vec) {
        if (container_items.size() >= 2048) break;
        container_items.push_back(std::move(item));
    }
    std::vector<std::string> container_categories(container_categories_set.begin(), container_categories_set.end());
    std::sort(container_categories.begin(), container_categories.end());

    std::vector<Json> player_items_vec;
    std::unordered_set<std::string> player_categories_set;
    std::size_t player_total_types = 0;
    bool has_player_gold = false;
    for (const auto& [form, entry] : player->GetInventory()) {
        if (form == nullptr || entry.first <= 0) continue;
        if (form->GetFormID() == 0x0000000FU || form->IsGold()) {
            if (is_mannequin) continue;
            has_player_gold = true;
        }
        const bool equipped = (entry.second && entry.second->IsWorn()) ||
            player->GetEquippedObject(false) == form ||
            player->GetEquippedObject(true) == form ||
            player->GetCurrentAmmo() == form;
        if (!IsPlayableNamedInventoryItem(form, entry.second.get(), equipped)) continue;
        const auto kind = std::string(ItemCategory(form->GetFormType()));
        player_categories_set.insert(kind);
        AppendPlayerInventoryItems(form, entry.first, equipped, entry.second.get(), [&](Json&& item) {
            ++player_total_types;
            if (!matches_filter(item, kind, form->GetFormID())) return;
            player_items_vec.push_back(std::move(item));
        });
    }
    if (!is_mannequin && !has_player_gold) {
        const std::int32_t player_gold = player->GetGoldAmount();
        if (player_gold > 0) {
            if (auto* gold_form = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000FU); gold_form != nullptr) {
                ++player_total_types;
                const auto kind = std::string(ItemCategory(gold_form->GetFormType()));
                player_categories_set.insert(kind);
                auto gold_item = PlayerItemJson(gold_form, player_gold, false, nullptr);
                if (gold_item.is_object() && matches_filter(gold_item, kind, 0x0000000FU)) {
                    player_items_vec.push_back(std::move(gold_item));
                }
            }
        }
    }
    sort_items(player_items_vec);

    Json player_items = Json::array();
    for (auto& item : player_items_vec) {
        if (player_items.size() >= 2048) break;
        player_items.push_back(std::move(item));
    }
    std::vector<std::string> player_categories(player_categories_set.begin(), player_categories_set.end());
    std::sort(player_categories.begin(), player_categories.end());

    float carry_current = player->GetWeightInContainer();
    if (!std::isfinite(carry_current) || carry_current < 0.0F) carry_current = 0.0F;
    float carry_max = 300.0F;
    if (auto* av = player->AsActorValueOwner(); av != nullptr) {
        carry_max = av->GetActorValue(RE::ActorValue::kCarryWeight);
        if (!std::isfinite(carry_max) || carry_max < 0.0F) carry_max = 300.0F;
    }

    const std::string container_type = is_mannequin ? "mannequin" :
        (actor != nullptr ? (actor->IsDead() ? "corpse" : "actor") : "container");

    Json equipment = nullptr;
    float armor_rating = 0.0F;
    Json actor_record = nullptr;
    if (actor != nullptr) {
        equipment = ReadActorEquipment(actor, true);
        if (const auto* av = actor->AsActorValueOwner(); av != nullptr) {
            armor_rating = std::max(0.0F, av->GetActorValue(RE::ActorValue::kDamageResist));
        }
        if (armor_rating <= 0.0F && equipment.is_object()) {
            for (const auto& [slot_key, slot_val] : equipment.items()) {
                if (slot_val.is_object() && slot_val.contains("armorRating") && slot_val["armorRating"].is_number()) {
                    armor_rating += slot_val["armorRating"].get<float>();
                }
            }
        }
        auto* npc_base = actor->GetActorBase();
        actor_record = {
            { "referenceId", std::to_string(actor->GetFormID()) },
            { "baseFormId", std::to_string(npc_base != nullptr ? npc_base->GetFormID() : actor->GetFormID()) },
            { "name", container_name },
            { "race", npc_base != nullptr ? NpcRaceName(npc_base) : std::string("Mannequin") },
            { "sex", npc_base != nullptr ? NpcSexName(npc_base->GetSex()) : std::string("None") },
            { "level", actor->GetLevel() },
            { "follower", actor->IsPlayerTeammate() },
            { "dead", actor->IsDead() },
            { "health", 100.0F },
            { "healthMax", 100.0F },
            { "armorRating", armor_rating },
            { "equipment", equipment }
        };
        AppendActorDossierFields(actor, npc_base, player, actor_record);
    }

    return {
        { "available", true },
        { "isMannequin", is_mannequin },
        { "containerRefId", std::to_string(container->GetFormID()) },
        { "containerBaseFormId", std::to_string(base != nullptr ? base->GetFormID() : container->GetFormID()) },
        { "containerName", container_name },
        { "containerType", container_type },
        { "variant", variant },
        { "containerMode", ContainerModeLabel(active_mode) },
        { "containerItems", std::move(container_items) },
        { "containerTotalTypes", container_total_types },
        { "containerCategories", std::move(container_categories) },
        { "playerName", player->GetName() != nullptr ? player->GetName() : "Dragonborn" },
        { "playerGold", std::max(0, player->GetGoldAmount()) },
        { "playerCarryWeight", { { "current", carry_current }, { "max", carry_max } } },
        { "playerItems", std::move(player_items) },
        { "playerTotalTypes", player_total_types },
        { "playerCategories", std::move(player_categories) },
        { "equipment", std::move(equipment) },
        { "armorRating", armor_rating },
        { "actor", std::move(actor_record) }
    };
}

Json PerformContainerTransfer(const Json& options)
{
    const auto rejected = [](std::string message) -> Json { return { { "accepted", false }, { "message", std::move(message) } }; };
    if (!options.is_object()) return rejected("Container transfer options must be an object.");

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr || player->IsDead()) return rejected("The player is unavailable.");

    RE::TESObjectREFR* container = nullptr;
    if (const auto field = options.find("containerRefId"); field != options.end()) {
        std::uint32_t ref_id = 0;
        if (field->is_number_integer()) ref_id = static_cast<std::uint32_t>(field->get<std::int64_t>());
        else if (field->is_string()) {
            const auto text = field->get<std::string>();
            const bool hex = text.starts_with("0x") || text.starts_with("0X");
            (void)std::from_chars(text.data() + (hex ? 2 : 0), text.data() + text.size(), ref_id, hex ? 16 : 10);
        }
        if (ref_id != 0) {
            container = LookupContainerRefById(ref_id);
            if (container != nullptr) g_active_container_ref_id = ref_id;
        }
    }
    if (container == nullptr) container = ResolveActiveContainerRef();
    if (container == nullptr) return rejected("No active container or mannequin is currently open.");

    const auto mode = LowerAscii(options.value("mode", std::string("take")));
    const bool is_mannequin = IsMannequinReference(container);
    auto* container_actor = container->As<RE::Actor>();
    const bool is_corpse = container_actor != nullptr && !is_mannequin && container_actor->IsDead();
    const auto active_mode = ActiveContainerMode();
    const bool is_pickpocket =
        mode == "steal" || mode == "stealall" || mode == "steal_all" ||
        mode == "plant" || mode == "plantall" || mode == "plant_all" ||
        (container_actor != nullptr && !is_mannequin && !container_actor->IsDead() &&
         (active_mode == RE::ContainerMenu::ContainerMode::kPickpocket ||
          active_mode == RE::ContainerMenu::ContainerMode::kSteal ||
          player->IsSneaking() || !container_actor->IsPlayerTeammate()));
    const bool live_pickpocket = is_pickpocket && container_actor != nullptr && !is_mannequin && !container_actor->IsDead();
    const auto take_reason = (is_pickpocket || active_mode == RE::ContainerMenu::ContainerMode::kSteal)
        ? RE::ITEM_REMOVE_REASON::kSteal
        : RE::ITEM_REMOVE_REASON::kRemove;
    const RE::AQUIRE_TYPE acquire_type = live_pickpocket
        ? RE::AQUIRE_TYPE::kPickPocket
        : ((is_pickpocket || active_mode == RE::ContainerMenu::ContainerMode::kSteal)
            ? RE::AQUIRE_TYPE::kSteal
            : (is_corpse ? RE::AQUIRE_TYPE::kDeadBody : RE::AQUIRE_TYPE::kContainer));
    auto* container_owner = container->GetOwner();
    const Json snapshot_opts{ { "containerRefId", std::to_string(container->GetFormID()) } };

    const auto count_in_ref = [](RE::TESObjectREFR* ref, RE::TESBoundObject* obj) -> std::int32_t {
        if (ref == nullptr || obj == nullptr) return 0;
        auto inv = ref->GetInventory();
        const auto it = inv.find(obj);
        if (it != inv.end() && it->second.first > 0) return it->second.first;
        if (obj->GetFormID() == 0x0000000FU || obj->IsGold()) {
            if (auto* act = ref->As<RE::Actor>(); act != nullptr) return std::max(0, act->GetGoldAmount());
        }
        return 0;
    };

    if (mode == "takeall" || mode == "take_all" || mode == "stealall" || mode == "steal_all") {
        std::int32_t moved_types = 0;
        std::int32_t moved_total = 0;
        bool moved_gold = false;
        bool caught = false;
        auto inv = container->GetInventory();
        auto player_inv = player->GetInventory();
        for (const auto& [form, entry] : inv) {
            if (form == nullptr || entry.first <= 0) continue;
            if (form->GetFormID() == 0x0000000FU || form->IsGold()) {
                moved_gold = true;
            }
            const bool worn = (entry.second && entry.second->IsWorn()) ||
                (container_actor != nullptr && (container_actor->GetEquippedObject(false) == form ||
                                                container_actor->GetEquippedObject(true) == form ||
                                                container_actor->GetCurrentAmmo() == form));
            if (!IsPlayableNamedInventoryItem(form, entry.second.get(), worn)) continue;
            if (live_pickpocket) {
                RE::InventoryEntryData fallback_entry(form, entry.first);
                RE::InventoryEntryData* entry_ptr = entry.second != nullptr ? entry.second.get() : &fallback_entry;
                if (!player->AttemptPickpocket(container, entry_ptr, entry.first, true)) {
                    caught = true;
                    break;
                }
            }
            if (worn && container_actor != nullptr) {
                if (auto* eq_mgr = RE::ActorEquipManager::GetSingleton(); eq_mgr != nullptr) {
                    (void)eq_mgr->UnequipObject(container_actor, form, nullptr, entry.first, nullptr, false, false, true, false, nullptr);
                }
            }
            const auto pit = player_inv.find(form);
            const std::int32_t before_player = (pit != player_inv.end() && pit->second.first > 0) ? pit->second.first : 0;
            const std::int32_t prior_stolen = ComputePlayerStolenCount(form, before_player, pit != player_inv.end() ? pit->second.second.get() : nullptr);
            const std::int32_t before_container = count_in_ref(container, form);
            container->RemoveItem(form, entry.first, take_reason, nullptr, player);
            const std::int32_t after_container = count_in_ref(container, form);
            const std::int32_t still_left = std::max(0, after_container - std::max(0, before_container - entry.first));
            if (still_left > 0 && take_reason != RE::ITEM_REMOVE_REASON::kRemove) {
                container->RemoveItem(form, still_left, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player);
            }
            if ((is_pickpocket || active_mode == RE::ContainerMenu::ContainerMode::kSteal) &&
                form->GetFormID() != 0x0000000FU && !form->IsGold()) {
                const std::int32_t after_player = std::max(count_in_ref(player, form), before_player + entry.first);
                RecordPlayerStolenCount(form->GetFormID(), prior_stolen, entry.first, after_player);
            }
            player->AddPlayerAddItemEvent(form, container_owner, container, acquire_type);
            RE::SendUIMessage::SendInventoryUpdateMessage(player, form);
            RE::SendUIMessage::SendInventoryUpdateMessage(container, form);
            ++moved_types;
            moved_total += entry.first;
        }
        if (!caught && !is_mannequin && !moved_gold && container_actor != nullptr) {
            const std::int32_t actor_gold = container_actor->GetGoldAmount();
            if (actor_gold > 0) {
                if (auto* gold_form = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000FU); gold_form != nullptr) {
                    if (live_pickpocket) {
                        RE::InventoryEntryData gold_entry(gold_form, actor_gold);
                        if (!player->AttemptPickpocket(container, &gold_entry, actor_gold, true)) {
                            caught = true;
                        }
                    }
                    if (!caught) {
                        const std::int32_t player_gold_before = player->GetGoldAmount();
                        container->RemoveItem(gold_form, actor_gold, take_reason, nullptr, player);
                        const std::int32_t gained = std::max(0, player->GetGoldAmount() - player_gold_before);
                        if (gained < actor_gold && take_reason != RE::ITEM_REMOVE_REASON::kRemove) {
                            container->RemoveItem(gold_form, actor_gold - gained, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player);
                        }
                        player->AddPlayerAddItemEvent(gold_form, container_owner, container, acquire_type);
                        RE::SendUIMessage::SendInventoryUpdateMessage(player, gold_form);
                        RE::SendUIMessage::SendInventoryUpdateMessage(container, gold_form);
                        ++moved_types;
                        moved_total += actor_gold;
                    }
                }
            }
        }
        if (moved_total > 0) {
            if (live_pickpocket) {
                RE::ItemsPickpocketed::SendEvent(moved_total);
            } else if (!is_mannequin && container_actor == nullptr && !is_corpse && !g_active_container_looted_notified) {
                g_active_container_looted_notified = true;
                RE::ChestsLooted::SendEvent();
            }
        }
        if (container_actor != nullptr) container_actor->Update3DModel();
        RE::PlaySound(caught ? "UIMenuCancel" : "ITMGenericUpSD");
        skyultra::InvalidateSharedGameDataCache();
        auto updated = ReadContainerSnapshot(snapshot_opts);
        if (caught) {
            return {
                { "accepted", moved_types > 0 },
                { "caught", true },
                { "closeMenu", true },
                { "mode", "stealAll" },
                { "movedTypes", moved_types },
                { "count", moved_total },
                { "message", moved_types > 0
                    ? ("Stole " + std::to_string(moved_total) + " item(s) before getting caught!")
                    : std::string("You have been caught pickpocketing!") },
                { "container", std::move(updated) }
            };
        }
        const std::string verb = is_pickpocket ? "Stole all items" : (is_corpse ? "Looted all items" : "Took all items");
        return {
            { "accepted", true },
            { "mode", is_pickpocket ? "stealAll" : "takeAll" },
            { "movedTypes", moved_types },
            { "count", moved_total },
            { "message", moved_types > 0 ? (verb + " (" + std::to_string(moved_total) + " total).") : std::string("The container is already empty.") },
            { "container", std::move(updated) }
        };
    }

    if (mode == "storeall" || mode == "store_all" || mode == "plantall" || mode == "plant_all") {
        std::int32_t moved_types = 0;
        std::int32_t moved_total = 0;
        bool caught = false;
        const bool include_equipped = options.value("includeEquipped", false);
        const std::string category_filter = options.value("category", std::string("all"));
        auto inv = player->GetInventory();
        for (const auto& [form, entry] : inv) {
            if (form == nullptr || entry.first <= 0) continue;
            if (form->GetFormID() == 0x0000000FU || form->IsGold()) continue;
            if (form->GetFormType() == RE::FormType::KeyMaster) continue;
            const bool equipped = (entry.second && entry.second->IsWorn()) ||
                player->GetEquippedObject(false) == form ||
                player->GetEquippedObject(true) == form ||
                player->GetCurrentAmmo() == form;
            if (equipped && !include_equipped) continue;
            if (!IsPlayableNamedInventoryItem(form, entry.second.get(), equipped)) continue;
            if (entry.second && entry.second->IsQuestObject()) continue;
            if (category_filter != "all" && ItemCategory(form->GetFormType()) != category_filter) continue;
            if (live_pickpocket) {
                RE::InventoryEntryData fallback_entry(form, entry.first);
                RE::InventoryEntryData* entry_ptr = entry.second != nullptr ? entry.second.get() : &fallback_entry;
                if (!player->AttemptPickpocket(container, entry_ptr, entry.first, false)) {
                    caught = true;
                    break;
                }
            }
            player->RemoveItem(form, entry.first, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, container);
            g_stolen_player_counts.erase(form->GetFormID());
            RE::SendUIMessage::SendInventoryUpdateMessage(player, form);
            RE::SendUIMessage::SendInventoryUpdateMessage(container, form);
            ++moved_types;
            moved_total += entry.first;
        }
        if (container_actor != nullptr) container_actor->Update3DModel();
        RE::PlaySound(caught ? "UIMenuCancel" : "ITMGenericDownSD");
        skyultra::InvalidateSharedGameDataCache();
        auto updated = ReadContainerSnapshot(snapshot_opts);
        if (caught) {
            return {
                { "accepted", moved_types > 0 },
                { "caught", true },
                { "closeMenu", true },
                { "mode", "plantAll" },
                { "movedTypes", moved_types },
                { "count", moved_total },
                { "message", moved_types > 0
                    ? ("Planted " + std::to_string(moved_total) + " item(s) before getting caught!")
                    : std::string("You were caught planting items!") },
                { "container", std::move(updated) }
            };
        }
        const std::string verb = is_pickpocket ? "Planted " : "Stored ";
        return {
            { "accepted", true },
            { "mode", is_pickpocket ? "plantAll" : "storeAll" },
            { "movedTypes", moved_types },
            { "count", moved_total },
            { "message", moved_types > 0 ? (verb + std::to_string(moved_total) + " items (" + std::to_string(moved_types) + " types).") : std::string(is_pickpocket ? "No unequipped items available to plant." : "No unequipped items available to store.") },
            { "container", std::move(updated) }
        };
    }

    bool from_stolen_stack = false;
    const std::uint32_t form_id = ParseItemFormIdField(options, "formId", &from_stolen_stack);
    auto* form = form_id != 0 ? RE::TESForm::LookupByID<RE::TESBoundObject>(form_id) : nullptr;
    if (form == nullptr) return rejected("That item is unavailable.");
    const std::string item_name = form->GetName() != nullptr ? StripLeadingDollar(form->GetName()) : std::string("Item");
    const std::int32_t requested_count = std::max(1, options.value("count", 1));

    if (mode == "take" || mode == "steal") {
        auto inv = container->GetInventory();
        const auto it = inv.find(form);
        std::int32_t available_count = (it != inv.end()) ? it->second.first : 0;
        if (available_count <= 0 && (form->GetFormID() == 0x0000000FU || form->IsGold()) && container_actor != nullptr) {
            available_count = container_actor->GetGoldAmount();
        }
        if (available_count <= 0) {
            return rejected(item_name + " is no longer in the container.");
        }
        const std::int32_t tx_count = std::clamp(requested_count, 1, available_count);
        if (live_pickpocket) {
            RE::InventoryEntryData fallback_entry(form, tx_count);
            RE::InventoryEntryData* entry_ptr = (it != inv.end() && it->second.second != nullptr)
                ? it->second.second.get()
                : &fallback_entry;
            if (!player->AttemptPickpocket(container, entry_ptr, tx_count, true)) {
                RE::PlaySound("UIMenuCancel");
                skyultra::InvalidateSharedGameDataCache();
                return {
                    { "accepted", false },
                    { "caught", true },
                    { "closeMenu", true },
                    { "mode", "steal" },
                    { "formId", std::to_string(form_id) },
                    { "message", "You have been caught pickpocketing!" }
                };
            }
        }
        bool worn = false;
        if (container_actor != nullptr) {
            worn = (it != inv.end() && it->second.second && it->second.second->IsWorn()) ||
                container_actor->GetEquippedObject(false) == form ||
                container_actor->GetEquippedObject(true) == form ||
                container_actor->GetCurrentAmmo() == form;
            if (worn && available_count - tx_count <= 0) {
                if (auto* eq_mgr = RE::ActorEquipManager::GetSingleton(); eq_mgr != nullptr) {
                    (void)eq_mgr->UnequipObject(container_actor, form, nullptr, tx_count, nullptr, false, false, true, false, nullptr);
                }
            }
        }
        auto player_inv_before = player->GetInventory();
        const auto pit_before = player_inv_before.find(form);
        const std::int32_t before_container = count_in_ref(container, form);
        const std::int32_t before_player = count_in_ref(player, form);
        const std::int32_t prior_stolen = ComputePlayerStolenCount(
            form,
            before_player,
            pit_before != player_inv_before.end() ? pit_before->second.second.get() : nullptr);
        container->RemoveItem(form, tx_count, take_reason, nullptr, player);
        const std::int32_t moved_so_far = std::max(
            std::max(0, before_container - count_in_ref(container, form)),
            std::max(0, count_in_ref(player, form) - before_player));
        if (moved_so_far < tx_count && take_reason != RE::ITEM_REMOVE_REASON::kRemove) {
            container->RemoveItem(form, tx_count - moved_so_far, RE::ITEM_REMOVE_REASON::kRemove, nullptr, player);
        }
        if ((is_pickpocket || active_mode == RE::ContainerMenu::ContainerMode::kSteal) &&
            form->GetFormID() != 0x0000000FU && !form->IsGold()) {
            const std::int32_t after_player = std::max(count_in_ref(player, form), before_player + tx_count);
            RecordPlayerStolenCount(form->GetFormID(), prior_stolen, tx_count, after_player);
        }
        player->AddPlayerAddItemEvent(form, container_owner, container, acquire_type);
        RE::SendUIMessage::SendInventoryUpdateMessage(player, form);
        RE::SendUIMessage::SendInventoryUpdateMessage(container, form);
        if (live_pickpocket) {
            RE::ItemsPickpocketed::SendEvent(tx_count);
        } else if (!is_mannequin && container_actor == nullptr && !is_corpse && !g_active_container_looted_notified) {
            g_active_container_looted_notified = true;
            RE::ChestsLooted::SendEvent();
        }
        if (container_actor != nullptr) container_actor->Update3DModel();
        RE::PlaySound((form->GetFormID() == 0x0000000FU || form->IsGold()) ? "ITMGoldUpSD" : "ITMGenericUpSD");
        skyultra::InvalidateSharedGameDataCache();
        auto updated = ReadContainerSnapshot(snapshot_opts);
        const std::string verb = is_pickpocket ? "Stole " : (is_corpse ? "Looted " : "Took ");
        return {
            { "accepted", true },
            { "mode", is_pickpocket ? "steal" : "take" },
            { "formId", std::to_string(form_id) },
            { "count", tx_count },
            { "message", verb + (tx_count > 1 ? std::to_string(tx_count) + "x " : std::string{}) + item_name + "." },
            { "container", std::move(updated) }
        };
    }

    if (mode == "store" || mode == "plant") {
        auto inv = player->GetInventory();
        const auto it = inv.find(form);
        std::int32_t total_carried = (it != inv.end()) ? it->second.first : 0;
        if (total_carried <= 0 && (form->GetFormID() == 0x0000000FU || form->IsGold())) {
            total_carried = player->GetGoldAmount();
        }
        if (total_carried <= 0) {
            return rejected("You do not have " + item_name + " in your inventory.");
        }
        if (it != inv.end() && it->second.second && it->second.second->IsQuestObject()) {
            return rejected("Quest items cannot be stored in containers.");
        }
        auto* player_entry = (it != inv.end()) ? it->second.second.get() : nullptr;
        const std::int32_t stolen_count = ComputePlayerStolenCount(form, total_carried, player_entry);
        const std::int32_t clean_count = std::max(0, total_carried - stolen_count);
        const std::int32_t available_count = from_stolen_stack
            ? (stolen_count > 0 ? stolen_count : total_carried)
            : (clean_count > 0 ? clean_count : total_carried);
        const std::int32_t tx_count = std::clamp(requested_count, 1, available_count);
        if (live_pickpocket) {
            RE::InventoryEntryData fallback_entry(form, tx_count);
            RE::InventoryEntryData* entry_ptr = player_entry != nullptr ? player_entry : &fallback_entry;
            if (!player->AttemptPickpocket(container, entry_ptr, tx_count, false)) {
                RE::PlaySound("UIMenuCancel");
                skyultra::InvalidateSharedGameDataCache();
                return {
                    { "accepted", false },
                    { "caught", true },
                    { "closeMenu", true },
                    { "mode", "plant" },
                    { "formId", from_stolen_stack ? (std::to_string(form_id) + ":stolen") : std::to_string(form_id) },
                    { "message", "You were caught planting " + item_name + "!" }
                };
            }
        }
        const bool is_worn = (player_entry != nullptr && player_entry->IsWorn()) ||
            player->GetEquippedObject(false) == form ||
            player->GetEquippedObject(true) == form ||
            player->GetCurrentAmmo() == form;
        if (is_worn && total_carried - tx_count <= 0) {
            if (auto* eq_mgr = RE::ActorEquipManager::GetSingleton(); eq_mgr != nullptr) {
                (void)eq_mgr->UnequipObject(player, form, nullptr, tx_count, nullptr, true, false, true, false, nullptr);
            }
        }
        const std::int32_t before_player = count_in_ref(player, form);
        player->RemoveItem(form, tx_count, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, container);
        const std::int32_t moved_so_far = std::max(0, before_player - count_in_ref(player, form));
        if (moved_so_far < tx_count) {
            player->RemoveItem(form, tx_count - moved_so_far, RE::ITEM_REMOVE_REASON::kRemove, nullptr, container);
        }
        const std::int32_t after_player = std::max(0, count_in_ref(player, form));
        ConsumePlayerStolenCount(form, player_entry, total_carried, tx_count, after_player, from_stolen_stack || clean_count == 0);
        RE::SendUIMessage::SendInventoryUpdateMessage(player, form);
        RE::SendUIMessage::SendInventoryUpdateMessage(container, form);
        const bool equip_on_mannequin = options.value("equip", is_mannequin);
        if (equip_on_mannequin && container_actor != nullptr &&
            (form->Is(RE::FormType::Armor) || form->Is(RE::FormType::Weapon) || form->Is(RE::FormType::Ammo) || form->Is(RE::FormType::Light))) {
            Json eq_opts{
                { "formId", std::to_string(form_id) },
                { "slot", options.value("slot", std::string("auto")) },
                { "referenceId", std::to_string(container_actor->GetFormID()) }
            };
            (void)ChangePlayerEquipment(eq_opts, false);
        }
        if (container_actor != nullptr) container_actor->Update3DModel();
        RE::PlaySound((form->GetFormID() == 0x0000000FU || form->IsGold()) ? "ITMGoldDownSD" : "ITMGenericDownSD");
        skyultra::InvalidateSharedGameDataCache();
        auto updated = ReadContainerSnapshot(snapshot_opts);
        const std::string verb = is_pickpocket ? "Planted " : "Stored ";
        return {
            { "accepted", true },
            { "mode", is_pickpocket ? "plant" : "store" },
            { "formId", from_stolen_stack ? (std::to_string(form_id) + ":stolen") : std::to_string(form_id) },
            { "count", tx_count },
            { "message", verb + (tx_count > 1 ? std::to_string(tx_count) + "x " : std::string{}) + item_name + "." },
            { "container", std::move(updated) }
        };
    }

    return rejected("Unsupported container transfer mode.");
}

struct ActiveBlacksmithStationState {
    bool active{ false };
    RE::FormID furniture_form_id{ 0 };
    RE::ObjectRefHandle furniture_ref{};
    std::string station_kind{ "forge" };
    std::string station_mode{ "craft" };
    std::string station_name{ "Forge" };
    bool is_skyforge{ false };
    bool grants_skill_xp{ true };
    std::vector<RE::FormID> bench_keyword_ids;
};

ActiveBlacksmithStationState g_active_blacksmith_station;

[[nodiscard]] bool ClassifyBlacksmithFurniture(
    const RE::TESFurniture* furniture,
    std::string& out_kind,
    std::string& out_mode,
    bool& out_skyforge,
    bool& out_grants_xp)
{
    if (furniture == nullptr) {
        return false;
    }
    const auto bench_type = furniture->workBenchData.benchType.get();
    const auto uses_skill = furniture->workBenchData.usesSkill.get();
    out_grants_xp = (uses_skill == RE::ActorValue::kSmithing);
    out_skyforge = false;

    if (bench_type == RE::TESFurniture::WorkBenchData::BenchType::kSmithingWeapon) {
        out_kind = "grindstone";
        out_mode = "temper";
        out_grants_xp = true;
        return true;
    }
    if (bench_type == RE::TESFurniture::WorkBenchData::BenchType::kSmithingArmor) {
        out_kind = "workbench";
        out_mode = "temper";
        out_grants_xp = true;
        return true;
    }
    if (bench_type != RE::TESFurniture::WorkBenchData::BenchType::kCreateObject) {
        return false;
    }

    if (uses_skill == RE::ActorValue::kAlchemy || uses_skill == RE::ActorValue::kEnchanting) {
        return false;
    }
    if (furniture->HasKeywordString("CraftingCookpot") ||
        furniture->HasKeywordString("CraftingCookPot") ||
        furniture->HasKeywordString("BYOHCraftingOven") ||
        furniture->HasKeywordString("BYOHHouseCraftingBench") ||
        furniture->HasKeywordString("BYOHCarpenterWorkbench") ||
        furniture->HasKeywordString("CraftingAlchemy") ||
        furniture->HasKeywordString("CraftingEnchanting") ||
        furniture->HasKeywordString("DLC2StaffEnchanter") ||
        furniture->HasKeywordString("DLC2SpiderImbuing")) {
        return false;
    }

    const char* raw_name = furniture->GetFullName();
    if (raw_name == nullptr || raw_name[0] == '\0') raw_name = furniture->GetName();
    const std::string lower_name = raw_name != nullptr ? LowerAscii(raw_name) : std::string{};

    if (furniture->HasKeywordString("CraftingTanningRack") ||
        furniture->HasKeywordID(0x0007866AU) ||
        lower_name.find("tanning") != std::string::npos) {
        out_kind = "tanning";
        out_mode = "craft";
        return true;
    }
    if (furniture->HasKeywordString("CraftingSmelter") ||
        furniture->HasKeywordID(0x000A5CCEU) ||
        lower_name.find("smelter") != std::string::npos) {
        out_kind = "smelter";
        out_mode = "craft";
        return true;
    }
    if (furniture->HasKeywordString("CraftingSmithingSkyforge") ||
        furniture->HasKeywordID(0x000F46CEU)) {
        out_kind = "forge";
        out_mode = "craft";
        out_skyforge = true;
        out_grants_xp = true;
        return true;
    }
    if (furniture->HasKeywordString("CraftingSmithingForge") ||
        furniture->HasKeywordString("CraftingSmithing") ||
        furniture->HasKeywordID(0x00088105U) ||
        uses_skill == RE::ActorValue::kSmithing ||
        lower_name.find("forge") != std::string::npos ||
        lower_name.find("anvil") != std::string::npos) {
        out_kind = "forge";
        out_mode = "craft";
        out_grants_xp = true;
        return true;
    }
    return false;
}

[[nodiscard]] bool RecipeMatchesActiveBench(
    const RE::BGSConstructibleObject* recipe,
    const ActiveBlacksmithStationState& station) noexcept
{
    if (recipe == nullptr || recipe->benchKeyword == nullptr) {
        return false;
    }
    const auto bench_id = recipe->benchKeyword->GetFormID();
    if (std::ranges::find(station.bench_keyword_ids, bench_id) != station.bench_keyword_ids.end()) {
        return true;
    }
    if (auto* furn = station.furniture_form_id != 0
            ? RE::TESForm::LookupByID<RE::TESFurniture>(station.furniture_form_id)
            : nullptr;
        furn != nullptr) {
        return furn->HasKeyword(recipe->benchKeyword);
    }
    return false;
}

[[nodiscard]] std::string ClassifySmithingCategory(const RE::TESForm* item, std::string_view station_kind)
{
    if (station_kind == "tanning") {
        return "Leather & Pelts";
    }
    if (station_kind == "smelter") {
        return "Ingots & Metals";
    }
    if (item == nullptr) {
        return "Misc";
    }
    if (item->Is(RE::FormType::Ammo)) {
        return "Ammo";
    }
    if (const auto* kw = item->As<RE::BGSKeywordForm>(); kw != nullptr) {
        if (kw->HasKeywordString("ArmorMaterialDaedric") || kw->HasKeywordString("WeapMaterialDaedric") ||
            kw->HasKeywordString("DLC1ArmorMaterialDawnguard")) {
            if (kw->HasKeywordString("ArmorMaterialDaedric") || kw->HasKeywordString("WeapMaterialDaedric")) {
                return "Daedric";
            }
        }
        if (kw->HasKeywordString("ArmorMaterialDragonplate") || kw->HasKeywordString("ArmorMaterialDragonscale") ||
            kw->HasKeywordString("DLC1WeapMaterialDragonbone") || kw->HasKeywordString("WeapMaterialDragonbone")) {
            return "Dragon";
        }
        if (kw->HasKeywordString("ArmorMaterialEbony") || kw->HasKeywordString("WeapMaterialEbony") ||
            kw->HasKeywordString("ArmorMaterialEbonyMail")) {
            return "Ebony";
        }
        if (kw->HasKeywordString("ArmorMaterialGlass") || kw->HasKeywordString("WeapMaterialGlass")) {
            return "Glass";
        }
        if (kw->HasKeywordString("DLC2ArmorMaterialStalhrimHeavy") || kw->HasKeywordString("DLC2ArmorMaterialStalhrimLight") ||
            kw->HasKeywordString("DLC2WeapMaterialStalhrim")) {
            return "Stalhrim";
        }
        if (kw->HasKeywordString("DLC2ArmorMaterialNordic") || kw->HasKeywordString("DLC2WeapMaterialNordic")) {
            return "Nordic";
        }
        if (kw->HasKeywordString("ArmorMaterialOrcish") || kw->HasKeywordString("WeapMaterialOrcish")) {
            return "Orcish";
        }
        if (kw->HasKeywordString("ArmorMaterialDwarven") || kw->HasKeywordString("WeapMaterialDwarven")) {
            return "Dwarven";
        }
        if (kw->HasKeywordString("ArmorMaterialElven") || kw->HasKeywordString("ArmorMaterialElvenGilded") ||
            kw->HasKeywordString("WeapMaterialElven")) {
            return "Elven";
        }
        if (kw->HasKeywordString("ArmorMaterialSteelPlate") || kw->HasKeywordString("ArmorMaterialScaled")) {
            return "Advanced";
        }
        if (kw->HasKeywordString("ArmorMaterialSteel") || kw->HasKeywordString("WeapMaterialSteel") ||
            kw->HasKeywordString("WeapMaterialSkyforge")) {
            return "Steel";
        }
        if (kw->HasKeywordString("ArmorMaterialImperialHeavy") || kw->HasKeywordString("ArmorMaterialImperialLight") ||
            kw->HasKeywordString("ArmorMaterialImperialStudded") || kw->HasKeywordString("WeapMaterialImperial")) {
            return "Imperial";
        }
        if (kw->HasKeywordString("ArmorMaterialIron") || kw->HasKeywordString("ArmorMaterialIronBanded") ||
            kw->HasKeywordString("WeapMaterialIron")) {
            return "Iron";
        }
        if (kw->HasKeywordString("DLC2ArmorMaterialBonemoldHeavy") || kw->HasKeywordString("DLC2ArmorMaterialChitinLight") ||
            kw->HasKeywordString("DLC2ArmorMaterialChitinHeavy")) {
            return "Dunmer";
        }
        if (kw->HasKeywordString("ArmorMaterialLeather")) {
            return "Leather";
        }
        if (kw->HasKeywordString("ArmorMaterialHide") || kw->HasKeywordString("ArmorMaterialStudded")) {
            return "Hide";
        }
        if (kw->HasKeywordString("VendorItemJewelry") || kw->HasKeywordString("ArmorJewelry")) {
            return "Jewelry";
        }
    }
    const char* raw_n = item->GetName();
    if (raw_n != nullptr && raw_n[0] != '\0') {
        const auto ln = LowerAscii(raw_n);
        if (ln.find("daedric") != std::string::npos) return "Daedric";
        if (ln.find("dragon") != std::string::npos) return "Dragon";
        if (ln.find("ebony") != std::string::npos) return "Ebony";
        if (ln.find("glass") != std::string::npos) return "Glass";
        if (ln.find("stalhrim") != std::string::npos) return "Stalhrim";
        if (ln.find("nordic") != std::string::npos) return "Nordic";
        if (ln.find("orcish") != std::string::npos) return "Orcish";
        if (ln.find("dwarven") != std::string::npos) return "Dwarven";
        if (ln.find("elven") != std::string::npos) return "Elven";
        if (ln.find("skyforge") != std::string::npos || ln.find("steel") != std::string::npos) return "Steel";
        if (ln.find("iron") != std::string::npos) return "Iron";
        if (ln.find("leather") != std::string::npos) return "Leather";
        if (ln.find("hide") != std::string::npos || ln.find("studded") != std::string::npos) return "Hide";
        if (ln.find("gold") != std::string::npos || ln.find("silver") != std::string::npos ||
            ln.find("ring") != std::string::npos || ln.find("necklace") != std::string::npos ||
            ln.find("circlet") != std::string::npos) {
            return "Jewelry";
        }
    }
    return "Misc";
}

[[nodiscard]] std::string FormatConditionCompareOp(RE::CONDITION_ITEM_DATA::OpCode op) noexcept
{
    using Op = RE::CONDITION_ITEM_DATA::OpCode;
    switch (op) {
    case Op::kEqualTo: return "=";
    case Op::kNotEqualTo: return "!=";
    case Op::kGreaterThan: return ">";
    case Op::kGreaterThanOrEqualTo: return ">=";
    case Op::kLessThan: return "<";
    case Op::kLessThanOrEqualTo: return "<=";
    default: return ">=";
    }
}

void ExtractConstructibleRequirements(
    const RE::BGSConstructibleObject* recipe,
    RE::PlayerCharacter* player,
    Json& out_requirements,
    bool& out_conditions_met,
    bool& out_has_non_perk_block,
    std::string& out_first_unmet_reason)
{
    out_requirements = Json::array();
    out_has_non_perk_block = false;
    out_first_unmet_reason.clear();
    if (recipe == nullptr || player == nullptr) {
        out_conditions_met = false;
        out_has_non_perk_block = true;
        out_first_unmet_reason = "Player data is unavailable.";
        return;
    }
    out_conditions_met = recipe->conditions.head == nullptr || recipe->conditions.IsTrue(player, player);
    if (recipe->conditions.head == nullptr) {
        return;
    }

    RE::ConditionCheckParams check_params(player, player);
    const auto* cursor = recipe->conditions.head;
    while (cursor != nullptr) {
        std::vector<const RE::TESConditionItem*> group;
        while (cursor != nullptr) {
            group.push_back(cursor);
            const bool more_or = cursor->data.flags.isOR;
            cursor = cursor->next;
            if (!more_or) {
                break;
            }
        }
        bool group_met = false;
        bool group_has_perk_or_skill = false;
        std::vector<std::pair<Json, std::string>> group_entries;

        for (const auto* item : group) {
            const bool item_met = item->IsTrue(check_params);
            if (item_met) {
                group_met = true;
            }
            const auto fn = item->data.functionData.function.get();
            const float cmp = item->data.flags.global && item->data.comparisonValue.g != nullptr
                ? item->data.comparisonValue.g->value
                : item->data.comparisonValue.f;

            if (fn == RE::FUNCTION_DATA::FunctionID::kHasPerk) {
                auto* perk = static_cast<RE::TESForm*>(item->data.functionData.params[0]) != nullptr
                    ? static_cast<RE::TESForm*>(item->data.functionData.params[0])->As<RE::BGSPerk>()
                    : nullptr;
                if (perk != nullptr) {
                    const char* raw_p = perk->GetName();
                    const std::string p_name = (raw_p != nullptr && raw_p[0] != '\0')
                        ? StripLeadingDollar(raw_p)
                        : std::string("Smithing Perk");
                    const bool negated =
                        (item->data.flags.opCode == RE::CONDITION_ITEM_DATA::OpCode::kEqualTo && cmp < 0.5F) ||
                        (item->data.flags.opCode == RE::CONDITION_ITEM_DATA::OpCode::kLessThan && cmp <= 1.0F);
                    const std::string label = negated ? ("Without " + p_name) : (p_name + " perk");
                    group_has_perk_or_skill = true;
                    group_entries.emplace_back(
                        Json{ { "label", label }, { "met", item_met }, { "kind", "perk" } },
                        "Requires " + label);
                }
            } else if (fn == RE::FUNCTION_DATA::FunctionID::kGetActorValue ||
                       fn == RE::FUNCTION_DATA::FunctionID::kGetBaseActorValue ||
                       fn == RE::FUNCTION_DATA::FunctionID::kGetPermanentActorValue) {
                const auto av = static_cast<RE::ActorValue>(
                    reinterpret_cast<std::uintptr_t>(item->data.functionData.params[0]));
                const char* skill = SkillLabelForActorValue(av);
                const std::string skill_name = skill != nullptr ? std::string(skill) : std::string("Skill");
                const int target_val = static_cast<int>(std::lround(cmp));
                const std::string label = skill_name + " " + FormatConditionCompareOp(item->data.flags.opCode) +
                    " " + std::to_string(target_val);
                group_has_perk_or_skill = true;
                group_entries.emplace_back(
                    Json{ { "label", label }, { "met", item_met }, { "kind", "skill" } },
                    "Requires " + label);
            } else if (fn == RE::FUNCTION_DATA::FunctionID::kGetItemCount) {
                auto* req_form = static_cast<RE::TESForm*>(item->data.functionData.params[0]);
                if (req_form != nullptr) {
                    const char* raw_n = req_form->GetName();
                    const std::string item_n = (raw_n != nullptr && raw_n[0] != '\0')
                        ? StripLeadingDollar(raw_n)
                        : std::string("Required Item");
                    const std::string label = "Carry " + item_n;
                    group_entries.emplace_back(
                        Json{ { "label", label }, { "met", item_met }, { "kind", "item" } },
                        "Requires " + item_n);
                }
            }
        }

        if (group_entries.size() == 1) {
            auto& entry_json = group_entries[0].first;
            entry_json["met"] = group_met;
            out_requirements.push_back(entry_json);
            if (!group_met && out_first_unmet_reason.empty()) {
                out_first_unmet_reason = group_entries[0].second;
            }
        } else if (group_entries.size() > 1) {
            std::string combined_label;
            for (std::size_t idx = 0; idx < group_entries.size(); ++idx) {
                if (idx > 0) combined_label += " or ";
                combined_label += group_entries[idx].first.value("label", std::string{});
            }
            const std::string kind = group_entries[0].first.value("kind", std::string("perk"));
            out_requirements.push_back(Json{ { "label", combined_label }, { "met", group_met }, { "kind", kind } });
            if (!group_met && out_first_unmet_reason.empty()) {
                out_first_unmet_reason = "Requires " + combined_label;
            }
        }

        if (!group_met && !group_has_perk_or_skill) {
            out_has_non_perk_block = true;
            if (out_first_unmet_reason.empty()) {
                out_first_unmet_reason = "Special crafting prerequisite not met.";
            }
        }
    }
}

[[nodiscard]] float ComputeTemperPerkMultiplier(RE::PlayerCharacter* player, const RE::TESForm* item) noexcept
{
    if (player == nullptr || item == nullptr) {
        return 1.0F;
    }
    const auto* kw = item->As<RE::BGSKeywordForm>();
    if (kw == nullptr) {
        return 1.0F;
    }
    struct PerkMapping {
        RE::FormID perk_id;
        std::array<std::string_view, 5> keywords;
    };
    static constexpr std::array<PerkMapping, 9> kMappings{{
        { 0x000CB40DU, { "ArmorMaterialSteel", "WeapMaterialSteel", "WeapMaterialSkyforge", "", "" } },
        { 0x000CB40EU, { "ArmorMaterialDwarven", "WeapMaterialDwarven", "", "", "" } },
        { 0x000CB40FU, { "ArmorMaterialElven", "ArmorMaterialElvenGilded", "WeapMaterialElven", "", "" } },
        { 0x000CB410U, { "ArmorMaterialOrcish", "WeapMaterialOrcish", "", "", "" } },
        { 0x000CB411U, { "ArmorMaterialGlass", "WeapMaterialGlass", "", "", "" } },
        { 0x000CB412U, { "ArmorMaterialEbony", "WeapMaterialEbony", "ArmorMaterialEbonyMail", "DLC2ArmorMaterialNordic", "DLC2WeapMaterialNordic" } },
        { 0x000CB413U, { "ArmorMaterialDaedric", "WeapMaterialDaedric", "", "", "" } },
        { 0x000CB414U, { "ArmorMaterialSteelPlate", "ArmorMaterialScaled", "", "", "" } },
        { 0x00052190U, { "ArmorMaterialDragonplate", "ArmorMaterialDragonscale", "DLC1WeapMaterialDragonbone", "WeapMaterialDragonbone", "" } }
    }};
    for (const auto& entry : kMappings) {
        auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(entry.perk_id);
        if (perk == nullptr || !player->HasPerk(perk)) {
            continue;
        }
        for (const auto candidate : entry.keywords) {
            if (!candidate.empty() && kw->HasKeywordString(candidate)) {
                return 2.0F;
            }
        }
    }
    if (kw->HasKeywordString("DLC2ArmorMaterialStalhrimHeavy") ||
        kw->HasKeywordString("DLC2ArmorMaterialStalhrimLight") ||
        kw->HasKeywordString("DLC2WeapMaterialStalhrim")) {
        if (auto* ebony_perk = RE::TESForm::LookupByID<RE::BGSPerk>(0x000CB412U);
            ebony_perk != nullptr && player->HasPerk(ebony_perk)) {
            return 2.0F;
        }
    }
    return 1.0F;
}

[[nodiscard]] float ComputeTargetTemperHealth(float smithing_skill, float perk_mult) noexcept
{
    if (!std::isfinite(smithing_skill) || smithing_skill < 14.0F) {
        return 1.0F;
    }
    const float raw_tiers = 1.0F + ((smithing_skill - 14.0F) * std::max(1.0F, perk_mult)) / 34.3333F;
    const int tier = std::max(1, static_cast<int>(std::floor(raw_tiers + 0.5F)));
    return 1.0F + static_cast<float>(tier) * 0.1F;
}

[[nodiscard]] std::int32_t ComputeItemEngineStat(
    RE::PlayerCharacter* player,
    RE::TESBoundObject* item,
    float health)
{
    if (player == nullptr || item == nullptr) {
        return 0;
    }
    RE::ExtraDataList* temp_xlist = nullptr;
    float val = 0.0F;
    {
        RE::InventoryEntryData temp_entry(item, 1);
        if (health > 1.01F) {
            temp_xlist = new RE::ExtraDataList();
            temp_xlist->Add(new RE::ExtraHealth(health));
            temp_entry.AddExtraList(temp_xlist);
        }
        val = item->Is(RE::FormType::Weapon)
            ? player->GetDamage(&temp_entry)
            : (item->Is(RE::FormType::Armor) ? player->GetArmorValue(&temp_entry) : 0.0F);
    }
    delete temp_xlist;
    return std::isfinite(val) ? std::max<std::int32_t>(0, static_cast<std::int32_t>(std::lround(val))) : 0;
}

[[nodiscard]] std::int32_t CountPlayerNonQuestItem(
    const RE::TESObjectREFR::InventoryItemMap& inv,
    RE::TESForm* item_form)
{
    if (item_form == nullptr) {
        return 0;
    }
    if (const auto it = inv.find(item_form->As<RE::TESBoundObject>()); it != inv.end()) {
        if (it->second.second != nullptr && it->second.second->IsQuestObject()) {
            return 0;
        }
        return std::max<std::int32_t>(0, it->second.first);
    }
    return 0;
}

[[nodiscard]] RE::TESObjectREFR* ResolveActiveWorkbenchRef(
    RE::PlayerCharacter* player,
    RE::ObjectRefHandle fallback_handle) noexcept
{
    if (player != nullptr) {
        const auto occupied = player->GetOccupiedFurniture();
        if (auto ref = occupied.get(); ref != nullptr) {
            return ref.get();
        }
    }
    if (auto ref = fallback_handle.get(); ref != nullptr) {
        return ref.get();
    }
    return nullptr;
}

[[nodiscard]] bool IsStalhrimCraftedItem(const RE::TESForm* item) noexcept
{
    if (item == nullptr) {
        return false;
    }
    const auto* kw = item->As<RE::BGSKeywordForm>();
    if (kw == nullptr) {
        return false;
    }
    return kw->HasKeywordString("DLC2ArmorMaterialStalhrimHeavy") ||
        kw->HasKeywordString("DLC2ArmorMaterialStalhrimLight") ||
        kw->HasKeywordString("DLC2WeapMaterialStalhrim");
}

void EmitNativeCraftedItemEvents(
    RE::PlayerCharacter* player,
    RE::ObjectRefHandle fallback_bench,
    RE::TESForm* item,
    bool is_created,
    bool is_enchanting,
    bool is_stalhrim) noexcept
{
    if (player == nullptr || item == nullptr) {
        return;
    }
    auto* workbench = ResolveActiveWorkbenchRef(player, fallback_bench);
    auto* bench_loc = workbench != nullptr ? workbench->GetCurrentLocation() : player->GetCurrentLocation();
    if (auto* story_mgr = RE::BGSStoryEventManager::GetSingleton(); story_mgr != nullptr) {
        RE::BGSCraftItemEvent craft_story_ev(workbench, bench_loc, item);
        (void)story_mgr->AddEvent(craft_story_ev);
    }
    if (auto* crafted_src = RE::ItemCrafted::GetEventSource(); crafted_src != nullptr) {
        RE::ItemCrafted::Event ev{ item, is_created, is_enchanting, is_stalhrim, 0, 0 };
        crafted_src->SendEvent(&ev);
    }
    if (auto* bound = item->As<RE::TESBoundObject>(); bound != nullptr) {
        RE::SendUIMessage::SendInventoryUpdateMessage(player, bound);
    }
}

class SkyUltraVirtualCraftingMenu final : public RE::IMenu {
public:
    SkyUltraVirtualCraftingMenu() noexcept
    {
        depthPriority = 0;
        menuFlags.set(RE::UI_MENU_FLAGS::kOnStack);
    }
    ~SkyUltraVirtualCraftingMenu() override = default;
    void Accept(CallbackProcessor*) override {}
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage&) override
    {
        return RE::UI_MESSAGE_RESULTS::kIgnore;
    }
    void AdvanceMovie(float, std::uint32_t) override {}
    void PostDisplay() override {}

    void SetSubMenu(RE::CraftingSubMenus::CraftingSubMenu* sub) noexcept
    {
        REL::RelocateMember<RE::CraftingSubMenus::CraftingSubMenu*>(this, 0x30, 0x40) = sub;
    }

private:
    std::uint8_t crafting_pad_[0x30]{};
};

struct VirtualCraftingProxyState {
    SkyUltraVirtualCraftingMenu* menu_proxy{ nullptr };
    bool menu_map_installed{ false };
    std::uint64_t generation{ 0 };
    std::chrono::steady_clock::time_point expires_at{};
    alignas(16) std::array<std::uint8_t, 0x260> disenchant_menu_buf{};
    alignas(16) std::array<std::uint8_t, 0x40> disenchant_entry_buf{};
    RE::InventoryEntryData* disenchant_inv_proxy{ nullptr };
    RE::ExtraEnchantment* disenchant_extra_ench{ nullptr };
};
VirtualCraftingProxyState g_virtual_crafting_proxy;

void ClearVirtualCraftingProxyUnlocked() noexcept
{
    if (g_virtual_crafting_proxy.menu_map_installed) {
        if (auto* ui = RE::UI::GetSingleton(); ui != nullptr) {
            if (auto it = ui->menuMap.find(RE::CraftingMenu::MENU_NAME); it != ui->menuMap.end()) {
                if (it->second.menu.get() == g_virtual_crafting_proxy.menu_proxy) {
                    it->second.menu.reset();
                }
            }
        }
        g_virtual_crafting_proxy.menu_map_installed = false;
    }
    if (g_virtual_crafting_proxy.menu_proxy != nullptr) {
        g_virtual_crafting_proxy.menu_proxy->SetSubMenu(nullptr);
    }
    // Zero BSTSmartPointer<ItemChangeEntry> raw storage without invoking ~BSTSmartPointer on non-heap buffer.
    auto* fake_menu = reinterpret_cast<RE::CraftingSubMenus::EnchantConstructMenu*>(
        g_virtual_crafting_proxy.disenchant_menu_buf.data());
    std::memset(std::addressof(fake_menu->selected.item), 0, sizeof(fake_menu->selected.item));
}

void ScheduleVirtualCraftingProxyExpiry(std::uint64_t expected_gen, int remaining_hops = 20) noexcept
{
    auto* tasks = SKSE::GetTaskInterface();
    if (tasks == nullptr) {
        return;
    }
    tasks->AddTask([expected_gen, remaining_hops]() {
        if (g_virtual_crafting_proxy.generation != expected_gen ||
            !g_virtual_crafting_proxy.menu_map_installed) {
            return;
        }
        if (std::chrono::steady_clock::now() >= g_virtual_crafting_proxy.expires_at ||
            remaining_hops <= 0) {
            ClearVirtualCraftingProxyUnlocked();
            return;
        }
        ScheduleVirtualCraftingProxyExpiry(expected_gen, remaining_hops - 1);
    });
}

void ArmVirtualCraftingProxy(
    RE::PlayerCharacter* player,
    RE::TESObjectREFR* workbench_ref,
    bool for_disenchant = false,
    RE::TESBoundObject* disenchant_item = nullptr,
    RE::EnchantmentItem* disenchant_ench = nullptr) noexcept
{
    if (player == nullptr) {
        return;
    }
    if (workbench_ref != nullptr) {
        if (auto* events = RE::ScriptEventSourceHolder::GetSingleton(); events != nullptr) {
            RE::TESFurnitureEvent furn_ev{
                RE::NiPointer<RE::TESObjectREFR>(player),
                RE::NiPointer<RE::TESObjectREFR>(workbench_ref),
                RE::TESFurnitureEvent::FurnitureEventType::kEnter
            };
            events->SendEvent(&furn_ev);
        }
    }

    auto* ui = RE::UI::GetSingleton();
    if (ui == nullptr) {
        return;
    }
    auto it = ui->menuMap.find(RE::CraftingMenu::MENU_NAME);
    if (it == ui->menuMap.end()) {
        return;
    }

    if (g_virtual_crafting_proxy.menu_proxy == nullptr) {
        g_virtual_crafting_proxy.menu_proxy = new SkyUltraVirtualCraftingMenu();
        g_virtual_crafting_proxy.menu_proxy->AddRef();
    }

    RE::CraftingSubMenus::CraftingSubMenu* sub_menu = nullptr;
    if (for_disenchant && disenchant_item != nullptr && !REL::Module::IsVR()) {
        ClearVirtualCraftingProxyUnlocked();
        g_virtual_crafting_proxy.disenchant_menu_buf.fill(0U);
        g_virtual_crafting_proxy.disenchant_entry_buf.fill(0U);

        if (g_virtual_crafting_proxy.disenchant_inv_proxy == nullptr) {
            g_virtual_crafting_proxy.disenchant_inv_proxy = new RE::InventoryEntryData(disenchant_item, 1);
        } else {
            g_virtual_crafting_proxy.disenchant_inv_proxy->object = disenchant_item;
            g_virtual_crafting_proxy.disenchant_inv_proxy->countDelta = 1;
        }
        if (disenchant_ench != nullptr) {
            if (g_virtual_crafting_proxy.disenchant_extra_ench == nullptr) {
                auto* xlist = new RE::ExtraDataList();
                auto* xench = new RE::ExtraEnchantment(disenchant_ench, 0U, false);
                xlist->Add(xench);
                g_virtual_crafting_proxy.disenchant_inv_proxy->AddExtraList(xlist);
                g_virtual_crafting_proxy.disenchant_extra_ench = xench;
            } else {
                g_virtual_crafting_proxy.disenchant_extra_ench->enchantment = disenchant_ench;
            }
        } else if (g_virtual_crafting_proxy.disenchant_extra_ench != nullptr) {
            g_virtual_crafting_proxy.disenchant_extra_ench->enchantment = nullptr;
        }

        auto* entry = reinterpret_cast<RE::CraftingSubMenus::EnchantConstructMenu::ItemChangeEntry*>(
            g_virtual_crafting_proxy.disenchant_entry_buf.data());
        REL::Relocation<std::uintptr_t> entry_vtbl{
            RE::VTABLE_CraftingSubMenus__EnchantConstructMenu__ItemChangeEntry[0]
        };
        *reinterpret_cast<std::uintptr_t*>(entry) = entry_vtbl.address();
        // Set BSIntrusiveRefCounted::_refCount high so smart pointer never deletes proxy buffer.
        *reinterpret_cast<std::uint32_t*>(g_virtual_crafting_proxy.disenchant_entry_buf.data() + 0x08) = 16U;
        entry->selected = true;
        entry->enabled = true;
        entry->data = g_virtual_crafting_proxy.disenchant_inv_proxy;

        auto* fake_menu = reinterpret_cast<RE::CraftingSubMenus::EnchantConstructMenu*>(
            g_virtual_crafting_proxy.disenchant_menu_buf.data());
        REL::Relocation<std::uintptr_t> enchant_menu_vtbl{
            RE::VTABLE_CraftingSubMenus__EnchantConstructMenu[0]
        };
        *reinterpret_cast<std::uintptr_t*>(fake_menu) = enchant_menu_vtbl.address();
        fake_menu->currentCategory = RE::CraftingSubMenus::EnchantConstructMenu::Category::Disenchant;
        *reinterpret_cast<RE::CraftingSubMenus::EnchantConstructMenu::ItemChangeEntry**>(
            std::addressof(fake_menu->selected.item)) = entry;

        sub_menu = reinterpret_cast<RE::CraftingSubMenus::CraftingSubMenu*>(fake_menu);
    }

    if (it->second.menu == nullptr || it->second.menu.get() == g_virtual_crafting_proxy.menu_proxy) {
        g_virtual_crafting_proxy.menu_proxy->SetSubMenu(sub_menu);
        it->second.menu.reset(g_virtual_crafting_proxy.menu_proxy);
        g_virtual_crafting_proxy.menu_map_installed = true;
        g_virtual_crafting_proxy.expires_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
        const auto gen = ++g_virtual_crafting_proxy.generation;
        ScheduleVirtualCraftingProxyExpiry(gen);
    }
}

[[nodiscard]] bool IsActiveRunningQuestObject(
    const RE::InventoryEntryData* entry,
    const RE::ExtraDataList* stack_xlist = nullptr) noexcept
{
    if (entry == nullptr && stack_xlist == nullptr) {
        return false;
    }
    const auto xlist_has_active_quest = [](const RE::ExtraDataList* xl) noexcept -> bool {
        if (xl == nullptr) {
            return false;
        }
        if (const auto* alias_arr = xl->GetByType<RE::ExtraAliasInstanceArray>(); alias_arr != nullptr) {
            bool saw_alias = false;
            for (const auto* inst : alias_arr->aliases) {
                if (inst == nullptr || inst->alias == nullptr) {
                    continue;
                }
                if (inst->alias->IsQuestObject()) {
                    saw_alias = true;
                    if (inst->quest == nullptr || (inst->quest->IsRunning() && !inst->quest->IsCompleted())) {
                        return true;
                    }
                }
            }
            if (saw_alias) {
                return false;
            }
        }
        return const_cast<RE::ExtraDataList*>(xl)->HasQuestObjectAlias();
    };
    if (stack_xlist != nullptr) {
        return xlist_has_active_quest(stack_xlist);
    }
    if (entry != nullptr && entry->extraLists != nullptr) {
        for (const auto* xl : *entry->extraLists) {
            if (xlist_has_active_quest(xl)) {
                return true;
            }
        }
    }
    return false;
}

void PlayBlacksmithSound(std::string_view station_kind) noexcept
{
    if (station_kind == "tanning") {
        RE::PlaySound("UISmithingTanningRack");
    } else if (station_kind == "smelter") {
        RE::PlaySound("UISmithingSmelter");
    } else if (station_kind == "grindstone") {
        RE::PlaySound("UISmithingSharpeningWheel");
    } else if (station_kind == "workbench") {
        RE::PlaySound("UISmithingArmorWorkbench");
    } else {
        RE::PlaySound("UISmithingForge");
    }
}

Json ReadBlacksmithSnapshot(const Json& options)
{
    (void)options;
    const auto unavailable = [](std::string message) -> Json {
        return {
            { "available", false },
            { "stationKind", "forge" },
            { "stationMode", "craft" },
            { "stationName", "Blacksmith" },
            { "categories", Json::array() },
            { "recipes", Json::array() },
            { "temperItems", Json::array() },
            { "message", std::move(message) }
        };
    };

    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* data_handler = RE::TESDataHandler::GetSingleton();
    if (player == nullptr || data_handler == nullptr) {
        return unavailable("Player crafting state is unavailable.");
    }
    if (!g_active_blacksmith_station.active) {
        if (!skyultra::CaptureActiveBlacksmithStation()) {
            return unavailable("No blacksmith crafting station is currently active.");
        }
    }
    const ActiveBlacksmithStationState station = g_active_blacksmith_station;

    const auto* av_owner = player->AsActorValueOwner();
    const float eff_smithing = av_owner != nullptr ? std::max(0.0F, av_owner->GetActorValue(RE::ActorValue::kSmithing)) : 15.0F;
    const float base_smithing = av_owner != nullptr ? std::max(0.0F, av_owner->GetBaseActorValue(RE::ActorValue::kSmithing)) : 15.0F;
    auto* arcane_perk = RE::TESForm::LookupByID<RE::BGSPerk>(0x0005218EU);
    const bool has_arcane = arcane_perk != nullptr && player->HasPerk(arcane_perk);
    const float max_carry = av_owner != nullptr
        ? std::max(1.0F, av_owner->GetActorValue(RE::ActorValue::kCarryWeight))
        : 300.0F;
    const float cur_carry = av_owner != nullptr
        ? std::max(0.0F, av_owner->GetActorValue(RE::ActorValue::kInventoryWeight))
        : std::max(0.0F, player->GetWeightInContainer());

    const auto inv = player->GetInventory();
    const auto& all_recipes = data_handler->GetFormArray<RE::BGSConstructibleObject>();

    Json categories_json = Json::array();
    Json recipes_json = Json::array();
    Json temper_json = Json::array();
    std::unordered_map<std::string, int> category_counts;

    if (station.station_mode == "craft") {
        for (auto* recipe : all_recipes) {
            if (recipe == nullptr || recipe->IsDeleted() || recipe->createdItem == nullptr ||
                !RecipeMatchesActiveBench(recipe, station)) {
                continue;
            }
            auto* out_form = recipe->createdItem;
            const auto* raw_name = out_form->GetName();
            if (raw_name == nullptr || raw_name[0] == '\0' || IsMissingOrUnnamedDisplayName(raw_name) ||
                IsHiddenInternalInventoryItem(raw_name)) {
                continue;
            }
            const std::string item_name = StripLeadingDollar(raw_name);

            Json requirements_arr;
            bool conditions_met = true;
            bool has_non_perk_block = false;
            std::string first_block_reason;
            ExtractConstructibleRequirements(
                recipe, player, requirements_arr, conditions_met, has_non_perk_block, first_block_reason);
            if (!conditions_met && has_non_perk_block) {
                continue;
            }

            Json materials_arr = Json::array();
            bool materials_met = true;
            std::int32_t max_craftable = 9999;
            std::string first_missing_mat;
            const auto req_span = std::span(
                recipe->requiredItems.containerObjects, recipe->requiredItems.numContainerObjects);
            for (const auto* req_obj : req_span) {
                if (req_obj == nullptr || req_obj->obj == nullptr) {
                    continue;
                }
                const std::int32_t req_count = std::max<std::int32_t>(1, std::abs(req_obj->count));
                const std::int32_t owned_count = CountPlayerNonQuestItem(inv, req_obj->obj);
                const bool met = owned_count >= req_count;
                if (!met) {
                    materials_met = false;
                    if (first_missing_mat.empty()) {
                        const char* mn = req_obj->obj->GetName();
                        first_missing_mat = "Missing " +
                            ((mn != nullptr && mn[0] != '\0') ? StripLeadingDollar(mn) : std::string("materials"));
                    }
                }
                max_craftable = std::min(max_craftable, owned_count / req_count);
                const char* raw_mat_name = req_obj->obj->GetName();
                materials_arr.push_back({
                    { "id", std::to_string(req_obj->obj->GetFormID()) },
                    { "name", (raw_mat_name != nullptr && raw_mat_name[0] != '\0') ? StripLeadingDollar(raw_mat_name) : std::string("Material") },
                    { "required", req_count },
                    { "owned", owned_count },
                    { "met", met }
                });
            }
            if (req_span.empty()) {
                max_craftable = 1;
            }
            if (!conditions_met) {
                max_craftable = 0;
            }
            const bool can_craft = conditions_met && materials_met;
            std::string block_reason;
            if (!conditions_met) {
                block_reason = first_block_reason;
            } else if (!materials_met) {
                block_reason = first_missing_mat;
            }

            const std::string category = ClassifySmithingCategory(out_form, station.station_kind);
            category_counts[category] += 1;

            Json base_item = PlayerItemJson(out_form, std::max<std::int32_t>(1, recipe->data.numConstructed), false, nullptr, false);
            if (!base_item.is_object()) {
                base_item = Json::object();
            }
            Json entry_json{
                { "recipeId", std::to_string(recipe->GetFormID()) },
                { "itemId", std::to_string(out_form->GetFormID()) },
                { "name", item_name },
                { "category", category },
                { "itemType", std::string(ItemCategory(out_form->GetFormType())) },
                { "outputCount", std::max<std::int32_t>(1, recipe->data.numConstructed) },
                { "value", std::max(0, out_form->GetGoldValue()) },
                { "weight", std::round(std::max(0.0F, out_form->GetWeight()) * 100.0F) / 100.0F },
                { "materials", std::move(materials_arr) },
                { "requirements", std::move(requirements_arr) },
                { "conditionsMet", conditions_met },
                { "materialsMet", materials_met },
                { "canCraft", can_craft },
                { "maxCraftable", std::max<std::int32_t>(0, max_craftable) },
                { "blockReason", block_reason },
                { "ownedCount", CountPlayerNonQuestItem(inv, out_form) }
            };
            for (const char* stat_key : { "damage", "armorRating", "weaponType", "armorTypeLabel", "enchanted", "enchantmentName" }) {
                if (base_item.contains(stat_key)) {
                    entry_json[stat_key] = base_item[stat_key];
                }
            }
            recipes_json.push_back(std::move(entry_json));
        }
    } else {
        std::unordered_map<RE::FormID, std::vector<RE::BGSConstructibleObject*>> temper_by_item;
        for (auto* recipe : all_recipes) {
            if (recipe == nullptr || recipe->IsDeleted() || recipe->createdItem == nullptr ||
                !RecipeMatchesActiveBench(recipe, station)) {
                continue;
            }
            temper_by_item[recipe->createdItem->GetFormID()].push_back(recipe);
        }

        for (const auto& [bound_obj, inv_pair] : inv) {
            if (bound_obj == nullptr || inv_pair.first <= 0 || inv_pair.second == nullptr) {
                continue;
            }
            if ((station.station_kind == "grindstone" && !bound_obj->Is(RE::FormType::Weapon)) ||
                (station.station_kind == "workbench" && !bound_obj->Is(RE::FormType::Armor))) {
                continue;
            }
            const auto rec_it = temper_by_item.find(bound_obj->GetFormID());
            if (rec_it == temper_by_item.end() || rec_it->second.empty()) {
                continue;
            }
            RE::BGSConstructibleObject* chosen_recipe = nullptr;
            bool chosen_conditions_met = false;
            Json chosen_requirements = Json::array();
            std::string chosen_block_reason;
            for (auto* cand : rec_it->second) {
                Json reqs;
                bool cond_met = false;
                bool non_perk_block = false;
                std::string block_str;
                ExtractConstructibleRequirements(cand, player, reqs, cond_met, non_perk_block, block_str);
                if (cond_met) {
                    chosen_recipe = cand;
                    chosen_conditions_met = true;
                    chosen_requirements = std::move(reqs);
                    chosen_block_reason.clear();
                    break;
                }
                if (chosen_recipe == nullptr && !non_perk_block) {
                    chosen_recipe = cand;
                    chosen_conditions_met = false;
                    chosen_requirements = std::move(reqs);
                    chosen_block_reason = std::move(block_str);
                }
            }
            if (chosen_recipe == nullptr) {
                continue;
            }

            Json materials_arr = Json::array();
            bool materials_met = true;
            std::string first_missing_mat;
            const auto req_span = std::span(
                chosen_recipe->requiredItems.containerObjects, chosen_recipe->requiredItems.numContainerObjects);
            for (const auto* req_obj : req_span) {
                if (req_obj == nullptr || req_obj->obj == nullptr) {
                    continue;
                }
                const std::int32_t req_count = std::max<std::int32_t>(1, std::abs(req_obj->count));
                const std::int32_t owned_count = CountPlayerNonQuestItem(inv, req_obj->obj);
                const bool met = owned_count >= req_count;
                if (!met) {
                    materials_met = false;
                    if (first_missing_mat.empty()) {
                        const char* mn = req_obj->obj->GetName();
                        first_missing_mat = "Missing " +
                            ((mn != nullptr && mn[0] != '\0') ? StripLeadingDollar(mn) : std::string("materials"));
                    }
                }
                const char* raw_mat_name = req_obj->obj->GetName();
                materials_arr.push_back({
                    { "id", std::to_string(req_obj->obj->GetFormID()) },
                    { "name", (raw_mat_name != nullptr && raw_mat_name[0] != '\0') ? StripLeadingDollar(raw_mat_name) : std::string("Material") },
                    { "required", req_count },
                    { "owned", owned_count },
                    { "met", met }
                });
            }

            auto* entry_data = inv_pair.second.get();
            const std::int32_t total_count = inv_pair.first;
            const auto* base_ench = bound_obj->As<RE::TESEnchantableForm>();
            const bool base_is_enchanted = base_ench != nullptr && base_ench->formEnchanting != nullptr;
            const float perk_mult = ComputeTemperPerkMultiplier(player, bound_obj);
            const float target_health = ComputeTargetTemperHealth(eff_smithing, perk_mult);
            const char* target_tier_ptr = TemperTierLabel(target_health);
            const std::string target_tier = target_tier_ptr != nullptr ? std::string(target_tier_ptr) : std::string{};
            const std::int32_t projected_stat_val = target_health > 1.01F
                ? ComputeItemEngineStat(player, bound_obj, target_health)
                : 0;
            const std::string category = ClassifySmithingCategory(bound_obj, station.station_kind);

            const auto emit_stack = [&](std::int32_t stack_index, std::int32_t stack_count, RE::ExtraDataList* x_list) {
                if (stack_count <= 0) {
                    return;
                }
                const auto* ex_health = x_list != nullptr ? x_list->GetByType<RE::ExtraHealth>() : nullptr;
                const float cur_health = (ex_health != nullptr && std::isfinite(ex_health->health)) ? ex_health->health : 1.0F;
                const char* cur_tier_ptr = TemperTierLabel(cur_health);
                const std::string cur_tier = cur_tier_ptr != nullptr ? std::string(cur_tier_ptr) : std::string("Base");

                const auto* ex_ench = x_list != nullptr ? x_list->GetByType<RE::ExtraEnchantment>() : nullptr;
                RE::EnchantmentItem* ench = ex_ench != nullptr ? ex_ench->enchantment : (base_is_enchanted ? base_ench->formEnchanting : nullptr);
                const bool is_enchanted = ench != nullptr;
                const bool equipped = x_list != nullptr &&
                    (x_list->HasType<RE::ExtraWorn>() || x_list->HasType<RE::ExtraWornLeft>());

                const char* disp_name = x_list != nullptr ? x_list->GetDisplayName(bound_obj) : bound_obj->GetName();
                if (disp_name == nullptr || disp_name[0] == '\0' || IsMissingOrUnnamedDisplayName(disp_name)) {
                    disp_name = bound_obj->GetName();
                }
                if (disp_name == nullptr || disp_name[0] == '\0' || IsMissingOrUnnamedDisplayName(disp_name) ||
                    IsHiddenInternalInventoryItem(disp_name)) {
                    return;
                }
                const std::string custom_name = CustomItemDisplayName(bound_obj->GetFormID());
                const std::string final_name = !custom_name.empty() ? custom_name : StripLeadingDollar(disp_name);

                Json stack_reqs = chosen_requirements;
                bool skill_upgrade_available = target_health > cur_health + 0.015F;
                bool arcane_ok = !is_enchanted || has_arcane;
                if (is_enchanted) {
                    stack_reqs.push_back({ { "label", "Arcane Blacksmith perk" }, { "met", has_arcane }, { "kind", "perk" } });
                }
                if (!skill_upgrade_available) {
                    stack_reqs.push_back({
                        { "label", "Higher Smithing skill or perk needed to surpass " + cur_tier },
                        { "met", false },
                        { "kind", "skill" }
                    });
                }

                std::string block_reason;
                if (!arcane_ok) {
                    block_reason = "Requires Arcane Blacksmith perk to improve enchanted gear.";
                } else if (!chosen_conditions_met) {
                    block_reason = chosen_block_reason;
                } else if (!skill_upgrade_available) {
                    block_reason = "Already improved as high as your current Smithing allows.";
                } else if (!materials_met) {
                    block_reason = first_missing_mat;
                }

                const bool can_temper = chosen_conditions_met && arcane_ok && skill_upgrade_available && materials_met;
                const std::int32_t current_stat = ComputeItemEngineStat(player, bound_obj, cur_health);
                const std::int32_t next_stat = skill_upgrade_available ? std::max(current_stat, projected_stat_val) : current_stat;

                Json base_item = PlayerItemJson(bound_obj, stack_count, equipped, nullptr, true);
                if (!base_item.is_object()) {
                    base_item = Json::object();
                }

                category_counts[category] += 1;
                Json item_entry{
                    { "id", std::to_string(bound_obj->GetFormID()) + ":" + std::to_string(stack_index) },
                    { "itemId", std::to_string(bound_obj->GetFormID()) },
                    { "recipeId", std::to_string(chosen_recipe->GetFormID()) },
                    { "stackIndex", stack_index },
                    { "name", final_name },
                    { "category", category },
                    { "itemType", std::string(ItemCategory(bound_obj->GetFormType())) },
                    { "count", stack_count },
                    { "value", std::max(0, bound_obj->GetGoldValue()) },
                    { "weight", std::round(std::max(0.0F, bound_obj->GetWeight()) * 100.0F) / 100.0F },
                    { "equipped", equipped },
                    { "enchanted", is_enchanted },
                    { "currentTier", cur_tier },
                    { "currentHealth", std::round(cur_health * 100.0F) / 100.0F },
                    { "targetTier", skill_upgrade_available ? target_tier : cur_tier },
                    { "targetHealth", std::round(target_health * 100.0F) / 100.0F },
                    { "currentStat", current_stat },
                    { "projectedStat", next_stat },
                    { "materials", materials_arr },
                    { "requirements", std::move(stack_reqs) },
                    { "canTemper", can_temper },
                    { "blockReason", block_reason }
                };
                if (ench != nullptr && ench->GetFullName() != nullptr && ench->GetFullName()[0] != '\0') {
                    item_entry["enchantmentName"] = ench->GetFullName();
                }
                if (base_item.contains("weaponType")) item_entry["weaponType"] = base_item["weaponType"];
                if (base_item.contains("armorTypeLabel")) item_entry["armorTypeLabel"] = base_item["armorTypeLabel"];
                temper_json.push_back(std::move(item_entry));
            };

            std::int32_t tracked_count = 0;
            std::int32_t idx = 0;
            if (entry_data->extraLists != nullptr) {
                for (auto* x_list : *entry_data->extraLists) {
                    if (x_list == nullptr) {
                        continue;
                    }
                    const std::int32_t x_cnt = std::max<std::int32_t>(1, x_list->GetCount());
                    tracked_count += x_cnt;
                    emit_stack(idx++, x_cnt, x_list);
                }
            }
            if (total_count > tracked_count) {
                emit_stack(-1, total_count - tracked_count, nullptr);
            }
        }
    }

    static constexpr std::array<std::string_view, 21> kSmithingCategoryOrder{
        "Iron", "Steel", "Advanced", "Imperial", "Hide", "Leather",
        "Dunmer", "Elven", "Dwarven", "Orcish", "Nordic", "Glass",
        "Ebony", "Stalhrim", "Daedric", "Dragon", "Jewelry", "Ammo",
        "Leather & Pelts", "Ingots & Metals", "Misc"
    };
    const auto category_rank = [](std::string_view name) noexcept -> std::size_t {
        for (std::size_t i = 0; i < kSmithingCategoryOrder.size(); ++i) {
            if (kSmithingCategoryOrder[i] == name) return i;
        }
        return kSmithingCategoryOrder.size();
    };
    std::vector<std::pair<std::string, int>> sorted_categories(category_counts.begin(), category_counts.end());
    std::sort(sorted_categories.begin(), sorted_categories.end(),
        [&](const auto& a, const auto& b) {
            const auto ra = category_rank(a.first);
            const auto rb = category_rank(b.first);
            if (ra != rb) return ra < rb;
            return a.first < b.first;
        });
    for (const auto& [cat_name, cat_count] : sorted_categories) {
        categories_json.push_back({ { "id", cat_name }, { "label", cat_name }, { "count", cat_count } });
    }

    const auto* raw_pname = player->GetName();
    return {
        { "available", true },
        { "stationKind", station.station_kind },
        { "stationMode", station.station_mode },
        { "stationName", station.station_name },
        { "isSkyforge", station.is_skyforge },
        { "smithingSkill", std::round(eff_smithing * 10.0F) / 10.0F },
        { "smithingBaseSkill", std::round(base_smithing * 10.0F) / 10.0F },
        { "hasArcaneBlacksmith", has_arcane },
        { "playerName", (raw_pname != nullptr && raw_pname[0] != '\0') ? std::string(raw_pname) : std::string("Dragonborn") },
        { "playerGold", std::max(0, player->GetGoldAmount()) },
        { "playerCarryWeight", { { "current", std::round(cur_carry * 10.0F) / 10.0F }, { "max", std::round(max_carry * 10.0F) / 10.0F } } },
        { "categories", std::move(categories_json) },
        { "recipes", std::move(recipes_json) },
        { "temperItems", std::move(temper_json) }
    };
}

Json PerformBlacksmithCraft(const Json& options)
{
    const auto rejected = [](std::string message) -> Json {
        return { { "accepted", false }, { "message", std::move(message) } };
    };
    if (!options.is_object()) {
        return rejected("Blacksmith craft parameters must be an object.");
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return rejected("Player data is unavailable.");
    }
    if (!g_active_blacksmith_station.active || g_active_blacksmith_station.station_mode != "craft") {
        return rejected("This crafting station does not create new items.");
    }
    const std::uint32_t recipe_id = ParseItemFormIdField(options, "recipeId");
    const std::int32_t count = std::clamp(options.value("count", 1), 1, 1000);
    auto* recipe = recipe_id != 0 ? RE::TESForm::LookupByID<RE::BGSConstructibleObject>(recipe_id) : nullptr;
    if (recipe == nullptr || recipe->createdItem == nullptr || !RecipeMatchesActiveBench(recipe, g_active_blacksmith_station)) {
        return rejected("That crafting recipe is unavailable at this station.");
    }
    if (recipe->conditions.head != nullptr && !recipe->conditions.IsTrue(player, player)) {
        return rejected("You do not meet all perk or skill requirements for this recipe.");
    }
    auto* created_bound = recipe->createdItem->As<RE::TESBoundObject>();
    if (created_bound == nullptr) {
        return rejected("The crafted item cannot be placed in inventory.");
    }

    const auto inv = player->GetInventory();
    const auto req_span = std::span(
        recipe->requiredItems.containerObjects, recipe->requiredItems.numContainerObjects);
    for (const auto* req_obj : req_span) {
        if (req_obj == nullptr || req_obj->obj == nullptr) {
            continue;
        }
        const std::int32_t needed = std::max<std::int32_t>(1, std::abs(req_obj->count)) * count;
        const std::int32_t owned = CountPlayerNonQuestItem(inv, req_obj->obj);
        if (owned < needed) {
            const char* mn = req_obj->obj->GetName();
            return rejected("Insufficient " + ((mn != nullptr && mn[0] != '\0') ? StripLeadingDollar(mn) : std::string("materials")) + ".");
        }
    }

    const std::int32_t per_craft_qty = std::max<std::int32_t>(1, recipe->data.numConstructed);
    const std::int32_t crafted_qty = per_craft_qty * count;
    const bool is_stalhrim = IsStalhrimCraftedItem(created_bound);
    const bool is_smithing_misc = is_stalhrim ||
        g_active_blacksmith_station.station_kind == "smelter" ||
        g_active_blacksmith_station.station_kind == "forge";
    auto* active_workbench = ResolveActiveWorkbenchRef(player, g_active_blacksmith_station.furniture_ref);
    ArmVirtualCraftingProxy(player, active_workbench);
    if (count <= 100) {
        std::unordered_map<RE::FormID, std::int32_t> remaining_by_mat;
        for (const auto* req_obj : req_span) {
            if (req_obj == nullptr || req_obj->obj == nullptr) {
                continue;
            }
            const auto mat_id = req_obj->obj->GetFormID();
            if (!remaining_by_mat.contains(mat_id)) {
                const auto it = inv.find(req_obj->obj->As<RE::TESBoundObject>());
                remaining_by_mat.emplace(mat_id, (it != inv.end()) ? std::max<std::int32_t>(0, it->second.first) : 0);
            }
        }
        for (std::int32_t iter = 0; iter < count; ++iter) {
            for (const auto* req_obj : req_span) {
                if (req_obj == nullptr || req_obj->obj == nullptr) {
                    continue;
                }
                const std::int32_t per_req = std::max<std::int32_t>(1, std::abs(req_obj->count));
                const auto it = inv.find(req_obj->obj->As<RE::TESBoundObject>());
                auto& rem = remaining_by_mat[req_obj->obj->GetFormID()];
                const std::int32_t before = rem > 0 ? rem : per_req;
                player->RemoveItem(req_obj->obj, per_req, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                rem = std::max(0, before - per_req);
                ConsumePlayerStolenCount(req_obj->obj, (it != inv.end() ? it->second.second.get() : nullptr), before, per_req, rem, false);
            }
            player->AddObjectToContainer(created_bound, nullptr, per_craft_qty, nullptr);
            EmitNativeCraftedItemEvents(
                player,
                g_active_blacksmith_station.furniture_ref,
                created_bound,
                true,
                false,
                is_smithing_misc);
        }
    } else {
        for (const auto* req_obj : req_span) {
            if (req_obj == nullptr || req_obj->obj == nullptr) {
                continue;
            }
            const std::int32_t needed = std::max<std::int32_t>(1, std::abs(req_obj->count)) * count;
            const auto it = inv.find(req_obj->obj->As<RE::TESBoundObject>());
            const std::int32_t before = (it != inv.end()) ? std::max<std::int32_t>(0, it->second.first) : needed;
            player->RemoveItem(req_obj->obj, needed, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            ConsumePlayerStolenCount(req_obj->obj, (it != inv.end() ? it->second.second.get() : nullptr), before, needed, std::max(0, before - needed), false);
        }
        player->AddObjectToContainer(created_bound, nullptr, crafted_qty, nullptr);
        for (std::int32_t iter = 0; iter < count; ++iter) {
            EmitNativeCraftedItemEvents(
                player,
                g_active_blacksmith_station.furniture_ref,
                created_bound,
                true,
                false,
                is_smithing_misc);
        }
    }

    float xp_gained = 0.0F;
    if (g_active_blacksmith_station.grants_skill_xp) {
        xp_gained = std::max(0.0F, recipe->CalcSkillUse()) * static_cast<float>(count);
        if (xp_gained > 0.0F) {
            player->UseSkill(RE::ActorValue::kSmithing, xp_gained, nullptr);
        }
    }

    PlayBlacksmithSound(g_active_blacksmith_station.station_kind);
    skyultra::InvalidateSharedGameDataCache();

    const char* raw_out_name = created_bound->GetName();
    const std::string crafted_name = (raw_out_name != nullptr && raw_out_name[0] != '\0')
        ? StripLeadingDollar(raw_out_name)
        : std::string("Item");
    auto updated_snapshot = ReadBlacksmithSnapshot(Json::object());
    return {
        { "accepted", true },
        { "recipeId", std::to_string(recipe_id) },
        { "craftedName", crafted_name },
        { "craftedCount", crafted_qty },
        { "xpGained", std::round(xp_gained * 10.0F) / 10.0F },
        { "message", "Crafted " + (crafted_qty > 1 ? (std::to_string(crafted_qty) + "x ") : std::string{}) + crafted_name + "." },
        { "snapshot", std::move(updated_snapshot) }
    };
}

Json PerformBlacksmithTemper(const Json& options)
{
    const auto rejected = [](std::string message) -> Json {
        return { { "accepted", false }, { "message", std::move(message) } };
    };
    if (!options.is_object()) {
        return rejected("Blacksmith temper parameters must be an object.");
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return rejected("Player data is unavailable.");
    }
    if (!g_active_blacksmith_station.active || g_active_blacksmith_station.station_mode != "temper") {
        return rejected("This station is not configured for tempering equipment.");
    }
    const std::uint32_t recipe_id = ParseItemFormIdField(options, "recipeId");
    const std::uint32_t item_id = ParseItemFormIdField(options, "itemId");
    const std::int32_t stack_index = options.value("stackIndex", -1);

    auto* recipe = recipe_id != 0 ? RE::TESForm::LookupByID<RE::BGSConstructibleObject>(recipe_id) : nullptr;
    auto* target_form = item_id != 0 ? RE::TESForm::LookupByID<RE::TESBoundObject>(item_id) : nullptr;
    if (recipe == nullptr || target_form == nullptr || recipe->createdItem != target_form ||
        !RecipeMatchesActiveBench(recipe, g_active_blacksmith_station)) {
        return rejected("That tempering recipe does not apply to the selected gear at this station.");
    }
    if (recipe->conditions.head != nullptr && !recipe->conditions.IsTrue(player, player)) {
        return rejected("You do not meet the Smithing perk requirements to improve this item.");
    }

    auto inv = player->GetInventory();
    const auto inv_it = inv.find(target_form);
    if (inv_it == inv.end() || inv_it->second.first <= 0) {
        return rejected("You no longer carry that item.");
    }

    auto* live_entry = FindLiveInventoryEntry(player, target_form);
    RE::ExtraDataList* target_xlist = nullptr;
    if (stack_index >= 0 && live_entry != nullptr && live_entry->extraLists != nullptr) {
        std::int32_t idx = 0;
        for (auto* candidate : *live_entry->extraLists) {
            if (candidate == nullptr) {
                continue;
            }
            if (idx == stack_index) {
                target_xlist = candidate;
                break;
            }
            ++idx;
        }
        if (target_xlist == nullptr) {
            return rejected("The selected equipment stack could not be located.");
        }
    }

    const auto* base_ench = target_form->As<RE::TESEnchantableForm>();
    const bool is_enchanted =
        (target_xlist != nullptr && target_xlist->HasType<RE::ExtraEnchantment>()) ||
        (base_ench != nullptr && base_ench->formEnchanting != nullptr);
    if (is_enchanted) {
        auto* arcane_perk = RE::TESForm::LookupByID<RE::BGSPerk>(0x0005218EU);
        if (arcane_perk == nullptr || !player->HasPerk(arcane_perk)) {
            return rejected("Requires the Arcane Blacksmith perk to improve enchanted items.");
        }
    }

    const auto* av_owner = player->AsActorValueOwner();
    const float eff_smithing = av_owner != nullptr ? std::max(0.0F, av_owner->GetActorValue(RE::ActorValue::kSmithing)) : 15.0F;
    const float perk_mult = ComputeTemperPerkMultiplier(player, target_form);
    const float target_health = ComputeTargetTemperHealth(eff_smithing, perk_mult);
    const float current_health = (target_xlist != nullptr && target_xlist->GetByType<RE::ExtraHealth>() != nullptr)
        ? target_xlist->GetByType<RE::ExtraHealth>()->health
        : 1.0F;
    if (target_health <= current_health + 0.015F) {
        return rejected("Your Smithing skill is not high enough to improve this item any further.");
    }

    const auto req_span = std::span(
        recipe->requiredItems.containerObjects, recipe->requiredItems.numContainerObjects);
    for (const auto* req_obj : req_span) {
        if (req_obj == nullptr || req_obj->obj == nullptr) {
            continue;
        }
        const std::int32_t needed = std::max<std::int32_t>(1, std::abs(req_obj->count));
        const std::int32_t owned = CountPlayerNonQuestItem(inv, req_obj->obj);
        if (owned < needed) {
            const char* mn = req_obj->obj->GetName();
            return rejected("Insufficient " + ((mn != nullptr && mn[0] != '\0') ? StripLeadingDollar(mn) : std::string("materials")) + ".");
        }
    }

    auto* active_workbench = ResolveActiveWorkbenchRef(player, g_active_blacksmith_station.furniture_ref);
    ArmVirtualCraftingProxy(player, active_workbench);
    for (const auto* req_obj : req_span) {
        if (req_obj == nullptr || req_obj->obj == nullptr) {
            continue;
        }
        const std::int32_t needed = std::max<std::int32_t>(1, std::abs(req_obj->count));
        const auto mat_it = inv.find(req_obj->obj->As<RE::TESBoundObject>());
        const std::int32_t before = (mat_it != inv.end()) ? std::max<std::int32_t>(0, mat_it->second.first) : needed;
        player->RemoveItem(req_obj->obj, needed, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        ConsumePlayerStolenCount(req_obj->obj, (mat_it != inv.end() ? mat_it->second.second.get() : nullptr), before, needed, std::max(0, before - needed), false);
    }

    RE::ExtraDataList* applied_xlist = nullptr;
    if (live_entry != nullptr) {
        if (target_xlist != nullptr) {
            const std::int32_t stack_count = std::max<std::int32_t>(1, target_xlist->GetCount());
            if (stack_count > 1) {
                target_xlist->SetCount(static_cast<std::uint16_t>(stack_count - 1));
                auto* split_list = new RE::ExtraDataList();
                split_list->Add(new RE::ExtraHealth(target_health));
                if (auto* owner = target_xlist->GetOwner(); owner != nullptr) {
                    split_list->SetOwner(owner);
                }
                live_entry->AddExtraList(split_list);
                applied_xlist = split_list;
            } else {
                if (auto* ex_health = target_xlist->GetByType<RE::ExtraHealth>(); ex_health != nullptr) {
                    ex_health->health = target_health;
                } else {
                    target_xlist->Add(new RE::ExtraHealth(target_health));
                }
                applied_xlist = target_xlist;
            }
        } else {
            auto* new_list = new RE::ExtraDataList();
            new_list->Add(new RE::ExtraHealth(target_health));
            live_entry->AddExtraList(new_list);
            applied_xlist = new_list;
        }
    } else {
        player->RemoveItem(target_form, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        auto* new_list = new RE::ExtraDataList();
        new_list->Add(new RE::ExtraHealth(target_health));
        player->AddObjectToContainer(target_form, new_list, 1, nullptr);
        applied_xlist = new_list;
    }

    if (applied_xlist != nullptr && applied_xlist->GetWorn()) {
        if (target_form->Is(RE::FormType::Armor)) {
            player->UpdateArmorAbility(target_form, applied_xlist);
        } else if (target_form->Is(RE::FormType::Weapon)) {
            player->UpdateWeaponAbility(target_form, applied_xlist, applied_xlist->HasType<RE::ExtraWornLeft>());
        }
    }

    float xp_gained = std::max(1.5F, std::sqrt(std::max(1.0F, static_cast<float>(target_form->GetGoldValue()))) * 0.45F);
    player->UseSkill(RE::ActorValue::kSmithing, xp_gained, nullptr);
    EmitNativeCraftedItemEvents(
        player,
        g_active_blacksmith_station.furniture_ref,
        target_form,
        false,
        false,
        false);

    PlayBlacksmithSound(g_active_blacksmith_station.station_kind);
    skyultra::InvalidateSharedGameDataCache();

    const char* raw_tier = TemperTierLabel(target_health);
    const std::string tier_label = raw_tier != nullptr ? std::string(raw_tier) : std::string("Improved");
    const std::int32_t new_stat = ComputeItemEngineStat(player, target_form, target_health);
    const char* raw_item_name = target_form->GetName();
    const std::string item_name = (raw_item_name != nullptr && raw_item_name[0] != '\0')
        ? StripLeadingDollar(raw_item_name)
        : std::string("Item");
    auto updated_snapshot = ReadBlacksmithSnapshot(Json::object());
    return {
        { "accepted", true },
        { "itemId", std::to_string(item_id) },
        { "recipeId", std::to_string(recipe_id) },
        { "itemName", item_name },
        { "newTier", tier_label },
        { "newStat", new_stat },
        { "xpGained", std::round(xp_gained * 10.0F) / 10.0F },
        { "message", "Improved " + item_name + " (" + tier_label + ")." },
        { "snapshot", std::move(updated_snapshot) }
    };
}

struct ActiveEnchantingStationState {
    bool active{ false };
    RE::FormID furniture_form_id{ 0 };
    RE::ObjectRefHandle furniture_ref{};
    std::string station_name{ "Arcane Enchanter" };
};

ActiveEnchantingStationState g_active_enchanting_station;

struct ActiveAlchemyStationState {
    bool active{ false };
    RE::FormID furniture_form_id{ 0 };
    RE::ObjectRefHandle furniture_ref{};
    std::string station_name{ "Alchemy Lab" };
};

ActiveAlchemyStationState g_active_alchemy_station;
std::atomic<bool> g_super_enchanting_enabled{ false };
std::unordered_map<std::uint32_t, std::string> g_enchantment_custom_names;
std::unordered_map<std::uint32_t, std::string> g_enchantment_original_names;

[[nodiscard]] float ReadPlayerEnchantingBaseSkillUnlocked(RE::PlayerCharacter* player) noexcept
{
    if (player == nullptr) {
        return 15.0F;
    }
    if (const auto* av_owner = player->AsActorValueOwner(); av_owner != nullptr) {
        return std::max(0.0F, av_owner->GetBaseActorValue(RE::ActorValue::kEnchanting));
    }
    return 15.0F;
}

[[nodiscard]] bool IsSuperEnchantingActiveUnlocked(RE::PlayerCharacter* player) noexcept
{
    return g_super_enchanting_enabled.load(std::memory_order_relaxed) &&
        ReadPlayerEnchantingBaseSkillUnlocked(player) >= 99.95F;
}

[[nodiscard]] std::string ResolveEnchantmentDisplayNameUnlocked(
    const RE::EnchantmentItem* ench,
    bool* out_is_custom = nullptr,
    std::string* out_original = nullptr)
{
    if (out_is_custom != nullptr) {
        *out_is_custom = false;
    }
    if (out_original != nullptr) {
        out_original->clear();
    }
    if (ench == nullptr) {
        if (out_original != nullptr) {
            *out_original = "Enchantment";
        }
        return "Enchantment";
    }
    const auto eid = ench->GetFormID();
    const char* raw_now = ench->GetName();
    std::string live_name = (raw_now != nullptr && raw_now[0] != '\0')
        ? StripLeadingDollar(raw_now)
        : std::string("Enchantment");
    std::string orig_name = live_name;
    if (const auto oit = g_enchantment_original_names.find(eid);
        oit != g_enchantment_original_names.end() && !oit->second.empty()) {
        orig_name = oit->second;
    }
    if (out_original != nullptr) {
        *out_original = orig_name;
    }
    if (const auto cit = g_enchantment_custom_names.find(eid);
        cit != g_enchantment_custom_names.end() && !cit->second.empty()) {
        if (out_is_custom != nullptr) {
            *out_is_custom = true;
        }
        return cit->second;
    }
    return live_name;
}

void ApplyCustomNameToEnchantmentUnlocked(
    RE::EnchantmentItem* ench,
    const std::string& custom_name)
{
    if (ench == nullptr) {
        return;
    }
    const auto eid = ench->GetFormID();
    if (!g_enchantment_original_names.contains(eid)) {
        const char* raw = ench->GetName();
        if (raw == nullptr || raw[0] == '\0') {
            if (const auto* fn = ench->As<RE::TESFullName>(); fn != nullptr) {
                raw = fn->GetFullName();
            }
        }
        if (raw != nullptr && raw[0] != '\0') {
            g_enchantment_original_names.emplace(eid, StripLeadingDollar(raw));
        }
    }
    if (custom_name.empty()) {
        g_enchantment_custom_names.erase(eid);
        if (const auto oit = g_enchantment_original_names.find(eid);
            oit != g_enchantment_original_names.end() && !oit->second.empty()) {
            if (auto* fn = ench->As<RE::TESFullName>(); fn != nullptr) {
                fn->SetFullName(oit->second.c_str());
            }
        }
        return;
    }
    g_enchantment_custom_names[eid] = custom_name;
    if (auto* fn = ench->As<RE::TESFullName>(); fn != nullptr) {
        fn->SetFullName(custom_name.c_str());
    }
}

[[nodiscard]] float ComputeDisenchantSkillUsePoints(const RE::EnchantmentItem* ench) noexcept
{
    const float base_val = ench != nullptr ? std::max(0.0F, ench->CalculateTotalGoldValue()) : 0.0F;
    return std::clamp(0.5F + (base_val * 0.0025F), 0.5F, 4.0F);
}

[[nodiscard]] bool ClassifyEnchantingFurniture(const RE::TESFurniture* furniture) noexcept
{
    if (furniture == nullptr) {
        return false;
    }
    if (furniture->HasKeywordString("DLC2StaffEnchanter") ||
        furniture->HasKeywordString("DLC2SpiderImbuing")) {
        return false;
    }
    const auto bench_type = furniture->workBenchData.benchType.get();
    const auto uses_skill = furniture->workBenchData.usesSkill.get();
    return bench_type == RE::TESFurniture::WorkBenchData::BenchType::kEnchanting ||
        uses_skill == RE::ActorValue::kEnchanting ||
        furniture->HasKeywordString("CraftingEnchanting") ||
        furniture->HasKeywordID(0x000C554EU);
}

[[nodiscard]] bool HasDisallowEnchantingKeyword(const RE::TESForm* form) noexcept
{
    if (form == nullptr) {
        return false;
    }
    if (auto* defaults = RE::BGSDefaultObjectManager::GetSingleton(); defaults != nullptr) {
        if (auto** disallow_kw = defaults->GetObject<RE::BGSKeyword>(RE::DefaultObjectID::kKeywordDisallowEnchanting);
            disallow_kw != nullptr && *disallow_kw != nullptr) {
            if (const auto* kw_form = form->As<RE::BGSKeywordForm>(); kw_form != nullptr && kw_form->HasKeyword(*disallow_kw)) {
                return true;
            }
        }
    }
    const auto* kw_form = form->As<RE::BGSKeywordForm>();
    return kw_form != nullptr &&
        (kw_form->HasKeywordID(0x000C27BDU) || kw_form->HasKeywordString("MagicDisallowEnchanting"));
}

[[nodiscard]] bool IsReusableSoulGem(const RE::TESSoulGem* gem) noexcept
{
    if (gem == nullptr) {
        return false;
    }
    const auto id = gem->GetFormID();
    if (id == 0x00063B27U || id == 0x00063B29U || gem->linkedSoulGem != nullptr) {
        return true;
    }
    if (auto* defaults = RE::BGSDefaultObjectManager::GetSingleton(); defaults != nullptr) {
        if (auto** reuse_kw = defaults->GetObject<RE::BGSKeyword>(RE::DefaultObjectID::kKeywordReusableSoulGem);
            reuse_kw != nullptr && *reuse_kw != nullptr && gem->HasKeyword(*reuse_kw)) {
            return true;
        }
    }
    return gem->HasKeywordString("ReusableSoulGem");
}

[[nodiscard]] float ReadGameSettingFloat(const char* name, float fallback) noexcept
{
    if (name == nullptr) {
        return fallback;
    }
    if (auto* coll = RE::GameSettingCollection::GetSingleton(); coll != nullptr) {
        if (auto* setting = coll->GetSetting(name); setting != nullptr && setting->GetType() == RE::Setting::Type::kFloat) {
            const float val = setting->GetFloat();
            if (std::isfinite(val)) {
                return val;
            }
        } else if (setting != nullptr && setting->GetType() == RE::Setting::Type::kInteger) {
            return static_cast<float>(setting->GetInteger());
        } else if (setting != nullptr && setting->GetType() == RE::Setting::Type::kUnsignedInteger) {
            return static_cast<float>(setting->GetUnsignedInteger());
        }
    }
    return fallback;
}

[[nodiscard]] float SoulLevelPoints(RE::SOUL_LEVEL level) noexcept
{
    switch (level) {
    case RE::SOUL_LEVEL::kPetty: return std::max(1.0F, ReadGameSettingFloat("iSoulLevelValuePetty", 250.0F));
    case RE::SOUL_LEVEL::kLesser: return std::max(1.0F, ReadGameSettingFloat("iSoulLevelValueLesser", 500.0F));
    case RE::SOUL_LEVEL::kCommon: return std::max(1.0F, ReadGameSettingFloat("iSoulLevelValueCommon", 1000.0F));
    case RE::SOUL_LEVEL::kGreater: return std::max(1.0F, ReadGameSettingFloat("iSoulLevelValueGreater", 1500.0F));
    case RE::SOUL_LEVEL::kGrand: return std::max(1.0F, ReadGameSettingFloat("iSoulLevelValueGrand", 3000.0F));
    default: return 0.0F;
    }
}

[[nodiscard]] float ComputeEnchantingSkillFactor(float eff_skill) noexcept
{
    if (!std::isfinite(eff_skill) || eff_skill < 0.0F) {
        return 1.0F;
    }
    float gmst_factor = ReadGameSettingFloat("fEnchantingSkillFactor", 1.25F);
    if (gmst_factor < 1.0F || gmst_factor > 5.0F) {
        gmst_factor = 1.25F;
    }
    const float raw = 1.0F + (eff_skill / 100.0F) * (gmst_factor - 1.0F) * ((eff_skill - 14.0F) / 85.0F);
    return std::max(1.0F, raw);
}

[[nodiscard]] std::int32_t ComputeEnchantingMaxSlots(RE::PlayerCharacter* player) noexcept
{
    if (player == nullptr) {
        return 1;
    }
    float allowed = 1.0F;
    RE::BGSEntryPoint::HandleEntryPoint(
        RE::BGSEntryPoint::ENTRY_POINT::kModNumberAppliedEnchantmentsAllowed,
        player,
        &allowed);
    std::int32_t count = std::max<std::int32_t>(1, static_cast<std::int32_t>(std::floor(allowed + 0.01F)));
    if (auto* extra_perk = RE::TESForm::LookupByID<RE::BGSPerk>(0x00058F7FU);
        extra_perk != nullptr && player->HasPerk(extra_perk)) {
        count = std::max<std::int32_t>(count, 2);
    }
    return std::clamp<std::int32_t>(count, 1, 4);
}

[[nodiscard]] std::string FormatEffectTemplateDescription(const RE::Effect* eff)
{
    if (eff == nullptr || eff->baseEffect == nullptr) {
        return {};
    }
    const char* raw = eff->baseEffect->magicItemDescription.c_str();
    return raw != nullptr ? std::string(raw) : std::string{};
}

[[nodiscard]] std::string FormatEnchantEffectText(
    const RE::Effect* eff,
    float mag,
    std::uint32_t dur)
{
    if (eff == nullptr || eff->baseEffect == nullptr) {
        return {};
    }
    const std::string mag_str = std::to_string(static_cast<int>(std::round(std::max(0.0F, mag))));
    std::string desc = FormatEffectTemplateDescription(eff);
    for (std::size_t pos = 0; (pos = desc.find('<', pos)) != std::string::npos;) {
        const auto end = desc.find('>', pos);
        if (end == std::string::npos) {
            break;
        }
        const auto token = LowerAscii(desc.substr(pos + 1, end - pos - 1));
        const std::string repl = token == "mag"
            ? mag_str
            : (token == "dur" ? std::to_string(dur) : (token == "area" ? std::to_string(eff->GetArea()) : std::string{}));
        desc.replace(pos, end - pos + 1, repl);
        pos += repl.size();
    }
    if (desc.empty()) {
        if (mag > 0.0F && dur > 0U) {
            desc = mag_str + " pts for " + std::to_string(dur) + "s";
        } else if (mag > 0.0F) {
            desc = "+" + mag_str + " pts";
        } else if (dur > 0U) {
            desc = std::to_string(dur) + "s";
        }
    }
    return desc;
}

[[nodiscard]] std::string_view MagicEffectSchoolLabel(
    const RE::EffectSetting* base,
    std::string_view fallback) noexcept
{
    if (base == nullptr) {
        return fallback;
    }
    switch (base->GetMagickSkill()) {
    case RE::ActorValue::kAlteration: return "Alteration";
    case RE::ActorValue::kConjuration: return "Conjuration";
    case RE::ActorValue::kDestruction: return "Destruction";
    case RE::ActorValue::kIllusion: return "Illusion";
    case RE::ActorValue::kRestoration: return "Restoration";
    default: return fallback;
    }
}

[[nodiscard]] std::string_view EnchantmentSchoolLabel(const RE::EnchantmentItem* ench) noexcept
{
    if (ench == nullptr) {
        return "Enchanting";
    }
    if (auto* costliest = ench->GetCostliestEffectItem(); costliest != nullptr && costliest->baseEffect != nullptr) {
        return MagicEffectSchoolLabel(costliest->baseEffect, "Enchanting");
    }
    return "Enchanting";
}

[[nodiscard]] std::string KeywordSlotFriendlyLabel(std::string_view edid)
{
    if (edid.empty()) return {};
    const auto lower = LowerAscii(edid);
    if (lower.find("cuirass") != std::string::npos || lower.find("clothingbody") != std::string::npos) return "Chest / Body";
    if (lower.find("helmet") != std::string::npos || lower.find("clothinghead") != std::string::npos) return "Head / Helm";
    if (lower.find("circlet") != std::string::npos || lower.find("clothingcirclet") != std::string::npos) return "Circlet";
    if (lower.find("gauntlet") != std::string::npos || lower.find("clothinghands") != std::string::npos) return "Hands / Bracers";
    if (lower.find("boots") != std::string::npos || lower.find("clothingfeet") != std::string::npos) return "Feet / Boots";
    if (lower.find("shield") != std::string::npos) return "Shield";
    if (lower.find("ring") != std::string::npos) return "Ring";
    if (lower.find("necklace") != std::string::npos) return "Necklace";
    if (lower.find("weapon") != std::string::npos) return "Weapon";
    return std::string(edid);
}

void ExtractWornRestrictionMetadata(
    const RE::BGSListForm* restrictions,
    std::vector<std::string>& out_ids,
    std::vector<std::string>& out_labels)
{
    out_ids.clear();
    out_labels.clear();
    if (restrictions == nullptr) {
        return;
    }
    std::unordered_set<std::string> seen_labels;
    auto push_kw = [&](const RE::TESForm* f) {
        if (f == nullptr) {
            return;
        }
        out_ids.push_back(std::to_string(f->GetFormID()));
        const char* raw_edid = f->GetFormEditorID();
        const std::string friendly = (raw_edid != nullptr && raw_edid[0] != '\0')
            ? KeywordSlotFriendlyLabel(raw_edid)
            : std::string{};
        if (!friendly.empty() && seen_labels.insert(friendly).second) {
            out_labels.push_back(friendly);
        }
    };
    restrictions->ForEachForm([&](RE::TESForm* form) {
        if (form == nullptr) {
            return RE::BSContainer::ForEachResult::kContinue;
        }
        if (const auto* nested = form->As<RE::BGSListForm>(); nested != nullptr) {
            nested->ForEachForm([&](RE::TESForm* inner) {
                push_kw(inner);
                return RE::BSContainer::ForEachResult::kContinue;
            });
        } else {
            push_kw(form);
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
}

[[nodiscard]] bool EnchantmentFitsItem(
    const RE::EnchantmentItem* ench,
    const RE::TESBoundObject* item,
    bool unrestricted_slots = false) noexcept
{
    if (ench == nullptr || item == nullptr) {
        return false;
    }
    const bool is_weapon = item->Is(RE::FormType::Weapon);
    const bool is_armor = item->Is(RE::FormType::Armor);
    if (!is_weapon && !is_armor) {
        return false;
    }
    const bool ench_is_weapon = ench->GetCastingType() != RE::MagicSystem::CastingType::kConstantEffect;
    if (is_weapon != ench_is_weapon) {
        return false;
    }
    if (is_weapon) {
        const auto* weap = item->As<RE::TESObjectWEAP>();
        if (weap == nullptr || weap->IsStaff()) {
            return false;
        }
    }
    if (unrestricted_slots || ench->data.wornRestrictions == nullptr) {
        return true;
    }
    std::vector<std::string> ids;
    std::vector<std::string> labels;
    ExtractWornRestrictionMetadata(ench->data.wornRestrictions, ids, labels);
    if (ids.empty()) {
        return true;
    }
    if (item->HasKeywordInList(ench->data.wornRestrictions, false)) {
        return true;
    }
    const auto* kw_form = item->As<RE::BGSKeywordForm>();
    if (kw_form == nullptr) {
        return false;
    }
    for (const auto& id_str : ids) {
        std::uint32_t fid = 0;
        if (std::from_chars(id_str.data(), id_str.data() + id_str.size(), fid).ec == std::errc{} && fid != 0) {
            if (kw_form->HasKeywordID(fid)) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] float ComputePerkPowerMultiplierForEffect(
    RE::PlayerCharacter* player,
    RE::EnchantmentItem* ench,
    RE::TESForm* target_item) noexcept
{
    if (player == nullptr || ench == nullptr || target_item == nullptr) {
        return 1.0F;
    }
    float probe = 100.0F;
    RE::BGSEntryPoint::HandleEntryPoint(
        RE::BGSEntryPoint::ENTRY_POINT::kModEnchantmentPower,
        player,
        ench,
        target_item,
        &probe);
    if (!std::isfinite(probe) || probe <= 0.0F) {
        return 1.0F;
    }
    return probe / 100.0F;
}

void BuildScaledEnchantmentEffects(
    RE::PlayerCharacter* player,
    const std::vector<RE::EnchantmentItem*>& selected_enchs,
    RE::TESBoundObject* target_item,
    float soul_points,
    float charge_slider,
    RE::BSTArray<RE::Effect>& out_effects,
    float& out_total_cost,
    std::int32_t& out_uses)
{
    out_effects.clear();
    out_total_cost = 0.0F;
    out_uses = 0;
    if (player == nullptr || target_item == nullptr) {
        return;
    }
    const bool is_weapon = target_item->Is(RE::FormType::Weapon);
    const auto* av_owner = player->AsActorValueOwner();
    const float eff_skill = av_owner != nullptr ? std::max(0.0F, av_owner->GetActorValue(RE::ActorValue::kEnchanting)) : 15.0F;
    const float skill_factor = ComputeEnchantingSkillFactor(eff_skill);
    const float armor_soul_scale = std::clamp(soul_points / 3000.0F, 0.05F, 2.0F);
    const float clamped_slider = std::clamp(charge_slider, 0.15F, 1.0F);

    std::uint32_t total_eff_count = 0;
    for (auto* ench : selected_enchs) {
        if (ench == nullptr) {
            continue;
        }
        for (const auto* src : ench->effects) {
            if (src != nullptr && src->baseEffect != nullptr) {
                ++total_eff_count;
            }
        }
    }
    out_effects.reserve(total_eff_count);

    for (auto* ench : selected_enchs) {
        if (ench == nullptr) {
            continue;
        }
        const float perk_mult = ComputePerkPowerMultiplierForEffect(player, ench, target_item);
        for (const auto* src : ench->effects) {
            if (src == nullptr || src->baseEffect == nullptr) {
                continue;
            }
            RE::Effect& created = out_effects.emplace_back();
            created.Copy(src);
            const auto flags = src->baseEffect->data.flags;
            const bool no_mag = flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoMagnitude);
            const bool power_mag = flags.any(RE::EffectSetting::EffectSettingData::Flag::kPowerAffectsMagnitude);
            const bool power_dur = flags.any(RE::EffectSetting::EffectSettingData::Flag::kPowerAffectsDuration);

            if (!no_mag && src->GetMagnitude() > 0.0F) {
                float mag = src->GetMagnitude();
                if (power_mag || !power_dur) {
                    mag *= skill_factor * perk_mult;
                    mag *= is_weapon ? clamped_slider : armor_soul_scale;
                }
                created.SetMagnitude(std::max(1.0F, std::floor(mag + 0.5F)));
            }
            if (power_dur && !power_mag && src->GetDuration() > 0U) {
                float dur = static_cast<float>(src->GetDuration()) * skill_factor * perk_mult;
                dur *= is_weapon ? clamped_slider : armor_soul_scale;
                created.SetDuration(std::max<std::int32_t>(1, static_cast<std::int32_t>(std::floor(dur + 0.5F))));
            }
            out_total_cost += std::max(0.0F, created.cost);
        }
    }
    if (is_weapon) {
        out_uses = out_total_cost > 0.01F
            ? std::max<std::int32_t>(1, static_cast<std::int32_t>(std::floor(soul_points / out_total_cost)))
            : std::max<std::int32_t>(1, static_cast<std::int32_t>(std::lround(soul_points)));
    }
}

Json ReadEnchantingSnapshot(const Json& options)
{
    (void)options;
    const auto unavailable = [](std::string message) -> Json {
        return {
            { "available", false },
            { "stationName", "Arcane Enchanter" },
            { "enchantableItems", Json::array() },
            { "disenchantableItems", Json::array() },
            { "knownEnchantments", Json::array() },
            { "soulGems", Json::array() },
            { "message", std::move(message) }
        };
    };

    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* data_handler = RE::TESDataHandler::GetSingleton();
    if (player == nullptr || data_handler == nullptr) {
        return unavailable("Player enchanting state is unavailable.");
    }
    if (!g_active_enchanting_station.active) {
        if (!skyultra::CaptureActiveEnchantingStation()) {
            return unavailable("No Arcane Enchanter station is currently active.");
        }
    }
    const ActiveEnchantingStationState station = g_active_enchanting_station;

    const auto* av_owner = player->AsActorValueOwner();
    const float eff_skill = av_owner != nullptr ? std::max(0.0F, av_owner->GetActorValue(RE::ActorValue::kEnchanting)) : 15.0F;
    const float base_skill = av_owner != nullptr ? std::max(0.0F, av_owner->GetBaseActorValue(RE::ActorValue::kEnchanting)) : 15.0F;
    const float skill_mult = ComputeEnchantingSkillFactor(eff_skill);
    const bool super_unlocked = base_skill >= 99.95F;
    const bool super_switch = g_super_enchanting_enabled.load(std::memory_order_relaxed);
    const bool super_active = super_unlocked && super_switch;
    const std::int32_t max_ench_slots = super_active ? 64 : ComputeEnchantingMaxSlots(player);
    const float max_carry = av_owner != nullptr
        ? std::max(1.0F, av_owner->GetActorValue(RE::ActorValue::kCarryWeight))
        : 300.0F;
    const float cur_carry = av_owner != nullptr
        ? std::max(0.0F, av_owner->GetActorValue(RE::ActorValue::kInventoryWeight))
        : std::max(0.0F, player->GetWeightInContainer());

    const auto inv = player->GetInventory();

    Json enchantable_json = Json::array();
    Json disenchantable_json = Json::array();
    Json soul_gems_json = Json::array();
    RE::TESBoundObject* sample_weapon = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00012EB7U);
    RE::TESBoundObject* sample_armor = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00013910U);

    for (const auto& [bound_obj, inv_pair] : inv) {
        if (bound_obj == nullptr || inv_pair.first <= 0) {
            continue;
        }
        auto* entry_data = inv_pair.second.get();
        const std::int32_t total_count = inv_pair.first;

        if (bound_obj->Is(RE::FormType::SoulGem)) {
            auto* gem = bound_obj->As<RE::TESSoulGem>();
            if (gem == nullptr) {
                continue;
            }
            const char* raw_gname = gem->GetName();
            if (raw_gname == nullptr || raw_gname[0] == '\0' || IsMissingOrUnnamedDisplayName(raw_gname)) {
                continue;
            }
            const bool reusable = IsReusableSoulGem(gem);
            const auto push_gem_stack = [&](std::int32_t stack_idx, std::int32_t stack_cnt, RE::ExtraDataList* x_list, RE::SOUL_LEVEL soul) {
                if (stack_cnt <= 0 || soul == RE::SOUL_LEVEL::kNone) {
                    return;
                }
                const float pts = SoulLevelPoints(soul);
                if (pts <= 0.0F) {
                    return;
                }
                const char* slabel = SoulLevelLabel(soul);
                const char* disp = x_list != nullptr ? x_list->GetDisplayName(bound_obj) : raw_gname;
                const std::string gname = (disp != nullptr && disp[0] != '\0') ? StripLeadingDollar(disp) : StripLeadingDollar(raw_gname);
                soul_gems_json.push_back({
                    { "id", std::to_string(gem->GetFormID()) + ":" + std::to_string(stack_idx) },
                    { "gemId", std::to_string(gem->GetFormID()) },
                    { "stackIndex", stack_idx },
                    { "name", gname },
                    { "soulLevel", static_cast<int>(soul) },
                    { "soulLabel", slabel != nullptr ? std::string(slabel) : std::string("Filled") },
                    { "soulPoints", std::round(pts) },
                    { "armorScale", std::clamp(pts / 3000.0F, 0.05F, 2.0F) },
                    { "count", stack_cnt },
                    { "value", std::max(0, gem->GetGoldValue()) },
                    { "weight", std::round(std::max(0.0F, gem->GetWeight()) * 100.0F) / 100.0F },
                    { "isReusable", reusable }
                });
            };
            std::int32_t tracked = 0;
            std::int32_t idx = 0;
            if (entry_data != nullptr && entry_data->extraLists != nullptr) {
                for (auto* x_list : *entry_data->extraLists) {
                    if (x_list == nullptr) {
                        continue;
                    }
                    const std::int32_t x_cnt = std::max<std::int32_t>(1, x_list->GetCount());
                    tracked += x_cnt;
                    RE::SOUL_LEVEL sl = x_list->GetSoulLevel();
                    if (sl == RE::SOUL_LEVEL::kNone) {
                        if (const auto* ex_s = x_list->GetByType<RE::ExtraSoul>(); ex_s != nullptr) {
                            sl = ex_s->GetContainedSoul();
                        }
                    }
                    if (sl == RE::SOUL_LEVEL::kNone) {
                        sl = gem->GetContainedSoul();
                    }
                    push_gem_stack(idx++, x_cnt, x_list, sl);
                }
            }
            if (total_count > tracked) {
                push_gem_stack(-1, total_count - tracked, nullptr, gem->GetContainedSoul());
            }
            continue;
        }

        const bool is_weapon = bound_obj->Is(RE::FormType::Weapon);
        const bool is_armor = bound_obj->Is(RE::FormType::Armor);
        if (!is_weapon && !is_armor) {
            continue;
        }
        if (is_weapon) {
            if (const auto* weap = bound_obj->As<RE::TESObjectWEAP>(); weap == nullptr || weap->IsStaff()) {
                continue;
            }
            if (sample_weapon == nullptr) sample_weapon = bound_obj;
        } else {
            if (sample_armor == nullptr) sample_armor = bound_obj;
        }
        if (!IsPlayableNamedInventoryItem(bound_obj, entry_data, false)) {
            continue;
        }

        Json base_meta = PlayerItemJson(bound_obj, 1, false, nullptr, true);
        if (!base_meta.is_object()) {
            continue;
        }
        Json keyword_ids = Json::array();
        if (const auto* kw_form = bound_obj->As<RE::BGSKeywordForm>(); kw_form != nullptr) {
            kw_form->ForEachKeyword([&](const RE::BGSKeyword* kw) {
                if (kw != nullptr) {
                    keyword_ids.push_back(std::to_string(kw->GetFormID()));
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
        const bool item_disallowed = HasDisallowEnchantingKeyword(bound_obj);
        const auto* base_ench_form = bound_obj->As<RE::TESEnchantableForm>();
        RE::EnchantmentItem* base_ench = base_ench_form != nullptr ? base_ench_form->formEnchanting : nullptr;

        const auto process_gear_stack = [&](std::int32_t stack_idx, std::int32_t stack_cnt, RE::ExtraDataList* x_list) {
            if (stack_cnt <= 0) {
                return;
            }
            const bool is_quest_item = IsActiveRunningQuestObject(x_list != nullptr ? nullptr : entry_data, x_list);
            const auto* ex_ench = x_list != nullptr ? x_list->GetByType<RE::ExtraEnchantment>() : nullptr;
            RE::EnchantmentItem* stack_ench = ex_ench != nullptr && ex_ench->enchantment != nullptr
                ? ex_ench->enchantment
                : base_ench;
            const bool equipped = x_list != nullptr &&
                (x_list->HasType<RE::ExtraWorn>() || x_list->HasType<RE::ExtraWornLeft>());
            const char* raw_disp = x_list != nullptr ? x_list->GetDisplayName(bound_obj) : bound_obj->GetName();
            if (raw_disp == nullptr || raw_disp[0] == '\0' || IsMissingOrUnnamedDisplayName(raw_disp)) {
                raw_disp = bound_obj->GetName();
            }
            if (raw_disp == nullptr || raw_disp[0] == '\0' || IsMissingOrUnnamedDisplayName(raw_disp)) {
                return;
            }
            const std::string item_name = StripLeadingDollar(raw_disp);
            const auto* ex_health = x_list != nullptr ? x_list->GetByType<RE::ExtraHealth>() : nullptr;
            const float cur_health = (ex_health != nullptr && std::isfinite(ex_health->health)) ? ex_health->health : 1.0F;
            const std::int32_t cur_stat = ComputeItemEngineStat(player, bound_obj, cur_health);

            if (stack_ench == nullptr) {
                if (item_disallowed && !super_active) {
                    return;
                }
                Json entry{
                    { "id", std::to_string(bound_obj->GetFormID()) + ":" + std::to_string(stack_idx) },
                    { "itemId", std::to_string(bound_obj->GetFormID()) },
                    { "stackIndex", stack_idx },
                    { "name", item_name },
                    { "itemKind", is_weapon ? "weapon" : "armor" },
                    { "category", std::string(ItemCategory(bound_obj->GetFormType())) },
                    { "count", stack_cnt },
                    { "value", std::max(0, bound_obj->GetGoldValue()) },
                    { "weight", std::round(std::max(0.0F, bound_obj->GetWeight()) * 100.0F) / 100.0F },
                    { "equipped", equipped },
                    { "keywordIds", keyword_ids }
                };
                if (is_weapon) entry["damage"] = cur_stat;
                else entry["armorRating"] = cur_stat;
                if (base_meta.contains("weaponType")) entry["weaponType"] = base_meta["weaponType"];
                if (base_meta.contains("armorTypeLabel")) entry["armorTypeLabel"] = base_meta["armorTypeLabel"];
                enchantable_json.push_back(std::move(entry));
            } else {
                RE::EnchantmentItem* base_known_form = stack_ench->data.baseEnchantment != nullptr
                    ? stack_ench->data.baseEnchantment
                    : stack_ench;
                const bool is_player_created = stack_ench->IsDynamicForm() ||
                    base_known_form == nullptr || base_known_form->IsDynamicForm();
                const bool already_known = !is_player_created &&
                    ((base_known_form->formFlags & RE::TESForm::RecordFlags::kKnown) != 0);
                const bool base_disallowed = item_disallowed || HasDisallowEnchantingKeyword(stack_ench) ||
                    (base_known_form != nullptr && HasDisallowEnchantingKeyword(base_known_form));
                const bool disallowed = !super_active && base_disallowed;
                const bool can_disenchant = !is_player_created && !already_known && !disallowed && !is_quest_item;

                std::string block_reason;
                if (is_quest_item) block_reason = "Active quest items cannot be disenchanted until the quest is over.";
                else if (is_player_created) block_reason = "Custom player enchantments cannot be disenchanted.";
                else if (disallowed) block_reason = "Unique enchantment locked — requires Enchanting 100 & Super Enchanting in Settings.";
                else if (already_known) block_reason = "Enchantment already known.";

                RE::EnchantmentItem* named_ench = (base_known_form != nullptr &&
                    (base_known_form->GetName()[0] != '\0' || g_enchantment_custom_names.contains(base_known_form->GetFormID())))
                    ? base_known_form
                    : stack_ench;
                const std::string ench_name = ResolveEnchantmentDisplayNameUnlocked(named_ench);

                Json eff_arr = Json::array();
                for (const auto* eff : stack_ench->effects) {
                    if (eff == nullptr || eff->baseEffect == nullptr) continue;
                    if (eff->baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) continue;
                    const char* ename = eff->baseEffect->GetFullName();
                    if (ename == nullptr || ename[0] == '\0') ename = eff->baseEffect->GetName();
                    if (ename == nullptr || ename[0] == '\0') continue;
                    eff_arr.push_back({
                        { "name", StripLeadingDollar(ename) },
                        { "description", FormatEnchantEffectText(eff, eff->GetMagnitude(), eff->GetDuration()) },
                        { "magnitude", eff->GetMagnitude() },
                        { "duration", eff->GetDuration() }
                    });
                }
                const float est_xp = ComputeDisenchantSkillUsePoints(stack_ench);
                Json dis_entry{
                    { "id", std::to_string(bound_obj->GetFormID()) + ":" + std::to_string(stack_idx) },
                    { "itemId", std::to_string(bound_obj->GetFormID()) },
                    { "stackIndex", stack_idx },
                    { "name", item_name },
                    { "itemKind", is_weapon ? "weapon" : "armor" },
                    { "category", std::string(ItemCategory(bound_obj->GetFormType())) },
                    { "count", stack_cnt },
                    { "value", std::max(0, bound_obj->GetGoldValue()) },
                    { "weight", std::round(std::max(0.0F, bound_obj->GetWeight()) * 100.0F) / 100.0F },
                    { "equipped", equipped },
                    { "enchantmentId", std::to_string(stack_ench->GetFormID()) },
                    { "baseEnchantmentId", base_known_form != nullptr ? std::to_string(base_known_form->GetFormID()) : std::to_string(stack_ench->GetFormID()) },
                    { "enchantmentName", ench_name },
                    { "effects", std::move(eff_arr) },
                    { "alreadyKnown", already_known },
                    { "isDisallowed", disallowed },
                    { "isUniqueLocked", base_disallowed },
                    { "isQuestItem", is_quest_item },
                    { "isPlayerCreated", is_player_created },
                    { "canDisenchant", can_disenchant },
                    { "blockReason", block_reason },
                    { "estimatedXp", std::round(est_xp * 10.0F) / 10.0F }
                };
                if (is_weapon) dis_entry["damage"] = cur_stat;
                else dis_entry["armorRating"] = cur_stat;
                if (base_meta.contains("weaponType")) dis_entry["weaponType"] = base_meta["weaponType"];
                if (base_meta.contains("armorTypeLabel")) dis_entry["armorTypeLabel"] = base_meta["armorTypeLabel"];
                disenchantable_json.push_back(std::move(dis_entry));
            }
        };

        std::int32_t tracked = 0;
        std::int32_t idx = 0;
        if (entry_data != nullptr && entry_data->extraLists != nullptr) {
            for (auto* x_list : *entry_data->extraLists) {
                if (x_list == nullptr) continue;
                const std::int32_t x_cnt = std::max<std::int32_t>(1, x_list->GetCount());
                tracked += x_cnt;
                process_gear_stack(idx++, x_cnt, x_list);
            }
        }
        if (total_count > tracked) {
            process_gear_stack(-1, total_count - tracked, nullptr);
        }
    }

    Json known_enchs_json = Json::array();
    std::unordered_map<RE::FormID, RE::EnchantmentItem*> best_known_by_base;
    for (auto* ench : data_handler->GetFormArray<RE::EnchantmentItem>()) {
        if (ench == nullptr || ench->IsDeleted() || ench->IsDynamicForm()) {
            continue;
        }
        if (ench->GetSpellType() != RE::MagicSystem::SpellType::kEnchantment) {
            continue;
        }
        if ((ench->formFlags & RE::TESForm::RecordFlags::kKnown) == 0 ||
            (!super_active && HasDisallowEnchantingKeyword(ench))) {
            continue;
        }
        auto* base_ench = ench->data.baseEnchantment != nullptr ? ench->data.baseEnchantment : ench;
        if (base_ench->IsDynamicForm() ||
            (base_ench->formFlags & RE::TESForm::RecordFlags::kKnown) == 0 ||
            (!super_active && HasDisallowEnchantingKeyword(base_ench))) {
            continue;
        }
        const char* bname = base_ench->GetName();
        if ((bname == nullptr || bname[0] == '\0' || IsMissingOrUnnamedDisplayName(bname)) &&
            !g_enchantment_custom_names.contains(base_ench->GetFormID())) {
            continue;
        }
        if (!best_known_by_base.contains(base_ench->GetFormID())) {
            best_known_by_base.emplace(base_ench->GetFormID(), base_ench);
        }
    }

    for (const auto& [base_id, ench] : best_known_by_base) {
        bool is_custom_named = false;
        std::string orig_ename;
        const std::string ename = ResolveEnchantmentDisplayNameUnlocked(ench, &is_custom_named, &orig_ename);
        const bool is_weapon_ench = ench->GetCastingType() != RE::MagicSystem::CastingType::kConstantEffect;
        std::vector<std::string> rest_ids;
        std::vector<std::string> rest_labels;
        ExtractWornRestrictionMetadata(ench->data.wornRestrictions, rest_ids, rest_labels);

        RE::TESForm* sample_target = is_weapon_ench
            ? static_cast<RE::TESForm*>(sample_weapon)
            : static_cast<RE::TESForm*>(sample_armor);
        const float perk_mult = ComputePerkPowerMultiplierForEffect(player, ench, sample_target);

        Json effs_json = Json::array();
        float base_cost_sum = 0.0F;
        for (const auto* eff : ench->effects) {
            if (eff == nullptr || eff->baseEffect == nullptr) continue;
            if (eff->baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) continue;
            const auto flags = eff->baseEffect->data.flags;
            const char* mname = eff->baseEffect->GetFullName();
            if (mname == nullptr || mname[0] == '\0') mname = eff->baseEffect->GetName();
            if (mname == nullptr || mname[0] == '\0') continue;
            base_cost_sum += std::max(0.0F, eff->cost);
            effs_json.push_back({
                { "name", StripLeadingDollar(mname) },
                { "descriptionTemplate", FormatEffectTemplateDescription(eff) },
                { "baseMagnitude", eff->GetMagnitude() },
                { "baseDuration", eff->GetDuration() },
                { "area", eff->GetArea() },
                { "powerAffectsMagnitude", flags.any(RE::EffectSetting::EffectSettingData::Flag::kPowerAffectsMagnitude) },
                { "powerAffectsDuration", flags.any(RE::EffectSetting::EffectSettingData::Flag::kPowerAffectsDuration) },
                { "noMagnitude", flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoMagnitude) },
                { "perkBonusMult", std::round(perk_mult * 1000.0F) / 1000.0F }
            });
        }
        if (effs_json.empty()) {
            continue;
        }
        known_enchs_json.push_back({
            { "id", std::to_string(base_id) },
            { "name", ename },
            { "originalName", orig_ename },
            { "isCustomNamed", is_custom_named },
            { "targetKind", is_weapon_ench ? "weapon" : "armor" },
            { "school", std::string(EnchantmentSchoolLabel(ench)) },
            { "baseCost", std::max(1.0F, base_cost_sum) },
            { "wornRestrictionIds", std::move(rest_ids) },
            { "wornRestrictionLabels", std::move(rest_labels) },
            { "effects", std::move(effs_json) }
        });
    }

    const auto* raw_pname = player->GetName();
    return {
        { "available", true },
        { "stationName", station.station_name },
        { "enchantingSkill", std::round(eff_skill * 10.0F) / 10.0F },
        { "enchantingBaseSkill", std::round(base_skill * 10.0F) / 10.0F },
        { "skillMultiplier", std::round(skill_mult * 1000.0F) / 1000.0F },
        { "maxEnchantments", max_ench_slots },
        { "hasExtraEffect", super_active || max_ench_slots >= 2 },
        { "superEnchantingUnlocked", super_unlocked },
        { "superEnchantingEnabled", super_switch },
        { "superEnchantingActive", super_active },
        { "unlimitedEnchantments", super_active },
        { "playerName", (raw_pname != nullptr && raw_pname[0] != '\0') ? std::string(raw_pname) : std::string("Dragonborn") },
        { "playerGold", std::max(0, player->GetGoldAmount()) },
        { "playerCarryWeight", { { "current", std::round(cur_carry * 10.0F) / 10.0F }, { "max", std::round(max_carry * 10.0F) / 10.0F } } },
        { "enchantableItems", std::move(enchantable_json) },
        { "disenchantableItems", std::move(disenchantable_json) },
        { "knownEnchantments", std::move(known_enchs_json) },
        { "soulGems", std::move(soul_gems_json) }
    };
}

Json PerformEnchantItem(const Json& options)
{
    const auto rejected = [](std::string message) -> Json {
        return { { "accepted", false }, { "message", std::move(message) } };
    };
    if (!options.is_object()) {
        return rejected("Enchanting parameters must be an object.");
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* created_mgr = RE::BGSCreatedObjectManager::GetSingleton();
    if (player == nullptr || created_mgr == nullptr) {
        return rejected("Enchanting engine state is unavailable.");
    }
    if (!g_active_enchanting_station.active && !skyultra::CaptureActiveEnchantingStation()) {
        return rejected("No Arcane Enchanter station is currently active.");
    }

    const bool super_active = IsSuperEnchantingActiveUnlocked(player);
    const float charge_slider = std::clamp(options.value("chargeSlider", 0.75F), 0.15F, 1.0F);
    const bool apply_name_to_all = options.value("applyCustomNameToAll", false);
    const auto sanitize_name = [](std::string val) {
        while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(val.begin());
        while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
        if (val.size() > 64) {
            val.resize(64);
        }
        return val;
    };
    const std::string top_custom_name = sanitize_name(options.value("customName", std::string{}));

    struct TargetSpec {
        std::uint32_t item_id{ 0 };
        std::int32_t stack_index{ -1 };
        std::string custom_name;
        RE::TESBoundObject* form{ nullptr };
        RE::ExtraDataList* xlist{ nullptr };
        bool is_weapon{ false };
    };
    std::vector<TargetSpec> targets;
    if (options.contains("items") && options["items"].is_array() && !options["items"].empty()) {
        for (const auto& entry : options["items"]) {
            if (!entry.is_object()) continue;
            const std::int32_t entry_count = std::clamp(entry.value("count", 1), 1, 256);
            for (std::int32_t c = 0; c < entry_count; ++c) {
                TargetSpec spec;
                spec.item_id = ParseItemFormIdField(entry, "itemId");
                spec.stack_index = entry.value("stackIndex", -1);
                spec.custom_name = sanitize_name(entry.value("customName", std::string{}));
                targets.push_back(std::move(spec));
            }
        }
    } else {
        TargetSpec spec;
        spec.item_id = ParseItemFormIdField(options, "itemId");
        spec.stack_index = options.value("stackIndex", -1);
        spec.custom_name = top_custom_name;
        targets.push_back(std::move(spec));
    }

    if (targets.empty()) {
        return rejected("Select an eligible weapon or piece of apparel to enchant.");
    }
    if (targets.size() > 1 && !super_active) {
        return rejected("Enchanting multiple items at once requires Enchanting 100 with Super Enchanting enabled.");
    }

    const auto inv_snapshot = player->GetInventory();
    const auto stack_key = [](std::uint32_t fid, std::int32_t sidx) noexcept -> std::uint64_t {
        return (static_cast<std::uint64_t>(fid) << 32) | static_cast<std::uint32_t>(sidx);
    };
    std::unordered_map<std::uint64_t, std::int32_t> remaining_item_counts;
    bool batch_is_weapon = false;
    for (std::size_t ti = 0; ti < targets.size(); ++ti) {
        auto& t = targets[ti];
        t.form = t.item_id != 0 ? RE::TESForm::LookupByID<RE::TESBoundObject>(t.item_id) : nullptr;
        if (t.form == nullptr || (!t.form->Is(RE::FormType::Weapon) && !t.form->Is(RE::FormType::Armor))) {
            return rejected("Select an eligible weapon or piece of apparel to enchant.");
        }
        t.is_weapon = t.form->Is(RE::FormType::Weapon);
        if (ti == 0) {
            batch_is_weapon = t.is_weapon;
        } else if (t.is_weapon != batch_is_weapon) {
            return rejected("Weapons and apparel cannot be enchanted in the same batch because they use different enchantments.");
        }
        if (const auto* weap = t.form->As<RE::TESObjectWEAP>(); weap != nullptr && weap->IsStaff()) {
            return rejected("Staves cannot be enchanted at an Arcane Enchanter.");
        }
        if (!super_active && HasDisallowEnchantingKeyword(t.form)) {
            return rejected("That item cannot be enchanted unless Super Enchanting is enabled.");
        }

        auto* live_item_entry = FindLiveInventoryEntry(player, t.form);
        const auto inv_it = inv_snapshot.find(t.form);
        const std::int32_t total_count = inv_it != inv_snapshot.end() ? inv_it->second.first : 0;
        if ((live_item_entry == nullptr || live_item_entry->countDelta <= 0) && total_count <= 0) {
            return rejected("You no longer carry that item.");
        }

        std::int32_t stack_avail = 0;
        std::int32_t tracked_extra = 0;
        if (live_item_entry != nullptr && live_item_entry->extraLists != nullptr) {
            std::int32_t idx = 0;
            for (auto* cand : *live_item_entry->extraLists) {
                if (cand == nullptr) continue;
                const std::int32_t cnt = std::max<std::int32_t>(1, cand->GetCount());
                tracked_extra += cnt;
                if (t.stack_index >= 0 && idx == t.stack_index) {
                    t.xlist = cand;
                    stack_avail = cnt;
                    break;
                }
                ++idx;
            }
        }
        if (t.stack_index >= 0) {
            if (t.xlist == nullptr) {
                return rejected("The selected item stack could not be located.");
            }
        } else {
            stack_avail = std::max<std::int32_t>(0, total_count - tracked_extra);
        }

        const auto key = stack_key(t.item_id, t.stack_index);
        if (!remaining_item_counts.contains(key)) {
            remaining_item_counts.emplace(key, stack_avail);
        }
        if (remaining_item_counts[key] <= 0) {
            return rejected("Insufficient item quantity for the selected enchanting batch.");
        }
        --remaining_item_counts[key];

        const auto* base_ench_form = t.form->As<RE::TESEnchantableForm>();
        if ((t.xlist != nullptr && t.xlist->HasType<RE::ExtraEnchantment>()) ||
            (base_ench_form != nullptr && base_ench_form->formEnchanting != nullptr)) {
            return rejected("One of the selected items is already enchanted.");
        }
        if (t.custom_name.empty() && !top_custom_name.empty() && (targets.size() == 1 || apply_name_to_all)) {
            t.custom_name = top_custom_name;
        }
    }

    if (!options.contains("enchantmentIds") || !options["enchantmentIds"].is_array() || options["enchantmentIds"].empty()) {
        return rejected("Select at least one learned enchantment.");
    }
    const std::int32_t max_slots = super_active ? 64 : ComputeEnchantingMaxSlots(player);
    std::vector<RE::EnchantmentItem*> selected_enchs;
    std::vector<std::string> applied_ench_names;
    std::unordered_set<RE::FormID> seen_enchs;
    for (const auto& entry_id_val : options["enchantmentIds"]) {
        std::uint32_t eid = 0;
        if (entry_id_val.is_number_integer()) {
            eid = static_cast<std::uint32_t>(entry_id_val.get<std::int64_t>());
        } else if (entry_id_val.is_string()) {
            const auto text = entry_id_val.get<std::string>();
            const bool hex = text.starts_with("0x") || text.starts_with("0X");
            (void)std::from_chars(text.data() + (hex ? 2 : 0), text.data() + text.size(), eid, hex ? 16 : 10);
        }
        auto* ench = eid != 0 ? RE::TESForm::LookupByID<RE::EnchantmentItem>(eid) : nullptr;
        if (ench == nullptr || ench->IsDynamicForm() || (ench->formFlags & RE::TESForm::RecordFlags::kKnown) == 0 ||
            (!super_active && HasDisallowEnchantingKeyword(ench))) {
            return rejected("One of the selected enchantments is unavailable.");
        }
        for (const auto& t : targets) {
            if (!EnchantmentFitsItem(ench, t.form, super_active)) {
                return rejected("Selected enchantment is incompatible with the chosen item type or slot.");
            }
        }
        if (!seen_enchs.insert(ench->GetFormID()).second) {
            return rejected("Duplicate enchantments cannot be applied to the same item.");
        }
        selected_enchs.push_back(ench);
        applied_ench_names.push_back(ResolveEnchantmentDisplayNameUnlocked(ench));
    }
    if (static_cast<std::int32_t>(selected_enchs.size()) > max_slots) {
        return rejected("Requires the Extra Effect perk (or Super Enchanting at skill 100) to apply additional enchantments.");
    }

    struct GemSpec {
        std::uint32_t gem_id{ 0 };
        std::int32_t stack_index{ -1 };
        RE::TESSoulGem* gem{ nullptr };
        RE::ExtraDataList* xlist{ nullptr };
        RE::SOUL_LEVEL soul{ RE::SOUL_LEVEL::kNone };
        float points{ 0.0F };
        bool reusable{ false };
    };
    std::vector<GemSpec> gems;
    if (options.contains("soulGems") && options["soulGems"].is_array() && !options["soulGems"].empty()) {
        for (const auto& gentry : options["soulGems"]) {
            if (!gentry.is_object()) continue;
            GemSpec gs;
            gs.gem_id = ParseItemFormIdField(gentry, gentry.contains("soulGemId") ? "soulGemId" : "gemId");
            gs.stack_index = gentry.contains("soulGemStackIndex")
                ? gentry.value("soulGemStackIndex", -1)
                : gentry.value("stackIndex", -1);
            gems.push_back(gs);
        }
    } else if (options.contains("soulGemId")) {
        GemSpec gs;
        gs.gem_id = ParseItemFormIdField(options, "soulGemId");
        gs.stack_index = options.value("soulGemStackIndex", -1);
        gems.push_back(gs);
    }

    if (gems.size() != targets.size()) {
        return rejected("Soul Gem count must exactly match the number of items being enchanted (1 filled Soul Gem per piece).");
    }

    std::unordered_map<std::uint64_t, std::int32_t> remaining_gem_counts;
    for (std::size_t gi = 0; gi < targets.size(); ++gi) {
        auto& g = gems[gi];
        g.gem = g.gem_id != 0 ? RE::TESForm::LookupByID<RE::TESSoulGem>(g.gem_id) : nullptr;
        if (g.gem == nullptr) {
            return rejected("Select a filled Soul Gem.");
        }
        g.reusable = IsReusableSoulGem(g.gem);
        auto* live_gem_entry = FindLiveInventoryEntry(player, g.gem);
        const auto git = inv_snapshot.find(g.gem);
        const std::int32_t total_gem_count = git != inv_snapshot.end() ? git->second.first : 0;
        if ((live_gem_entry == nullptr || live_gem_entry->countDelta <= 0) && total_gem_count <= 0) {
            return rejected("You no longer have one of the selected Soul Gems.");
        }

        std::int32_t gem_avail = 0;
        std::int32_t tracked_gem_extra = 0;
        if (live_gem_entry != nullptr && live_gem_entry->extraLists != nullptr) {
            std::int32_t idx = 0;
            for (auto* cand : *live_gem_entry->extraLists) {
                if (cand == nullptr) continue;
                const std::int32_t cnt = std::max<std::int32_t>(1, cand->GetCount());
                tracked_gem_extra += cnt;
                if (g.stack_index >= 0 && idx == g.stack_index) {
                    g.xlist = cand;
                    gem_avail = cnt;
                    break;
                }
                ++idx;
            }
        }
        if (g.stack_index >= 0) {
            if (g.xlist == nullptr) {
                return rejected("One of the selected Soul Gems is no longer available.");
            }
        } else {
            gem_avail = std::max<std::int32_t>(0, total_gem_count - tracked_gem_extra);
        }
        if (g.reusable) {
            gem_avail = std::min<std::int32_t>(gem_avail, 1);
        }

        const auto gkey = stack_key(g.gem_id, g.stack_index);
        if (!remaining_gem_counts.contains(gkey)) {
            remaining_gem_counts.emplace(gkey, gem_avail);
        }
        if (remaining_gem_counts[gkey] <= 0) {
            return rejected("Not enough filled Soul Gems of the selected type for every piece.");
        }
        --remaining_gem_counts[gkey];

        g.soul = g.xlist != nullptr ? g.xlist->GetSoulLevel() : RE::SOUL_LEVEL::kNone;
        if (g.soul == RE::SOUL_LEVEL::kNone && g.xlist != nullptr) {
            if (const auto* ex_s = g.xlist->GetByType<RE::ExtraSoul>(); ex_s != nullptr) {
                g.soul = ex_s->GetContainedSoul();
            }
        }
        if (g.soul == RE::SOUL_LEVEL::kNone) {
            g.soul = g.gem->GetContainedSoul();
        }
        g.points = SoulLevelPoints(g.soul);
        if (g.soul == RE::SOUL_LEVEL::kNone || g.points <= 0.0F) {
            return rejected("One of the selected Soul Gems is empty.");
        }
    }

    auto* inv_changes = player->GetInventoryChanges();
    if (inv_changes == nullptr) {
        return rejected("Player inventory changes are unavailable.");
    }

    ArmVirtualCraftingProxy(player, g_active_enchanting_station.furniture_ref.get().get());

    Json enchanted_items_json = Json::array();
    std::uint16_t last_charge = 0U;
    std::int32_t last_uses = 0;
    std::string first_item_name = "Item";

    const auto resolve_live_xlist = [&](RE::TESBoundObject* obj, RE::ExtraDataList* preferred) -> RE::ExtraDataList* {
        if (preferred == nullptr) {
            return nullptr;
        }
        auto* live_entry = FindLiveInventoryEntry(player, obj);
        if (live_entry == nullptr || live_entry->extraLists == nullptr) {
            return nullptr;
        }
        for (auto* cand : *live_entry->extraLists) {
            if (cand == preferred) {
                return cand;
            }
        }
        return nullptr;
    };

    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto& t = targets[i];
        const auto& g = gems[i];

        RE::BSTArray<RE::Effect> created_effects;
        float total_cost = 0.0F;
        std::int32_t projected_uses = 0;
        BuildScaledEnchantmentEffects(
            player, selected_enchs, t.form, g.points, charge_slider, created_effects, total_cost, projected_uses);
        if (created_effects.empty()) {
            return rejected("Failed to construct enchantment effects.");
        }

        RE::EnchantmentItem* created_ench = t.is_weapon
            ? created_mgr->AddWeaponEnchantment(created_effects)
            : created_mgr->AddArmorEnchantment(created_effects);
        if (created_ench == nullptr) {
            return rejected("Skyrim could not register the new enchantment form.");
        }

        const std::uint16_t charge_u16 = t.is_weapon
            ? static_cast<std::uint16_t>(std::clamp<int>(static_cast<int>(std::lround(g.points)), 1, 65535))
            : 0U;
        RE::ExtraDataList* live_target_xlist = resolve_live_xlist(t.form, t.xlist);
        if (live_target_xlist != nullptr && live_target_xlist->HasType<RE::ExtraEnchantment>()) {
            live_target_xlist = nullptr;
        }
        RE::ExtraDataList* applied_xlist = inv_changes->EnchantObject(t.form, live_target_xlist, created_ench, charge_u16);
        if (applied_xlist == nullptr) {
            return rejected("Failed to bind enchantment to one of the selected items.");
        }

        if (!t.custom_name.empty()) {
            if (auto* ex_text = applied_xlist->GetByType<RE::ExtraTextDisplayData>(); ex_text != nullptr) {
                ex_text->SetName(t.custom_name.c_str());
            } else {
                applied_xlist->Add(new RE::ExtraTextDisplayData(t.custom_name.c_str()));
            }
        }

        if (applied_xlist->GetWorn()) {
            if (!t.is_weapon) {
                player->UpdateArmorAbility(t.form, applied_xlist);
            } else {
                player->UpdateWeaponAbility(t.form, applied_xlist, applied_xlist->HasType<RE::ExtraWornLeft>());
            }
        }

        RE::ExtraDataList* live_gem_xlist = resolve_live_xlist(g.gem, g.xlist);
        if (g.reusable) {
            if (live_gem_xlist != nullptr) {
                if (auto* ex_s = live_gem_xlist->GetByType<RE::ExtraSoul>(); ex_s != nullptr) {
                    ex_s->soul = RE::SOUL_LEVEL::kNone;
                }
            } else if (g.gem->linkedSoulGem != nullptr) {
                player->RemoveItem(g.gem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                player->AddObjectToContainer(g.gem->linkedSoulGem, nullptr, 1, nullptr);
            }
        } else {
            player->RemoveItem(g.gem, 1, RE::ITEM_REMOVE_REASON::kRemove, live_gem_xlist, nullptr);
        }

        EmitNativeCraftedItemEvents(
            player,
            g_active_enchanting_station.furniture_ref,
            t.form,
            false,
            true,
            false);

        const char* raw_piece_name = !t.custom_name.empty() ? t.custom_name.c_str() : t.form->GetName();
        const std::string piece_name = (raw_piece_name != nullptr && raw_piece_name[0] != '\0')
            ? StripLeadingDollar(raw_piece_name)
            : std::string("Item");
        if (i == 0) {
            first_item_name = piece_name;
            last_charge = charge_u16;
            last_uses = projected_uses;
        }
        enchanted_items_json.push_back({
            { "itemId", std::to_string(t.item_id) },
            { "itemName", piece_name },
            { "charge", charge_u16 },
            { "uses", projected_uses }
        });
    }

    const float xp_gained = static_cast<float>(targets.size());
    player->UseSkill(RE::ActorValue::kEnchanting, xp_gained, nullptr);
    RE::PlaySound("UIEnchantingItemCreate");
    skyultra::InvalidateSharedGameDataCache();

    auto updated_snapshot = ReadEnchantingSnapshot(Json::object());
    const std::string done_msg = targets.size() > 1
        ? ("Super-enchanted " + std::to_string(targets.size()) + " items.")
        : ("Enchanted " + first_item_name + ".");
    return {
        { "accepted", true },
        { "itemId", std::to_string(targets[0].item_id) },
        { "itemName", first_item_name },
        { "enchantmentNames", std::move(applied_ench_names) },
        { "enchantedItems", std::move(enchanted_items_json) },
        { "batchCount", static_cast<std::int32_t>(targets.size()) },
        { "charge", last_charge },
        { "uses", last_uses },
        { "xpGained", xp_gained },
        { "message", done_msg },
        { "snapshot", std::move(updated_snapshot) }
    };
}

Json PerformDisenchantItem(const Json& options)
{
    const auto rejected = [](std::string message) -> Json {
        return { { "accepted", false }, { "message", std::move(message) } };
    };
    if (!options.is_object()) {
        return rejected("Disenchant parameters must be an object.");
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return rejected("Player enchanting state is unavailable.");
    }
    if (!g_active_enchanting_station.active && !skyultra::CaptureActiveEnchantingStation()) {
        return rejected("No Arcane Enchanter station is currently active.");
    }

    const bool super_active = IsSuperEnchantingActiveUnlocked(player);
    const std::uint32_t item_id = ParseItemFormIdField(options, "itemId");
    const std::int32_t stack_index = options.value("stackIndex", -1);
    auto* target_form = item_id != 0 ? RE::TESForm::LookupByID<RE::TESBoundObject>(item_id) : nullptr;
    if (target_form == nullptr) {
        return rejected("The selected item could not be found.");
    }

    auto inv = player->GetInventory();
    const auto inv_it = inv.find(target_form);
    if (inv_it == inv.end() || inv_it->second.first <= 0) {
        return rejected("You no longer carry that item.");
    }
    auto* inv_entry = inv_it->second.second.get();

    auto* live_entry = FindLiveInventoryEntry(player, target_form);
    RE::ExtraDataList* target_xlist = nullptr;
    if (stack_index >= 0 && live_entry != nullptr && live_entry->extraLists != nullptr) {
        std::int32_t idx = 0;
        for (auto* cand : *live_entry->extraLists) {
            if (cand == nullptr) continue;
            if (idx == stack_index) {
                target_xlist = cand;
                break;
            }
            ++idx;
        }
        if (target_xlist == nullptr) {
            return rejected("The selected item stack could not be located.");
        }
    }

    if (IsActiveRunningQuestObject(target_xlist != nullptr ? nullptr : inv_entry, target_xlist)) {
        return rejected("Active quest items cannot be disenchanted until the quest is over.");
    }

    RE::EnchantmentItem* ench = ResolveItemEnchantment(target_form, target_xlist, inv_entry);
    if (ench == nullptr) {
        return rejected("That item has no enchantment to extract.");
    }
    RE::EnchantmentItem* base_ench = ench->data.baseEnchantment != nullptr ? ench->data.baseEnchantment : ench;
    if (ench->IsDynamicForm() || base_ench == nullptr || base_ench->IsDynamicForm()) {
        return rejected("Custom player enchantments cannot be disenchanted.");
    }
    const bool base_disallowed = HasDisallowEnchantingKeyword(target_form) ||
        HasDisallowEnchantingKeyword(ench) ||
        HasDisallowEnchantingKeyword(base_ench);
    if (base_disallowed && !super_active) {
        return rejected("Unique enchantment requires Enchanting 100 with Super Enchanting enabled in Settings.");
    }
    if ((base_ench->formFlags & RE::TESForm::RecordFlags::kKnown) != 0) {
        return rejected("You have already learned this enchantment.");
    }

    const std::string ench_name = ResolveEnchantmentDisplayNameUnlocked(base_ench);
    const char* raw_iname = target_form->GetName();
    const std::string item_name = (raw_iname != nullptr && raw_iname[0] != '\0')
        ? StripLeadingDollar(raw_iname)
        : std::string("Item");

    base_ench->SetPlayerKnows(true);

    const float xp_gained = ComputeDisenchantSkillUsePoints(ench);
    player->UseSkill(RE::ActorValue::kEnchanting, xp_gained, nullptr);

    const bool is_worn = (target_xlist != nullptr &&
        (target_xlist->HasType<RE::ExtraWorn>() || target_xlist->HasType<RE::ExtraWornLeft>())) ||
        player->GetEquippedObject(false) == target_form ||
        player->GetEquippedObject(true) == target_form;
    if (is_worn) {
        if (auto* eq_mgr = RE::ActorEquipManager::GetSingleton(); eq_mgr != nullptr) {
            (void)eq_mgr->UnequipObject(player, target_form, target_xlist, 1, nullptr, false, false, true, false, nullptr);
        }
    }

    ArmVirtualCraftingProxy(
        player,
        g_active_enchanting_station.furniture_ref.get().get(),
        true,
        target_form,
        ench);

    const std::int32_t before_cnt = inv_it->second.first;
    player->RemoveItem(target_form, 1, RE::ITEM_REMOVE_REASON::kRemove, target_xlist, nullptr);
    ConsumePlayerStolenCount(target_form, inv_entry, before_cnt, 1, std::max(0, before_cnt - 1), false);
    RE::SendUIMessage::SendInventoryUpdateMessage(player, target_form);
    RE::PlaySound("UIEnchantingItemDestroy");
    RE::PlaySound("UIEnchantingLearnEffect");
    const std::string notice = "Enchantment learned: " + ench_name;
    RE::SendHUDMessage::ShowHUDMessage(notice.c_str());
    skyultra::InvalidateSharedGameDataCache();

    auto updated_snapshot = ReadEnchantingSnapshot(Json::object());
    return {
        { "accepted", true },
        { "itemId", std::to_string(item_id) },
        { "itemName", item_name },
        { "learnedEnchantmentId", std::to_string(base_ench->GetFormID()) },
        { "learnedEnchantmentName", ench_name },
        { "xpGained", std::round(xp_gained * 10.0F) / 10.0F },
        { "message", "Destroyed " + item_name + " and learned " + ench_name + "." },
        { "snapshot", std::move(updated_snapshot) }
    };
}

Json PerformRenameEnchantment(const Json& options)
{
    const auto rejected = [](std::string message) -> Json {
        return { { "accepted", false }, { "message", std::move(message) } };
    };
    if (!options.is_object()) {
        return rejected("Rename parameters must be an object.");
    }
    const std::uint32_t ench_id = ParseItemFormIdField(options, "enchantmentId");
    const bool reset = options.value("reset", false);
    std::string new_name = reset ? std::string{} : options.value("name", std::string{});
    while (!new_name.empty() && std::isspace(static_cast<unsigned char>(new_name.front()))) new_name.erase(new_name.begin());
    while (!new_name.empty() && std::isspace(static_cast<unsigned char>(new_name.back()))) new_name.pop_back();
    if (!reset && new_name.empty()) {
        return rejected("Enter a non-empty enchantment name.");
    }
    if (new_name.size() > 80) {
        new_name.resize(80);
    }

    auto* ench = ench_id != 0 ? RE::TESForm::LookupByID<RE::EnchantmentItem>(ench_id) : nullptr;
    if (ench == nullptr || ench->IsDynamicForm()) {
        return rejected("Selected enchantment cannot be renamed.");
    }
    auto* base_ench = ench->data.baseEnchantment != nullptr ? ench->data.baseEnchantment : ench;
    if (base_ench == nullptr || base_ench->IsDynamicForm()) {
        return rejected("Selected enchantment cannot be renamed.");
    }

    ApplyCustomNameToEnchantmentUnlocked(base_ench, new_name);
    bool is_custom = false;
    std::string orig_name;
    const std::string resolved_name = ResolveEnchantmentDisplayNameUnlocked(base_ench, &is_custom, &orig_name);
    RE::PlaySound("UIMenuOK");
    skyultra::InvalidateSharedGameDataCache();

    auto updated_snapshot = ReadEnchantingSnapshot(Json::object());
    return {
        { "accepted", true },
        { "enchantmentId", std::to_string(base_ench->GetFormID()) },
        { "name", resolved_name },
        { "originalName", orig_name },
        { "isCustomNamed", is_custom },
        { "message", reset ? ("Restored name to " + resolved_name + ".") : ("Renamed enchantment to " + resolved_name + ".") },
        { "snapshot", std::move(updated_snapshot) }
    };
}

[[nodiscard]] bool ClassifyAlchemyFurniture(const RE::TESFurniture* furniture) noexcept
{
    if (furniture == nullptr) {
        return false;
    }
    const auto bench_type = furniture->workBenchData.benchType.get();
    if (bench_type == RE::TESFurniture::WorkBenchData::BenchType::kEnchanting ||
        bench_type == RE::TESFurniture::WorkBenchData::BenchType::kEnchantingExperiment ||
        bench_type == RE::TESFurniture::WorkBenchData::BenchType::kSmithingWeapon ||
        bench_type == RE::TESFurniture::WorkBenchData::BenchType::kSmithingArmor ||
        bench_type == RE::TESFurniture::WorkBenchData::BenchType::kCreateObject) {
        return false;
    }
    const auto uses_skill = furniture->workBenchData.usesSkill.get();
    return bench_type == RE::TESFurniture::WorkBenchData::BenchType::kAlchemy ||
        bench_type == RE::TESFurniture::WorkBenchData::BenchType::kAlchemyExperiment ||
        uses_skill == RE::ActorValue::kAlchemy ||
        furniture->HasKeywordString("CraftingAlchemy") ||
        furniture->HasKeywordID(0x000C554DU);
}

struct AlchemyPowerContext {
    float effective_skill{ 15.0F };
    float base_skill{ 15.0F };
    float fortify_pct{ 0.0F };
    float fortify_mult{ 1.0F };
    float init_mult{ 4.0F };
    float skill_factor{ 1.5F };
    float gold_mult{ 1.0F };
    float skill_mult{ 4.3F };
    std::int32_t alchemist_rank{ 0 };
    float alchemist_mult{ 1.0F };
    bool has_physician{ false };
    bool has_benefactor{ false };
    bool has_poisoner{ false };
    bool has_purity{ false };
    std::int32_t experimenter_effects{ 1 };
};

[[nodiscard]] AlchemyPowerContext ComputeAlchemyPowerContext(RE::PlayerCharacter* player) noexcept
{
    AlchemyPowerContext ctx{};
    ctx.init_mult = std::clamp(ReadGameSettingFloat("fAlchemyIngredientInitMult", 4.0F), 0.25F, 20.0F);
    ctx.skill_factor = std::clamp(ReadGameSettingFloat("fAlchemySkillFactor", 1.5F), 1.0F, 5.0F);
    ctx.gold_mult = std::clamp(ReadGameSettingFloat("fAlchemyGoldMult", 1.0F), 0.05F, 5.0F);
    if (player == nullptr) {
        ctx.skill_mult = ctx.init_mult * (1.0F + (ctx.skill_factor - 1.0F) * (15.0F / 100.0F));
        return ctx;
    }

    if (const auto* av_owner = player->AsActorValueOwner(); av_owner != nullptr) {
        ctx.effective_skill = std::max(0.0F, av_owner->GetActorValue(RE::ActorValue::kAlchemy));
        ctx.base_skill = std::max(0.0F, av_owner->GetBaseActorValue(RE::ActorValue::kAlchemy));
        ctx.fortify_pct = std::max(-90.0F, av_owner->GetActorValue(RE::ActorValue::kAlchemyModifier));
    }
    ctx.skill_mult = ctx.init_mult * (1.0F + (ctx.skill_factor - 1.0F) * (ctx.effective_skill / 100.0F));
    ctx.fortify_mult = std::max(0.1F, 1.0F + (ctx.fortify_pct / 100.0F));

    const auto has_perk_id = [player](RE::FormID id) noexcept -> bool {
        if (auto* perk = RE::TESForm::LookupByID<RE::BGSPerk>(id); perk != nullptr) {
            return player->HasPerk(perk);
        }
        return false;
    };

    if (has_perk_id(0x000C07CDU)) ctx.alchemist_rank = 5;
    else if (has_perk_id(0x000C07CCU)) ctx.alchemist_rank = 4;
    else if (has_perk_id(0x000C07CBU)) ctx.alchemist_rank = 3;
    else if (has_perk_id(0x000C07CAU)) ctx.alchemist_rank = 2;
    else if (has_perk_id(0x000BE127U)) ctx.alchemist_rank = 1;
    ctx.alchemist_mult = 1.0F + (0.20F * static_cast<float>(ctx.alchemist_rank));

    ctx.has_physician = has_perk_id(0x00058215U);
    ctx.has_benefactor = has_perk_id(0x00058216U);
    ctx.has_poisoner = has_perk_id(0x00058217U);
    ctx.has_purity = has_perk_id(0x0005821DU);
    if (!ctx.has_purity) {
        if (auto* ep = RE::BGSEntryPoint::GetEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kPurifyAlchemyIngredients);
            ep != nullptr &&
            ep->parameters.count == 0 &&
            ep->functionType == RE::BGSEntryPointFunction::ENTRY_POINT_FUNCTION_TYPE::kValue) {
            float purify_val = 0.0F;
            RE::BGSEntryPoint::HandleEntryPoint(
                RE::BGSEntryPoint::ENTRY_POINT::kPurifyAlchemyIngredients,
                player,
                &purify_val);
            if (purify_val > 0.5F) {
                ctx.has_purity = true;
            }
        }
    }

    if (has_perk_id(0x00105F2BU)) ctx.experimenter_effects = 4;
    else if (has_perk_id(0x00105F2AU)) ctx.experimenter_effects = 3;
    else if (has_perk_id(0x00058218U)) ctx.experimenter_effects = 2;
    else ctx.experimenter_effects = 1;
    return ctx;
}

[[nodiscard]] bool IsRestoreVitalAlchemyEffect(const RE::EffectSetting* base) noexcept
{
    if (base == nullptr || base->IsHostile() || base->IsDetrimental()) {
        return false;
    }
    if (!base->HasArchetype(RE::EffectSetting::Archetype::kValueModifier)) {
        return false;
    }
    const auto av = base->data.primaryAV;
    return av == RE::ActorValue::kHealth ||
        av == RE::ActorValue::kMagicka ||
        av == RE::ActorValue::kStamina;
}

[[nodiscard]] float ComputeAlchemyEffectCost(
    const RE::EffectSetting* base,
    float mag,
    std::uint32_t dur) noexcept
{
    if (base == nullptr) {
        return 0.0F;
    }
    const float base_cost = std::max(0.0F, base->data.baseCost);
    const float mag_factor = mag <= 1.0F ? 1.0F : std::pow(mag, 1.1F);
    const float dur_factor = dur == 0U ? 1.0F : std::pow(static_cast<float>(dur) / 10.0F, 1.1F);
    const float result = base_cost * mag_factor * dur_factor;
    return std::isfinite(result) ? std::max(0.0F, result) : 0.0F;
}

struct EvaluatedAlchemyResult {
    bool valid{ false };
    bool is_poison{ false };
    std::string default_name{ "Potion" };
    std::int32_t gold_value{ 0 };
    std::vector<RE::EffectSetting*> matched_settings;
    std::vector<std::string> cleansed_effect_names;
    RE::BSTArray<RE::Effect> effects;
};

[[nodiscard]] EvaluatedAlchemyResult EvaluateAlchemyCombination(
    const AlchemyPowerContext& ctx,
    const std::vector<RE::IngredientItem*>& ingredients)
{
    EvaluatedAlchemyResult res;
    if (ingredients.size() < 2 || ingredients.size() > 3) {
        return res;
    }

    struct MatchedCandidate {
        RE::EffectSetting* base{ nullptr };
        const RE::Effect* winner{ nullptr };
        float raw_cost{ -1.0F };
        bool is_negative{ false };
        bool is_restore_vital{ false };
        float baseline_cost{ 0.0F };
    };

    std::vector<RE::EffectSetting*> ordered_settings;
    std::unordered_map<RE::FormID, std::vector<std::pair<std::size_t, const RE::Effect*>>> by_effect;
    for (std::size_t ing_idx = 0; ing_idx < ingredients.size(); ++ing_idx) {
        auto* ing = ingredients[ing_idx];
        if (ing == nullptr) continue;
        std::unordered_set<RE::FormID> seen_in_ing;
        for (const auto* eff : ing->effects) {
            if (eff == nullptr || eff->baseEffect == nullptr) continue;
            const auto eid = eff->baseEffect->GetFormID();
            if (eid == 0 || !seen_in_ing.insert(eid).second) continue;
            auto& bucket = by_effect[eid];
            if (bucket.empty()) {
                ordered_settings.push_back(eff->baseEffect);
            }
            bucket.emplace_back(ing_idx, eff);
        }
    }

    const float base_power = ctx.skill_mult * ctx.fortify_mult * ctx.alchemist_mult;
    const auto compute_scaled_pair = [](
        const RE::EffectSetting* base,
        const RE::Effect* src,
        float mult,
        float& out_mag,
        std::uint32_t& out_dur) noexcept
    {
        const auto flags = base->data.flags;
        const bool no_mag = flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoMagnitude);
        const bool no_dur = flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoDuration);
        const bool power_mag = flags.any(RE::EffectSetting::EffectSettingData::Flag::kPowerAffectsMagnitude);
        const bool power_dur = flags.any(RE::EffectSetting::EffectSettingData::Flag::kPowerAffectsDuration);
        const float src_mag = std::max(0.0F, src->GetMagnitude());
        const std::uint32_t src_dur = src->GetDuration();

        if (no_mag || src_mag <= 0.0F) {
            out_mag = 0.0F;
        } else if (power_mag || !power_dur) {
            out_mag = std::max(1.0F, std::round(src_mag * mult));
        } else {
            out_mag = std::max(1.0F, src_mag);
        }

        if (no_dur || src_dur == 0U) {
            out_dur = 0U;
        } else if (power_dur && !power_mag) {
            out_dur = std::max<std::uint32_t>(1U, static_cast<std::uint32_t>(std::round(static_cast<float>(src_dur) * mult)));
        } else {
            out_dur = src_dur;
        }
    };

    std::vector<MatchedCandidate> candidates;
    for (auto* setting : ordered_settings) {
        const auto it = by_effect.find(setting->GetFormID());
        if (it == by_effect.end() || it->second.size() < 2) {
            continue;
        }
        MatchedCandidate cand;
        cand.base = setting;
        cand.is_negative = setting->IsHostile() || setting->IsDetrimental();
        cand.is_restore_vital = IsRestoreVitalAlchemyEffect(setting);
        for (const auto& pair : it->second) {
            const auto* eff = pair.second;
            const float c = ComputeAlchemyEffectCost(setting, eff->GetMagnitude(), eff->GetDuration());
            if (cand.winner == nullptr ||
                c > cand.raw_cost + 1e-4F ||
                (std::abs(c - cand.raw_cost) <= 1e-4F && eff->GetMagnitude() > cand.winner->GetMagnitude()) ||
                (std::abs(c - cand.raw_cost) <= 1e-4F && std::abs(eff->GetMagnitude() - cand.winner->GetMagnitude()) <= 1e-4F && eff->GetDuration() > cand.winner->GetDuration())) {
                cand.winner = eff;
                cand.raw_cost = c;
            }
        }
        if (cand.winner == nullptr) {
            continue;
        }
        float b_mag = 0.0F;
        std::uint32_t b_dur = 0U;
        compute_scaled_pair(setting, cand.winner, base_power, b_mag, b_dur);
        cand.baseline_cost = ComputeAlchemyEffectCost(setting, b_mag, b_dur);
        res.matched_settings.push_back(setting);
        candidates.push_back(cand);
    }

    if (candidates.empty()) {
        return res;
    }

    const MatchedCandidate* dominant = &candidates.front();
    for (const auto& cand : candidates) {
        if (cand.baseline_cost > dominant->baseline_cost + 1e-4F) {
            dominant = &cand;
        }
    }
    res.is_poison = dominant->is_negative;

    std::vector<MatchedCandidate> kept;
    kept.reserve(candidates.size());
    for (const auto& cand : candidates) {
        if (ctx.has_purity && cand.is_negative != res.is_poison) {
            const char* raw_cn = cand.base->GetFullName();
            if (raw_cn == nullptr || raw_cn[0] == '\0') raw_cn = cand.base->GetName();
            res.cleansed_effect_names.push_back((raw_cn != nullptr && raw_cn[0] != '\0') ? StripLeadingDollar(raw_cn) : std::string("Effect"));
            continue;
        }
        kept.push_back(cand);
    }
    if (kept.empty()) {
        return res;
    }

    res.effects.reserve(static_cast<std::uint32_t>(kept.size()));
    float sum_cost = 0.0F;
    float best_final_cost = -1.0F;
    std::string primary_name = res.is_poison ? "Damage Health" : "Restore Health";
    for (const auto& cand : kept) {
        float eff_mult = base_power;
        if (!cand.is_negative && !res.is_poison) {
            if (cand.is_restore_vital && ctx.has_physician) {
                eff_mult *= 1.25F;
            }
            if (ctx.has_benefactor) {
                eff_mult *= 1.25F;
            }
        } else if (cand.is_negative && res.is_poison && ctx.has_poisoner) {
            eff_mult *= 1.25F;
        }

        float f_mag = 0.0F;
        std::uint32_t f_dur = 0U;
        compute_scaled_pair(cand.base, cand.winner, eff_mult, f_mag, f_dur);
        const float eff_cost = ComputeAlchemyEffectCost(cand.base, f_mag, f_dur);

        RE::Effect& created = res.effects.emplace_back();
        created.Copy(cand.winner);
        created.SetMagnitude(f_mag);
        created.SetDuration(static_cast<std::int32_t>(f_dur));
        created.cost = eff_cost;

        sum_cost += eff_cost;
        if (eff_cost > best_final_cost + 1e-4F) {
            best_final_cost = eff_cost;
            const char* raw_n = cand.base->GetFullName();
            if (raw_n == nullptr || raw_n[0] == '\0') raw_n = cand.base->GetName();
            if (raw_n != nullptr && raw_n[0] != '\0') {
                primary_name = StripLeadingDollar(raw_n);
            }
        }
    }

    res.valid = !res.effects.empty();
    res.gold_value = std::max<std::int32_t>(1, static_cast<std::int32_t>(std::floor(sum_cost)));
    res.default_name = (res.is_poison ? "Poison of " : "Potion of ") + primary_name;
    return res;
}

Json ReadAlchemySnapshot(const Json& options)
{
    (void)options;
    const auto unavailable = [](std::string message) -> Json {
        return {
            { "available", false },
            { "stationName", "Alchemy Lab" },
            { "ingredients", Json::array() },
            { "knownEffects", Json::array() },
            { "message", std::move(message) }
        };
    };

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return unavailable("Player alchemy state is unavailable.");
    }
    if (!g_active_alchemy_station.active) {
        if (!skyultra::CaptureActiveAlchemyStation()) {
            return unavailable("No Alchemy Lab station is currently active.");
        }
    }
    const ActiveAlchemyStationState station = g_active_alchemy_station;
    const AlchemyPowerContext ctx = ComputeAlchemyPowerContext(player);

    const auto* av_owner = player->AsActorValueOwner();
    const float max_carry = av_owner != nullptr
        ? std::max(1.0F, av_owner->GetActorValue(RE::ActorValue::kCarryWeight))
        : 300.0F;
    const float cur_carry = av_owner != nullptr
        ? std::max(0.0F, av_owner->GetActorValue(RE::ActorValue::kInventoryWeight))
        : std::max(0.0F, player->GetWeightInContainer());

    struct KnownEffectAggregate {
        std::string effect_id;
        std::string name;
        bool is_hostile{ false };
        bool is_detrimental{ false };
        std::string school{ "Alchemy" };
        std::string desc_template;
        std::int32_t ingredient_count{ 0 };
    };
    std::unordered_map<std::string, KnownEffectAggregate> known_effect_map;

    const auto inv = player->GetInventory();
    Json ingredients_json = Json::array();
    for (const auto& [bound_obj, inv_pair] : inv) {
        if (!IsLiveInventoryForm(bound_obj) || inv_pair.first <= 0 || !bound_obj->Is(RE::FormType::Ingredient)) {
            continue;
        }
        auto* ing = bound_obj->As<RE::IngredientItem>();
        if (ing == nullptr) {
            continue;
        }
        auto* entry_data = inv_pair.second.get();
        const char* raw_name = entry_data != nullptr ? entry_data->GetDisplayName() : nullptr;
        if (raw_name == nullptr || raw_name[0] == '\0') {
            raw_name = ing->GetFullName();
        }
        if (raw_name == nullptr || raw_name[0] == '\0') {
            raw_name = ing->GetName();
        }
        if (raw_name == nullptr || raw_name[0] == '\0' || IsMissingOrUnnamedDisplayName(raw_name)) {
            continue;
        }
        const std::string ing_name = StripLeadingDollar(raw_name);
        const std::string form_id_str = std::to_string(ing->GetFormID());
        const bool is_stolen = ComputePlayerStolenCount(ing, inv_pair.first, entry_data) > 0;
        const bool is_quest = entry_data != nullptr && entry_data->IsQuestObject();

        Json effects_json = Json::array();
        std::int32_t known_count = 0;
        std::unordered_set<std::string> ing_known_effect_ids;
        const std::uint32_t max_slots = std::min<std::uint32_t>(4U, ing->effects.size());
        for (std::uint32_t slot = 0; slot < max_slots; ++slot) {
            const auto* eff = ing->effects[slot];
            const bool known = (ing->gamedata.knownEffectFlags & (static_cast<std::uint16_t>(1U) << slot)) != 0U;
            if (!known || eff == nullptr || eff->baseEffect == nullptr) {
                effects_json.push_back({
                    { "slot", static_cast<int>(slot) },
                    { "known", false }
                });
                continue;
            }
            ++known_count;
            const auto* base = eff->baseEffect;
            const char* raw_eff_name = base->GetFullName();
            if (raw_eff_name == nullptr || raw_eff_name[0] == '\0') {
                raw_eff_name = base->GetName();
            }
            const std::string eff_name = (raw_eff_name != nullptr && raw_eff_name[0] != '\0')
                ? StripLeadingDollar(raw_eff_name)
                : std::string("Alchemical Effect");
            const std::string eff_id = std::to_string(base->GetFormID());
            const auto flags = base->data.flags;
            const std::string school(MagicEffectSchoolLabel(base, "Alchemy"));
            const std::string desc_tpl = FormatEffectTemplateDescription(eff);
            const bool hostile = base->IsHostile();
            const bool detrimental = base->IsDetrimental();

            effects_json.push_back({
                { "slot", static_cast<int>(slot) },
                { "known", true },
                { "effectId", eff_id },
                { "name", eff_name },
                { "descriptionTemplate", desc_tpl },
                { "isHostile", hostile },
                { "isDetrimental", detrimental },
                { "baseMagnitude", std::max(0.0F, eff->GetMagnitude()) },
                { "baseDuration", eff->GetDuration() },
                { "baseCost", std::max(0.0F, base->data.baseCost) },
                { "powerAffectsMagnitude", flags.any(RE::EffectSetting::EffectSettingData::Flag::kPowerAffectsMagnitude) },
                { "powerAffectsDuration", flags.any(RE::EffectSetting::EffectSettingData::Flag::kPowerAffectsDuration) },
                { "noMagnitude", flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoMagnitude) },
                { "noDuration", flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoDuration) },
                { "school", school },
                { "isRestoreVital", IsRestoreVitalAlchemyEffect(base) }
            });

            if (ing_known_effect_ids.insert(eff_id).second) {
                auto& agg = known_effect_map[eff_id];
                if (agg.effect_id.empty()) {
                    agg.effect_id = eff_id;
                    agg.name = eff_name;
                    agg.is_hostile = hostile;
                    agg.is_detrimental = detrimental;
                    agg.school = school;
                    agg.desc_template = desc_tpl;
                }
                ++agg.ingredient_count;
            }
        }

        ingredients_json.push_back({
            { "id", form_id_str },
            { "itemId", form_id_str },
            { "name", ing_name },
            { "count", inv_pair.first },
            { "value", std::max(0, ing->GetGoldValue()) },
            { "weight", std::round(std::max(0.0F, ing->GetWeight()) * 100.0F) / 100.0F },
            { "isFood", ing->IsFood() },
            { "isStolen", is_stolen },
            { "isQuest", is_quest },
            { "knownCount", known_count },
            { "effects", std::move(effects_json) }
        });
    }

    std::sort(ingredients_json.begin(), ingredients_json.end(), [](const Json& a, const Json& b) {
        return LowerAscii(a.value("name", std::string{})) < LowerAscii(b.value("name", std::string{}));
    });

    std::vector<KnownEffectAggregate> known_vec;
    known_vec.reserve(known_effect_map.size());
    for (auto& [_, agg] : known_effect_map) {
        known_vec.push_back(std::move(agg));
    }
    std::sort(known_vec.begin(), known_vec.end(), [](const KnownEffectAggregate& a, const KnownEffectAggregate& b) {
        if (a.ingredient_count != b.ingredient_count) return a.ingredient_count > b.ingredient_count;
        return LowerAscii(a.name) < LowerAscii(b.name);
    });
    Json known_effects_json = Json::array();
    for (const auto& agg : known_vec) {
        known_effects_json.push_back({
            { "effectId", agg.effect_id },
            { "name", agg.name },
            { "isHostile", agg.is_hostile },
            { "isDetrimental", agg.is_detrimental },
            { "school", agg.school },
            { "ingredientCount", agg.ingredient_count },
            { "descriptionTemplate", agg.desc_template }
        });
    }

    const auto* raw_pname = player->GetName();
    return {
        { "available", true },
        { "stationName", station.station_name },
        { "alchemySkill", std::round(ctx.effective_skill * 10.0F) / 10.0F },
        { "alchemyBaseSkill", std::round(ctx.base_skill * 10.0F) / 10.0F },
        { "fortifyAlchemyPct", std::round(ctx.fortify_pct * 10.0F) / 10.0F },
        { "skillMultiplier", std::round(ctx.skill_mult * 1000.0F) / 1000.0F },
        { "initMult", ctx.init_mult },
        { "skillFactor", ctx.skill_factor },
        { "goldMult", ctx.gold_mult },
        { "alchemistRank", ctx.alchemist_rank },
        { "hasPhysician", ctx.has_physician },
        { "hasBenefactor", ctx.has_benefactor },
        { "hasPoisoner", ctx.has_poisoner },
        { "hasPurity", ctx.has_purity },
        { "experimenterEffects", ctx.experimenter_effects },
        { "playerName", (raw_pname != nullptr && raw_pname[0] != '\0') ? std::string(raw_pname) : std::string("Dragonborn") },
        { "playerGold", std::max(0, player->GetGoldAmount()) },
        { "carryWeight", std::round(cur_carry * 10.0F) / 10.0F },
        { "maxCarryWeight", std::round(max_carry * 10.0F) / 10.0F },
        { "ingredients", std::move(ingredients_json) },
        { "knownEffects", std::move(known_effects_json) }
    };
}

Json PerformAlchemyCraft(const Json& options)
{
    const auto rejected = [](std::string message) -> Json {
        return { { "accepted", false }, { "message", std::move(message) } };
    };
    if (!options.is_object()) {
        return rejected("Alchemy craft parameters must be an object.");
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* created_mgr = RE::BGSCreatedObjectManager::GetSingleton();
    if (player == nullptr || created_mgr == nullptr) {
        return rejected("Alchemy engine state is unavailable.");
    }
    if (!g_active_alchemy_station.active && !skyultra::CaptureActiveAlchemyStation()) {
        return rejected("No Alchemy Lab station is currently active.");
    }
    if (!options.contains("ingredientIds") || !options["ingredientIds"].is_array()) {
        return rejected("Select 2 or 3 ingredients to combine.");
    }

    auto parse_id_val = [](const Json& val) noexcept -> std::uint32_t {
        std::uint32_t parsed = 0;
        if (val.is_number_integer()) {
            const auto v = val.get<std::int64_t>();
            if (v > 0) parsed = static_cast<std::uint32_t>(v);
        } else if (val.is_string()) {
            const auto text = val.get<std::string>();
            const bool hex = text.starts_with("0x") || text.starts_with("0X");
            (void)std::from_chars(text.data() + (hex ? 2 : 0), text.data() + text.size(), parsed, hex ? 16 : 10);
        }
        return parsed;
    };

    std::vector<RE::IngredientItem*> selected_ingredients;
    std::unordered_set<RE::FormID> seen_ids;
    for (const auto& entry_val : options["ingredientIds"]) {
        const std::uint32_t id = parse_id_val(entry_val);
        if (id == 0) continue;
        auto* ing = RE::TESForm::LookupByID<RE::IngredientItem>(id);
        if (ing == nullptr) {
            return rejected("One of the selected ingredients is unavailable.");
        }
        if (!seen_ids.insert(ing->GetFormID()).second) {
            return rejected("Each vessel slot must hold a different ingredient.");
        }
        selected_ingredients.push_back(ing);
    }
    if (selected_ingredients.size() < 2 || selected_ingredients.size() > 3) {
        return rejected("Select 2 or 3 distinct ingredients to combine.");
    }

    std::int32_t craft_count = std::clamp(options.value("count", 1), 1, 500);
    std::string custom_name = options.value("customName", std::string{});
    while (!custom_name.empty() && std::isspace(static_cast<unsigned char>(custom_name.front()))) custom_name.erase(custom_name.begin());
    while (!custom_name.empty() && std::isspace(static_cast<unsigned char>(custom_name.back()))) custom_name.pop_back();
    if (custom_name.size() > 64) {
        custom_name.resize(64);
    }

    const auto inv = player->GetInventory();
    struct IngInvState {
        RE::IngredientItem* ing{ nullptr };
        std::int32_t count{ 0 };
        const RE::InventoryEntryData* entry{ nullptr };
        std::string name;
    };
    std::vector<IngInvState> ing_states;
    ing_states.reserve(selected_ingredients.size());
    for (auto* ing : selected_ingredients) {
        const auto inv_it = inv.find(ing);
        const std::int32_t carried = inv_it != inv.end() ? std::max<std::int32_t>(0, inv_it->second.first) : 0;
        if (carried < craft_count) {
            const char* raw_n = ing->GetName();
            const std::string n = (raw_n != nullptr && raw_n[0] != '\0') ? StripLeadingDollar(raw_n) : std::string("ingredients");
            return rejected("Insufficient " + n + " carried.");
        }
        const char* raw_n = ing->GetFullName();
        if (raw_n == nullptr || raw_n[0] == '\0') raw_n = ing->GetName();
        ing_states.push_back({
            ing,
            carried,
            inv_it != inv.end() ? inv_it->second.second.get() : nullptr,
            (raw_n != nullptr && raw_n[0] != '\0') ? StripLeadingDollar(raw_n) : std::string("Ingredient")
        });
    }

    std::unordered_map<RE::FormID, int> known_effect_owners;
    bool has_shared_known_effect = false;
    for (auto* ing : selected_ingredients) {
        std::unordered_set<RE::FormID> per_ing_known;
        const std::uint32_t max_slots = std::min<std::uint32_t>(4U, ing->effects.size());
        for (std::uint32_t slot = 0; slot < max_slots; ++slot) {
            const bool known = (ing->gamedata.knownEffectFlags & (static_cast<std::uint16_t>(1U) << slot)) != 0U;
            const auto* eff = ing->effects[slot];
            if (!known || eff == nullptr || eff->baseEffect == nullptr) continue;
            const auto eid = eff->baseEffect->GetFormID();
            if (eid != 0 && per_ing_known.insert(eid).second) {
                if (++known_effect_owners[eid] >= 2) {
                    has_shared_known_effect = true;
                }
            }
        }
    }
    if (!has_shared_known_effect) {
        craft_count = 1;
    }

    const AlchemyPowerContext ctx = ComputeAlchemyPowerContext(player);
    EvaluatedAlchemyResult eval = EvaluateAlchemyCombination(ctx, selected_ingredients);
    if (!eval.valid || eval.effects.empty()) {
        auto* active_workbench = ResolveActiveWorkbenchRef(player, g_active_alchemy_station.furniture_ref);
        ArmVirtualCraftingProxy(player, active_workbench);
        for (const auto& st : ing_states) {
            player->RemoveItem(st.ing, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            ConsumePlayerStolenCount(st.ing, st.entry, st.count, 1, std::max(0, st.count - 1), false);
        }
        RE::PlaySound("UIMenuCancel");
        skyultra::InvalidateSharedGameDataCache();
        auto updated_snapshot = ReadAlchemySnapshot(Json::object());
        return {
            { "accepted", true },
            { "failedCombination", true },
            { "isPoison", false },
            { "craftedName", "" },
            { "craftedCount", 0 },
            { "goldValue", 0 },
            { "xpGained", 0.0F },
            { "discoveredEffects", Json::array() },
            { "message", "Potion creation failed — the selected ingredients share no matching effects." },
            { "snapshot", std::move(updated_snapshot) }
        };
    }

    RE::CreatedObjPtr<RE::AlchemyItem> created_potion;
    if (eval.is_poison) {
        using add_poison_t = void (*)(
            RE::BGSCreatedObjectManager*,
            RE::CreatedObjPtr<RE::AlchemyItem>&,
            RE::BSTArray<RE::Effect>&);
        static REL::Relocation<add_poison_t> add_poison_fn{ RELOCATION_ID(35266, 36168) };
        add_poison_fn(created_mgr, created_potion, eval.effects);
    } else {
        created_mgr->AddPotion(created_potion, eval.effects);
    }
    auto* potion_item = created_potion.get();
    if (potion_item == nullptr) {
        return rejected("Skyrim could not register the brewed alchemy form.");
    }

    const char* raw_potion_full = potion_item->GetFullName();
    const std::string lower_full = raw_potion_full != nullptr
        ? LowerAscii(StripLeadingDollar(raw_potion_full))
        : std::string{};
    if (lower_full.empty() || lower_full == "potion" || lower_full == "poison" ||
        lower_full == "created potion" || lower_full == "created poison") {
        potion_item->SetFullName(eval.default_name.c_str());
    }
    if (potion_item->GetGoldValue() <= 0 && eval.gold_value > 0) {
        potion_item->data.costOverride = eval.gold_value;
        potion_item->data.flags.set(RE::AlchemyItem::AlchemyFlag::kCostOverride);
    }
    if (potion_item->weight <= 0.0F) {
        potion_item->weight = 0.5F;
    }

    Json discovered_json = Json::array();
    std::unordered_set<std::string> discovered_seen;
    for (auto* ing : selected_ingredients) {
        const std::uint32_t max_slots = std::min<std::uint32_t>(4U, ing->effects.size());
        for (std::uint32_t slot = 0; slot < max_slots; ++slot) {
            const auto* eff = ing->effects[slot];
            if (eff == nullptr || eff->baseEffect == nullptr) continue;
            auto* setting = eff->baseEffect;
            const bool matched = std::any_of(
                eval.matched_settings.begin(),
                eval.matched_settings.end(),
                [setting](const RE::EffectSetting* m) noexcept { return m == setting; });
            if (!matched) continue;
            const bool was_known = (ing->gamedata.knownEffectFlags & (static_cast<std::uint16_t>(1U) << slot)) != 0U;
            if (!was_known && ing->LearnEffect(slot)) {
                const char* raw_en = setting->GetFullName();
                if (raw_en == nullptr || raw_en[0] == '\0') raw_en = setting->GetName();
                const std::string en = (raw_en != nullptr && raw_en[0] != '\0') ? StripLeadingDollar(raw_en) : std::string("Alchemical Effect");
                if (discovered_seen.insert(en).second) {
                    discovered_json.push_back(en);
                }
            }
        }
    }

    auto* active_workbench = ResolveActiveWorkbenchRef(player, g_active_alchemy_station.furniture_ref);
    ArmVirtualCraftingProxy(player, active_workbench);

    const auto make_custom_xlist = [&custom_name]() -> RE::ExtraDataList* {
        if (custom_name.empty()) {
            return nullptr;
        }
        auto* xl = new RE::ExtraDataList();
        xl->Add(new RE::ExtraTextDisplayData(custom_name.c_str()));
        return xl;
    };

    if (potion_item->IsDynamicForm()) {
        for (std::int32_t ref_i = 0; ref_i < craft_count; ++ref_i) {
            created_mgr->IncrementRef(potion_item);
        }
    }

    if (craft_count <= 100) {
        std::vector<std::int32_t> remaining;
        remaining.reserve(ing_states.size());
        for (const auto& st : ing_states) remaining.push_back(st.count);
        for (std::int32_t iter = 0; iter < craft_count; ++iter) {
            for (std::size_t si = 0; si < ing_states.size(); ++si) {
                const auto& st = ing_states[si];
                const std::int32_t before = remaining[si];
                player->RemoveItem(st.ing, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                remaining[si] = std::max(0, before - 1);
                ConsumePlayerStolenCount(st.ing, st.entry, before, 1, remaining[si], false);
            }
            player->AddObjectToContainer(potion_item, make_custom_xlist(), 1, nullptr);
            EmitNativeCraftedItemEvents(
                player,
                g_active_alchemy_station.furniture_ref,
                potion_item,
                true,
                false,
                false);
        }
    } else {
        for (const auto& st : ing_states) {
            player->RemoveItem(st.ing, craft_count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            ConsumePlayerStolenCount(st.ing, st.entry, st.count, craft_count, std::max(0, st.count - craft_count), false);
        }
        player->AddObjectToContainer(potion_item, make_custom_xlist(), craft_count, nullptr);
        for (std::int32_t iter = 0; iter < craft_count; ++iter) {
            EmitNativeCraftedItemEvents(
                player,
                g_active_alchemy_station.furniture_ref,
                potion_item,
                true,
                false,
                false);
        }
    }

    const std::int32_t final_gold = potion_item->GetGoldValue() > 0 ? potion_item->GetGoldValue() : eval.gold_value;
    const float xp_gained = std::max(1.0F, static_cast<float>(final_gold) * ctx.gold_mult) * static_cast<float>(craft_count);
    player->UseSkill(RE::ActorValue::kAlchemy, xp_gained, nullptr);
    RE::PlaySound("ITMPotionUse");
    skyultra::InvalidateSharedGameDataCache();

    const std::string output_name = !custom_name.empty() ? custom_name : eval.default_name;
    auto updated_snapshot = ReadAlchemySnapshot(Json::object());
    std::string msg = "Brewed " + (craft_count > 1 ? (std::to_string(craft_count) + "x ") : std::string{}) + output_name + ".";
    if (!discovered_json.empty()) {
        msg += " Discovered " + std::to_string(discovered_json.size()) + " effect" + (discovered_json.size() == 1 ? "." : "s.");
    }
    return {
        { "accepted", true },
        { "failedCombination", false },
        { "isPoison", eval.is_poison },
        { "craftedName", output_name },
        { "craftedCount", craft_count },
        { "goldValue", final_gold },
        { "xpGained", std::round(xp_gained * 10.0F) / 10.0F },
        { "discoveredEffects", std::move(discovered_json) },
        { "message", std::move(msg) },
        { "snapshot", std::move(updated_snapshot) }
    };
}

Json PerformEatIngredient(const Json& options)
{
    const auto rejected = [](std::string message) -> Json {
        return { { "accepted", false }, { "message", std::move(message) } };
    };
    if (!options.is_object()) {
        return rejected("Taste ingredient parameters must be an object.");
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        return rejected("Player alchemy state is unavailable.");
    }
    const std::uint32_t ing_id = ParseItemFormIdField(options, "ingredientId");
    auto* ing = ing_id != 0 ? RE::TESForm::LookupByID<RE::IngredientItem>(ing_id) : nullptr;
    if (ing == nullptr) {
        return rejected("Select an ingredient to sample.");
    }
    const auto inv = player->GetInventory();
    const auto inv_it = inv.find(ing);
    const std::int32_t carried = inv_it != inv.end() ? std::max<std::int32_t>(0, inv_it->second.first) : 0;
    if (carried <= 0) {
        return rejected("You no longer carry that ingredient.");
    }

    const char* raw_n = ing->GetFullName();
    if (raw_n == nullptr || raw_n[0] == '\0') raw_n = ing->GetName();
    const std::string ing_name = (raw_n != nullptr && raw_n[0] != '\0') ? StripLeadingDollar(raw_n) : std::string("Ingredient");

    const AlchemyPowerContext ctx = ComputeAlchemyPowerContext(player);
    const std::uint32_t max_reveal = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(std::clamp(ctx.experimenter_effects, 1, 4)),
        static_cast<std::uint32_t>(ing->effects.size()));
    std::vector<std::uint32_t> slots_to_reveal;
    for (std::uint32_t slot = 0; slot < max_reveal; ++slot) {
        const bool known = (ing->gamedata.knownEffectFlags & (static_cast<std::uint16_t>(1U) << slot)) != 0U;
        if (!known && ing->effects[slot] != nullptr && ing->effects[slot]->baseEffect != nullptr) {
            slots_to_reveal.push_back(slot);
        }
    }
    if (slots_to_reveal.empty()) {
        return rejected("You already know the first " + std::to_string(max_reveal) + " effect(s) of " + ing_name + " — mix it at the lab to discover deeper traits.");
    }

    // Clear any lingering crafting menu proxy so SkyRewrite logs normal ingredient consumption + effect learning.
    ClearVirtualCraftingProxyUnlocked();
    player->RemoveItem(ing, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    ConsumePlayerStolenCount(ing, inv_it != inv.end() ? inv_it->second.second.get() : nullptr, carried, 1, std::max(0, carried - 1), false);

    Json discovered_json = Json::array();
    for (const auto slot : slots_to_reveal) {
        if (ing->LearnEffect(slot)) {
            if (const auto* eff = ing->effects[slot]; eff != nullptr && eff->baseEffect != nullptr) {
                const char* raw_en = eff->baseEffect->GetFullName();
                if (raw_en == nullptr || raw_en[0] == '\0') raw_en = eff->baseEffect->GetName();
                discovered_json.push_back((raw_en != nullptr && raw_en[0] != '\0') ? StripLeadingDollar(raw_en) : std::string("Alchemical Effect"));
            }
        }
    }

    if (!discovered_json.empty()) {
        player->UseSkill(RE::ActorValue::kAlchemy, 2.0F * static_cast<float>(discovered_json.size()), nullptr);
    }
    RE::PlaySound("UIMenuOK");
    skyultra::InvalidateSharedGameDataCache();

    auto updated_snapshot = ReadAlchemySnapshot(Json::object());
    std::string msg = "Sampled " + ing_name + ": learned ";
    for (std::size_t i = 0; i < discovered_json.size(); ++i) {
        if (i > 0) msg += ", ";
        msg += discovered_json[i].get<std::string>();
    }
    msg += ".";
    return {
        { "accepted", true },
        { "ingredientName", ing_name },
        { "discoveredEffects", std::move(discovered_json) },
        { "message", std::move(msg) },
        { "snapshot", std::move(updated_snapshot) }
    };
}

[[nodiscard]] std::string SkillActorValueFriendlyName(RE::ActorValue av) noexcept
{
    switch (av) {
    case RE::ActorValue::kOneHanded: return "One-Handed";
    case RE::ActorValue::kTwoHanded: return "Two-Handed";
    case RE::ActorValue::kArchery: return "Archery";
    case RE::ActorValue::kBlock: return "Block";
    case RE::ActorValue::kSmithing: return "Smithing";
    case RE::ActorValue::kHeavyArmor: return "Heavy Armor";
    case RE::ActorValue::kLightArmor: return "Light Armor";
    case RE::ActorValue::kPickpocket: return "Pickpocket";
    case RE::ActorValue::kLockpicking: return "Lockpicking";
    case RE::ActorValue::kSneak: return "Sneak";
    case RE::ActorValue::kAlchemy: return "Alchemy";
    case RE::ActorValue::kSpeech: return "Speech";
    case RE::ActorValue::kAlteration: return "Alteration";
    case RE::ActorValue::kConjuration: return "Conjuration";
    case RE::ActorValue::kDestruction: return "Destruction";
    case RE::ActorValue::kIllusion: return "Illusion";
    case RE::ActorValue::kRestoration: return "Restoration";
    case RE::ActorValue::kEnchanting: return "Enchanting";
    default: return {};
    }
}

struct ActiveBookMenuState {
    bool active{ false };
    RE::FormID book_form_id{ 0 };
    RE::ObjectRefHandle target_ref{};
    std::string title;
    std::string text;
    std::vector<std::string> pages;
    std::vector<std::string> images;
    std::string book_style{ "book" };
    bool can_take{ false };
    bool is_stealing{ false };
    bool taken{ false };
    std::int32_t value{ 0 };
    float weight{ 0.0F };
    bool teaches_skill{ false };
    std::string skill_name;
    bool teaches_spell{ false };
    std::string spell_name;
};
ActiveBookMenuState g_active_book_menu;

[[nodiscard]] bool CaptureActiveBookMenuUnlocked()
{
    g_active_book_menu = {};
    auto* book = RE::BookMenu::GetTargetForm();
    if (book == nullptr) {
        return false;
    }
    auto target_ref = RE::BookMenu::GetTargetReference();
    auto* player = RE::PlayerCharacter::GetSingleton();

    const char* raw_title = target_ref != nullptr ? target_ref->GetDisplayFullName() : nullptr;
    if (raw_title == nullptr || raw_title[0] == '\0') {
        raw_title = book->GetFullName();
    }
    if (raw_title == nullptr || raw_title[0] == '\0') {
        raw_title = book->GetName();
    }
    std::string title = (raw_title != nullptr && raw_title[0] != '\0')
        ? StripLeadingDollar(raw_title)
        : std::string("Tome");

    RE::BSString raw_desc = RE::BookMenu::GetDescription();
    std::string desc_str = raw_desc.c_str() != nullptr ? std::string(raw_desc.c_str()) : std::string{};
    if (desc_str.empty()) {
        RE::BSString book_desc;
        book->GetDescription(book_desc, target_ref.get());
        if (book_desc.empty()) {
            book->GetDescription(book_desc, book);
        }
        if (book_desc.empty()) {
            book->itemCardDescription.GetDescription(book_desc, target_ref.get());
        }
        if (book_desc.c_str() != nullptr) {
            desc_str = book_desc.c_str();
        }
    }

    const char* p_name = player != nullptr ? player->GetName() : nullptr;
    auto parsed = ParseBookContent(
        desc_str,
        title,
        (p_name != nullptr && p_name[0] != '\0') ? p_name : "Dragonborn");
    if (parsed.images.empty()) {
        ExtractBookModelTextures(book, title, parsed.images, parsed.text.empty());
    }

    const auto lower_title = LowerAscii(title);
    std::string book_style = "book";
    if (lower_title.find("map") != std::string::npos || (!parsed.images.empty() && parsed.text.size() < 120)) {
        book_style = "map";
    } else if (lower_title.find("note") != std::string::npos ||
               lower_title.find("letter") != std::string::npos ||
               lower_title.find("recipe") != std::string::npos ||
               lower_title.find("bounty") != std::string::npos ||
               lower_title.find("orders") != std::string::npos ||
               lower_title.find("missive") != std::string::npos ||
               lower_title.find("journal") != std::string::npos ||
               (parsed.pages.size() <= 1 && parsed.text.size() < 900)) {
        book_style = "note";
    }

    const bool has_world_ref = target_ref != nullptr && !target_ref->IsDeleted() && !target_ref->IsDisabled();
    const bool can_take = has_world_ref && book->CanBeTaken();
    const bool is_stealing = can_take && player != nullptr && player->WouldBeStealing(target_ref.get());

    g_active_book_menu.active = true;
    g_active_book_menu.book_form_id = book->GetFormID();
    g_active_book_menu.target_ref = target_ref ? target_ref->CreateRefHandle() : RE::ObjectRefHandle{};
    g_active_book_menu.title = std::move(title);
    g_active_book_menu.text = parsed.text.empty()
        ? (!parsed.images.empty() ? std::string{} : ("[" + g_active_book_menu.title + "]\n\nThis note or book has no inscription text."))
        : std::move(parsed.text);
    g_active_book_menu.pages = std::move(parsed.pages);
    g_active_book_menu.images = std::move(parsed.images);
    g_active_book_menu.book_style = std::move(book_style);
    g_active_book_menu.can_take = can_take;
    g_active_book_menu.is_stealing = is_stealing;
    g_active_book_menu.taken = false;
    g_active_book_menu.value = std::max(0, book->GetGoldValue());
    g_active_book_menu.weight = std::round(std::max(0.0F, book->GetWeight()) * 100.0F) / 100.0F;
    g_active_book_menu.teaches_skill = book->TeachesSkill();
    if (g_active_book_menu.teaches_skill) {
        g_active_book_menu.skill_name = SkillActorValueFriendlyName(book->GetSkill());
    }
    g_active_book_menu.teaches_spell = book->TeachesSpell();
    if (g_active_book_menu.teaches_spell) {
        if (auto* spell = book->GetSpell(); spell != nullptr) {
            const char* sname = spell->GetFullName();
            if (sname != nullptr && sname[0] != '\0') {
                g_active_book_menu.spell_name = StripLeadingDollar(sname);
            }
        }
    }
    return true;
}

struct ActivateKeyBindingInfo {
    std::string device{ "keyboard" };
    std::string label{ "E" };
    std::string code{ "KeyE" };
    std::string key{ "e" };
    std::int32_t mouse_button{ -1 };
};

[[nodiscard]] ActivateKeyBindingInfo ResolveKeyboardActivateKeyInfo(std::uint32_t kb_key)
{
    ActivateKeyBindingInfo out{};
    using Key = RE::BSKeyboardDevice::Keys::Key;
    switch (kb_key) {
    case Key::kA: return { "keyboard", "A", "KeyA", "a", -1 };
    case Key::kB: return { "keyboard", "B", "KeyB", "b", -1 };
    case Key::kC: return { "keyboard", "C", "KeyC", "c", -1 };
    case Key::kD: return { "keyboard", "D", "KeyD", "d", -1 };
    case Key::kE: return { "keyboard", "E", "KeyE", "e", -1 };
    case Key::kF: return { "keyboard", "F", "KeyF", "f", -1 };
    case Key::kG: return { "keyboard", "G", "KeyG", "g", -1 };
    case Key::kH: return { "keyboard", "H", "KeyH", "h", -1 };
    case Key::kI: return { "keyboard", "I", "KeyI", "i", -1 };
    case Key::kJ: return { "keyboard", "J", "KeyJ", "j", -1 };
    case Key::kK: return { "keyboard", "K", "KeyK", "k", -1 };
    case Key::kL: return { "keyboard", "L", "KeyL", "l", -1 };
    case Key::kM: return { "keyboard", "M", "KeyM", "m", -1 };
    case Key::kN: return { "keyboard", "N", "KeyN", "n", -1 };
    case Key::kO: return { "keyboard", "O", "KeyO", "o", -1 };
    case Key::kP: return { "keyboard", "P", "KeyP", "p", -1 };
    case Key::kQ: return { "keyboard", "Q", "KeyQ", "q", -1 };
    case Key::kR: return { "keyboard", "R", "KeyR", "r", -1 };
    case Key::kS: return { "keyboard", "S", "KeyS", "s", -1 };
    case Key::kT: return { "keyboard", "T", "KeyT", "t", -1 };
    case Key::kU: return { "keyboard", "U", "KeyU", "u", -1 };
    case Key::kV: return { "keyboard", "V", "KeyV", "v", -1 };
    case Key::kW: return { "keyboard", "W", "KeyW", "w", -1 };
    case Key::kX: return { "keyboard", "X", "KeyX", "x", -1 };
    case Key::kY: return { "keyboard", "Y", "KeyY", "y", -1 };
    case Key::kZ: return { "keyboard", "Z", "KeyZ", "z", -1 };
    case Key::kNum0: return { "keyboard", "0", "Digit0", "0", -1 };
    case Key::kNum1: return { "keyboard", "1", "Digit1", "1", -1 };
    case Key::kNum2: return { "keyboard", "2", "Digit2", "2", -1 };
    case Key::kNum3: return { "keyboard", "3", "Digit3", "3", -1 };
    case Key::kNum4: return { "keyboard", "4", "Digit4", "4", -1 };
    case Key::kNum5: return { "keyboard", "5", "Digit5", "5", -1 };
    case Key::kNum6: return { "keyboard", "6", "Digit6", "6", -1 };
    case Key::kNum7: return { "keyboard", "7", "Digit7", "7", -1 };
    case Key::kNum8: return { "keyboard", "8", "Digit8", "8", -1 };
    case Key::kNum9: return { "keyboard", "9", "Digit9", "9", -1 };
    case Key::kSpacebar: return { "keyboard", "Space", "Space", " ", -1 };
    case Key::kEnter: return { "keyboard", "Enter", "Enter", "Enter", -1 };
    case Key::kKP_Enter: return { "keyboard", "NumEnter", "NumpadEnter", "Enter", -1 };
    case Key::kTab: return { "keyboard", "Tab", "Tab", "Tab", -1 };
    case Key::kBackspace: return { "keyboard", "Backspace", "Backspace", "Backspace", -1 };
    case Key::kCapsLock: return { "keyboard", "Caps", "CapsLock", "CapsLock", -1 };
    case Key::kLeftShift: return { "keyboard", "LShift", "ShiftLeft", "Shift", -1 };
    case Key::kRightShift: return { "keyboard", "RShift", "ShiftRight", "Shift", -1 };
    case Key::kLeftControl: return { "keyboard", "LCtrl", "ControlLeft", "Control", -1 };
    case Key::kRightControl: return { "keyboard", "RCtrl", "ControlRight", "Control", -1 };
    case Key::kLeftAlt: return { "keyboard", "LAlt", "AltLeft", "Alt", -1 };
    case Key::kRightAlt: return { "keyboard", "RAlt", "AltRight", "Alt", -1 };
    case Key::kF1: return { "keyboard", "F1", "F1", "F1", -1 };
    case Key::kF2: return { "keyboard", "F2", "F2", "F2", -1 };
    case Key::kF3: return { "keyboard", "F3", "F3", "F3", -1 };
    case Key::kF4: return { "keyboard", "F4", "F4", "F4", -1 };
    case Key::kF5: return { "keyboard", "F5", "F5", "F5", -1 };
    case Key::kF6: return { "keyboard", "F6", "F6", "F6", -1 };
    case Key::kF7: return { "keyboard", "F7", "F7", "F7", -1 };
    case Key::kF8: return { "keyboard", "F8", "F8", "F8", -1 };
    case Key::kF9: return { "keyboard", "F9", "F9", "F9", -1 };
    case Key::kF10: return { "keyboard", "F10", "F10", "F10", -1 };
    case Key::kF11: return { "keyboard", "F11", "F11", "F11", -1 };
    case Key::kF12: return { "keyboard", "F12", "F12", "F12", -1 };
    case Key::kKP_0: return { "keyboard", "Num0", "Numpad0", "0", -1 };
    case Key::kKP_1: return { "keyboard", "Num1", "Numpad1", "1", -1 };
    case Key::kKP_2: return { "keyboard", "Num2", "Numpad2", "2", -1 };
    case Key::kKP_3: return { "keyboard", "Num3", "Numpad3", "3", -1 };
    case Key::kKP_4: return { "keyboard", "Num4", "Numpad4", "4", -1 };
    case Key::kKP_5: return { "keyboard", "Num5", "Numpad5", "5", -1 };
    case Key::kKP_6: return { "keyboard", "Num6", "Numpad6", "6", -1 };
    case Key::kKP_7: return { "keyboard", "Num7", "Numpad7", "7", -1 };
    case Key::kKP_8: return { "keyboard", "Num8", "Numpad8", "8", -1 };
    case Key::kKP_9: return { "keyboard", "Num9", "Numpad9", "9", -1 };
    case Key::kKP_Plus: return { "keyboard", "Num+", "NumpadAdd", "+", -1 };
    case Key::kKP_Subtract: return { "keyboard", "Num-", "NumpadSubtract", "-", -1 };
    case Key::kKP_Multiply: return { "keyboard", "Num*", "NumpadMultiply", "*", -1 };
    case Key::kKP_Divide: return { "keyboard", "Num/", "NumpadDivide", "/", -1 };
    case Key::kKP_Decimal: return { "keyboard", "Num.", "NumpadDecimal", ".", -1 };
    case Key::kMinus: return { "keyboard", "-", "Minus", "-", -1 };
    case Key::kEquals: return { "keyboard", "=", "Equal", "=", -1 };
    case Key::kBracketLeft: return { "keyboard", "[", "BracketLeft", "[", -1 };
    case Key::kBracketRight: return { "keyboard", "]", "BracketRight", "]", -1 };
    case Key::kSemicolon: return { "keyboard", ";", "Semicolon", ";", -1 };
    case Key::kApostrophe: return { "keyboard", "'", "Quote", "'", -1 };
    case Key::kTilde: return { "keyboard", "`", "Backquote", "`", -1 };
    case Key::kBackslash: return { "keyboard", "\\", "Backslash", "\\", -1 };
    case Key::kComma: return { "keyboard", ",", "Comma", ",", -1 };
    case Key::kPeriod: return { "keyboard", ".", "Period", ".", -1 };
    case Key::kSlash: return { "keyboard", "/", "Slash", "/", -1 };
    case Key::kInsert: return { "keyboard", "Ins", "Insert", "Insert", -1 };
    case Key::kDelete: return { "keyboard", "Del", "Delete", "Delete", -1 };
    case Key::kHome: return { "keyboard", "Home", "Home", "Home", -1 };
    case Key::kEnd: return { "keyboard", "End", "End", "End", -1 };
    case Key::kPageUp: return { "keyboard", "PgUp", "PageUp", "PageUp", -1 };
    case Key::kPageDown: return { "keyboard", "PgDn", "PageDown", "PageDown", -1 };
    case Key::kUp: return { "keyboard", "Up", "ArrowUp", "ArrowUp", -1 };
    case Key::kDown: return { "keyboard", "Down", "ArrowDown", "ArrowDown", -1 };
    case Key::kLeft: return { "keyboard", "Left", "ArrowLeft", "ArrowLeft", -1 };
    case Key::kRight: return { "keyboard", "Right", "ArrowRight", "ArrowRight", -1 };
    default: break;
    }
    const LONG lparam = static_cast<LONG>((kb_key & 0xFFU) << 16U) |
        ((kb_key & 0x80U) != 0U ? (1L << 24) : 0L);
    wchar_t key_buf[64]{};
    if (const int len = GetKeyNameTextW(lparam, key_buf, static_cast<int>(std::size(key_buf))); len > 0) {
        std::string label_utf8;
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, key_buf, len, nullptr, 0, nullptr, nullptr);
        if (bytes > 0) {
            label_utf8.resize(static_cast<std::size_t>(bytes));
            (void)WideCharToMultiByte(CP_UTF8, 0, key_buf, len, label_utf8.data(), bytes, nullptr, nullptr);
            if (!label_utf8.empty()) {
                out.label = label_utf8;
                out.code = label_utf8.size() == 1 ? ("Key" + label_utf8) : label_utf8;
                out.key = label_utf8;
                return out;
            }
        }
    }
    return out;
}

[[nodiscard]] std::string ResolveMouseButtonLabel(std::uint32_t mouse_key)
{
    using MKey = RE::BSWin32MouseDevice::Keys::Key;
    switch (mouse_key) {
    case MKey::kLeftButton: return "M1";
    case MKey::kRightButton: return "M2";
    case MKey::kMiddleButton: return "M3";
    case MKey::kButton3: return "M4";
    case MKey::kButton4: return "M5";
    case MKey::kButton5: return "M6";
    case MKey::kButton6: return "M7";
    case MKey::kButton7: return "M8";
    case MKey::kWheelUp: return "WheelUp";
    case MKey::kWheelDown: return "WheelDown";
    default: return "M" + std::to_string(mouse_key + 1U);
    }
}

[[nodiscard]] ActivateKeyBindingInfo ResolveSkyrimActivateKeyInfo()
{
    ActivateKeyBindingInfo out{};
    auto* control_map = RE::ControlMap::GetSingleton();
    auto* user_events = RE::UserEvents::GetSingleton();
    if (control_map == nullptr || user_events == nullptr || user_events->activate.empty()) {
        return out;
    }

    if (control_map->GetRuntimeData().ignoreKeyboardMouse) {
        const auto pad_key = control_map->GetMappedKey(
            user_events->activate, RE::INPUT_DEVICE::kGamepad, RE::ControlMap::InputContextID::kGameplay);
        switch (pad_key) {
        case RE::BSWin32GamepadDevice::Keys::kA: return { "gamepad", "A", "Enter", "Enter", -1 };
        case RE::BSWin32GamepadDevice::Keys::kB: return { "gamepad", "B", "Escape", "Escape", -1 };
        case RE::BSWin32GamepadDevice::Keys::kX: return { "gamepad", "X", "Tab", "Tab", -1 };
        case RE::BSWin32GamepadDevice::Keys::kY: return { "gamepad", "Y", "Space", " ", -1 };
        case RE::BSWin32GamepadDevice::Keys::kLeftShoulder: return { "gamepad", "LB", "", "", -1 };
        case RE::BSWin32GamepadDevice::Keys::kRightShoulder: return { "gamepad", "RB", "", "", -1 };
        case RE::BSWin32GamepadDevice::Keys::kLeftThumb: return { "gamepad", "LS", "", "", -1 };
        case RE::BSWin32GamepadDevice::Keys::kRightThumb: return { "gamepad", "RS", "", "", -1 };
        case RE::BSWin32GamepadDevice::Keys::kStart: return { "gamepad", "Start", "", "", -1 };
        case RE::BSWin32GamepadDevice::Keys::kBack: return { "gamepad", "Back", "", "", -1 };
        case RE::BSWin32GamepadDevice::Keys::kUp: return { "gamepad", "D-Up", "", "", -1 };
        case RE::BSWin32GamepadDevice::Keys::kDown: return { "gamepad", "D-Down", "", "", -1 };
        case RE::BSWin32GamepadDevice::Keys::kLeft: return { "gamepad", "D-Left", "", "", -1 };
        case RE::BSWin32GamepadDevice::Keys::kRight: return { "gamepad", "D-Right", "", "", -1 };
        default: break;
        }
        return out;
    }

    const auto mouse_key = control_map->GetMappedKey(
        user_events->activate, RE::INPUT_DEVICE::kMouse, RE::ControlMap::InputContextID::kGameplay);
    const auto kb_key = control_map->GetMappedKey(
        user_events->activate, RE::INPUT_DEVICE::kKeyboard, RE::ControlMap::InputContextID::kGameplay);

    const bool mouse_valid = mouse_key != 0xFFU && mouse_key != 0xFFFFFFFFU && mouse_key <= 9U;
    const bool kb_valid = kb_key != 0U && kb_key != 0xFFU && kb_key != 0xFFFFFFFFU;

    if (mouse_valid && (!kb_valid || mouse_key >= RE::BSWin32MouseDevice::Keys::kMiddleButton)) {
        const auto kb_fallback = kb_valid ? ResolveKeyboardActivateKeyInfo(kb_key) : ActivateKeyBindingInfo{};
        return {
            "mouse",
            ResolveMouseButtonLabel(mouse_key),
            kb_valid ? kb_fallback.code : std::string{},
            kb_valid ? kb_fallback.key : std::string{},
            static_cast<std::int32_t>(mouse_key)
        };
    }

    if (kb_valid) {
        return ResolveKeyboardActivateKeyInfo(kb_key);
    }

    if (mouse_valid) {
        return {
            "mouse",
            ResolveMouseButtonLabel(mouse_key),
            std::string{},
            std::string{},
            static_cast<std::int32_t>(mouse_key)
        };
    }

    return out;
}

[[nodiscard]] Json ReadActiveBookSnapshot(const Json& options)
{
    (void)options;
    if (!g_active_book_menu.active) {
        if (!CaptureActiveBookMenuUnlocked()) {
            return { { "available", false } };
        }
    }
    auto ref = g_active_book_menu.target_ref.get();
    auto* player = RE::PlayerCharacter::GetSingleton();
    const bool still_valid_ref = ref != nullptr && !ref->IsDeleted() && !ref->IsDisabled();
    const bool can_take = g_active_book_menu.can_take && !g_active_book_menu.taken && still_valid_ref;
    const bool is_stealing = can_take && player != nullptr ? player->WouldBeStealing(ref.get()) : g_active_book_menu.is_stealing;
    const auto activate_key = ResolveSkyrimActivateKeyInfo();

    return {
        { "available", true },
        { "bookId", std::to_string(g_active_book_menu.book_form_id) },
        { "refId", still_valid_ref ? std::to_string(ref->GetFormID()) : std::string{} },
        { "title", g_active_book_menu.title },
        { "text", g_active_book_menu.text },
        { "pages", g_active_book_menu.pages },
        { "images", g_active_book_menu.images },
        { "bookStyle", g_active_book_menu.book_style },
        { "canTake", can_take },
        { "isStealing", is_stealing },
        { "taken", g_active_book_menu.taken },
        { "value", g_active_book_menu.value },
        { "weight", g_active_book_menu.weight },
        { "teachesSkill", g_active_book_menu.teaches_skill },
        { "skillName", g_active_book_menu.skill_name },
        { "teachesSpell", g_active_book_menu.teaches_spell },
        { "spellName", g_active_book_menu.spell_name },
        { "activateDevice", activate_key.device },
        { "activateMouseButton", activate_key.mouse_button },
        { "activateKeyLabel", activate_key.label },
        { "activateKeyCode", activate_key.code },
        { "activateKey", activate_key.key }
    };
}

[[nodiscard]] Json PerformTakeActiveBook(const Json& options)
{
    (void)options;
    const auto rejected = [](std::string message) -> Json {
        return { { "accepted", false }, { "message", std::move(message) } };
    };
    if (!g_active_book_menu.active) {
        return rejected("No active world book is currently open.");
    }
    if (g_active_book_menu.taken) {
        return rejected("You already took this book.");
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto ref = g_active_book_menu.target_ref.get();
    auto* book = g_active_book_menu.book_form_id != 0
        ? RE::TESForm::LookupByID<RE::TESObjectBOOK>(g_active_book_menu.book_form_id)
        : nullptr;
    if (player == nullptr || ref == nullptr || ref->IsDeleted() || ref->IsDisabled() || !g_active_book_menu.can_take) {
        return rejected("This book cannot be taken.");
    }
    if (book != nullptr && !book->CanBeTaken()) {
        return rejected("This book is fixed in place and cannot be taken.");
    }
    const bool stolen = player->WouldBeStealing(ref.get());
    player->PickUpObject(ref.get(), 1, false, true);
    g_active_book_menu.taken = true;
    if (book != nullptr) {
        RE::SendUIMessage::SendInventoryUpdateMessage(player, book);
    }
    skyultra::InvalidateSharedGameDataCache();

    return {
        { "accepted", true },
        { "taken", true },
        { "stolen", stolen },
        { "title", g_active_book_menu.title },
        { "message", (stolen ? "Stole " : "Took ") + g_active_book_menu.title + "." },
        { "book", ReadActiveBookSnapshot(Json::object()) }
    };
}

std::string_view QuestCategoryName(RE::QUEST_DATA::Type quest_type) noexcept
{
    switch (quest_type) {
    case RE::QUEST_DATA::Type::kMainQuest: return "Main Quest";
    case RE::QUEST_DATA::Type::kMagesGuild: return "College of Winterhold";
    case RE::QUEST_DATA::Type::kThievesGuild: return "Thieves Guild";
    case RE::QUEST_DATA::Type::kDarkBrotherhood: return "Dark Brotherhood";
    case RE::QUEST_DATA::Type::kCompanionsQuest: return "Companions";
    case RE::QUEST_DATA::Type::kMiscellaneous: return "Miscellaneous";
    case RE::QUEST_DATA::Type::kDaedric: return "Daedric";
    case RE::QUEST_DATA::Type::kSideQuest: return "Side Quest";
    case RE::QUEST_DATA::Type::kCivilWar: return "Civil War";
    case RE::QUEST_DATA::Type::kDLC01_Vampire: return "Dawnguard";
    case RE::QUEST_DATA::Type::kDLC02_Dragonborn: return "Dragonborn";
    default: return "Quest";
    }
}

std::pair<Json, std::size_t> ReadActiveQuestsJson()
{
    Json quests = Json::array();
    auto* data_handler = RE::TESDataHandler::GetSingleton();
    if (data_handler == nullptr) {
        return { std::move(quests), 0 };
    }

    std::vector<Json> collected;
    for (auto* quest : data_handler->GetFormArray<RE::TESQuest>()) {
        if (quest == nullptr || !quest->IsRunning() || quest->IsCompleted()) {
            continue;
        }
        const auto* raw_name = quest->GetName();
        if (raw_name == nullptr || raw_name[0] == '\0') {
            continue;
        }
        Json objectives = Json::array();
        for (const auto* objective : quest->objectives) {
            if (objective == nullptr) {
                continue;
            }
            const auto* text = objective->displayText.c_str();
            if (text == nullptr || text[0] == '\0') {
                continue;
            }
            const auto state = objective->state.get();
            if (state != RE::QUEST_OBJECTIVE_STATE::kDisplayed &&
                state != RE::QUEST_OBJECTIVE_STATE::kCompletedDisplayed &&
                state != RE::QUEST_OBJECTIVE_STATE::kFailedDisplayed) {
                continue;
            }
            objectives.push_back({
                { "index", objective->index },
                { "text", text },
                { "completed", state == RE::QUEST_OBJECTIVE_STATE::kCompletedDisplayed ||
                    state == RE::QUEST_OBJECTIVE_STATE::kCompleted },
                { "failed", state == RE::QUEST_OBJECTIVE_STATE::kFailedDisplayed ||
                    state == RE::QUEST_OBJECTIVE_STATE::kFailed }
            });
        }
        if (!quest->IsActive() && objectives.empty()) {
            continue;
        }
        const auto* editor_id = quest->GetFormEditorID();
        collected.push_back({
            { "id", editor_id != nullptr && editor_id[0] != '\0' ?
                std::string(editor_id) : std::to_string(quest->GetFormID()) },
            { "formId", std::to_string(quest->GetFormID()) },
            { "name", raw_name },
            { "stage", quest->GetCurrentStageID() },
            { "active", quest->IsActive() },
            { "type", QuestCategoryName(quest->GetType()) },
            { "objectives", std::move(objectives) }
        });
    }

    std::sort(collected.begin(), collected.end(), [](const Json& left, const Json& right) {
        const bool left_active = left.value("active", false);
        const bool right_active = right.value("active", false);
        if (left_active != right_active) {
            return left_active > right_active;
        }
        return left.value("name", std::string{}) < right.value("name", std::string{});
    });

    const auto total_count = collected.size();
    constexpr std::size_t kMaxQuestsInSnapshot = 24;
    for (std::size_t i = 0; i < collected.size() && i < kMaxQuestsInSnapshot; ++i) {
        quests.push_back(std::move(collected[i]));
    }
    return { std::move(quests), total_count };
}

std::pair<Json, std::size_t> ReadFollowersJson(RE::PlayerCharacter* player)
{
    Json followers = Json::array();
    auto* process_lists = RE::ProcessLists::GetSingleton();
    if (player == nullptr || process_lists == nullptr) {
        return { std::move(followers), 0 };
    }

    const auto player_position = player->GetPosition();
    std::vector<Json> collected;
    process_lists->ForAllActors([&](RE::Actor* actor) {
        if (actor == nullptr || actor == player || actor->IsDisabled() ||
            actor->IsDeleted() || !actor->IsPlayerTeammate()) {
            return RE::BSContainer::ForEachResult::kContinue;
        }
        auto* base = actor->GetActorBase();
        if (base == nullptr || base->GetName() == nullptr || base->GetName()[0] == '\0') {
            return RE::BSContainer::ForEachResult::kContinue;
        }
        const auto position = actor->GetPosition();
        const auto dx = position.x - player_position.x;
        const auto dy = position.y - player_position.y;
        const auto dz = position.z - player_position.z;
        const auto distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        const auto* actor_values = actor->AsActorValueOwner();
        const auto health = actor_values == nullptr ? 0.0F :
            std::max(actor_values->GetActorValue(RE::ActorValue::kHealth), 0.0F);
        const auto health_max = std::max(actor->GetActorValueMax(RE::ActorValue::kHealth), 0.0F);
        const auto armor_rating = actor_values == nullptr ? 0.0F :
            std::max(actor_values->GetActorValue(RE::ActorValue::kDamageResist), 0.0F);
        collected.push_back({
            { "referenceId", std::to_string(actor->GetFormID()) },
            { "baseFormId", std::to_string(base->GetFormID()) },
            { "name", base->GetName() },
            { "race", NpcRaceName(base) },
            { "sex", NpcSexName(base->GetSex()) },
            { "distance", std::isfinite(distance) ? distance : 0.0F },
            { "level", actor->GetLevel() },
            { "follower", true },
            { "dead", actor->IsDead() },
            { "health", health },
            { "healthMax", health_max },
            { "armorRating", armor_rating },
            { "equipment", ReadActorEquipment(actor, false) }
        });
        return RE::BSContainer::ForEachResult::kContinue;
    });

    std::sort(collected.begin(), collected.end(), [](const Json& left, const Json& right) {
        return left.value("distance", 0.0F) < right.value("distance", 0.0F);
    });

    const auto total_count = collected.size();
    constexpr std::size_t kMaxFollowersInSnapshot = 16;
    for (std::size_t i = 0; i < collected.size() && i < kMaxFollowersInSnapshot; ++i) {
        followers.push_back(std::move(collected[i]));
    }
    return { std::move(followers), total_count };
}

struct CompassPoiClassification {
    const char* kind;
    const char* category_label;
    int priority;
};

std::string LowerAsciiString(std::string_view input)
{
    std::string out;
    out.reserve(input.size());
    for (unsigned char ch : input) {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

bool ContainsWordOrPhrase(std::string_view haystack_lower, std::string_view needle_lower) noexcept
{
    return haystack_lower.find(needle_lower) != std::string_view::npos;
}

CompassPoiClassification ClassifyInteriorOrPlaceName(std::string_view raw_name)
{
    const auto lower = LowerAsciiString(raw_name);
    if (ContainsWordOrPhrase(lower, "blacksmith") ||
        ContainsWordOrPhrase(lower, "warmaiden") ||
        ContainsWordOrPhrase(lower, "skyforge") ||
        ContainsWordOrPhrase(lower, "forge") ||
        ContainsWordOrPhrase(lower, "smithy") ||
        ContainsWordOrPhrase(lower, "armory") ||
        ContainsWordOrPhrase(lower, "scorched hammer") ||
        ContainsWordOrPhrase(lower, "missing merchant") ||
        ContainsWordOrPhrase(lower, "riverwood trader") && ContainsWordOrPhrase(lower, "anvil")) {
        return { "blacksmith", "Blacksmith", 88 };
    }
    if (ContainsWordOrPhrase(lower, "inn") ||
        ContainsWordOrPhrase(lower, "tavern") ||
        ContainsWordOrPhrase(lower, "bannered mare") ||
        ContainsWordOrPhrase(lower, "sleeping giant") ||
        ContainsWordOrPhrase(lower, "bee and barb") ||
        ContainsWordOrPhrase(lower, "winking skeever") ||
        ContainsWordOrPhrase(lower, "candlehearth") ||
        ContainsWordOrPhrase(lower, "silver-blood inn") ||
        ContainsWordOrPhrase(lower, "dead man's drink") ||
        ContainsWordOrPhrase(lower, "frozen hearth") ||
        ContainsWordOrPhrase(lower, "four shields") ||
        ContainsWordOrPhrase(lower, "drunken huntsman") ||
        ContainsWordOrPhrase(lower, "new gnisis") ||
        ContainsWordOrPhrase(lower, "ragged flagon") ||
        ContainsWordOrPhrase(lower, "meadery")) {
        return { "inn", "Inn & Tavern", 86 };
    }
    if (ContainsWordOrPhrase(lower, "dragonsreach") ||
        ContainsWordOrPhrase(lower, "blue palace") ||
        ContainsWordOrPhrase(lower, "palace of the kings") ||
        ContainsWordOrPhrase(lower, "understone keep") ||
        ContainsWordOrPhrase(lower, "mistveil keep") ||
        ContainsWordOrPhrase(lower, "castle") ||
        ContainsWordOrPhrase(lower, "jarl") ||
        ContainsWordOrPhrase(lower, "high hrothgar") ||
        ContainsWordOrPhrase(lower, "fort ") ||
        ContainsWordOrPhrase(lower, "keep")) {
        return { "castle", "Castle & Keep", 90 };
    }
    if (ContainsWordOrPhrase(lower, "general goods") ||
        ContainsWordOrPhrase(lower, "belethor") ||
        ContainsWordOrPhrase(lower, "riverwood trader") ||
        ContainsWordOrPhrase(lower, "arcadia's cauldron") ||
        ContainsWordOrPhrase(lower, "apothecary") ||
        ContainsWordOrPhrase(lower, "alchem") ||
        ContainsWordOrPhrase(lower, "pawned prawn") ||
        ContainsWordOrPhrase(lower, "bits and pieces") ||
        ContainsWordOrPhrase(lower, "radient raiment") ||
        ContainsWordOrPhrase(lower, "radiant raiment") ||
        ContainsWordOrPhrase(lower, "angeline's aromatics") ||
        ContainsWordOrPhrase(lower, "fletcher") ||
        ContainsWordOrPhrase(lower, "grave concoctions") ||
        ContainsWordOrPhrase(lower, "elgrim's elixirs") ||
        ContainsWordOrPhrase(lower, "sadri's used wares") ||
        ContainsWordOrPhrase(lower, "white phial") ||
        ContainsWordOrPhrase(lower, "cure") ||
        ContainsWordOrPhrase(lower, "elixir") ||
        ContainsWordOrPhrase(lower, "cauldron") ||
        ContainsWordOrPhrase(lower, "trader") ||
        ContainsWordOrPhrase(lower, "emporium") ||
        ContainsWordOrPhrase(lower, "market") ||
        ContainsWordOrPhrase(lower, "shop") ||
        ContainsWordOrPhrase(lower, "wares") ||
        ContainsWordOrPhrase(lower, "aromatics") ||
        ContainsWordOrPhrase(lower, "raiment") ||
        ContainsWordOrPhrase(lower, "company")) {
        return { "vendor", "Merchant Vendor", 85 };
    }
    if (ContainsWordOrPhrase(lower, "breezehome") ||
        ContainsWordOrPhrase(lower, "honeyside") ||
        ContainsWordOrPhrase(lower, "proudspire") ||
        ContainsWordOrPhrase(lower, "hjerim") ||
        ContainsWordOrPhrase(lower, "vlindrel") ||
        ContainsWordOrPhrase(lower, "vindrel") ||
        ContainsWordOrPhrase(lower, "lakeview") ||
        ContainsWordOrPhrase(lower, "windstad") ||
        ContainsWordOrPhrase(lower, "heljarchen") ||
        ContainsWordOrPhrase(lower, "severin") ||
        ContainsWordOrPhrase(lower, "tundra homestead") ||
        ContainsWordOrPhrase(lower, "hendraheim") ||
        ContainsWordOrPhrase(lower, "myrwatch") ||
        ContainsWordOrPhrase(lower, "shadowfoot") ||
        ContainsWordOrPhrase(lower, "bloodchill") ||
        ContainsWordOrPhrase(lower, "gallows hall") ||
        ContainsWordOrPhrase(lower, "goldenhills") ||
        ContainsWordOrPhrase(lower, "dead man's dread")) {
        return { "home", "Player Home", 96 };
    }
    if (ContainsWordOrPhrase(lower, "jorrvaskr") ||
        ContainsWordOrPhrase(lower, "house") ||
        ContainsWordOrPhrase(lower, "home") ||
        ContainsWordOrPhrase(lower, "manor") ||
        ContainsWordOrPhrase(lower, "cottage") ||
        ContainsWordOrPhrase(lower, "farm") ||
        ContainsWordOrPhrase(lower, "villa") ||
        ContainsWordOrPhrase(lower, "estate") ||
        ContainsWordOrPhrase(lower, "hall of the vigilant")) {
        return { "home", "Home & Residence", 86 };
    }
    if (ContainsWordOrPhrase(lower, "temple") ||
        ContainsWordOrPhrase(lower, "hall of the dead") ||
        ContainsWordOrPhrase(lower, "catacomb") ||
        ContainsWordOrPhrase(lower, "shrine") ||
        ContainsWordOrPhrase(lower, "chapel")) {
        return { "temple", "Temple & Shrine", 80 };
    }
    if (ContainsWordOrPhrase(lower, "skyrim")) {
        return { "city", "Skyrim World", 68 };
    }
    return { "door", "Entrance", 62 };
}

CompassPoiClassification ClassifyMapMarkerType(RE::MARKER_TYPE type, std::string_view name)
{
    const auto by_name = ClassifyInteriorOrPlaceName(name);
    if (std::string_view(by_name.kind) == "blacksmith" ||
        std::string_view(by_name.kind) == "inn" ||
        std::string_view(by_name.kind) == "vendor" ||
        std::string_view(by_name.kind) == "home") {
        return by_name;
    }

    switch (type) {
    case RE::MARKER_TYPE::kCity:
    case RE::MARKER_TYPE::kWhiterunCapitol:
    case RE::MARKER_TYPE::kSolitudeCapitol:
    case RE::MARKER_TYPE::kMarkarthCapitol:
    case RE::MARKER_TYPE::kRiftenCapitol:
    case RE::MARKER_TYPE::kWindhelmCapitol:
        return { "city", "Major City", 92 };
    case RE::MARKER_TYPE::kTown:
    case RE::MARKER_TYPE::kDawnstarCapitol:
    case RE::MARKER_TYPE::kFalkreathCapitol:
    case RE::MARKER_TYPE::kMorthalCapitol:
    case RE::MARKER_TYPE::kWinterholdCapitol:
    case RE::MARKER_TYPE::kDLC02RavenRock:
        return { "city", "Town", 88 };
    case RE::MARKER_TYPE::kSettlement:
    case RE::MARKER_TYPE::kFarm:
    case RE::MARKER_TYPE::kWoodMill:
    case RE::MARKER_TYPE::kWheatMill:
    case RE::MARKER_TYPE::kStable:
    case RE::MARKER_TYPE::kShack:
    case RE::MARKER_TYPE::kNordicDwelling:
        return { "home", "Settlement & Homestead", 82 };
    case RE::MARKER_TYPE::kFort:
    case RE::MARKER_TYPE::kImperialTower:
    case RE::MARKER_TYPE::kNordicTower:
    case RE::MARKER_TYPE::kOrcStronghold:
    case RE::MARKER_TYPE::kRiftenCastle:
    case RE::MARKER_TYPE::kWindhelmCastle:
    case RE::MARKER_TYPE::kWhiterunCastle:
    case RE::MARKER_TYPE::kSolitudeCastle:
    case RE::MARKER_TYPE::kMarkarthCastle:
    case RE::MARKER_TYPE::kWinterholdCastle:
    case RE::MARKER_TYPE::kMorthalCastle:
    case RE::MARKER_TYPE::kFalkreathCastle:
    case RE::MARKER_TYPE::kDawnstarCastle:
    case RE::MARKER_TYPE::kDLC02TelvanniTower:
    case RE::MARKER_TYPE::kDLC02CastleKarstaag:
        return { "castle", "Fort & Keep", 87 };
    case RE::MARKER_TYPE::kCave:
        return { "cave", "Cave", 78 };
    case RE::MARKER_TYPE::kNordicRuin:
    case RE::MARKER_TYPE::kDwemerRuin:
        return { "ruin", "Ancient Ruin", 80 };
    case RE::MARKER_TYPE::kMine:
    case RE::MARKER_TYPE::kSmelter:
        return { "blacksmith", "Mine & Smelter", 79 };
    case RE::MARKER_TYPE::kDragonLair:
        return { "dragon", "Dragon Lair", 89 };
    case RE::MARKER_TYPE::kDoomstone:
    case RE::MARKER_TYPE::kDLC02StandingStone:
        return { "stone", "Standing Stone", 86 };
    case RE::MARKER_TYPE::kAltar:
    case RE::MARKER_TYPE::kShrine:
    case RE::MARKER_TYPE::kDLC02MiraakTemple:
        return { "temple", "Shrine & Sanctuary", 83 };
    case RE::MARKER_TYPE::kCamp:
    case RE::MARKER_TYPE::kGiantCamp:
    case RE::MARKER_TYPE::kImperialCamp:
    case RE::MARKER_TYPE::kStormcloakCamp:
        return { "camp", "Camp", 76 };
    case RE::MARKER_TYPE::kDoor:
        return { "door", "Doorway", 74 };
    case RE::MARKER_TYPE::kQuestTarget:
    case RE::MARKER_TYPE::kQuestTargetDoor:
        return { "quest", "Quest Objective", 100 };
    default:
        return { "dungeon", "Landmark", 74 };
    }
}

float NormalizeCompassAngleDeg(float deg) noexcept
{
    if (!std::isfinite(deg)) {
        return 0.0F;
    }
    float wrapped = std::fmod(deg, 360.0F);
    if (wrapped < 0.0F) {
        wrapped += 360.0F;
    }
    return wrapped;
}

float ComputeWorldBearingDeg(const RE::NiPoint3& from, const RE::NiPoint3& to) noexcept
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    if (std::abs(dx) < 0.001F && std::abs(dy) < 0.001F) {
        return 0.0F;
    }
    const float deg = NormalizeCompassAngleDeg(std::atan2(dx, dy) * 57.295779513F);
    return std::round(deg * 10.0F) * 0.1F;
}

double NormalizeCompassAnglePreciseDeg(double deg) noexcept
{
    if (!std::isfinite(deg)) {
        return 0.0;
    }
    double wrapped = std::fmod(deg, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
    return wrapped;
}

double ReadCompassHeadingPreciseDeg(RE::PlayerCharacter* player) noexcept
{
    constexpr double kRadToDeg = 57.29577951308232;
    if (auto* cam = RE::PlayerCamera::GetSingleton(); cam != nullptr) {
        if (cam->cameraRoot != nullptr) {
            const auto& r = cam->cameraRoot->world.rotate.entry;
            const double fx = static_cast<double>(r[0][1]);
            const double fy = static_cast<double>(r[1][1]);
            if (std::isfinite(fx) && std::isfinite(fy) && (std::abs(fx) > 1e-4 || std::abs(fy) > 1e-4)) {
                return NormalizeCompassAnglePreciseDeg(std::atan2(fx, fy) * kRadToDeg);
            }
        }
        const float cam_yaw = cam->GetRuntimeData2().yaw;
        if (std::isfinite(cam_yaw) && (std::abs(cam_yaw) > 1e-5F || player == nullptr || std::abs(player->GetAngleZ()) < 1e-4F)) {
            return NormalizeCompassAnglePreciseDeg(static_cast<double>(cam_yaw) * kRadToDeg);
        }
    }
    if (player != nullptr) {
        const float angle = player->GetAngleZ();
        if (std::isfinite(angle)) {
            return NormalizeCompassAnglePreciseDeg(static_cast<double>(angle) * kRadToDeg);
        }
    }
    return 0.0;
}

float ReadCompassHeadingDeg(RE::PlayerCharacter* player) noexcept
{
    return NormalizeCompassAngleDeg(
        static_cast<float>(std::round(ReadCompassHeadingPreciseDeg(player) * 100.0) * 0.01));
}

bool IsPassivePreyOrCritterName(std::string_view lower_name) noexcept
{
    if (lower_name.empty()) {
        return false;
    }
    static constexpr std::array<std::string_view, 32> kPassiveWildlifeTokens{
        "deer", "elk", "rabbit", "hare", "fox", "goat", "cow", "cattle",
        "chicken", "hen", "rooster", "horse", "stallion", "mare", "pony",
        "dog", "husky", "hound", "hawk", "salmon", "longfin", "spadetail",
        "histcarp", "perch", "betty", "torchbug", "butterfly", "monarch",
        "moth", "clam", "crab_passive", "dwarven mudcrab"
    };
    for (const auto token : kPassiveWildlifeTokens) {
        if (lower_name.find(token) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

bool IsGenuineHostileThreat(RE::Actor* actor, RE::PlayerCharacter* player, float dist_units) noexcept
{
    if (actor == nullptr || player == nullptr || actor == player ||
        actor->IsDead() || actor->IsDisabled() || actor->IsDeleted() ||
        actor->IsPlayerTeammate() || !actor->IsHostileToActor(player)) {
        return false;
    }
    const char* raw_name = actor->GetDisplayFullName();
    const std::string lower_name = LowerAsciiString(raw_name != nullptr ? raw_name : "");
    if (IsPassivePreyOrCritterName(lower_name)) {
        return false;
    }
    if (const auto* race = actor->GetRace(); race != nullptr) {
        const char* raw_race = race->GetFullName();
        if (raw_race != nullptr && IsPassivePreyOrCritterName(LowerAsciiString(raw_race))) {
            return false;
        }
        const char* raw_edid = race->GetFormEditorID();
        if (raw_edid != nullptr && IsPassivePreyOrCritterName(LowerAsciiString(raw_edid))) {
            return false;
        }
    }

    if (actor->IsInCombat()) {
        return true;
    }

    float aggression = 1.0F;
    float confidence = 2.0F;
    if (auto* av_owner = actor->AsActorValueOwner(); av_owner != nullptr) {
        aggression = av_owner->GetActorValue(RE::ActorValue::kAggression);
        confidence = av_owner->GetActorValue(RE::ActorValue::kConfidence);
    }

    // Cowardly or unaggressive creatures (e.g. fleeing wildlife) are never hostile threats on the compass.
    if (aggression < 1.0F || confidence <= 0.0F) {
        return false;
    }

    (void)dist_units;
    return true;
}

const char* ElevationRelationLabel(float dz) noexcept
{
    if (dz > 260.0F) {
        return "above";
    }
    if (dz < -260.0F) {
        return "below";
    }
    return "level";
}

struct CachedWorldCompassMarker {
    std::string id;
    std::string name;
    std::string kind;
    std::string category_label;
    std::string base_category_label;
    RE::NiPoint3 position{};
    bool discovered{ true };
    int priority{ 70 };
    RE::ObjectRefHandle marker_ref{};
    bool is_map_marker{ false };
};

std::atomic<float> g_compass_marker_radius_meters{ 50.0F };

struct CompassStaticMarkerCache {
    bool valid{ false };
    std::uint64_t timestamp_ms{ 0 };
    std::uint32_t cell_form_id{ 0 };
    std::uint32_t world_form_id{ 0 };
    float radius_meters{ 50.0F };
    RE::NiPoint3 scan_origin{};
    std::vector<CachedWorldCompassMarker> markers;
};
CompassStaticMarkerCache g_compass_static_cache;

struct CompassLiveOutputCache {
    bool valid{ false };
    std::uint64_t version{ 0 };
    std::uint64_t timestamp_ms{ 0 };
    std::uint32_t cell_form_id{ 0 };
    std::uint32_t world_form_id{ 0 };
    float radius_meters{ 50.0F };
    RE::NiPoint3 player_pos{};
    Json markers;
    std::string serialized;
};
CompassLiveOutputCache g_compass_live_cache;
std::unordered_set<const RE::MapMarkerData*> g_discovered_marker_data;

[[nodiscard]] bool IsMapMarkerDiscovered(const RE::MapMarkerData* map_data) noexcept
{
    return map_data != nullptr &&
        (map_data->flags.any(RE::MapMarkerData::Flag::kCanTravelTo) ||
         g_discovered_marker_data.contains(map_data));
}

std::string ResolveQuestAliasText(RE::TESQuest* quest, std::string text, RE::TESObjectREFR* fallback_target_ref)
{
    if (quest == nullptr || text.empty()) {
        return text;
    }

    auto resolve_alias_actor_name = [&](const std::string& alias_key) -> std::string {
        const auto lower_key = LowerAsciiString(StripLeadingDollar(alias_key));
        for (auto* base_alias : quest->aliases) {
            if (base_alias == nullptr) {
                continue;
            }
            const char* raw_alias_name = base_alias->aliasName.c_str();
            if (raw_alias_name == nullptr || raw_alias_name[0] == '\0') {
                continue;
            }
            if (LowerAsciiString(raw_alias_name) == lower_key) {
                auto aliased_handle = quest->GetAliasedRef(base_alias->aliasID);
                if (auto aliased_ref = aliased_handle.get(); aliased_ref != nullptr) {
                    if (const char* ref_name = aliased_ref->GetDisplayFullName(); ref_name != nullptr && ref_name[0] != '\0') {
                        return StripLeadingDollar(ref_name);
                    }
                    if (auto* base_obj = aliased_ref->GetBaseObject(); base_obj != nullptr) {
                        if (const char* base_name = base_obj->GetName(); base_name != nullptr && base_name[0] != '\0') {
                            return StripLeadingDollar(base_name);
                        }
                    }
                }
            }
        }
        if (fallback_target_ref != nullptr) {
            if (const char* ref_name = fallback_target_ref->GetDisplayFullName(); ref_name != nullptr && ref_name[0] != '\0') {
                return StripLeadingDollar(ref_name);
            }
        }
        return {};
    };

    // 1. Replace any <Alias=Name> or <Alias.ShortName=Name> tokens
    std::size_t pos = 0;
    while ((pos = text.find("<Alias", pos)) != std::string::npos) {
        const auto end_pos = text.find('>', pos);
        if (end_pos == std::string::npos) {
            break;
        }
        const auto token = text.substr(pos, end_pos - pos + 1);
        const auto eq_pos = token.find('=');
        std::string replacement;
        if (eq_pos != std::string::npos && eq_pos + 1 < token.size() - 1) {
            const auto alias_name = token.substr(eq_pos + 1, token.size() - eq_pos - 2);
            replacement = resolve_alias_actor_name(alias_name);
        }
        if (replacement.empty() && fallback_target_ref != nullptr) {
            if (const char* ref_name = fallback_target_ref->GetDisplayFullName(); ref_name != nullptr && ref_name[0] != '\0') {
                replacement = StripLeadingDollar(ref_name);
            }
        }
        if (!replacement.empty()) {
            text.replace(pos, end_pos - pos + 1, replacement);
            pos += replacement.size();
        } else {
            text.erase(pos, end_pos - pos + 1);
        }
    }

    // 2. Handle literal "Alias Follower" or "Alias <aliasName>" strings emitted by follower mods
    const auto lower_text = LowerAsciiString(text);
    const auto alias_prefix_pos = lower_text.find("alias ");
    if (alias_prefix_pos != std::string::npos) {
        std::string resolved_name;
        if (fallback_target_ref != nullptr) {
            if (const char* ref_name = fallback_target_ref->GetDisplayFullName(); ref_name != nullptr && ref_name[0] != '\0') {
                resolved_name = StripLeadingDollar(ref_name);
            }
        }
        if (resolved_name.empty()) {
            for (auto* base_alias : quest->aliases) {
                if (base_alias == nullptr) {
                    continue;
                }
                auto aliased_handle = quest->GetAliasedRef(base_alias->aliasID);
                if (auto aliased_ref = aliased_handle.get(); aliased_ref != nullptr) {
                    if (const char* ref_name = aliased_ref->GetDisplayFullName(); ref_name != nullptr && ref_name[0] != '\0') {
                        resolved_name = StripLeadingDollar(ref_name);
                        break;
                    }
                }
            }
        }
        if (!resolved_name.empty()) {
            const auto waiting_pos = lower_text.find("waiting", alias_prefix_pos);
            if (waiting_pos != std::string::npos) {
                return resolved_name + " (Waiting)";
            }
            return resolved_name;
        }
    }

    return StripLeadingDollar(text);
}

float ReadActorHealthPercent(RE::Actor* actor) noexcept
{
    if (actor == nullptr) {
        return 100.0F;
    }
    const auto* av_owner = actor->AsActorValueOwner();
    const float current = av_owner != nullptr
        ? std::max(0.0F, av_owner->GetActorValue(RE::ActorValue::kHealth))
        : 0.0F;
    const float max_val = std::max(1.0F, actor->GetActorValueMax(RE::ActorValue::kHealth));
    return std::clamp(std::round((current / max_val) * 100.0F), 0.0F, 100.0F);
}

std::uint64_t SharedSnapshotNowMs() noexcept
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool IsPlayerInSafeGameplayState(RE::PlayerCharacter* player, bool allow_interactive_menus = false) noexcept
{
    if (player == nullptr || !player->Is3DLoaded()) {
        return false;
    }
    auto* parent_cell = player->GetParentCell();
    if (parent_cell == nullptr || !parent_cell->IsAttached()) {
        return false;
    }
    if (!REL::Module::IsVR()) {
        const auto& rt = player->GetPlayerRuntimeData();
        if (rt.queuedTargetLoc.isValid ||
            rt.playerFlags.isLoading ||
            rt.playerFlags.travelUseDoor ||
            rt.playerFlags.fastTraveling) {
            return false;
        }
    }
    auto* ui = RE::UI::GetSingleton();
    if (ui == nullptr) {
        return false;
    }
    if (ui->IsMenuOpen(std::string_view("Main Menu")) ||
        ui->IsMenuOpen(std::string_view("Loading Menu")) ||
        ui->IsMenuOpen(std::string_view("Fader Menu")) ||
        ui->IsMenuOpen(std::string_view("Mist Menu")) ||
        ui->IsMenuOpen(std::string_view("LoadWaitSpinner")) ||
        ui->IsMenuOpen(std::string_view("Sleep/Wait Menu")) ||
        ui->IsMenuOpen(std::string_view("RaceSex Menu")) ||
        ui->IsMenuOpen(std::string_view("Credits Menu")) ||
        ui->IsMenuOpen(std::string_view("TitleSequence Menu"))) {
        return false;
    }
    if (!allow_interactive_menus) {
        if (ui->GameIsPaused() ||
            ui->IsModalMenuOpen() ||
            ui->IsMenuOpen(std::string_view("MapMenu")) ||
            ui->IsMenuOpen(std::string_view("TweenMenu")) ||
            ui->IsMenuOpen(std::string_view("Journal Menu")) ||
            ui->IsMenuOpen(std::string_view("StatsMenu")) ||
            ui->IsMenuOpen(std::string_view("InventoryMenu")) ||
            ui->IsMenuOpen(std::string_view("MagicMenu")) ||
            ui->IsMenuOpen(std::string_view("BarterMenu")) ||
            ui->IsMenuOpen(std::string_view("ContainerMenu")) ||
            ui->IsMenuOpen(std::string_view("FavoritesMenu")) ||
            ui->IsMenuOpen(std::string_view("Dialogue Menu")) ||
            ui->IsMenuOpen(std::string_view("Lockpicking Menu")) ||
            ui->IsMenuOpen(std::string_view("Book Menu")) ||
            ui->IsMenuOpen(std::string_view("Crafting Menu")) ||
            ui->IsMenuOpen(std::string_view("GiftMenu")) ||
            ui->IsMenuOpen(std::string_view("Training Menu")) ||
            ui->IsMenuOpen(std::string_view("LevelUp Menu")) ||
            ui->IsMenuOpen(std::string_view("MessageBoxMenu")) ||
            ui->IsMenuOpen(std::string_view("Console"))) {
            return false;
        }
    }
    return true;
}

struct WorldPersistentMapMarkerCache {
    std::uint32_t world_form_id{ 0 };
    std::vector<RE::ObjectRefHandle> marker_handles;
};
WorldPersistentMapMarkerCache g_world_persistent_markers_cache;

Json ReadCompassMarkersJson(RE::PlayerCharacter* player)
{
    Json out = Json::array();
    if (!IsPlayerInSafeGameplayState(player, false)) {
        return out;
    }

    const auto player_pos = player->GetPosition();
    auto* parent_cell = player->GetParentCell();
    if (parent_cell == nullptr || !parent_cell->IsAttached()) {
        return out;
    }
    const bool is_interior = parent_cell->IsInteriorCell();
    const std::uint32_t cell_id = parent_cell->GetFormID();
    auto* worldspace = player->GetWorldspace();
    const std::uint32_t world_id = worldspace != nullptr ? worldspace->GetFormID() : 0U;
    const auto now_ms = SharedSnapshotNowMs();

    // Only treat actual space transitions (interior <-> exterior, different worldspace, or interior cell change)
    // as a loading transition. Walking across exterior 4096x4096 grid cell borders in the same worldspace must NEVER blank the compass!
    static bool s_has_observed_space = false;
    static bool s_last_observed_interior = false;
    static std::uint32_t s_last_observed_cell_id = 0;
    static std::uint32_t s_last_observed_world_id = 0;
    static std::uint64_t s_space_transition_ms = 0;

    const bool space_changed = !s_has_observed_space ||
        (is_interior != s_last_observed_interior) ||
        (world_id != s_last_observed_world_id) ||
        (is_interior && cell_id != s_last_observed_cell_id);

    if (space_changed) {
        const bool was_initial = !s_has_observed_space;
        s_has_observed_space = true;
        s_last_observed_interior = is_interior;
        s_last_observed_cell_id = cell_id;
        s_last_observed_world_id = world_id;
        s_space_transition_ms = was_initial ? 0ULL : now_ms;
        g_compass_static_cache.valid = false;
        g_compass_static_cache.markers.clear();
        g_compass_live_cache.valid = false;
        g_compass_live_cache.timestamp_ms = 0;
        ++g_compass_live_cache.version;
        if (!was_initial) {
            return out;
        }
    } else {
        s_last_observed_cell_id = cell_id;
    }

    // Brief settle window ONLY after an actual door/worldspace load transition (never on exterior grid cell borders).
    if (s_space_transition_ms > 0 && now_ms >= s_space_transition_ms && (now_ms - s_space_transition_ms) < 550) {
        return out;
    }

    const float local_radius_meters = std::clamp(
        g_compass_marker_radius_meters.load(std::memory_order_relaxed), 50.0F, 1000.0F);
    const float local_radius_units = local_radius_meters * 70.0F;
    const float scan_cache_radius_units = local_radius_units + 1000.0F;
    const float door_scan_radius_units = std::min(scan_cache_radius_units, 12000.0F);

    // Reuse recent live marker bearings & distances across high-rate camera-only turns.
    if (g_compass_live_cache.valid &&
        g_compass_live_cache.world_form_id == world_id &&
        (!is_interior || g_compass_live_cache.cell_form_id == cell_id) &&
        std::abs(g_compass_live_cache.radius_meters - local_radius_meters) < 0.5F &&
        now_ms >= g_compass_live_cache.timestamp_ms) {
        const auto cache_age_ms = now_ms - g_compass_live_cache.timestamp_ms;
        const float cdx = player_pos.x - g_compass_live_cache.player_pos.x;
        const float cdy = player_pos.y - g_compass_live_cache.player_pos.y;
        const float cdz = player_pos.z - g_compass_live_cache.player_pos.z;
        const float moved_sq = cdx * cdx + cdy * cdy + cdz * cdz;
        if (cache_age_ms < 100 || (moved_sq < (16.0F * 16.0F) && cache_age_ms < 180)) {
            return g_compass_live_cache.markers;
        }
    }

    // 70 Skyrim units per meter. Default local radius is 50m (3500 units), configurable 50m..1000m.
    // We scan +1000 units wider for cache hysteresis, and filter local POIs/NPCs to local_radius_meters when emitting live markers.
    const float origin_dx = player_pos.x - g_compass_static_cache.scan_origin.x;
    const float origin_dy = player_pos.y - g_compass_static_cache.scan_origin.y;
    const float origin_MovedSq = origin_dx * origin_dx + origin_dy * origin_dy;

    if (!g_compass_static_cache.valid ||
        g_compass_static_cache.world_form_id != world_id ||
        (is_interior && g_compass_static_cache.cell_form_id != cell_id) ||
        std::abs(g_compass_static_cache.radius_meters - local_radius_meters) >= 0.5F ||
        (now_ms - g_compass_static_cache.timestamp_ms) >= 30000 ||
        origin_MovedSq > (650.0F * 650.0F)) {
        std::vector<CachedWorldCompassMarker> scanned;
        std::unordered_set<std::uint32_t> seen_refs;
        std::unordered_set<std::string> seen_door_destinations;

        const auto consider_map_marker_ref = [&](RE::TESObjectREFR* ref) {
            if (is_interior || worldspace == nullptr) {
                return;
            }
            if (ref == nullptr || ref->IsDisabled() || ref->IsDeleted()) {
                return;
            }
            if (ref->GetWorldspace() != worldspace) {
                return;
            }
            if (auto* ref_cell = ref->GetParentCell(); ref_cell != nullptr && ref_cell->IsInteriorCell()) {
                return;
            }
            if (!seen_refs.insert(ref->GetFormID()).second) {
                return;
            }
            auto* extra_marker = ref->extraList.GetByType<RE::ExtraMapMarker>();
            if (extra_marker == nullptr || extra_marker->mapData == nullptr) {
                return;
            }
            const auto marker_type = extra_marker->mapData->type.get();
            if (marker_type == RE::MARKER_TYPE::kPlayerSet) {
                // Handled separately as unlimited-range customMarker
                return;
            }
            const auto* raw_name = extra_marker->mapData->locationName.GetFullName();
            if (raw_name == nullptr || raw_name[0] == '\0') {
                return;
            }
            const auto clean_name = StripLeadingDollar(raw_name);
            if (clean_name.empty()) {
                return;
            }
            const auto pos = ref->GetPosition();
            const float dx = pos.x - player_pos.x;
            const float dy = pos.y - player_pos.y;
            const float dz = pos.z - player_pos.z;
            const float dist_units = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (!std::isfinite(dist_units) || dist_units > scan_cache_radius_units) {
                return;
            }
            const auto classified = ClassifyMapMarkerType(marker_type, clean_name);
            const bool discovered = IsMapMarkerDiscovered(extra_marker->mapData);
            seen_door_destinations.insert(LowerAsciiString(clean_name));
            scanned.push_back(CachedWorldCompassMarker{
                "map:" + std::to_string(ref->GetFormID()),
                clean_name,
                classified.kind,
                discovered ? std::string(classified.category_label) : (std::string("Undiscovered ") + classified.category_label),
                std::string(classified.category_label),
                pos,
                discovered,
                classified.priority,
                ref->CreateRefHandle(),
                true
            });
        };

        if (!is_interior && worldspace != nullptr) {
            std::vector<RE::NiPointer<RE::TESObjectREFR>> map_marker_refs;
            if (!REL::Module::IsVR()) {
                auto& rt = player->GetPlayerRuntimeData();
                const RE::BSSpinLockGuard marker_lock(rt.questTargetsLock);
                map_marker_refs.reserve(rt.currentMapMarkers.size());
                for (const auto& marker_handle : rt.currentMapMarkers) {
                    if (auto marker_ref = marker_handle.get(); marker_ref != nullptr) {
                        map_marker_refs.push_back(std::move(marker_ref));
                    }
                }
            }
            for (const auto& marker_ref : map_marker_refs) {
                consider_map_marker_ref(marker_ref.get());
            }

            // Also cache worldspace persistent-cell map markers once per worldspace so markers across cell boundaries never flicker or rescan the persistent cell every 9m.
            if (worldspace->persistentCell != nullptr) {
                if (g_world_persistent_markers_cache.world_form_id != world_id) {
                    g_world_persistent_markers_cache.marker_handles.clear();
                    g_world_persistent_markers_cache.world_form_id = world_id;
                    worldspace->persistentCell->ForEachReference([&](RE::TESObjectREFR* ref) {
                        if (ref != nullptr && !ref->IsDeleted()) {
                            auto* base = ref->GetBaseObject();
                            if ((base != nullptr && base->GetFormID() == 0x00000010U) ||
                                ref->extraList.HasType(RE::ExtraDataType::kMapMarker)) {
                                g_world_persistent_markers_cache.marker_handles.push_back(ref->CreateRefHandle());
                            }
                        }
                        return RE::BSContainer::ForEachResult::kContinue;
                    });
                }
                const float scan_radius_sq = scan_cache_radius_units * scan_cache_radius_units;
                for (const auto& marker_handle : g_world_persistent_markers_cache.marker_handles) {
                    if (auto marker_ref = marker_handle.get();
                        marker_ref != nullptr && !marker_ref->IsDisabled() && !marker_ref->IsDeleted()) {
                        const auto mpos = marker_ref->GetPosition();
                        const float mdx = mpos.x - player_pos.x;
                        const float mdy = mpos.y - player_pos.y;
                        const float mdz = mpos.z - player_pos.z;
                        if ((mdx * mdx + mdy * mdy + mdz * mdz) <= scan_radius_sq) {
                            consider_map_marker_ref(marker_ref.get());
                        }
                    }
                }
            }
        }

        // Collect candidate references across all loaded cells in range (using TES::ForEachReferenceInRange when outdoors
        // so doors/markers on the other side of an exterior cell seam never disappear).
        std::vector<RE::NiPointer<RE::TESObjectREFR>> cell_candidates;
        cell_candidates.reserve(96);
        auto collect_candidate = [&](RE::TESObjectREFR* ref) {
            if (ref == nullptr || ref == player || ref->IsDisabled() || ref->IsDeleted()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            auto* base = ref->GetBaseObject();
            if (base == nullptr) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            if (!base->Is(RE::FormType::Door) && base->GetFormID() != 0x00000010U) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            cell_candidates.emplace_back(ref);
            return cell_candidates.size() >= 96
                ? RE::BSContainer::ForEachResult::kStop
                : RE::BSContainer::ForEachResult::kContinue;
        };

        if (!is_interior && RE::TES::GetSingleton() != nullptr) {
            RE::TES::GetSingleton()->ForEachReferenceInRange(player, door_scan_radius_units, collect_candidate);
        } else if (parent_cell->IsAttached()) {
            parent_cell->ForEachReferenceInRange(player_pos, door_scan_radius_units, collect_candidate);
        }

        for (const auto& ref_ptr : cell_candidates) {
            auto* ref = ref_ptr.get();
            if (ref == nullptr || ref->IsDisabled() || ref->IsDeleted()) {
                continue;
            }
            if (ref->extraList.HasType(RE::ExtraDataType::kMapMarker)) {
                consider_map_marker_ref(ref);
                continue;
            }
            auto* base = ref->GetBaseObject();
            if (base == nullptr || !base->Is(RE::FormType::Door)) {
                continue;
            }
            if (!ref->extraList.HasType(RE::ExtraDataType::kTeleport)) {
                continue;
            }
            auto* extra_teleport = ref->extraList.GetByType<RE::ExtraTeleport>();
            if (extra_teleport == nullptr || extra_teleport->teleportData == nullptr) {
                continue;
            }
            auto linked_door = extra_teleport->teleportData->linkedDoor.get();
            if (linked_door == nullptr) {
                continue;
            }
            auto* dest_cell = linked_door->GetParentCell();
            const char* raw_dest = dest_cell != nullptr ? dest_cell->GetFullName() : nullptr;
            if (raw_dest == nullptr || raw_dest[0] == '\0') {
                if (auto* dest_loc = linked_door->GetCurrentLocation(); dest_loc != nullptr) {
                    raw_dest = dest_loc->GetName();
                }
            }
            if (raw_dest == nullptr || raw_dest[0] == '\0') {
                continue;
            }
            const auto clean_dest = StripLeadingDollar(raw_dest);
            if (clean_dest.empty()) {
                continue;
            }
            const auto pos = ref->GetPosition();
            const float dx = pos.x - player_pos.x;
            const float dy = pos.y - player_pos.y;
            const float dz = pos.z - player_pos.z;
            const float dist_units = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (!std::isfinite(dist_units) || dist_units > door_scan_radius_units) {
                continue;
            }
            const auto dest_key = LowerAsciiString(clean_dest);
            if (!seen_door_destinations.insert(dest_key).second) {
                continue;
            }
            auto classified = ClassifyInteriorOrPlaceName(clean_dest);
            if (dest_cell != nullptr && player->GetActorBase() != nullptr &&
                dest_cell->GetActorOwner() == player->GetActorBase()) {
                classified = { "home", "Player Home", 96 };
            }
            scanned.push_back(CachedWorldCompassMarker{
                "door:" + std::to_string(ref->GetFormID()),
                clean_dest,
                classified.kind,
                classified.category_label,
                classified.category_label,
                pos,
                true,
                classified.priority,
                {},
                false
            });
        }

        g_compass_static_cache.markers = std::move(scanned);
        g_compass_static_cache.cell_form_id = cell_id;
        g_compass_static_cache.world_form_id = world_id;
        g_compass_static_cache.radius_meters = local_radius_meters;
        g_compass_static_cache.scan_origin = player_pos;
        g_compass_static_cache.timestamp_ms = now_ms;
        g_compass_static_cache.valid = true;
    }

    struct LiveCompassItem {
        Json payload;
        int priority{};
        float distance_meters{};
    };
    std::vector<LiveCompassItem> live_items;
    live_items.reserve(32);

    struct CachedDynamicCompassEntry {
        std::string id;
        std::string name;
        std::string subtitle;
        std::string kind;
        std::string category_label;
        RE::NiPoint3 position{};
        bool hostile{ false };
        int priority{ 95 };
        bool unlimited_distance{ false };
        std::string reference_id;
        std::string base_form_id;
        int level{ 0 };
        float health_pct{ 100.0F };
        RE::ObjectRefHandle live_ref{};
    };
    static std::uint64_t s_quest_cache_ms = 0;
    static std::uint32_t s_quest_cache_cell = 0;
    static std::uint32_t s_quest_cache_world = 0;
    static std::vector<CachedDynamicCompassEntry> s_quest_entries;
    static std::unordered_set<std::uint32_t> s_quest_follower_ids;

    static std::uint64_t s_actor_cache_ms = 0;
    static std::uint32_t s_actor_cache_cell = 0;
    static std::uint32_t s_actor_cache_world = 0;
    static float s_actor_cache_radius = 50.0F;
    static std::vector<CachedDynamicCompassEntry> s_actor_entries;

    if (s_quest_cache_cell != cell_id || s_quest_cache_world != world_id || (now_ms - s_quest_cache_ms) >= 2000) {
        s_quest_entries.clear();
        s_quest_follower_ids.clear();

        const auto is_ref_in_current_space = [&](const RE::TESObjectREFR* ref) noexcept -> bool {
            if (ref == nullptr) {
                return false;
            }
            const auto* ref_cell = ref->GetParentCell();
            if (is_interior) {
                return ref_cell == parent_cell;
            }
            if (worldspace == nullptr) {
                return ref_cell == parent_cell;
            }
            return ref->GetWorldspace() == worldspace &&
                (ref_cell == nullptr || !ref_cell->IsInteriorCell());
        };

        // 0. Player-Placed Custom Map Marker ("a marker they put" — only when reachable in the current world/interior cell)
        if (!REL::Module::IsVR()) {
            auto& rt = player->GetPlayerRuntimeData();
            RE::NiPointer<RE::TESObjectREFR> custom_marker_ref = rt.playerMapMarker.get();
            if (custom_marker_ref == nullptr) {
                const RE::BSSpinLockGuard marker_lock(rt.questTargetsLock);
                for (const auto& marker_handle : rt.currentMapMarkers) {
                    if (auto m_ref = marker_handle.get(); m_ref != nullptr && !m_ref->IsDisabled() && !m_ref->IsDeleted()) {
                        if (auto* extra_m = m_ref->extraList.GetByType<RE::ExtraMapMarker>();
                            extra_m != nullptr && extra_m->mapData != nullptr &&
                            extra_m->mapData->type.get() == RE::MARKER_TYPE::kPlayerSet) {
                            custom_marker_ref = m_ref;
                            break;
                        }
                    }
                }
            }
            if (custom_marker_ref != nullptr &&
                !custom_marker_ref->IsDisabled() &&
                !custom_marker_ref->IsDeleted() &&
                custom_marker_ref->GetParentCell() != nullptr) {
                RE::TESObjectREFR* resolved_custom_ref = nullptr;
                std::string marker_subtitle = "Placed on World Map";
                if (rt.playerMarkerPath != nullptr && !rt.playerMarkerPath->teleportRefs.empty()) {
                    for (const auto& step : rt.playerMarkerPath->teleportRefs) {
                        if (step.ref != nullptr && !step.ref->IsDisabled() && !step.ref->IsDeleted() &&
                            is_ref_in_current_space(step.ref)) {
                            resolved_custom_ref = step.ref;
                            marker_subtitle = "Route to Custom Waypoint";
                            break;
                        }
                    }
                }
                if (resolved_custom_ref == nullptr && is_ref_in_current_space(custom_marker_ref.get())) {
                    resolved_custom_ref = custom_marker_ref.get();
                }
                if (resolved_custom_ref != nullptr) {
                    const RE::NiPoint3 marker_pos = resolved_custom_ref->GetPosition();
                    if (std::abs(marker_pos.x) > 1.0F || std::abs(marker_pos.y) > 1.0F) {
                        s_quest_entries.push_back(CachedDynamicCompassEntry{
                            "custom:" + std::to_string(custom_marker_ref->GetFormID()),
                            "Custom Waypoint",
                            std::move(marker_subtitle),
                            "customMarker",
                            "Custom Map Marker",
                            marker_pos,
                            false,
                            115,
                            true,
                            {},
                            {},
                            0,
                            100.0F,
                            resolved_custom_ref->CreateRefHandle()
                        });
                    }
                }
            }
        }

        // 1. Active Quest Objective Targets (all active quests checked; filtered strictly to the current cell/worldspace)
        if (auto* data_handler = RE::TESDataHandler::GetSingleton(); data_handler != nullptr) {
            std::unordered_set<std::uint32_t> seen_quest_targets;
            for (auto* quest : data_handler->GetFormArray<RE::TESQuest>()) {
                if (quest == nullptr || !quest->IsRunning() || !quest->IsActive() || quest->IsCompleted()) {
                    continue;
                }
                const auto* quest_raw_name = quest->GetName();
                const std::string quest_name = (quest_raw_name != nullptr && quest_raw_name[0] != '\0')
                    ? StripLeadingDollar(quest_raw_name)
                    : std::string("Active Quest");

                for (auto* objective : quest->objectives) {
                    if (objective == nullptr ||
                        objective->state.get() != RE::QUEST_OBJECTIVE_STATE::kDisplayed ||
                        objective->targets == nullptr ||
                        objective->numTargets == 0) {
                        continue;
                    }
                    const auto* obj_text = objective->displayText.c_str();
                    const std::string raw_label = (obj_text != nullptr && obj_text[0] != '\0')
                        ? StripLeadingDollar(obj_text)
                        : quest_name;

                    for (std::uint32_t ti = 0; ti < objective->numTargets; ++ti) {
                        auto* quest_target = objective->targets[ti];
                        if (quest_target == nullptr) {
                            continue;
                        }

                        // Resolve final target ref from quest alias lookup first; avoid GetTargetRef condition evaluation when alias already resolves.
                        RE::ObjectRefHandle final_handle = quest->GetAliasedRef(quest_target->alias);
                        auto final_ref = final_handle.get();
                        if (final_ref == nullptr) {
                            quest_target->GetTargetRef(final_handle, false, quest);
                            final_ref = final_handle.get();
                        }

                        // Resolve tracking ref inside the current space from cached teleportPath without mixing exterior/cross-world coords into interiors.
                        RE::NiPointer<RE::TESObjectREFR> target_ref;
                        for (const auto& step : quest_target->teleportPath.teleportRefs) {
                            if (step.ref != nullptr && !step.ref->IsDisabled() && !step.ref->IsDeleted() &&
                                is_ref_in_current_space(step.ref)) {
                                target_ref = RE::NiPointer<RE::TESObjectREFR>(step.ref);
                                break;
                            }
                        }
                        if (target_ref == nullptr && final_ref != nullptr &&
                            !final_ref->IsDisabled() && !final_ref->IsDeleted() &&
                            is_ref_in_current_space(final_ref.get())) {
                            target_ref = final_ref;
                        }
                        if (target_ref == nullptr) {
                            continue;
                        }

                        // Check if this quest objective is actually a waiting follower tracker (e.g. Extended Followers / EFF)
                        RE::Actor* follower_actor = nullptr;
                        if (final_ref != nullptr && final_ref->Is(RE::FormType::ActorCharacter)) {
                            follower_actor = final_ref->As<RE::Actor>();
                        } else if (target_ref->Is(RE::FormType::ActorCharacter)) {
                            follower_actor = target_ref->As<RE::Actor>();
                        }
                        const std::string resolved_label = ResolveQuestAliasText(
                            quest,
                            raw_label,
                            follower_actor != nullptr ? static_cast<RE::TESObjectREFR*>(follower_actor) : final_ref.get());
                        const auto lower_qname = LowerAsciiString(quest_name);
                        const auto lower_label = LowerAsciiString(resolved_label);
                        const bool is_follower_waiting_quest =
                            (follower_actor != nullptr) &&
                            (follower_actor->IsPlayerTeammate() ||
                             lower_qname.find("follower") != std::string::npos ||
                             lower_label.find("waiting") != std::string::npos ||
                             LowerAsciiString(raw_label).find("follower") != std::string::npos);

                        if (is_follower_waiting_quest && follower_actor != nullptr) {
                            if (!s_quest_follower_ids.insert(follower_actor->GetFormID()).second) {
                                continue;
                            }
                            const char* raw_fname = follower_actor->GetDisplayFullName();
                            const std::string follower_name = (raw_fname != nullptr && raw_fname[0] != '\0')
                                ? StripLeadingDollar(raw_fname)
                                : resolved_label;
                            auto* actor_base = follower_actor->GetActorBase();
                            s_quest_entries.push_back(CachedDynamicCompassEntry{
                                "follower:" + std::to_string(follower_actor->GetFormID()),
                                follower_name + " (Waiting)",
                                "Follower Waiting",
                                "follower",
                                "Follower Waiting",
                                target_ref->GetPosition(),
                                false,
                                105,
                                true,
                                std::to_string(follower_actor->GetFormID()),
                                actor_base != nullptr ? std::to_string(actor_base->GetFormID()) : std::string{},
                                static_cast<int>(follower_actor->GetLevel()),
                                ReadActorHealthPercent(follower_actor),
                                target_ref->CreateRefHandle()
                            });
                            continue;
                        }

                        if (!seen_quest_targets.insert(target_ref->GetFormID()).second) {
                            continue;
                        }

                        const RE::NiPoint3 target_pos = target_ref->GetPosition();

                        const bool is_load_door = target_ref->GetBaseObject() != nullptr &&
                            target_ref->GetBaseObject()->Is(RE::FormType::Door) &&
                            target_ref->extraList.HasType(RE::ExtraDataType::kTeleport);
                        std::string door_dest;
                        if (is_load_door) {
                            if (auto* extra_tp = target_ref->extraList.GetByType<RE::ExtraTeleport>();
                                extra_tp != nullptr && extra_tp->teleportData != nullptr) {
                                if (auto linked = extra_tp->teleportData->linkedDoor.get(); linked != nullptr) {
                                    auto* dest_cell = linked->GetParentCell();
                                    const char* raw_d = dest_cell != nullptr ? dest_cell->GetFullName() : nullptr;
                                    if (raw_d == nullptr || raw_d[0] == '\0') {
                                        if (auto* dest_loc = linked->GetCurrentLocation(); dest_loc != nullptr) {
                                            raw_d = dest_loc->GetName();
                                        }
                                    }
                                    if (raw_d != nullptr && raw_d[0] != '\0') {
                                        door_dest = StripLeadingDollar(raw_d);
                                    }
                                }
                            }
                        }
                        std::string resolved_subtitle = resolved_label;
                        if (!door_dest.empty()) {
                            resolved_subtitle = (resolved_label != quest_name && !resolved_label.empty())
                                ? (door_dest + " · " + resolved_label)
                                : door_dest;
                        }
                        s_quest_entries.push_back(CachedDynamicCompassEntry{
                            "quest:" + std::to_string(quest->GetFormID()) + ":" + std::to_string(target_ref->GetFormID()),
                            quest_name,
                            std::move(resolved_subtitle),
                            is_load_door ? "questDoor" : "quest",
                            is_load_door ? "Quest Passage" : "Active Quest",
                            target_pos,
                            false,
                            110,
                            true,
                            {},
                            {},
                            0,
                            100.0F,
                            target_ref->CreateRefHandle()
                        });
                    }
                }
            }
        }

        s_quest_cache_cell = cell_id;
        s_quest_cache_world = world_id;
        s_quest_cache_ms = now_ms;
    }

    if (s_actor_cache_cell != cell_id || s_actor_cache_world != world_id ||
        std::abs(s_actor_cache_radius - local_radius_meters) >= 0.5F ||
        (now_ms - s_actor_cache_ms) >= 250) {
        s_actor_entries.clear();
        std::unordered_set<std::uint32_t> seen_follower_actors = s_quest_follower_ids;
        std::unordered_set<std::uint32_t> seen_actor_ids;
        std::vector<std::pair<float, CachedDynamicCompassEntry>> candidate_enemies;

        // 2. Hostile Actors in configured radius + Waiting Player Followers (high + middle-high process actors)
        if (auto* process_lists = RE::ProcessLists::GetSingleton(); process_lists != nullptr) {
            const auto consider_actor = [&](RE::Actor* actor) {
                if (actor == nullptr || actor == player ||
                    actor->IsDisabled() || actor->IsDeleted() || actor->IsDead()) {
                    return;
                }
                const auto actor_form_id = actor->GetFormID();
                if (!seen_actor_ids.insert(actor_form_id).second) {
                    return;
                }
                const auto* actor_cell = actor->GetParentCell();
                const bool in_current_space = is_interior
                    ? (actor_cell == parent_cell)
                    : (worldspace != nullptr
                        ? (actor->GetWorldspace() == worldspace &&
                           (actor_cell == nullptr || !actor_cell->IsInteriorCell()))
                        : (actor_cell == parent_cell));
                if (!in_current_space) {
                    return;
                }

                // Check for waiting player teammate/follower so their real name always appears when waiting
                if (actor->IsPlayerTeammate() && !actor->IsHostileToActor(player)) {
                    float waiting_val = 0.0F;
                    if (auto* av_owner = actor->AsActorValueOwner(); av_owner != nullptr) {
                        waiting_val = av_owner->GetActorValue(RE::ActorValue::kWaitingForPlayer);
                    }
                    if (waiting_val > 0.0F && seen_follower_actors.insert(actor_form_id).second) {
                        const auto* raw_fname = actor->GetDisplayFullName();
                        const std::string follower_name = (raw_fname != nullptr && raw_fname[0] != '\0')
                            ? StripLeadingDollar(raw_fname)
                            : std::string("Follower");
                        auto* actor_base = actor->GetActorBase();
                        s_actor_entries.push_back(CachedDynamicCompassEntry{
                            "follower:" + std::to_string(actor_form_id),
                            follower_name + " (Waiting)",
                            "Follower Waiting",
                            "follower",
                            "Follower Waiting",
                            actor->GetPosition(),
                            false,
                            105,
                            true,
                            std::to_string(actor_form_id),
                            actor_base != nullptr ? std::to_string(actor_base->GetFormID()) : std::string{},
                            static_cast<int>(actor->GetLevel()),
                            ReadActorHealthPercent(actor),
                            actor->CreateRefHandle()
                        });
                    }
                    return;
                }

                const auto pos = actor->GetPosition();
                const float dx = pos.x - player_pos.x;
                const float dy = pos.y - player_pos.y;
                const float dz = pos.z - player_pos.z;
                const float dist_units = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (!std::isfinite(dist_units) || dist_units > scan_cache_radius_units) {
                    return;
                }
                if (!IsGenuineHostileThreat(actor, player, dist_units)) {
                    return;
                }
                const auto* raw_actor_name = actor->GetDisplayFullName();
                const std::string actor_name = (raw_actor_name != nullptr && raw_actor_name[0] != '\0')
                    ? StripLeadingDollar(raw_actor_name)
                    : std::string("Hostile Enemy");
                auto* actor_base = actor->GetActorBase();
                candidate_enemies.emplace_back(
                    dist_units,
                    CachedDynamicCompassEntry{
                        "enemy:" + std::to_string(actor_form_id),
                        actor_name,
                        "Lv. " + std::to_string(std::max<int>(1, actor->GetLevel())) + " Hostile",
                        "enemy",
                        "Hostile Target",
                        pos,
                        true,
                        98,
                        false,
                        std::to_string(actor_form_id),
                        actor_base != nullptr ? std::to_string(actor_base->GetFormID()) : std::string{},
                        static_cast<int>(actor->GetLevel()),
                        ReadActorHealthPercent(actor),
                        actor->CreateRefHandle()
                    });
            };

            process_lists->ForEachHighActor([&](RE::Actor* actor) {
                consider_actor(actor);
                return RE::BSContainer::ForEachResult::kContinue;
            });
            for (auto handle : process_lists->middleHighActorHandles) {
                auto actor_ptr = handle.get();
                if (auto* actor = actor_ptr.get(); actor != nullptr) {
                    consider_actor(actor);
                }
            }

            std::sort(
                candidate_enemies.begin(),
                candidate_enemies.end(),
                [](const auto& left, const auto& right) { return left.first < right.first; });
            constexpr std::size_t kMaxEnemyCompassMarkers = 12;
            for (std::size_t idx = 0; idx < candidate_enemies.size() && idx < kMaxEnemyCompassMarkers; ++idx) {
                s_actor_entries.push_back(std::move(candidate_enemies[idx].second));
            }
        }

        s_actor_cache_cell = cell_id;
        s_actor_cache_world = world_id;
        s_actor_cache_radius = local_radius_meters;
        s_actor_cache_ms = now_ms;
    }

    const auto append_dynamic_entries = [&](const std::vector<CachedDynamicCompassEntry>& entries) {
        for (const auto& dyn : entries) {
            RE::NiPoint3 live_pos = dyn.position;
            if (auto live_ref = dyn.live_ref.get(); live_ref != nullptr && !live_ref->IsDisabled() && !live_ref->IsDeleted()) {
                live_pos = live_ref->GetPosition();
            }
            const float dx = live_pos.x - player_pos.x;
            const float dy = live_pos.y - player_pos.y;
            const float dz = live_pos.z - player_pos.z;
            const float dist_units = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (!std::isfinite(dist_units)) {
                continue;
            }
            const float dist_m = std::max(0.5F, std::round((dist_units / 70.0F) * 10.0F) * 0.1F);
            // Enforce configured local radius for non-unlimited markers (such as hostile enemies)
            if (!dyn.unlimited_distance && (dist_units > local_radius_units || dist_m > local_radius_meters)) {
                continue;
            }
            Json item_json{
                { "id", dyn.id },
                { "name", dyn.name },
                { "kind", dyn.kind },
                { "categoryLabel", dyn.category_label },
                { "angle", ComputeWorldBearingDeg(player_pos, live_pos) },
                { "distance", dist_m },
                { "discovered", true },
                { "hostile", dyn.hostile },
                { "elevation", ElevationRelationLabel(dz) }
            };
            if (!dyn.subtitle.empty()) {
                item_json["subtitle"] = dyn.subtitle;
            }
            if (!dyn.reference_id.empty()) {
                item_json["referenceId"] = dyn.reference_id;
            }
            if (!dyn.base_form_id.empty()) {
                item_json["baseFormId"] = dyn.base_form_id;
            }
            if (dyn.level > 0) {
                item_json["level"] = dyn.level;
            }
            if (dyn.hostile || dyn.kind == "follower") {
                item_json["healthPct"] = dyn.health_pct;
            }
            live_items.push_back(LiveCompassItem{
                std::move(item_json),
                dyn.priority,
                dist_m
            });
        }
    };
    append_dynamic_entries(s_quest_entries);
    append_dynamic_entries(s_actor_entries);

    // 3. Cached Nearby Map Markers & City Building Doors within configured radius
    for (auto& cached : g_compass_static_cache.markers) {
        if (cached.is_map_marker) {
            if (auto ref = cached.marker_ref.get(); ref != nullptr && !ref->IsDisabled() && !ref->IsDeleted()) {
                if (auto* extra_marker = ref->extraList.GetByType<RE::ExtraMapMarker>();
                    extra_marker != nullptr && extra_marker->mapData != nullptr) {
                    const bool live_discovered = cached.discovered || IsMapMarkerDiscovered(extra_marker->mapData);
                    if (live_discovered != cached.discovered) {
                        cached.discovered = live_discovered;
                        cached.category_label = live_discovered
                            ? cached.base_category_label
                            : (std::string("Undiscovered ") + cached.base_category_label);
                    }
                }
            }
        }
        const float dx = cached.position.x - player_pos.x;
        const float dy = cached.position.y - player_pos.y;
        const float dz = cached.position.z - player_pos.z;
        const float dist_units = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!std::isfinite(dist_units) || dist_units > local_radius_units) {
            continue;
        }
        const float dist_m = std::max(0.5F, std::round((dist_units / 70.0F) * 10.0F) * 0.1F);
        if (dist_m > local_radius_meters) {
            continue;
        }
        int effective_priority = cached.priority;
        if (dist_m <= 28.0F &&
            (cached.kind == "home" || cached.kind == "inn" || cached.kind == "blacksmith" ||
             cached.kind == "vendor" || cached.kind == "castle")) {
            effective_priority += 25;
        }
        live_items.push_back(LiveCompassItem{
            Json{
                { "id", cached.id },
                { "name", cached.name },
                { "kind", cached.kind },
                { "categoryLabel", cached.category_label },
                { "angle", ComputeWorldBearingDeg(player_pos, cached.position) },
                { "distance", dist_m },
                { "discovered", cached.discovered },
                { "hostile", false },
                { "elevation", ElevationRelationLabel(dz) }
            },
            effective_priority,
            dist_m
        });
    }

    std::sort(live_items.begin(), live_items.end(), [](const LiveCompassItem& left, const LiveCompassItem& right) {
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        return left.distance_meters < right.distance_meters;
    });

    constexpr std::size_t kMaxCompassMarkers = 24;
    for (std::size_t i = 0; i < live_items.size() && i < kMaxCompassMarkers; ++i) {
        out.push_back(std::move(live_items[i].payload));
    }
    g_compass_live_cache.markers = out;
    g_compass_live_cache.serialized = g_compass_live_cache.markers.dump();
    g_compass_live_cache.player_pos = player_pos;
    g_compass_live_cache.cell_form_id = cell_id;
    g_compass_live_cache.world_form_id = world_id;
    g_compass_live_cache.radius_meters = local_radius_meters;
    g_compass_live_cache.timestamp_ms = now_ms;
    g_compass_live_cache.valid = true;
    ++g_compass_live_cache.version;
    return out;
}

struct SharedSnapshotCacheEntry {
    bool valid{ false };
    std::uint64_t timestamp_ms{ 0 };
    std::uint64_t sequence{ 0 };
    Json snapshot;
    std::string serialized;
    std::unordered_map<std::string, std::shared_ptr<const Json>> topic_slices;
    std::unordered_map<std::string, std::shared_ptr<const std::string>> topic_serialized;
    Json vitals_core_slice;
    std::string vitals_core_serialized;
    std::string vitals_compass_serialized;
    std::string vitals_heading_serialized;
};

constexpr std::uint64_t kSharedSnapshotCacheTtlMs = 450;
constexpr std::uint64_t kSharedHudSnapshotCacheTtlMs = 120;
constexpr std::uint64_t kHeavySnapshotSlicesCacheTtlMs = 30000;
constexpr std::array<std::string_view, 6> kCanonicalGameTopics{
    "vitals", "equipment", "inventory", "effects", "quests", "followers"
};

SharedSnapshotCacheEntry g_shared_full_cache;
SharedSnapshotCacheEntry g_shared_hud_cache;
std::uint64_t g_shared_snapshot_sequence{ 0 };

struct HeavySnapshotSlicesCache {
    bool valid{ false };
    bool has_heavy_hud_slices{ false };
    bool has_full_inventory{ false };
    std::uint64_t timestamp_ms{ 0 };
    Json equipment;
    std::shared_ptr<const Json> equipment_slice;
    std::shared_ptr<const std::string> equipment_serialized;
    std::vector<Json> inventory_items;
    std::size_t inventory_type_count{ 0 };
    std::int64_t total_item_count{ 0 };
    std::shared_ptr<const Json> inventory_slice;
    std::shared_ptr<const std::string> inventory_serialized;
    std::shared_ptr<const Json> compact_inventory_slice;
    std::shared_ptr<const std::string> compact_inventory_serialized;
    Json quests;
    std::size_t quest_count{ 0 };
    std::shared_ptr<const Json> quests_slice;
    std::shared_ptr<const std::string> quests_serialized;
    Json followers;
    std::size_t follower_count{ 0 };
    std::shared_ptr<const Json> followers_slice;
    std::shared_ptr<const std::string> followers_serialized;
    std::uint64_t effects_timestamp_ms{ 0 };
    Json active_effects{ Json::array() };
    std::size_t active_effect_count{ 0 };
    std::shared_ptr<const Json> effects_slice;
    std::shared_ptr<const std::string> effects_serialized;
    std::int32_t gold{ 0 };
    float inventory_weight{ 0.0F };
};
HeavySnapshotSlicesCache g_heavy_snapshot_slices;

Json BuildPlayerSnapshotJson(bool compact, bool include_heavy_hud = true)
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!IsPlayerInSafeGameplayState(player, !compact)) {
        return Json{ { "available", false } };
    }

    const auto* actor_values = player->AsActorValueOwner();
    if (actor_values == nullptr) {
        return Json{ { "available", false } };
    }
    const auto value = [actor_values](RE::ActorValue actor_value) {
        const auto raw = actor_values->GetActorValue(actor_value);
        return std::isfinite(raw) ? std::max(raw, 0.0F) : 0.0F;
    };
    const auto maximum = [player](RE::ActorValue actor_value) {
        const auto raw = player->GetActorValueMax(actor_value);
        return std::isfinite(raw) ? std::max(raw, 0.0F) : 0.0F;
    };

    if (!g_heavy_snapshot_slices.compact_inventory_serialized ||
        g_heavy_snapshot_slices.compact_inventory_serialized->empty()) {
        auto compact_inv = std::make_shared<Json>(Json{
            { "inventory", Json::array() },
            { "inventoryTypeCount", 0 },
            { "inventoryItemCount", 0 },
            { "inventoryTruncated", false }
        });
        g_heavy_snapshot_slices.compact_inventory_serialized =
            std::make_shared<std::string>(compact_inv->dump());
        g_heavy_snapshot_slices.compact_inventory_slice = std::move(compact_inv);
    }

    const auto now_ms = SharedSnapshotNowMs();
    const bool expired = !g_heavy_snapshot_slices.valid ||
        !g_heavy_snapshot_slices.has_heavy_hud_slices ||
        (now_ms - g_heavy_snapshot_slices.timestamp_ms) >= kHeavySnapshotSlicesCacheTtlMs;
    const bool need_heavy_refresh = (!compact || include_heavy_hud) &&
        (expired || (!compact && !g_heavy_snapshot_slices.has_full_inventory));
    if (need_heavy_refresh) {
        Json equipment = {
            { "head", nullptr },
            { "body", nullptr },
            { "hands", nullptr },
            { "feet", nullptr },
            { "neck", nullptr },
            { "ring", nullptr },
            { "cape", nullptr },
            { "lantern", nullptr },
            { "ammo", PlayerItemJson(player->GetCurrentAmmo(), 1, true) },
            { "mainHand", PlayerItemJson(player->GetEquippedObject(false), 1, true, player->GetEquippedEntryData(false)) },
            { "offHand", PlayerItemJson(player->GetEquippedObject(true), 1, true, player->GetEquippedEntryData(true)) }
        };

        struct LightInvCandidate {
            RE::TESBoundObject* form{};
            std::int32_t count{};
            const RE::InventoryEntryData* entry_data{};
            std::string name;
            bool worn{};
            bool equipped{};
        };
        std::vector<LightInvCandidate> candidates;
        std::int64_t total_item_count{};
        std::int32_t cached_gold{};
        auto inventory = player->GetInventory();
        if (!compact) {
            candidates.reserve(inventory.size());
        }
        const auto* left_hand_obj = player->GetEquippedObject(true);
        const auto* right_hand_obj = player->GetEquippedObject(false);
        const auto* current_ammo_obj = player->GetCurrentAmmo();

        for (const auto& [form, inventory_entry] : inventory) {
            if (form == nullptr || inventory_entry.first <= 0) {
                continue;
            }
            if (form->GetFormID() == 0x0000000FU || form->IsGold()) {
                cached_gold += std::max(inventory_entry.first, 0);
            }
            auto* entry_data = inventory_entry.second.get();
            if (!IsPlayableNamedInventoryItem(form, entry_data)) {
                continue;
            }
            const bool worn = entry_data != nullptr && entry_data->IsWorn();
            const bool equipped = worn || left_hand_obj == form || right_hand_obj == form || current_ammo_obj == form;
            if (worn) {
                if (auto* armor = form->As<RE::TESObjectARMO>(); armor != nullptr) {
                    const auto worn_item = PlayerItemJson(form, inventory_entry.first, true, entry_data);
                    if (worn_item.is_object()) {
                        for (const auto& slot : ItemEquipSlots(armor)) {
                            if (slot != "auto") equipment[slot.get<std::string>()] = worn_item;
                        }
                    }
                }
            }
            if (compact) {
                continue;
            }
            const auto* raw_name = entry_data != nullptr ? entry_data->GetDisplayName() : form->GetName();
            std::string clean_name = (raw_name != nullptr && raw_name[0] != '\0')
                ? StripLeadingDollar(raw_name)
                : StripLeadingDollar(form->GetName());
            if (clean_name.empty()) {
                continue;
            }
            total_item_count += inventory_entry.first;

            candidates.push_back(LightInvCandidate{
                form,
                inventory_entry.first,
                entry_data,
                std::move(clean_name),
                worn,
                equipped
            });
        }

        if (!compact) {
            std::sort(candidates.begin(), candidates.end(), [](const LightInvCandidate& a, const LightInvCandidate& b) {
                if (a.name != b.name) return a.name < b.name;
                return a.form->GetFormID() < b.form->GetFormID();
            });

            std::vector<Json> inventory_items;
            inventory_items.reserve(std::min<std::size_t>(candidates.size(), kMaxPlayerSnapshotItems));
            for (std::size_t i = 0; i < candidates.size() && inventory_items.size() < kMaxPlayerSnapshotItems; ++i) {
                const auto& cand = candidates[i];
                AppendPlayerInventoryItems(cand.form, cand.count, cand.equipped, cand.entry_data, [&](Json&& item_json) {
                    if (inventory_items.size() < kMaxPlayerSnapshotItems) {
                        inventory_items.push_back(std::move(item_json));
                    }
                });
            }
            g_heavy_snapshot_slices.inventory_type_count = inventory_items.size();
            g_heavy_snapshot_slices.inventory_items = std::move(inventory_items);
            g_heavy_snapshot_slices.total_item_count = total_item_count;
            g_heavy_snapshot_slices.has_full_inventory = true;
            auto inv_slice = std::make_shared<Json>(Json{
                { "inventory", g_heavy_snapshot_slices.inventory_items },
                { "inventoryTypeCount", g_heavy_snapshot_slices.inventory_type_count },
                { "inventoryItemCount", g_heavy_snapshot_slices.total_item_count },
                { "inventoryTruncated", g_heavy_snapshot_slices.inventory_type_count > kMaxPlayerSnapshotItems }
            });
            g_heavy_snapshot_slices.inventory_serialized =
                std::make_shared<std::string>(inv_slice->dump());
            g_heavy_snapshot_slices.inventory_slice = std::move(inv_slice);
        } else if (expired) {
            g_heavy_snapshot_slices.has_full_inventory = false;
            g_heavy_snapshot_slices.inventory_items.clear();
            g_heavy_snapshot_slices.inventory_type_count = 0;
            g_heavy_snapshot_slices.total_item_count = 0;
            g_heavy_snapshot_slices.inventory_slice.reset();
            g_heavy_snapshot_slices.inventory_serialized.reset();
        }

        auto [quests, quest_count] = ReadActiveQuestsJson();
        auto [followers, follower_count] = ReadFollowersJson(player);

        g_heavy_snapshot_slices.equipment = std::move(equipment);
        auto eq_slice = std::make_shared<Json>(Json{
            { "equipment", g_heavy_snapshot_slices.equipment }
        });
        g_heavy_snapshot_slices.equipment_serialized =
            std::make_shared<std::string>(eq_slice->dump());
        g_heavy_snapshot_slices.equipment_slice = std::move(eq_slice);

        g_heavy_snapshot_slices.quests = std::move(quests);
        g_heavy_snapshot_slices.quest_count = quest_count;
        auto q_slice = std::make_shared<Json>(Json{
            { "quests", g_heavy_snapshot_slices.quests },
            { "questCount", quest_count }
        });
        g_heavy_snapshot_slices.quests_serialized =
            std::make_shared<std::string>(q_slice->dump());
        g_heavy_snapshot_slices.quests_slice = std::move(q_slice);

        g_heavy_snapshot_slices.followers = std::move(followers);
        g_heavy_snapshot_slices.follower_count = follower_count;
        auto f_slice = std::make_shared<Json>(Json{
            { "followers", g_heavy_snapshot_slices.followers },
            { "followerCount", follower_count }
        });
        g_heavy_snapshot_slices.followers_serialized =
            std::make_shared<std::string>(f_slice->dump());
        g_heavy_snapshot_slices.followers_slice = std::move(f_slice);

        g_heavy_snapshot_slices.gold = std::max(cached_gold, 0);
        g_heavy_snapshot_slices.timestamp_ms = now_ms;
        g_heavy_snapshot_slices.has_heavy_hud_slices = true;
        g_heavy_snapshot_slices.valid = true;
    } else if (compact && !include_heavy_hud) {
        if (!g_heavy_snapshot_slices.valid) {
            g_heavy_snapshot_slices.equipment = Json{
                { "head", nullptr }, { "body", nullptr }, { "hands", nullptr }, { "feet", nullptr },
                { "neck", nullptr }, { "ring", nullptr }, { "cape", nullptr }, { "lantern", nullptr },
                { "ammo", nullptr }, { "mainHand", nullptr }, { "offHand", nullptr }
            };
            auto eq_slice = std::make_shared<Json>(Json{ { "equipment", g_heavy_snapshot_slices.equipment } });
            g_heavy_snapshot_slices.equipment_serialized = std::make_shared<std::string>(eq_slice->dump());
            g_heavy_snapshot_slices.equipment_slice = std::move(eq_slice);

            g_heavy_snapshot_slices.quests = Json::array();
            g_heavy_snapshot_slices.quest_count = 0;
            auto q_slice = std::make_shared<Json>(Json{ { "quests", Json::array() }, { "questCount", 0 } });
            g_heavy_snapshot_slices.quests_serialized = std::make_shared<std::string>(q_slice->dump());
            g_heavy_snapshot_slices.quests_slice = std::move(q_slice);

            g_heavy_snapshot_slices.followers = Json::array();
            g_heavy_snapshot_slices.follower_count = 0;
            auto f_slice = std::make_shared<Json>(Json{ { "followers", Json::array() }, { "followerCount", 0 } });
            g_heavy_snapshot_slices.followers_serialized = std::make_shared<std::string>(f_slice->dump());
            g_heavy_snapshot_slices.followers_slice = std::move(f_slice);

            g_heavy_snapshot_slices.valid = true;
        }
        g_heavy_snapshot_slices.gold = std::max<std::int32_t>(player->GetGoldAmount(), 0);
        if (g_heavy_snapshot_slices.timestamp_ms == 0) {
            g_heavy_snapshot_slices.timestamp_ms = now_ms;
        }
    }

    Json equipment = g_heavy_snapshot_slices.equipment;
    std::vector<Json> inventory_items = compact ? std::vector<Json>{} : g_heavy_snapshot_slices.inventory_items;
    const auto inventory_type_count = compact ? std::size_t{ 0 } : g_heavy_snapshot_slices.inventory_type_count;
    const auto total_item_count = compact ? std::int64_t{ 0 } : g_heavy_snapshot_slices.total_item_count;

    const auto* raw_player_name = player->GetName();
    const std::string player_name = raw_player_name != nullptr && raw_player_name[0] != '\0' ?
        raw_player_name : "Dragonborn";
    const auto* location = player->GetCurrentLocation();
    const auto* location_name = location != nullptr ? location->GetName() : nullptr;
    Json resistances = Json::array();
    struct NamedActorValue {
        const char* id;
        const char* label;
        RE::ActorValue actor_value;
        const char* group;
    };
    constexpr std::array resistance_values{
        NamedActorValue{"fire", "Fire", RE::ActorValue::kResistFire, ""},
        NamedActorValue{"frost", "Frost", RE::ActorValue::kResistFrost, ""},
        NamedActorValue{"shock", "Shock", RE::ActorValue::kResistShock, ""},
        NamedActorValue{"magic", "Magic", RE::ActorValue::kResistMagic, ""},
        NamedActorValue{"poison", "Poison", RE::ActorValue::kPoisonResist, ""},
        NamedActorValue{"disease", "Disease", RE::ActorValue::kResistDisease, ""}
    };
    if (!compact) for (const auto& entry : resistance_values) {
        const auto resistance = actor_values->GetActorValue(entry.actor_value);
        resistances.push_back({ {"id", entry.id}, {"label", entry.label},
            {"value", std::isfinite(resistance) ? resistance : 0.0F} });
    }
    Json skills = Json::array();
    constexpr std::array skill_values{
        NamedActorValue{"oneHanded", "One-handed", RE::ActorValue::kOneHanded, "Combat"},
        NamedActorValue{"twoHanded", "Two-handed", RE::ActorValue::kTwoHanded, "Combat"},
        NamedActorValue{"archery", "Archery", RE::ActorValue::kArchery, "Combat"},
        NamedActorValue{"block", "Block", RE::ActorValue::kBlock, "Combat"},
        NamedActorValue{"smithing", "Smithing", RE::ActorValue::kSmithing, "Combat"},
        NamedActorValue{"heavyArmor", "Heavy armor", RE::ActorValue::kHeavyArmor, "Combat"},
        NamedActorValue{"alteration", "Alteration", RE::ActorValue::kAlteration, "Magic"},
        NamedActorValue{"conjuration", "Conjuration", RE::ActorValue::kConjuration, "Magic"},
        NamedActorValue{"destruction", "Destruction", RE::ActorValue::kDestruction, "Magic"},
        NamedActorValue{"illusion", "Illusion", RE::ActorValue::kIllusion, "Magic"},
        NamedActorValue{"restoration", "Restoration", RE::ActorValue::kRestoration, "Magic"},
        NamedActorValue{"enchanting", "Enchanting", RE::ActorValue::kEnchanting, "Magic"},
        NamedActorValue{"lightArmor", "Light armor", RE::ActorValue::kLightArmor, "Stealth"},
        NamedActorValue{"sneak", "Sneak", RE::ActorValue::kSneak, "Stealth"},
        NamedActorValue{"lockpicking", "Lockpicking", RE::ActorValue::kLockpicking, "Stealth"},
        NamedActorValue{"pickpocket", "Pickpocket", RE::ActorValue::kPickpocket, "Stealth"},
        NamedActorValue{"alchemy", "Alchemy", RE::ActorValue::kAlchemy, "Stealth"},
        NamedActorValue{"speech", "Speech", RE::ActorValue::kSpeech, "Stealth"}
    };
    if (!compact) for (const auto& entry : skill_values) {
        skills.push_back({ {"id", entry.id}, {"label", entry.label},
            {"group", entry.group}, {"value", value(entry.actor_value)} });
    }
    const std::uint64_t effects_ttl_ms = compact ? 15000 : 10000;
    if (!g_heavy_snapshot_slices.valid || g_heavy_snapshot_slices.effects_timestamp_ms == 0 ||
        (now_ms - g_heavy_snapshot_slices.effects_timestamp_ms) >= effects_ttl_ms) {
        Json active_effects = Json::array();
        std::size_t active_effect_count{};
        RE::MagicTarget::EffectVisitor collect_effects([&](RE::ActiveEffect* active_effect) {
            if (active_effect == nullptr || active_effect->flags.any(RE::ActiveEffect::Flag::kInactive) ||
                active_effect->flags.any(RE::ActiveEffect::Flag::kDispelled)) return RE::BSContainer::ForEachResult::kContinue;
            const auto* base_effect = active_effect->GetBaseObject();
            if (base_effect == nullptr || base_effect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) return RE::BSContainer::ForEachResult::kContinue;
            const auto* effect_name = base_effect->GetName();
            if (effect_name == nullptr || effect_name[0] == '\0') return RE::BSContainer::ForEachResult::kContinue;
            const auto effect_id = std::to_string(base_effect->GetFormID()) + ":" + std::to_string(active_effect->usUniqueID);
            if (g_deleted_active_effect_ids.contains(effect_id) ||
                g_deleted_active_effect_ids.contains(std::to_string(base_effect->GetFormID()))) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            ++active_effect_count;
            if (active_effects.size() >= 48) return RE::BSContainer::ForEachResult::kContinue;
            const auto duration = std::isfinite(active_effect->duration) ? std::max(0.0F, active_effect->duration) : 0.0F;
            const auto elapsed = std::isfinite(active_effect->elapsedSeconds) ? std::max(0.0F, active_effect->elapsedSeconds) : 0.0F;
            const auto magnitude = std::isfinite(active_effect->magnitude) ? (std::round(active_effect->magnitude * 10.0F) / 10.0F) : 0.0F;
            const auto raw_remaining = std::max(0.0F, duration - elapsed);
            const auto remaining_val = (compact || raw_remaining > 30.0F)
                ? (std::round(raw_remaining / 15.0F) * 15.0F)
                : std::round(raw_remaining);
            active_effects.push_back({
                {"id", effect_id},
                {"name", effect_name},
                {"magnitude", magnitude},
                {"duration", std::round(duration)},
                {"remaining", remaining_val},
                {"hostile", base_effect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHostile)}
            });
            return RE::BSContainer::ForEachResult::kContinue;
        });
        // Actor's base-class offsets vary by runtime. An implicit conversion to
        // MagicTarget uses the compiler's layout instead of the running game's.
        if (auto* magic_target = player->AsMagicTarget(); magic_target != nullptr) {
            magic_target->VisitEffects(collect_effects);
        }
        g_heavy_snapshot_slices.active_effects = std::move(active_effects);
        g_heavy_snapshot_slices.active_effect_count = active_effect_count;
        auto fx_slice = std::make_shared<Json>(Json{
            { "activeEffects", g_heavy_snapshot_slices.active_effects },
            { "activeEffectCount", active_effect_count }
        });
        g_heavy_snapshot_slices.effects_serialized =
            std::make_shared<std::string>(fx_slice->dump());
        g_heavy_snapshot_slices.effects_slice = std::move(fx_slice);
        const float raw_inv_weight = actor_values->GetActorValue(RE::ActorValue::kInventoryWeight);
        g_heavy_snapshot_slices.inventory_weight = (std::isfinite(raw_inv_weight) && raw_inv_weight >= 0.0F)
            ? raw_inv_weight
            : player->GetWeightInContainer();
        g_heavy_snapshot_slices.effects_timestamp_ms = now_ms;
    }
    Json active_effects = g_heavy_snapshot_slices.active_effects;
    const auto active_effect_count = g_heavy_snapshot_slices.active_effect_count;
    const float raw_inv_weight = actor_values->GetActorValue(RE::ActorValue::kInventoryWeight);
    const float inventory_weight = (std::isfinite(raw_inv_weight) && raw_inv_weight >= 0.0F)
        ? raw_inv_weight
        : g_heavy_snapshot_slices.inventory_weight;
    const auto heading_deg = ReadCompassHeadingDeg(player);
    Json calendar_data = nullptr;
    if (const auto* calendar = RE::Calendar::GetSingleton()) {
        const auto hour = calendar->GetHour();
        const auto day = calendar->GetDay();
        calendar_data = {{"hour",std::isfinite(hour) ? hour : 0.0F},{"minute",calendar->GetMinutes()},
            {"day",std::isfinite(day) ? day : 1.0F},{"month",calendar->GetMonthName()},{"year",calendar->GetYear()}};
    }
    Json quests = g_heavy_snapshot_slices.quests;
    const auto quest_count = g_heavy_snapshot_slices.quest_count;
    Json followers = g_heavy_snapshot_slices.followers;
    const auto follower_count = g_heavy_snapshot_slices.follower_count;
    Json compass_markers = compact ? ReadCompassMarkersJson(player) : Json::array();
    return Json{
        { "available", true },
        { "snapshotScope", compact ? "hud" : "full" },
        { "playerName", player_name },
        { "characterKey", skyultra::GetPlayerCharacterKey() },
        { "portraitRevision", skyultra::GetPlayerPortraitRevision() },
        { "level", player->GetLevel() },
        { "heading", heading_deg },
        { "compassMarkers", std::move(compass_markers) },
        { "calendar", std::move(calendar_data) },
        { "health", { { "current", value(RE::ActorValue::kHealth) }, { "max", maximum(RE::ActorValue::kHealth) } } },
        { "stamina", { { "current", value(RE::ActorValue::kStamina) }, { "max", maximum(RE::ActorValue::kStamina) } } },
        { "magicka", { { "current", value(RE::ActorValue::kMagicka) }, { "max", maximum(RE::ActorValue::kMagicka) } } },
        { "armorRating", value(RE::ActorValue::kDamageResist) },
        { "gold", g_heavy_snapshot_slices.gold },
        { "location", location_name != nullptr && location_name[0] != '\0' ? location_name : "Skyrim" },
        { "carryWeight", { {"current", std::isfinite(inventory_weight) ? std::max(inventory_weight, 0.0F) : 0.0F}, {"max", value(RE::ActorValue::kCarryWeight)} } },
        { "resistances", std::move(resistances) },
        { "skills", std::move(skills) },
        { "activeEffects", std::move(active_effects) },
        { "activeEffectCount", active_effect_count },
        { "equipment", std::move(equipment) },
        { "inventory", std::move(inventory_items) },
        { "inventoryTypeCount", inventory_type_count },
        { "inventoryItemCount", total_item_count },
        { "inventoryTruncated", inventory_type_count > kMaxPlayerSnapshotItems },
        { "quests", std::move(quests) },
        { "questCount", quest_count },
        { "followers", std::move(followers) },
        { "followerCount", follower_count }
    };
}

void PopulateSnapshotTopicSlices(SharedSnapshotCacheEntry& entry, bool compact)
{
    entry.topic_slices.clear();
    entry.topic_serialized.clear();
    entry.vitals_core_slice = Json{};
    entry.vitals_core_serialized.clear();
    entry.vitals_compass_serialized.clear();
    entry.vitals_heading_serialized.clear();
    if (!entry.snapshot.value("available", false)) {
        return;
    }

    const auto& s = entry.snapshot;
    entry.vitals_core_slice = Json{
        { "playerName", s.value("playerName", std::string("Dragonborn")) },
        { "characterKey", s.value("characterKey", std::string("player")) },
        { "portraitRevision", s.value("portraitRevision", std::uint64_t{1}) },
        { "level", s.value("level", 1) },
        { "calendar", s.contains("calendar") ? s["calendar"] : Json(nullptr) },
        { "health", s["health"] },
        { "stamina", s["stamina"] },
        { "magicka", s["magicka"] },
        { "armorRating", s.value("armorRating", 0.0F) },
        { "gold", s.value("gold", 0) },
        { "location", s.value("location", std::string("Skyrim")) },
        { "carryWeight", s["carryWeight"] },
        { "resistances", s["resistances"] },
        { "skills", s["skills"] }
    };
    entry.vitals_core_serialized = entry.vitals_core_slice.dump();
    entry.vitals_heading_serialized = s["heading"].dump();
    entry.vitals_compass_serialized = (compact && g_compass_live_cache.valid)
        ? g_compass_live_cache.serialized
        : (s.contains("compassMarkers") ? s["compassMarkers"].dump() : std::string("[]"));

    auto vitals_slice = entry.vitals_core_slice;
    vitals_slice["heading"] = s.value("heading", 0.0F);
    vitals_slice["compassMarkers"] = s.contains("compassMarkers") ? s["compassMarkers"] : Json::array();
    entry.topic_serialized["vitals"] = std::make_shared<std::string>(
        entry.vitals_core_serialized + "|" + entry.vitals_heading_serialized + "|" + entry.vitals_compass_serialized);
    entry.topic_slices["vitals"] = std::make_shared<Json>(std::move(vitals_slice));

    entry.topic_slices["equipment"] = g_heavy_snapshot_slices.equipment_slice;
    entry.topic_serialized["equipment"] = g_heavy_snapshot_slices.equipment_serialized;
    entry.topic_slices["inventory"] = compact
        ? g_heavy_snapshot_slices.compact_inventory_slice
        : g_heavy_snapshot_slices.inventory_slice;
    entry.topic_serialized["inventory"] = compact
        ? g_heavy_snapshot_slices.compact_inventory_serialized
        : g_heavy_snapshot_slices.inventory_serialized;
    entry.topic_slices["effects"] = g_heavy_snapshot_slices.effects_slice;
    entry.topic_serialized["effects"] = g_heavy_snapshot_slices.effects_serialized;
    entry.topic_slices["quests"] = g_heavy_snapshot_slices.quests_slice;
    entry.topic_serialized["quests"] = g_heavy_snapshot_slices.quests_serialized;
    entry.topic_slices["followers"] = g_heavy_snapshot_slices.followers_slice;
    entry.topic_serialized["followers"] = g_heavy_snapshot_slices.followers_serialized;
}

SharedSnapshotCacheEntry& GetOrCollectSharedSnapshotMutable(bool compact, bool include_heavy_hud = true)
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    auto& target = compact ? g_shared_hud_cache : g_shared_full_cache;
    const auto now_ms = SharedSnapshotNowMs();
    const auto ttl_ms = compact ? kSharedHudSnapshotCacheTtlMs : kSharedSnapshotCacheTtlMs;
    if (target.valid &&
        (!compact || !include_heavy_hud || g_heavy_snapshot_slices.has_heavy_hud_slices) &&
        (now_ms - target.timestamp_ms) < ttl_ms) {
        return target;
    }

    target.snapshot = BuildPlayerSnapshotJson(compact, include_heavy_hud);
    target.timestamp_ms = now_ms;
    target.sequence = ++g_shared_snapshot_sequence;
    target.snapshot["sequence"] = target.sequence;
    target.serialized.clear();
    PopulateSnapshotTopicSlices(target, compact);
    target.valid = true;
    return target;
}

const SharedSnapshotCacheEntry& GetOrCollectSharedSnapshot(bool compact, bool include_heavy_hud = true)
{
    return GetOrCollectSharedSnapshotMutable(compact, include_heavy_hud);
}

RE::BSFixedString ReadPlayerSnapshotJSON(bool compact = false)
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    auto& cached = GetOrCollectSharedSnapshotMutable(compact, true);
    if (cached.serialized.empty()) {
        cached.serialized = cached.snapshot.dump();
    }
    return RE::BSFixedString(cached.serialized.c_str());
}

RE::BSFixedString PapyrusGetPlayerSnapshotJSON(RE::StaticFunctionTag*)
{
    return ReadPlayerSnapshotJSON();
}

struct PapyrusMod {
    std::string id;
    std::string folder_name;
    SkyUltraModHandle native_handle{};
};

struct PapyrusView {
    SkyUltraViewHandle native_handle{};
    std::int32_t mod_token{};
    std::unordered_set<std::string> request_methods;
};

struct PapyrusRequest {
    SkyUltraViewHandle native_view{};
    std::string native_request_id;
    std::string method;
    std::string params_json;
};

std::mutex g_papyrus_mutex;
std::int32_t g_next_mod_token{ 1 };
std::int32_t g_next_view_token{ 1 };
std::int32_t g_next_request_token{ 1 };
std::unordered_map<std::int32_t, PapyrusMod> g_mods;
std::unordered_map<std::string, std::int32_t> g_mod_tokens_by_id;
std::unordered_map<std::int32_t, PapyrusView> g_views;
std::unordered_map<SkyUltraViewHandle, std::int32_t> g_view_tokens_by_native_handle;
std::unordered_map<std::int32_t, PapyrusRequest> g_requests;
std::unordered_map<std::string, std::int32_t> g_request_tokens_by_native_id;

const SkyUltraNativeAPI_V4* NativeApi() noexcept
{
    const auto* api = SkyUltra_GetAPI_V4();
    return api != nullptr && api->abi_version >= 4 &&
            api->struct_size >= sizeof(SkyUltraNativeAPI_V4) ?
        api : nullptr;
}

template <class Map>
std::int32_t AllocateToken(std::int32_t& next_token, const Map& entries)
{
    const auto first = next_token;
    do {
        const auto candidate = next_token;
        next_token = next_token >= kMaxPapyrusToken ? 1 : next_token + 1;
        if (!entries.contains(candidate)) {
            return candidate;
        }
    } while (next_token != first);
    return 0;
}

std::string NativeRequestKey(SkyUltraViewHandle view, std::string_view request_id)
{
    auto key = std::to_string(view);
    key.push_back(':');
    key.append(request_id);
    return key;
}

bool PapyrusIsReady(RE::StaticFunctionTag*)
{
    return skyultra::PapyrusApiReady();
}

bool SendModEvent(std::string_view event_name, std::string_view string_arg, std::int32_t token)
{
    auto* source = SKSE::GetModCallbackEventSource();
    if (source == nullptr) {
        return false;
    }

    const std::string owned_event_name(event_name);
    const std::string owned_string_arg(string_arg);
    SKSE::ModCallbackEvent event;
    event.eventName = owned_event_name.c_str();
    event.strArg = owned_string_arg.c_str();
    event.numArg = static_cast<float>(token);
    event.sender = nullptr;
    source->SendEvent(&event);
    return true;
}

std::int32_t PapyrusRegisterMod(RE::StaticFunctionTag*, RE::BSFixedString mod_id)
{
    if (mod_id.empty()) {
        SKSE::log::error("SkyUltra Papyrus RegisterMod failed: the mod ID was empty.");
        return 0;
    }
    const auto* api = NativeApi();
    if (api == nullptr) {
        SKSE::log::error("SkyUltra Papyrus RegisterMod('{}') failed: native API V4 is unavailable.",
            mod_id.c_str());
        return 0;
    }

    std::string id(mod_id.data(), mod_id.size());
    for (auto& value : id) {
        if (value >= 'A' && value <= 'Z') {
            value = static_cast<char>(value - 'A' + 'a');
        }
    }
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        if (const auto existing = g_mod_tokens_by_id.find(id);
            existing != g_mod_tokens_by_id.end()) {
            return existing->second;
        }
    }

    SkyUltraModHandle native_mod{};
    const auto result = api->register_mod(
        id.data(), static_cast<std::uint32_t>(id.size()), &native_mod);
    if (result != SKYULTRA_OK) {
        SKSE::log::error("SkyUltra Papyrus RegisterMod('{}') failed with native result {}.",
            id, static_cast<std::int32_t>(result));
        return 0;
    }

    const std::scoped_lock lock(g_papyrus_mutex);
    if (const auto existing = g_mod_tokens_by_id.find(id);
        existing != g_mod_tokens_by_id.end()) {
        return existing->second;
    }
    const auto token = AllocateToken(g_next_mod_token, g_mods);
    if (token == 0) {
        SKSE::log::error("SkyUltra Papyrus RegisterMod('{}') failed: no Papyrus mod handles remain.", id);
        return 0;
    }
    g_mod_tokens_by_id.emplace(id, token);
    g_mods.emplace(token, PapyrusMod{ id, id, native_mod });
    return token;
}

std::int32_t PapyrusRegisterModForQuest(RE::StaticFunctionTag*, RE::TESForm* owner_form)
{
    if (owner_form == nullptr) {
        SKSE::log::error("SkyUltra Papyrus RegisterModForQuest failed: quest form was null.");
        return 0;
    }
    const auto* owner_file = owner_form->GetFile();
    if (owner_file == nullptr) {
        SKSE::log::error("SkyUltra Papyrus RegisterModForQuest failed: quest plugin was unavailable.");
        return 0;
    }

    const std::string_view plugin_name = owner_file->GetFilename();
    std::string error;
    auto manifest = skyultra::FindModManifestForPapyrusPlugin(
        skyultra::GetInstalledWebModsDirectory(), plugin_name, error);
    if (!manifest) {
        SKSE::log::error("SkyUltra Papyrus could not find a mod for quest plugin '{}': {}",
            plugin_name, error);
        return 0;
    }

    const auto mod_token = PapyrusRegisterMod(
        nullptr, RE::BSFixedString(manifest->id.c_str()));
    if (mod_token == 0) {
        return 0;
    }
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        if (const auto found = g_mods.find(mod_token); found != g_mods.end()) {
            found->second.folder_name = manifest->folder_name;
        }
    }
    SKSE::log::info("SkyUltra associated quest plugin '{}' with mod folder '{}'.",
        plugin_name, manifest->folder_name);
    return mod_token;
}

RE::BSFixedString PapyrusGetModFolderName(RE::StaticFunctionTag*, std::int32_t mod_token)
{
    const std::scoped_lock lock(g_papyrus_mutex);
    const auto found = g_mods.find(mod_token);
    return found != g_mods.end() ?
        RE::BSFixedString(found->second.folder_name.c_str()) : RE::BSFixedString{};
}

std::int32_t PapyrusCreateView(
    RE::StaticFunctionTag*,
    std::int32_t mod_token,
    RE::BSFixedString view_name,
    std::int32_t pixel_width,
    std::int32_t pixel_height,
    float x,
    float y,
    float width,
    float height,
    bool visible)
{
    if (view_name.empty() || pixel_width <= 0 || pixel_height <= 0 ||
        static_cast<std::uint32_t>(pixel_width) > kMaxPixelDimension ||
        static_cast<std::uint32_t>(pixel_height) > kMaxPixelDimension ||
        !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
        !std::isfinite(height) || width <= 0.0F || height <= 0.0F) {
        return 0;
    }

    PapyrusMod mod;
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        const auto found = g_mods.find(mod_token);
        if (found == g_mods.end()) {
            return 0;
        }
        mod = found->second;
    }

    const auto* api = NativeApi();
    if (api == nullptr) {
        return 0;
    }
    std::int32_t view_token{};
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        view_token = AllocateToken(g_next_view_token, g_views);
    }
    if (view_token == 0) {
        return 0;
    }

    std::string id = mod.id + ".Papyrus." + std::string(view_name.data(), view_name.size());
    if (id.size() > 128) {
        return 0;
    }
    SkyUltraViewConfigV1 config{};
    config.struct_size = static_cast<std::uint32_t>(sizeof(config));
    config.id_utf8 = id.data();
    config.id_length = static_cast<std::uint32_t>(id.size());
    config.initial_url_utf8 = "";
    config.initial_url_length = 0;
    config.pixel_width = static_cast<std::uint32_t>(pixel_width);
    config.pixel_height = static_cast<std::uint32_t>(pixel_height);
    config.bounds = { x, y, width, height };
    config.clip = { x, y, width, height };
    config.opacity = 1.0F;
    config.scale = 1.0F;
    config.z_index = 0;
    config.max_fps = SKYULTRA_FPS_UNCAPPED;
    // Papyrus pages are opaque by default. The C ABI remains available to
    // native plugins that explicitly need transparent HUD-style surfaces.
    config.flags = (visible ? SKYULTRA_VIEW_VISIBLE : 0U) |
        SKYULTRA_VIEW_ENABLE_JAVASCRIPT | SKYULTRA_VIEW_ENABLE_GAMEPAD;
    config.layer = SKYULTRA_LAYER_PANEL;
    config.anchor = SKYULTRA_ANCHOR_TOP_LEFT;
    config.browser_engine = SKYULTRA_BROWSER_ENGINE_DEFAULT;

    SkyUltraViewHandle native_view{};
    if (api->create_mod_view(mod.native_handle, &config, &native_view) != SKYULTRA_OK) {
        return 0;
    }
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        g_views.emplace(view_token, PapyrusView{ native_view, mod_token, {} });
        g_view_tokens_by_native_handle.emplace(native_view, view_token);
    }
    return view_token;
}

std::int32_t PapyrusCreateFullScreenModView(
    RE::StaticFunctionTag*,
    std::int32_t mod_token,
    RE::BSFixedString view_name,
    bool visible)
{
    const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
    if (screen.width == 0 || screen.height == 0) {
        SKSE::log::error("SkyUltra could not create a full-screen Papyrus view: game viewport is unavailable.");
        return 0;
    }
    return PapyrusCreateView(nullptr, mod_token, std::move(view_name),
        static_cast<std::int32_t>(screen.width), static_cast<std::int32_t>(screen.height),
        0.0F, 0.0F, static_cast<float>(screen.width), static_cast<float>(screen.height), visible);
}

std::int32_t PapyrusGetViewState(RE::StaticFunctionTag*, std::int32_t view_token)
{
    const auto* api = NativeApi();
    if (api == nullptr) {
        return -1;
    }
    SkyUltraViewHandle native_view{};
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        const auto found = g_views.find(view_token);
        if (found == g_views.end()) {
            return -1;
        }
        native_view = found->second.native_handle;
    }
    SkyUltraViewStateV1 state{};
    return api->get_view_state(native_view, &state) == SKYULTRA_OK ?
        static_cast<std::int32_t>(state) : -1;
}

bool PapyrusSetActorPreview(RE::StaticFunctionTag*,std::int32_t token,RE::BSFixedString json)
{
    SkyUltraViewHandle handle{};{const std::scoped_lock lock(g_papyrus_mutex);const auto view=g_views.find(token);if(view==g_views.end())return false;handle=view->second.native_handle;}
    return skyultra::QueueActorRendererRequest(handle,"game.setActorPreview",json.c_str());
}
bool PapyrusCloseActorPreview(RE::StaticFunctionTag*,std::int32_t token)
{
    SkyUltraViewHandle handle{};{const std::scoped_lock lock(g_papyrus_mutex);const auto view=g_views.find(token);if(view==g_views.end())return false;handle=view->second.native_handle;}
    return skyultra::QueueActorRendererRequest(handle,"game.closeActorPreview","{}");
}
RE::BSFixedString PapyrusActorPreviewStatus(RE::StaticFunctionTag*,std::int32_t token)
{
    SkyUltraViewHandle handle{};{const std::scoped_lock lock(g_papyrus_mutex);const auto view=g_views.find(token);if(view==g_views.end())return "{}";handle=view->second.native_handle;}
    return RE::BSFixedString(skyultra::ReadActorRendererStatus(handle).c_str());
}

bool PapyrusSetViewVisible(RE::StaticFunctionTag*, std::int32_t view_token, bool visible)
{
    const auto* api = NativeApi();
    if (api == nullptr) {
        return false;
    }
    SkyUltraViewHandle native_view{};
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        const auto found = g_views.find(view_token);
        if (found == g_views.end()) {
            return false;
        }
        native_view = found->second.native_handle;
    }
    return api->set_visible(native_view,
               static_cast<std::uint8_t>(visible ? 1 : 0)) == SKYULTRA_OK;
}

bool PapyrusSetViewBounds(
    RE::StaticFunctionTag*, std::int32_t view_token, float x, float y, float width, float height)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
        !std::isfinite(height) || width <= 0.0F || height <= 0.0F) {
        return false;
    }
    const auto* api = NativeApi();
    if (api == nullptr) {
        return false;
    }
    SkyUltraViewHandle native_view{};
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        const auto found = g_views.find(view_token);
        if (found == g_views.end()) {
            return false;
        }
        native_view = found->second.native_handle;
    }
    return api->set_bounds(native_view, { x, y, width, height }) == SKYULTRA_OK;
}

bool PapyrusSendEvent(
    RE::StaticFunctionTag*, std::int32_t view_token,
    RE::BSFixedString event_name, RE::BSFixedString json)
{
    if (event_name.empty() || json.empty()) {
        return false;
    }
    const auto* api = NativeApi();
    if (api == nullptr) {
        return false;
    }
    SkyUltraViewHandle native_view{};
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        const auto found = g_views.find(view_token);
        if (found == g_views.end()) {
            return false;
        }
        native_view = found->second.native_handle;
    }
    return api->send_event_json(native_view,
               event_name.data(), event_name.size(), json.data(), json.size()) == SKYULTRA_OK;
}

bool ReplaceRequestMethod(std::int32_t view_token, std::string_view method, bool add)
{
    if (method.empty() || method.size() > 128 || method.find('\0') != std::string_view::npos) {
        return false;
    }
    const auto* api = NativeApi();
    if (api == nullptr) {
        return false;
    }

    SkyUltraViewHandle native_view{};
    std::unordered_set<std::string> methods;
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        const auto found = g_views.find(view_token);
        if (found == g_views.end()) {
            return false;
        }
        native_view = found->second.native_handle;
        methods = found->second.request_methods;
    }
    const std::string owned_method(method);
    if (add) {
        if (methods.size() >= 128 && !methods.contains(owned_method)) {
            return false;
        }
        methods.emplace(owned_method);
    } else if (methods.erase(owned_method) == 0) {
        return false;
    }

    std::vector<SkyUltraPageRequestMethodV2> native_methods;
    native_methods.reserve(methods.size());
    for (const auto& name : methods) {
        native_methods.push_back({ name.data(), static_cast<std::uint32_t>(name.size()) });
    }
    if (api->set_page_request_allowlist(native_view, native_methods.data(),
            static_cast<std::uint32_t>(native_methods.size())) != SKYULTRA_OK) {
        return false;
    }

    const std::scoped_lock lock(g_papyrus_mutex);
    if (const auto found = g_views.find(view_token); found != g_views.end() &&
        found->second.native_handle == native_view) {
        found->second.request_methods = std::move(methods);
        return true;
    }
    return false;
}

bool PapyrusAddPageRequestMethod(
    RE::StaticFunctionTag*, std::int32_t view_token, RE::BSFixedString method)
{
    return !method.empty() && ReplaceRequestMethod(
        view_token, std::string_view(method.data(), method.size()), true);
}

bool PapyrusRemovePageRequestMethod(
    RE::StaticFunctionTag*, std::int32_t view_token, RE::BSFixedString method)
{
    return !method.empty() && ReplaceRequestMethod(
        view_token, std::string_view(method.data(), method.size()), false);
}

bool PapyrusReloadView(RE::StaticFunctionTag*, std::int32_t view_token)
{
    const auto* api = NativeApi();
    if (api == nullptr) {
        return false;
    }
    SkyUltraViewHandle native_view{};
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        const auto found = g_views.find(view_token);
        if (found == g_views.end()) {
            return false;
        }
        native_view = found->second.native_handle;
    }
    return api->reload_view(native_view) == SKYULTRA_OK;
}

bool PapyrusCloseView(RE::StaticFunctionTag*, std::int32_t view_token)
{
    const auto* api = NativeApi();
    if (api == nullptr) {
        return false;
    }
    SkyUltraViewHandle native_view{};
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        const auto found = g_views.find(view_token);
        if (found == g_views.end()) {
            return false;
        }
        native_view = found->second.native_handle;
    }
    if (api->destroy_view(native_view) != SKYULTRA_OK) {
        return false;
    }
    skyultra::ForgetPapyrusView(native_view);
    return true;
}

RE::BSFixedString PapyrusGetPageRequestPayload(RE::StaticFunctionTag*, std::int32_t request_token)
{
    const std::scoped_lock lock(g_papyrus_mutex);
    const auto found = g_requests.find(request_token);
    return found != g_requests.end() ?
        RE::BSFixedString(found->second.params_json.c_str()) : RE::BSFixedString{};
}

bool CompletePapyrusPageRequest(
    std::int32_t request_token, bool success, std::string_view result_json, std::string_view error)
{
    const auto* api = NativeApi();
    if (api == nullptr) {
        return false;
    }
    PapyrusRequest request;
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        const auto found = g_requests.find(request_token);
        if (found == g_requests.end()) {
            return false;
        }
        request = found->second;
    }
    if ((success && (result_json.empty() || result_json.size() > 65536)) ||
        (!success && (error.empty() || error.size() > 4096))) {
        return false;
    }
    const auto result = api->complete_page_request(request.native_view,
        request.native_request_id.data(), static_cast<std::uint32_t>(request.native_request_id.size()),
        static_cast<std::uint8_t>(success ? 1 : 0),
        success ? result_json.data() : nullptr,
        success ? static_cast<std::uint32_t>(result_json.size()) : 0,
        success ? nullptr : error.data(),
        success ? 0 : static_cast<std::uint32_t>(error.size()));
    if (result != SKYULTRA_OK) {
        return false;
    }

    const std::scoped_lock lock(g_papyrus_mutex);
    const auto key = NativeRequestKey(request.native_view, request.native_request_id);
    g_request_tokens_by_native_id.erase(key);
    g_requests.erase(request_token);
    return true;
}

bool PapyrusCompletePageRequest(
    RE::StaticFunctionTag*, std::int32_t request_token, RE::BSFixedString result_json)
{
    return CompletePapyrusPageRequest(request_token, true,
        std::string_view(result_json.data(), result_json.size()), {});
}

bool PapyrusRejectPageRequest(
    RE::StaticFunctionTag*, std::int32_t request_token, RE::BSFixedString error)
{
    return !error.empty() && CompletePapyrusPageRequest(request_token, false, {},
        std::string_view(error.data(), error.size()));
}

}  // namespace

namespace skyultra {

void CaptureActiveBarterVendor() noexcept
{
    (void)ResolveActiveBarterVendor();
}

void CaptureActiveContainerRef() noexcept
{
    g_active_container_looted_notified = false;
    (void)ResolveActiveContainerRef();
}

void NotifyCompassLocationDiscovered(const RE::MapMarkerData* map_marker_data) noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    if (map_marker_data != nullptr) {
        g_discovered_marker_data.insert(map_marker_data);
    }
    g_compass_static_cache.valid = false;
    g_compass_live_cache.valid = false;
    g_compass_live_cache.timestamp_ms = 0;
    ++g_compass_live_cache.version;
    g_shared_hud_cache.valid = false;
    g_shared_full_cache.valid = false;
}

void ResetCompassDiscoveryState() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    g_discovered_marker_data.clear();
    g_compass_static_cache.valid = false;
    g_compass_live_cache.valid = false;
}

bool CaptureActiveBlacksmithStation() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    g_active_blacksmith_station = {};
    auto* ui = RE::UI::GetSingleton();
    auto* player = RE::PlayerCharacter::GetSingleton();
    RE::TESFurniture* furniture = nullptr;
    RE::ObjectRefHandle furniture_ref{};
    if (ui != nullptr) {
        if (auto crafting_menu = ui->GetMenu<RE::CraftingMenu>(); crafting_menu != nullptr) {
            if (auto* sub = crafting_menu->GetCraftingSubMenu(); sub != nullptr) {
                furniture = sub->furniture;
                if (auto* smithing_sub = skyrim_cast<RE::CraftingSubMenus::SmithingMenu*>(sub);
                    smithing_sub != nullptr && smithing_sub->furnitureRef != nullptr) {
                    furniture_ref = smithing_sub->furnitureRef->GetHandle();
                }
            }
        }
    }
    if (!furniture_ref && player != nullptr) {
        furniture_ref = player->GetOccupiedFurniture();
    }
    if (furniture == nullptr) {
        if (auto ref = furniture_ref.get(); ref != nullptr && ref->GetBaseObject() != nullptr) {
            furniture = ref->GetBaseObject()->As<RE::TESFurniture>();
        }
    }
    if (furniture == nullptr) {
        return false;
    }

    std::string kind;
    std::string mode;
    bool skyforge = false;
    bool grants_xp = false;
    if (!ClassifyBlacksmithFurniture(furniture, kind, mode, skyforge, grants_xp)) {
        return false;
    }

    g_active_blacksmith_station.active = true;
    g_active_blacksmith_station.furniture_form_id = furniture->GetFormID();
    g_active_blacksmith_station.furniture_ref = furniture_ref;
    g_active_blacksmith_station.station_kind = std::move(kind);
    g_active_blacksmith_station.station_mode = std::move(mode);
    g_active_blacksmith_station.is_skyforge = skyforge;
    g_active_blacksmith_station.grants_skill_xp = grants_xp;

    const char* raw_station = furniture->GetFullName();
    if (raw_station == nullptr || raw_station[0] == '\0') raw_station = furniture->GetName();
    if (raw_station != nullptr && raw_station[0] != '\0') {
        g_active_blacksmith_station.station_name = StripLeadingDollar(raw_station);
    } else if (g_active_blacksmith_station.station_kind == "tanning") {
        g_active_blacksmith_station.station_name = "Tanning Rack";
    } else if (g_active_blacksmith_station.station_kind == "smelter") {
        g_active_blacksmith_station.station_name = "Smelter";
    } else if (g_active_blacksmith_station.station_kind == "grindstone") {
        g_active_blacksmith_station.station_name = "Grindstone";
    } else if (g_active_blacksmith_station.station_kind == "workbench") {
        g_active_blacksmith_station.station_name = "Armor Workbench";
    } else {
        g_active_blacksmith_station.station_name = skyforge ? "Skyforge" : "Blacksmith Forge";
    }

    for (auto* kw : furniture->GetKeywords()) {
        if (kw != nullptr) {
            g_active_blacksmith_station.bench_keyword_ids.push_back(kw->GetFormID());
        }
    }
    if (g_active_blacksmith_station.station_kind == "grindstone") {
        g_active_blacksmith_station.bench_keyword_ids.push_back(0x00088108U);
    } else if (g_active_blacksmith_station.station_kind == "workbench") {
        g_active_blacksmith_station.bench_keyword_ids.push_back(0x000ADB78U);
    }
    return true;
}

void ClearActiveBlacksmithStation() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    g_active_blacksmith_station = {};
}

template <typename Classifier>
bool CaptureActiveCraftingStationUnlocked(
    bool& out_active,
    RE::FormID& out_form_id,
    RE::ObjectRefHandle& out_ref,
    std::string& out_name,
    const char* default_name,
    Classifier&& classify) noexcept
{
    auto* ui = RE::UI::GetSingleton();
    auto* player = RE::PlayerCharacter::GetSingleton();
    RE::TESFurniture* furniture = nullptr;
    RE::ObjectRefHandle furniture_ref{};
    if (ui != nullptr) {
        if (auto crafting_menu = ui->GetMenu<RE::CraftingMenu>(); crafting_menu != nullptr) {
            if (auto* sub = crafting_menu->GetCraftingSubMenu(); sub != nullptr) {
                furniture = sub->furniture;
            }
        }
    }
    if (player != nullptr) {
        furniture_ref = player->GetOccupiedFurniture();
    }
    if (furniture == nullptr) {
        if (auto ref = furniture_ref.get(); ref != nullptr && ref->GetBaseObject() != nullptr) {
            furniture = ref->GetBaseObject()->As<RE::TESFurniture>();
        }
    }
    if (furniture == nullptr || !classify(furniture)) {
        return false;
    }
    out_active = true;
    out_form_id = furniture->GetFormID();
    out_ref = furniture_ref;
    const char* raw_station = furniture->GetFullName();
    if (raw_station == nullptr || raw_station[0] == '\0') {
        raw_station = furniture->GetName();
    }
    out_name = (raw_station != nullptr && raw_station[0] != '\0')
        ? StripLeadingDollar(raw_station)
        : std::string(default_name);
    return true;
}

bool CaptureActiveEnchantingStation() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    g_active_enchanting_station = {};
    return CaptureActiveCraftingStationUnlocked(
        g_active_enchanting_station.active,
        g_active_enchanting_station.furniture_form_id,
        g_active_enchanting_station.furniture_ref,
        g_active_enchanting_station.station_name,
        "Arcane Enchanter",
        ClassifyEnchantingFurniture);
}

void ClearActiveEnchantingStation() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    g_active_enchanting_station = {};
}

bool CaptureActiveAlchemyStation() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    g_active_alchemy_station = {};
    return CaptureActiveCraftingStationUnlocked(
        g_active_alchemy_station.active,
        g_active_alchemy_station.furniture_form_id,
        g_active_alchemy_station.furniture_ref,
        g_active_alchemy_station.station_name,
        "Alchemy Lab",
        ClassifyAlchemyFurniture);
}

void ClearActiveAlchemyStation() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    g_active_alchemy_station = {};
}

void ReleaseVirtualCraftingProxyNow() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    ClearVirtualCraftingProxyUnlocked();
}

void SetSuperEnchantingEnabled(bool enabled) noexcept
{
    g_super_enchanting_enabled.store(enabled, std::memory_order_relaxed);
    InvalidateSharedGameDataCache();
}

bool IsSuperEnchantingEnabled() noexcept
{
    return g_super_enchanting_enabled.load(std::memory_order_relaxed);
}

void ApplySavedEnchantmentCustomNames(const std::unordered_map<std::uint32_t, std::string>& names)
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    for (const auto& [old_id, _] : g_enchantment_custom_names) {
        if (!names.contains(old_id)) {
            if (auto* old_ench = RE::TESForm::LookupByID<RE::EnchantmentItem>(old_id); old_ench != nullptr) {
                if (const auto oit = g_enchantment_original_names.find(old_id);
                    oit != g_enchantment_original_names.end() && !oit->second.empty()) {
                    if (auto* fn = old_ench->As<RE::TESFullName>(); fn != nullptr) {
                        fn->SetFullName(oit->second.c_str());
                    }
                }
            }
        }
    }
    g_enchantment_custom_names.clear();
    for (const auto& [id, custom_name] : names) {
        if (id == 0 || custom_name.empty()) {
            continue;
        }
        if (auto* ench = RE::TESForm::LookupByID<RE::EnchantmentItem>(id); ench != nullptr) {
            ApplyCustomNameToEnchantmentUnlocked(ench, custom_name);
        } else {
            g_enchantment_custom_names[id] = custom_name;
        }
    }
}

std::unordered_map<std::uint32_t, std::string> GetSavedEnchantmentCustomNames()
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    return g_enchantment_custom_names;
}

float ReadPlayerEnchantingSkillLevel() noexcept
{
    return ReadPlayerEnchantingBaseSkillUnlocked(RE::PlayerCharacter::GetSingleton());
}

bool CaptureActiveBookMenu()
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    return CaptureActiveBookMenuUnlocked();
}

void ClearActiveBookMenu() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    g_active_book_menu = {};
}

bool HasActiveBookMenu() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    return g_active_book_menu.active;
}

bool OpenCapturedBookNativeMenu() noexcept
{
    RE::NiPointer<RE::TESObjectREFR> ref;
    RE::TESObjectBOOK* book = nullptr;
    {
        const std::scoped_lock lock(g_shared_game_data_mutex);
        if (!g_active_book_menu.active) {
            return false;
        }
        ref = g_active_book_menu.target_ref.get();
        if (g_active_book_menu.book_form_id != 0) {
            book = RE::TESForm::LookupByID<RE::TESObjectBOOK>(g_active_book_menu.book_form_id);
        }
        const bool taken = g_active_book_menu.taken;
        g_active_book_menu = {};
        if (taken) {
            ref.reset();
        }
    }
    if (ref != nullptr && !ref->IsDeleted() && !ref->IsDisabled()) {
        RE::BookMenu::OpenMenuFromReference(ref.get());
        return true;
    }
    if (book != nullptr) {
        RE::BookMenu::OpenMenuFromBaseForm(book);
        return true;
    }
    return false;
}

bool IsActiveContainerMannequin() noexcept
{
    return IsMannequinReference(ResolveActiveContainerRef());
}

bool IsActiveContainerCorpse() noexcept
{
    auto* ref = ResolveActiveContainerRef();
    if (ref == nullptr || IsMannequinReference(ref)) {
        return false;
    }
    auto* actor = ref->As<RE::Actor>();
    return actor != nullptr && actor->IsDead();
}

bool IsActiveContainerPickpocket() noexcept
{
    auto* ref = ResolveActiveContainerRef();
    if (ref == nullptr || IsMannequinReference(ref)) {
        return false;
    }
    auto* actor = ref->As<RE::Actor>();
    if (actor == nullptr || actor->IsDead()) {
        return false;
    }
    const auto mode = RE::ContainerMenu::GetContainerMode();
    if (mode == RE::ContainerMenu::ContainerMode::kPickpocket ||
        mode == RE::ContainerMenu::ContainerMode::kSteal) {
        return true;
    }
    if (auto* player = RE::PlayerCharacter::GetSingleton(); player != nullptr && player->IsSneaking()) {
        return true;
    }
    return !actor->IsPlayerTeammate();
}

void InvalidateSharedGameDataCache() noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    g_shared_full_cache.valid = false;
    g_shared_hud_cache.valid = false;
    g_compass_live_cache.valid = false;
    g_compass_live_cache.timestamp_ms = 0;
    ++g_compass_live_cache.version;
    g_heavy_snapshot_slices.valid = false;
    g_heavy_snapshot_slices.has_full_inventory = false;
    g_heavy_snapshot_slices.effects_timestamp_ms = 0;
    g_inventory_page_cache.valid = false;
    g_nearby_npcs_cache.valid = false;
}

void InvalidateSharedLightGameDataCache(bool invalidate_effects) noexcept
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    g_shared_full_cache.valid = false;
    g_shared_hud_cache.valid = false;
    if (invalidate_effects) {
        g_heavy_snapshot_slices.effects_timestamp_ms = 0;
    }
}

void LogWearableLightDiagnostic(std::string_view reason) noexcept
{
    try {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player == nullptr || player->GetParentCell() == nullptr) {
            return;
        }

        std::string worn_desc;
        for (const auto& [form, inv_entry] : player->GetInventory()) {
            if (form == nullptr || inv_entry.first <= 0) {
                continue;
            }
            const auto* armor = form->As<RE::TESObjectARMO>();
            auto* entry_data = inv_entry.second.get();
            if (armor == nullptr || entry_data == nullptr) {
                continue;
            }
            RE::ExtraDataList* worn_extra = nullptr;
            bool is_worn = false;
            if (entry_data->extraLists != nullptr) {
                for (auto* candidate : *entry_data->extraLists) {
                    if (candidate != nullptr &&
                        (candidate->HasType<RE::ExtraWorn>() || candidate->HasType<RE::ExtraWornLeft>())) {
                        worn_extra = candidate;
                        is_worn = true;
                        break;
                    }
                }
            }
            if (!is_worn && entry_data->IsWorn()) {
                is_worn = true;
            }
            if (!is_worn || !HasLightArchetypeEnchantment(form, worn_extra, entry_data)) {
                continue;
            }
            if (!worn_desc.empty()) {
                worn_desc.append(", ");
            }
            const char* item_name = armor->GetName();
            char buf[256]{};
            std::snprintf(buf, sizeof(buf), "%08X(%s)",
                armor->GetFormID(),
                (item_name != nullptr && item_name[0] != '\0') ? item_name : "<unnamed>");
            worn_desc.append(buf);
        }

        std::vector<RE::ActiveEffect*> light_effects;
        if (auto* magic_target = player->AsMagicTarget(); magic_target != nullptr) {
            RE::MagicTarget::EffectVisitor visitor([&](RE::ActiveEffect* ae) {
                if (ae == nullptr) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                const auto* base = ae->GetBaseObject();
                if (base != nullptr && base->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kLight) {
                    light_effects.push_back(ae);
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            magic_target->VisitEffects(visitor);
        }

        const auto* cam = RE::PlayerCamera::GetSingleton();
        const bool is_3rd_person = (cam != nullptr && cam->IsInThirdPerson());
        const bool has_3d = player->Is3DLoaded();
        auto* p3d = player->Get3D();
        auto* p3d_3rd = player->Get3D(false);
        auto* p3d_1st = player->Get3D(true);
        const auto player_pos = player->GetPosition();

        std::string equipped_torch_desc;
        for (const bool left_hand : { false, true }) {
            if (const auto* eq = player->GetEquippedObject(left_hand);
                eq != nullptr && eq->Is(RE::FormType::Light)) {
                if (!equipped_torch_desc.empty()) {
                    equipped_torch_desc.append(", ");
                }
                const char* eq_name = eq->GetName();
                char ebuf[192]{};
                std::snprintf(ebuf, sizeof(ebuf), "%s:%08X(%s)",
                    left_hand ? "L" : "R",
                    eq->GetFormID(),
                    (eq_name != nullptr && eq_name[0] != '\0') ? eq_name : "<unnamed>");
                equipped_torch_desc.append(ebuf);
            }
        }

        const auto* extra_magic_light = player->extraList.GetByType<RE::ExtraMagicLight>();
        const bool has_extra_magic = (extra_magic_light != nullptr);
        const RE::NiLight* extra_light_ptr = (extra_magic_light != nullptr && extra_magic_light->lightData != nullptr)
            ? extra_magic_light->lightData->light.get()
            : nullptr;
        const float extra_dimmer = (extra_magic_light != nullptr && extra_magic_light->lightData != nullptr)
            ? extra_magic_light->lightData->wantDimmer
            : -1.0F;
        const auto* extra_light = player->extraList.GetByType<RE::ExtraLight>();
        const bool has_extra_light = (extra_light != nullptr);
        const RE::NiLight* extra_ref_light_ptr = (extra_light != nullptr && extra_light->lightData != nullptr)
            ? extra_light->lightData->light.get()
            : nullptr;

        const auto format_ni_light = [](std::string_view tag, const RE::NiLight* light, float dimmer) {
            if (light == nullptr) {
                return std::string(tag) + "=none";
            }
            const char* name = (light->name.c_str() != nullptr) ? light->name.c_str() : "<null>";
            const char* parent_name = (light->parent != nullptr && light->parent->name.c_str() != nullptr)
                ? light->parent->name.c_str()
                : "<null>";
            const auto& lrt = light->GetLightRuntimeData();
            char buf[320]{};
            std::snprintf(buf, sizeof(buf),
                "%.*s=0x%llX('%s',parent='%s',cull=%d,dim=%.2f,pos=(%.1f,%.1f,%.1f),fade=%.2f,rad=%.1f,rgb=(%.2f,%.2f,%.2f))",
                static_cast<int>(tag.size()), tag.data(),
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(light)),
                name, parent_name, light->GetAppCulled() ? 1 : 0, dimmer,
                light->world.translate.x, light->world.translate.y, light->world.translate.z,
                lrt.fade, lrt.radius.x, lrt.diffuse.red, lrt.diffuse.green, lrt.diffuse.blue);
            return std::string(buf);
        };

        std::string extra_light_details;
        if (extra_light_ptr != nullptr) {
            extra_light_details = format_ni_light("extraMagic", extra_light_ptr, extra_dimmer);
        }
        if (extra_ref_light_ptr != nullptr) {
            if (!extra_light_details.empty()) extra_light_details.append("; ");
            const float ref_dimmer = extra_light->lightData->wantDimmer;
            extra_light_details.append(format_ni_light("extraLight", extra_ref_light_ptr, ref_dimmer));
        }

        const auto is_attach_light_name = [](const char* raw_name) {
            if (raw_name == nullptr || raw_name[0] == '\0') return false;
            std::string lower;
            for (const char* p = raw_name; *p != '\0'; ++p) {
                lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
            }
            return lower.find("attachlight") != std::string::npos ||
                   lower.find("attachenblight") != std::string::npos;
        };

        std::string attached_3d_lights;
        const auto scan_actor_3d = [&](const char* root_tag, RE::NiAVObject* root) {
            if (root == nullptr) return;
            std::size_t visited = 0;
            std::size_t reported = 0;
            std::vector<std::pair<RE::NiAVObject*, int>> stack;
            stack.emplace_back(root, 0);
            while (!stack.empty() && visited < 512U && reported < 8U) {
                auto [obj, depth] = stack.back();
                stack.pop_back();
                if (obj == nullptr) continue;
                ++visited;
                const char* obj_name = (obj->name.c_str() != nullptr) ? obj->name.c_str() : "";
                const auto* ni_light = netimmerse_cast<const RE::NiLight*>(obj);
                if (ni_light != nullptr || is_attach_light_name(obj_name)) {
                    ++reported;
                    if (!attached_3d_lights.empty()) attached_3d_lights.append("; ");
                    const char* parent_name = (obj->parent != nullptr && obj->parent->name.c_str() != nullptr)
                        ? obj->parent->name.c_str()
                        : "<null>";
                    if (ni_light != nullptr) {
                        const auto& lrt = ni_light->GetLightRuntimeData();
                        char abuf[320]{};
                        std::snprintf(abuf, sizeof(abuf),
                            "%s:'%s'(p='%s',niLight=1,cull=%d,pos=(%.1f,%.1f,%.1f),fade=%.2f,rad=%.1f,rgb=(%.2f,%.2f,%.2f))",
                            root_tag, obj_name, parent_name, obj->GetAppCulled() ? 1 : 0,
                            obj->world.translate.x, obj->world.translate.y, obj->world.translate.z,
                            lrt.fade, lrt.radius.x, lrt.diffuse.red, lrt.diffuse.green, lrt.diffuse.blue);
                        attached_3d_lights.append(abuf);
                    } else {
                        char abuf[256]{};
                        std::snprintf(abuf, sizeof(abuf),
                            "%s:'%s'(p='%s',niLight=0,cull=%d,pos=(%.1f,%.1f,%.1f))",
                            root_tag, obj_name, parent_name, obj->GetAppCulled() ? 1 : 0,
                            obj->world.translate.x, obj->world.translate.y, obj->world.translate.z);
                        attached_3d_lights.append(abuf);
                    }
                }
                if (depth < 12) {
                    if (auto* node = obj->AsNode(); node != nullptr) {
                        const auto& children = node->GetChildren();
                        for (std::uint16_t i = 0; i < children.size(); ++i) {
                            if (auto* child = children[i].get(); child != nullptr) {
                                stack.emplace_back(child, depth + 1);
                            }
                        }
                    }
                }
            }
        };
        scan_actor_3d("3rd", p3d_3rd);
        if (p3d_1st != nullptr && p3d_1st != p3d_3rd) {
            scan_actor_3d("1st", p3d_1st);
        }

        const auto& shader_state = RE::BSShaderManager::State::GetSingleton();
        auto* ssn = shader_state.shadowSceneNode[0];
        bool disable_light_upd = false;
        std::size_t active_lights = 0;
        std::size_t active_shadow_lights = 0;
        std::size_t queue_add = 0;
        std::size_t queue_rem = 0;
        std::string nearby_bs_lights;
        if (ssn != nullptr) {
            const auto& rt = ssn->GetRuntimeData();
            disable_light_upd = rt.disableLightUpdate;
            active_lights = rt.activeLights.size();
            active_shadow_lights = rt.activeShadowLights.size();
            {
                const RE::BSSpinLockGuard queue_guard(rt.lightQueueLock);
                queue_add = rt.lightQueueAdd.size();
                queue_rem = rt.lightQueueRemove.size();
            }

            struct NearbyBsEntry {
                float dist{};
                bool is_shadow{};
                const RE::BSLight* bs{};
            };
            std::vector<NearbyBsEntry> nearby;
            const auto consider_light = [&](const RE::BSLight* bsl, bool is_shadow) {
                if (bsl == nullptr) return;
                const auto* ni = bsl->light.get();
                const RE::NiPoint3 pos = ni != nullptr ? ni->world.translate : bsl->worldTranslate;
                const float dx = pos.x - player_pos.x;
                const float dy = pos.y - player_pos.y;
                const float dz = pos.z - player_pos.z;
                const float dist_sq = dx * dx + dy * dy + dz * dz;
                if (dist_sq <= 250.0F * 250.0F) {
                    nearby.push_back({ std::sqrt(dist_sq), is_shadow, bsl });
                }
            };
            for (const auto& bsl : rt.activeLights) consider_light(bsl.get(), false);
            for (const auto& bssl : rt.activeShadowLights) consider_light(bssl.get(), true);
            std::sort(nearby.begin(), nearby.end(), [](const NearbyBsEntry& a, const NearbyBsEntry& b) {
                return a.dist < b.dist;
            });
            const auto is_under_player = [&](const RE::NiAVObject* obj) {
                for (int steps = 0; obj != nullptr && steps < 32; ++steps, obj = obj->parent) {
                    if (obj == p3d_3rd || obj == p3d_1st) return true;
                }
                return false;
            };
            const std::size_t limit = std::min<std::size_t>(nearby.size(), 8U);
            for (std::size_t i = 0; i < limit; ++i) {
                const auto& item = nearby[i];
                const auto* bsl = item.bs;
                const auto* ni = bsl->light.get();
                const char* p_name = (ni != nullptr && ni->parent != nullptr && ni->parent->name.c_str() != nullptr)
                    ? ni->parent->name.c_str()
                    : "<null>";
                const bool on_player = (ni != nullptr && is_under_player(ni));
                if (!nearby_bs_lights.empty()) nearby_bs_lights.append("; ");
                if (ni != nullptr) {
                    const auto& lrt = ni->GetLightRuntimeData();
                    char nbuf[320]{};
                    std::snprintf(nbuf, sizeof(nbuf),
                        "{dist=%.1f,sh=%d,dyn=%d,pt=%d,lum=%.2f,onP=%d,parent='%s',rad=%.1f,fade=%.2f,rgb=(%.2f,%.2f,%.2f)}",
                        item.dist, item.is_shadow ? 1 : 0, bsl->dynamic ? 1 : 0, bsl->pointLight ? 1 : 0,
                        bsl->luminance, on_player ? 1 : 0, p_name,
                        lrt.radius.x, lrt.fade, lrt.diffuse.red, lrt.diffuse.green, lrt.diffuse.blue);
                    nearby_bs_lights.append(nbuf);
                } else {
                    char nbuf[160]{};
                    std::snprintf(nbuf, sizeof(nbuf),
                        "{dist=%.1f,sh=%d,dyn=%d,pt=%d,lum=%.2f,ni=null}",
                        item.dist, item.is_shadow ? 1 : 0, bsl->dynamic ? 1 : 0, bsl->pointLight ? 1 : 0,
                        bsl->luminance);
                    nearby_bs_lights.append(nbuf);
                }
            }
        }

        if (worn_desc.empty() && light_effects.empty() && !has_extra_magic && !has_extra_light &&
            equipped_torch_desc.empty() && attached_3d_lights.empty() && nearby_bs_lights.empty()) {
            return;
        }

        std::uint32_t paused_count = 0;
        std::uint32_t custom_render_count = 0;
        std::uint32_t item_menus_count = 0;
        std::size_t menu_stack_size = 0;
        std::string open_menus;
        if (auto* ui = RE::UI::GetSingleton(); ui != nullptr) {
            paused_count = ui->numPausesGame;
            custom_render_count = ui->numCustomRendering;
            item_menus_count = ui->numItemMenus;
            menu_stack_size = ui->menuStack.size();
            for (const auto& entry : ui->menuMap) {
                const auto* menu_name = entry.first.c_str();
                if (entry.second.menu != nullptr && menu_name != nullptr && menu_name[0] != '\0' &&
                    ui->IsMenuOpen(menu_name)) {
                    if (!open_menus.empty()) {
                        open_menus.push_back(',');
                    }
                    open_menus.append(menu_name);
                }
            }
        }

        std::int32_t magic_light_max = -999;
        if (auto* gs = RE::GameSettingCollection::GetSingleton(); gs != nullptr) {
            if (const auto* setting = gs->GetSetting("iMagicLightMaxCount"); setting != nullptr) {
                magic_light_max = setting->GetInteger();
            }
        }

        SKSE::log::info(
            "SkyUltra wearable light diag [{}]: paused={}, customRender={}, itemMenus={}, stack={}, openMenus=[{}], "
            "3rdPerson={}, has3D={}, p3d=0x{:X}/0x{:X}/0x{:X}(eq3rd={}), iMagicLightMaxCount={}, "
            "extraMagicLight={} (ptr=0x{:X}, dimmer={:.2f}), extraLight={}, torch=[{}], "
            "ssn=0x{:X} (disableUpd={}, active={}, shadow={}, qAdd={}, qRem={}, sg={}), "
            "playerPos=({:.1f},{:.1f},{:.1f}), presentHook={}, worn=[{}], kLightEffects={}.",
            reason,
            paused_count,
            custom_render_count,
            item_menus_count,
            menu_stack_size,
            open_menus,
            is_3rd_person,
            has_3d,
            reinterpret_cast<std::uintptr_t>(p3d),
            reinterpret_cast<std::uintptr_t>(p3d_3rd),
            reinterpret_cast<std::uintptr_t>(p3d_1st),
            p3d == p3d_3rd,
            magic_light_max,
            has_extra_magic,
            reinterpret_cast<std::uintptr_t>(extra_light_ptr),
            extra_dimmer,
            has_extra_light,
            equipped_torch_desc,
            reinterpret_cast<std::uintptr_t>(ssn),
            disable_light_upd,
            active_lights,
            active_shadow_lights,
            queue_add,
            queue_rem,
            shader_state.sceneGraph,
            player_pos.x,
            player_pos.y,
            player_pos.z,
            skyultra::IsD3D11PresentHookInstalled(),
            worn_desc,
            light_effects.size());

        if (!extra_light_details.empty() || !attached_3d_lights.empty()) {
            SKSE::log::info(
                "SkyUltra wearable light extra/3d [{}]: extra=[{}], player3dLights=[{}].",
                reason,
                extra_light_details,
                attached_3d_lights);
        }
        if (!nearby_bs_lights.empty()) {
            SKSE::log::info(
                "SkyUltra wearable light nearby [{}]: [{}].",
                reason,
                nearby_bs_lights);
        }

        for (std::size_t idx = 0; idx < light_effects.size(); ++idx) {
            auto* ae = light_effects[idx];
            if (ae == nullptr) {
                continue;
            }
            const auto* base = ae->GetBaseObject();
            const RE::FormID src_id = ae->source != nullptr ? ae->source->GetFormID() : 0U;
            const RE::FormID base_id = base != nullptr ? base->GetFormID() : 0U;
            const auto flags_val = ae->flags.underlying();
            const auto cond_val = static_cast<std::int32_t>(ae->conditionStatus.get());
            const RE::FormID assoc_id = (base != nullptr && base->data.associatedForm != nullptr)
                ? base->data.associatedForm->GetFormID()
                : 0U;
            const int assoc_type = (base != nullptr && base->data.associatedForm != nullptr)
                ? static_cast<int>(base->data.associatedForm->GetFormType())
                : -1;
            const RE::FormID hit_art_id = (base != nullptr && base->data.hitEffectArt != nullptr)
                ? base->data.hitEffectArt->GetFormID()
                : 0U;

            std::size_t hit_count = 0;
            std::string hit_details;
            if (ae->hitEffects != nullptr) {
                for (auto* ref_fx : *ae->hitEffects) {
                    if (ref_fx == nullptr) {
                        continue;
                    }
                    ++hit_count;
                    const bool attached = ref_fx->GetAttached();
                    const bool finished = ref_fx->finished;
                    auto* mre = ref_fx->As<RE::ModelReferenceEffect>();
                    if (mre == nullptr) {
                        mre = netimmerse_cast<RE::ModelReferenceEffect*>(ref_fx);
                    }
                    const bool is_mre = (mre != nullptr);
                    const std::uint32_t mre_flags = is_mre ? mre->flags.underlying() : 0U;
                    const bool has_art_obj = is_mre && (mre->artObject != nullptr);
                    const bool has_clone_task = is_mre && (mre->cloneTask != nullptr);
                    const bool has_art_3d = is_mre && (mre->artObject3D != nullptr);
                    auto* root = ref_fx->GetAttachRoot();
                    const char* root_name = (root != nullptr && root->name.c_str() != nullptr)
                        ? root->name.c_str()
                        : "<null>";
                    const bool root_is_actor3d = (root != nullptr && root == p3d);
                    auto* attach_light = (root != nullptr)
                        ? root->GetObjectByName(RE::BSFixedString("AttachLight"))
                        : nullptr;
                    const bool has_attach_light = (attach_light != nullptr);
                    const bool attach_culled = has_attach_light && attach_light->GetAppCulled();
                    const RE::NiPoint3 al_pos = has_attach_light ? attach_light->world.translate : RE::NiPoint3{};

                    if (!hit_details.empty()) {
                        hit_details.append("; ");
                    }
                    char hbuf[384]{};
                    std::snprintf(hbuf, sizeof(hbuf),
                        "#%zu{att=%d,fin=%d,mre=%d,mFlags=0x%X,artObj=%d,clone=%d,art3D=%d,"
                        "root='%s',rootEqP3D=%d,attachLight=%d,alCull=%d,alPos=(%.1f,%.1f,%.1f)}",
                        hit_count - 1,
                        attached ? 1 : 0,
                        finished ? 1 : 0,
                        is_mre ? 1 : 0,
                        mre_flags,
                        has_art_obj ? 1 : 0,
                        has_clone_task ? 1 : 0,
                        has_art_3d ? 1 : 0,
                        root_name,
                        root_is_actor3d ? 1 : 0,
                        has_attach_light ? 1 : 0,
                        attach_culled ? 1 : 0,
                        al_pos.x, al_pos.y, al_pos.z);
                    hit_details.append(hbuf);
                }
            }

            const auto* light_fx = skyrim_cast<RE::LightEffect*>(ae);
            RE::NiPointLight* npl = (light_fx != nullptr) ? light_fx->light.get() : nullptr;
            std::string npl_info = "none";
            if (npl != nullptr) {
                const char* parent_name = (npl->parent != nullptr && npl->parent->name.c_str() != nullptr)
                    ? npl->parent->name.c_str()
                    : "<null>";
                const auto& lrt = npl->GetLightRuntimeData();
                char nbuf[320]{};
                std::snprintf(nbuf, sizeof(nbuf),
                    "ptr=0x%llX,eqExtra=%d,parent='%s',cull=%d,pos=(%.1f,%.1f,%.1f),"
                    "fade=%.2f,rad=%.1f,rgb=(%.2f,%.2f,%.2f)",
                    static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(npl)),
                    (static_cast<const RE::NiLight*>(npl) == extra_light_ptr) ? 1 : 0,
                    parent_name,
                    npl->GetAppCulled() ? 1 : 0,
                    npl->world.translate.x, npl->world.translate.y, npl->world.translate.z,
                    lrt.fade,
                    lrt.radius.x,
                    lrt.diffuse.red, lrt.diffuse.green, lrt.diffuse.blue);
                npl_info = nbuf;
            }

            const RE::NiLight* target_light = npl != nullptr
                ? static_cast<const RE::NiLight*>(npl)
                : extra_light_ptr;
            std::string bs_info = "none";
            if (ssn != nullptr && target_light != nullptr) {
                const auto& rt = ssn->GetRuntimeData();
                bool in_active = false;
                bool in_shadow = false;
                bool in_q_add = false;
                const RE::BSLight* found_bs = nullptr;
                for (const auto& bsl : rt.activeLights) {
                    if (bsl != nullptr && bsl->light.get() == target_light) {
                        in_active = true;
                        if (found_bs == nullptr) {
                            found_bs = bsl.get();
                        }
                    }
                }
                for (const auto& bssl : rt.activeShadowLights) {
                    if (bssl != nullptr && bssl->light.get() == target_light) {
                        in_shadow = true;
                        if (found_bs == nullptr) {
                            found_bs = bssl.get();
                        }
                    }
                }
                {
                    const RE::BSSpinLockGuard queue_guard(rt.lightQueueLock);
                    for (const auto& bsl : rt.lightQueueAdd) {
                        if (bsl != nullptr && bsl->light.get() == target_light) {
                            in_q_add = true;
                            if (found_bs == nullptr) {
                                found_bs = bsl.get();
                            }
                        }
                    }
                }
                if (found_bs != nullptr) {
                    char bbuf[256]{};
                    std::snprintf(bbuf, sizeof(bbuf),
                        "active=%d,shadow=%d,qAdd=%d,pt=%d,dyn=%d,lum=%.2f,lod=%.2f,"
                        "frust=%u,land=%d,water=%d,wPos=(%.1f,%.1f,%.1f)",
                        in_active ? 1 : 0,
                        in_shadow ? 1 : 0,
                        in_q_add ? 1 : 0,
                        found_bs->pointLight ? 1 : 0,
                        found_bs->dynamic ? 1 : 0,
                        found_bs->luminance,
                        found_bs->lodDimmer,
                        found_bs->frustrumCull,
                        found_bs->affectLand ? 1 : 0,
                        found_bs->affectWater ? 1 : 0,
                        found_bs->worldTranslate.x, found_bs->worldTranslate.y, found_bs->worldTranslate.z);
                    bs_info = bbuf;
                } else {
                    bs_info = "notInSsn";
                }
            }

            SKSE::log::info(
                "SkyUltra wearable light fx[{}] [{}]: src={:08X}, base={:08X}, flags=0x{:X}, cond={}, "
                "elapsed={:.2f}/{:.2f}, mag={:.2f}, assoc={:08X}(type={}), hitArt={:08X}, "
                "hitEffects({})=[{}], npl=[{}], bsLight=[{}].",
                idx,
                reason,
                src_id,
                base_id,
                flags_val,
                cond_val,
                ae->elapsedSeconds,
                ae->duration,
                ae->magnitude,
                assoc_id,
                assoc_type,
                hit_art_id,
                hit_count,
                hit_details,
                npl_info,
                bs_info);
        }
    } catch (...) {
    }
}

std::optional<float> ReadPlayerCompassHeadingFast() noexcept
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!IsPlayerInSafeGameplayState(player, false)) {
        return std::nullopt;
    }
    return ReadCompassHeadingDeg(player);
}

std::optional<double> ReadPlayerCompassHeadingPrecise() noexcept
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!IsPlayerInSafeGameplayState(player, false)) {
        return std::nullopt;
    }
    return ReadCompassHeadingPreciseDeg(player);
}

void SetCompassMarkerRadiusMeters(float radius_meters) noexcept
{
    const float clamped = std::isfinite(radius_meters)
        ? std::clamp(radius_meters, 50.0F, 1000.0F)
        : 50.0F;
    g_compass_marker_radius_meters.store(std::round(clamped), std::memory_order_relaxed);
}

float GetCompassMarkerRadiusMeters() noexcept
{
    return std::clamp(g_compass_marker_radius_meters.load(std::memory_order_relaxed), 50.0F, 1000.0F);
}

std::string CollectSharedGameDataUpdateJSON(
    bool compact,
    const std::vector<std::string>& subscribed_topics,
    std::unordered_map<std::string, std::string>& in_out_topic_signatures,
    std::uint64_t& out_sequence,
    bool force_full,
    bool include_heading_only_updates)
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    const bool needs_heavy_hud = !compact ||
        subscribed_topics.empty() ||
        std::any_of(subscribed_topics.begin(), subscribed_topics.end(), [](const std::string& t) {
            return t == "equipment" || t == "quests" || t == "followers";
        });
    if (compact && !force_full &&
        g_shared_hud_cache.valid &&
        g_shared_hud_cache.snapshot.value("available", false) &&
        !g_shared_hud_cache.vitals_core_serialized.empty() &&
        g_heavy_snapshot_slices.valid &&
        g_heavy_snapshot_slices.effects_timestamp_ms != 0 &&
        g_compass_live_cache.valid) {
        const auto now_ms = SharedSnapshotNowMs();
        const float active_compass_radius_m = GetCompassMarkerRadiusMeters();
        const bool caches_fresh =
            now_ms >= g_shared_hud_cache.timestamp_ms &&
            (now_ms - g_shared_hud_cache.timestamp_ms) < 120 &&
            now_ms >= g_compass_live_cache.timestamp_ms &&
            (now_ms - g_compass_live_cache.timestamp_ms) < 150 &&
            std::abs(g_compass_live_cache.radius_meters - active_compass_radius_m) < 0.5F &&
            (!needs_heavy_hud ||
             (g_heavy_snapshot_slices.has_heavy_hud_slices &&
              (now_ms - g_heavy_snapshot_slices.timestamp_ms) < kHeavySnapshotSlicesCacheTtlMs)) &&
            (now_ms - g_heavy_snapshot_slices.effects_timestamp_ms) < 15000 &&
            g_compass_live_cache.serialized == g_shared_hud_cache.vitals_compass_serialized;
        if (caches_fresh &&
            in_out_topic_signatures.contains("vitals.core") &&
            in_out_topic_signatures.contains("vitals.compass") &&
            in_out_topic_signatures.contains("vitals.heading") &&
            in_out_topic_signatures["vitals.core"] == g_shared_hud_cache.vitals_core_serialized &&
            in_out_topic_signatures["vitals.compass"] == g_shared_hud_cache.vitals_compass_serialized) {
            bool non_vitals_match = true;
            if (!subscribed_topics.empty()) {
                for (const auto& topic : subscribed_topics) {
                    if (topic == "vitals") {
                        continue;
                    }
                    const auto ser_it = g_shared_hud_cache.topic_serialized.find(topic);
                    const auto prev_it = in_out_topic_signatures.find(topic);
                    if (ser_it == g_shared_hud_cache.topic_serialized.end() || !ser_it->second ||
                        prev_it == in_out_topic_signatures.end() || prev_it->second != *ser_it->second) {
                        non_vitals_match = false;
                        break;
                    }
                }
            } else {
                for (const auto topic : kCanonicalGameTopics) {
                    if (topic == "vitals") {
                        continue;
                    }
                    const std::string topic_str(topic);
                    const auto ser_it = g_shared_hud_cache.topic_serialized.find(topic_str);
                    const auto prev_it = in_out_topic_signatures.find(topic_str);
                    if (ser_it == g_shared_hud_cache.topic_serialized.end() || !ser_it->second ||
                        prev_it == in_out_topic_signatures.end() || prev_it->second != *ser_it->second) {
                        non_vitals_match = false;
                        break;
                    }
                }
            }
            if (non_vitals_match) {
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (IsPlayerInSafeGameplayState(player, false)) {
                    auto* parent_cell = player->GetParentCell();
                    auto* worldspace = player->GetWorldspace();
                    const std::uint32_t cell_id = parent_cell != nullptr ? parent_cell->GetFormID() : 0U;
                    const std::uint32_t world_id = worldspace != nullptr ? worldspace->GetFormID() : 0U;
                    const auto player_pos = player->GetPosition();
                    const float cdx = player_pos.x - g_compass_live_cache.player_pos.x;
                    const float cdy = player_pos.y - g_compass_live_cache.player_pos.y;
                    const float cdz = player_pos.z - g_compass_live_cache.player_pos.z;
                    const bool same_place = parent_cell != nullptr && parent_cell->IsAttached() &&
                        world_id == g_compass_live_cache.world_form_id &&
                        (!parent_cell->IsInteriorCell() || cell_id == g_compass_live_cache.cell_form_id) &&
                        (cdx * cdx + cdy * cdy + cdz * cdz) < (32.0F * 32.0F);
                    if (same_place) {
                        if (!include_heading_only_updates) {
                            out_sequence = g_shared_hud_cache.sequence;
                            return {};
                        }
                        const float heading_deg = ReadCompassHeadingDeg(player);
                        const std::string heading_serialized = Json(heading_deg).dump();
                        if (in_out_topic_signatures["vitals.heading"] == heading_serialized) {
                            out_sequence = g_shared_hud_cache.sequence;
                            return {};
                        }
                        in_out_topic_signatures["vitals.heading"] = heading_serialized;
                        g_shared_hud_cache.vitals_heading_serialized = heading_serialized;
                        g_shared_hud_cache.snapshot["heading"] = heading_deg;
                        auto vitals_sig = std::make_shared<std::string>(
                            g_shared_hud_cache.vitals_core_serialized + "|" + heading_serialized + "|" +
                            g_shared_hud_cache.vitals_compass_serialized);
                        in_out_topic_signatures["vitals"] = *vitals_sig;
                        g_shared_hud_cache.topic_serialized["vitals"] = std::move(vitals_sig);
                        out_sequence = ++g_shared_snapshot_sequence;
                        g_shared_hud_cache.sequence = out_sequence;
                        g_shared_hud_cache.snapshot["sequence"] = out_sequence;
                        return Json{
                            { "available", true },
                            { "sequence", out_sequence },
                            { "snapshotScope", "hud" },
                            { "changedTopics", Json::array({ "vitals" }) },
                            { "delta", { { "heading", heading_deg } } }
                        }.dump();
                    }
                }
            }
        }
    }

    const auto& cached = GetOrCollectSharedSnapshot(compact, needs_heavy_hud);
    out_sequence = cached.sequence;

    if (!cached.snapshot.value("available", false)) {
        const auto existing = in_out_topic_signatures.find("__available");
        if (!force_full && existing != in_out_topic_signatures.end() && existing->second == "false") {
            return {};
        }
        in_out_topic_signatures.clear();
        in_out_topic_signatures.emplace("__available", "false");
        return Json{
            { "available", false },
            { "sequence", cached.sequence },
            { "snapshotScope", compact ? "hud" : "full" },
            { "changedTopics", Json::array() },
            { "delta", Json::object() }
        }.dump();
    }

    in_out_topic_signatures["__available"] = "true";
    std::vector<std::string> effective_topics;
    if (subscribed_topics.empty()) {
        effective_topics.reserve(kCanonicalGameTopics.size());
        for (const auto topic : kCanonicalGameTopics) {
            effective_topics.emplace_back(topic);
        }
    } else {
        effective_topics = subscribed_topics;
    }

    Json changed_topics = Json::array();
    Json delta = Json::object();
    for (const auto& topic : effective_topics) {
        const auto slice_it = cached.topic_slices.find(topic);
        const auto serialized_it = cached.topic_serialized.find(topic);
        if (slice_it == cached.topic_slices.end() || serialized_it == cached.topic_serialized.end() ||
            !slice_it->second || !serialized_it->second) {
            continue;
        }
        if (topic == "vitals") {
            const auto prev_vitals = in_out_topic_signatures.find("vitals");
            const bool has_sub_signatures = !force_full && prev_vitals != in_out_topic_signatures.end() &&
                in_out_topic_signatures.contains("vitals.core") &&
                in_out_topic_signatures.contains("vitals.compass") &&
                in_out_topic_signatures.contains("vitals.heading");
            if (!has_sub_signatures) {
                in_out_topic_signatures["vitals"] = *serialized_it->second;
                in_out_topic_signatures["vitals.core"] = cached.vitals_core_serialized;
                in_out_topic_signatures["vitals.compass"] = cached.vitals_compass_serialized;
                in_out_topic_signatures["vitals.heading"] = cached.vitals_heading_serialized;
                changed_topics.push_back(topic);
                for (const auto& [field_key, field_val] : slice_it->second->items()) {
                    delta[field_key] = field_val;
                }
                delta["heading"] = cached.snapshot.value("heading", 0.0F);
                continue;
            }
            bool vitals_changed = false;
            if (in_out_topic_signatures["vitals.core"] != cached.vitals_core_serialized) {
                in_out_topic_signatures["vitals.core"] = cached.vitals_core_serialized;
                for (const auto& [field_key, field_val] : cached.vitals_core_slice.items()) {
                    delta[field_key] = field_val;
                }
                vitals_changed = true;
            }
            if (in_out_topic_signatures["vitals.compass"] != cached.vitals_compass_serialized) {
                in_out_topic_signatures["vitals.compass"] = cached.vitals_compass_serialized;
                delta["compassMarkers"] = cached.snapshot.contains("compassMarkers")
                    ? cached.snapshot["compassMarkers"]
                    : Json::array();
                vitals_changed = true;
            }
            if (in_out_topic_signatures["vitals.heading"] != cached.vitals_heading_serialized) {
                in_out_topic_signatures["vitals.heading"] = cached.vitals_heading_serialized;
                if (include_heading_only_updates || vitals_changed || force_full) {
                    delta["heading"] = cached.snapshot.value("heading", 0.0F);
                    vitals_changed = true;
                }
            }
            if (vitals_changed) {
                in_out_topic_signatures["vitals"] = *serialized_it->second;
                changed_topics.push_back(topic);
            }
            continue;
        }
        const auto prev_it = in_out_topic_signatures.find(topic);
        if (!force_full && prev_it != in_out_topic_signatures.end() &&
            prev_it->second == *serialized_it->second) {
            continue;
        }
        in_out_topic_signatures[topic] = *serialized_it->second;
        changed_topics.push_back(topic);
        for (const auto& [field_key, field_val] : slice_it->second->items()) {
            delta[field_key] = field_val;
        }
    }

    if (changed_topics.empty() && !force_full) {
        return {};
    }

    Json update{
        { "available", true },
        { "sequence", cached.sequence },
        { "snapshotScope", compact ? "hud" : "full" },
        { "changedTopics", std::move(changed_topics) },
        { "delta", std::move(delta) }
    };
    return update.dump();
}

std::string ReadGameDataJSON(std::string_view method, std::string_view params_json)
{
    const std::scoped_lock lock(g_shared_game_data_mutex);
    if(method=="game.getInventory")return ReadInventoryPage(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.equipItem"||method=="game.unequipItem")return ChangePlayerEquipment(Json::parse(params_json,nullptr,false),method=="game.unequipItem").dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.itemAction")return PerformInventoryItemAction(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.renameItem")return RenamePlayerItem(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.getBarterSnapshot")return ReadBarterSnapshot(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.barterTransaction")return PerformBarterTransaction(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.getContainerSnapshot")return ReadContainerSnapshot(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.containerTransfer")return PerformContainerTransfer(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.getBlacksmithSnapshot")return ReadBlacksmithSnapshot(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.blacksmithCraft")return PerformBlacksmithCraft(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.blacksmithTemper")return PerformBlacksmithTemper(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.getEnchantingSnapshot")return ReadEnchantingSnapshot(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.enchantItem")return PerformEnchantItem(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.disenchantItem")return PerformDisenchantItem(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.renameEnchantment")return PerformRenameEnchantment(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.getAlchemySnapshot")return ReadAlchemySnapshot(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.alchemyCraft")return PerformAlchemyCraft(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.eatIngredient")return PerformEatIngredient(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.getActiveBookSnapshot")return ReadActiveBookSnapshot(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.takeActiveBook")return PerformTakeActiveBook(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.getSpells")return ReadSpellBook().dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.equipSpell")return EquipPlayerSpell(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.renameSpell")return RenamePlayerSpell(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.deleteSpell")return DeletePlayerSpellOrEffect(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.getFavorites")return ReadPlayerFavorites().dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.setFavorite")return SetPlayerFavorite(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if (method == "game.getPlayerSnapshot") {
        const auto options = Json::parse(params_json, nullptr, false);
        const bool compact = options.is_object() && options.contains("scope") && options["scope"] == "hud";
        return ReadPlayerSnapshotJSON(compact).c_str();
    }
    const RE::BSFixedString params(std::string(params_json).c_str());
    if (method == "game.getNearbyNpcs") return PapyrusGetNearbyNpcsJSON(nullptr, params).c_str();
    if (method == "game.searchNpcs") return PapyrusSearchNpcsJSON(nullptr, params).c_str();
    return R"({"available":false})";
}

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm)
{
    if (vm == nullptr) {
        return false;
    }
    vm->RegisterFunction("RegisterMod", kPapyrusClass, PapyrusRegisterMod);
    vm->RegisterFunction("RegisterModForQuest", kPapyrusClass, PapyrusRegisterModForQuest);
    vm->RegisterFunction("GetModFolderName", kPapyrusClass, PapyrusGetModFolderName);
    vm->RegisterFunction("GetPlayerSnapshotJSON", kPapyrusClass, PapyrusGetPlayerSnapshotJSON);
    vm->RegisterFunction("SearchNpcsJSON", kPapyrusClass, PapyrusSearchNpcsJSON);
    vm->RegisterFunction("GetNearbyNpcsJSON", kPapyrusClass, PapyrusGetNearbyNpcsJSON);
    vm->RegisterFunction("IsReady", kPapyrusClass, PapyrusIsReady);
    vm->RegisterFunction("CreateModView", kPapyrusClass, PapyrusCreateView);
    vm->RegisterFunction("CreateFullScreenModView", kPapyrusClass, PapyrusCreateFullScreenModView);
    vm->RegisterFunction("GetViewState", kPapyrusClass, PapyrusGetViewState);
    vm->RegisterFunction("SetActorPreview",kPapyrusClass,PapyrusSetActorPreview);
    vm->RegisterFunction("CloseActorPreview",kPapyrusClass,PapyrusCloseActorPreview);
    vm->RegisterFunction("GetActorPreviewStatusJSON",kPapyrusClass,PapyrusActorPreviewStatus);
    vm->RegisterFunction("SetViewVisible", kPapyrusClass, PapyrusSetViewVisible);
    vm->RegisterFunction("SetViewBounds", kPapyrusClass, PapyrusSetViewBounds);
    vm->RegisterFunction("SendEvent", kPapyrusClass, PapyrusSendEvent);
    vm->RegisterFunction("AddPageRequestMethod", kPapyrusClass, PapyrusAddPageRequestMethod);
    vm->RegisterFunction("RemovePageRequestMethod", kPapyrusClass, PapyrusRemovePageRequestMethod);
    vm->RegisterFunction("ReloadView", kPapyrusClass, PapyrusReloadView);
    vm->RegisterFunction("CloseView", kPapyrusClass, PapyrusCloseView);
    vm->RegisterFunction("GetPageRequestPayload", kPapyrusClass, PapyrusGetPageRequestPayload);
    vm->RegisterFunction("CompletePageRequest", kPapyrusClass, PapyrusCompletePageRequest);
    vm->RegisterFunction("RejectPageRequest", kPapyrusClass, PapyrusRejectPageRequest);
    SKSE::log::info("SkyUltra registered its Papyrus view and page bridge functions.");
    return true;
}

bool IsPapyrusView(SkyUltraViewHandle view) noexcept
{
    const std::scoped_lock lock(g_papyrus_mutex);
    return g_view_tokens_by_native_handle.contains(view);
}

bool DispatchPapyrusPageRequest(
    SkyUltraViewHandle view,
    std::string_view request_id,
    std::string_view method,
    std::string_view params_json,
    bool cancelled)
{
    if (cancelled) {
        std::int32_t token{};
        std::string owned_method;
        {
            const std::scoped_lock lock(g_papyrus_mutex);
            const auto native_request = g_request_tokens_by_native_id.find(
                NativeRequestKey(view, request_id));
            if (native_request == g_request_tokens_by_native_id.end()) {
                return false;
            }
            token = native_request->second;
            if (const auto request = g_requests.find(token); request != g_requests.end()) {
                owned_method = request->second.method;
                g_requests.erase(request);
            }
            g_request_tokens_by_native_id.erase(native_request);
        }
        return SendModEvent(kCancelledEvent, owned_method, token);
    }

    std::int32_t token{};
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        if (!g_view_tokens_by_native_handle.contains(view) ||
            g_requests.size() >= kMaxPapyrusPendingRequests) {
            return false;
        }
        token = AllocateToken(g_next_request_token, g_requests);
        if (token == 0) {
            return false;
        }
        const auto key = NativeRequestKey(view, request_id);
        if (g_request_tokens_by_native_id.contains(key)) {
            return false;
        }
        g_requests.emplace(token, PapyrusRequest{
            view, std::string(request_id), std::string(method), std::string(params_json) });
        g_request_tokens_by_native_id.emplace(key, token);
    }

    if (SendModEvent(kRequestEvent, method, token)) {
        return true;
    }
    const std::scoped_lock lock(g_papyrus_mutex);
    const auto key = NativeRequestKey(view, request_id);
    g_request_tokens_by_native_id.erase(key);
    g_requests.erase(token);
    return false;
}

void ForgetPapyrusView(SkyUltraViewHandle view)
{
    std::vector<std::pair<std::int32_t, std::string>> cancelled;
    {
        const std::scoped_lock lock(g_papyrus_mutex);
        const auto token = g_view_tokens_by_native_handle.find(view);
        if (token == g_view_tokens_by_native_handle.end()) {
            return;
        }
        g_views.erase(token->second);
        g_view_tokens_by_native_handle.erase(token);
        for (auto request = g_requests.begin(); request != g_requests.end();) {
            if (request->second.native_view != view) {
                ++request;
                continue;
            }
            const auto request_token = request->first;
            const auto key = NativeRequestKey(view, request->second.native_request_id);
            g_request_tokens_by_native_id.erase(key);
            cancelled.emplace_back(request_token, request->second.method);
            request = g_requests.erase(request);
        }
    }
    for (const auto& [token, method] : cancelled) {
        (void)SendModEvent(kCancelledEvent, method, token);
    }
}

}  // namespace skyultra
