#include "driver.h"
#include "state.h"
#include "ym2413.h"

#ifndef MAME_INLINE
#ifndef MAME_INLINE
#define MAME_INLINE static inline
#endif
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif

/* output final shift */
#if (SAMPLE_BITS==16)
	#define FINAL_SH	(0)
	#define MAXOUT		(+32767)
	#define MINOUT		(-32768)
#else
	#define FINAL_SH	(8)
	#define MAXOUT		(+127)
	#define MINOUT		(-128)
#endif


#define FREQ_SH			16  /* 16.16 fixed point (frequency calculations) */
#define EG_SH			16  /* 16.16 fixed point (EG timing)              */
#define LFO_SH			24  /*  8.24 fixed point (LFO calculations)       */

#define FREQ_MASK		((1<<FREQ_SH)-1)

/* envelope output entries */
#define ENV_BITS		10
#define ENV_LEN			(1<<ENV_BITS)
#define ENV_STEP		(128.0/ENV_LEN)

#define MAX_ATT_INDEX	((1<<(ENV_BITS-2))-1) /*255*/
#define MIN_ATT_INDEX	(0)

/* sinwave entries */
#define SIN_BITS		10
#define SIN_LEN			(1<<SIN_BITS)
#define SIN_MASK		(SIN_LEN-1)

#define TL_RES_LEN		(256)	/* 8 bits addressing (real chip) */



/* register number to channel number , slot offset */
#define SLOT1 0
#define SLOT2 1

/* Envelope Generator phases */

#define EG_DMP			5
#define EG_ATT			4
#define EG_DEC			3
#define EG_SUS			2
#define EG_REL			1
#define EG_OFF			0


/* save output as raw 16-bit sample */

//#define SAVE_SAMPLE

#ifdef SAVE_SAMPLE
MAME_INLINE signed int acc_calc(signed int value)
{
	if (value>=0)
	{
		if (value < 0x0200)
			return (value & ~0);
		if (value < 0x0400)
			return (value & ~1);
		if (value < 0x0800)
			return (value & ~3);
		if (value < 0x1000)
			return (value & ~7);
		if (value < 0x2000)
			return (value & ~15);
		if (value < 0x4000)
			return (value & ~31);
		return (value & ~63);
	}
	/*else value < 0*/
	if (value > -0x0200)
		return (~abs(value) & ~0);
	if (value > -0x0400)
		return (~abs(value) & ~1);
	if (value > -0x0800)
		return (~abs(value) & ~3);
	if (value > -0x1000)
		return (~abs(value) & ~7);
	if (value > -0x2000)
		return (~abs(value) & ~15);
	if (value > -0x4000)
		return (~abs(value) & ~31);
	return (~abs(value) & ~63);
}


static FILE *sample[1];
	#if 0	/*save to MONO file */
		#define SAVE_ALL_CHANNELS \
		{	signed int pom = acc_calc(mo); \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
		}
	#else	/*save to STEREO file */
		#define SAVE_ALL_CHANNELS \
		{	signed int pom = mo; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
			pom = ro; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
		}
		#define SAVE_SEPARATE_CHANNEL(j) \
		{	signed int pom = outchan; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
			pom = chip->instvol_r[j]>>4; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
		}
	#endif
#endif

/*#define LOG_CYM_FILE*/
#ifdef LOG_CYM_FILE
	FILE * cymfile = NULL;
#endif




typedef struct{
	UINT32	ar;			/* attack rate: AR<<2			*/
	UINT32	dr;			/* decay rate:  DR<<2			*/
	UINT32	rr;			/* release rate:RR<<2			*/
	UINT8	KSR;		/* key scale rate				*/
	UINT8	ksl;		/* keyscale level				*/
	UINT8	ksr;		/* key scale rate: kcode>>KSR	*/
	UINT8	mul;		/* multiple: mul_tab[ML]		*/

	/* Phase Generator */
	UINT32	phase;		/* frequency counter			*/
	UINT32	freq;		/* frequency counter step		*/
	UINT8   fb_shift;	/* feedback shift value			*/
	INT32   op1_out[2];	/* slot1 output for feedback	*/

	/* Envelope Generator */
	UINT8	eg_type;	/* percussive/nonpercussive mode*/
	UINT8	state;		/* phase type					*/
	UINT32	TL;			/* total level: TL << 2			*/
	INT32	TLL;		/* adjusted now TL				*/
	INT32	volume;		/* envelope counter				*/
	UINT32	sl;			/* sustain level: sl_tab[SL]	*/

	UINT8	eg_sh_dp;	/* (dump state)					*/
	UINT8	eg_sel_dp;	/* (dump state)					*/
	UINT8	eg_sh_ar;	/* (attack state)				*/
	UINT8	eg_sel_ar;	/* (attack state)				*/
	UINT8	eg_sh_dr;	/* (decay state)				*/
	UINT8	eg_sel_dr;	/* (decay state)				*/
	UINT8	eg_sh_rr;	/* (release state for non-perc.)*/
	UINT8	eg_sel_rr;	/* (release state for non-perc.)*/
	UINT8	eg_sh_rs;	/* (release state for perc.mode)*/
	UINT8	eg_sel_rs;	/* (release state for perc.mode)*/

	UINT32	key;		/* 0 = KEY OFF, >0 = KEY ON		*/

	/* LFO */
	UINT32	AMmask;		/* LFO Amplitude Modulation enable mask */
	UINT8	vib;		/* LFO Phase Modulation enable flag (active high)*/

	/* waveform select */
	UINT32 wavetable;
} YM2413_OPLL_SLOT;

typedef struct{
    YM2413_OPLL_SLOT SLOT[2];
	/* phase generator state */
	UINT32  block_fnum;	/* block+fnum					*/
	UINT32  fc;			/* Freq. freqement base			*/
	UINT32  ksl_base;	/* KeyScaleLevel Base step		*/
	UINT8   kcode;		/* key code (for key scaling)	*/
	UINT8   sus;		/* sus on/off (release speed in percussive mode)*/
} YM2413_OPLL_CH;

/* chip state */
typedef struct {
    YM2413_OPLL_CH P_CH[9];                /* OPLL chips have 9 channels*/
	UINT8	instvol_r[9];			/* instrument/volume (or volume/volume in percussive mode)*/

	UINT32	eg_cnt;					/* global envelope generator counter	*/
	UINT32	eg_timer;				/* global envelope generator counter works at frequency = chipclock/72 */
	UINT32	eg_timer_add;			/* step of eg_timer						*/
	UINT32	eg_timer_overflow;		/* envelope generator timer overlfows every 1 sample (on real chip) */

	UINT8	rhythm;					/* Rhythm mode					*/

	/* LFO */
	UINT32	lfo_am_cnt;
	UINT32	lfo_am_inc;
	UINT32	lfo_pm_cnt;
	UINT32	lfo_pm_inc;

	UINT32	noise_rng;				/* 23 bit noise shift register	*/
	UINT32	noise_p;				/* current noise 'phase'		*/
	UINT32	noise_f;				/* current noise period			*/


/* instrument settings */
/*
	0-user instrument
	1-15 - fixed instruments
	16 -bass drum settings
	17,18 - other percussion instruments
*/
	UINT8 inst_tab[19][8];

	/* external event callback handlers */
	OPLL_UPDATEHANDLER UpdateHandler; /* stream update handler		*/
	int UpdateParam;				/* stream update parameter		*/

	UINT32	fn_tab[1024];			/* fnumber->increment counter	*/

	UINT8 address;					/* address register				*/
	UINT8 status;					/* status flag					*/

	int clock;						/* master clock  (Hz)			*/
	int rate;						/* sampling rate (Hz)			*/
	double freqbase;				/* frequency base				*/
} YM2413;

/* key scale level */
/* table is 3dB/octave, DV converts this into 6dB/octave */
/* 0.1875 is bit 0 weight of the envelope counter (volume) expressed in the 'decibel' scale */
#define DV (0.1875/1.0)
static const UINT32 ksl_tab[8*16]=
{
	/* OCT 0 */
	 0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
	 0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
	 0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
	 0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
	/* OCT 1 */
	 0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
	 0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
	 0.000/DV, 0.750/DV, 1.125/DV, 1.500/DV,
	 1.875/DV, 2.250/DV, 2.625/DV, 3.000/DV,
	/* OCT 2 */
	 0.000/DV, 0.000/DV, 0.000/DV, 0.000/DV,
	 0.000/DV, 1.125/DV, 1.875/DV, 2.625/DV,
	 3.000/DV, 3.750/DV, 4.125/DV, 4.500/DV,
	 4.875/DV, 5.250/DV, 5.625/DV, 6.000/DV,
	/* OCT 3 */
	 0.000/DV, 0.000/DV, 0.000/DV, 1.875/DV,
	 3.000/DV, 4.125/DV, 4.875/DV, 5.625/DV,
	 6.000/DV, 6.750/DV, 7.125/DV, 7.500/DV,
	 7.875/DV, 8.250/DV, 8.625/DV, 9.000/DV,
	/* OCT 4 */
	 0.000/DV, 0.000/DV, 3.000/DV, 4.875/DV,
	 6.000/DV, 7.125/DV, 7.875/DV, 8.625/DV,
	 9.000/DV, 9.750/DV,10.125/DV,10.500/DV,
	10.875/DV,11.250/DV,11.625/DV,12.000/DV,
	/* OCT 5 */
	 0.000/DV, 3.000/DV, 6.000/DV, 7.875/DV,
	 9.000/DV,10.125/DV,10.875/DV,11.625/DV,
	12.000/DV,12.750/DV,13.125/DV,13.500/DV,
	13.875/DV,14.250/DV,14.625/DV,15.000/DV,
	/* OCT 6 */
	 0.000/DV, 6.000/DV, 9.000/DV,10.875/DV,
	12.000/DV,13.125/DV,13.875/DV,14.625/DV,
	15.000/DV,15.750/DV,16.125/DV,16.500/DV,
	16.875/DV,17.250/DV,17.625/DV,18.000/DV,
	/* OCT 7 */
	 0.000/DV, 9.000/DV,12.000/DV,13.875/DV,
	15.000/DV,16.125/DV,16.875/DV,17.625/DV,
	18.000/DV,18.750/DV,19.125/DV,19.500/DV,
	19.875/DV,20.250/DV,20.625/DV,21.000/DV
};
#undef DV

/* sustain level table (3dB per step) */
/* 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,45 (dB)*/
#define SC(db) (UINT32) ( db * (1.0/ENV_STEP) )
static const UINT32 sl_tab[16]={
 SC( 0),SC( 1),SC( 2),SC(3 ),SC(4 ),SC(5 ),SC(6 ),SC( 7),
 SC( 8),SC( 9),SC(10),SC(11),SC(12),SC(13),SC(14),SC(15)
};
#undef SC


#define RATE_STEPS (8)
static const unsigned char eg_inc[15*RATE_STEPS]={

/*cycle:0 1  2 3  4 5  6 7*/

/* 0 */ 0,1, 0,1, 0,1, 0,1, /* rates 00..12 0 (increment by 0 or 1) */
/* 1 */ 0,1, 0,1, 1,1, 0,1, /* rates 00..12 1 */
/* 2 */ 0,1, 1,1, 0,1, 1,1, /* rates 00..12 2 */
/* 3 */ 0,1, 1,1, 1,1, 1,1, /* rates 00..12 3 */

/* 4 */ 1,1, 1,1, 1,1, 1,1, /* rate 13 0 (increment by 1) */
/* 5 */ 1,1, 1,2, 1,1, 1,2, /* rate 13 1 */
/* 6 */ 1,2, 1,2, 1,2, 1,2, /* rate 13 2 */
/* 7 */ 1,2, 2,2, 1,2, 2,2, /* rate 13 3 */

/* 8 */ 2,2, 2,2, 2,2, 2,2, /* rate 14 0 (increment by 2) */
/* 9 */ 2,2, 2,4, 2,2, 2,4, /* rate 14 1 */
/*10 */ 2,4, 2,4, 2,4, 2,4, /* rate 14 2 */
/*11 */ 2,4, 4,4, 2,4, 4,4, /* rate 14 3 */

/*12 */ 4,4, 4,4, 4,4, 4,4, /* rates 15 0, 15 1, 15 2, 15 3 (increment by 4) */
/*13 */ 8,8, 8,8, 8,8, 8,8, /* rates 15 2, 15 3 for attack */
/*14 */ 0,0, 0,0, 0,0, 0,0, /* infinity rates for attack and decay(s) */
};


#define O(a) (a*RATE_STEPS)

/*note that there is no O(13) in this table - it's directly in the code */
static const unsigned char eg_rate_select[16+64+16]={	/* Envelope Generator rates (16 + 64 rates + 16 RKS) */
/* 16 infinite time rates */
O(14),O(14),O(14),O(14),O(14),O(14),O(14),O(14),
O(14),O(14),O(14),O(14),O(14),O(14),O(14),O(14),

/* rates 00-12 */
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),

/* rate 13 */
O( 4),O( 5),O( 6),O( 7),

/* rate 14 */
O( 8),O( 9),O(10),O(11),

/* rate 15 */
O(12),O(12),O(12),O(12),

/* 16 dummy rates (same as 15 3) */
O(12),O(12),O(12),O(12),O(12),O(12),O(12),O(12),
O(12),O(12),O(12),O(12),O(12),O(12),O(12),O(12),

};
#undef O

/*rate  0,    1,    2,    3,    4,   5,   6,   7,  8,  9, 10, 11, 12, 13, 14, 15 */
/*shift 13,   12,   11,   10,   9,   8,   7,   6,  5,  4,  3,  2,  1,  0,  0,  0 */
/*mask  8191, 4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3,  1,  0,  0,  0 */

#define O(a) (a*1)
static const unsigned char eg_rate_shift[16+64+16]={	/* Envelope Generator counter shifts (16 + 64 rates + 16 RKS) */
/* 16 infinite time rates */
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),

/* rates 00-12 */
O(13),O(13),O(13),O(13),
O(12),O(12),O(12),O(12),
O(11),O(11),O(11),O(11),
O(10),O(10),O(10),O(10),
O( 9),O( 9),O( 9),O( 9),
O( 8),O( 8),O( 8),O( 8),
O( 7),O( 7),O( 7),O( 7),
O( 6),O( 6),O( 6),O( 6),
O( 5),O( 5),O( 5),O( 5),
O( 4),O( 4),O( 4),O( 4),
O( 3),O( 3),O( 3),O( 3),
O( 2),O( 2),O( 2),O( 2),
O( 1),O( 1),O( 1),O( 1),

/* rate 13 */
O( 0),O( 0),O( 0),O( 0),

/* rate 14 */
O( 0),O( 0),O( 0),O( 0),

/* rate 15 */
O( 0),O( 0),O( 0),O( 0),

/* 16 dummy rates (same as 15 3) */
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),

};
#undef O


/* multiple table */
#define ML 2
static const UINT8 mul_tab[16]= {
/* 1/2, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,10,12,12,15,15 */
   0.50*ML, 1.00*ML, 2.00*ML, 3.00*ML, 4.00*ML, 5.00*ML, 6.00*ML, 7.00*ML,
   8.00*ML, 9.00*ML,10.00*ML,10.00*ML,12.00*ML,12.00*ML,15.00*ML,15.00*ML
};
#undef ML

/*	TL_TAB_LEN is calculated as:
*	11 - sinus amplitude bits     (Y axis)
*	2  - sinus sign bit           (Y axis)
*	TL_RES_LEN - sinus resolution (X axis)
*/
#define TL_TAB_LEN (11*2*TL_RES_LEN)
static signed int tl_tab[TL_TAB_LEN];

#define ENV_QUIET		(TL_TAB_LEN>>5)

/* sin waveform table in 'decibel' scale */
/* two waveforms on OPLL type chips */
static unsigned int sin_tab[SIN_LEN * 2];


/* LFO Amplitude Modulation table (verified on real YM3812)
   27 output levels (triangle waveform); 1 level takes one of: 192, 256 or 448 samples

   Length: 210 elements.

	Each of the elements has to be repeated
	exactly 64 times (on 64 consecutive samples).
	The whole table takes: 64 * 210 = 13440 samples.

We use data>>1, until we find what it really is on real chip...

*/

#define LFO_AM_TAB_ELEMENTS 210

static const UINT8 lfo_am_table[LFO_AM_TAB_ELEMENTS] = {
0,0,0,0,0,0,0,
1,1,1,1,
2,2,2,2,
3,3,3,3,
4,4,4,4,
5,5,5,5,
6,6,6,6,
7,7,7,7,
8,8,8,8,
9,9,9,9,
10,10,10,10,
11,11,11,11,
12,12,12,12,
13,13,13,13,
14,14,14,14,
15,15,15,15,
16,16,16,16,
17,17,17,17,
18,18,18,18,
19,19,19,19,
20,20,20,20,
21,21,21,21,
22,22,22,22,
23,23,23,23,
24,24,24,24,
25,25,25,25,
26,26,26,
25,25,25,25,
24,24,24,24,
23,23,23,23,
22,22,22,22,
21,21,21,21,
20,20,20,20,
19,19,19,19,
18,18,18,18,
17,17,17,17,
16,16,16,16,
15,15,15,15,
14,14,14,14,
13,13,13,13,
12,12,12,12,
11,11,11,11,
10,10,10,10,
9,9,9,9,
8,8,8,8,
7,7,7,7,
6,6,6,6,
5,5,5,5,
4,4,4,4,
3,3,3,3,
2,2,2,2,
1,1,1,1
};

/* LFO Phase Modulation table (verified on real YM2413) */
static const INT8 lfo_pm_table[8*8] = {

/* FNUM2/FNUM = 0 00xxxxxx (0x0000) */
0, 0, 0, 0, 0, 0, 0, 0,

/* FNUM2/FNUM = 0 01xxxxxx (0x0040) */
1, 0, 0, 0,-1, 0, 0, 0,

/* FNUM2/FNUM = 0 10xxxxxx (0x0080) */
2, 1, 0,-1,-2,-1, 0, 1,

/* FNUM2/FNUM = 0 11xxxxxx (0x00C0) */
3, 1, 0,-1,-3,-1, 0, 1,

/* FNUM2/FNUM = 1 00xxxxxx (0x0100) */
4, 2, 0,-2,-4,-2, 0, 2,

/* FNUM2/FNUM = 1 01xxxxxx (0x0140) */
5, 2, 0,-2,-5,-2, 0, 2,

/* FNUM2/FNUM = 1 10xxxxxx (0x0180) */
6, 3, 0,-3,-6,-3, 0, 3,

/* FNUM2/FNUM = 1 11xxxxxx (0x01C0) */
7, 3, 0,-3,-7,-3, 0, 3,
};






/* This is not 100% perfect yet but very close */
/*
 - multi parameters are 100% correct (instruments and drums)
 - LFO PM and AM enable are 100% correct
 - waveform DC and DM select are 100% correct
*/

static unsigned char table[19][8] = {
/* MULT  MULT modTL DcDmFb AR/DR AR/DR SL/RR SL/RR */
/*   0     1     2     3     4     5     6    7    */
  {0x49, 0x4c, 0x4c, 0x12, 0x00, 0x00, 0x00, 0x00 },	//0

  {0x61, 0x61, 0x1e, 0x17, 0xf0, 0x78, 0x00, 0x17 },	//1
  {0x13, 0x41, 0x1e, 0x0d, 0xd7, 0xf7, 0x13, 0x13 },	//2
  {0x13, 0x01, 0x99, 0x04, 0xf2, 0xf4, 0x11, 0x23 },	//3
  {0x21, 0x61, 0x1b, 0x07, 0xaf, 0x64, 0x40, 0x27 },	//4

//{0x22, 0x21, 0x1e, 0x09, 0xf0, 0x76, 0x08, 0x28 },	//5
  {0x22, 0x21, 0x1e, 0x06, 0xf0, 0x75, 0x08, 0x18 },	//5

//{0x31, 0x22, 0x16, 0x09, 0x90, 0x7f, 0x00, 0x08 },	//6
  {0x31, 0x22, 0x16, 0x05, 0x90, 0x71, 0x00, 0x13 },	//6

  {0x21, 0x61, 0x1d, 0x07, 0x82, 0x80, 0x10, 0x17 },	//7
  {0x23, 0x21, 0x2d, 0x16, 0xc0, 0x70, 0x07, 0x07 },	//8
  {0x61, 0x61, 0x1b, 0x06, 0x64, 0x65, 0x10, 0x17 },	//9

//{0x61, 0x61, 0x0c, 0x08, 0x85, 0xa0, 0x79, 0x07 },	//A
  {0x61, 0x61, 0x0c, 0x18, 0x85, 0xf0, 0x70, 0x07 },	//A

  {0x23, 0x01, 0x07, 0x11, 0xf0, 0xa4, 0x00, 0x22 },	//B
  {0x97, 0xc1, 0x24, 0x07, 0xff, 0xf8, 0x22, 0x12 },	//C

//{0x61, 0x10, 0x0c, 0x08, 0xf2, 0xc4, 0x40, 0xc8 },	//D
  {0x61, 0x10, 0x0c, 0x05, 0xf2, 0xf4, 0x40, 0x44 },	//D

  {0x01, 0x01, 0x55, 0x03, 0xf3, 0x92, 0xf3, 0xf3 },	//E
  {0x61, 0x41, 0x89, 0x03, 0xf1, 0xf4, 0xf0, 0x13 },	//F

/* drum instruments definitions */
/* MULTI MULTI modTL  xxx  AR/DR AR/DR SL/RR SL/RR */
/*   0     1     2     3     4     5     6    7    */
  {0x01, 0x01, 0x16, 0x00, 0xfd, 0xf8, 0x2f, 0x6d },/* BD(multi verified, modTL verified, mod env - verified(close), carr. env verifed) */
  {0x01, 0x01, 0x00, 0x00, 0xd8, 0xd8, 0xf9, 0xf8 },/* HH(multi verified), SD(multi not used) */
  {0x05, 0x01, 0x00, 0x00, 0xf8, 0xba, 0x49, 0x55 },/* TOM(multi,env verified), TOP CYM(multi verified, env verified) */
};

/* lock level of common table */
static int num_lock = 0;

/* work table */
static void *cur_chip = NULL;	/* current chip pointer */
static YM2413_OPLL_SLOT *SLOT7_1,*SLOT7_2,*SLOT8_1,*SLOT8_2;

static signed int output[2];
static signed int outchan;

static UINT32	LFO_AM;
static INT32	LFO_PM;


MAME_INLINE int limit( int val, int max, int min ) {
	if ( val > max )
		val = max;
	else if ( val < min )
		val = min;

	return val;
}


/* advance LFO to next sample */
MAME_INLINE void advance_lfo(YM2413 *chip)
{
	/* LFO */
	chip->lfo_am_cnt += chip->lfo_am_inc;
	if (chip->lfo_am_cnt >= (LFO_AM_TAB_ELEMENTS<<LFO_SH) )	/* lfo_am_table is 210 elements long */
		chip->lfo_am_cnt -= (LFO_AM_TAB_ELEMENTS<<LFO_SH);

	LFO_AM = lfo_am_table[ chip->lfo_am_cnt >> LFO_SH ] >> 1;

	chip->lfo_pm_cnt += chip->lfo_pm_inc;
	LFO_PM = (chip->lfo_pm_cnt>>LFO_SH) & 7;
}

/* advance to next sample */
MAME_INLINE void advance(YM2413 *chip)
{
    YM2413_OPLL_CH *CH;
    YM2413_OPLL_SLOT *op;
	unsigned int i;

//profiler_mark(PROFILER_USER3);

	/* Envelope Generator */
	chip->eg_timer += chip->eg_timer_add;

	while (chip->eg_timer >= chip->eg_timer_overflow)
	{
		chip->eg_timer -= chip->eg_timer_overflow;

		chip->eg_cnt++;

		for (i=0; i<9*2; i++)
		{
			CH  = &chip->P_CH[i/2];

			op  = &CH->SLOT[i&1];

			switch(op->state)
			{

			case EG_DMP:		/* dump phase */
			/*dump phase is performed by both operators in each channel*/
			/*when CARRIER envelope gets down to zero level,
			**  phases in BOTH opearators are reset (at the same time ?)
			*/
				if ( !(chip->eg_cnt & ((1<<op->eg_sh_dp)-1) ) )
				{
					op->volume += eg_inc[op->eg_sel_dp + ((chip->eg_cnt>>op->eg_sh_dp)&7)];

					if ( op->volume >= MAX_ATT_INDEX )
					{
						op->volume = MAX_ATT_INDEX;
						op->state = EG_ATT;
						/* restart Phase Generator  */
						op->phase = 0;
					}
				}
			break;

			case EG_ATT:		/* attack phase */
				if ( !(chip->eg_cnt & ((1<<op->eg_sh_ar)-1) ) )
				{
					op->volume += (~op->volume *
	                        		           (eg_inc[op->eg_sel_ar + ((chip->eg_cnt>>op->eg_sh_ar)&7)])
        			                          ) >>2;

					if (op->volume <= MIN_ATT_INDEX)
					{
						op->volume = MIN_ATT_INDEX;
						op->state = EG_DEC;
					}
				}
			break;

			case EG_DEC:	/* decay phase */
				if ( !(chip->eg_cnt & ((1<<op->eg_sh_dr)-1) ) )
				{
					op->volume += eg_inc[op->eg_sel_dr + ((chip->eg_cnt>>op->eg_sh_dr)&7)];

					if ( op->volume >= op->sl )
						op->state = EG_SUS;
				}
			break;

			case EG_SUS:	/* sustain phase */
				/* this is important behaviour:
				one can change percusive/non-percussive modes on the fly and
				the chip will remain in sustain phase - verified on real YM3812 */

				if(op->eg_type)		/* non-percussive mode (sustained tone) */
				{
									/* do nothing */
				}
				else				/* percussive mode */
				{
					/* during sustain phase chip adds Release Rate (in percussive mode) */
					if ( !(chip->eg_cnt & ((1<<op->eg_sh_rr)-1) ) )
					{
						op->volume += eg_inc[op->eg_sel_rr + ((chip->eg_cnt>>op->eg_sh_rr)&7)];

						if ( op->volume >= MAX_ATT_INDEX )
							op->volume = MAX_ATT_INDEX;
					}
					/* else do nothing in sustain phase */
				}
			break;

			case EG_REL:	/* release phase */
			/* exclude modulators in melody channels from performing anything in this mode*/
			/* allowed are only carriers in melody mode and rhythm slots in rhythm mode */

			/*This table shows which operators and on what conditions are allowed to perform EG_REL:
			(a) - always perform EG_REL
			(n) - never perform EG_REL
			(r) - perform EG_REL in Rhythm mode ONLY
				0: 0 (n),  1 (a)
				1: 2 (n),  3 (a)
				2: 4 (n),  5 (a)
				3: 6 (n),  7 (a)
				4: 8 (n),  9 (a)
				5: 10(n),  11(a)
				6: 12(r),  13(a)
				7: 14(r),  15(a)
				8: 16(r),  17(a)
			*/
				if ( (i&1) || ((chip->rhythm&0x20) && (i>=12)) )/* exclude modulators */
				{
					if(op->eg_type)		/* non-percussive mode (sustained tone) */
					/*this is correct: use RR when SUS = OFF*/
					/*and use RS when SUS = ON*/
					{
						if (CH->sus)
						{
							if ( !(chip->eg_cnt & ((1<<op->eg_sh_rs)-1) ) )
							{
								op->volume += eg_inc[op->eg_sel_rs + ((chip->eg_cnt>>op->eg_sh_rs)&7)];
								if ( op->volume >= MAX_ATT_INDEX )
								{
									op->volume = MAX_ATT_INDEX;
									op->state = EG_OFF;
								}
							}
						}
						else
						{
							if ( !(chip->eg_cnt & ((1<<op->eg_sh_rr)-1) ) )
							{
								op->volume += eg_inc[op->eg_sel_rr + ((chip->eg_cnt>>op->eg_sh_rr)&7)];
								if ( op->volume >= MAX_ATT_INDEX )
								{
									op->volume = MAX_ATT_INDEX;
									op->state = EG_OFF;
								}
							}
						}
					}
					else				/* percussive mode */
					{
						if ( !(chip->eg_cnt & ((1<<op->eg_sh_rs)-1) ) )
						{
							op->volume += eg_inc[op->eg_sel_rs + ((chip->eg_cnt>>op->eg_sh_rs)&7)];
							if ( op->volume >= MAX_ATT_INDEX )
							{
								op->volume = MAX_ATT_INDEX;
								op->state = EG_OFF;
							}
						}
					}
				}
			break;

			default:
			break;
			}
		}
	}

//profiler_mark(PROFILER_END);

//profiler_mark(PROFILER_USER4);

	for (i=0; i<9*2; i++)
	{
		CH  = &chip->P_CH[i/2];
		op  = &CH->SLOT[i&1];

		/* Phase Generator */
		if(op->vib)
		{
			UINT8 block;

			unsigned int fnum_lfo   = 8*((CH->block_fnum&0x01c0) >> 6);
			unsigned int block_fnum = CH->block_fnum * 2;
			signed int lfo_fn_table_index_offset = lfo_pm_table[LFO_PM + fnum_lfo ];

			if (lfo_fn_table_index_offset)	/* LFO phase modulation active */
			{
				block_fnum += lfo_fn_table_index_offset;
				block = (block_fnum&0x1c00) >> 10;
				op->phase += (chip->fn_tab[block_fnum&0x03ff] >> (7-block)) * op->mul;
			}
			else	/* LFO phase modulation  = zero */
			{
				op->phase += op->freq;
			}
		}
		else	/* LFO phase modulation disabled for this operator */
		{
			op->phase += op->freq;
		}
	}

	/*	The Noise Generator of the YM3812 is 23-bit shift register.
	*	Period is equal to 2^23-2 samples.
	*	Register works at sampling frequency of the chip, so output
	*	can change on every sample.
	*
	*	Output of the register and input to the bit 22 is:
	*	bit0 XOR bit14 XOR bit15 XOR bit22
	*
	*	Simply use bit 22 as the noise output.
	*/

	chip->noise_p += chip->noise_f;
	i = chip->noise_p >> FREQ_SH;		/* number of events (shifts of the shift register) */
	chip->noise_p &= FREQ_MASK;
	while (i)
	{
		/*
		UINT32 j;
		j = ( (chip->noise_rng) ^ (chip->noise_rng>>14) ^ (chip->noise_rng>>15) ^ (chip->noise_rng>>22) ) & 1;
		chip->noise_rng = (j<<22) | (chip->noise_rng>>1);
		*/

		/*
			Instead of doing all the logic operations above, we
			use a trick here (and use bit 0 as the noise output).
			The difference is only that the noise bit changes one
			step ahead. This doesn't matter since we don't know
			what is real state of the noise_rng after the reset.
		*/

		if (chip->noise_rng & 1) chip->noise_rng ^= 0x800302;
		chip->noise_rng >>= 1;

		i--;
	}
//profiler_mark(PROFILER_END);
}


MAME_INLINE signed int op_calc(UINT32 phase, unsigned int env, signed int pm, unsigned int wave_tab)
{
	UINT32 p;

	p = (env<<5) + sin_tab[wave_tab + ((((signed int)((phase & ~FREQ_MASK) + (pm<<17))) >> FREQ_SH ) & SIN_MASK) ];

	if (p >= TL_TAB_LEN)
		return 0;
	return tl_tab[p];
}

MAME_INLINE signed int op_calc1(UINT32 phase, unsigned int env, signed int pm, unsigned int wave_tab)
{
	UINT32 p;
	INT32  i;

	i = (phase & ~FREQ_MASK) + pm;

/*logerror("i=%08x (i>>16)&511=%8i phase=%i [pm=%08x] ",i, (i>>16)&511, phase>>FREQ_SH, pm);*/

	p = (env<<5) + sin_tab[ wave_tab + ((i>>FREQ_SH) & SIN_MASK)];

/*logerror("(p&255=%i p>>8=%i) out= %i\n", p&255,p>>8, tl_tab[p&255]>>(p>>8) );*/

	if (p >= TL_TAB_LEN)
		return 0;
	return tl_tab[p];
}


#define volume_calc(OP) ((OP)->TLL + ((UINT32)(OP)->volume) + (LFO_AM & (OP)->AMmask))

/* calculate output */
MAME_INLINE void chan_calc( YM2413_OPLL_CH *CH )
{
    YM2413_OPLL_SLOT *SLOT;
	unsigned int env;
	signed int out;
	signed int phase_modulation;	/* phase modulation input (SLOT 2) */


	/* SLOT 1 */
	SLOT = &CH->SLOT[SLOT1];
	env  = volume_calc(SLOT);
	out  = SLOT->op1_out[0] + SLOT->op1_out[1];

	SLOT->op1_out[0] = SLOT->op1_out[1];
	phase_modulation = SLOT->op1_out[0];

	SLOT->op1_out[1] = 0;

	if( env < ENV_QUIET )
	{
		if (!SLOT->fb_shift)
			out = 0;
		SLOT->op1_out[1] = op_calc1(SLOT->phase, env, (out<<SLOT->fb_shift), SLOT->wavetable );
	}

	/* SLOT 2 */

outchan=0;

	SLOT++;
	env = volume_calc(SLOT);
	if( env < ENV_QUIET )
	{
		signed int outp = op_calc(SLOT->phase, env, phase_modulation, SLOT->wavetable);
		output[0] += outp;
		outchan = outp;
		//output[0] += op_calc(SLOT->phase, env, phase_modulation, SLOT->wavetable);
	}
}

/*
	operators used in the rhythm sounds generation process:

	Envelope Generator:

channel  operator  register number   Bass  High  Snare Tom  Top
/ slot   number    TL ARDR SLRR Wave Drum  Hat   Drum  Tom  Cymbal
 6 / 0   12        50  70   90   f0  +
 6 / 1   15        53  73   93   f3  +
 7 / 0   13        51  71   91   f1        +
 7 / 1   16        54  74   94   f4              +
 8 / 0   14        52  72   92   f2                    +
 8 / 1   17        55  75   95   f5                          +

	Phase Generator:

channel  operator  register number   Bass  High  Snare Tom  Top
/ slot   number    MULTIPLE          Drum  Hat   Drum  Tom  Cymbal
 6 / 0   12        30                +
 6 / 1   15        33                +
 7 / 0   13        31                      +     +           +
 7 / 1   16        34                -----  n o t  u s e d -----
 8 / 0   14        32                                  +
 8 / 1   17        35                      +                 +

channel  operator  register number   Bass  High  Snare Tom  Top
number   number    BLK/FNUM2 FNUM    Drum  Hat   Drum  Tom  Cymbal
   6     12,15     B6        A6      +

   7     13,16     B7        A7            +     +           +

   8     14,17     B8        A8            +           +     +

*/

/* calculate rhythm */

MAME_INLINE void rhythm_calc( YM2413_OPLL_CH *CH, unsigned int noise )
{
    YM2413_OPLL_SLOT *SLOT;
	signed int out;
	unsigned int env;
	signed int phase_modulation;	/* phase modulation input (SLOT 2) */


	/* Bass Drum (verified on real YM3812):
	  - depends on the channel 6 'connect' register:
	      when connect = 0 it works the same as in normal (non-rhythm) mode (op1->op2->out)
	      when connect = 1 _only_ operator 2 is present on output (op2->out), operator 1 is ignored
	  - output sample always is multiplied by 2
	*/


	/* SLOT 1 */
	SLOT = &CH[6].SLOT[SLOT1];
	env = volume_calc(SLOT);

	out = SLOT->op1_out[0] + SLOT->op1_out[1];
	SLOT->op1_out[0] = SLOT->op1_out[1];

	phase_modulation = SLOT->op1_out[0];

	SLOT->op1_out[1] = 0;
	if( env < ENV_QUIET )
	{
		if (!SLOT->fb_shift)
			out = 0;
		SLOT->op1_out[1] = op_calc1(SLOT->phase, env, (out<<SLOT->fb_shift), SLOT->wavetable );
	}

	/* SLOT 2 */
	SLOT++;
	env = volume_calc(SLOT);
	if( env < ENV_QUIET )
		output[1] += op_calc(SLOT->phase, env, phase_modulation, SLOT->wavetable) * 2;


	/* Phase generation is based on: */
	// HH  (13) channel 7->slot 1 combined with channel 8->slot 2 (same combination as TOP CYMBAL but different output phases)
	// SD  (16) channel 7->slot 1
	// TOM (14) channel 8->slot 1
	// TOP (17) channel 7->slot 1 combined with channel 8->slot 2 (same combination as HIGH HAT but different output phases)

	/* Envelope generation based on: */
	// HH  channel 7->slot1
	// SD  channel 7->slot2
	// TOM channel 8->slot1
	// TOP channel 8->slot2


	/* The following formulas can be well optimized.
	   I leave them in direct form for now (in case I've missed something).
	*/

	/* High Hat (verified on real YM3812) */
	env = volume_calc(SLOT7_1);
	if( env < ENV_QUIET )
	{

		/* high hat phase generation:
			phase = d0 or 234 (based on frequency only)
			phase = 34 or 2d0 (based on noise)
		*/

		/* base frequency derived from operator 1 in channel 7 */
		unsigned char bit7 = ((SLOT7_1->phase>>FREQ_SH)>>7)&1;
		unsigned char bit3 = ((SLOT7_1->phase>>FREQ_SH)>>3)&1;
		unsigned char bit2 = ((SLOT7_1->phase>>FREQ_SH)>>2)&1;

		unsigned char res1 = (bit2 ^ bit7) | bit3;

		/* when res1 = 0 phase = 0x000 | 0xd0; */
		/* when res1 = 1 phase = 0x200 | (0xd0>>2); */
		UINT32 phase = res1 ? (0x200|(0xd0>>2)) : 0xd0;

		/* enable gate based on frequency of operator 2 in channel 8 */
		unsigned char bit5e= ((SLOT8_2->phase>>FREQ_SH)>>5)&1;
		unsigned char bit3e= ((SLOT8_2->phase>>FREQ_SH)>>3)&1;

		unsigned char res2 = (bit3e | bit5e);

		/* when res2 = 0 pass the phase from calculation above (res1); */
		/* when res2 = 1 phase = 0x200 | (0xd0>>2); */
		if (res2)
			phase = (0x200|(0xd0>>2));


		/* when phase & 0x200 is set and noise=1 then phase = 0x200|0xd0 */
		/* when phase & 0x200 is set and noise=0 then phase = 0x200|(0xd0>>2), ie no change */
		if (phase&0x200)
		{
			if (noise)
				phase = 0x200|0xd0;
		}
		else
		/* when phase & 0x200 is clear and noise=1 then phase = 0xd0>>2 */
		/* when phase & 0x200 is clear and noise=0 then phase = 0xd0, ie no change */
		{
			if (noise)
				phase = 0xd0>>2;
		}

		output[1] += op_calc(phase<<FREQ_SH, env, 0, SLOT7_1->wavetable) * 2;
	}

	/* Snare Drum (verified on real YM3812) */
	env = volume_calc(SLOT7_2);
	if( env < ENV_QUIET )
	{
		/* base frequency derived from operator 1 in channel 7 */
		unsigned char bit8 = ((SLOT7_1->phase>>FREQ_SH)>>8)&1;

		/* when bit8 = 0 phase = 0x100; */
		/* when bit8 = 1 phase = 0x200; */
		UINT32 phase = bit8 ? 0x200 : 0x100;

		/* Noise bit XOR'es phase by 0x100 */
		/* when noisebit = 0 pass the phase from calculation above */
		/* when noisebit = 1 phase ^= 0x100; */
		/* in other words: phase ^= (noisebit<<8); */
		if (noise)
			phase ^= 0x100;

		output[1] += op_calc(phase<<FREQ_SH, env, 0, SLOT7_2->wavetable) * 2;
	}

	/* Tom Tom (verified on real YM3812) */
	env = volume_calc(SLOT8_1);
	if( env < ENV_QUIET )
		output[1] += op_calc(SLOT8_1->phase, env, 0, SLOT8_1->wavetable) * 2;

	/* Top Cymbal (verified on real YM2413) */
	env = volume_calc(SLOT8_2);
	if( env < ENV_QUIET )
	{
		/* base frequency derived from operator 1 in channel 7 */
		unsigned char bit7 = ((SLOT7_1->phase>>FREQ_SH)>>7)&1;
		unsigned char bit3 = ((SLOT7_1->phase>>FREQ_SH)>>3)&1;
		unsigned char bit2 = ((SLOT7_1->phase>>FREQ_SH)>>2)&1;

		unsigned char res1 = (bit2 ^ bit7) | bit3;

		/* when res1 = 0 phase = 0x000 | 0x100; */
		/* when res1 = 1 phase = 0x200 | 0x100; */
		UINT32 phase = res1 ? 0x300 : 0x100;

		/* enable gate based on frequency of operator 2 in channel 8 */
		unsigned char bit5e= ((SLOT8_2->phase>>FREQ_SH)>>5)&1;
		unsigned char bit3e= ((SLOT8_2->phase>>FREQ_SH)>>3)&1;

		unsigned char res2 = (bit3e | bit5e);
		/* when res2 = 0 pass the phase from calculation above (res1); */
		/* when res2 = 1 phase = 0x200 | 0x100; */
		if (res2)
			phase = 0x300;

		output[1] += op_calc(phase<<FREQ_SH, env, 0, SLOT8_2->wavetable) * 2;
	}

}


/* generic table initialize */
static int init_tables(void)
{
	signed int i,x;
	signed int n;
	double o,m;


	for (x=0; x<TL_RES_LEN; x++)
	{
		m = (1<<16) / pow(2, (x+1) * (ENV_STEP/4.0) / 8.0);
		m = floor(m);

		/* we never reach (1<<16) here due to the (x+1) */
		/* result fits within 16 bits at maximum */

		n = (int)m;		/* 16 bits here */
		n >>= 4;		/* 12 bits here */
		if (n&1)		/* round to nearest */
			n = (n>>1)+1;
		else
			n = n>>1;
						/* 11 bits here (rounded) */
		tl_tab[ x*2 + 0 ] = n;
		tl_tab[ x*2 + 1 ] = -tl_tab[ x*2 + 0 ];

		for (i=1; i<11; i++)
		{
			tl_tab[ x*2+0 + i*2*TL_RES_LEN ] =  tl_tab[ x*2+0 ]>>i;
			tl_tab[ x*2+1 + i*2*TL_RES_LEN ] = -tl_tab[ x*2+0 + i*2*TL_RES_LEN ];
		}
	#if 0
			logerror("tl %04i", x*2);
			for (i=0; i<11; i++)
				logerror(", [%02i] %5i", i*2, tl_tab[ x*2 /*+1*/ + i*2*TL_RES_LEN ] );
			logerror("\n");
	#endif
	}
	/*logerror("ym2413.c: TL_TAB_LEN = %i elements (%i bytes)\n",TL_TAB_LEN, (int)sizeof(tl_tab));*/


	for (i=0; i<SIN_LEN; i++)
	{
		/* non-standard sinus */
		m = sin( ((i*2)+1) * PI / SIN_LEN ); /* checked against the real chip */

		/* we never reach zero here due to ((i*2)+1) */

		if (m>0.0)
			o = 8*log(1.0/m)/log(2);	/* convert to 'decibels' */
		else
			o = 8*log(-1.0/m)/log(2);	/* convert to 'decibels' */

		o = o / (ENV_STEP/4);

		n = (int)(2.0*o);
		if (n&1)						/* round to nearest */
			n = (n>>1)+1;
		else
			n = n>>1;

		/* waveform 0: standard sinus  */
		sin_tab[ i ] = n*2 + (m>=0.0? 0: 1 );

		/*logerror("ym2413.c: sin [%4i (hex=%03x)]= %4i (tl_tab value=%5i)\n", i, i, sin_tab[i], tl_tab[sin_tab[i]] );*/


		/* waveform 1:  __      __     */
		/*             /  \____/  \____*/
		/* output only first half of the sinus waveform (positive one) */
		if (i & (1<<(SIN_BITS-1)) )
			sin_tab[1*SIN_LEN+i] = TL_TAB_LEN;
		else
			sin_tab[1*SIN_LEN+i] = sin_tab[i];

		/*logerror("ym2413.c: sin1[%4i]= %4i (tl_tab value=%5i)\n", i, sin_tab[1*SIN_LEN+i], tl_tab[sin_tab[1*SIN_LEN+i]] );*/
	}
#if 0
	logerror("YM2413.C: ENV_QUIET= %08x (*32=%08x)\n", ENV_QUIET, ENV_QUIET*32 );
	for (i=0; i<ENV_QUIET; i++)
	{
		logerror("tl_tb[%4x(%4i)]=%8x\n", i<<5, i, tl_tab[i<<5] );
	}
#endif
#ifdef SAVE_SAMPLE
	sample[0]=fopen("sampsum.pcm","wb");
#endif

	return 1;
}

static void OPLCloseTable( void )
{
#ifdef SAVE_SAMPLE
	fclose(sample[0]);
#endif
}

static void OPLL_initalize(YM2413 *chip)
{
	int i;
	
	/* frequency base */
	chip->freqbase  = (chip->rate) ? ((double)chip->clock / 72.0) / chip->rate  : 0;
#if 0
	chip->rate = (double)chip->clock / 72.0;
	chip->freqbase  = 1.0;
	logerror("freqbase=%f\n", chip->freqbase);
#endif

	/* make fnumber -> increment counter table */
	for( i = 0 ; i < 1024; i++ )
	{
		/* OPLL (YM2413) phase increment counter = 18bit */

		chip->fn_tab[i] = (UINT32)( (double)i * 64 * chip->freqbase * (1<<(FREQ_SH-10)) ); /* -10 because chip works with 10.10 fixed point, while we use 16.16 */
#if 0
		logerror("ym2413.c: fn_tab[%4i] = %08x (dec=%8i)\n",
				 i, chip->fn_tab[i]>>6, chip->fn_tab[i]>>6 );
#endif
	}

#if 0
	for( i=0 ; i < 16 ; i++ )
	{
		logerror("ym2413.c: sl_tab[%i] = %08x\n", i, sl_tab[i] );
	}
	for( i=0 ; i < 8 ; i++ )
	{
		int j;
		logerror("ym2413.c: ksl_tab[oct=%2i] =",i);
		for (j=0; j<16; j++)
		{
			logerror("%08x ", ksl_tab[i*16+j] );
		}
		logerror("\n");
	}
#endif


	/* Amplitude modulation: 27 output levels (triangle waveform); 1 level takes one of: 192, 256 or 448 samples */
	/* One entry from LFO_AM_TABLE lasts for 64 samples */
	chip->lfo_am_inc = (1.0 / 64.0 ) * (1<<LFO_SH) * chip->freqbase;

	/* Vibrato: 8 output levels (triangle waveform); 1 level takes 1024 samples */
	chip->lfo_pm_inc = (1.0 / 1024.0) * (1<<LFO_SH) * chip->freqbase;

	/*logerror ("chip->lfo_am_inc = %8x ; chip->lfo_pm_inc = %8x\n", chip->lfo_am_inc, chip->lfo_pm_inc);*/

	/* Noise generator: a step takes 1 sample */
	chip->noise_f = (1.0 / 1.0) * (1<<FREQ_SH) * chip->freqbase;
	/*logerror("YM2413init noise_f=%8x\n", chip->noise_f);*/

	chip->eg_timer_add  = (1<<EG_SH)  * chip->freqbase;
	chip->eg_timer_overflow = ( 1 ) * (1<<EG_SH);
	/*logerror("YM2413init eg_timer_add=%8x eg_timer_overflow=%8x\n", chip->eg_timer_add, chip->eg_timer_overflow);*/

}

MAME_INLINE void KEY_ON(YM2413_OPLL_SLOT *SLOT, UINT32 key_set)
{
	if( !SLOT->key )
	{
		/* do NOT restart Phase Generator (verified on real YM2413)*/
		/* phase -> Dump */
		SLOT->state = EG_DMP;
	}
	SLOT->key |= key_set;
}

MAME_INLINE void KEY_OFF(YM2413_OPLL_SLOT *SLOT, UINT32 key_clr)
{
	if( SLOT->key )
	{
		SLOT->key &= key_clr;

		if( !SLOT->key )
		{
			/* phase -> Release */
			if (SLOT->state>EG_REL)
				SLOT->state = EG_REL;
		}
	}
}

/* update phase increment counter of operator (also update the EG rates if necessary) */
MAME_INLINE void CALC_FCSLOT(YM2413_OPLL_CH *CH,YM2413_OPLL_SLOT *SLOT)
{
	int ksr;
	UINT32 SLOT_rs;
	UINT32 SLOT_dp;

	/* (frequency) phase increment counter */
	SLOT->freq = CH->fc * SLOT->mul;
	ksr = CH->kcode >> SLOT->KSR;

	if( SLOT->ksr != ksr )
	{
		SLOT->ksr = ksr;

		/* calculate envelope generator rates */
		if ((SLOT->ar + SLOT->ksr) < 16+62)
		{
			SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar + SLOT->ksr ];
			SLOT->eg_sel_ar = eg_rate_select[SLOT->ar + SLOT->ksr ];
		}
		else
		{
			SLOT->eg_sh_ar  = 0;
			SLOT->eg_sel_ar = 13*RATE_STEPS;
		}
		SLOT->eg_sh_dr  = eg_rate_shift [SLOT->dr + SLOT->ksr ];
		SLOT->eg_sel_dr = eg_rate_select[SLOT->dr + SLOT->ksr ];
		SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr + SLOT->ksr ];
		SLOT->eg_sel_rr = eg_rate_select[SLOT->rr + SLOT->ksr ];

	}

	if (CH->sus)
		SLOT_rs  = 16 + (5<<2);
	else
		SLOT_rs  = 16 + (7<<2);

	SLOT->eg_sh_rs  = eg_rate_shift [SLOT_rs + SLOT->ksr ];
	SLOT->eg_sel_rs = eg_rate_select[SLOT_rs + SLOT->ksr ];

	SLOT_dp  = 16 + (13<<2);
	SLOT->eg_sh_dp  = eg_rate_shift [SLOT_dp + SLOT->ksr ];
	SLOT->eg_sel_dp = eg_rate_select[SLOT_dp + SLOT->ksr ];
}

/* set multi,am,vib,EG-TYP,KSR,mul */
MAME_INLINE void set_mul(YM2413 *chip,int slot,int v)
{
    YM2413_OPLL_CH   *CH   = &chip->P_CH[slot/2];
    YM2413_OPLL_SLOT *SLOT = &CH->SLOT[slot&1];

	SLOT->mul     = mul_tab[v&0x0f];
	SLOT->KSR     = (v&0x10) ? 0 : 2;
	SLOT->eg_type = (v&0x20);
	SLOT->vib     = (v&0x40);
	SLOT->AMmask  = (v&0x80) ? ~0 : 0;
	CALC_FCSLOT(CH,SLOT);
}

/* set ksl, tl */
MAME_INLINE void set_ksl_tl(YM2413 *chip,int chan,int v)
{
	int ksl;
    YM2413_OPLL_CH   *CH   = &chip->P_CH[chan];
/* modulator */
    YM2413_OPLL_SLOT *SLOT = &CH->SLOT[SLOT1];

	ksl = v>>6; /* 0 / 1.5 / 3.0 / 6.0 dB/OCT */

	SLOT->ksl = ksl ? 3-ksl : 31;
	SLOT->TL  = (v&0x3f)<<(ENV_BITS-2-7); /* 7 bits TL (bit 6 = always 0) */
	SLOT->TLL = SLOT->TL + (CH->ksl_base>>SLOT->ksl);
}

/* set ksl , waveforms, feedback */
MAME_INLINE void set_ksl_wave_fb(YM2413 *chip,int chan,int v)
{
	int ksl;
    YM2413_OPLL_CH   *CH   = &chip->P_CH[chan];
/* modulator */
    YM2413_OPLL_SLOT *SLOT = &CH->SLOT[SLOT1];
	SLOT->wavetable = ((v&0x08)>>3)*SIN_LEN;
	SLOT->fb_shift  = (v&7) ? (v&7) + 8 : 0;

/*carrier*/
	SLOT = &CH->SLOT[SLOT2];
	ksl = v>>6; /* 0 / 1.5 / 3.0 / 6.0 dB/OCT */

	SLOT->ksl = ksl ? 3-ksl : 31;
	SLOT->TLL = SLOT->TL + (CH->ksl_base>>SLOT->ksl);

	SLOT->wavetable = ((v&0x10)>>4)*SIN_LEN;
}

/* set attack rate & decay rate  */
MAME_INLINE void set_ar_dr(YM2413 *chip,int slot,int v)
{
    YM2413_OPLL_CH   *CH   = &chip->P_CH[slot/2];
    YM2413_OPLL_SLOT *SLOT = &CH->SLOT[slot&1];

	SLOT->ar = (v>>4)  ? 16 + ((v>>4)  <<2) : 0;

	if ((SLOT->ar + SLOT->ksr) < 16+62)
	{
		SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar + SLOT->ksr ];
		SLOT->eg_sel_ar = eg_rate_select[SLOT->ar + SLOT->ksr ];
	}
	else
	{
		SLOT->eg_sh_ar  = 0;
		SLOT->eg_sel_ar = 13*RATE_STEPS;
	}

	SLOT->dr    = (v&0x0f)? 16 + ((v&0x0f)<<2) : 0;
	SLOT->eg_sh_dr  = eg_rate_shift [SLOT->dr + SLOT->ksr ];
	SLOT->eg_sel_dr = eg_rate_select[SLOT->dr + SLOT->ksr ];
}

/* set sustain level & release rate */
MAME_INLINE void set_sl_rr(YM2413 *chip,int slot,int v)
{
    YM2413_OPLL_CH   *CH   = &chip->P_CH[slot/2];
    YM2413_OPLL_SLOT *SLOT = &CH->SLOT[slot&1];

	SLOT->sl  = sl_tab[ v>>4 ];

	SLOT->rr  = (v&0x0f)? 16 + ((v&0x0f)<<2) : 0;
	SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr + SLOT->ksr ];
	SLOT->eg_sel_rr = eg_rate_select[SLOT->rr + SLOT->ksr ];
}

static void load_instrument(YM2413 *chip, UINT32 chan, UINT32 slot, UINT8* inst )
{
	set_mul			(chip, slot,   inst[0]);
	set_mul			(chip, slot+1, inst[1]);
	set_ksl_tl		(chip, chan,   inst[2]);
	set_ksl_wave_fb	(chip, chan,   inst[3]);
	set_ar_dr		(chip, slot,   inst[4]);
	set_ar_dr		(chip, slot+1, inst[5]);
	set_sl_rr		(chip, slot,   inst[6]);
	set_sl_rr		(chip, slot+1, inst[7]);
}
static void update_instrument_zero(YM2413 *chip, UINT8 r )
{
	UINT8* inst = &chip->inst_tab[0][0]; /* point to user instrument */
	UINT32 chan;
	UINT32 chan_max;

	chan_max = 9;
	if (chip->rhythm & 0x20)
		chan_max=6;

	switch(r)
	{
	case 0:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_mul			(chip, chan*2, inst[0]);
			}
		}
        break;
	case 1:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_mul			(chip, chan*2+1,inst[1]);
			}
		}
        break;
	case 2:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_ksl_tl		(chip, chan,   inst[2]);
			}
		}
        break;
	case 3:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_ksl_wave_fb	(chip, chan,   inst[3]);
			}
		}
        break;
	case 4:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_ar_dr		(chip, chan*2, inst[4]);
			}
		}
        break;
	case 5:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_ar_dr		(chip, chan*2+1,inst[5]);
			}
		}
        break;
	case 6:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_sl_rr		(chip, chan*2, inst[6]);
			}
		}
        break;
	case 7:
		for (chan=0; chan<chan_max; chan++)
		{
			if ((chip->instvol_r[chan]&0xf0)==0)
			{
				set_sl_rr		(chip, chan*2+1,inst[7]);
			}
		}
        break;
    }
}

/* write a value v to register r on chip chip */
static void OPLLWriteReg(YM2413 *chip, int r, int v)
{
    YM2413_OPLL_CH *CH;
    YM2413_OPLL_SLOT *SLOT;
	UINT8 *inst;
	int chan;
	int slot;

	/* adjust bus to 8 bits */
	r &= 0xff;
	v &= 0xff;


#ifdef LOG_CYM_FILE
	if ((cymfile) && (r!=8) )
	{
		fputc( (unsigned char)r, cymfile );
		fputc( (unsigned char)v, cymfile );
	}
#endif


	switch(r&0xf0)
	{
	case 0x00:	/* 00-0f:control */
	{
		switch(r&0x0f)
		{
		case 0x00:	/* AM/VIB/EGTYP/KSR/MULTI (modulator) */
		case 0x01:	/* AM/VIB/EGTYP/KSR/MULTI (carrier) */
		case 0x02:	/* Key Scale Level, Total Level (modulator) */
		case 0x03:	/* Key Scale Level, carrier waveform, modulator waveform, Feedback */
		case 0x04:	/* Attack, Decay (modulator) */
		case 0x05:	/* Attack, Decay (carrier) */
		case 0x06:	/* Sustain, Release (modulator) */
		case 0x07:	/* Sustain, Release (carrier) */
			chip->inst_tab[0][r & 0x07] = v;
			update_instrument_zero(chip,r&7);
		break;

		case 0x0e:	/* x, x, r,bd,sd,tom,tc,hh */
		{
			if(v&0x20)
			{
				if ((chip->rhythm&0x20)==0)
				/*rhythm off to on*/
				{
					logerror("YM2413: Rhythm mode enable\n");

	/* Load instrument settings for channel seven(chan=6 since we're zero based). (Bass drum) */
					chan = 6;
					inst = &chip->inst_tab[16][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);

	/* Load instrument settings for channel eight. (High hat and snare drum) */
					chan = 7;
					inst = &chip->inst_tab[17][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);

					CH   = &chip->P_CH[chan];
					SLOT = &CH->SLOT[SLOT1]; /* modulator envelope is HH */
					SLOT->TL  = ((chip->instvol_r[chan]>>4)<<2)<<(ENV_BITS-2-7); /* 7 bits TL (bit 6 = always 0) */
					SLOT->TLL = SLOT->TL + (CH->ksl_base>>SLOT->ksl);

	/* Load instrument settings for channel nine. (Tom-tom and top cymbal) */
					chan = 8;
					inst = &chip->inst_tab[18][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);

					CH   = &chip->P_CH[chan];
					SLOT = &CH->SLOT[SLOT1]; /* modulator envelope is TOM */
					SLOT->TL  = ((chip->instvol_r[chan]>>4)<<2)<<(ENV_BITS-2-7); /* 7 bits TL (bit 6 = always 0) */
					SLOT->TLL = SLOT->TL + (CH->ksl_base>>SLOT->ksl);
				}
				/* BD key on/off */
				if(v&0x10)
				{
					KEY_ON (&chip->P_CH[6].SLOT[SLOT1], 2);
					KEY_ON (&chip->P_CH[6].SLOT[SLOT2], 2);
				}
				else
				{
					KEY_OFF(&chip->P_CH[6].SLOT[SLOT1],~2);
					KEY_OFF(&chip->P_CH[6].SLOT[SLOT2],~2);
				}
				/* HH key on/off */
				if(v&0x01) KEY_ON (&chip->P_CH[7].SLOT[SLOT1], 2);
				else       KEY_OFF(&chip->P_CH[7].SLOT[SLOT1],~2);
				/* SD key on/off */
				if(v&0x08) KEY_ON (&chip->P_CH[7].SLOT[SLOT2], 2);
				else       KEY_OFF(&chip->P_CH[7].SLOT[SLOT2],~2);
				/* TOM key on/off */
				if(v&0x04) KEY_ON (&chip->P_CH[8].SLOT[SLOT1], 2);
				else       KEY_OFF(&chip->P_CH[8].SLOT[SLOT1],~2);
				/* TOP-CY key on/off */
				if(v&0x02) KEY_ON (&chip->P_CH[8].SLOT[SLOT2], 2);
				else       KEY_OFF(&chip->P_CH[8].SLOT[SLOT2],~2);
			}
			else
			{
				if (chip->rhythm&0x20)
				/*rhythm on to off*/
				{
					logerror("YM2413: Rhythm mode disable\n");
	/* Load instrument settings for channel seven(chan=6 since we're zero based).*/
					chan = 6;
					inst = &chip->inst_tab[chip->instvol_r[chan]>>4][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);

	/* Load instrument settings for channel eight.*/
					chan = 7;
					inst = &chip->inst_tab[chip->instvol_r[chan]>>4][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);

	/* Load instrument settings for channel nine.*/
					chan = 8;
					inst = &chip->inst_tab[chip->instvol_r[chan]>>4][0];
					slot = chan*2;

					load_instrument(chip, chan, slot, inst);
				}
				/* BD key off */
				KEY_OFF(&chip->P_CH[6].SLOT[SLOT1],~2);
				KEY_OFF(&chip->P_CH[6].SLOT[SLOT2],~2);
				/* HH key off */
				KEY_OFF(&chip->P_CH[7].SLOT[SLOT1],~2);
				/* SD key off */
				KEY_OFF(&chip->P_CH[7].SLOT[SLOT2],~2);
				/* TOM key off */
				KEY_OFF(&chip->P_CH[8].SLOT[SLOT1],~2);
				/* TOP-CY off */
				KEY_OFF(&chip->P_CH[8].SLOT[SLOT2],~2);
			}
			chip->rhythm  = v&0x3f;
		}
		break;
		}
	}
	break;

	case 0x10:
	case 0x20:
	{
		int block_fnum;

		chan = r&0x0f;

		if (chan >= 9)
			chan -= 9;	/* verified on real YM2413 */

		CH = &chip->P_CH[chan];

		if(r&0x10)
		{	/* 10-18: FNUM 0-7 */
			block_fnum  = (CH->block_fnum&0x0f00) | v;
		}
		else
		{	/* 20-28: suson, keyon, block, FNUM 8 */
			block_fnum = ((v&0x0f)<<8) | (CH->block_fnum&0xff);

			if(v&0x10)
			{
				KEY_ON (&CH->SLOT[SLOT1], 1);
				KEY_ON (&CH->SLOT[SLOT2], 1);
			}
			else
			{
				KEY_OFF(&CH->SLOT[SLOT1],~1);
				KEY_OFF(&CH->SLOT[SLOT2],~1);
			}


			if (CH->sus!=(v&0x20))
				logerror("chan=%i sus=%2x\n",chan,v&0x20);

			CH->sus = v & 0x20;
		}
		/* update */
		if(CH->block_fnum != block_fnum)
		{
			UINT8 block;

			CH->block_fnum = block_fnum;

			/* BLK 2,1,0 bits -> bits 3,2,1 of kcode, FNUM MSB -> kcode LSB */
			CH->kcode    = (block_fnum&0x0f00)>>8;

			CH->ksl_base = ksl_tab[block_fnum>>5];

			block_fnum   = block_fnum * 2;
			block        = (block_fnum&0x1c00) >> 10;
			CH->fc       = chip->fn_tab[block_fnum&0x03ff] >> (7-block);

			/* refresh Total Level in both SLOTs of this channel */
			CH->SLOT[SLOT1].TLL = CH->SLOT[SLOT1].TL + (CH->ksl_base>>CH->SLOT[SLOT1].ksl);
			CH->SLOT[SLOT2].TLL = CH->SLOT[SLOT2].TL + (CH->ksl_base>>CH->SLOT[SLOT2].ksl);

			/* refresh frequency counter in both SLOTs of this channel */
			CALC_FCSLOT(CH,&CH->SLOT[SLOT1]);
			CALC_FCSLOT(CH,&CH->SLOT[SLOT2]);
		}
	}
	break;

	case 0x30:	/* inst 4 MSBs, VOL 4 LSBs */
	{
		UINT8 old_instvol;

		chan = r&0x0f;

		if (chan >= 9)
			chan -= 9;	/* verified on real YM2413 */

		old_instvol = chip->instvol_r[chan];
		chip->instvol_r[chan] = v;	/* store for later use */

		CH   = &chip->P_CH[chan];
		SLOT = &CH->SLOT[SLOT2]; /* carrier */
		SLOT->TL  = ((v&0x0f)<<2)<<(ENV_BITS-2-7); /* 7 bits TL (bit 6 = always 0) */
		SLOT->TLL = SLOT->TL + (CH->ksl_base>>SLOT->ksl);


		/*check wether we are in rhythm mode and handle instrument/volume register accordingly*/
		if ((chan>=6) && (chip->rhythm&0x20))
		{
			/* we're in rhythm mode*/

			if (chan>=7) /* only for channel 7 and 8 (channel 6 is handled in usual way)*/
			{
				SLOT = &CH->SLOT[SLOT1]; /* modulator envelope is HH(chan=7) or TOM(chan=8) */
				SLOT->TL  = ((chip->instvol_r[chan]>>4)<<2)<<(ENV_BITS-2-7); /* 7 bits TL (bit 6 = always 0) */
				SLOT->TLL = SLOT->TL + (CH->ksl_base>>SLOT->ksl);
			}
		}
		else
		{
			if ( (old_instvol&0xf0) == (v&0xf0) )
				return;

			inst = &chip->inst_tab[chip->instvol_r[chan]>>4][0];
			slot = chan*2;

			load_instrument(chip, chan, slot, inst);

		#if 0
			logerror("YM2413: chan#%02i inst=%02i:  (r=%2x, v=%2x)\n",chan,v>>4,r,v);
			logerror("  0:%2x  1:%2x\n",inst[0],inst[1]);	logerror("  2:%2x  3:%2x\n",inst[2],inst[3]);
			logerror("  4:%2x  5:%2x\n",inst[4],inst[5]);	logerror("  6:%2x  7:%2x\n",inst[6],inst[7]);
		#endif
		}
	}
	break;

	default:
	break;
	}
}

#ifdef LOG_CYM_FILE
static void cymfile_callback (int n)
{
	if (cymfile)
	{
		fputc( (unsigned char)8, cymfile );
	}
}
#endif

/* lock/unlock for common table */
static int OPLL_LockTable(void)
{
	num_lock++;
	if(num_lock>1) return 0;

	/* first time */

	cur_chip = NULL;
	/* allocate total level table (128kb space) */
	if( !init_tables() )
	{
		num_lock--;
		return -1;
	}

#ifdef LOG_CYM_FILE
	cymfile = fopen("2413_.cym","wb");
	if (cymfile)
		timer_pulse ( TIME_IN_HZ(110), 0, cymfile_callback); /*110 Hz pulse timer*/
	else
		logerror("Could not create file 2413_.cym\n");
#endif

	return 0;
}

static void OPLL_UnLockTable(void)
{
	if(num_lock) num_lock--;
	if(num_lock) return;

	/* last time */

	cur_chip = NULL;
	OPLCloseTable();

#ifdef LOG_CYM_FILE
	fclose (cymfile);
	cymfile = NULL;
#endif

}

static void OPLLResetChip(YM2413 *chip)
{
	int c,s;
	int i;

	chip->eg_timer = 0;
	chip->eg_cnt   = 0;

	chip->noise_rng = 1;	/* noise shift register */


	/* setup instruments table */
	for (i=0; i<19; i++)
	{
		for (c=0; c<8; c++)
		{
			chip->inst_tab[i][c] = table[i][c];
		}
	}


	/* reset with register write */
	OPLLWriteReg(chip,0x0f,0); /*test reg*/
	for(i = 0x3f ; i >= 0x10 ; i-- ) OPLLWriteReg(chip,i,0x00);

	/* reset operator parameters */
	for( c = 0 ; c < 9 ; c++ )
	{
        YM2413_OPLL_CH *CH = &chip->P_CH[c];
		for(s = 0 ; s < 2 ; s++ )
		{
			/* wave table */
			CH->SLOT[s].wavetable = 0;
			CH->SLOT[s].state     = EG_OFF;
			CH->SLOT[s].volume    = MAX_ATT_INDEX;
		}
	}
}

/* Create one of virtual YM2413 */
/* 'clock' is chip clock in Hz  */
/* 'rate'  is sampling rate  */
static YM2413 *OPLLCreate(int clock, int rate)
{
	char *ptr;
	YM2413 *chip;
	int state_size;

	if (OPLL_LockTable() ==-1) return NULL;

	/* calculate chip state size */
	state_size  = sizeof(YM2413);

	/* allocate memory block */
	ptr = malloc(state_size);

	if (ptr==NULL)
		return NULL;

	/* clear */
	memset(ptr,0,state_size);

	chip  = (YM2413 *)ptr;

	chip->clock = clock;
	chip->rate  = rate;

	/* init global tables */
	OPLL_initalize(chip);

	/* reset chip */
	OPLLResetChip(chip);
	return chip;
}

/* Destroy one of virtual YM3812 */
static void OPLLDestroy(YM2413 *chip)
{
	OPLL_UnLockTable();
	if (chip) {
		free(chip);
		chip = NULL;
	}
}

/* Option handlers */

static void OPLLSetUpdateHandler(YM2413 *chip,OPLL_UPDATEHANDLER UpdateHandler,int param)
{
	chip->UpdateHandler = UpdateHandler;
	chip->UpdateParam = param;
}

/* YM3812 I/O interface */
static void OPLLWrite(YM2413 *chip,int a,int v)
{
	if( !(a&1) )
	{	/* address port */
		chip->address = v & 0xff;
	}
	else
	{	/* data port */
		if(chip->UpdateHandler) chip->UpdateHandler(chip->UpdateParam,0);
		OPLLWriteReg(chip,chip->address,v);
	}
}

static unsigned char OPLLRead(YM2413 *chip,int a)
{
	if( !(a&1) )
	{
		/* status port */
		return chip->status;
	}
	return 0xff;
}





#define MAX_OPLL_CHIPS 4


static YM2413 *OPLL_YM2413[MAX_OPLL_CHIPS];	/* array of pointers to the YM2413's */
static int YM2413NumChips = 0;				/* number of chips */

int YM2413Init(int num, int clock, int rate)
{
	int i;

	if (YM2413NumChips)
		return -1;	/* duplicate init. */

	YM2413NumChips = num;

	for (i = 0;i < YM2413NumChips; i++)
	{
		/* emulator create */
		OPLL_YM2413[i] = OPLLCreate(clock, rate);
		if(OPLL_YM2413[i] == NULL)
		{
			/* it's really bad - we run out of memeory */
			YM2413NumChips = 0;
			return -1;
		}
	}

	return 0;
}

void YM2413Shutdown(void)
{
	int i;

	for (i = 0;i < YM2413NumChips; i++)
	{
		/* emulator shutdown */
        if(OPLL_YM2413[i])
            OPLLDestroy(OPLL_YM2413[i]);
		OPLL_YM2413[i] = NULL;
	}
	YM2413NumChips = 0;
}

void YM2413ResetChip(int which)
{
	OPLLResetChip(OPLL_YM2413[which]);
}

void YM2413Write(int which, int a, int v)
{
	OPLLWrite(OPLL_YM2413[which], a, v);
}

void YM2413WriteReg(int which, int r, int v)
{
	OPLLWriteReg(OPLL_YM2413[which], r, v);
}

unsigned char YM2413Read(int which, int a)
{
	return OPLLRead(OPLL_YM2413[which], a) & 0x03 ;
}

void YM2413SetUpdateHandler(int which,OPLL_UPDATEHANDLER UpdateHandler,int param)
{
	OPLLSetUpdateHandler(OPLL_YM2413[which], UpdateHandler, param);
}


/*
** Generate samples for one of the YM2413's
**
** 'which' is the virtual YM2413 number
** '*buffer' is the output buffer pointer
** 'length' is the number of samples that should be generated
*/
void YM2413UpdateOne(int which, INT16 **buffers, int length)
{
	YM2413		*chip  = OPLL_YM2413[which];
	UINT8		rhythm = chip->rhythm&0x20;
	SAMP		*bufMO = buffers[0];
	SAMP		*bufRO = buffers[1];

	int i;

	if( (void *)chip != cur_chip ){
		cur_chip = (void *)chip;
		/* rhythm slots */
		SLOT7_1 = &chip->P_CH[7].SLOT[SLOT1];
		SLOT7_2 = &chip->P_CH[7].SLOT[SLOT2];
		SLOT8_1 = &chip->P_CH[8].SLOT[SLOT1];
		SLOT8_2 = &chip->P_CH[8].SLOT[SLOT2];
	}


	for( i=0; i < length ; i++ )
	{
		int mo,ro;

		output[0] = 0;
		output[1] = 0;

		advance_lfo(chip);

		/* FM part */
		chan_calc(&chip->P_CH[0]);
//SAVE_SEPARATE_CHANNEL(0);
		chan_calc(&chip->P_CH[1]);
		chan_calc(&chip->P_CH[2]);
		chan_calc(&chip->P_CH[3]);
		chan_calc(&chip->P_CH[4]);
		chan_calc(&chip->P_CH[5]);

		if(!rhythm)
		{
			chan_calc(&chip->P_CH[6]);
			chan_calc(&chip->P_CH[7]);
			chan_calc(&chip->P_CH[8]);
		}
		else		/* Rhythm part */
		{
			rhythm_calc(&chip->P_CH[0], (chip->noise_rng>>0)&1 );
		}

		mo = output[0];
		ro = output[1];

		mo >>= FINAL_SH;
		ro >>= FINAL_SH;

		/* limit check */
		mo = limit( mo , MAXOUT, MINOUT );
		ro = limit( ro , MAXOUT, MINOUT );

		#ifdef SAVE_SAMPLE
		if (which==0)
		{
			SAVE_ALL_CHANNELS
		}
		#endif

		/* store to sound buffer */
		bufMO[i] = mo;
		bufRO[i] = ro;

		advance(chip);
	}

}

void YM2413Scan(INT32 which, INT32 nAction)
{
	YM2413 *chip  = OPLL_YM2413[which];
	
	if (nAction & ACB_DRIVER_DATA) {
		INT32 chnum;
		INT32 slotnum;
		
		SCAN_VAR(chip->instvol_r);
		SCAN_VAR(chip->eg_cnt);
		SCAN_VAR(chip->eg_timer);
		SCAN_VAR(chip->eg_timer_add);
		SCAN_VAR(chip->eg_timer_overflow);
		SCAN_VAR(chip->rhythm);
		SCAN_VAR(chip->lfo_am_cnt);
		SCAN_VAR(chip->lfo_am_inc);
		SCAN_VAR(chip->lfo_pm_cnt);
		SCAN_VAR(chip->lfo_pm_inc);
		SCAN_VAR(chip->noise_rng);
		SCAN_VAR(chip->noise_p);
		SCAN_VAR(chip->noise_f);
		SCAN_VAR(chip->inst_tab);
		SCAN_VAR(chip->address);
		SCAN_VAR(chip->status);
		
		for (chnum = 0; chnum < 9; chnum++) {
			YM2413_OPLL_CH *ch = &chip->P_CH[chnum];
		
			SCAN_VAR(ch->block_fnum);
			SCAN_VAR(ch->fc);
			SCAN_VAR(ch->ksl_base);
			SCAN_VAR(ch->kcode);
			SCAN_VAR(ch->sus);
			
			for (slotnum = 0; slotnum < 2; slotnum++) {
				YM2413_OPLL_SLOT *sl = &ch->SLOT[slotnum];
				
				SCAN_VAR(sl->ar);
				SCAN_VAR(sl->dr);
				SCAN_VAR(sl->rr);
				SCAN_VAR(sl->KSR);
				SCAN_VAR(sl->ksl);
				SCAN_VAR(sl->ksr);
				SCAN_VAR(sl->mul);
				SCAN_VAR(sl->phase);
				SCAN_VAR(sl->freq);
				SCAN_VAR(sl->fb_shift);
				SCAN_VAR(sl->op1_out);
				SCAN_VAR(sl->eg_type);
				SCAN_VAR(sl->state);
				SCAN_VAR(sl->TL);
				SCAN_VAR(sl->TLL);
				SCAN_VAR(sl->volume);
				SCAN_VAR(sl->sl);
				SCAN_VAR(sl->eg_sh_dp);
				SCAN_VAR(sl->eg_sel_dp);
				SCAN_VAR(sl->eg_sh_ar);
				SCAN_VAR(sl->eg_sel_ar);
				SCAN_VAR(sl->eg_sh_dr);
				SCAN_VAR(sl->eg_sel_dr);
				SCAN_VAR(sl->eg_sh_rr);
				SCAN_VAR(sl->eg_sel_rr);
				SCAN_VAR(sl->eg_sh_rs);
				SCAN_VAR(sl->eg_sel_rs);
				SCAN_VAR(sl->key);
				SCAN_VAR(sl->AMmask);
				SCAN_VAR(sl->vib);
				SCAN_VAR(sl->wavetable);
			}
		}
	}
}
