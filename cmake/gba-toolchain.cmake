# The GBA: clang and lld only, no sysroot (gba/ has the startup code, the
# linker script and the little C library the port needs).
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER clang)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET armv4t-none-eabi)
set(CMAKE_ASM_COMPILER_TARGET armv4t-none-eabi)
set(CMAKE_C_FLAGS_INIT "-mcpu=arm7tdmi -ffreestanding -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=arm7tdmi")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib -fuse-ld=lld -Wl,--gc-sections")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
find_program(CMAKE_OBJCOPY llvm-objcopy REQUIRED)
