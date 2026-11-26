# The DKC2 port: the host tools and the host build of the recompiled game.
#
#   cmake -S . -B build -DDKC2_ROM=baserom.sfc -DASAR=/path/to/asar
#   cmake --build build -j4
#
# DKC2_ROM is the player's own ROM (USA, Rev 1): the recompiled code is made
# from it at build time and never committed.
cmake_minimum_required(VERSION 3.16)
project(dkc2port C)

set(DKC2_ROM "" CACHE FILEPATH "Your Donkey Kong Country 2 (USA, Rev 1) ROM")
set(ASAR "asar" CACHE FILEPATH "asar 1.9 or later")
find_package(PNG REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(GLOB LAKESNES_SOURCES third_party/lakesnes/*.c)
add_library(lakesnes STATIC ${LAKESNES_SOURCES})
target_compile_options(lakesnes PRIVATE -O2 -w)
target_link_libraries(lakesnes PRIVATE m)

add_library(toolcommon STATIC tools/common/tool_common.c)
target_link_libraries(toolcommon PUBLIC PNG::PNG)

add_executable(snesrun tools/snesrun.c)
target_link_libraries(snesrun lakesnes toolcommon)

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
  file(GLOB RECOMP tools/recomp/*.py)
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

  add_executable(hostrun host/hostrun.c src/runtime.c ${GEN_SOURCES})
  target_include_directories(hostrun PRIVATE src host ${GEN})
  set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1;-w")
  target_link_libraries(hostrun lakesnes toolcommon)
endif()
