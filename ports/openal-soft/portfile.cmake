# Overlay port: use Homebrew openal-soft on macOS to avoid AppleClang 15 build failure
set(VCPKG_BUILD_TYPE release)

find_path(OPENAL_INCLUDE_DIR "AL/al.h" HINTS "$ENV{HOMEBREW_PREFIX}/include" "/opt/homebrew/include" "/usr/local/include")
find_library(OPENAL_LIBRARY NAMES openal "OpenAL" HINTS "$ENV{HOMEBREW_PREFIX}/lib" "/opt/homebrew/lib" "/usr/local/lib")

if(NOT OPENAL_INCLUDE_DIR OR NOT OPENAL_LIBRARY)
    message(FATAL_ERROR "openal-soft not found via Homebrew. Run: brew install openal-soft")
endif()

set(PACKAGE_VERSION "1.25.1")

file(INSTALL "${OPENAL_INCLUDE_DIR}/AL" DESTINATION "${CURRENT_PACKAGES_DIR}/include")
file(INSTALL "${OPENAL_LIBRARY}" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")

configure_file("${CMAKE_CURRENT_LIST_DIR}/OpenALConfig.cmake.in"
    "${CURRENT_PACKAGES_DIR}/share/openal-soft/OpenALConfig.cmake" @ONLY)

file(WRITE "${CURRENT_PACKAGES_DIR}/share/openal-soft/vcpkg-cmake-wrapper.cmake"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/OpenALConfig.cmake\")")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/openal-soft")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/openal-soft/copyright" "OpenAL Soft - see https://github.com/kcat/openal-soft")
