#pragma once
#include "SkyUltra/NativeAPI.h"
#include <string>
#include <string_view>
namespace skyultra {
// Papyrus bridge. Mutations are queued on Skyrim's game thread; status is a copied DTO.
[[nodiscard]] bool QueueActorRendererRequest(SkyUltraViewHandle view,std::string method,std::string params_json);
[[nodiscard]] std::string ReadActorRendererStatus(SkyUltraViewHandle view);
// Closes the SkyUltra InventoryMenu replacement on the next game-thread task, mirroring how
// scripted inventory items (storage gems, backpacks) close Skyrim's own InventoryMenu.
void RequestCloseInventoryReplacement(std::string_view reason);
}
