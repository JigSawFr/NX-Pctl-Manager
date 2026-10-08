cmake_minimum_required(VERSION 3.10)

# Borealis lives in extern/ as a git submodule (xfangfang/borealis, wiliwili branch).
set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/extern/borealis/library)

# On the Switch, draw with deko3d (devkitPro's own GPU API) rather than mesa's
# OpenGL: mesa (libEGL + libglapi) was ~5 MB of the 9.4 MB .nro and most of
# the start-up time. -DUSE_DEKO3D=OFF (make GL=1) builds the OpenGL variant.
# Declared before borealis' options so this default wins; ignored on desktop.
option(USE_DEKO3D "Switch: draw with deko3d instead of OpenGL (mesa)" ON)

# PLATFORM_SWITCH / PLATFORM_DESKTOP / USE_DEKO3D … options.
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

# Sets up the devkitPro cross-toolchain when PLATFORM_SWITCH=ON.
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

project(playguard LANGUAGES CXX C)

# borealis' toolchain.cmake sets CMAKE_C_FLAGS / CMAKE_CXX_FLAGS as normal
# variables (include paths), which hides the cache values devkitPro's
# NintendoSwitch platform file initialises: the Cortex-A57 tuning, the crc and
# crypto extensions and the thread-local model libnx is built with. Put them
# back for every target (borealis, yoga, fmt, tinyxml2 and the app).
if (PLATFORM_SWITCH AND NX_ARCH_SETTINGS)
    string(PREPEND CMAKE_C_FLAGS "${NX_ARCH_SETTINGS} ")
    string(PREPEND CMAKE_CXX_FLAGS "${NX_ARCH_SETTINGS} ")
endif ()
# devkitPro's platform file defaults to Release (-O2 -g; elf2nro keeps no
# debug data). borealis prints its "Build Type:" line before project() picks
# that default, so say what is actually used.
# deko3d draws with precompiled shaders: borealis' nanovg loads them from
# romfs:/shaders/, so compile them (uam, part of switch-dev) for the romfs.
set(PLAYGUARD_DKSH_DIR ${CMAKE_BINARY_DIR}/dksh)
if (PLATFORM_SWITCH AND USE_DEKO3D)
    gen_dksh(${PLAYGUARD_DKSH_DIR})
    foreach (shader fill_vsh fill_fsh fill_aa_fsh)
        if (NOT EXISTS ${PLAYGUARD_DKSH_DIR}/${shader}.dksh)
            message(FATAL_ERROR "uam did not produce ${shader}.dksh")
        endif ()
    endforeach ()
endif ()
if (PLATFORM_SWITCH)
    if (USE_DEKO3D)
        message(STATUS "PlayGuard renderer: deko3d")
    else ()
        message(STATUS "PlayGuard renderer: OpenGL (mesa)")
    endif ()
endif ()
string(TOUPPER "${CMAKE_BUILD_TYPE}" PLAYGUARD_BUILD_TYPE_UC)
message(STATUS "PlayGuard build type: ${CMAKE_BUILD_TYPE}; C++ flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${PLAYGUARD_BUILD_TYPE_UC}}")

# App metadata. The NACP version must match the release tag (CI checks it).
# release-please bumps these three lines (see release-please-config.json).
set(VERSION_MAJOR "1") # x-release-please-major
set(VERSION_MINOR "0") # x-release-please-minor
set(VERSION_ALTER "0") # x-release-please-patch
set(APP_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}")
set(PROJECT_TITLEID "")
set(PROJECT_AUTHOR "JigSawFr")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

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

# libcurl for the update check (util/http.cpp). On the Switch: devkitPro's
# switch-curl portlib (TLS through the console's ssl service), static, so its
# own dependencies are listed after it.
if (PLATFORM_SWITCH)
    list(APPEND APP_PLATFORM_LIB curl z nx)
else ()
    find_package(CURL REQUIRED)
    list(APPEND APP_PLATFORM_LIB CURL::libcurl)
endif ()

# Sources: UI (C++) + service layer. The real service layer (source/core/*.c)
# talks to libnx and is Switch-only; the desktop build swaps in the simulated
# backend from source/sim/ so the UI can be run and screenshotted on a PC.
file(GLOB_RECURSE PLAYGUARD_SRC CONFIGURE_DEPENDS source/*.cpp source/*.c)
if (PLATFORM_SWITCH)
    list(FILTER PLAYGUARD_SRC EXCLUDE REGEX ".*/source/sim/.*")
    list(APPEND PLAYGUARD_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
else ()
    list(FILTER PLAYGUARD_SRC EXCLUDE REGEX ".*/source/core/.*\\.c$")
    # The console-free parts are shared with the simulator: one implementation.
    list(APPEND PLAYGUARD_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/core/pure.c
                              ${CMAKE_CURRENT_SOURCE_DIR}/source/core/calendar.c
                              ${CMAKE_CURRENT_SOURCE_DIR}/source/core/rescue.c
                              ${CMAKE_CURRENT_SOURCE_DIR}/source/core/write_guard.c)
endif ()

program_target(${PROJECT_NAME} "${PLAYGUARD_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME} PRIVATE source ${APP_PLATFORM_INCLUDE})
# -ffunction-sections / -fdata-sections come from borealis (PUBLIC) and
# --gc-sections from libnx's switch.specs.
target_compile_options(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_OPTION})
target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${APP_VERSION}")
target_link_libraries(${PROJECT_NAME} PRIVATE borealis ${APP_PLATFORM_LIB})
# On the Switch: a linker map (build/borealis.map) to see what makes the .nro big.
target_link_options(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_LINK_OPTION})

# Resources: borealis built-ins first, then ours on top. On the Switch the
# bundled fonts and demo assets are dropped (the system shared font is used).
# en-GB reuses the English strings, already in British spelling (borealis looks
# the locale up verbatim).
set(BOREALIS_RESOURCES ${BOREALIS_LIBRARY}/../resources)
set(MERGED_RESOURCES ${CMAKE_BINARY_DIR}/resources)
set(ASSEMBLE_RESOURCES
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BOREALIS_RESOURCES} ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/xml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/img/pokemon
    COMMAND ${CMAKE_COMMAND} -E remove -f ${MERGED_RESOURCES}/img/tiles.png ${MERGED_RESOURCES}/img/demo_icon.jpg
            ${MERGED_RESOURCES}/img/borealis_96.png ${MERGED_RESOURCES}/img/borealis_256.png
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES}/i18n/en-US ${MERGED_RESOURCES}/i18n/en-GB
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_ICON} ${MERGED_RESOURCES}/img/app_icon.jpg)

set(PLAYGUARD_COPY_SHADERS)
if (PLATFORM_SWITCH AND USE_DEKO3D)
    set(PLAYGUARD_COPY_SHADERS COMMAND ${CMAKE_COMMAND} -E copy_directory ${PLAYGUARD_DKSH_DIR} ${MERGED_RESOURCES}/shaders)
endif ()

if (PLATFORM_SWITCH)
    add_custom_target(${PROJECT_NAME}.nro ALL DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "PlayGuard" "${PROJECT_AUTHOR}"
                "${APP_VERSION}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        ${ASSEMBLE_RESOURCES}
        ${PLAYGUARD_COPY_SHADERS}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/font
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro
                --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp
                --romfsdir=${MERGED_RESOURCES}
    )
else ()
    add_custom_target(${PROJECT_NAME}_resources ALL ${ASSEMBLE_RESOURCES})
    add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_resources)
endif ()
