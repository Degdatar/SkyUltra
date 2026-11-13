# Credits and third-party libraries

Thank you to every maintainer, author and contributor whose work makes SkyUltra possible. Special thanks to **CEF/Chromium**, **Ultralight/WebKit**, **SKSE**, and **CommonLibSSE/CommonLibSSE-NG** for the browser and Skyrim foundations.

This inventory describes the checked-in CMake/vcpkg configuration, frontend package manifests and npm lockfiles. It distinguishes runtime libraries, SDK/UI dependencies and authoring tools. These are project dependencies, **not a list of extra mods players must install**. SkyUI, ENB and shader mods are not required to run SkyUltra.

## Browser and Skyrim foundations

| Project | Contribution / source |
| --- | --- |
| [Chromium Embedded Framework (CEF)](https://github.com/chromiumembedded/cef) and [Chromium](https://www.chromium.org/) | Embedded Chromium browser, multiprocess host and GPU/shared-texture integration. CEF's bundled copyright/license is retained under [licenses/cef](licenses/cef/). Chromium also includes its own third-party components/notices. |
| [Ultralight](https://ultralig.ht/) and [WebKit](https://webkit.org/) | Embedded web engine, renderer, JavaScript and web-platform support. See the retained [Ultralight license/EULA/notices](licenses/ultralight/) and [Ultralight WebCore](https://github.com/ultralight-ux/WebCore). |
| [Ultralight Platform](https://github.com/ultralight-ux/Ultralight) | Reference GPU driver/context, filesystem, fonts, audio and platform integration. Its notice is retained under [licenses/ultralight-platform](licenses/ultralight-platform/). |
| [SKSE team](https://skse.silverlock.org/) | Skyrim Script Extender plugin loading, messaging, game task integration and Papyrus extension support. |
| [CommonLibSSE](https://github.com/Ryan-rsm-McKenzie/CommonLibSSE), [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG) and contributors | C++ game types, runtime/address abstractions and SKSE integration. The selected local fork's manifest and its retained notices identify the exact source/license terms for that checkout. |
| [Address Library maintainers](https://www.nexusmods.com/skyrimspecialedition/mods/32444) and [VR Address Library maintainers](https://www.nexusmods.com/skyrimspecialedition/mods/58101) | Version-specific address data used by the native integration. |
| [Valve OpenVR](https://github.com/ValveSoftware/openvr) | The optional VR rendering/input path. |
| [Microsoft DirectXTK](https://github.com/microsoft/DirectXTK) and [DirectXMath](https://github.com/microsoft/DirectXMath) | Sprite composition, texture helpers and matrix/vector math. |

## Native libraries and platform code

| Library | Role |
| --- | --- |
| [{fmt}](https://github.com/fmtlib/fmt) | Formatting, including the logging stack |
| [spdlog](https://github.com/gabime/spdlog) | Native logging |
| [nlohmann/json](https://github.com/nlohmann/json) | Manifests, bridge payloads, settings and data serialization |
| [rapidcsv](https://github.com/d99kris/rapidcsv) | CSV support in the native dependency stack, including VR address-library paths |
| [SimpleIni](https://github.com/brofield/simpleini) | INI configuration support |
| [toml11](https://github.com/ToruNiina/toml11) | CommonLib configuration support |
| [Xbyak](https://github.com/herumi/xbyak) | Native code-generation/hook utilities in the CommonLib stack |
| HDE64 / Vyacheslav Patkov | Instruction decoding for CommonLib hook safety; [retained notice](licenses/commonlib/LICENSE-hde64.txt) |
| [D3D12 Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator) and [DirectX-Headers](https://github.com/microsoft/DirectX-Headers) | Support included in Ultralight's Windows platform source. SkyUltra's game compositor itself uses D3D11. |
| Microsoft Windows SDK | Direct3D 11, DXGI, shader compiler, WIC image encoding, COM, Media Foundation and Windows platform services |
| [CMake](https://cmake.org/) and [vcpkg](https://github.com/microsoft/vcpkg), including `vcpkg-cmake-config` | Build configuration, dependency discovery and packaging support |

The selected vcpkg libraries' copyright files are retained in [licenses/native](licenses/native/). CommonLib's [manifest](https://github.com/CharmedBaryon/CommonLibSSE-NG) and the actual local source remain authoritative for that dependency; this acknowledgment does not assign a new license to SkyUltra or change upstream terms.

## Frontend, SDK and tooling

| Project | Used for |
| --- | --- |
| [React and React DOM](https://github.com/facebook/react) | Built-in application, React hooks and reusable components |
| [Radix UI](https://github.com/radix-ui/primitives) | Context menus, dialogs, dropdown menus, tabs and tooltips, together with their supporting Radix packages |
| [Motion](https://github.com/motiondivision/motion) | UI animation helpers and supporting Motion packages |
| [Lucide](https://github.com/lucide-icons/lucide) | React icons |
| [Zustand](https://github.com/pmndrs/zustand) | React starter state management |
| [Tailwind CSS](https://github.com/tailwindlabs/tailwindcss) | Component styling toolchain |
| [class-variance-authority](https://github.com/joe-bell/cva), [clsx](https://github.com/lukeed/clsx), [tailwind-merge](https://github.com/dcastil/tailwind-merge) | Variant and class-name composition |
| [TypeScript](https://github.com/microsoft/TypeScript) | SDK/application types and source validation |
| [DefinitelyTyped](https://github.com/DefinitelyTyped/DefinitelyTyped) | `@types/react` and `@types/react-dom` |
| [Vite](https://github.com/vitejs/vite) | Development server and web packaging, with its bundling dependencies |
| [PostCSS](https://github.com/postcss/postcss) and [Autoprefixer](https://github.com/postcss/autoprefixer) | Stylesheet processing |
| [Node.js](https://nodejs.org/) and [npm](https://github.com/npm/cli) | Authoring scripts, package resolution and tooling |

Thanks also to the authors of **every transitive npm package recorded in [the complete inventory](dependencies/inventory.json)**, including the CSS, bundler, source-map, parser, watcher, resolver and platform-specific packages behind these tools. The inventory includes names, versions, license metadata when recorded by the lockfile, declaration roles and source lockfiles. It is not a claim that every optional platform package ships in a runtime bundle.

Regenerate or check the inventory without building or installing:

```powershell
node tools/Update-DependencyInventory.cjs
node tools/Update-DependencyInventory.cjs --check
```

## Libraries embedded by browser runtimes

Ultralight's supplied notices acknowledge **WebKit, Brotli, cURL, FreeType, HarfBuzz, mimalloc, ICU, libjpeg-turbo, libpng, LibreSSL, libxml2, libxslt, nghttp2, Skia, SQLite and zlib**, among other platform components. Its notices also identify optional AppCore/platform components such as Roboto, GLFW, glad, Khronos headers, D3D12 Memory Allocator and operating-system libraries. Their inclusion depends on the selected SDK/platform build; they are not separately installed Skyrim mods.

CEF/Chromium likewise includes many embedded third-party libraries. Preserve the matching browser distribution's full license/credits resources when packaging; the top-level CEF license alone is not an enumeration of all Chromium components. SkyUltra's existing CMake staging retains supplied browser license files; the copies here document the SDK snapshots used for this source audit.

## Community acknowledgments

Thank you to the **[PrismaUI](https://www.prismaui.dev/)** team and **[Magelight UI](https://github.com/Severause/MagelightUI)** author Severause and contributors for advancing web interfaces in Skyrim and publishing integration APIs and examples. SkyUltra's [migration documentation](MIGRATION_GUIDE.md) uses those published contracts and its own supported adapters/shims. These frameworks and other UI/graphics mods are not runtime requirements for SkyUltra.
