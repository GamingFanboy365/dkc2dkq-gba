# The DKC2 port.
#
# Host tools and the PC builds of the recompiled game:
#   cmake -S . -B build -DDKC2_ROM=baserom.sfc -DASAR=/path/to/asar
#   cmake --build build -j4
#
# The GBA build:
#   cmake -S . -B build-gba -DCMAKE_TOOLCHAIN_FILE=cmake/gba-toolchain.cmake \
#         -DDKC2_ROM=baserom.sfc -DASAR=/path/to/asar
#   cmake --build build-gba -j4     # -> build-gba/dkc2.gba
#
# DKC2_ROM is the player's own ROM (USA, Rev 1). The recompiled code is made
# from it at build time, and the cartridge carries it; neither is ever
# committed or distributed.
cmake_minimum_required(VERSION 3.16)
project(dkc2port C ASM)

set(DKC2_ROM "" CACHE FILEPATH "Your Donkey Kong Country 2 (USA, Rev 1) ROM")
set(ASAR "asar" CACHE FILEPATH "asar 1.9 or later")
option(DKC2_SYNC_LOG "GBA: print a hash of WRAM every frame, to compare with the reference" OFF)
set(DKC2_SHOTS "" CACHE STRING "GBA: game frames (waits) N,N,.. after which to print 'shot N' (for gbarun -t)")
set(DKC2_TEST_KEYS "" CACHE STRING "GBA: a KEYS script to play instead of the joypad (tools/common/tool_common.h)")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(NOT DKC2_ROM AND CMAKE_SYSTEM_NAME STREQUAL "Generic")
  message(FATAL_ERROR "The GBA build needs DKC2_ROM: your Donkey Kong Country 2 (USA, Rev 1) ROM")
endif()

if(DKC2_ROM)
  # The recompiled code, from the disassembly, the ROM and the traces.
  set(GEN ${CMAKE_BINARY_DIR}/gen)
  set(GEN_SOURCES)
  foreach(bank 80 B3 B4 B5 B6 B8 B9 BA BB BC BE)
    list(APPEND GEN_SOURCES ${GEN}/bank_${bank}.c)
  endforeach()
  list(APPEND GEN_SOURCES ${GEN}/entries.c)
  file(GLOB TRACES ${CMAKE_SOURCE_DIR}/traces/*.trace)
  file(GLOB ASM ${CMAKE_SOURCE_DIR}/*.asm)
  file(GLOB RECOMP ${CMAKE_SOURCE_DIR}/tools/recomp/*.py)
  set(TRACE_ARGS)
  foreach(t ${TRACES})
    list(APPEND TRACE_ARGS --trace ${t})
  endforeach()
  add_custom_command(
    OUTPUT ${GEN_SOURCES} ${GEN}/regions.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/recomp/recomp.py
            --root ${CMAKE_SOURCE_DIR} --rom ${DKC2_ROM} --asar ${ASAR} --out ${GEN}
            --probe-cache ${CMAKE_BINARY_DIR}/probe.json ${TRACE_ARGS}
    DEPENDS ${RECOMP} ${ASM} ${TRACES}
    COMMENT "Recompiling the game's code from the ROM")
  set(PORT_SOURCES src/runtime.c src/snes_hw.c src/game_loop.c)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
  # ---- the GBA ----
  set(GBA_FLAGS -mthumb -O2 -Wall -Wno-unused-label)
  add_executable(dkc2.elf gba/crt0.S gba/main.c gba/render.c gba/debug.c gba/libc.c ${PORT_SOURCES} ${GEN_SOURCES})
  target_include_directories(dkc2.elf PRIVATE gba/include gba src ${GEN})
  target_compile_options(dkc2.elf PRIVATE $<$<COMPILE_LANGUAGE:C>:${GBA_FLAGS}>)
  target_compile_definitions(dkc2.elf PRIVATE "EWRAM_DATA=__attribute__((section(\".ewram_bss\")))")
  if(DKC2_SYNC_LOG)
    target_compile_definitions(dkc2.elf PRIVATE SYNC_LOG)
  endif()
  if(DKC2_SHOTS)
    target_compile_definitions(dkc2.elf PRIVATE "SHOT_LOG=${DKC2_SHOTS}")
  endif()
  if(DKC2_TEST_KEYS)
    file(WRITE ${CMAKE_BINARY_DIR}/test_keys.txt "${DKC2_TEST_KEYS}")
    add_custom_command(
      OUTPUT ${GEN}/test_keys.h
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/keys_to_c.py ${CMAKE_BINARY_DIR}/test_keys.txt ${GEN}/test_keys.h
      DEPENDS ${CMAKE_BINARY_DIR}/test_keys.txt ${CMAKE_SOURCE_DIR}/tools/keys_to_c.py)
    target_sources(dkc2.elf PRIVATE ${GEN}/test_keys.h)
    target_compile_definitions(dkc2.elf PRIVATE TEST_KEYS)
  endif()
  # The C library mustn't turn its own loops into calls to itself.
  set_source_files_properties(gba/libc.c PROPERTIES COMPILE_OPTIONS "-fno-builtin")
  # The recompiled code: Thumb's pc-relative ranges are too short for its
  # biggest functions; ARM it is.
  set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "-w;-marm")
  # What the recompiled code calls into, ARM too, so the memory map's inline
  # functions inline everywhere (their hot parts are in IWRAM).
  set_source_files_properties(src/snes_hw.c src/runtime.c gba/render.c PROPERTIES COMPILE_OPTIONS "-marm")
  target_link_options(dkc2.elf PRIVATE -T ${CMAKE_SOURCE_DIR}/gba/gba.ld -Wl,-Map=${CMAKE_BINARY_DIR}/dkc2.map)
  set_target_properties(dkc2.elf PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/gba/gba.ld)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/dkc2-engine.gba
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:dkc2.elf> ${CMAKE_BINARY_DIR}/dkc2-engine.gba
    DEPENDS dkc2.elf)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/dkc2.gba
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/make_cartridge.py
            ${CMAKE_BINARY_DIR}/dkc2-engine.gba ${DKC2_ROM} ${CMAKE_BINARY_DIR}/dkc2.gba
    DEPENDS ${CMAKE_BINARY_DIR}/dkc2-engine.gba ${CMAKE_SOURCE_DIR}/tools/make_cartridge.py)
  add_custom_target(cartridge ALL DEPENDS ${CMAKE_BINARY_DIR}/dkc2.gba)
  return()
endif()

# ---- the host ----
find_package(PNG REQUIRED)

file(GLOB LAKESNES_SOURCES third_party/lakesnes/*.c)
add_library(lakesnes STATIC ${LAKESNES_SOURCES})
target_compile_options(lakesnes PRIVATE -O2 -w)
target_link_libraries(lakesnes PRIVATE m)

add_library(toolcommon STATIC tools/common/tool_common.c)
target_link_libraries(toolcommon PUBLIC PNG::PNG)

add_executable(snesrun tools/snesrun.c)
target_link_libraries(snesrun lakesnes toolcommon)

if(DKC2_ROM)
  # The recompiled code on LakeSnes's hardware.
  add_executable(hostrun host/hostrun.c src/runtime.c ${GEN_SOURCES})
  target_include_directories(hostrun PRIVATE src host ${GEN})
  set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1;-w")
  target_link_libraries(hostrun lakesnes toolcommon)

  # The same code on the port's own hardware model, the GBA's.
  add_library(gen_budget OBJECT ${GEN_SOURCES})
  target_include_directories(gen_budget PRIVATE src src/budget ${GEN})
  target_compile_definitions(gen_budget PRIVATE CHECK_ASSUMPTIONS)
  add_executable(hostmini host/hostmini.c ${PORT_SOURCES} $<TARGET_OBJECTS:gen_budget>)
  target_include_directories(hostmini PRIVATE src src/budget)
  target_compile_definitions(hostmini PRIVATE CHECK_ASSUMPTIONS)
  target_link_libraries(hostmini lakesnes toolcommon)
endif()
