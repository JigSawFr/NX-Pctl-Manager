cmake_minimum_required(VERSION 3.10)

# Borealis lives in extern/ as a git submodule (xfangfang/borealis, wiliwili branch).
set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library)

# PLATFORM_SWITCH / PLATFORM_DESKTOP / USE_DEKO3D … options.
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

# Sets up the devkitPro cross-toolchain when PLATFORM_SWITCH=ON.
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

project(nx_pctl_manager LANGUAGES CXX C)

# App metadata. The NACP version must match the release tag (CI checks it).
set(VERSION_MAJOR "4")
set(VERSION_MINOR "0")
set(VERSION_ALTER "0")
set(APP_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}")
set(PROJECT_TITLEID "")
set(PROJECT_AUTHOR "Taylor")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

# PROBE=1: extra diagnostic shortcuts in the Play timer tab.
option(PCTL_PROBE "Include the diagnostic shortcuts" OFF)
# READ_ONLY=1: "Pctl Diagnostics" build that cannot change anything.
option(PCTL_READ_ONLY "Disable all parental-control and clock changes" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

# On desktop borealis looks for "<BRLS_RESOURCES_DIR>/resources/" at runtime:
# point it at the merged copy assembled below.
if (PLATFORM_DESKTOP)
    set(BRLS_RESOURCES_DIR ${CMAKE_BINARY_DIR})
endif ()

add_subdirectory(${BOREALIS_LIBRARY} ${CMAKE_BINARY_DIR}/borealis_library)

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

# Sources: UI (C++) + service layer. The real service layer (source/core/*.c)
# talks to libnx and is Switch-only; the desktop build swaps in the simulated
# backend from source/sim/ so the UI can be run and screenshotted on a PC.
file(GLOB_RECURSE NX_PCTL_SRC CONFIGURE_DEPENDS source/*.cpp source/*.c)
if (PLATFORM_SWITCH)
    list(FILTER NX_PCTL_SRC EXCLUDE REGEX ".*/source/sim/.*")
    list(APPEND NX_PCTL_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
else ()
    list(FILTER NX_PCTL_SRC EXCLUDE REGEX ".*/source/core/.*\\.c$")
endif ()

program_target(${PROJECT_NAME} "${NX_PCTL_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME} PRIVATE source ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})
target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${APP_VERSION}")
target_link_libraries(${PROJECT_NAME} PRIVATE borealis ${APP_PLATFORM_LIB})

if (PCTL_PROBE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PCTL_PROBE=1)
endif ()
if (PCTL_READ_ONLY)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PCTL_READ_ONLY=1)
    set(PCTL_APP_TITLE "Pctl Diagnostics")
else ()
    set(PCTL_APP_TITLE "Pctl Manager")
endif ()

# Resources: borealis built-ins first, then ours on top. On the Switch the
# bundled fonts and demo assets are dropped (the system shared font is used).
# fr-CA reuses the French strings (borealis looks the locale up verbatim).
set(BOREALIS_RESOURCES ${BOREALIS_LIBRARY}/../resources)
set(MERGED_RESOURCES ${CMAKE_BINARY_DIR}/resources)
set(ASSEMBLE_RESOURCES
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BOREALIS_RESOURCES} ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/xml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/img/pokemon
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES}/i18n/fr ${MERGED_RESOURCES}/i18n/fr-CA
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_ICON} ${MERGED_RESOURCES}/img/app_icon.jpg)

if (PLATFORM_SWITCH)
    add_custom_target(${PROJECT_NAME}.nro ALL DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "${PCTL_APP_TITLE}" "${PROJECT_AUTHOR}"
                "${APP_VERSION}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        ${ASSEMBLE_RESOURCES}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/font
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro
                --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp
                --romfsdir=${MERGED_RESOURCES}
    )
else ()
    add_custom_target(${PROJECT_NAME}_resources ALL ${ASSEMBLE_RESOURCES})
    add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_resources)
endif ()
