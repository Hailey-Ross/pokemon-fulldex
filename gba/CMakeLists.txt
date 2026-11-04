# The GBA games for Pokemon Fulldex (its SSR_GBA_DIR): target gbawide, frontend/embed.h.
# mGBA at patches/mgba.commit with patches/mgba-wide.patch, our player, and the widescreen patches
# (wide-patches/prebuilt, made by wide-patches/build.sh) built in.
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.16)
project(gbawide C CXX)

set(GBAW_MGBA_DIR "" CACHE PATH "An mGBA checkout with patches/mgba-wide.patch already applied (empty: fetched)")
if(NOT GBAW_MGBA_DIR)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/patches/mgba.commit" GBAW_MGBA_COMMIT)
    string(STRIP "${GBAW_MGBA_COMMIT}" GBAW_MGBA_COMMIT)
    include(FetchContent)
    FetchContent_Declare(gbaw_mgba
        GIT_REPOSITORY https://github.com/mgba-emu/mgba.git
        GIT_TAG ${GBAW_MGBA_COMMIT}
        PATCH_COMMAND git apply "${CMAKE_CURRENT_SOURCE_DIR}/patches/mgba-wide.patch"
        UPDATE_DISCONNECTED ON)
    FetchContent_GetProperties(gbaw_mgba)
    if(NOT gbaw_mgba_POPULATED)
        FetchContent_Populate(gbaw_mgba)
    endif()
    set(GBAW_MGBA_DIR "${gbaw_mgba_SOURCE_DIR}")
endif()

# mGBA as a plain static library: no frontends, no optional libraries
foreach(opt LIBMGBA_ONLY BUILD_STATIC)
    set(${opt} ON CACHE BOOL "" FORCE)
endforeach()
foreach(opt BUILD_SHARED USE_FFMPEG USE_LIBZIP USE_MINIZIP USE_SQLITE3 USE_ELF USE_LUA USE_EDITLINE USE_EPOXY
        USE_DISCORD_RPC USE_JSON_C USE_LZMA USE_PNG USE_ZLIB ENABLE_SCRIPTING BUILD_QT BUILD_SDL BUILD_TEST
        BUILD_SUITE BUILD_CINEMA BUILD_EXAMPLE BUILD_PERF BUILD_GL BUILD_GLES2 BUILD_GLES3 BUILD_LIBRETRO)
    set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory("${GBAW_MGBA_DIR}" mgba EXCLUDE_FROM_ALL)

# Our widescreen patches and check lists, as data in the program
set(GBAW_EMBED "${CMAKE_CURRENT_BINARY_DIR}/gbaw_files.cpp")
file(GLOB GBAW_DATA "${CMAKE_CURRENT_SOURCE_DIR}/wide-patches/prebuilt/*.ips" "${CMAKE_CURRENT_SOURCE_DIR}/wide-patches/versions/*.check")
add_custom_command(OUTPUT "${GBAW_EMBED}"
    COMMAND ${CMAKE_COMMAND} -D "OUT=${GBAW_EMBED}" -D "FILES=${GBAW_DATA}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake"
    DEPENDS ${GBAW_DATA} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake"
    VERBATIM)

# Inside Fulldex (GBAW_EMBEDDED, set by its CMakeLists) the library; on its own also gbawide.exe (frontend/main.cpp).
option(GBAW_EMBEDDED "Built inside Pokemon Fulldex" OFF)
if(NOT GBAW_EMBEDDED)
    find_package(SDL2 REQUIRED)
endif()
# Fulldex's ui.h (the pause menu's look): Fulldex's own frontend folder, or the fork's beside this one
set(GBAW_FULLDEX_FRONTEND "${CMAKE_SOURCE_DIR}/frontend")
if(NOT EXISTS "${GBAW_FULLDEX_FRONTEND}/ui.h")
    set(GBAW_FULLDEX_FRONTEND "${CMAKE_CURRENT_SOURCE_DIR}/../frontend")
endif()
# A stand-alone Windows build links SDL2 and the C++ runtime in: one .exe to ship
set(GBAW_SDL SDL2::SDL2)
if(NOT GBAW_EMBEDDED AND WIN32 AND TARGET SDL2::SDL2-static)
    set(GBAW_SDL SDL2::SDL2-static)
endif()

# PokeDaisy's EXPAND (GPL-3.0, third_party/pokedaisy, unchanged): widescreen for what our patches don't fit
set(GBAW_PD "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pokedaisy")
set(GBAW_PD_SOURCES "${GBAW_PD}/pk_wide.c" "${GBAW_PD}/pk_wide_gb.c" "${GBAW_PD}/pk_cheats.c")
set(GBAW_PD_COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pokedaisy_compat.h")
if(MSVC)
    set_source_files_properties(${GBAW_PD_SOURCES} PROPERTIES COMPILE_OPTIONS "/w;/FI${GBAW_PD_COMPAT}")
else()
    set_source_files_properties(${GBAW_PD_SOURCES} PROPERTIES COMPILE_OPTIONS "-w;-include;${GBAW_PD_COMPAT}")
endif()
add_library(gbawide STATIC frontend/embed.cpp frontend/wide_rom.cpp frontend/box_art.cpp ${GBAW_PD_SOURCES} "${GBAW_EMBED}")
target_include_directories(gbawide PRIVATE "${GBAW_PD}")
if(WIN32 OR NOT UNIX)
    target_compile_definitions(gbawide PRIVATE PK_WIDE_NO_GUARD) # (its fault guard is POSIX signals)
endif()
# Box art downloads with Fulldex's own requests on every platform where they are linked in (inside Fulldex)
if(GBAW_EMBEDDED)
    target_compile_definitions(gbawide PRIVATE GBAW_FULLDEX_HTTP)
endif()
target_compile_features(gbawide PRIVATE cxx_std_17)
target_include_directories(gbawide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/frontend"
    PRIVATE "${GBAW_MGBA_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/mgba/include" "${GBAW_FULLDEX_FRONTEND}")
target_compile_definitions(gbawide PRIVATE BUILD_STATIC ENABLE_DIRECTORIES ENABLE_VFS ENABLE_VFS_FD M_CORE_GB M_CORE_GBA)
if(TARGET ${GBAW_SDL})
    target_link_libraries(gbawide PRIVATE ${GBAW_SDL})
else()
    target_include_directories(gbawide PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_STATIC_INCLUDE_DIRS})
endif()
target_link_libraries(gbawide PRIVATE mgba)
if(WIN32)
    target_link_libraries(gbawide PRIVATE ws2_32 iphlpapi shlwapi ole32 uuid winmm winhttp)
endif()

if(NOT GBAW_EMBEDDED)
    add_executable(gbawide-player frontend/main.cpp)
    set_target_properties(gbawide-player PROPERTIES OUTPUT_NAME gbawide)
    target_compile_features(gbawide-player PRIVATE cxx_std_17)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(gbawide-player PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(gbawide-player PRIVATE gbawide ${GBAW_SDL})
    # RetroAchievements with Fulldex's own code (gba_ra.h, achievements.cpp, rcheevos) when it is beside us
    if(EXISTS "${GBAW_FULLDEX_FRONTEND}/gba_ra.h" AND EXISTS "${GBAW_FULLDEX_FRONTEND}/thirdparty/rcheevos/include")
        file(GLOB_RECURSE GBAW_RC_SOURCES "${GBAW_FULLDEX_FRONTEND}/thirdparty/rcheevos/src/*.c")
        add_library(gbawide-rcheevos STATIC ${GBAW_RC_SOURCES})
        target_include_directories(gbawide-rcheevos PUBLIC "${GBAW_FULLDEX_FRONTEND}/thirdparty/rcheevos/include")
        target_sources(gbawide-player PRIVATE "${GBAW_FULLDEX_FRONTEND}/achievements.cpp"
            "${GBAW_FULLDEX_FRONTEND}/achievements_http.cpp")
        target_include_directories(gbawide-player PRIVATE "${GBAW_FULLDEX_FRONTEND}" "${GBAW_FULLDEX_FRONTEND}/../melonDS/src")
        set(GBAW_RA_VERSION "0.0.0")
        if(EXISTS "${GBAW_FULLDEX_FRONTEND}/../CHANGELOG.md")
            file(STRINGS "${GBAW_FULLDEX_FRONTEND}/../CHANGELOG.md" _gbaw_releases REGEX "^## v[0-9]")
            if(_gbaw_releases)
                list(GET _gbaw_releases 0 GBAW_RA_VERSION)
                string(REGEX REPLACE "^## v([0-9.]+).*" "\\1" GBAW_RA_VERSION "${GBAW_RA_VERSION}")
            endif()
        endif()
        target_compile_definitions(gbawide-player PRIVATE GBAW_RA SSR_RA_VERSION="${GBAW_RA_VERSION}")
        target_link_libraries(gbawide-player PRIVATE gbawide-rcheevos)
        target_compile_definitions(gbawide PRIVATE GBAW_FULLDEX_HTTP) # (achievements_http.cpp is linked in: box art uses it)
        if(WIN32)
            target_link_libraries(gbawide-player PRIVATE winhttp)
        elseif(UNIX AND NOT APPLE)
            target_link_libraries(gbawide-player PRIVATE ${CMAKE_DL_LIBS})
        endif()
    endif()
    if(WIN32)
        set_target_properties(gbawide-player PROPERTIES WIN32_EXECUTABLE ON)
        if(MINGW)
            target_link_options(gbawide-player PRIVATE -static -static-libgcc -static-libstdc++)
        endif()
    endif()
endif()
