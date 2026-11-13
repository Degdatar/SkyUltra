#include "SkyUltra/UltralightRuntimeLoader.h"

#include <Windows.h>
#include <delayimp.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace skyultra {
namespace {

struct RuntimeModules {
    std::mutex mutex;
    HMODULE core{};
    HMODULE web{};
    HMODULE ultralight{};
};

RuntimeModules& Modules()
{
    static RuntimeModules modules;
    return modules;
}

std::filesystem::path PluginDirectory()
{
    constexpr std::size_t kPathCapacity = 32768;
    std::array<wchar_t, kPathCapacity> module_path{};
    const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
    const auto length = GetModuleFileNameW(module, module_path.data(),
        static_cast<DWORD>(module_path.size()));
    if (length == 0 || length >= module_path.size()) {
        return {};
    }
    return std::filesystem::path(module_path.data()).parent_path();
}

std::filesystem::path RuntimeDirectory()
{
    return PluginDirectory() / L"SkyUltra" / L"Ultralight";
}

HMODULE LoadRuntimeFile(const std::filesystem::path& directory, const wchar_t* filename,
    std::uint32_t* error_code)
{
    const auto path = directory / filename;
    const auto module = LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (module == nullptr && error_code != nullptr) {
        *error_code = GetLastError();
    }
    return module;
}

}  // namespace

FARPROC WINAPI DelayLoadNotify(unsigned notification, PDelayLoadInfo info) noexcept
{
    if (notification != dliNotePreLoadLibrary || info == nullptr || info->szDll == nullptr) {
        return nullptr;
    }

    if (std::strcmp(info->szDll, "SUL.dll") != 0 &&
        std::strcmp(info->szDll, "SULCore.dll") != 0) {
        return nullptr;
    }

    std::uint32_t error_code{};
    if (!LoadPrivateUltralightRuntime(&error_code)) {
        SetLastError(error_code);
        return nullptr;
    }

    auto& modules = Modules();
    const std::scoped_lock lock(modules.mutex);
    const auto module = std::strcmp(info->szDll, "SUL.dll") == 0 ?
        modules.ultralight : modules.core;
    return reinterpret_cast<FARPROC>(module);
}

bool LoadPrivateUltralightRuntime(std::uint32_t* error_code) noexcept
{
    if (error_code != nullptr) {
        *error_code = ERROR_SUCCESS;
    }

    try {
        auto& modules = Modules();
        const std::scoped_lock lock(modules.mutex);
        const auto directory = RuntimeDirectory();
        if (directory.empty()) {
            if (error_code != nullptr) {
                *error_code = GetLastError() == ERROR_SUCCESS ? ERROR_PATH_NOT_FOUND : GetLastError();
            }
            return false;
        }

        if (modules.core == nullptr) {
            modules.core = LoadRuntimeFile(directory, L"SULCore.dll", error_code);
        }
        if (modules.core == nullptr) {
            return false;
        }

        if (modules.web == nullptr) {
            modules.web = LoadRuntimeFile(directory, L"SULWeb.dll", error_code);
        }
        if (modules.web == nullptr) {
            return false;
        }

        if (modules.ultralight == nullptr) {
            modules.ultralight = LoadRuntimeFile(directory, L"SUL.dll", error_code);
        }
        return modules.ultralight != nullptr;
    } catch (...) {
        if (error_code != nullptr) {
            *error_code = ERROR_NOT_ENOUGH_MEMORY;
        }
        return false;
    }
}

}  // namespace skyultra

extern "C" {
const PfnDliHook __pfnDliNotifyHook2 = &skyultra::DelayLoadNotify;
}
