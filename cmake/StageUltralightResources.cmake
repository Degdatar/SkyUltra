if(NOT DEFINED ULTRALIGHT_RESOURCES_SOURCE OR
   NOT DEFINED ULTRALIGHT_RESOURCES_DESTINATION)
    message(FATAL_ERROR "Ultralight resource staging paths were not provided.")
endif()

foreach(required_file IN ITEMS icudt67l.dat cacert.pem)
    if(NOT EXISTS "${ULTRALIGHT_RESOURCES_SOURCE}/${required_file}")
        message(FATAL_ERROR "Ultralight SDK resource is missing: ${required_file}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${ULTRALIGHT_RESOURCES_DESTINATION}")

# Copy the complete SDK resource set into SkyUltra's private runtime folder.
file(REMOVE
    "${ULTRALIGHT_RESOURCES_DESTINATION}/SkyUltra_icudt67l.dat"
    "${ULTRALIGHT_RESOURCES_DESTINATION}/SkyUltra_cacert.pem")
file(COPY "${ULTRALIGHT_RESOURCES_SOURCE}/"
    DESTINATION "${ULTRALIGHT_RESOURCES_DESTINATION}")

# Ultralight requests these standard resource names. SkyUltra's filesystem
# adapter maps those logical paths to the uniquely named packaged files.
file(RENAME "${ULTRALIGHT_RESOURCES_DESTINATION}/icudt67l.dat"
    "${ULTRALIGHT_RESOURCES_DESTINATION}/SkyUltra_icudt67l.dat")
file(RENAME "${ULTRALIGHT_RESOURCES_DESTINATION}/cacert.pem"
    "${ULTRALIGHT_RESOURCES_DESTINATION}/SkyUltra_cacert.pem")


