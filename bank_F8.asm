DATA_F80000:
	incbin "baserom.sfc":$380000..$38063E

DATA_F8063E:
	incbin "baserom.sfc":$38063E..$380D10

DATA_F80D10:
	incbin "baserom.sfc":$380D10..$380D66

nintendo_copyright_tilemap:
	incbin "baserom.sfc":$380D66..$380FA6

nintendo_copyright_tiledata:
	incbin "baserom.sfc":$380FA6..$382106

DATA_F82106:
	incbin "baserom.sfc":$382106..$388000
