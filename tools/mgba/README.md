# gbarun: headless GBA runner (libmgba)

`gbarun.c` runs a GBA ROM with no window and no audio. It plays back a scripted
joypad sequence, writes PNG screenshots at chosen frames, and can load and save
savestates. It can also dump work RAM. Use it to check a build (or a reference
cartridge) without a display. The full option list is in the comment at the top of `gbarun.c`.

## Build libmgba (once)

Needs cmake, a C compiler, libpng, zlib and freetype dev packages
(`apt-get install cmake build-essential libpng-dev zlib1g-dev libfreetype-dev`).

```sh
git clone --depth 1 https://github.com/mgba-emu/mgba.git "$MGBA"
cmake -S "$MGBA" -B "$MGBA/build" -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_STATIC=ON -DBUILD_SHARED=OFF -DSKIP_FRONTEND=ON \
  -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_LIBRETRO=OFF -DBUILD_GL=OFF \
  -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DUSE_FFMPEG=OFF -DUSE_EPOXY=OFF \
  -DUSE_SQLITE3=OFF -DUSE_ELF=OFF -DUSE_LUA=OFF -DUSE_DISCORD_RPC=OFF \
  -DUSE_LIBZIP=OFF -DUSE_MINIZIP=OFF -DUSE_LZMA=OFF -DUSE_EDITLINE=OFF
make -C "$MGBA/build" -j4 mgba        # -> $MGBA/build/libmgba.a
```

## Build gbarun

```sh
gcc -O2 -Wall -DENABLE_DIRECTORIES \
  -I"$MGBA/include" -I"$MGBA/build/include" \
  tools/mgba/gbarun.c "$MGBA/build/libmgba.a" \
  -lpng -lz -lfreetype -lpthread -lm -o gbarun
```

`-DENABLE_DIRECTORIES` is required. libmgba is built with it, but the generated
`mgba/flags.h` does not define it. Without it, `struct mCore` has a different
layout in gbarun than in the library, and gbarun crashes at startup.

## Use

```sh
# Boot, save a state at frame 1400, take a screenshot every 60 frames
./gbarun -n 1400 -s 60-1400/60 -o out -w title.ss game.gba

# Resume from that state, press Start, hold Right+B for 2 s, then shoot
./gbarun -l title.ss -k "10:-,5:START,120:RIGHT+B" -s 0-135/15 -o out2 game.gba

# The same, set through environment variables
STATE=title.ss KEYS="10:-,5:START" SHOTS=30,60 OUT=out3 ./gbarun game.gba
```

- Key script: `COUNT:KEYS` segments separated by commas. `KEYS` is `-` for no
  keys, or names joined with `+`: `A B L R START SELECT UP DOWN LEFT RIGHT`.
  When the script ends, no keys are held. Without `-n`, the run stops when the
  script ends (600 frames if there is no script). If you want shots after the
  last keypress, pass `-n`.
- Shot list: frame numbers or ranges `FIRST-LAST/STEP`. Frames count from the
  start of this run, also when a state was loaded. Files are written as
  `OUT/PREFIX_%06d.png`.
- `-w FILE` writes a savestate at the end of the run, or after frame `-W N`.
- `-l FILE` loads a savestate before the run starts.
- `-m` dumps work RAM at every shot frame: EWRAM (256 KiB) and IWRAM (32 KiB),
  as `.ewram` and `.iwram` files. Diff the dumps to find variables.
- `-S FILE` loads a battery save. The save is kept in memory and the file is
  never written.

The run is deterministic: the same ROM, state and key script always give
identical frames.
