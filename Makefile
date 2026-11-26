object = DKC2_custom.sfc
# The game's data isn't in this repository: the disassembly incbins it from
# the player's own ROM, Donkey Kong Country 2 (USA, Rev 1), copied here.
baserom = baserom.sfc
baserom_sha256 = b79c2bb86f6fc76e1fc61c62fc16d51c664c381e58bc2933be643bbc4d8b610c

all:
	@echo "$(baserom_sha256)  $(baserom)" | sha256sum -c --quiet - || \
		(echo "Copy your Donkey Kong Country 2 (USA, Rev 1) ROM to $(baserom)"; false)
	rm -f $(object)
	asar all.asm $(object)
	-@echo "$(baserom_sha256) $(object)" | sha256sum -c -
run:
	../../../lsnes/lsnes/lsnes --rom=DKC2_custom.sfc --lua=./DKC2.lua --unpause 2>/dev/null
clean:
	rm -f $(object)
