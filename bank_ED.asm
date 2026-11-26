DATA_ED0000:
	incbin "baserom.sfc":$2D0000..$2D02A1

title_screen_tilemap:
	incbin "baserom.sfc":$2D02A1..$2D0997

title_screen_tiledata:
	incbin "baserom.sfc":$2D0997..$2D57EF

DATA_ED57EF:
	incbin "baserom.sfc":$2D57EF..$2D5E3F

DATA_ED5E3F:
	incbin "baserom.sfc":$2D5E3F..$2D7429

DATA_ED7429:
	db $01, $04, $4F, $0C, $50, $0C, $51, $0C
	db $52, $0C

DATA_ED7433:
	db $01, $04, $53, $0C, $54, $0C, $55, $0C
	db $56, $0C

DATA_ED743D:
	db $01, $04, $53, $0C, $54, $0C, $57, $0C
	db $58, $0C

DATA_ED7447:
	dw DATA_ED744D
	dw DATA_ED748B
	dw DATA_ED74C9

DATA_ED744D:
	incbin "baserom.sfc":$2D744D..$2D748B

DATA_ED748B:
	incbin "baserom.sfc":$2D748B..$2D74C9

DATA_ED74C9:
	incbin "baserom.sfc":$2D74C9..$2D7507

DATA_ED7507:
	incbin "baserom.sfc":$2D7507..$2D7569

DATA_ED7569:
	incbin "baserom.sfc":$2D7569..$2D7607

DATA_ED7607:
	incbin "baserom.sfc":$2D7607..$2D7639

DATA_ED7639:
	dw DATA_ED763F
	dw DATA_ED7687
	dw DATA_ED76CF

DATA_ED763F:
	incbin "baserom.sfc":$2D763F..$2D7687

DATA_ED7687:
	incbin "baserom.sfc":$2D7687..$2D76CF

DATA_ED76CF:
	incbin "baserom.sfc":$2D76CF..$2D7717

DATA_ED7717:
	incbin "baserom.sfc":$2D7717..$2D783B

DATA_ED783B:
	dw DATA_ED7855
	dw DATA_ED7859
	dw DATA_ED785D
	dw DATA_ED7861
	dw DATA_ED7865
	dw DATA_ED7869
	dw DATA_ED786D
	dw DATA_ED7871
	dw DATA_ED7875
	dw DATA_ED7879
	dw DATA_ED787D
	dw DATA_ED7881
	dw DATA_ED7885

DATA_ED7855:
	db $A7, $0C, $A8, $0C

DATA_ED7859:
	db $A9, $0C, $AA, $0C

DATA_ED785D:
	db $AB, $0C, $AC, $0C

DATA_ED7861:
	db $AD, $0C, $AE, $0C

DATA_ED7865:
	db $AF, $0C, $B0, $0C

DATA_ED7869:
	db $B1, $0C, $B2, $0C

DATA_ED786D:
	db $B3, $0C, $B4, $0C

DATA_ED7871:
	db $B5, $0C, $B6, $0C

DATA_ED7875:
	db $B7, $0C, $B8, $0C

DATA_ED7879:
	db $B9, $0C, $BA, $0C

DATA_ED787D:
	db $59, $0C, $59, $8C

DATA_ED7881:
	db $00, $0C, $5C, $0C

DATA_ED7885:
	db $00, $00, $00, $00

DATA_ED7889:
	db $02, $02, $5A, $0C, $5B, $0C, $5D, $0C
	db $5E, $0C


DATA_ED7893:
	dw DATA_ED78B5
	dw DATA_ED7897

DATA_ED7897:
	incbin "baserom.sfc":$2D7897..$2D78B5

DATA_ED78B5:
	incbin "baserom.sfc":$2D78B5..$2D78D3

DATA_ED78D3:
	incbin "baserom.sfc":$2D78D3..$2D8415

DATA_ED8415:
	incbin "baserom.sfc":$2D8415..$2D8584

DATA_ED8584:
	incbin "baserom.sfc":$2D8584..$2D9179

DATA_ED9179:
	incbin "baserom.sfc":$2D9179..$2D9C9A

DATA_ED9C9A:
	incbin "baserom.sfc":$2D9C9A..$2D9E72

DATA_ED9E72:
	incbin "baserom.sfc":$2D9E72..$2DA030

DATA_EDA030:
	incbin "baserom.sfc":$2DA030..$2DA1AB

DATA_EDA1AB:
	incbin "baserom.sfc":$2DA1AB..$2DE455

DATA_EDE455:
	incbin "baserom.sfc":$2DE455..$2DE53C

DATA_EDE53C:
	incbin "baserom.sfc":$2DE53C..$2DE653

DATA_EDE653:
	incbin "baserom.sfc":$2DE653..$2E0000
