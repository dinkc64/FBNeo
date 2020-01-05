#define ADC_NES 												\
	{															\
		int c = (P & F_C);										\
		int sum = A + tmp + c;									\
		P &= ~(F_V | F_C);										\
		if( ~(A^tmp) & (A^sum) & F_N )							\
			P |= F_V;											\
		if( sum & 0xff00 )										\
			P |= F_C;											\
		A = (UINT8) sum;										\
	}															\
	SET_NZ(A)

/* N2A03 *******************************************************
 *  SBC Subtract with carry - no decimal mode
 ***************************************************************/
#define SBC_NES 												\
	{															\
		int c = (P & F_C) ^ F_C;								\
		int sum = A - tmp - c;									\
		P &= ~(F_V | F_C);										\
		if( (A^tmp) & (A^sum) & F_N )							\
			P |= F_V;											\
		if( (sum & 0xff00) == 0 )								\
			P |= F_C;											\
		A = (UINT8) sum;										\
	}															\
	SET_NZ(A)
