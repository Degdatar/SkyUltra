if(NOT DEFINED SKYULTRA_RESOURCE_SOURCE OR
   NOT DEFINED SKYULTRA_RESOURCE_DESTINATION OR
   NOT DEFINED SKYULTRA_PLUGIN_RESOURCES_DESTINATION)
    message(FATAL_ERROR "SkyUltra resource staging paths were not provided.")
endif()

if(NOT IS_DIRECTORY "${SKYULTRA_RESOURCE_SOURCE}")
    message(FATAL_ERROR "SkyUltra resource source directory is missing: ${SKYULTRA_RESOURCE_SOURCE}")
endif()

file(MAKE_DIRECTORY "${SKYULTRA_RESOURCE_DESTINATION}")

# SkyUltra.ini is plugin configuration, not a web asset; keep it beside the DLL.
# Stage all other owned resources so styles, fonts, and new image files are
# included automatically as they are added to the source resources directory.
file(COPY "${SKYULTRA_RESOURCE_SOURCE}/"
    DESTINATION "${SKYULTRA_RESOURCE_DESTINATION}"
    PATTERN "SkyUltra.ini" EXCLUDE
    PATTERN "SkyUltraDemo.html" EXCLUDE
    PATTERN "SkyUltraWindowTest.html" EXCLUDE)

# Retire prior menu pages from this project's existing generated package as well.
file(REMOVE
    "${SKYULTRA_RESOURCE_DESTINATION}/SkyUltraDemo.html"
    "${SKYULTRA_RESOURCE_DESTINATION}/SkyUltraWindowTest.html")

# Remove the old flat copies from this project's generated package output.
file(REMOVE
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/icudt67l.dat"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/cacert.pem"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltraDemo.html"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltraWindowTest.html"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltraBridge.js"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltraLogo.png"
    "${SKYULTRA_PLUGIN_RESOURCES_DESTINATION}/SkyUltraLogoMark.png")
