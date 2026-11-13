#include "SkyUltra/FrameworkServices.h"
#include <SKSE/SKSE.h>
#include <RE/A/ActorValues.h>
#include <RE/A/ActorEquipManager.h>
#include <RE/A/AlchemyItem.h>
#include <RE/B/BGSDefaultObjectManager.h>
#include <RE/B/BGSEquipSlot.h>
#include <RE/A/ActiveEffect.h>
#include <RE/B/BGSBaseAlias.h>
#include <RE/B/BarterMenu.h>
#include <RE/C/ContainerMenu.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSLightingShaderMaterialBase.h>
#include <RE/B/BGSLocation.h>
#include <RE/B/BSModelDB.h>
#include <RE/B/BSShaderProperty.h>
#include <RE/B/BGSTextureSet.h>
#include <RE/B/BSResourceNiBinaryStream.h>
#include <RE/C/Calendar.h>
#include <RE/E/Effect.h>
#include <RE/E/EffectSetting.h>
#include <RE/E/EnchantmentItem.h>
#include <RE/E/ExtraCharge.h>
#include <RE/E/ExtraDataList.h>
#include <RE/E/ExtraEnchantment.h>
#include <RE/E/ExtraMapMarker.h>
#include <RE/E/ExtraSoul.h>
#include <RE/E/ExtraTeleport.h>
#include <RE/E/ExtraWorn.h>
#include <RE/E/ExtraWornLeft.h>
#include <RE/F/FormTraits.h>
#include <RE/I/IngredientItem.h>
#include <RE/I/InventoryEntryData.h>
#include <RE/I/ItemRemoveReason.h>
#include <RE/M/MagicTarget.h>
#include <RE/M/MenuTopicManager.h>
#include <RE/M/Misc.h>
#include <RE/N/NativeFunction.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiSourceTexture.h>
#include <RE/P/PlayerCamera.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/P/ProcessLists.h>
#include <RE/R/Renderer.h>
#include <RE/S/ScrollItem.h>
#include <RE/S/SpellItem.h>
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
#include <RE/T/TESObjectSTAT.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/T/TESWorldSpace.h>
#include <RE/T/TES.h>
#include <RE/T/TESAmmo.h>
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

bool IsPlayableNamedInventoryItem(RE::TESForm* form, const RE::InventoryEntryData* entry_data = nullptr, bool allow_equipped = false)
{
    if (form == nullptr || !form->IsInventoryObject() || form->Is(RE::FormType::LeveledItem)) {
        return false;
    }
    if (form->GetFormID() == 0x0000000FU || form->IsGold()) {
        return true;
    }
    if (!allow_equipped) {
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
    }
    const char* raw_name = entry_data != nullptr ? const_cast<RE::InventoryEntryData*>(entry_data)->GetDisplayName() : nullptr;
    if (raw_name == nullptr || raw_name[0] == '\0' || IsMissingOrUnnamedDisplayName(raw_name)) {
        raw_name = form->GetName();
    }
    if (raw_name == nullptr || raw_name[0] == '\0' || IsMissingOrUnnamedDisplayName(raw_name)) {
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
    if (form == nullptr || form->Is(RE::FormType::LeveledItem)) {
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
    const std::string name = stripped_name;

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
    } else if (form->Is(RE::FormType::Ingredient)) {
        item["useAction"] = "eat";
    } else if (auto* book = form->As<RE::TESObjectBOOK>(); book != nullptr) {
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
    if (scan_inventory) {
        for (const auto& [form, inventory_entry] : actor->GetInventory()) {
            if (form == nullptr || inventory_entry.first <= 0) {
                continue;
            }
            auto* entry_data = inventory_entry.second.get();
            const bool worn = entry_data != nullptr && entry_data->IsWorn();
            auto* armor = form->As<RE::TESObjectARMO>();
            if (armor == nullptr || !worn) {
                continue;
            }
            const auto worn_item = PlayerItemJson(form, inventory_entry.first, true, entry_data, is_player);
            for (const auto& slot : ItemEquipSlots(armor)) {
                if (slot != "auto") {
                    equipment[slot.get<std::string>()] = worn_item;
                }
            }
        }
    }
    using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
    const auto fill_slot = [&](const char* key, Slot mask) {
        if (!equipment[key].is_null()) {
            return;
        }
        if (auto* armor = actor->GetWornArmor(mask)) {
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

RE::BSFixedString PapyrusGetNearbyNpcsJSON(
    RE::StaticFunctionTag*, RE::BSFixedString params)
{
    const auto request = ParsePageParams(params);
    auto requested_radius = 6000.0;
    if (const auto field = request.find("radius"); field != request.end() && field->is_number()) {
        requested_radius = field->get<double>();
    }
    if (!std::isfinite(requested_radius)) {
        requested_radius = 6000.0;
    }
    const auto radius = static_cast<float>(std::clamp(requested_radius, 500.0, 20000.0));
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* process_lists = RE::ProcessLists::GetSingleton();
    if (player == nullptr || process_lists == nullptr || !player->Is3DLoaded() ||
        player->GetParentCell() == nullptr || !player->GetParentCell()->IsAttached()) {
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

    const auto player_position = player->GetPosition();
    const auto radius_squared = radius * radius;
    Json actors = Json::array();
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

        const auto* actor_values = actor->AsActorValueOwner();
        const auto health = actor_values == nullptr ? 0.0F :
            std::max(actor_values->GetActorValue(RE::ActorValue::kHealth), 0.0F);
        const auto health_max = std::max(actor->GetActorValueMax(RE::ActorValue::kHealth), 0.0F);
        const auto armor_rating = actor_values == nullptr ? 0.0F :
            std::max(actor_values->GetActorValue(RE::ActorValue::kDamageResist), 0.0F);
        actors.push_back({
            { "referenceId", std::to_string(actor->GetFormID()) },
            { "baseFormId", std::to_string(base->GetFormID()) },
            { "name", base->GetName() },
            { "race", NpcRaceName(base) },
            { "sex", NpcSexName(base->GetSex()) },
            { "distance", std::sqrt(distance_squared) },
            { "level", actor->GetLevel() },
            { "follower", actor->IsPlayerTeammate() },
            { "dead", actor->IsDead() },
            { "health", health },
            { "healthMax", health_max },
            { "armorRating", armor_rating },
            { "equipment", ReadActorEquipment(actor, false) }
        });
        return RE::BSContainer::ForEachResult::kContinue;
    });
    std::sort(actors.begin(), actors.end(), [](const Json& left, const Json& right) {
        return left.value("distance", 0.0F) < right.value("distance", 0.0F);
    });
    constexpr std::size_t kMaxNearbyActors = 40;
    const auto total = actors.size();
    if (actors.size() > kMaxNearbyActors) {
        actors.erase(actors.begin() + kMaxNearbyActors, actors.end());
    }
    return RE::BSFixedString(Json{
        { "available", true }, { "radius", radius }, { "totalNearby", total },
        { "results", std::move(actors) }
    }.dump());
}

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
    const bool equippable_only=(wear=="equippable"||category=="equippable");
    std::vector<Json> items;std::unordered_set<std::string> categories;std::size_t total{};
    for(const auto& [form,entry]:player->GetInventory()){
        if(!form||entry.first<=0)continue;
        const bool equipped=(entry.second&&entry.second->IsWorn())||player->GetEquippedObject(false)==form||player->GetEquippedObject(true)==form||player->GetCurrentAmmo()==form;
        if(!IsPlayableNamedInventoryItem(form,entry.second.get(),equipped))continue;
        if(equippable_only&&(ItemEquipSlots(form).empty()||form->Is(RE::FormType::Scroll)))continue;
        ++total;
        const auto kind=std::string(ItemCategory(form->GetFormType()));categories.insert(kind);
        if(category!="all"&&category!="equippable"&&category!=kind)continue;
        if(wear=="equipped"&&!equipped||wear=="carried"&&equipped)continue;
        char hex[11]{};std::snprintf(hex,sizeof(hex),"0x%08X",form->GetFormID());
        AppendPlayerInventoryItems(form,entry.first,equipped,entry.second.get(),[&](Json&& item){
            if(!query.empty()&&LowerAscii(item["name"].get<std::string>()+" "+kind+" "+item["id"].get<std::string>()+" "+hex+(item.value("stolen",false)?" stolen":"")).find(query)==std::string::npos)return;
            items.push_back(std::move(item));
        });
    }
    std::sort(items.begin(),items.end(),[&](const auto& a,const auto& b){
        if(sort=="value"||sort=="weight"||sort=="count")if(a[sort]!=b[sort])return a[sort].template get<double>()>b[sort].template get<double>();
        const auto an=LowerAscii(a["name"].template get<std::string>()),bn=LowerAscii(b["name"].template get<std::string>());
        return an!=bn?an<bn:a["id"].template get<std::string>()<b["id"].template get<std::string>();
    });
    Json results=Json::array();
    for(auto i=static_cast<std::size_t>(offset);i<items.size()&&results.size()<static_cast<std::size_t>(limit);++i){
        results.push_back(std::move(items[i]));
    }
    std::vector<std::string> sorted_categories(categories.begin(),categories.end());std::sort(sorted_categories.begin(),sorted_categories.end());
    return {{"available",true},{"results",std::move(results)},{"totalTypes",total},{"totalMatches",items.size()},
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
                    (void)manager->UnequipObject(player,form,pextra,1,nullptr,false,false,true,true,nullptr);
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
    if(weapon&&!both_hands){
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
    // This function runs on the bridge's game task. Apply equip/unequip immediately
    // on the game thread so equipment slots and inventory state update right away.
    const bool queue_equip = false;
    const bool apply_now = true;
    if(unequip){
        const bool unequipped = manager->UnequipObject(target,form,extra,count,hand,queue_equip,false,target==player,apply_now);
        if(extra!=nullptr && (!unequipped || (entry && entry->IsWorn()))){
            (void)manager->UnequipObject(target,form,nullptr,count,hand,queue_equip,false,target==player,apply_now);
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
    }
    else manager->EquipObject(target,form,extra,count,hand,queue_equip,false,target==player,apply_now);
    if((RE::UI::GetSingleton() != nullptr && RE::UI::GetSingleton()->GameIsPaused()) || is_mannequin)target->Update3DModel();
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
        if (game_paused) {
            player->Update3DModel();
        }
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
                manager->EquipObject(player, alch, extra, 1, nullptr, false, false, true, true);
                ConsumePlayerStolenCount(form, entry, carried_count, 1, std::max(0, carried_count - 1), from_stolen_stack || clean_count == 0);
                return finish_ok("Applied " + item_name + ".");
            }
            if (!player->DrinkPotion(alch, extra)) {
                manager->EquipObject(player, alch, extra, 1, nullptr, false, false, true, true);
            }
            ConsumePlayerStolenCount(form, entry, carried_count, 1, std::max(0, carried_count - 1), from_stolen_stack || clean_count == 0);
            return finish_ok((action == "eat" ? "Ate " : "Drank ") + item_name + ".");
        }
        if (auto* ingr = form->As<RE::IngredientItem>(); ingr != nullptr) {
            ingr->LearnEffect(0U);
            manager->EquipObject(player, ingr, extra, 1, nullptr, false, false, true, true);
            ConsumePlayerStolenCount(form, entry, carried_count, 1, std::max(0, carried_count - 1), from_stolen_stack || clean_count == 0);
            return finish_ok("Ate " + item_name + ".");
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
        const bool queue_equip = !game_paused && !scripted_use_item;
        const bool apply_now = game_paused || scripted_use_item;
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
        skyultra::InvalidateSharedGameDataCache();
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
    skyultra::InvalidateSharedGameDataCache();
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
        skyultra::InvalidateSharedGameDataCache();
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
        skyultra::InvalidateSharedGameDataCache();
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
    skyultra::InvalidateSharedGameDataCache();
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
        skyultra::InvalidateSharedGameDataCache();
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
                (void)eq_mgr->UnequipObject(player, form, extra, 1, nullptr, false, false, true, true, nullptr);
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
            if (sort == "value" || sort == "weight" || sort == "count") {
                const double av = a.value(sort, 0.0);
                const double bv = b.value(sort, 0.0);
                if (av != bv) return av > bv;
            }
            const auto an = LowerAscii(a.value("name", std::string{}));
            const auto bn = LowerAscii(b.value("name", std::string{}));
            return an != bn ? an < bn : a.value("id", std::string{}) < b.value("id", std::string{});
        });
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
    }

    return {
        { "available", true },
        { "isMannequin", is_mannequin },
        { "containerRefId", std::to_string(container->GetFormID()) },
        { "containerBaseFormId", std::to_string(base != nullptr ? base->GetFormID() : container->GetFormID()) },
        { "containerName", container_name },
        { "containerType", container_type },
        { "containerMode", ContainerModeLabel(ActiveContainerMode()) },
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
                    (void)eq_mgr->UnequipObject(container_actor, form, nullptr, entry.first, nullptr, false, false, true, true, nullptr);
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
                        ++moved_types;
                        moved_total += actor_gold;
                    }
                }
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
                    (void)eq_mgr->UnequipObject(container_actor, form, nullptr, tx_count, nullptr, false, false, true, true, nullptr);
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
                (void)eq_mgr->UnequipObject(player, form, nullptr, tx_count, nullptr, false, false, true, true, nullptr);
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
    return std::round(deg * 100.0F) * 0.01F;
}

float ReadCompassHeadingDeg(RE::PlayerCharacter* player) noexcept
{
    if (auto* cam = RE::PlayerCamera::GetSingleton(); cam != nullptr) {
        if (cam->cameraRoot != nullptr) {
            const auto& r = cam->cameraRoot->world.rotate.entry;
            const float fx = r[0][1];
            const float fy = r[1][1];
            if (std::isfinite(fx) && std::isfinite(fy) && (std::abs(fx) > 1e-4F || std::abs(fy) > 1e-4F)) {
                return std::round(NormalizeCompassAngleDeg(std::atan2(fx, fy) * 57.295779513F) * 100.0F) * 0.01F;
            }
        }
        const float cam_yaw = cam->GetRuntimeData2().yaw;
        if (std::isfinite(cam_yaw) && (std::abs(cam_yaw) > 1e-5F || player == nullptr || std::abs(player->GetAngleZ()) < 1e-4F)) {
            return std::round(NormalizeCompassAngleDeg(cam_yaw * 57.295779513F) * 100.0F) * 0.01F;
        }
    }
    if (player != nullptr) {
        const float angle = player->GetAngleZ();
        if (std::isfinite(angle)) {
            return std::round(NormalizeCompassAngleDeg(angle * 57.295779513F) * 100.0F) * 0.01F;
        }
    }
    return 0.0F;
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

    if (actor->IsInCombat()) {
        return true;
    }

    // Territorial wildlife (Mudcrab, Mammoth, Horker, Netch) that is not in combat should only show if very close (< 14m)
    if (lower_name.find("mudcrab") != std::string::npos ||
        lower_name.find("mammoth") != std::string::npos ||
        lower_name.find("horker") != std::string::npos ||
        lower_name.find("netch") != std::string::npos) {
        return dist_units <= (14.0F * 70.0F);
    }

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
    RE::NiPoint3 position{};
    bool discovered{ true };
    int priority{ 70 };
};

struct CompassStaticMarkerCache {
    bool valid{ false };
    std::uint64_t timestamp_ms{ 0 };
    std::uint32_t cell_form_id{ 0 };
    std::uint32_t world_form_id{ 0 };
    RE::NiPoint3 scan_origin{};
    std::vector<CachedWorldCompassMarker> markers;
};
CompassStaticMarkerCache g_compass_static_cache;

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

    // 50 meters = 3500 Skyrim units (70 units per meter). We scan slightly wider (4200 units = 60m) for cache hysteresis,
    // and strictly filter local POIs/NPCs to 50m (3500 units) when emitting live markers.
    constexpr float kLocalRadiusMeters = 50.0F;
    constexpr float kLocalRadiusUnits = kLocalRadiusMeters * 70.0F; // 3500.0F
    constexpr float kScanCacheRadiusUnits = 4200.0F;

    const float origin_dx = player_pos.x - g_compass_static_cache.scan_origin.x;
    const float origin_dy = player_pos.y - g_compass_static_cache.scan_origin.y;
    const float origin_MovedSq = origin_dx * origin_dx + origin_dy * origin_dy;

    if (!g_compass_static_cache.valid ||
        g_compass_static_cache.world_form_id != world_id ||
        (is_interior && g_compass_static_cache.cell_form_id != cell_id) ||
        (now_ms - g_compass_static_cache.timestamp_ms) >= 900 ||
        origin_MovedSq > (450.0F * 450.0F)) {
        std::vector<CachedWorldCompassMarker> scanned;
        std::unordered_set<std::uint32_t> seen_refs;
        std::unordered_set<std::string> seen_door_destinations;

        const auto consider_map_marker_ref = [&](RE::TESObjectREFR* ref) {
            if (ref == nullptr || ref->IsDisabled() || ref->IsDeleted()) {
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
            if (!std::isfinite(dist_units) || dist_units > kScanCacheRadiusUnits) {
                return;
            }
            const auto classified = ClassifyMapMarkerType(marker_type, clean_name);
            const bool discovered = extra_marker->mapData->flags.any(RE::MapMarkerData::Flag::kCanTravelTo);
            seen_door_destinations.insert(LowerAsciiString(clean_name));
            scanned.push_back(CachedWorldCompassMarker{
                "map:" + std::to_string(ref->GetFormID()),
                clean_name,
                classified.kind,
                discovered ? std::string(classified.category_label) : (std::string("Undiscovered ") + classified.category_label),
                pos,
                discovered,
                classified.priority
            });
        };

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

        // Also check worldspace persistent cell for map markers within 60m so markers across cell boundaries never flicker.
        if (worldspace != nullptr && worldspace->persistentCell != nullptr) {
            std::vector<RE::NiPointer<RE::TESObjectREFR>> persistent_markers;
            worldspace->persistentCell->ForEachReferenceInRange(player_pos, kScanCacheRadiusUnits, [&](RE::TESObjectREFR* ref) {
                if (ref != nullptr && !ref->IsDisabled() && !ref->IsDeleted()) {
                    auto* base = ref->GetBaseObject();
                    if ((base != nullptr && base->GetFormID() == 0x00000010U) ||
                        ref->extraList.HasType(RE::ExtraDataType::kMapMarker)) {
                        persistent_markers.emplace_back(ref);
                    }
                }
                return persistent_markers.size() >= 64
                    ? RE::BSContainer::ForEachResult::kStop
                    : RE::BSContainer::ForEachResult::kContinue;
            });
            for (const auto& marker_ref : persistent_markers) {
                consider_map_marker_ref(marker_ref.get());
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
            RE::TES::GetSingleton()->ForEachReferenceInRange(player, kScanCacheRadiusUnits, collect_candidate);
        } else if (parent_cell->IsAttached()) {
            parent_cell->ForEachReferenceInRange(player_pos, kScanCacheRadiusUnits, collect_candidate);
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
            if (!std::isfinite(dist_units) || dist_units > kScanCacheRadiusUnits) {
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
                pos,
                true,
                classified.priority
            });
        }

        g_compass_static_cache.markers = std::move(scanned);
        g_compass_static_cache.cell_form_id = cell_id;
        g_compass_static_cache.world_form_id = world_id;
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
    static std::uint64_t s_dynamic_cache_ms = 0;
    static std::uint32_t s_dynamic_cache_cell = 0;
    static std::uint32_t s_dynamic_cache_world = 0;
    static std::vector<CachedDynamicCompassEntry> s_dynamic_entries;

    if (s_dynamic_cache_cell != cell_id || s_dynamic_cache_world != world_id || (now_ms - s_dynamic_cache_ms) >= 180) {
        s_dynamic_entries.clear();

        const bool allow_cross_cell_tracking =
            (s_space_transition_ms == 0 || (now_ms >= s_space_transition_ms && (now_ms - s_space_transition_ms) >= 650)) &&
            (REL::Module::IsVR() || !player->GetPlayerRuntimeData().playerFlags.forceQuestTargetRepath);

        std::unordered_set<std::uint32_t> seen_follower_actors;

        // 0. Player-Placed Custom Map Marker ("a marker they put" — always shown regardless of distance)
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
                RE::NiPoint3 marker_pos = custom_marker_ref->GetPosition();
                std::string marker_subtitle = "Placed on World Map";
                if (rt.playerMarkerPath != nullptr && !rt.playerMarkerPath->teleportRefs.empty()) {
                    if (auto* step_ref = rt.playerMarkerPath->teleportRefs[0].ref;
                        step_ref != nullptr && !step_ref->IsDisabled() && !step_ref->IsDeleted()) {
                        marker_pos = step_ref->GetPosition();
                        marker_subtitle = "Route to Custom Waypoint";
                    }
                } else if (is_interior && custom_marker_ref->GetParentCell() != parent_cell &&
                           (rt.exteriorPosition.x != 0.0F || rt.exteriorPosition.y != 0.0F)) {
                    marker_pos = rt.exteriorPosition;
                    marker_subtitle = "Exit to Custom Waypoint";
                }
                if (std::abs(marker_pos.x) > 1.0F || std::abs(marker_pos.y) > 1.0F) {
                    s_dynamic_entries.push_back(CachedDynamicCompassEntry{
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
                        100.0F
                    });
                }
            }
        }

        // 1. Active Quest Objective Targets (all active quests checked; shown regardless of distance)
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

                        // Resolve final target ref (for alias / follower / actor resolution and fallback)
                        RE::ObjectRefHandle final_handle{};
                        quest_target->GetTargetRef(final_handle, false, quest);
                        auto final_ref = final_handle.get();
                        if (final_ref == nullptr) {
                            final_handle = quest->GetAliasedRef(quest_target->alias);
                            final_ref = final_handle.get();
                        }

                        // Resolve tracking ref (either direct target or next load door in teleportPath)
                        RE::NiPointer<RE::TESObjectREFR> target_ref;
                        if (allow_cross_cell_tracking) {
                            RE::ObjectRefHandle tracking_handle{};
                            if (quest_target->GetTrackingRef(tracking_handle, quest)) {
                                target_ref = tracking_handle.get();
                            }
                        }
                        if (target_ref == nullptr && !quest_target->teleportPath.teleportRefs.empty()) {
                            if (auto* tp_ref = quest_target->teleportPath.teleportRefs[0].ref; tp_ref != nullptr) {
                                target_ref = RE::NiPointer<RE::TESObjectREFR>(tp_ref);
                            }
                        }
                        if (target_ref == nullptr) {
                            target_ref = final_ref;
                        }
                        if (target_ref == nullptr || target_ref->IsDisabled() || target_ref->IsDeleted()) {
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
                            if (!seen_follower_actors.insert(follower_actor->GetFormID()).second) {
                                continue;
                            }
                            const char* raw_fname = follower_actor->GetDisplayFullName();
                            const std::string follower_name = (raw_fname != nullptr && raw_fname[0] != '\0')
                                ? StripLeadingDollar(raw_fname)
                                : resolved_label;
                            auto* actor_base = follower_actor->GetActorBase();
                            s_dynamic_entries.push_back(CachedDynamicCompassEntry{
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

                        RE::NiPoint3 target_pos = target_ref->GetPosition();
                        // If target is in an exterior worldspace while player is also in that worldspace, use its world position.
                        // If player is in an interior and target_ref is in exterior without a resolved door, fall back to player's exteriorPosition door direction if available.
                        if (is_interior && target_ref->GetParentCell() != parent_cell &&
                            quest_target->teleportPath.teleportRefs.empty() && !REL::Module::IsVR()) {
                            const auto& rt = player->GetPlayerRuntimeData();
                            if (rt.exteriorPosition.x != 0.0F || rt.exteriorPosition.y != 0.0F) {
                                target_pos = rt.exteriorPosition;
                            }
                        }

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
                        s_dynamic_entries.push_back(CachedDynamicCompassEntry{
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
                            (is_interior && target_ref->GetParentCell() != parent_cell) ? RE::ObjectRefHandle{} : target_ref->CreateRefHandle()
                        });
                    }
                }
            }
        }

        // 2. Hostile Actors in 50m Radius + Waiting Player Followers
        if (auto* process_lists = RE::ProcessLists::GetSingleton(); process_lists != nullptr) {
            std::size_t enemy_markers = 0;
            process_lists->ForAllActors([&](RE::Actor* actor) {
                if (actor == nullptr || actor == player ||
                    actor->IsDisabled() || actor->IsDeleted() || actor->IsDead() ||
                    !actor->Is3DLoaded()) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                // Check for waiting player teammate/follower so their real name always appears when waiting
                if (actor->IsPlayerTeammate() && !actor->IsHostileToActor(player)) {
                    float waiting_val = 0.0F;
                    if (auto* av_owner = actor->AsActorValueOwner(); av_owner != nullptr) {
                        waiting_val = av_owner->GetActorValue(RE::ActorValue::kWaitingForPlayer);
                    }
                    if (waiting_val > 0.0F && seen_follower_actors.insert(actor->GetFormID()).second) {
                        const auto* raw_fname = actor->GetDisplayFullName();
                        const std::string follower_name = (raw_fname != nullptr && raw_fname[0] != '\0')
                            ? StripLeadingDollar(raw_fname)
                            : std::string("Follower");
                        auto* actor_base = actor->GetActorBase();
                        s_dynamic_entries.push_back(CachedDynamicCompassEntry{
                            "follower:" + std::to_string(actor->GetFormID()),
                            follower_name + " (Waiting)",
                            "Follower Waiting",
                            "follower",
                            "Follower Waiting",
                            actor->GetPosition(),
                            false,
                            105,
                            true,
                            std::to_string(actor->GetFormID()),
                            actor_base != nullptr ? std::to_string(actor_base->GetFormID()) : std::string{},
                            static_cast<int>(actor->GetLevel()),
                            ReadActorHealthPercent(actor),
                            actor->CreateRefHandle()
                        });
                    }
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                if (enemy_markers >= 10) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                const auto pos = actor->GetPosition();
                const float dx = pos.x - player_pos.x;
                const float dy = pos.y - player_pos.y;
                const float dz = pos.z - player_pos.z;
                const float dist_units = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (!std::isfinite(dist_units) || dist_units > kScanCacheRadiusUnits) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                if (!IsGenuineHostileThreat(actor, player, dist_units)) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                const auto* raw_actor_name = actor->GetDisplayFullName();
                const std::string actor_name = (raw_actor_name != nullptr && raw_actor_name[0] != '\0')
                    ? StripLeadingDollar(raw_actor_name)
                    : std::string("Hostile Enemy");
                auto* actor_base = actor->GetActorBase();
                ++enemy_markers;
                s_dynamic_entries.push_back(CachedDynamicCompassEntry{
                    "enemy:" + std::to_string(actor->GetFormID()),
                    actor_name,
                    "Lv. " + std::to_string(std::max<int>(1, actor->GetLevel())) + " Hostile",
                    "enemy",
                    "Hostile Target",
                    pos,
                    true,
                    98,
                    false,
                    std::to_string(actor->GetFormID()),
                    actor_base != nullptr ? std::to_string(actor_base->GetFormID()) : std::string{},
                    static_cast<int>(actor->GetLevel()),
                    ReadActorHealthPercent(actor),
                    actor->CreateRefHandle()
                });
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }

        s_dynamic_cache_cell = cell_id;
        s_dynamic_cache_world = world_id;
        s_dynamic_cache_ms = now_ms;
    }

    for (const auto& dyn : s_dynamic_entries) {
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
        // Enforce 50m radius for non-unlimited markers (such as hostile enemies)
        if (!dyn.unlimited_distance && (dist_units > kLocalRadiusUnits || dist_m > kLocalRadiusMeters)) {
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

    // 3. Cached Nearby Map Markers & City Building Doors strictly within 50m radius
    for (const auto& cached : g_compass_static_cache.markers) {
        const float dx = cached.position.x - player_pos.x;
        const float dy = cached.position.y - player_pos.y;
        const float dz = cached.position.z - player_pos.z;
        const float dist_units = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!std::isfinite(dist_units) || dist_units > kLocalRadiusUnits) {
            continue;
        }
        const float dist_m = std::max(0.5F, std::round((dist_units / 70.0F) * 10.0F) * 0.1F);
        if (dist_m > kLocalRadiusMeters) {
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
    return out;
}

struct SharedSnapshotCacheEntry {
    bool valid{ false };
    std::uint64_t timestamp_ms{ 0 };
    std::uint64_t sequence{ 0 };
    Json snapshot;
    std::string serialized;
    std::unordered_map<std::string, Json> topic_slices;
    std::unordered_map<std::string, std::string> topic_serialized;
};

constexpr std::uint64_t kSharedSnapshotCacheTtlMs = 450;
constexpr std::uint64_t kSharedHudSnapshotCacheTtlMs = 8;
constexpr std::uint64_t kHeavySnapshotSlicesCacheTtlMs = 8000;
constexpr std::array<std::string_view, 6> kCanonicalGameTopics{
    "vitals", "equipment", "inventory", "effects", "quests", "followers"
};

SharedSnapshotCacheEntry g_shared_full_cache;
SharedSnapshotCacheEntry g_shared_hud_cache;
std::uint64_t g_shared_snapshot_sequence{ 0 };

struct HeavySnapshotSlicesCache {
    bool valid{ false };
    bool has_full_inventory{ false };
    std::uint64_t timestamp_ms{ 0 };
    Json equipment;
    std::vector<Json> inventory_items;
    std::size_t inventory_type_count{ 0 };
    std::int64_t total_item_count{ 0 };
    Json quests;
    std::size_t quest_count{ 0 };
    Json followers;
    std::size_t follower_count{ 0 };
    std::uint64_t effects_timestamp_ms{ 0 };
    Json active_effects{ Json::array() };
    std::size_t active_effect_count{ 0 };
    float inventory_weight{ 0.0F };
};
HeavySnapshotSlicesCache g_heavy_snapshot_slices;

Json BuildPlayerSnapshotJson(bool compact)
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

    const auto now_ms = SharedSnapshotNowMs();
    const bool need_heavy_refresh = !g_heavy_snapshot_slices.valid ||
        (!compact && !g_heavy_snapshot_slices.has_full_inventory) ||
        (now_ms - g_heavy_snapshot_slices.timestamp_ms) >= kHeavySnapshotSlicesCacheTtlMs;
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
        }

        auto [quests, quest_count] = ReadActiveQuestsJson();
        auto [followers, follower_count] = ReadFollowersJson(player);

        g_heavy_snapshot_slices.equipment = std::move(equipment);
        g_heavy_snapshot_slices.quests = std::move(quests);
        g_heavy_snapshot_slices.quest_count = quest_count;
        g_heavy_snapshot_slices.followers = std::move(followers);
        g_heavy_snapshot_slices.follower_count = follower_count;
        g_heavy_snapshot_slices.timestamp_ms = now_ms;
        g_heavy_snapshot_slices.valid = true;
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
    if (!compact || g_heavy_snapshot_slices.effects_timestamp_ms == 0 ||
        (now_ms - g_heavy_snapshot_slices.effects_timestamp_ms) >= 1200) {
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
            active_effects.push_back({
                {"id", effect_id},
                {"name", effect_name},
                {"magnitude", std::isfinite(active_effect->magnitude) ? active_effect->magnitude : 0.0F},
                {"duration", duration}, {"remaining", std::max(0.0F, duration - elapsed)},
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
        g_heavy_snapshot_slices.inventory_weight = player->GetWeightInContainer();
        g_heavy_snapshot_slices.effects_timestamp_ms = now_ms;
    }
    Json active_effects = g_heavy_snapshot_slices.active_effects;
    const auto active_effect_count = g_heavy_snapshot_slices.active_effect_count;
    const auto inventory_weight = g_heavy_snapshot_slices.inventory_weight;
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
        { "level", player->GetLevel() },
        { "heading", heading_deg },
        { "compassMarkers", std::move(compass_markers) },
        { "calendar", std::move(calendar_data) },
        { "health", { { "current", value(RE::ActorValue::kHealth) }, { "max", maximum(RE::ActorValue::kHealth) } } },
        { "stamina", { { "current", value(RE::ActorValue::kStamina) }, { "max", maximum(RE::ActorValue::kStamina) } } },
        { "magicka", { { "current", value(RE::ActorValue::kMagicka) }, { "max", maximum(RE::ActorValue::kMagicka) } } },
        { "armorRating", value(RE::ActorValue::kDamageResist) },
        { "gold", std::max(player->GetGoldAmount(), 0) },
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

void PopulateSnapshotTopicSlices(SharedSnapshotCacheEntry& entry)
{
    entry.topic_slices.clear();
    entry.topic_serialized.clear();
    if (!entry.snapshot.value("available", false)) {
        return;
    }

    const auto& s = entry.snapshot;
    entry.topic_slices.emplace("vitals", Json{
        { "playerName", s.value("playerName", std::string("Dragonborn")) },
        { "level", s.value("level", 1) },
        { "heading", s.value("heading", 0.0F) },
        { "compassMarkers", s.contains("compassMarkers") ? s["compassMarkers"] : Json::array() },
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
    });
    entry.topic_slices.emplace("equipment", Json{
        { "equipment", s["equipment"] }
    });
    entry.topic_slices.emplace("inventory", Json{
        { "inventory", s["inventory"] },
        { "inventoryTypeCount", s.value("inventoryTypeCount", std::size_t{ 0 }) },
        { "inventoryItemCount", s.value("inventoryItemCount", std::int64_t{ 0 }) },
        { "inventoryTruncated", s.value("inventoryTruncated", false) }
    });
    entry.topic_slices.emplace("effects", Json{
        { "activeEffects", s["activeEffects"] },
        { "activeEffectCount", s.value("activeEffectCount", std::size_t{ 0 }) }
    });
    entry.topic_slices.emplace("quests", Json{
        { "quests", s["quests"] },
        { "questCount", s.value("questCount", std::size_t{ 0 }) }
    });
    entry.topic_slices.emplace("followers", Json{
        { "followers", s["followers"] },
        { "followerCount", s.value("followerCount", std::size_t{ 0 }) }
    });

    for (const auto& [topic, slice] : entry.topic_slices) {
        entry.topic_serialized.emplace(topic, slice.dump());
    }
}

const SharedSnapshotCacheEntry& GetOrCollectSharedSnapshot(bool compact)
{
    auto& target = compact ? g_shared_hud_cache : g_shared_full_cache;
    const auto now_ms = SharedSnapshotNowMs();
    const auto ttl_ms = compact ? kSharedHudSnapshotCacheTtlMs : kSharedSnapshotCacheTtlMs;
    if (target.valid && (now_ms - target.timestamp_ms) < ttl_ms) {
        return target;
    }

    target.snapshot = BuildPlayerSnapshotJson(compact);
    target.timestamp_ms = now_ms;
    target.sequence = ++g_shared_snapshot_sequence;
    target.snapshot["sequence"] = target.sequence;
    target.serialized = target.snapshot.dump();
    PopulateSnapshotTopicSlices(target);
    target.valid = true;
    return target;
}

RE::BSFixedString ReadPlayerSnapshotJSON(bool compact = false)
{
    const auto& cached = GetOrCollectSharedSnapshot(compact);
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
    (void)ResolveActiveContainerRef();
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
    g_shared_full_cache.valid = false;
    g_shared_hud_cache.valid = false;
    g_heavy_snapshot_slices.valid = false;
}

std::string CollectSharedGameDataUpdateJSON(
    bool compact,
    const std::vector<std::string>& subscribed_topics,
    std::unordered_map<std::string, std::string>& in_out_topic_signatures,
    std::uint64_t& out_sequence,
    bool force_full)
{
    const auto& cached = GetOrCollectSharedSnapshot(compact);
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
        if (slice_it == cached.topic_slices.end() || serialized_it == cached.topic_serialized.end()) {
            continue;
        }
        const auto prev_it = in_out_topic_signatures.find(topic);
        if (!force_full && prev_it != in_out_topic_signatures.end() &&
            prev_it->second == serialized_it->second) {
            continue;
        }
        in_out_topic_signatures[topic] = serialized_it->second;
        changed_topics.push_back(topic);
        for (const auto& [field_key, field_val] : slice_it->second.items()) {
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
    if(method=="game.getInventory")return ReadInventoryPage(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.equipItem"||method=="game.unequipItem")return ChangePlayerEquipment(Json::parse(params_json,nullptr,false),method=="game.unequipItem").dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.itemAction")return PerformInventoryItemAction(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.getBarterSnapshot")return ReadBarterSnapshot(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.barterTransaction")return PerformBarterTransaction(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.getContainerSnapshot")return ReadContainerSnapshot(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
    if(method=="game.containerTransfer")return PerformContainerTransfer(Json::parse(params_json,nullptr,false)).dump(-1,' ',false,Json::error_handler_t::replace);
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
