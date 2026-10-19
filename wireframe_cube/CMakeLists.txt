cmake_minimum_required(VERSION 3.20)
project(wireframe_cube LANGUAGES NONE)

# Arduino CLI owns preprocessing, dependency discovery and the ESP32 toolchain.
# Keep board defaults and build logic in the existing PowerShell entry point.
find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
set(ARDUINO_PORT "COM6" CACHE STRING "Serial port used by the upload target")
set(ARDUINO_REVISION "v2" CACHE STRING "Display board revision")
set_property(CACHE ARDUINO_REVISION PROPERTY STRINGS original v2)
if(NOT ARDUINO_REVISION MATCHES "^(original|v2)$")
    message(FATAL_ERROR "ARDUINO_REVISION must be original or v2")
endif()

# Empty overrides preserve Build.ps1 defaults.
set(ARDUINO_CLI_PATH "" CACHE FILEPATH "Optional Arduino CLI executable override")
set(ARDUINO_CONFIG_FILE "" CACHE FILEPATH "Optional Arduino CLI YAML configuration override")
set(ARDUINO_FQBN "" CACHE STRING "Optional fully qualified board name override")
set(build_command "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
    -File "${CMAKE_CURRENT_SOURCE_DIR}/Build.ps1"
    -Revision "${ARDUINO_REVISION}")
foreach(pair IN ITEMS "ARDUINO_CLI_PATH|CliPath" "ARDUINO_CONFIG_FILE|ConfigFile" "ARDUINO_FQBN|Fqbn")
    string(REPLACE "|" ";" pair "${pair}")
    list(GET pair 0 variable)
    list(GET pair 1 parameter)
    if(NOT "${${variable}}" STREQUAL "")
        list(APPEND build_command "-${parameter}" "${${variable}}")
    endif()
endforeach()

add_custom_target(build ALL
    COMMAND ${build_command}
    COMMENT "Compiling wireframe_cube through Arduino CLI"
    VERBATIM USES_TERMINAL)

# Build.ps1 -Upload compiles before flashing, so upload works independently
# without a second compilation from a dependency on the build target.
add_custom_target(upload
    COMMAND ${build_command} -Upload -Port "${ARDUINO_PORT}"
    COMMENT "Compiling and uploading wireframe_cube to ${ARDUINO_PORT}"
    VERBATIM USES_TERMINAL)

add_custom_target(upload-plan
    COMMAND ${build_command} -Upload -Port "${ARDUINO_PORT}" -DryRun
    COMMENT "Showing the upload command without compiling or flashing"
    VERBATIM USES_TERMINAL)
