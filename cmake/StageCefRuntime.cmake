if(NOT DEFINED CEF_BINARY_SOURCE OR
   NOT DEFINED CEF_RESOURCE_SOURCE OR
   NOT DEFINED CEF_ROOT_SOURCE OR
   NOT DEFINED CEF_PACKAGE_DESTINATION OR
   NOT DEFINED CEF_DIAGNOSTICS_DESTINATION)
    message(FATAL_ERROR "SkyUltra CEF staging paths were not provided.")
endif()

foreach(required_dir IN ITEMS "${CEF_BINARY_SOURCE}" "${CEF_RESOURCE_SOURCE}" "${CEF_ROOT_SOURCE}")
    if(NOT IS_DIRECTORY "${required_dir}")
        message(FATAL_ERROR "Required CEF source directory is missing: ${required_dir}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${CEF_PACKAGE_DESTINATION}")
file(MAKE_DIRECTORY "${CEF_PACKAGE_DESTINATION}/locales")
file(MAKE_DIRECTORY "${CEF_DIAGNOSTICS_DESTINATION}")

# Remove stale directories/files from earlier builds and move subprocess PDBs
# out of the Data package into Diagnostics/ so Crashpad/dump analysis keeps symbols
# without shipping extra debug files in the player mod folder.
file(REMOVE_RECURSE
    "${CEF_PACKAGE_DESTINATION}/Resources"
    "${CEF_PACKAGE_DESTINATION}/Release")
file(REMOVE "${CEF_PACKAGE_DESTINATION}/README.txt")

if(EXISTS "${CEF_PACKAGE_DESTINATION}/SkyUltraCefSubprocess.pdb")
    file(COPY_FILE
        "${CEF_PACKAGE_DESTINATION}/SkyUltraCefSubprocess.pdb"
        "${CEF_DIAGNOSTICS_DESTINATION}/SkyUltraCefSubprocess.pdb"
        ONLY_IF_DIFFERENT)
    file(REMOVE "${CEF_PACKAGE_DESTINATION}/SkyUltraCefSubprocess.pdb")
endif()

foreach(cef_binary IN ITEMS
    chrome_elf.dll
    d3dcompiler_47.dll
    dxcompiler.dll
    dxil.dll
    libcef.dll
    v8_context_snapshot.bin
    vk_swiftshader.dll
    vk_swiftshader_icd.json
    vulkan-1.dll)
    if(NOT EXISTS "${CEF_BINARY_SOURCE}/${cef_binary}")
        message(FATAL_ERROR "Missing required CEF binary: ${CEF_BINARY_SOURCE}/${cef_binary}")
    endif()
    file(COPY_FILE
        "${CEF_BINARY_SOURCE}/${cef_binary}"
        "${CEF_PACKAGE_DESTINATION}/${cef_binary}"
        ONLY_IF_DIFFERENT)
endforeach()

foreach(cef_resource IN ITEMS
    chrome_100_percent.pak
    chrome_200_percent.pak
    icudtl.dat
    resources.pak)
    if(NOT EXISTS "${CEF_RESOURCE_SOURCE}/${cef_resource}")
        message(FATAL_ERROR "Missing required CEF resource: ${CEF_RESOURCE_SOURCE}/${cef_resource}")
    endif()
    file(COPY_FILE
        "${CEF_RESOURCE_SOURCE}/${cef_resource}"
        "${CEF_PACKAGE_DESTINATION}/${cef_resource}"
        ONLY_IF_DIFFERENT)
endforeach()

if(EXISTS "${CEF_ROOT_SOURCE}/LICENSE.txt")
    file(COPY_FILE
        "${CEF_ROOT_SOURCE}/LICENSE.txt"
        "${CEF_PACKAGE_DESTINATION}/LICENSE.txt"
        ONLY_IF_DIFFERENT)
endif()

# Only ship en-US.pak (settings.locale = "en-US"): removes 219 unused locale and
# empty gender-stub .pak files while preserving full ICU/Unicode & JS Intl in icudtl.dat.
file(GLOB stale_locale_paks "${CEF_PACKAGE_DESTINATION}/locales/*.pak")
foreach(locale_pak IN LISTS stale_locale_paks)
    get_filename_component(locale_name "${locale_pak}" NAME)
    if(NOT locale_name STREQUAL "en-US.pak")
        file(REMOVE "${locale_pak}")
    endif()
endforeach()

if(NOT EXISTS "${CEF_RESOURCE_SOURCE}/locales/en-US.pak")
    message(FATAL_ERROR "Missing required CEF locale pak: ${CEF_RESOURCE_SOURCE}/locales/en-US.pak")
endif()
file(COPY_FILE
    "${CEF_RESOURCE_SOURCE}/locales/en-US.pak"
    "${CEF_PACKAGE_DESTINATION}/locales/en-US.pak"
    ONLY_IF_DIFFERENT)
