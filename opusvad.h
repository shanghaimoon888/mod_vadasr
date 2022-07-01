//#include <emmintrin.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <intrin.h>

int silk_VAD_Get(
	// int          state,                       /*  Encoder state                               */
	const short pIn[] /* I    PCM input                                   */
);

#define TYPE_NO_VOICE_ACTIVITY 0
#define TYPE_UNVOICED 1
#define TYPE_VOICED 2

#define SPEECH_ACTIVITY_DTX_THRES 0.05f
#define SILK_FIX_CONST(C, Q) ((int)((C) * ((long)1 << (Q)) + 0.5))
#define silk_int16_MAX 0x7FFF			 /*  2^15 - 1 =  32767 */
#define silk_int16_MIN ((short)0x8000)	 /* -2^15     = -32768 */
#define silk_int32_MAX 0x7FFFFFFF		 /*  2^31 - 1 =  2147483647 */
#define silk_int32_MIN ((int)0x80000000) /* -2^31     = -2147483648 */
#define silk_memset(dest, src, size) memset((dest), (src), (size))

#define VAD_NOISE_LEVEL_SMOOTH_COEF_Q16 1024 /* Must be <  4096 */
#define VAD_NOISE_LEVELS_BIAS 50

/* Sigmoid settings */
#define VAD_NEGATIVE_OFFSET_Q5 128 /* sigmoid is 0 at -128 */
#define VAD_SNR_FACTOR_Q16 45000

/* smoothing for SNR measurement */
#define VAD_SNR_SMOOTH_COEF_Q18 4096

#define VAD_N_BANDS 4
#define VAD_INTERNAL_SUBFRAMES_LOG2 2
#define VAD_INTERNAL_SUBFRAMES (1 << VAD_INTERNAL_SUBFRAMES_LOG2)
#define silk_uint8_MAX 0xFF /*  2^8 - 1 = 255 */

#define VARDECL(type, var) type *var
#define silk_RSHIFT32(a, shift) ((a) >> (shift))
#define silk_RSHIFT(a, shift) ((a) >> (shift))
#define silk_LSHIFT32(a, shift) ((a) << (shift))
#define silk_LSHIFT(a, shift) ((a) << (shift))
#define ALLOC(var, size, type) var = ((type *)alloca(sizeof(type) * (size)))
#define silk_ADD16(a, b) ((a) + (b))
#define silk_ADD32(a, b) ((a) + (b))
#define silk_ADD64(a, b) ((a) + (b))

#define silk_SUB16(a, b) ((a) - (b))
#define silk_SUB32(a, b) ((a) - (b))
#define silk_SUB64(a, b) ((a) - (b))
#define silk_SMULWB(a32, b32)                                                                                          \
	((((a32) >> 16) * (int)((short)(b32))) + ((((a32)&0x0000FFFF) * (int)((short)(b32))) >> 16))
#define silk_SMLAWB(a32, b32, c32)                                                                                     \
	((a32) + ((((b32) >> 16) * (int)((short)(c32))) + ((((b32)&0x0000FFFF) * (int)((short)(c32))) >> 16)))
#define silk_SAT16(a) ((a) > silk_int16_MAX ? silk_int16_MAX : ((a) < silk_int16_MIN ? silk_int16_MIN : (a)))
#define silk_MLA(a32, b32, c32) silk_ADD32((a32), ((b32) * (c32)))
#define silk_SMLABB(a32, b32, c32) ((a32) + ((int)((short)(b32))) * (int)((short)(c32)))
#define silk_ADD_POS_SAT32(a, b) ((((unsigned int)(a) + (unsigned int)(b)) & 0x80000000) ? silk_int32_MAX : ((a) + (b)))
#define silk_ADD_POS_SAT32(a, b) ((((unsigned int)(a) + (unsigned int)(b)) & 0x80000000) ? silk_int32_MAX : ((a) + (b)))
#define silk_DIV32_16(a32, b16) ((int)((a32) / (b16)))
#define silk_DIV32(a32, b32) ((int)((a32) / (b32)))
#define silk_RSHIFT_ROUND(a, shift) ((shift) == 1 ? ((a) >> 1) + ((a)&1) : (((a) >> ((shift)-1)) + 1) >> 1)

#define silk_SMULWW(a32, b32) silk_MLA(silk_SMULWB((a32), (b32)), (a32), silk_RSHIFT_ROUND((b32), 16))
#define silk_min(a, b) (((a) < (b)) ? (a) : (b))
#define silk_max(a, b) (((a) > (b)) ? (a) : (b))
#define silk_ADD_LSHIFT32(a, b, shift) silk_ADD32((a), silk_LSHIFT32((b), (shift))) /* shift >= 0 */
#define silk_MUL(a32, b32) ((a32) * (b32))
#define silk_SMULBB(a32, b32) ((int)((short)(a32)) * (int)((short)(b32)))
#define silk_LIMIT(a, limit1, limit2)                                                                                  \
	((limit1) > (limit2) ? ((a) > (limit1) ? (limit1) : ((a) < (limit2) ? (limit2) : (a)))                             \
						 : ((a) > (limit2) ? (limit2) : ((a) < (limit1) ? (limit1) : (a))))

#define silk_LSHIFT_SAT32(a, shift)                                                                                    \
	(silk_LSHIFT32(silk_LIMIT((a), silk_RSHIFT32(silk_int32_MIN, (shift)), silk_RSHIFT32(silk_int32_MAX, (shift))),    \
				   (shift)))

static const int tiltWeights[VAD_N_BANDS] = {30000, 6000, -12000, -12000};
static const int sigm_LUT_neg_Q15[6] = {16384, 8812, 3906, 1554, 589, 219};
static const int sigm_LUT_slope_Q10[6] = {237, 153, 73, 30, 12, 7};
static const int sigm_LUT_pos_Q15[6] = {16384, 23955, 28861, 31213, 32178, 32548};

static __inline int ec_bsr(unsigned long _x)
{
	//return __builtin_clz(_x);
	unsigned long ret;
	_BitScanReverse(&ret, _x);
	return (int)ret;
}
#define EC_CLZ0 (1)
#define EC_CLZ(_x) (-ec_bsr(_x))
#define EC_ILOG(_x) (EC_CLZ0 - EC_CLZ(_x))
static int silk_min_int(int a, int b) { return (((a) < (b)) ? (a) : (b)); }
static int silk_max_int(int a, int b) { return (((a) > (b)) ? (a) : (b)); }
static int silk_max_32(int a, int b) { return (((a) > (b)) ? (a) : (b)); }
static int silk_CLZ32(int in32) { return in32 ? 32 - EC_ILOG(in32) : 32; }
static int silk_ROR32(int a32, int rot)
{
	unsigned int x = (unsigned int)a32;
	unsigned int r = (unsigned int)rot;
	unsigned int m = (unsigned int)-rot;
	if (rot == 0) {
		return a32;
	} else if (rot < 0) {
		return (int)((x << m) | (x >> (32 - m)));
	} else {
		return (int)((x << (32 - r)) | (x >> r));
	}
}
static void silk_CLZ_FRAC(int in,	   /* I  input                               */
						  int *lz,	   /* O  number of leading zeros             */
						  int *frac_Q7 /* O  the 7 bits right after the leading one */
)
{
	int lzeros = silk_CLZ32(in);

	*lz = lzeros;
	*frac_Q7 = silk_ROR32(in, 24 - lzeros) & 0x7f;
}

/* Approximation of square root                                          */
/* Accuracy: < +/- 10%  for output values > 15                           */
/*           < +/- 2.5% for output values > 120                          */
static int silk_SQRT_APPROX(int x)
{
	int y, lz, frac_Q7;

	if (x <= 0) { return 0; }

	silk_CLZ_FRAC(x, &lz, &frac_Q7);

	if (lz & 1) {
		y = 32768;
	} else {
		y = 46214; /* 46214 = sqrt(2) * 32768 */
	}

	/* get scaling right */
	y >>= silk_RSHIFT(lz, 1);

	/* increment using fractional part of input */
	y = silk_SMLAWB(y, y, silk_SMULBB(213, frac_Q7));

	return y;
}

static short A_fb1_20 = 5394 << 1;
static short A_fb1_21 = -24290; /* (int16)(20623 << 1) */

typedef struct {
	int AnaState[2];				  /* Analysis filterbank state: 0-8 kHz                                   */
	int AnaState1[2];				  /* Analysis filterbank state: 0-4 kHz                                   */
	int AnaState2[2];				  /* Analysis filterbank state: 0-2 kHz                                   */
	int XnrgSubfr[4];				  /* Subframe energies                                                    */
	int NrgRatioSmth_Q8[VAD_N_BANDS]; /* Smoothed energy level in each band                                   */
	short HPstate;					  /* State of differentiator in the lowest band                           */
	int NL[VAD_N_BANDS];			  /* Noise energy level in each band                                      */
	int inv_NL[VAD_N_BANDS];		  /* Inverse noise energy level in each band                              */
	int NoiseLevelBias[VAD_N_BANDS];  /* Noise level estimator bias/offset                                    */
	int counter;					  /* Frame counter used in the initial phase                              */
} VAD_state;

/* Split signal into two decimated bands using first-order allpass filters */
void silk_ana_filt_bank_1(const short *in, /* I    Input signal [N]                                            */
						  int *S,		   /* I/O  State vector [2]                                            */
						  short *outL,	   /* O    Low band [N/2]                                              */
						  short *outH,	   /* O    High band [N/2]                                             */
						  const int N	   /* I    Number of input samples                                     */
)
{
	int k, N2 = silk_RSHIFT(N, 1);
	int in32, X, Y, out_1, out_2;

	/* Internal variables and state are in Q10 format */
	for (k = 0; k < N2; k++) {
		/* Convert to Q10 */
		in32 = silk_LSHIFT((int)in[2 * k], 10);

		/* All-pass section for even input sample */
		Y = silk_SUB32(in32, S[0]);
		X = silk_SMLAWB(Y, Y, A_fb1_21);
		out_1 = silk_ADD32(S[0], X);
		S[0] = silk_ADD32(in32, X);

		/* Convert to Q10 */
		in32 = silk_LSHIFT((int)in[2 * k + 1], 10);

		/* All-pass section for odd input sample, and add to output of previous section */
		Y = silk_SUB32(in32, S[1]);
		X = silk_SMULWB(Y, A_fb1_20);
		out_2 = silk_ADD32(S[1], X);
		S[1] = silk_ADD32(in32, X);

		/* Add/subtract, convert back to int16 and store to output */
		outL[k] = (short)silk_SAT16(silk_RSHIFT_ROUND(silk_ADD32(out_2, out_1), 11));
		outH[k] = (short)silk_SAT16(silk_RSHIFT_ROUND(silk_SUB32(out_2, out_1), 11));
	}
}

void silk_VAD_GetNoiseLevels(const int pX[VAD_N_BANDS], /* I    subband energies                            */
							 VAD_state *psSilk_VAD		/* I/O  Pointer to Silk VAD state                   */
)
{
	int k;
	int nl, nrg, inv_nrg;
	int coef, min_coef;

	/* Initially faster smoothing */
	if (psSilk_VAD->counter < 1000) { /* 1000 = 20 sec */
		min_coef = silk_DIV32_16(silk_int16_MAX, silk_RSHIFT(psSilk_VAD->counter, 4) + 1);
	} else {
		min_coef = 0;
	}

	for (k = 0; k < VAD_N_BANDS; k++) {
		/* Get old noise level estimate for current band */
		nl = psSilk_VAD->NL[k];
		// silk_assert(nl >= 0);

		/* Add bias */
		nrg = silk_ADD_POS_SAT32(pX[k], psSilk_VAD->NoiseLevelBias[k]);
		// silk_assert(nrg > 0);

		/* Invert energies */
		inv_nrg = silk_DIV32(silk_int32_MAX, nrg);
		// silk_assert(inv_nrg >= 0);

		/* Less update when subband energy is high */
		if (nrg > silk_LSHIFT(nl, 3)) {
			coef = VAD_NOISE_LEVEL_SMOOTH_COEF_Q16 >> 3;
		} else if (nrg < nl) {
			coef = VAD_NOISE_LEVEL_SMOOTH_COEF_Q16;
		} else {
			coef = silk_SMULWB(silk_SMULWW(inv_nrg, nl), VAD_NOISE_LEVEL_SMOOTH_COEF_Q16 << 1);
		}

		/* Initially faster smoothing */
		coef = silk_max_int(coef, min_coef);

		/* Smooth inverse energies */
		psSilk_VAD->inv_NL[k] = silk_SMLAWB(psSilk_VAD->inv_NL[k], inv_nrg - psSilk_VAD->inv_NL[k], coef);
		// silk_assert(psSilk_VAD->inv_NL[k] >= 0);

		/* Compute noise level by inverting again */
		nl = silk_DIV32(silk_int32_MAX, psSilk_VAD->inv_NL[k]);
		// silk_assert(nl >= 0);

		/* Limit noise levels (guarantee 7 bits of head room) */
		nl = silk_min(nl, 0x00FFFFFF);

		/* Store as part of state */
		psSilk_VAD->NL[k] = nl;
	}

	/* Increment frame counter */
	psSilk_VAD->counter++;
}

int silk_lin2log(const int inLin /* I  input in linear scale                                         */
)
{
	int lz, frac_Q7;

	silk_CLZ_FRAC(inLin, &lz, &frac_Q7);

	/* Piece-wise parabolic approximation */
	return silk_ADD_LSHIFT32(silk_SMLAWB(frac_Q7, silk_MUL(frac_Q7, 128 - frac_Q7), 179), 31 - lz, 7);
}

int silk_sigm_Q15(int in_Q5 /* I                                                                */
)
{
	int ind;

	if (in_Q5 < 0) {
		/* Negative input */
		in_Q5 = -in_Q5;
		if (in_Q5 >= 6 * 32) {
			return 0; /* Clip */
		} else {
			/* Linear interpolation of look up table */
			ind = silk_RSHIFT(in_Q5, 5);
			return (sigm_LUT_neg_Q15[ind] - silk_SMULBB(sigm_LUT_slope_Q10[ind], in_Q5 & 0x1F));
		}
	} else {
		/* Positive input */
		if (in_Q5 >= 6 * 32) {
			return 32767; /* clip */
		} else {
			/* Linear interpolation of look up table */
			ind = silk_RSHIFT(in_Q5, 5);
			return (sigm_LUT_pos_Q15[ind] + silk_SMULBB(sigm_LUT_slope_Q10[ind], in_Q5 & 0x1F));
		}
	}
}
int silk_VAD_Init(						/* O    Return value, 0 if success                  */
				  VAD_state *psSilk_VAD /* I/O  Pointer to Silk VAD state                   */
)
{
	int b, ret = 0;

	/* reset state memory */
	silk_memset(psSilk_VAD, 0, sizeof(VAD_state));

	/* init noise levels */
	/* Initialize array with approx pink noise levels (psd proportional to inverse of frequency) */
	for (b = 0; b < VAD_N_BANDS; b++) {
		psSilk_VAD->NoiseLevelBias[b] = silk_max_32(silk_DIV32_16(VAD_NOISE_LEVELS_BIAS, b + 1), 1);
	}

	/* Initialize state */
	for (b = 0; b < VAD_N_BANDS; b++) {
		psSilk_VAD->NL[b] = silk_MUL(100, psSilk_VAD->NoiseLevelBias[b]);
		psSilk_VAD->inv_NL[b] = silk_DIV32(silk_int32_MAX, psSilk_VAD->NL[b]);
	}
	psSilk_VAD->counter = 15;

	/* init smoothed energy-to-noise ratio*/
	for (b = 0; b < VAD_N_BANDS; b++) { psSilk_VAD->NrgRatioSmth_Q8[b] = 100 * 256; /* 100 * 256 --> 20 dB SNR */ }

	return (ret);
}

// static int noSpeechCounter;

int silk_VAD_Get(
	// int          state,                       /*  Encoder state                               */
	const short pIn[] /* I    PCM input                                   */
)
{
	int SA_Q15, pSNR_dB_Q7, input_tilt;
	int decimated_framelength1, decimated_framelength2;
	int decimated_framelength;
	int dec_subframe_length, dec_subframe_offset, SNR_Q7, i, b, s;
	int sumSquared, smooth_coef_Q16;
	short HPstateTmp;
	VARDECL(short, X);
	int Xnrg[4];
	int NrgToNoiseRatio_Q8[4];
	int speech_nrg, x_tmp;
	int X_offset[4];
	// int ret = 0;
	int frame_length = 20; //
	int fs_kHz = 8;
	int input_quality_bands_Q15[VAD_N_BANDS];
	int signalType;
	int VAD_flag;
	VAD_state *psSilk_VAD;
	int ret1;
	int input_tilt_Q15;
	int speech_activity_Q8;
	/* Safety checks
	silk_assert(4 == 4);
	silk_assert(MAX_FRAME_LENGTH >= frame_length);
	silk_assert(frame_length <= 512);
	silk_assert(frame_length == 8 * silk_RSHIFT(frame_length, 3));
	*/
	/***********************/
	/* Filter and Decimate */
	/***********************/
	decimated_framelength1 = silk_RSHIFT(frame_length, 1);
	decimated_framelength2 = silk_RSHIFT(frame_length, 2);
	decimated_framelength = silk_RSHIFT(frame_length, 3);
	/* Decimate into 4 bands:
	0       L      3L       L              3L                             5L
	-      --       -              --                             --
	8       8       2               4                              4

	[0-1 kHz| temp. |1-2 kHz|    2-4 kHz    |            4-8 kHz           |

	They're arranged to allow the minimal ( frame_length / 4 ) extra
	scratch space during the downsampling process */
	X_offset[0] = 0;
	X_offset[1] = decimated_framelength + decimated_framelength2;
	X_offset[2] = X_offset[1] + decimated_framelength;
	X_offset[3] = X_offset[2] + decimated_framelength2;
	ALLOC(X, X_offset[3] + decimated_framelength1, short);

	psSilk_VAD = (VAD_state *)malloc(sizeof(VAD_state));
	ret1 = silk_VAD_Init(psSilk_VAD);

	/* 0-8 kHz to 0-4 kHz and 4-8 kHz */
	silk_ana_filt_bank_1(pIn, &psSilk_VAD->AnaState[0], X, &X[X_offset[3]], frame_length);

	/* 0-4 kHz to 0-2 kHz and 2-4 kHz */
	silk_ana_filt_bank_1(X, &psSilk_VAD->AnaState1[0], X, &X[X_offset[2]], decimated_framelength1);

	/* 0-2 kHz to 0-1 kHz and 1-2 kHz */
	silk_ana_filt_bank_1(X, &psSilk_VAD->AnaState2[0], X, &X[X_offset[1]], decimated_framelength2);

	/*********************************************/
	/* HP filter on lowest band (differentiator) */
	/*********************************************/
	X[decimated_framelength - 1] = silk_RSHIFT(X[decimated_framelength - 1], 1);
	HPstateTmp = X[decimated_framelength - 1];
	for (i = decimated_framelength - 1; i > 0; i--) {
		X[i - 1] = silk_RSHIFT(X[i - 1], 1);
		X[i] -= X[i - 1];
	}
	X[0] -= psSilk_VAD->HPstate;
	psSilk_VAD->HPstate = HPstateTmp;

	/*************************************/
	/* Calculate the energy in each band */
	/*************************************/
	for (b = 0; b < 4; b++) {
		/* Find the decimated framelength in the non-uniformly divided bands */
		decimated_framelength = silk_RSHIFT(frame_length, silk_min_int(4 - b, 4 - 1));

		/* Split length into subframe lengths */
		dec_subframe_length = silk_RSHIFT(decimated_framelength, VAD_INTERNAL_SUBFRAMES_LOG2);
		dec_subframe_offset = 0;

		/* Compute energy per sub-frame */
		/* initialize with summed energy of last subframe */
		Xnrg[b] = psSilk_VAD->XnrgSubfr[b];
		for (s = 0; s < VAD_INTERNAL_SUBFRAMES; s++) {
			sumSquared = 0;
			for (i = 0; i < dec_subframe_length; i++) {
				/* The energy will be less than dec_subframe_length * ( silk_short_MIN / 8 ) ^ 2.            */
				/* Therefore we can accumulate with no risk of overflow (unless dec_subframe_length > 128)  */
				x_tmp = silk_RSHIFT(X[X_offset[b] + i + dec_subframe_offset], 3);
				sumSquared = silk_SMLABB(sumSquared, x_tmp, x_tmp);

				/* Safety check */
				// silk_assert(sumSquared >= 0);
			}

			/* Add/saturate summed energy of current subframe */
			if (s < VAD_INTERNAL_SUBFRAMES - 1) {
				Xnrg[b] = silk_ADD_POS_SAT32(Xnrg[b], sumSquared);
			} else {
				/* Look-ahead subframe */
				Xnrg[b] = silk_ADD_POS_SAT32(Xnrg[b], silk_RSHIFT(sumSquared, 1));
			}

			dec_subframe_offset += dec_subframe_length;
		}
		psSilk_VAD->XnrgSubfr[b] = sumSquared;
	}

	/********************/
	/* Noise estimation */
	/********************/
	silk_VAD_GetNoiseLevels(&Xnrg[0], psSilk_VAD);

	/***********************************************/
	/* Signal-plus-noise to noise ratio estimation */
	/***********************************************/
	sumSquared = 0;
	input_tilt = 0;
	for (b = 0; b < 4; b++) {
		speech_nrg = Xnrg[b] - psSilk_VAD->NL[b];
		if (speech_nrg > 0) {
			/* Divide, with sufficient resolution */
			if ((Xnrg[b] & 0xFF800000) == 0) {
				NrgToNoiseRatio_Q8[b] = silk_DIV32(silk_LSHIFT(Xnrg[b], 8), psSilk_VAD->NL[b] + 1);
			} else {
				NrgToNoiseRatio_Q8[b] = silk_DIV32(Xnrg[b], silk_RSHIFT(psSilk_VAD->NL[b], 8) + 1);
			}

			/* Convert to log domain */
			SNR_Q7 = silk_lin2log(NrgToNoiseRatio_Q8[b]) - 8 * 128;

			/* Sum-of-squares */
			sumSquared = silk_SMLABB(sumSquared, SNR_Q7, SNR_Q7); /* Q14 */

			/* Tilt measure */
			if (speech_nrg < ((int)1 << 20)) {
				/* Scale down SNR value for small subband speech energies */
				SNR_Q7 = silk_SMULWB(silk_LSHIFT(silk_SQRT_APPROX(speech_nrg), 6), SNR_Q7);
			}
			input_tilt = silk_SMLAWB(input_tilt, tiltWeights[b], SNR_Q7);
		} else {
			NrgToNoiseRatio_Q8[b] = 256;
		}
	}

	/* Mean-of-squares */
	sumSquared = silk_DIV32_16(sumSquared, 4); /* Q14 */

	/* Root-mean-square approximation, scale to dBs, and write to output pointer */
	pSNR_dB_Q7 = (short)(3 * silk_SQRT_APPROX(sumSquared)); /* Q7 */

	/*********************************/
	/* Speech Probability Estimation */
	/*********************************/
	SA_Q15 = silk_sigm_Q15(silk_SMULWB(VAD_SNR_FACTOR_Q16, pSNR_dB_Q7) - VAD_NEGATIVE_OFFSET_Q5);

	/**************************/
	/* Frequency Tilt Measure */
	/**************************/
	input_tilt_Q15 = silk_LSHIFT(silk_sigm_Q15(input_tilt) - 16384, 1);

	/**************************************************/
	/* Scale the sigmoid output based on power levels */
	/**************************************************/
	speech_nrg = 0;

	for (b = 0; b < 4; b++) {
		/* Accumulate signal-without-noise energies, higher frequency bands have more weight */
		speech_nrg += (b + 1) * silk_RSHIFT(Xnrg[b] - psSilk_VAD->NL[b], 4);
	}

	/* Power scaling */
	if (speech_nrg <= 0) {
		SA_Q15 = silk_RSHIFT(SA_Q15, 1);
	} else if (speech_nrg < 32768) {
		if (frame_length == 10 * fs_kHz) {
			speech_nrg = silk_LSHIFT_SAT32(speech_nrg, 16);
		} else {
			speech_nrg = silk_LSHIFT_SAT32(speech_nrg, 15);
		}

		/* square-root */
		speech_nrg = silk_SQRT_APPROX(speech_nrg);
		SA_Q15 = silk_SMULWB(32768 + speech_nrg, SA_Q15);
	}

	/* Copy the resulting speech activity in Q8 */
	speech_activity_Q8 = silk_min_int(silk_RSHIFT(SA_Q15, 7), silk_uint8_MAX);

	/***********************************/
	/* Energy Level and SNR estimation */
	/***********************************/
	/* Smoothing coefficient */
	smooth_coef_Q16 = silk_SMULWB(VAD_SNR_SMOOTH_COEF_Q18, silk_SMULWB((int)SA_Q15, SA_Q15));

	if (frame_length == 10 * fs_kHz) { smooth_coef_Q16 >>= 1; }

	for (b = 0; b < 4; b++) {
		/* compute smoothed energy-to-noise ratio per band */
		psSilk_VAD->NrgRatioSmth_Q8[b] = silk_SMLAWB(
			psSilk_VAD->NrgRatioSmth_Q8[b], NrgToNoiseRatio_Q8[b] - psSilk_VAD->NrgRatioSmth_Q8[b], smooth_coef_Q16);

		/* signal to noise ratio in dB per band */
		SNR_Q7 = 3 * (silk_lin2log(psSilk_VAD->NrgRatioSmth_Q8[b]) - 8 * 128);
		/* quality = sigmoid( 0.25 * ( SNR_dB - 16 ) ); */
		input_quality_bands_Q15[b] = silk_sigm_Q15(silk_RSHIFT(SNR_Q7 - 16 * 128, 4));
	}
	// gap************************************************************//
	if (speech_activity_Q8 < SILK_FIX_CONST(SPEECH_ACTIVITY_DTX_THRES, 8)) {
		signalType = TYPE_NO_VOICE_ACTIVITY;
		// noSpeechCounter++;
		VAD_flag = 0;
	} else {
		signalType = TYPE_UNVOICED;
		VAD_flag = 1;
	}
	free(psSilk_VAD);
	return (VAD_flag);
}