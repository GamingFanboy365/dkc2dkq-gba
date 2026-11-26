spc_base_engine:

arch spc700
base $04D8

CODE_04D8:
	CLRP					;$04D8   |
	MOV X, #$FF				;$04D9   |
	MOV SP, X				;$04DB   |
	INC X					;$04DC   |
	MOV $F4, X				;$04DD   |
	INC X					;$04DF   |
	MOV $E9, X				;$04E0   |
	MOV A, #$00				;$04E2   |
	MOV $00, A				;$04E4   |
	MOV $01, #$D0				;$04E6   |
CODE_04E9:					;   |
	MOV Y, A				;$04E9   |
CODE_04EA:					;   |
	MOV ($00)+Y, A				;$04EA   |
	INC Y					;$04EC   |
	BNE CODE_04EA				;$04ED   |
	INC $01					;$04EF   |
	BNE CODE_04E9				;$04F1   |
CODE_04F3:					;   |
	MOV X, #$FF				;$04F3   |
	MOV $F2, #$6C				;$04F5   |
	MOV $F3, X				;$04F8   |
	MOV $F2, #$7D				;$04FA   |
	MOV $F3, #$00				;$04FD   |
	MOV $F2, #$6D				;$0500   |
	MOV $F3, X				;$0503   |
	MOV $04B7, X				;$0505   |
	MOV X, $E9				;$0508   |
CODE_050A:					;   |
	CMP X, $F4				;$050A   |
	BNE CODE_050A				;$050C   |
	MOVW YA, $F5				;$050E   |
	MOV $F4, X				;$0510   |
	INC X					;$0512   |
	MOV $0539, A				;$0513   |
	MOV $0542, A				;$0516   |
	MOV $053A, Y				;$0519   |
	MOV $0543, Y				;$051C   |
CODE_051F:					;   |
	CMP X, $F4				;$051F   |
	BNE CODE_051F				;$0521   |
	MOVW YA, $F5				;$0523   |
	MOV $F4, X				;$0525   |
	INC X					;$0527   |
	MOV $EA, A				;$0528   |
	MOV $EB, Y				;$052A   |
	DECW $EA				;$052C   |
	BMI CODE_0556				;$052E   |
	MOV Y, #$00				;$0530   |
CODE_0532:					;   |
	CMP X, $F4				;$0532   |
	BNE CODE_0532				;$0534   |
	MOV A, $F5				;$0536   |
DATA_0538:					;   |
	MOV $0000+Y, A				;$0538   |
	MOV A, $F6				;$053B   |
	MOV $F4, X				;$053D   |
	INC X					;$053F   |
	INC Y					;$0540   |
	MOV $0000+Y, A				;$0541   |
	INC Y					;$0544   |
	BEQ CODE_054E				;$0545   |
CODE_0547:					;   |
	DECW $EA				;$0547   |
	BPL CODE_0532				;$0549   |
	JMP CODE_050A				;$054B   |

CODE_054E:
	INC $053A				;$054E   |
	INC $0543				;$0551   |
	BRA CODE_0547				;$0554   |

CODE_0556:
	MOV $E9, X				;$0556   |
	MOV X, #$00				;$0558   |
	JMP (DATA_0538+1+X)			;$055A   |

	db $00
	dw $D604

base off
arch 65816

spc_sound_engine:
arch spc700
	incbin "baserom.sfc":$2E0088..$2E0188

base $0660
CODE_0660:
	MOV A, $055D				;$0660   |
	ASL A					;$0663   |
	MOV Y, A				;$0664   |
	MOV A, $1312+Y				;$0665   |
	MOV $E5, A				;$0668   |
	MOV A, $1313+Y				;$066A   |
	MOV $E6, A				;$066D   |
	JMP CODE_0678				;$066F   |

CODE_0672:
	MOV $E6, #$13				;$0672   |
	MOV $E5, #$00				;$0675   |
CODE_0678:					;   |
	CALL CODE_100B				;$0678   |
	MOV A, #$00				;$067B   |
	MOV $1C, A				;$067D   |
	MOV $1D, A				;$067F   |
	MOV $F1, A				;$0681   |
CODE_0683:					;   |
	MOV A, $E9				;$0683   |
	CMP A, $F4				;$0685   |
	BEQ CODE_068C				;$0687   |
	JMP CODE_0781				;$0689   |

CODE_068C:
	MOV X, $F6				;$068C   |
	MOV $055D, X				;$068E   |
	MOV X, $F5				;$0691   |
	MOV $F4, A				;$0693   |
	INC A					;$0695   |
	MOV $E9, A				;$0696   |
	MOV A, X				;$0698   |
	CMP A, #$80				;$0699   |
	BPL CODE_06A0				;$069B   |
	JMP CODE_0773				;$069D   |

CODE_06A0:
	AND A, #$07				;$06A0   |
	ASL A					;$06A2   |
	MOV X, A				;$06A3   |
	JMP (DATA_06A7+X)			;$06A4   |

DATA_06A7:
	dw CODE_070A
	dw CODE_0702
	dw CODE_06FA
	dw CODE_06B7
	dw CODE_0739
	dw CODE_0712
	dw CODE_077B
	dw CODE_07DB

CODE_06B7:
	MOV X, #$7F				;$06B7   |
CODE_06B9:					;   |
	MOV A, #$71				;$06B9   |
CODE_06BB:					;   |
	MOV Y, A				;$06BB   |
	MOV $F2, Y				;$06BC   |
	CALL CODE_06EB				;$06BE   |
	DEC Y					;$06C1   |
	MOV $F2, Y				;$06C2   |
	CALL CODE_06EB				;$06C4   |
	MOV A, Y				;$06C7   |
	SETC					;$06C8   |
	SBC A, #$0F				;$06C9   |
	BPL CODE_06BB				;$06CB   |
	MOV $F2, #$0C				;$06CD   |
	CALL CODE_06EB				;$06D0   |
	MOV $F2, #$1C				;$06D3   |
	CALL CODE_06EB				;$06D6   |
	MOV $F2, #$2C				;$06D9   |
	CALL CODE_06EB				;$06DC   |
	MOV $F2, #$3C				;$06DF   |
	CALL CODE_06EB				;$06E2   |
	DEC X					;$06E5   |
	BNE CODE_06B9				;$06E6   |
	JMP CODE_0660				;$06E8   |

CODE_06EB:
	MOV A, $F3				;$06EB   |
	BEQ CODE_06F7				;$06ED   |
	BMI CODE_06F5				;$06EF   |
	DEC A					;$06F1   |
	DEC A					;$06F2   |
	BRA CODE_06F7				;$06F3   |

CODE_06F5:
	INC A					;$06F5   |
	INC A					;$06F6   |
CODE_06F7:					;   |
	MOV $F3, A				;$06F7   |
	RET					;$06F9   |

CODE_06FA:					;   |
	MOV A, $055D				;$06FA   |
	MOV $1D, A				;$06FD   |
	JMP CODE_0781				;$06FF   |

CODE_0702:					;   |
	MOV A, $055D				;$0702   |
	MOV $E7, A				;$0705   |
	JMP CODE_0781				;$0707   |

CODE_070A:					;   |
	MOV A, $055D				;$070A   |
	MOV $E8, A				;$070D   |
	JMP CODE_0781				;$070F   |

CODE_0712:					;   |
	MOV A, $04B6				;$0712   |
	PUSH A					;$0715   |
	PUSH X					;$0716   |
	MOV X, #$05				;$0717   |
	MOV A, $055D				;$0719   |
	MOV $04B6, A				;$071C   |
	MOV $F2, #$50				;$071F   |
	MOV A, $F3				;$0722   |
	CALL CODE_0C59				;$0724   |
	MOV $F3, A				;$0727   |
	INC $F2					;$0729   |
	MOV A, $F3				;$072B   |
	CALL CODE_0C59				;$072D   |
	MOV $F3, A				;$0730   |
	POP X					;$0732   |
	POP A					;$0733   |
	MOV $04B6, A				;$0734   |
	BRA CODE_0781				;$0737   |

CODE_0739:					;   |
	MOV A, $055D				;$0739   |
	BMI CODE_074A				;$073C   |
	CLRC					;$073E   |
	MOV A, $055D				;$073F   |
	MOV $EC, A				;$0742   |
	MOV A, #$00				;$0744   |
	MOV $ED, A				;$0746   |
	BRA CODE_0756				;$0748   |

CODE_074A:
	MOV $055D, A				;$074A   |
	MOV A, $055D				;$074D   |
	MOV $EC, A				;$0750   |
	MOV A, #$FF				;$0752   |
	MOV $ED, A				;$0754   |
CODE_0756:					;   |
	MOVW YA, $EC				;$0756   |
	ADDW YA, $EC				;$0758   |
	ADDW YA, $EC				;$075A   |
	ADDW YA, $EC				;$075C   |
	ADDW YA, $EC				;$075E   |
	ADDW YA, $EC				;$0760   |
	ADDW YA, $EC				;$0762   |
	ADDW YA, $EC				;$0764   |
	MOVW $EC, YA				;$0766   |
	MOV $F2, #$4D				;$0768   |
	MOV A, $F3				;$076B   |
	AND A, #$DF				;$076D   |
	MOV $F3, A				;$076F   |
	BRA CODE_0781				;$0771   |

CODE_0773:					;   |
	MOV X, $055D				;$0773   |
	CALL CODE_10F7				;$0776   |
	BRA CODE_078E				;$0779   |

CODE_077B:					;   |
	MOV $1C, #$01				;$077B   |
	MOV $F1, #$00				;$077E   |

CODE_0781:
	MOV A, $1C				;$0781   |
	BNE CODE_0788				;$0783   |
	JMP CODE_0683				;$0785   |

CODE_0788:
	MOV ($FA), ($E4)			;$0788   |
	MOV $F1, #$01				;$078B   |
CODE_078E:					;   |
	MOV A, $FD				;$078E   |
	BEQ CODE_078E				;$0790   |
	MOV $F1, #$01				;$0792   |
	MOV $20, #$00				;$0795   |
	CLRC					;$0798   |
	ADC ($1E), ($1F)			;$0799   |
	ROR $20					;$079C   |
	MOV $23, #$00				;$079E   |
	CLRC					;$07A1   |
	ADC ($21), ($22)			;$07A2   |
	ROR $23					;$07A5   |
	MOV X, #$00				;$07A7   |
CODE_07A9:					;   |
	MOV A, $20				;$07A9   |
	BNE CODE_07B2				;$07AB   |
	CALL CODE_09BC				;$07AD   |
	BRA CODE_07B7				;$07B0   |

CODE_07B2:
	CALL CODE_0813				;$07B2   |
	BNE CODE_07B2				;$07B5   |
CODE_07B7:					;   |
	MOV A, $01E0+X				;$07B7   |
	BEQ CODE_07D0				;$07BA   |
	PUSH X					;$07BC   |
	MOV A, X				;$07BD   |
	OR A, #$08				;$07BE   |
	MOV X, A				;$07C0   |
	MOV A, $23				;$07C1   |
	BNE CODE_07CA				;$07C3   |
	CALL CODE_09BC				;$07C5   |
	BRA CODE_07CF				;$07C8   |

CODE_07CA:
	CALL CODE_0813				;$07CA   |
	BNE CODE_07CA				;$07CD   |
CODE_07CF:					;   |
	POP X					;$07CF   |
CODE_07D0:					;   |
	INC X					;$07D0   |
	CMP X, #$08				;$07D1   |
	BEQ CODE_07D8				;$07D3   |
	JMP CODE_07A9				;$07D5   |

CODE_07D8:
	JMP CODE_0683				;$07D8   |

CODE_07DB:
	MOV A, $055D				;$07DB   |
	BEQ CODE_07E3				;$07DE   |
	JMP CODE_04F3				;$07E0   |

CODE_07E3:
	MOV $F2, #$5C				;$07E3   |
	MOV $F3, #$FF				;$07E6   |
	MOV $F1, #$00				;$07E9   |
	MOV $FB, #$C8				;$07EC   |
	MOV $F1, #$02				;$07EF   |
CODE_07F2:					;   |
	MOV A, $FE				;$07F2   |
	BEQ CODE_07F2				;$07F4   |
	MOV $F2, #$6C				;$07F6   |
	MOV $F3, #$A0				;$07F9   |
	MOV X, #$00				;$07FC   |
	MOV $F2, #$4D				;$07FE   |
	MOV $F3, X				;$0801   |
	MOV $F2, #$2C				;$0803   |
	MOV $F3, X				;$0806   |
	MOV $F2, #$3C				;$0808   |
	MOV $F3, X				;$080B   |
	CALL CODE_100B				;$080D   |
	JMP CODE_04F3				;$0810   |

CODE_0813:
	MOV A, $0110+X				;$0813   |
	BNE CODE_081B				;$0816   |
	MOV A, #$00				;$0818   |
	RET					;$081A   |

CODE_081B:
	DEC $34+X				;$081B   |
	MOV A, $34+X				;$081D   |
	CMP A, #$01				;$081F   |
	BEQ CODE_0839				;$0821   |
	CMP A, #$FF				;$0823   |
	BNE CODE_082F				;$0825   |
	MOV A, $24+X				;$0827   |
	BEQ CODE_0850				;$0829   |
	DEC $24+X				;$082B   |
	BRA CODE_084A				;$082D   |

CODE_082F:
	CMP A, #$00				;$082F   |
	BNE CODE_084A				;$0831   |
	MOV A, $24+X				;$0833   |
	BEQ CODE_0850				;$0835   |
	BRA CODE_084A				;$0837   |

CODE_0839:
	MOV A, $24+X				;$0839   |
	BNE CODE_084A				;$083B   |
	MOV A, $01E0+X				;$083D   |
	BNE CODE_084A				;$0840   |
	MOV A, $0F95+X				;$0842   |
	MOV $F2, #$5C				;$0845   |
	MOV $F3, A				;$0848   |
CODE_084A:					;   |
	CALL CODE_09BC				;$084A   |
	MOV A, #$00				;$084D   |
	RET					;$084F   |

CODE_0850:
	MOV A, $44+X				;$0850   |
	MOV Y, $54+X				;$0852   |
	MOVW $00, YA				;$0854   |
	MOV Y, #$00				;$0856   |
	MOV A, ($00)+Y				;$0858   |
	BMI CODE_0862				;$085A   |
	PUSH X					;$085C   |
	ASL A					;$085D   |
	MOV X, A				;$085E   |
	JMP (DATA_0FA5+X)			;$085F   |

CODE_0862:
	CALL CODE_0867				;$0862   |
	BRA CODE_084A				;$0865   |

CODE_0867:
	CMP A, #$80				;$0867   |
	BNE CODE_088B				;$0869   |
	MOV A, $01E0+X				;$086B   |
	BNE CODE_0888				;$086E   |
	MOV A, $0F95+X				;$0870   |
	MOV $F2, #$5C				;$0873   |
	MOV $F3, A				;$0876   |
	MOV A, X				;$0878   |
	AND A, #$07				;$0879   |
	XCN A					;$087B   |
	OR A, #$02				;$087C   |
	MOV $F2, A				;$087E   |
	MOV A, #$00				;$0880   |
	MOV $F3, A				;$0882   |
	INC $F2					;$0884   |
	MOV $F3, A				;$0886   |
CODE_0888:					;   |
	JMP CODE_0983				;$0888   |

CODE_088B:
	CMP A, #$E0				;$088B   |
	BMI CODE_0899				;$088D   |
	CMP A, #$E1				;$088F   |
	BEQ CODE_0897				;$0891   |
	MOV A, $0C+X				;$0893   |
	BRA CODE_0899				;$0895   |

CODE_0897:
	MOV A, $14+X				;$0897   |
CODE_0899:					;   |
	CLRC					;$0899   |
	ADC A, #$24				;$089A   |
	ADC A, $0140+X				;$089C   |
	ASL A					;$089F   |
	PUSH X					;$08A0   |
	MOV Y, $64+X				;$08A1   |
	BEQ CODE_08DF				;$08A3   |
	MOV X, A				;$08A5   |
	MOV $04, Y				;$08A6   |
	MOV A, Y				;$08A8   |
	BPL CODE_08AE				;$08A9   |
	EOR A, #$FF				;$08AB   |
	INC A					;$08AD   |
CODE_08AE:					;   |
	MOV Y, A				;$08AE   |
	PUSH Y					;$08AF   |
	MOV A, $1199+X				;$08B0   |
	MUL YA					;$08B3   |
	MOV $02, Y				;$08B4   |
	MOV $03, #$00				;$08B6   |
	POP Y					;$08B9   |
	MOV A, $119A+X				;$08BA   |
	MUL YA					;$08BD   |
	ADDW YA, $02				;$08BE   |
	MOV $03, Y				;$08C0   |
	LSR $03					;$08C2   |
	ROR A					;$08C4   |
	LSR $03					;$08C5   |
	ROR A					;$08C7   |
	MOV $02, A				;$08C8   |
	MOV A, $119A+X				;$08CA   |
	MOV Y, A				;$08CD   |
	MOV A, $1199+X				;$08CE   |
	MOV X, $04				;$08D1   |
	BMI CODE_08D9				;$08D3   |
	ADDW YA, $02				;$08D5   |
	BRA CODE_08DB				;$08D7   |

CODE_08D9:
	SUBW YA, $02				;$08D9   |
CODE_08DB:					;   |
	MOVW $02, YA				;$08DB   |
	BRA CODE_08EA				;$08DD   |

CODE_08DF:
	MOV X, A				;$08DF   |
	MOV A, $1199+X				;$08E0   |
	MOV $02, A				;$08E3   |
	MOV A, $119A+X				;$08E5   |
	MOV $03, A				;$08E8   |
CODE_08EA:					;   |
	POP A					;$08EA   |
	MOV X, A				;$08EB   |
	AND A, #$07				;$08EC   |
	XCN A					;$08EE   |
	MOV $F2, A				;$08EF   |
	MOV A, $01E0+X				;$08F1   |
	BEQ CODE_08F9				;$08F4   |
	JMP CODE_0983				;$08F6   |

CODE_08F9:
	MOV A, $0254+X				;$08F9   |
	CALL CODE_0C59				;$08FC   |
	MOV $F3, A				;$08FF   |
	INC $F2					;$0901   |
	MOV A, $0264+X				;$0903   |
	CALL CODE_0C59				;$0906   |
	MOV $F3, A				;$0909   |
	INC $F2					;$090B   |
	MOV A, $0150+X				;$090D   |
	AND A, #$01				;$0910   |
	BEQ CODE_092B				;$0912   |
	MOV A, $0160+X				;$0914   |
	MOV $01A0+X, A				;$0917   |
	MOV A, $0170+X				;$091A   |
	MOV $0100+X, A				;$091D   |
	MOV A, $0180+X				;$0920   |
	MOV $94+X, A				;$0923   |
	MOV A, $0190+X				;$0925   |
	MOV $01C0+X, A				;$0928   |
CODE_092B:					;   |
	MOV A, $0150+X				;$092B   |
	AND A, #$02				;$092E   |
	BEQ CODE_094D				;$0930   |
	MOV A, $0234+X				;$0932   |
	BPL CODE_093D				;$0935   |
	EOR A, #$FF				;$0937   |
	INC A					;$0939   |
	MOV $0234+X, A				;$093A   |
CODE_093D:					;   |
	MOV A, $0200+X				;$093D   |
	LSR A					;$0940   |
	MOV $A4+X, A				;$0941   |
	MOV A, $0210+X				;$0943   |
	MOV $B4+X, A				;$0946   |
	MOV A, $0220+X				;$0948   |
	MOV $C4+X, A				;$094B   |
CODE_094D:					;   |
	MOV A, $02				;$094D   |
	MOV $84+X, A				;$094F   |
	MOV $F3, A				;$0951   |
	INC $F2					;$0953   |
	MOV A, $03				;$0955   |
	MOV $74+X, A				;$0957   |
	MOV $F3, A				;$0959   |
	INC $F2					;$095B   |
	MOV A, $0244+X				;$095D   |
	MOV $F3, A				;$0960   |
	INC $F2					;$0962   |
	MOV A, $0274+X				;$0964   |
	MOV $F3, A				;$0967   |
	INC $F2					;$0969   |
	MOV A, $0284+X				;$096B   |
	MOV $F3, A				;$096E   |
	INC $F2					;$0970   |
	MOV $F3, #$7F				;$0972   |
	MOV $F2, #$5C				;$0975   |
	MOV $F3, #$00				;$0978   |
	MOV $F2, #$4C				;$097B   |
	MOV A, $0F95+X				;$097E   |
	MOV $F3, A				;$0981   |
CODE_0983:					;   |
	MOV A, $0120+X				;$0983   |
	BEQ CODE_0997				;$0986   |
	MOV $00, #$01				;$0988   |
	MOV A, $0120+X				;$098B   |
	MOV $34+X, A				;$098E   |
	MOV A, $0130+X				;$0990   |
	MOV $24+X, A				;$0993   |
	BRA CODE_09AE				;$0995   |

CODE_0997:
	MOV Y, #$01				;$0997   |
	MOV A, ($00)+Y				;$0999   |
	MOV $34+X, A				;$099B   |
	MOV A, $01D0+X				;$099D   |
	BEQ CODE_09AB				;$09A0   |
	MOV A, $34+X				;$09A2   |
	MOV $24+X, A				;$09A4   |
	INC Y					;$09A6   |
	MOV A, ($00)+Y				;$09A7   |
	MOV $34+X, A				;$09A9   |
CODE_09AB:					;   |
	INC Y					;$09AB   |
	MOV $00, Y				;$09AC   |
CODE_09AE:					;   |
	MOV $01, #$00				;$09AE   |
	MOV A, $44+X				;$09B1   |
	MOV Y, $54+X				;$09B3   |
	ADDW YA, $00				;$09B5   |
	MOV $54+X, Y				;$09B7   |
	MOV $44+X, A				;$09B9   |
	RET					;$09BB   |

CODE_09BC:
	MOV A, $0150+X				;$09BC   |
	AND A, #$01				;$09BF   |
	BNE CODE_09C6				;$09C1   |
	JMP CODE_0A39				;$09C3   |

CODE_09C6:
	MOV A, $01A0+X				;$09C6   |
	BEQ CODE_09DA				;$09C9   |
	CMP A, #$FF				;$09CB   |
	BEQ CODE_0A39				;$09CD   |
	DEC A					;$09CF   |
	MOV $01A0+X, A				;$09D0   |
	BNE CODE_0A39				;$09D3   |
	MOV A, #$01				;$09D5   |
	MOV $0100+X, A				;$09D7   |
CODE_09DA:					;   |
	MOV A, $0100+X				;$09DA   |
	DEC A					;$09DD   |
	MOV $0100+X, A				;$09DE   |
	BNE CODE_0A39				;$09E1   |
	MOV A, $0170+X				;$09E3   |
	MOV $0100+X, A				;$09E6   |
	MOV A, $01C0+X				;$09E9   |
	BEQ CODE_0A10				;$09EC   |
	DEC A					;$09EE   |
	MOV $01C0+X, A				;$09EF   |
	MOV A, $01B0+X				;$09F2   |
	EOR A, #$FF				;$09F5   |
	INC A					;$09F7   |
	MOV $00, A				;$09F8   |
	BPL CODE_0A00				;$09FA   |
	MOV A, #$FF				;$09FC   |
	BRA CODE_0A02				;$09FE   |

CODE_0A00:
	MOV A, #$00				;$0A00   |
CODE_0A02:					;   |
	MOV $01, A				;$0A02   |
	MOV A, $84+X				;$0A04   |
	MOV Y, $74+X				;$0A06   |
	ADDW YA, $00				;$0A08   |
	MOV $74+X, Y				;$0A0A   |
	MOV $84+X, A				;$0A0C   |
	BRA CODE_0A1B				;$0A0E   |

CODE_0A10:
	MOV A, $01B0+X				;$0A10   |
	MOV $00, A				;$0A13   |
	BPL CODE_0A00				;$0A15   |
	MOV A, #$FF				;$0A17   |
	BRA CODE_0A02				;$0A19   |

CODE_0A1B:
	MOV A, $01E0+X				;$0A1B   |
	BNE CODE_0A30				;$0A1E   |
	MOV A, X				;$0A20   |
	AND A, #$07				;$0A21   |
	XCN A					;$0A23   |
	OR A, #$02				;$0A24   |
	MOV $F2, A				;$0A26   |
	MOV A, $84+X				;$0A28   |
	MOV $F3, A				;$0A2A   |
	INC $F2					;$0A2C   |
	MOV $F3, Y				;$0A2E   |
CODE_0A30:					;   |
	DEC $94+X				;$0A30   |
	BNE CODE_0A39				;$0A32   |
	MOV A, #$FF				;$0A34   |
	MOV $01A0+X, A				;$0A36   |
CODE_0A39:					;   |
	MOV A, $0150+X				;$0A39   |
	AND A, #$02				;$0A3C   |
	BEQ CODE_0AB4				;$0A3E   |
	MOV A, $C4+X				;$0A40   |
	BEQ CODE_0A48				;$0A42   |
	DEC $C4+X				;$0A44   |
	BRA CODE_0AB4				;$0A46   |

CODE_0A48:
	DEC $B4+X				;$0A48   |
	BNE CODE_0AB4				;$0A4A   |
	MOV A, $0210+X				;$0A4C   |
	MOV $B4+X, A				;$0A4F   |
	MOV A, $0234+X				;$0A51   |
	MOV $00, A				;$0A54   |
	BPL CODE_0A5C				;$0A56   |
	MOV A, #$FF				;$0A58   |
	BRA CODE_0A5E				;$0A5A   |

CODE_0A5C:
	MOV A, #$00				;$0A5C   |
CODE_0A5E:					;   |
	MOV $01, A				;$0A5E   |
	MOV A, $84+X				;$0A60   |
	MOV Y, $74+X				;$0A62   |
	CMP X, #$0D				;$0A64   |
	BNE CODE_0A87				;$0A66   |
	PUSH A					;$0A68   |
	MOV A, $EC				;$0A69   |
	CMP A, $EE				;$0A6B   |
	BNE CODE_0A78				;$0A6D   |
	MOV A, $ED				;$0A6F   |
	CMP A, $EF				;$0A71   |
	BNE CODE_0A78				;$0A73   |
	POP A					;$0A75   |
	BRA CODE_0A87				;$0A76   |

CODE_0A78:
	POP A					;$0A78   |
	SUBW YA, $EE				;$0A79   |
	ADDW YA, $EC				;$0A7B   |
	PUSH A					;$0A7D   |
	MOV A, $EC				;$0A7E   |
	MOV $EE, A				;$0A80   |
	MOV A, $ED				;$0A82   |
	MOV $EF, A				;$0A84   |
	POP A					;$0A86   |
CODE_0A87:					;   |
	ADDW YA, $00				;$0A87   |
	MOV $74+X, Y				;$0A89   |
	MOV $84+X, A				;$0A8B   |
	MOV A, $01E0+X				;$0A8D   |
	BNE CODE_0AA2				;$0A90   |
	MOV A, X				;$0A92   |
	AND A, #$07				;$0A93   |
	XCN A					;$0A95   |
	OR A, #$02				;$0A96   |
	MOV $F2, A				;$0A98   |
	MOV A, $84+X				;$0A9A   |
	MOV $F3, A				;$0A9C   |
	INC $F2					;$0A9E   |
	MOV $F3, Y				;$0AA0   |
CODE_0AA2:					;   |
	DEC $A4+X				;$0AA2   |
	BNE CODE_0AB4				;$0AA4   |
	MOV A, $0200+X				;$0AA6   |
	MOV $A4+X, A				;$0AA9   |
	MOV A, $0234+X				;$0AAB   |
	EOR A, #$FF				;$0AAE   |
	INC A					;$0AB0   |
	MOV $0234+X, A				;$0AB1   |
CODE_0AB4:					;   |
	MOV A, $0150+X				;$0AB4   |
	AND A, #$0C				;$0AB7   |
	BNE CODE_0ABE				;$0AB9   |
	JMP CODE_0B17				;$0ABB   |

CODE_0ABE:
	MOV A, $02A4+X				;$0ABE   |
	BEQ CODE_0ACD				;$0AC1   |
	MOV A, $02A4+X				;$0AC3   |
	DEC A					;$0AC6   |
	MOV $02A4+X, A				;$0AC7   |
	JMP CODE_0B17				;$0ACA   |

CODE_0ACD:
	MOV A, $02B4+X				;$0ACD   |
	DEC A					;$0AD0   |
	MOV $02B4+X, A				;$0AD1   |
	BEQ CODE_0AD9				;$0AD4   |
	JMP CODE_0B17				;$0AD6   |

CODE_0AD9:
	MOV A, $02C4+X				;$0AD9   |
	MOV $02B4+X, A				;$0ADC   |
	MOV A, $01E0+X				;$0ADF   |
	BNE CODE_0AF8				;$0AE2   |
	MOV A, X				;$0AE4   |
	AND A, #$07				;$0AE5   |
	XCN A					;$0AE7   |
	OR A, #$00				;$0AE8   |
	MOV $F2, A				;$0AEA   |
	MOV A, $0254+X				;$0AEC   |
	MOV $F3, A				;$0AEF   |
	MOV A, $0264+X				;$0AF1   |
	INC $F2					;$0AF4   |
	MOV $F3, A				;$0AF6   |
CODE_0AF8:					;   |
	MOV A, $02E4+X				;$0AF8   |
	DEC A					;$0AFB   |
	MOV $02E4+X, A				;$0AFC   |
	BNE CODE_0B17				;$0AFF   |
	MOV A, $0150+X				;$0B01   |
	AND A, #$08				;$0B04   |
	BNE CODE_0B17				;$0B06   |
	MOV A, $02F4+X				;$0B08   |
	MOV $02E4+X, A				;$0B0B   |
	MOV A, $02D4+X				;$0B0E   |
	EOR A, #$FF				;$0B11   |
	INC A					;$0B13   |
	MOV $02D4+X, A				;$0B14   |
CODE_0B17:
	RET					;$0B17   |

CODE_0B18:					;   |
	POP X					;$0B18   |
	MOV A, #$00				;$0B19   |
	MOV $0110+X, A				;$0B1B   |
	MOV A, $01E0+X				;$0B1E   |
	BNE CODE_0B2B				;$0B21   |
	MOV $F2, #$5C				;$0B23   |
	MOV A, $0F95+X				;$0B26   |
	MOV $F3, A				;$0B29   |
CODE_0B2B:					;   |
	MOV A, X				;$0B2B   |
	CMP A, #$08				;$0B2C   |
	BCC CODE_0B61				;$0B2E   |
	PUSH X					;$0B30   |
	SETC					;$0B31   |
	SBC A, #$08				;$0B32   |
	MOV X, A				;$0B34   |
	MOV A, #$00				;$0B35   |
	MOV $01E0+X, A				;$0B37   |
	MOV $F2, #$3D				;$0B3A   |
	MOV A, $0F95+X				;$0B3D   |
	EOR A, #$FF				;$0B40   |
	AND A, $F3				;$0B42   |
	MOV $F3, A				;$0B44   |
	MOV $F2, #$4D				;$0B46   |
	MOV A, $0294+X				;$0B49   |
	BEQ CODE_0B57				;$0B4C   |
	MOV A, $0F95+X				;$0B4E   |
	OR A, $F3				;$0B51   |
	MOV $F3, A				;$0B53   |
	BRA CODE_0B60				;$0B55   |

CODE_0B57:
	MOV A, $0F95+X				;$0B57   |
	EOR A, #$FF				;$0B5A   |
	AND A, $F3				;$0B5C   |
	MOV $F3, A				;$0B5E   |
CODE_0B60:					;   |
	POP X					;$0B60   |
CODE_0B61:					;   |
	MOV A, #$00				;$0B61   |
	RET					;$0B63   |

CODE_0B64:
	POP Y					;$0B64   |
	POP A					;$0B65   |
	POP X					;$0B66   |
	PUSH A					;$0B67   |
	PUSH Y					;$0B68   |
CODE_0B69:					;   |
	MOV Y, #$01				;$0B69   |
	MOV $34+X, Y				;$0B6B   |
	MOV A, #$00				;$0B6D   |
	MOV $24+X, A				;$0B6F   |
	RET					;$0B71   |

CODE_0B72:					;   |
	CALL CODE_0B64				;$0B72   |
	CALL CODE_0B8B				;$0B75   |
CODE_0B78:					;   |
	MOV $00, #$02				;$0B78   |
CODE_0B7B:					;   |
	MOV $01, #$00				;$0B7B   |
	MOV A, $44+X				;$0B7E   |
	MOV Y, $54+X				;$0B80   |
	ADDW YA, $00				;$0B82   |
	MOV $54+X, Y				;$0B84   |
	MOV $44+X, A				;$0B86   |
	MOV A, #$01				;$0B88   |
	RET					;$0B8A   |

CODE_0B8B:
	PUSH X					;$0B8B   |
	MOV A, ($00)+Y				;$0B8C   |
	MOV X, A				;$0B8E   |
	MOV A, $0560+X				;$0B8F   |
	POP X					;$0B92   |
	MOV $0244+X, A				;$0B93   |
	RET					;$0B96   |

CODE_0B97:					;   |
	CALL CODE_0B64				;$0B97   |
	CALL CODE_0B8B				;$0B9A   |
	INC Y					;$0B9D   |
	MOV A, ($00)+Y				;$0B9E   |
	MOV $0140+X, A				;$0BA0   |
	INC Y					;$0BA3   |
	MOV A, ($00)+Y				;$0BA4   |
	MOV $64+X, A				;$0BA6   |
	INC Y					;$0BA8   |
	CALL CODE_0BC2				;$0BA9   |
	INC Y					;$0BAC   |
	CALL CODE_0E4E				;$0BAD   |
	MOV $00, #$08				;$0BB0   |
	JMP CODE_0B7B				;$0BB3   |

CODE_0BB6:					;   |
	CALL CODE_0B64				;$0BB6   |
	CALL CODE_0BC2				;$0BB9   |
CODE_0BBC:					;   |
	MOV $00, #$03				;$0BBC   |
	JMP CODE_0B7B				;$0BBF   |

CODE_0BC2:
	MOV A, $1D				;$0BC2   |
	BNE CODE_0BD2				;$0BC4   |
	MOV A, ($00)+Y				;$0BC6   |
	MOV $0254+X, A				;$0BC8   |
	INC Y					;$0BCB   |
CODE_0BCC:					;   |
	MOV A, ($00)+Y				;$0BCC   |
	MOV $0264+X, A				;$0BCE   |
	RET					;$0BD1   |

CODE_0BD2:
	MOV A, ($00)+Y				;$0BD2   |
	BPL CODE_0BD9				;$0BD4   |
	EOR A, #$FF				;$0BD6   |
	INC A					;$0BD8   |
CODE_0BD9:					;   |
	LSR A					;$0BD9   |
	MOV $03, A				;$0BDA   |
	INC Y					;$0BDC   |
	CLRC					;$0BDD   |
	MOV A, ($00)+Y				;$0BDE   |
	BPL CODE_0BE5				;$0BE0   |
	EOR A, #$FF				;$0BE2   |
	INC A					;$0BE4   |
CODE_0BE5:					;   |
	LSR A					;$0BE5   |
	CLRC					;$0BE6   |
	ADC A, $03				;$0BE7   |
	MOV $0254+X, A				;$0BE9   |
	MOV $0264+X, A				;$0BEC   |
	RET					;$0BEF   |

CODE_0BF0:					;   |
	CALL CODE_0B64				;$0BF0   |
	MOV $0254+X, A				;$0BF3   |
	CALL CODE_0BCC				;$0BF6   |
	MOV A, $0264+X				;$0BF9   |
	MOV $0254+X, A				;$0BFC   |
	JMP CODE_0B78				;$0BFF   |

CODE_0C02:					;   |
	CALL CODE_0B64				;$0C02   |
	MOV A, $04B8				;$0C05   |
	MOV $0254+X, A				;$0C08   |
	MOV A, $04B9				;$0C0B   |
	MOV $0264+X, A				;$0C0E   |
	MOV A, $1D				;$0C11   |
	BNE CODE_0C2E				;$0C13   |
	JMP CODE_0ED6				;$0C15   |

CODE_0C18:					;   |
	CALL CODE_0B64				;$0C18   |
	MOV A, $04BA				;$0C1B   |
	MOV $0254+X, A				;$0C1E   |
	MOV A, $04BB				;$0C21   |
	MOV $0264+X, A				;$0C24   |
	MOV A, $1D				;$0C27   |
	BNE CODE_0C2E				;$0C29   |
	JMP CODE_0ED6				;$0C2B   |

CODE_0C2E:
	MOV A, $0254+X				;$0C2E   |
	BPL CODE_0C36				;$0C31   |
	EOR A, #$FF				;$0C33   |
	INC A					;$0C35   |
CODE_0C36:					;   |
	LSR A					;$0C36   |
	MOV $00, A				;$0C37   |
	MOV A, $0264+X				;$0C39   |
	BPL CODE_0C41				;$0C3C   |
	EOR A, #$FF				;$0C3E   |
	INC A					;$0C40   |
CODE_0C41:					;   |
	LSR A					;$0C41   |
	CLRC					;$0C42   |
	ADC A, $00				;$0C43   |
	MOV $0254+X, A				;$0C45   |
	MOV $0264+X, A				;$0C48   |
	JMP CODE_0ED6				;$0C4B   |

CODE_0C4E:					;   |
	CALL CODE_0B64				;$0C4E   |
	MOV A, ($00)+Y				;$0C51   |
	MOV $04B6, A				;$0C53   |
	JMP CODE_0B78				;$0C56   |

CODE_0C59:
	PUSH X					;$0C59   |
	MOV Y, $04B6				;$0C5A   |
	CMP X, #$08				;$0C5D   |
	BCS CODE_0C6F				;$0C5F   |
	CMP A, #$00				;$0C61   |
	BMI CODE_0C71				;$0C63   |
	MUL YA					;$0C65   |
	MOV X, #$64				;$0C66   |
	DIV YA, X				;$0C68   |
	CMP A, #$7F				;$0C69   |
	BMI CODE_0C6F				;$0C6B   |
	MOV A, #$7F				;$0C6D   |
CODE_0C6F:					;   |
	POP X					;$0C6F   |
	RET					;$0C70   |

CODE_0C71:
	EOR A, #$FF				;$0C71   |
	INC A					;$0C73   |
	MUL YA					;$0C74   |
	MOV X, #$64				;$0C75   |
	DIV YA, X				;$0C77   |
	CMP A, #$7F				;$0C78   |
	BMI CODE_0C7E				;$0C7A   |
	MOV A, #$7F				;$0C7C   |
CODE_0C7E:					;   |
	EOR A, #$FF				;$0C7E   |
	INC A					;$0C80   |
	POP X					;$0C81   |
	RET					;$0C82   |

CODE_0C83:					;   |
	CALL CODE_0B64				;$0C83   |
	MOV A, ($00)+Y				;$0C86   |
	MOV $04B8, A				;$0C88   |
	INC Y					;$0C8B   |
	MOV A, ($00)+Y				;$0C8C   |
	MOV $04B9, A				;$0C8E   |
	INC Y					;$0C91   |
	MOV A, ($00)+Y				;$0C92   |
	MOV $04BA, A				;$0C94   |
	INC Y					;$0C97   |
	MOV A, ($00)+Y				;$0C98   |
	MOV $04BB, A				;$0C9A   |
	JMP CODE_0F76				;$0C9D   |

CODE_0CA0:					;   |
	CALL CODE_0B64				;$0CA0   |
	MOV A, ($00)+Y				;$0CA3   |
	CALL CODE_10F0				;$0CA5   |
	MOV $F2, #$7D				;$0CA8   |
	CLRC					;$0CAB   |
	LSR A					;$0CAC   |
	MOV $F3, A				;$0CAD   |
	MOV $F2, #$6D				;$0CAF   |
	ROL A					;$0CB2   |
	ROL A					;$0CB3   |
	ROL A					;$0CB4   |
	MOV $00, A				;$0CB5   |
	MOV A, #$FF				;$0CB7   |
	SETC					;$0CB9   |
	SBC A, $00				;$0CBA   |
	MOV $F3, A				;$0CBC   |
	MOV $04B7, A				;$0CBE   |
	MOV Y, A				;$0CC1   |
	MOV A, #$00				;$0CC2   |
	MOVW $00, YA				;$0CC4   |
CODE_0CC6:					;   |
	MOV Y, A				;$0CC6   |
CODE_0CC7:					;   |
	MOV ($00)+Y, A				;$0CC7   |
	INC Y					;$0CC9   |
	BNE CODE_0CC7				;$0CCA   |
	INC $01					;$0CCC   |
	MOV Y, $01				;$0CCE   |
	CMP Y, #$00				;$0CD0   |
	BNE CODE_0CC6				;$0CD2   |
	JMP CODE_0B78				;$0CD4   |

CODE_0CD7:					;   |
	CALL CODE_0B64				;$0CD7   |
	MOV A, ($00)+Y				;$0CDA   |
	MOV $44+X, A				;$0CDC   |
	INC Y					;$0CDE   |
	MOV A, ($00)+Y				;$0CDF   |
	MOV $54+X, A				;$0CE1   |
	MOV A, #$01				;$0CE3   |
	RET					;$0CE5   |

CODE_0CE6:					;   |
	CALL CODE_0B64				;$0CE6   |
	MOV A, ($00)+Y				;$0CE9   |
	MOV $04, A				;$0CEB   |
	INC Y					;$0CED   |
	CALL CODE_0D1C				;$0CEE   |
CODE_0CF1:					;   |
	MOV $0334+Y, A				;$0CF1   |
CODE_0CF4:					;   |
	INC $D4+X				;$0CF4   |
	MOVW YA, $02				;$0CF6   |
	MOV $44+X, A				;$0CF8   |
	MOV $54+X, Y				;$0CFA   |
	MOV A, #$01				;$0CFC   |
	RET					;$0CFE   |

CODE_0CFF:					;   |
	CALL CODE_0B64				;$0CFF   |
	MOV $04, #$01				;$0D02   |
	CALL CODE_0D1C				;$0D05   |
	BEQ CODE_0D0E				;$0D08   |
	DEC A					;$0D0A   |
	JMP CODE_0CF1				;$0D0B   |

CODE_0D0E:
	DEC A					;$0D0E   |
	MOV $0334+Y, A				;$0D0F   |
	MOV A, $03B4+Y				;$0D12   |
	DEC A					;$0D15   |
	MOV $03B4+Y, A				;$0D16   |
	JMP CODE_0CF4				;$0D19   |

CODE_0D1C:
	MOV A, ($00)+Y				;$0D1C   |
	MOV $02, A				;$0D1E   |
	INC Y					;$0D20   |
	MOV A, ($00)+Y				;$0D21   |
	MOV $03, A				;$0D23   |
	MOV Y, $D4+X				;$0D25   |
	MOV A, $04				;$0D27   |
	MOV $0434+Y, A				;$0D29   |
	MOV A, $54+X				;$0D2C   |
	MOV $03B4+Y, A				;$0D2E   |
	MOV A, $44+X				;$0D31   |
	RET					;$0D33   |

CODE_0D34:					;   |
	CALL CODE_0B64				;$0D34   |
	DEC $D4+X				;$0D37   |
	MOV Y, $D4+X				;$0D39   |
	MOV A, $03B4+Y				;$0D3B   |
	MOV $54+X, A				;$0D3E   |
	MOV A, $0334+Y				;$0D40   |
	MOV $44+X, A				;$0D43   |
	MOV A, $0434+Y				;$0D45   |
	DEC A					;$0D48   |
	MOV $0434+Y, A				;$0D49   |
	BEQ CODE_0D6A				;$0D4C   |
	MOV A, $44+X				;$0D4E   |
	MOV Y, $54+X				;$0D50   |
	MOVW $00, YA				;$0D52   |
	MOV Y, #$02				;$0D54   |
	MOV A, ($00)+Y				;$0D56   |
	MOV $02, A				;$0D58   |
	INC Y					;$0D5A   |
	MOV A, ($00)+Y				;$0D5B   |
	MOV $03, A				;$0D5D   |
	INC $D4+X				;$0D5F   |
	MOVW YA, $02				;$0D61   |
	MOV $44+X, A				;$0D63   |
	MOV $54+X, Y				;$0D65   |
	MOV A, #$01				;$0D67   |
	RET					;$0D69   |

CODE_0D6A:
	MOV $00, #$04				;$0D6A   |
	JMP CODE_0B7B				;$0D6D   |

CODE_0D70:					;   |
	CALL CODE_0B64				;$0D70   |
	MOV A, ($00)+Y				;$0D73   |
	MOV $0120+X, A				;$0D75   |
	MOV A, $01D0+X				;$0D78   |
	BEQ CODE_0D89				;$0D7B   |
	MOV A, $0120+X				;$0D7D   |
	MOV $0130+X, A				;$0D80   |
	INC Y					;$0D83   |
	MOV A, ($00)+Y				;$0D84   |
	MOV $0120+X, A				;$0D86   |
CODE_0D89:					;   |
	INC Y					;$0D89   |
	MOV $00, Y				;$0D8A   |
	JMP CODE_0B7B				;$0D8C   |

CODE_0D8F:					;   |
	POP X					;$0D8F   |
	MOV A, #$00				;$0D90   |
	MOV $0120+X, A				;$0D92   |
	MOV $0130+X, A				;$0D95   |
	JMP CODE_0DDF				;$0D98   |

CODE_0D9B:					;   |
	POP X					;$0D9B   |
	MOV Y, #$04				;$0D9C   |
	MOV A, ($00)+Y				;$0D9E   |
	BRA CODE_0DAA				;$0DA0   |

CODE_0DA2:					;   |
	POP X					;$0DA2   |
	MOV Y, #$04				;$0DA3   |
	MOV A, ($00)+Y				;$0DA5   |
	EOR A, #$FF				;$0DA7   |
	INC A					;$0DA9   |
CODE_0DAA:					;   |
	MOV $01B0+X, A				;$0DAA   |
	MOV A, $0150+X				;$0DAD   |
	OR A, #$01				;$0DB0   |
	MOV $0150+X, A				;$0DB2   |
	CALL CODE_0B69				;$0DB5   |
	MOV A, ($00)+Y				;$0DB8   |
	MOV $0160+X, A				;$0DBA   |
	INC Y					;$0DBD   |
	MOV A, ($00)+Y				;$0DBE   |
	MOV $0170+X, A				;$0DC0   |
	INC Y					;$0DC3   |
	MOV A, ($00)+Y				;$0DC4   |
	MOV $0180+X, A				;$0DC6   |
	INC Y					;$0DC9   |
	INC Y					;$0DCA   |
	MOV A, ($00)+Y				;$0DCB   |
	MOV $0190+X, A				;$0DCD   |
	MOV $00, #$06				;$0DD0   |
	JMP CODE_0B7B				;$0DD3   |

CODE_0DD6:					;   |
	POP X					;$0DD6   |
	MOV A, $0150+X				;$0DD7   |
	AND A, #$FE				;$0DDA   |
	MOV $0150+X, A				;$0DDC   |
CODE_0DDF:					;   |
	MOV A, #$01				;$0DDF   |
	MOV $00, A				;$0DE1   |
	MOV $34+X, A				;$0DE3   |
	DEC A					;$0DE5   |
	MOV $24+X, A				;$0DE6   |
	JMP CODE_0B7B				;$0DE8   |

CODE_0DEB:					;   |
	POP X					;$0DEB   |
	MOV Y, #$01				;$0DEC   |
	MOV A, ($00)+Y				;$0DEE   |
	MOV $1F, A				;$0DF0   |
CODE_0DF2:					;   |
	CALL CODE_0B69				;$0DF2   |
	JMP CODE_0B78				;$0DF5   |

CODE_0DF8:					;   |
	POP X					;$0DF8   |
	MOV Y, #$01				;$0DF9   |
	MOV A, ($00)+Y				;$0DFB   |
	CLRC					;$0DFD   |
	ADC A, $1F				;$0DFE   |
	MOV $1F, A				;$0E00   |
	JMP CODE_0DF2				;$0E02   |

CODE_0E05:					;   |
	POP X					;$0E05   |
	MOV A, $0150+X				;$0E06   |
	AND A, #$FD				;$0E09   |
	MOV $0150+X, A				;$0E0B   |
	JMP CODE_0DDF				;$0E0E   |

CODE_0E11:					;   |
	POP X					;$0E11   |
	MOV A, #$00				;$0E12   |
	CALL CODE_0E25				;$0E14   |
	JMP CODE_0D6A				;$0E17   |

CODE_0E1A:					;   |
	POP X					;$0E1A   |
	MOV Y, #$04				;$0E1B   |
	MOV A, ($00)+Y				;$0E1D   |
	CALL CODE_0E25				;$0E1F   |
	JMP CODE_0F76				;$0E22   |

CODE_0E25:
	MOV $0220+X, A				;$0E25   |
	MOV A, $0150+X				;$0E28   |
	OR A, #$02				;$0E2B   |
	MOV $0150+X, A				;$0E2D   |
	CALL CODE_0B69				;$0E30   |
	MOV A, ($00)+Y				;$0E33   |
	MOV $0200+X, A				;$0E35   |
	INC Y					;$0E38   |
	MOV A, ($00)+Y				;$0E39   |
	MOV $0210+X, A				;$0E3B   |
	INC Y					;$0E3E   |
	MOV A, ($00)+Y				;$0E3F   |
	MOV $0234+X, A				;$0E41   |
	RET					;$0E44   |

CODE_0E45:					;   |
	CALL CODE_0B64				;$0E45   |
	CALL CODE_0E4E				;$0E48   |
	JMP CODE_0BBC				;$0E4B   |

CODE_0E4E:
	MOV A, ($00)+Y				;$0E4E   |
	MOV $0274+X, A				;$0E50   |
	INC Y					;$0E53   |
	MOV A, ($00)+Y				;$0E54   |
	MOV $0284+X, A				;$0E56   |
	RET					;$0E59   |

CODE_0E5A:					;   |
	POP X					;$0E5A   |
	MOV Y, #$01				;$0E5B   |
	MOV A, ($00)+Y				;$0E5D   |
	MOV $0C+X, A				;$0E5F   |
	JMP CODE_0E6B				;$0E61   |

CODE_0E64:					;   |
	POP X					;$0E64   |
	MOV Y, #$01				;$0E65   |
	MOV A, ($00)+Y				;$0E67   |
	MOV $14+X, A				;$0E69   |
CODE_0E6B:					;   |
	CALL CODE_0B69				;$0E6B   |
	JMP CODE_0B78				;$0E6E   |

CODE_0E71:					;   |
	CALL CODE_0B64				;$0E71   |
	MOV A, ($00)+Y				;$0E74   |
	MOV $64+X, A				;$0E76   |
	JMP CODE_0B78				;$0E78   |

CODE_0E7B:					;   |
	CALL CODE_0B64				;$0E7B   |
	MOV $24+X, A				;$0E7E   |
	MOV A, ($00)+Y				;$0E80   |
	MOV $0140+X, A				;$0E82   |
	JMP CODE_0B78				;$0E85   |

CODE_0E88:					;   |
	CALL CODE_0B64				;$0E88   |
	MOV A, ($00)+Y				;$0E8B   |
	CLRC					;$0E8D   |
	ADC A, $0140+X				;$0E8E   |
	MOV $0140+X, A				;$0E91   |
	JMP CODE_0B78				;$0E94   |

CODE_0E97:					;   |
	POP X					;$0E97   |
	MOV $F2, #$0D				;$0E98   |
	MOV Y, #$01				;$0E9B   |
	MOV A, ($00)+Y				;$0E9D   |
	MOV $F3, A				;$0E9F   |
	INC Y					;$0EA1   |
	MOV $F2, #$2C				;$0EA2   |
	MOV A, ($00)+Y				;$0EA5   |
	MOV $0232, A				;$0EA7   |
	MOV $F3, A				;$0EAA   |
	MOV $F2, #$3C				;$0EAC   |
	INC Y					;$0EAF   |
	MOV A, ($00)+Y				;$0EB0   |
	MOV $0233, A				;$0EB2   |
	MOV $F3, A				;$0EB5   |
	MOV A, #$00				;$0EB7   |
	MOV $04B5, A				;$0EB9   |
	MOV $F2, #$6C				;$0EBC   |
	MOV $F3, A				;$0EBF   |
	JMP CODE_0D6A				;$0EC1   |

CODE_0EC4:					;   |
	CALL CODE_0B64				;$0EC4   |
	MOV $F2, #$4D				;$0EC7   |
	MOV A, $0F95+X				;$0ECA   |
	OR A, $F3				;$0ECD   |
	MOV $F3, A				;$0ECF   |
	MOV A, #$01				;$0ED1   |
	MOV $0294+X, A				;$0ED3   |
CODE_0ED6:					;   |
	MOV $00, #$01				;$0ED6   |
	JMP CODE_0B7B				;$0ED9   |

CODE_0EDC:					;   |
	POP X					;$0EDC   |
	MOV $F2, #$4D				;$0EDD   |
	MOV A, $0F95+X				;$0EE0   |
	EOR A, #$FF				;$0EE3   |
	AND A, $F3				;$0EE5   |
	MOV $F3, A				;$0EE7   |
	MOV A, #$00				;$0EE9   |
	MOV $0294+X, A				;$0EEB   |
	MOV $24+X, A				;$0EEE   |
	INC A					;$0EF0   |
	MOV $34+X, A				;$0EF1   |
	JMP CODE_0ED6				;$0EF3   |

CODE_0EF6:					;   |
	CALL CODE_0B64				;$0EF6   |
	MOV $F2, #$0F				;$0EF9   |
CODE_0EFC:					;   |
	MOV A, ($00)+Y				;$0EFC   |
	MOV $F3, A				;$0EFE   |
	INC Y					;$0F00   |
	CLRC					;$0F01   |
	ADC $F2, #$10				;$0F02   |
	CMP $F2, #$8F				;$0F05   |
	BNE CODE_0EFC				;$0F08   |
	MOV $00, #$09				;$0F0A   |
	JMP CODE_0B7B				;$0F0D   |

CODE_0F10:					;   |
	CALL CODE_0B64				;$0F10   |
	MOV A, ($00)+Y				;$0F13   |
	MOV $04B4, A				;$0F15   |
	OR A, $04B5				;$0F18   |
	MOV $F2, #$6C				;$0F1B   |
	MOV $F3, A				;$0F1E   |
	JMP CODE_0B78				;$0F20   |

CODE_0F23:					;   |
	POP X					;$0F23   |
	MOV $F2, #$3D				;$0F24   |
	MOV A, $0F95+X				;$0F27   |
	OR A, $F3				;$0F2A   |
	MOV $F3, A				;$0F2C   |
CODE_0F2E:					;   |
	CALL CODE_0B69				;$0F2E   |
	JMP CODE_0ED6				;$0F31   |

CODE_0F34:					;   |
	POP X					;$0F34   |
	MOV $F2, #$3D				;$0F35   |
	MOV A, $0F95+X				;$0F38   |
	EOR A, #$FF				;$0F3B   |
	AND A, $F3				;$0F3D   |
	MOV $F3, A				;$0F3F   |
	JMP CODE_0F2E				;$0F41   |

CODE_0F44:					;   |
	POP X					;$0F44   |
	MOV Y, #$04				;$0F45   |
	MOV A, ($00)+Y				;$0F47   |
	EOR A, #$FF				;$0F49   |
	INC A					;$0F4B   |
	BRA CODE_0F53				;$0F4C   |

CODE_0F4E:					;   |
	POP X					;$0F4E   |
	MOV Y, #$04				;$0F4F   |
	MOV A, ($00)+Y				;$0F51   |
CODE_0F53:					;   |
	MOV $01B0+X, A				;$0F53   |
	MOV A, $0150+X				;$0F56   |
	OR A, #$01				;$0F59   |
	MOV $0150+X, A				;$0F5B   |
	CALL CODE_0B69				;$0F5E   |
	MOV A, ($00)+Y				;$0F61   |
	MOV $0160+X, A				;$0F63   |
	INC Y					;$0F66   |
	MOV A, ($00)+Y				;$0F67   |
	MOV $0170+X, A				;$0F69   |
	INC Y					;$0F6C   |
	MOV A, ($00)+Y				;$0F6D   |
	MOV $0190+X, A				;$0F6F   |
	ASL A					;$0F72   |
	MOV $0180+X, A				;$0F73   |
CODE_0F76:					;   |
	MOV $00, #$05				;$0F76   |
	JMP CODE_0B7B				;$0F79   |

CODE_0F7C:					;   |
	CALL CODE_0B64				;$0F7C   |
	INC A					;$0F7F   |
	MOV $01D0+X, A				;$0F80   |
	JMP CODE_0ED6				;$0F83   |

CODE_0F86:
	CALL CODE_0B64				;$0F86   |
	MOV $01D0+X, A				;$0F89   |
	JMP CODE_0ED6				;$0F8C   |

CODE_0F8F:					;   |
	MOV $00, #$07				;$0F8F   |
	JMP CODE_0B7B				;$0F92   |

DATA_0F95:
	incbin "baserom.sfc":$2E0ABD..$2E0ACD

DATA_0FA5:
	dw CODE_0B18
	dw CODE_0B72
	dw CODE_0BB6
	dw CODE_0CD7
	dw CODE_0CE6
	dw CODE_0D34
	dw CODE_0D70
	dw CODE_0D8F
	dw CODE_0D9B
	dw CODE_0DA2
	dw CODE_0DD6
	dw CODE_0DEB
	dw CODE_0DF8
	dw CODE_0E11
	dw CODE_0E05
	dw CODE_0E1A
	dw CODE_0E45
	dw !null_pointer
	dw CODE_0E71
	dw CODE_0E7B
	dw CODE_0E88
	dw CODE_0E97
	dw CODE_0EC4
	dw CODE_0EDC
	dw CODE_0EF6
	dw CODE_0F10
	dw CODE_0F23
	dw CODE_0F34
	dw CODE_0E5A
	dw CODE_0E64
	dw CODE_0C83
	dw CODE_0CA0
	dw CODE_0C02
	dw CODE_0CFF
	dw CODE_0B97
	dw CODE_0BF0
	dw CODE_0C4E
	dw !null_pointer
	dw CODE_0F44
	dw CODE_0F4E
	dw !null_pointer
	dw !null_pointer
	dw !null_pointer
	dw CODE_0F7C
	dw CODE_0F86
	dw !null_pointer
	dw !null_pointer
	dw !null_pointer
	dw CODE_0EDC
	dw CODE_0C18
	dw CODE_0EDC

CODE_100B:
	MOV A, #$00				;$100B   |
	MOV $EC, A				;$100D   |
	MOV $ED, A				;$100F   |
	MOV $EE, A				;$1011   |
	MOV $EF, A				;$1013   |
	MOV $F2, #$6C				;$1015   |
	MOV $F3, #$E0				;$1018   |
	MOV $F2, #$2C				;$101B   |
	MOV $0232, A				;$101E   |
	MOV $F3, A				;$1021   |
	MOV $F2, #$3C				;$1023   |
	MOV $0233, A				;$1026   |
	MOV $F3, A				;$1029   |
	MOV $F2, #$0D				;$102B   |
	MOV $F3, A				;$102E   |
	MOV $F2, #$4C				;$1030   |
	MOV $F3, A				;$1033   |
	MOV $F2, #$5C				;$1035   |
	MOV $F3, #$FF				;$1038   |
	MOV $F2, #$2D				;$103B   |
	MOV $F3, A				;$103E   |
	MOV $F2, #$3D				;$1040   |
	MOV $F3, A				;$1043   |
	MOV $F2, #$4D				;$1045   |
	MOV $F3, A				;$1048   |
	MOV A, #$3C				;$104A   |
	MOV $0230, A				;$104C   |
	MOV $0231, A				;$104F   |
	MOV $F2, #$0C				;$1052   |
	MOV $F3, A				;$1055   |
	MOV $F2, #$1C				;$1057   |
	MOV $F3, A				;$105A   |
	MOV A, #$64				;$105C   |
	MOV $04B6, A				;$105E   |
	MOV $F2, #$5D				;$1061   |
	MOV $F3, #$31				;$1064   |
	MOV Y, #$08				;$1067   |
	MOV $F2, #$00				;$1069   |
CODE_106C:					;   |
	MOV A, #$7F				;$106C   |
	MOV $F3, A				;$106E   |
	INC $F2					;$1070   |
	MOV $F3, A				;$1072   |
	CLRC					;$1074   |
	ADC $F2, #$04				;$1075   |
	MOV A, #$00				;$1078   |
	MOV $F3, A				;$107A   |
	INC $F2					;$107C   |
	MOV $F3, A				;$107E   |
	INC $F2					;$1080   |
	MOV $F3, #$FF				;$1082   |
	CLRC					;$1085   |
	ADC $F2, #$09				;$1086   |
	DEC Y					;$1089   |
	BNE CODE_106C				;$108A   |
	MOV $E7, #$FF				;$108C   |
	MOV $E8, #$FF				;$108F   |
	MOV A, #$64				;$1092   |
	MOV $E4, A				;$1094   |
	MOV A, #$20				;$1096   |
	MOV $04B5, A				;$1098   |
	MOV $00, #$08				;$109B   |
	MOV X, #$00				;$109E   |
	MOV Y, #$00				;$10A0   |
	MOV $0A, Y				;$10A2   |
	MOV $04B4, Y				;$10A4   |
	MOV $01, Y				;$10A7   |
CODE_10A9:					;   |
	MOV A, #$01				;$10A9   |
	MOV $34+X, A				;$10AB   |
	MOV $0110+X, A				;$10AD   |
	MOV A, ($E5)+Y				;$10B0   |
	MOV $44+X, A				;$10B2   |
	INC Y					;$10B4   |
	MOV A, ($E5)+Y				;$10B5   |
	MOV $54+X, A				;$10B7   |
	MOV A, $01				;$10B9   |
	MOV $D4+X, A				;$10BB   |
	MOV A, #$00				;$10BD   |
	MOV $01D0+X, A				;$10BF   |
	MOV $24+X, A				;$10C2   |
	MOV $0120+X, A				;$10C4   |
	MOV $0130+X, A				;$10C7   |
	MOV $0150+X, A				;$10CA   |
	MOV $0140+X, A				;$10CD   |
	MOV $64+X, A				;$10D0   |
	MOV $01E0+X, A				;$10D2   |
	MOV $0294+X, A				;$10D5   |
	INC X					;$10D8   |
	INC Y					;$10D9   |
	CLRC					;$10DA   |
	ADC $01, #$08				;$10DB   |
	DBNZ $00, CODE_10A9			;$10DE   |
	MOV A, ($E5)+Y				;$10E1   |
	MOV $1F, A				;$10E3   |
	INC Y					;$10E5   |
	MOV A, ($E5)+Y				;$10E6   |
	MOV $22, A				;$10E8   |
	MOV A, #$00				;$10EA   |
	MOV $1E, A				;$10EC   |
	MOV $21, A				;$10EE   |
CODE_10F0:					;   |
	MOV $F2, #$6C				;$10F0   |
	MOV $F3, #$20				;$10F3   |
	RET					;$10F6   |

CODE_10F7:
	PUSH A					;$10F7   |
	CMP A, #$60				;$10F8   |
	BPL CODE_1104				;$10FA   |
	SETC					;$10FC   |
	SBC A, $2410				;$10FD   |
	BPL CODE_110D				;$1100   |
	BRA CODE_1111				;$1102   |

CODE_1104:
	SETC					;$1104   |
	SBC A, #$60				;$1105   |
	SETC					;$1107   |
	SBC A, $2E94				;$1108   |
	BMI CODE_1111				;$110B   |
CODE_110D:					;   |
	POP A					;$110D   |
	MOV A, #$00				;$110E   |
	PUSH A					;$1110   |
CODE_1111:					;   |
	POP A					;$1111   |
	ASL A					;$1112   |
	PUSH A					;$1113   |
	MOV A, #$01				;$1114   |
	MOV $01E0+X, A				;$1116   |
	MOV $F2, #$3D				;$1119   |
	MOV A, $0F95+X				;$111C   |
	EOR A, #$FF				;$111F   |
	AND A, $F3				;$1121   |
	MOV $F3, A				;$1123   |
	MOV A, X				;$1125   |
	CLRC					;$1126   |
	ADC A, #$08				;$1127   |
	MOV X, A				;$1129   |
	ASL A					;$112A   |
	ASL A					;$112B   |
	ASL A					;$112C   |
	MOV $D4+X, A				;$112D   |
	MOV A, #$01				;$112F   |
	MOV $0110+X, A				;$1131   |
	DEC A					;$1134   |
	MOV $0120+X, A				;$1135   |
	MOV $0130+X, A				;$1138   |
	MOV $24+X, A				;$113B   |
	MOV $01D0+X, A				;$113D   |
	MOV $01E0+X, A				;$1140   |
	MOV $0150+X, A				;$1143   |
	MOV $0140+X, A				;$1146   |
	MOV $0294+X, A				;$1149   |
	MOV $64+X, A				;$114C   |
	MOV A, #$7F				;$114E   |
	MOV $0254+X, A				;$1150   |
	MOV $0264+X, A				;$1153   |
	MOV $0314+X, A				;$1156   |
	MOV $0324+X, A				;$1159   |
	MOV A, #$8E				;$115C   |
	MOV $0274+X, A				;$115E   |
	MOV A, #$E0				;$1161   |
	MOV $0284+X, A				;$1163   |
	POP A					;$1166   |
	CMP A, #$C0				;$1167   |
	BCS CODE_1179				;$1169   |
	MOV Y, A				;$116B   |
	MOV A, $2412+Y				;$116C   |
	MOV $44+X, A				;$116F   |
	INC Y					;$1171   |
	MOV A, $2412+Y				;$1172   |
	MOV $54+X, A				;$1175   |
	BRA CODE_1188				;$1177   |

CODE_1179:
	SETC					;$1179   |
	SBC A, #$C0				;$117A   |
	MOV Y, A				;$117C   |
	MOV A, $2E96+Y				;$117D   |
	MOV $44+X, A				;$1180   |
	INC Y					;$1182   |
	MOV A, $2E96+Y				;$1183   |
	MOV $54+X, A				;$1186   |
CODE_1188:					;   |
	MOV A, #$02				;$1188   |
	MOV $34+X, A				;$118A   |
	MOV $F2, #$4D				;$118C   |
	MOV A, $0F95+X				;$118F   |
	EOR A, #$FF				;$1192   |
	AND A, $F3				;$1194   |
	MOV $F3, A				;$1196   |
	RET					;$1198   |

DATA_1199:
	incbin "baserom.sfc":$2E0CC1..$2E0D85

	db $FF
base off
arch 65816

sample_table:
	dl DATA_EECE62
	dl DATA_EECFFC
	dl DATA_EED1CC
	dl DATA_EED47C
	dl DATA_EED63A
	dl DATA_EED87E
	dl DATA_EEDC73
	dl DATA_EEE055
	dl DATA_EEE1AF
	dl DATA_EEE8CE
	dl DATA_EEE9A2
	dl DATA_EEEE4B
	dl DATA_EEF6B6
	dl DATA_EEF8C5
	dl DATA_EEFDC8
	dl DATA_EEFFDF
	dl DATA_EF001A
	dl DATA_EF05BE
	dl DATA_EF0997
	dl DATA_EF120B
	dl DATA_EF1EDC
	dl DATA_EF288C
	dl DATA_EF2F7E
	dl DATA_EF3288
	dl DATA_EF335C
	dl DATA_EF35BC
	dl DATA_EF37B0
	dl DATA_EF3A10
	dl DATA_EF3D5A
	dl DATA_EF4512
	dl DATA_EF4C79
	dl DATA_EF5053
	dl DATA_EF5910
	dl DATA_EF5927
	dl DATA_EF596B
	dl DATA_EF5D96
	dl DATA_EF63EF
	dl DATA_EF66CD
	dl DATA_EF66ED
	dl DATA_EF6731
	dl DATA_EF6748
	dl DATA_EF69CC
	dl DATA_EF720B
	dl DATA_EF72FA
	dl DATA_EF7A2B
	dl DATA_EF8006
	dl DATA_EF818E
	dl DATA_EF8880
	dl DATA_EF90BF
	dl DATA_EF90D6
	dl DATA_EF943B
	dl DATA_EF9677
	dl DATA_EFA228
	dl DATA_EFA7E8
	dl DATA_EFA81A
	dl DATA_EFAC18
	dl DATA_EFAC4A
	dl DATA_EFAC7C
	dl DATA_EFACAE
	dl DATA_EFACE0
	dl DATA_EFAD12
	dl DATA_EFAD44
	dl DATA_EFAD76
	dl DATA_EFADA8
	dl DATA_EFADDA
	dl DATA_EFAE0C
	dl DATA_EFAE3E
	dl DATA_EFAE70
	dl DATA_EFAEA2
	dl DATA_EFAED4
	dl DATA_EFAF06
	dl DATA_EFAF38
	dl DATA_EFAF6A
	dl DATA_EFAF9C
	dl DATA_EFAFCE
	dl DATA_EFB000
	dl DATA_EFB032
	dl DATA_EFB064
	dl DATA_EFB096
	dl DATA_EFB0C8
	dl DATA_EFB0FA
	dl DATA_EFB11A
	dl DATA_EFB13A
	dl DATA_EFB15A
	dl DATA_EFB17A
	dl DATA_EFB19A
	dl DATA_EFB1BA
	dl DATA_EFB1DA
	dl DATA_EFB1FA
	dl DATA_EFB21A
	dl DATA_EFB23A
	dl DATA_EFB26B
	dl DATA_EFB29C
	dl DATA_EFB2CD
	dl DATA_EFB2FE
	dl DATA_EFB32F
	dl DATA_EFB360
	dl DATA_EFB391
	dl DATA_EFB3C2
	dl DATA_EFB3F3
	dl DATA_EFB424
	dl DATA_EFB455
	dl DATA_EFB486
	dl DATA_EFB4B7
	dl DATA_EFB4E8
	dl DATA_EFB519
	dl DATA_EFB54A
	dl DATA_EFB57B
	dl DATA_EFB5AC
	dl DATA_EFB5DD
	dl DATA_EFB60E
	dl DATA_EFB63F
	dl DATA_EFB670
	dl DATA_EFB6A1
	dl DATA_EFB6D2
	dl DATA_EFB703
	dl DATA_EFB734
	dl DATA_EFB765
	dl DATA_EFB796
	dl DATA_EFB7C7
	dl DATA_EFB7F8
	dl DATA_EFB829
	dl DATA_EFB85A
	dl DATA_EFB912
	dl DATA_EFD404
	dl DATA_EFDB08
	dl DATA_EFE26F
	dl DATA_EFE8F5
	dl DATA_EFECBD
	dl DATA_EFEDF4
	dl DATA_EFEEF5
	dl DATA_EFF035
	dl DATA_EFF11B
	dl DATA_EFF894
	dl DATA_EFFAFC
	dl DATA_EFFECD
	dl DATA_F00162
	dl DATA_F0062F
	dl DATA_F00823
	dl DATA_F009FC
	dl DATA_F00A1C
	dl DATA_F00FAF
	dl DATA_F01674
	dl DATA_F01B6E
	dl DATA_F01BA9
	dl DATA_F02667
	dl DATA_F02800
	dl DATA_F02988
	dl DATA_F02C2F
	dl DATA_F03A17
	dl DATA_F03A5B
	dl DATA_F03B1D
	dl DATA_F03D2B
	dl DATA_F04854
	dl DATA_F057E3
	dl DATA_F05FFD
	dl DATA_F073F1
	dl DATA_F08A2D
	dl DATA_F09BFB
	dl DATA_F0A157
	dl DATA_F0A5FF
	dl DATA_F0AE2B
	dl DATA_F0B2B0
	dl DATA_F0B7FA
	dl DATA_F0BD45
	dl DATA_F0C93E
	dl DATA_F0CD45
	dl DATA_F0DC8C
	dl DATA_F0F65E
	dl DATA_F1078A
	dl DATA_F10F26
	dl DATA_F117C7
	dl DATA_F11EEE
	dl DATA_F11F0D
	dl DATA_F12A9A
	dl DATA_F133B1
	dl DATA_F139DC
	dl DATA_F14B98
	dl DATA_F14DA6
	dl DATA_F15542
	dl DATA_F15C0F
	dl DATA_F1663C
	dl DATA_F16F1C
	dl DATA_F179D9
	dl DATA_F18004
	dl DATA_F186E4
	dl DATA_F192E5
	dl DATA_F19304
	dl DATA_F19311
	dl DATA_F1946B
	dl DATA_F1A225
	dl DATA_F1A811
	dl DATA_F1AD5B
	dl DATA_F1B55A
	dl DATA_F1B795
	dl DATA_F1B9D0
	dl DATA_F1BC0B
	dl DATA_F1C03F
	dl DATA_F1C916
	dl DATA_F1CC05
	dl DATA_F1CD4E
	dl DATA_F1CFF6
	dl DATA_F1D190
	dl DATA_F1D68A
	dl DATA_F1DF7C
	dl DATA_F1E973
	dl DATA_F1EDA6
	dl DATA_F1F188
	dl DATA_F1F195
	dl DATA_F1F337
	dl DATA_F1F92C
	dl DATA_F20080
	dl DATA_F202BB
	dl DATA_F218D4
	dl DATA_F21E5D
	dl DATA_F22132
	dl DATA_F225B6
	dl DATA_F22C44
	dl DATA_F232D2
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer
	dl !null_pointer

song_data:
	%offset(song_sample_sets, 3)
	dl DATA_EE1939
	dl DATA_EE1223
	dl DATA_EE195F
	dl DATA_EE124D
	dl DATA_EE1F6E
	dl DATA_EE1263
	dl DATA_EE2CD3
	dl DATA_EE127D
	dl DATA_EE3D78
	dl DATA_EE12C1
	dl DATA_EE421F
	dl DATA_EE12D7
	dl DATA_EE52FC
	dl DATA_EE1337
	dl DATA_EE6007
	dl DATA_EE135F
	dl DATA_EE6F47
	dl DATA_EE13B3
	dl DATA_EE748C
	dl DATA_EE13D3
	dl DATA_EE8572
	dl DATA_EE1437
	dl DATA_EE8BC0
	dl DATA_EE144B
	dl DATA_EE9BE0
	dl DATA_EE147F
	dl DATA_EEA050
	dl DATA_EE1497
	dl DATA_EEAF4A
	dl DATA_EE14D9
	dl DATA_EEBDBD
	dl DATA_EE1545
	dl DATA_F2357A
	dl DATA_EE157D
	dl DATA_F244D4
	dl DATA_EE15C3
	dl DATA_F24760
	dl DATA_EE15D5
	dl DATA_F25872
	dl DATA_EE1601
	dl DATA_F25D1A
	dl DATA_EE1613
	dl DATA_F25FC2
	dl DATA_EE1623
	dl DATA_F26F5C
	dl DATA_EE1655
	dl DATA_F28044
	dl DATA_EE168D
	dl DATA_F290D1
	dl DATA_EE16BF
	dl DATA_F29654
	dl DATA_EE16D5
	dl DATA_F29F81
	dl DATA_EE16EF
	dl DATA_F2AB97
	dl DATA_EE1735
	dl DATA_F2BAD2
	dl DATA_EE176D
	dl DATA_F2BFAF
	dl DATA_EE1781
	dl DATA_F2CFE4
	dl DATA_EE17B1
	dl DATA_F2D64B
	dl DATA_EE1837
	dl DATA_EEBDBD
	dl DATA_EE1863
	dl DATA_F25FC2
	dl DATA_EE1895
	dl DATA_EEBDBD
	dl DATA_EE1545
	dl DATA_EEBDBD
	dl DATA_EE18CB
	dl DATA_F2E6A1
	dl DATA_EE18F1
	dl DATA_EEBDBD
	dl DATA_EE1903
	dl DATA_F2E728
	dl DATA_EE1935
	dl DATA_F2E72C
	dl DATA_EE1937
	dl !null_pointer

DATA_EE1179:
	db $00, $00

DATA_EE117B:
	%offset(DATA_EE117D, 2)
	dl DATA_F2F107
	dl DATA_F2F107
	dl DATA_F2F107
	dl DATA_F2F559
	dl DATA_F2F559
	dl DATA_F2F559
	dl DATA_F2F2F1
	dl DATA_F2F559
	dl DATA_F2F107
	dl DATA_F2F559
	dl DATA_F2F723
	dl DATA_F2F559
	dl DATA_F2F107
	dl DATA_F2F979
	dl DATA_F2F12F
	dl DATA_F2F559
	dl DATA_F2F2F1
	dl DATA_F2F107
	dl DATA_F2F2F1
	dl DATA_F2F723
	dl DATA_F2F107
	dl DATA_F2F723
	dl DATA_F2F979
	dl DATA_F2F12F
	dl DATA_F2F107
	dl DATA_F2F107
	dl DATA_F2F2F1
	dl DATA_F2F6C6
	dl DATA_F2F107
	dl DATA_F2F2F1
	dl DATA_F2F2F1
	dl DATA_F2F723
	dl DATA_F2F12F
	dl DATA_F2F979
	dl DATA_F2F979
	dl DATA_F2F2F1
	dl DATA_F2F2F1
	dl DATA_F2F2F1
	dl DATA_F2F107
	dl DATA_F2F107
	dl DATA_F2F107

DATA_EE11F6:
	db $00, $00, $00

global_sample_map:
	incbin "baserom.sfc":$2E11F9..$2E1223

DATA_EE1223:
	incbin "baserom.sfc":$2E1223..$2E124D

DATA_EE124D:
	incbin "baserom.sfc":$2E124D..$2E1263

DATA_EE1263:
	incbin "baserom.sfc":$2E1263..$2E127D

DATA_EE127D:
	incbin "baserom.sfc":$2E127D..$2E12C1

DATA_EE12C1:
	incbin "baserom.sfc":$2E12C1..$2E12D7

DATA_EE12D7:
	incbin "baserom.sfc":$2E12D7..$2E1337

DATA_EE1337:
	incbin "baserom.sfc":$2E1337..$2E135F

DATA_EE135F:
	incbin "baserom.sfc":$2E135F..$2E13B3

DATA_EE13B3:
	incbin "baserom.sfc":$2E13B3..$2E13D3

DATA_EE13D3:
	incbin "baserom.sfc":$2E13D3..$2E1437

DATA_EE1437:
	incbin "baserom.sfc":$2E1437..$2E144B

DATA_EE144B:
	incbin "baserom.sfc":$2E144B..$2E147F

DATA_EE147F:
	incbin "baserom.sfc":$2E147F..$2E1497

DATA_EE1497:
	incbin "baserom.sfc":$2E1497..$2E14D9

DATA_EE14D9:
	incbin "baserom.sfc":$2E14D9..$2E1545

DATA_EE1545:
	incbin "baserom.sfc":$2E1545..$2E157D

DATA_EE157D:
	incbin "baserom.sfc":$2E157D..$2E15C3

DATA_EE15C3:
	incbin "baserom.sfc":$2E15C3..$2E15D5

DATA_EE15D5:
	incbin "baserom.sfc":$2E15D5..$2E1601

DATA_EE1601:
	incbin "baserom.sfc":$2E1601..$2E1613

DATA_EE1613:
	incbin "baserom.sfc":$2E1613..$2E1623

DATA_EE1623:
	incbin "baserom.sfc":$2E1623..$2E1655

DATA_EE1655:
	incbin "baserom.sfc":$2E1655..$2E168D

DATA_EE168D:
	incbin "baserom.sfc":$2E168D..$2E16BF

DATA_EE16BF:
	incbin "baserom.sfc":$2E16BF..$2E16D5

DATA_EE16D5:
	incbin "baserom.sfc":$2E16D5..$2E16EF

DATA_EE16EF:
	incbin "baserom.sfc":$2E16EF..$2E1735

DATA_EE1735:
	incbin "baserom.sfc":$2E1735..$2E176D

DATA_EE176D:
	incbin "baserom.sfc":$2E176D..$2E1781

DATA_EE1781:
	incbin "baserom.sfc":$2E1781..$2E17B1

DATA_EE17B1:
	incbin "baserom.sfc":$2E17B1..$2E1837

DATA_EE1837:
	incbin "baserom.sfc":$2E1837..$2E1863

DATA_EE1863:
	incbin "baserom.sfc":$2E1863..$2E1895

DATA_EE1895:
	incbin "baserom.sfc":$2E1895..$2E18CB

DATA_EE18CB:
	incbin "baserom.sfc":$2E18CB..$2E18F1

DATA_EE18F1:
	incbin "baserom.sfc":$2E18F1..$2E1903

DATA_EE1903:
	incbin "baserom.sfc":$2E1903..$2E1935

DATA_EE1935:
	db $FF, $FF

DATA_EE1937:
	db $FF, $FF

DATA_EE1939:
	incbin "baserom.sfc":$2E1939..$2E195F

DATA_EE195F:
	incbin "baserom.sfc":$2E195F..$2E1F6E

DATA_EE1F6E:
	incbin "baserom.sfc":$2E1F6E..$2E2CD3

DATA_EE2CD3:
	incbin "baserom.sfc":$2E2CD3..$2E3D78

DATA_EE3D78:
	incbin "baserom.sfc":$2E3D78..$2E421F

DATA_EE421F:
	incbin "baserom.sfc":$2E421F..$2E52FC

DATA_EE52FC:
	incbin "baserom.sfc":$2E52FC..$2E6007

DATA_EE6007:
	incbin "baserom.sfc":$2E6007..$2E6F47

DATA_EE6F47:
	incbin "baserom.sfc":$2E6F47..$2E748C

DATA_EE748C:
	incbin "baserom.sfc":$2E748C..$2E8572

DATA_EE8572:
	incbin "baserom.sfc":$2E8572..$2E8BC0

DATA_EE8BC0:
	incbin "baserom.sfc":$2E8BC0..$2E9BE0

DATA_EE9BE0:
	incbin "baserom.sfc":$2E9BE0..$2EA050

DATA_EEA050:
	incbin "baserom.sfc":$2EA050..$2EAF4A

DATA_EEAF4A:
	incbin "baserom.sfc":$2EAF4A..$2EBDBD

DATA_EEBDBD:
	incbin "baserom.sfc":$2EBDBD..$2ECE62

DATA_EECE62:
	incbin "baserom.sfc":$2ECE62..$2ECFFC

DATA_EECFFC:
	incbin "baserom.sfc":$2ECFFC..$2ED1CC

DATA_EED1CC:
	incbin "baserom.sfc":$2ED1CC..$2ED47C

DATA_EED47C:
	incbin "baserom.sfc":$2ED47C..$2ED63A

DATA_EED63A:
	incbin "baserom.sfc":$2ED63A..$2ED87E

DATA_EED87E:
	incbin "baserom.sfc":$2ED87E..$2EDC73

DATA_EEDC73:
	incbin "baserom.sfc":$2EDC73..$2EE055

DATA_EEE055:
	incbin "baserom.sfc":$2EE055..$2EE1AF

DATA_EEE1AF:
	incbin "baserom.sfc":$2EE1AF..$2EE8CE

DATA_EEE8CE:
	incbin "baserom.sfc":$2EE8CE..$2EE9A2

DATA_EEE9A2:
	incbin "baserom.sfc":$2EE9A2..$2EEE4B

DATA_EEEE4B:
	incbin "baserom.sfc":$2EEE4B..$2EF6B6

DATA_EEF6B6:
	incbin "baserom.sfc":$2EF6B6..$2EF8C5

DATA_EEF8C5:
	incbin "baserom.sfc":$2EF8C5..$2EFDC8

DATA_EEFDC8:
	incbin "baserom.sfc":$2EFDC8..$2EFFDF

DATA_EEFFDF:
	incbin "baserom.sfc":$2EFFDF..$2F0000
