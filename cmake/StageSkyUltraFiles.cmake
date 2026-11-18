if(NOT DEFINED SKYULTRA_RESOURCE_SOURCE OR
   NOT DEFINED SKYULTRA_RESOURCE_DESTINATION OR
   NOT DEFINED SKYULTRA_PLUGIN_RESOURCES_DESTINATION)
    message(FATAL_ERROR "SkyUltra resource staging paths were not provided.")
endif()

if(NOT IS_DIRECTORY "${SKYULTRA_RESOURCE_SOURCE}")
    message(FATAL_ERROR "SkyUltra resource source directory is missing: ${SKYULTRA_RESOURCE_SOURCE}")
endif()

# Retire the duplicate SKSE/Plugins/resources/SkyUltra/Files copy from generated
# packages: SWFs are staged directly to Data/Interface, and SkyUltraSplash.mp4,
# logos, and SkyUltraBridge.js live inside Data/SkyUltra/skyultra.menu/Files/.
file(REMOVE_RECURSE "${SKYULTRA_RESOURCE_DESTINATION}")

# Remove the old flat copies from this project's generated package output.
file(REMOVE
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltra.ini"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/icudt67l.dat"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/cacert.pem"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltraDemo.html"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltraWindowTest.html"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltraBridge.js"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltraLogo.png"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltraLogoMark.png")
