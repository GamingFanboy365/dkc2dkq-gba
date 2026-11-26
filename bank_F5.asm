DATA_F50000:
if !version == 0
	db $32, $02, $93, $12
else
	db $02, $12, $93, $32
endif

DATA_F50004:
	incbin "baserom.sfc":$350004..$350147

DATA_F50147:
	incbin "baserom.sfc":$350147..$3502C7

DATA_F502C7:
	incbin "baserom.sfc":$3502C7..$350447

DATA_F50447:
	incbin "baserom.sfc":$350447..$3505C7

DATA_F505C7:
	incbin "baserom.sfc":$3505C7..$350747

DATA_F50747:
	incbin "baserom.sfc":$350747..$3508C7

DATA_F508C7:
	incbin "baserom.sfc":$3508C7..$350A47

DATA_F50A47:
	incbin "baserom.sfc":$350A47..$350BC7

DATA_F50BC7:
	incbin "baserom.sfc":$350BC7..$350D47

DATA_F50D47:
	incbin "baserom.sfc":$350D47..$350EC7

DATA_F50EC7:
	incbin "baserom.sfc":$350EC7..$351047

DATA_F51047:
	incbin "baserom.sfc":$351047..$3511C7

DATA_F511C7:
	incbin "baserom.sfc":$3511C7..$351347

DATA_F51347:
	incbin "baserom.sfc":$351347..$3514C7

DATA_F514C7:
	incbin "baserom.sfc":$3514C7..$351647

DATA_F51647:
	incbin "baserom.sfc":$351647..$3517C7

DATA_F517C7:
	incbin "baserom.sfc":$3517C7..$351907

DATA_F51907:
	incbin "baserom.sfc":$351907..$351A47

DATA_F51A47:
	incbin "baserom.sfc":$351A47..$351B87

DATA_F51B87:
	incbin "baserom.sfc":$351B87..$351CC7

DATA_F51CC7:
	incbin "baserom.sfc":$351CC7..$351D67

DATA_F51D67:
	incbin "baserom.sfc":$351D67..$351E07

DATA_F51E07:
	incbin "baserom.sfc":$351E07..$351EA7

DATA_F51EA7:
	incbin "baserom.sfc":$351EA7..$351F47

DATA_F51F47:
	incbin "baserom.sfc":$351F47..$351FE7

DATA_F51FE7:
	incbin "baserom.sfc":$351FE7..$352BA7

DATA_F52BA7:
	incbin "baserom.sfc":$352BA7..$352FC7

DATA_F52FC7:
	incbin "baserom.sfc":$352FC7..$35325B

DATA_F5325B:
	incbin "baserom.sfc":$35325B..$3547BC

DATA_F547BC:
	incbin "baserom.sfc":$3547BC..$35484A

DATA_F5484A:
	incbin "baserom.sfc":$35484A..$354BCA

DATA_F54BCA:
	incbin "baserom.sfc":$354BCA..$354F4A

DATA_F54F4A:
	incbin "baserom.sfc":$354F4A..$3552CA

DATA_F552CA:
	incbin "baserom.sfc":$3552CA..$35564A

DATA_F5564A:
	incbin "baserom.sfc":$35564A..$3559CA

DATA_F559CA:
	incbin "baserom.sfc":$3559CA..$355D4A

DATA_F55D4A:
	incbin "baserom.sfc":$355D4A..$356AC9

DATA_F56AC9:
	incbin "baserom.sfc":$356AC9..$356C13

DATA_F56C13:
	incbin "baserom.sfc":$356C13..$356E33

DATA_F56E33:
	incbin "baserom.sfc":$356E33..$357053

DATA_F57053:
	incbin "baserom.sfc":$357053..$357273

DATA_F57273:
	incbin "baserom.sfc":$357273..$357493

DATA_F57493:
	incbin "baserom.sfc":$357493..$3576B3

DATA_F576B3:
	incbin "baserom.sfc":$3576B3..$3578D3

DATA_F578D3:
	incbin "baserom.sfc":$3578D3..$357AF3

DATA_F57AF3:
	incbin "baserom.sfc":$357AF3..$357D13

DATA_F57D13:
	incbin "baserom.sfc":$357D13..$358000
