cmake_minimum_required(VERSION 3.26)

project(SkyUltra VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "SkyUltra's initial Skyrim host targets Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "SkyUltra requires a 64-bit toolchain.")
endif()

set(SKYULTRA_COMMONLIBSSE_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/_deps/CommonLibSSE-NG/source"
    CACHE PATH "Local CommonLibSSE-NG source checkout")

if(SKYULTRA_COMMONLIBSSE_SOURCE_DIR AND
   EXISTS "${SKYULTRA_COMMONLIBSSE_SOURCE_DIR}/CMakeLists.txt")
    set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLibSSE-NG's own tests" FORCE)
    # Build one CommonLib target for flat SE/AE and VR. GOG AE uses the same
    # engine family as AE and is recognized by SKSE's store-specific runtime version.
    set(ENABLE_SKYRIM_SE ON CACHE BOOL "Enable CommonLibSSE-NG Skyrim SE support" FORCE)
    set(ENABLE_SKYRIM_AE ON CACHE BOOL "Enable CommonLibSSE-NG Skyrim AE support" FORCE)
    set(ENABLE_SKYRIM_VR ON CACHE BOOL "Enable CommonLibSSE-NG Skyrim VR support" FORCE)
    add_subdirectory(
        "${SKYULTRA_COMMONLIBSSE_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/commonlibsse-ng")
else()
    find_package(CommonLibSSE CONFIG REQUIRED)
endif()

if(NOT TARGET CommonLibSSE::CommonLibSSE)
    message(FATAL_ERROR
        "The selected CommonLibSSE-NG dependency must provide "
        "CommonLibSSE::CommonLibSSE.")
endif()

find_package(nlohmann_json CONFIG REQUIRED)

add_library(skyultra_core STATIC
    src/core/BridgeScript.cpp
    src/core/ModManifest.cpp
    src/core/ViewManager.cpp
)

target_include_directories(skyultra_core PUBLIC include)
target_compile_features(skyultra_core PUBLIC cxx_std_20)
target_compile_definitions(skyultra_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(skyultra_core PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(skyultra_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(SkyUltra SHARED
    src/skse/Plugin.cpp
    src/skse/Papyrus.cpp
)

target_precompile_headers(SkyUltra PRIVATE <SKSE/Impl/PCH.h>)
target_link_libraries(SkyUltra PRIVATE
    skyultra_core
    CommonLibSSE::CommonLibSSE
    nlohmann_json::nlohmann_json
)

target_compile_features(SkyUltra PRIVATE cxx_std_23)
target_compile_definitions(SkyUltra PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_definitions(SkyUltra PRIVATE SKYULTRA_BUILDING_PLUGIN)
set_target_properties(SkyUltra PROPERTIES
    OUTPUT_NAME "SkyUltra"
    CXX_EXTENSIONS OFF
)

# Keep source-level symbols with Release builds so CrashLogger can resolve
# plugin faults from the package directory instead of reporting only an RVA.
if(MSVC)
    target_compile_options(SkyUltra PRIVATE "$<$<CONFIG:Release>:/Zi>")
    target_link_options(SkyUltra PRIVATE "$<$<CONFIG:Release>:/DEBUG:FULL>")
endif()

option(SKYULTRA_BUILD_CEF "Build the CEF/Chromium browser backend" ON)
option(SKYULTRA_BUILD_ULTRALIGHT "Build the Ultralight GPU browser backend" ON)
set(SKYULTRA_CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/_deps/cef" CACHE PATH
    "Root of the CEF binary distribution")
set(SKYULTRA_DEFAULT_BACKEND "CEF" CACHE STRING
    "Browser backend selected by default (CEF or ULTRALIGHT)")
set_property(CACHE SKYULTRA_DEFAULT_BACKEND PROPERTY STRINGS CEF ULTRALIGHT)

if(NOT SKYULTRA_BUILD_CEF AND NOT SKYULTRA_BUILD_ULTRALIGHT)
    message(FATAL_ERROR "Enable at least one SkyUltra browser backend.")
endif()
if(SKYULTRA_DEFAULT_BACKEND STREQUAL "ULTRALIGHT" AND NOT SKYULTRA_BUILD_ULTRALIGHT)
    message(FATAL_ERROR "SKYULTRA_DEFAULT_BACKEND=ULTRALIGHT requires SKYULTRA_BUILD_ULTRALIGHT=ON.")
elseif(SKYULTRA_DEFAULT_BACKEND STREQUAL "CEF" AND NOT SKYULTRA_BUILD_CEF)
    message(FATAL_ERROR "SKYULTRA_DEFAULT_BACKEND=CEF requires SKYULTRA_BUILD_CEF=ON.")
elseif(NOT SKYULTRA_DEFAULT_BACKEND STREQUAL "ULTRALIGHT" AND
       NOT SKYULTRA_DEFAULT_BACKEND STREQUAL "CEF")
    message(FATAL_ERROR "SKYULTRA_DEFAULT_BACKEND must be ULTRALIGHT or CEF.")
endif()

# Browser runtimes and the optional VR path use delay-loaded DLLs so a build
# can include both engines while loading their private runtime files lazily.
target_link_libraries(SkyUltra PRIVATE delayimp)

if(SKYULTRA_BUILD_CEF)
    enable_language(RC)
    if(NOT EXISTS "${SKYULTRA_CEF_ROOT}/include/cef_app.h")
        message(FATAL_ERROR "Set SKYULTRA_CEF_ROOT to an extracted CEF binary distribution.")
    endif()
    if(NOT EXISTS "${SKYULTRA_CEF_ROOT}/Release/libcef.lib" OR
       NOT EXISTS "${SKYULTRA_CEF_ROOT}/Release/libcef.dll" OR
       NOT EXISTS "${SKYULTRA_CEF_ROOT}/libcef_dll/CMakeLists.txt")
        message(FATAL_ERROR "The CEF binary distribution is missing Release libraries or libcef_dll sources.")
    endif()

    set(CEF_ROOT "${SKYULTRA_CEF_ROOT}")
    list(APPEND CMAKE_MODULE_PATH "${SKYULTRA_CEF_ROOT}/cmake")
    set(USE_SANDBOX OFF CACHE BOOL "Build CEF without its Chromium sandbox" FORCE)
    # CEF defaults its wrapper to /MT, but this project and vcpkg use the
    # dynamic CRT (/MD). Keep the wrapper and subprocess on the same runtime.
    set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING
        "Runtime library used by the CEF wrapper" FORCE)
    find_package(CEF REQUIRED)
    add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/cef-wrapper")

    target_compile_definitions(SkyUltra PRIVATE SKYULTRA_WITH_CEF=1)

    add_library(SkyUltraCEF SHARED IMPORTED GLOBAL)
    set_target_properties(SkyUltraCEF PROPERTIES IMPORTED_CONFIGURATIONS "DEBUG;RELEASE")
    set_target_properties(SkyUltraCEF PROPERTIES
        IMPORTED_IMPLIB_DEBUG "${CEF_LIB_DEBUG}"
        IMPORTED_LOCATION_DEBUG "${CEF_BINARY_DIR_DEBUG}/libcef.dll"
        IMPORTED_IMPLIB_RELEASE "${CEF_LIB_RELEASE}"
        IMPORTED_LOCATION_RELEASE "${CEF_BINARY_DIR_RELEASE}/libcef.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${SKYULTRA_CEF_ROOT}")
    set_property(TARGET SkyUltraCEF PROPERTY MAP_IMPORTED_CONFIG_NOCONFIG RELEASE)

    add_library(skyultra_cef STATIC src/cef/CefBackend.cpp)
    target_include_directories(skyultra_cef PUBLIC include PRIVATE "${SKYULTRA_CEF_ROOT}")
    target_compile_features(skyultra_cef PUBLIC cxx_std_20)
    target_compile_definitions(skyultra_cef PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(skyultra_cef PUBLIC libcef_dll_wrapper SkyUltraCEF ${CEF_STANDARD_LIBS})
    target_link_libraries(SkyUltra PRIVATE skyultra_cef)
    target_link_options(SkyUltra PRIVATE "/DELAYLOAD:libcef.dll")

    add_executable(SkyUltraCefSubprocess WIN32
        src/cef/CefSubprocess.cpp
        src/cef/SkyUltraCefSubprocess.rc)
    target_include_directories(SkyUltraCefSubprocess PRIVATE "${SKYULTRA_CEF_ROOT}")
    target_link_libraries(SkyUltraCefSubprocess PRIVATE libcef_dll_wrapper SkyUltraCEF ${CEF_STANDARD_LIBS})
    target_compile_features(SkyUltraCefSubprocess PRIVATE cxx_std_20)
    target_compile_definitions(SkyUltraCefSubprocess PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

    set(SKYULTRA_CEF_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package/Data/SKSE/Plugins/SkyUltra/CEF")
    set_target_properties(SkyUltraCefSubprocess PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${SKYULTRA_CEF_PACKAGE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${SKYULTRA_CEF_PACKAGE_DIR}")
    foreach(cef_binary IN LISTS CEF_BINARY_FILES)
        add_custom_command(TARGET SkyUltraCefSubprocess POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_BINARY_DIR_RELEASE}/${cef_binary}" "${SKYULTRA_CEF_PACKAGE_DIR}")
    endforeach()
    foreach(cef_resource IN LISTS CEF_RESOURCE_FILES)
        if(IS_DIRECTORY "${CEF_RESOURCE_DIR}/${cef_resource}")
            add_custom_command(TARGET SkyUltraCefSubprocess POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CEF_RESOURCE_DIR}/${cef_resource}"
                    "${SKYULTRA_CEF_PACKAGE_DIR}/${cef_resource}")
        else()
            add_custom_command(TARGET SkyUltraCefSubprocess POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CEF_RESOURCE_DIR}/${cef_resource}"
                    "${SKYULTRA_CEF_PACKAGE_DIR}")
        endif()
    endforeach()
    foreach(cef_notice IN ITEMS LICENSE.txt README.txt)
        if(EXISTS "${SKYULTRA_CEF_ROOT}/${cef_notice}")
            add_custom_command(TARGET SkyUltraCefSubprocess POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SKYULTRA_CEF_ROOT}/${cef_notice}"
                    "${SKYULTRA_CEF_PACKAGE_DIR}/${cef_notice}")
        endif()
    endforeach()
endif()

if(SKYULTRA_BUILD_ULTRALIGHT)
    set(Ultralight_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmake/Ultralight" CACHE PATH
        "Path to the Ultralight 2.0 CMake package")
    find_package(Ultralight 2.0 CONFIG REQUIRED)
    if(NOT ULTRALIGHT_PLATFORM_DIR)
        message(FATAL_ERROR "The selected Ultralight SDK has no D3D11 platform sources.")
    endif()
    set(UL_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH
        "Ultralight SDK root used by its reference platform sources" FORCE)
    add_subdirectory("${ULTRALIGHT_PLATFORM_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ultralight-platform")

    add_library(skyultra_ultralight STATIC src/ultralight/UltralightBackend.cpp)
    target_include_directories(skyultra_ultralight PUBLIC include)
    target_compile_features(skyultra_ultralight PUBLIC cxx_std_20)
    target_compile_definitions(skyultra_ultralight PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(skyultra_ultralight PUBLIC
        CommonLibSSE::CommonLibSSE
        Ultralight::Ultralight
        Ultralight::Platform)
    target_link_libraries(SkyUltra PRIVATE skyultra_ultralight)
    target_sources(SkyUltra PRIVATE src/ultralight/UltralightRuntimeLoader.cpp)
    target_link_options(SkyUltra PRIVATE
        "/DELAYLOAD:Ultralight.dll"
        "/DELAYLOAD:UltralightCore.dll")
    target_compile_definitions(SkyUltra PRIVATE SKYULTRA_WITH_ULTRALIGHT=1)
endif()

if(SKYULTRA_BUILD_CEF OR SKYULTRA_BUILD_ULTRALIGHT)
    find_package(directxtk CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)

    # CommonLib exports a Windows 7 SDK target and a shared PCH. Media Engine
    # requires Windows 8 declarations, so keep this optional implementation in
    # an independent target. Neither its SDK target nor Windows headers leak
    # into the Skyrim/CommonLib translation units.
    add_library(skyultra_video STATIC src/render/VideoSplash.cpp)
    target_include_directories(skyultra_video PRIVATE include)
    target_compile_features(skyultra_video PRIVATE cxx_std_20)
    target_compile_definitions(skyultra_video PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0602 _WIN32_WINNT=0x0602)
    target_link_libraries(skyultra_video PRIVATE
        spdlog::spdlog ole32 oleaut32 mfplat mf mfuuid d3d11 dxgi windowscodecs)

    add_library(skyultra_d3d11_overlay STATIC
        src/render/D3D11Overlay.cpp
        src/render/ActorAnimation.cpp
        src/render/ActorPoseFile.cpp
        src/render/ActorPreview.cpp)
    target_include_directories(skyultra_d3d11_overlay PUBLIC include)
    target_compile_features(skyultra_d3d11_overlay PUBLIC cxx_std_20)
    target_precompile_headers(skyultra_d3d11_overlay PRIVATE <SKSE/Impl/PCH.h>)
    target_compile_definitions(skyultra_d3d11_overlay PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(skyultra_d3d11_overlay PUBLIC
        CommonLibSSE::CommonLibSSE
        Microsoft::DirectXTK
        skyultra_video
        d3dcompiler windowscodecs ole32)
    target_link_libraries(SkyUltra PRIVATE skyultra_d3d11_overlay)

    if(ENABLE_SKYRIM_VR)
        # The same plugin DLL is loaded by flat SE/AE/GOG as by VR. Keep OpenVR
        # optional at DLL load time; the VR path resolves it only in Skyrim VR.
        target_link_options(SkyUltra PRIVATE "/DELAYLOAD:openvr_api.dll")
    endif()

    if(SKYULTRA_DEFAULT_BACKEND STREQUAL "ULTRALIGHT")
        target_compile_definitions(SkyUltra PRIVATE SKYULTRA_DEFAULT_BACKEND_ULTRALIGHT=1)
    else()
        target_compile_definitions(SkyUltra PRIVATE SKYULTRA_DEFAULT_BACKEND_CEF=1)
    endif()

    set(SKYULTRA_PLUGIN_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package/Data/SKSE/Plugins")
    set_target_properties(SkyUltra PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${SKYULTRA_PLUGIN_PACKAGE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${SKYULTRA_PLUGIN_PACKAGE_DIR}")

    set(SKYULTRA_PAPYRUS_PACKAGE_SOURCE_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Source/Scripts")
    set(SKYULTRA_PAPYRUS_PACKAGE_SCRIPT_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Scripts")
    set(SKYULTRA_PAPYRUS_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/papyrus/Source/Scripts/SkyUltraNative.psc")
    set(SKYULTRA_PAPYRUS_SAMPLE_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/papyrus/Examples/SkyUltraPapyrusSampleQuest.psc")
    set(SKYULTRA_PAPYRUS_COMPILED_PEX
        "${CMAKE_CURRENT_SOURCE_DIR}/sdk/papyrus/Compiled/SkyUltraNative.pex")
    file(MAKE_DIRECTORY
        "${SKYULTRA_PAPYRUS_PACKAGE_SOURCE_DIR}"
        "${SKYULTRA_PAPYRUS_PACKAGE_SCRIPT_DIR}")
    configure_file("${SKYULTRA_PAPYRUS_SOURCE}"
        "${SKYULTRA_PAPYRUS_PACKAGE_SOURCE_DIR}/SkyUltraNative.psc" COPYONLY)
    configure_file("${SKYULTRA_PAPYRUS_SAMPLE_SOURCE}"
        "${SKYULTRA_PAPYRUS_PACKAGE_SOURCE_DIR}/SkyUltraPapyrusSampleQuest.psc" COPYONLY)
    configure_file("${SKYULTRA_PAPYRUS_COMPILED_PEX}"
        "${SKYULTRA_PAPYRUS_PACKAGE_SCRIPT_DIR}/SkyUltraNative.pex" COPYONLY)

    if(SKYULTRA_BUILD_ULTRALIGHT)
        set(SKYULTRA_ULTRALIGHT_RUNTIME_DIR
            "$<TARGET_FILE_DIR:SkyUltra>/SkyUltra/Ultralight")
        set(SKYULTRA_ULTRALIGHT_RESOURCES_DIR
            "$<TARGET_FILE_DIR:SkyUltra>/resources/SkyUltra/Ultralight")
        add_custom_command(TARGET SkyUltra POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${SKYULTRA_ULTRALIGHT_RUNTIME_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ULTRALIGHT_BINARY_DIR}/Ultralight.dll"
                "${SKYULTRA_ULTRALIGHT_RUNTIME_DIR}/SUL.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ULTRALIGHT_BINARY_DIR}/UltralightCore.dll"
                "${SKYULTRA_ULTRALIGHT_RUNTIME_DIR}/SULCore.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ULTRALIGHT_BINARY_DIR}/WebCore.dll"
                "${SKYULTRA_ULTRALIGHT_RUNTIME_DIR}/SULWeb.dll"
            VERBATIM)
        add_custom_command(TARGET SkyUltra POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                "-DULTRALIGHT_RESOURCES_SOURCE=${ULTRALIGHT_RESOURCES_DIR}"
                "-DULTRALIGHT_RESOURCES_DESTINATION=${SKYULTRA_ULTRALIGHT_RESOURCES_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/StageUltralightResources.cmake"
            VERBATIM)
        find_program(SKYULTRA_POWERSHELL_EXECUTABLE NAMES pwsh.exe powershell.exe REQUIRED)
        add_custom_command(TARGET SkyUltra POST_BUILD
            COMMAND "${SKYULTRA_POWERSHELL_EXECUTABLE}"
                -NoProfile -NonInteractive -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PatchUltralightRuntimeImports.ps1"
                -PluginPath "$<TARGET_FILE:SkyUltra>"
                -RuntimeDirectory "${SKYULTRA_ULTRALIGHT_RUNTIME_DIR}"
            VERBATIM)
    endif()

    add_custom_command(TARGET SkyUltra POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:SkyUltra>/resources/SkyUltra/Files"
            "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Interface"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/SkyUltra.ini"
            "$<TARGET_FILE_DIR:SkyUltra>/SkyUltra.ini"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/skyultra_inventory.swf"
            "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Interface/skyultra_inventory.swf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/skyultra_barter.swf"
            "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Interface/skyultra_barter.swf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/skyultra_magic.swf"
            "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Interface/skyultra_magic.swf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/skyultra_favorites.swf"
            "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Interface/skyultra_favorites.swf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/skyultra_container.swf"
            "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Interface/skyultra_container.swf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/skyultra_mannequin.swf"
            "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Interface/skyultra_mannequin.swf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/skyultra_loot.swf"
            "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Interface/skyultra_loot.swf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/skyultra_pickpocket.swf"
            "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Interface/skyultra_pickpocket.swf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/skyultra_compass.swf"
            "${CMAKE_CURRENT_BINARY_DIR}/package/Data/Interface/skyultra_compass.swf"
        COMMAND ${CMAKE_COMMAND}
            "-DSKYULTRA_RESOURCE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "-DSKYULTRA_RESOURCE_DESTINATION=$<TARGET_FILE_DIR:SkyUltra>/resources/SkyUltra/Files"
            "-DSKYULTRA_PLUGIN_RESOURCES_DESTINATION=$<TARGET_FILE_DIR:SkyUltra>/resources"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/StageSkyUltraFiles.cmake"
        VERBATIM)
endif()

