cmake_minimum_required(VERSION 3.20)
project(STT2001Recompiled LANGUAGES C CXX)
set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../reference/psxrecomp" CACHE PATH "PSXRecomp source checkout")
get_filename_component(PSXRECOMP_ROOT "${PSXRECOMP_ROOT}" REALPATH)
set(PSX_RECOMP_UI OFF CACHE BOOL "Minimal P0 runtime")
set(PSX_REWIND OFF CACHE BOOL "P0 does not need rewind")
include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")
psxrecomp_add_game_runtime(psx-runtime
    PRELOADED_MODS_DIR "NONE"
    WINDOW_TITLE "STT2001 P0 Japanese baseline"
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    MAX_PLAYERS 1
    GEN_MARKER "generated/SLPS_028.63_dispatch.c"
    GEN_FULL_GLOB "generated/SLPS_028.63_full_*.c"
)
add_custom_command(TARGET psx-runtime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/game.toml" "$<TARGET_FILE_DIR:psx-runtime>/game.toml")
option(STT_P1 "Build the native Chinese dialogue runtime" OFF)
if(STT_P1)
    target_sources(psx-runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/p1/native_caption.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/p1/pc_controls.cpp")
    target_link_libraries(psx-runtime PRIVATE gdi32 opengl32)
    set_target_properties(psx-runtime PROPERTIES OUTPUT_NAME "STT2001_PC_Chinese_ShortText")
    add_custom_command(TARGET psx-runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/config.ini" "$<TARGET_FILE_DIR:psx-runtime>/config.ini"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/stt2001.cht" "$<TARGET_FILE_DIR:psx-runtime>/stt2001.cht"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/p1/data"
            "$<TARGET_FILE_DIR:psx-runtime>/pc-data-v6")
    # Earlier executables retain their own data instead of being upgraded in place.
    if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/pc-data/dialogues.bin")
        add_custom_command(TARGET psx-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:psx-runtime>/pc-data"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/p1/data/dialogues-v3.bin" "$<TARGET_FILE_DIR:psx-runtime>/pc-data/dialogues.bin")
    endif()
    if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/p1-data/dialogues.bin")
        add_custom_command(TARGET psx-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:psx-runtime>/p1-data"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/p1/data/dialogues-legacy.bin" "$<TARGET_FILE_DIR:psx-runtime>/p1-data/dialogues.bin")
    endif()
endif()
