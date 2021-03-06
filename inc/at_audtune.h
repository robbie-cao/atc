#ifndef _AT_AUDTUNE_H_
#define _AT_AUDTUNE_H_

#include "audioapi.h"


#define AUDIO_TUNE_PARM_DELIMITERS		" "
#define AUDIO_TUNE_EOL_MARK				"\0"


typedef enum
{
	AUD_CMD_GETMODE=0,
	AUD_CMD_SETMODE,
	AUD_CMD_GETPARM,
	AUD_CMD_SETPARM,
	AUD_CMD_STARTTUNE,
	AUD_CMD_STOPTUNE
} AudioTuneCommand_t;


typedef enum
{
	AUD_DATA_TYPE_INT8=0,
	AUD_DATA_TYPE_INT8_ARRAY,
	AUD_DATA_TYPE_UINT8,
	AUD_DATA_TYPE_UINT8_ARRAY,
	AUD_DATA_TYPE_INT16,
	AUD_DATA_TYPE_INT16_ARRAY,
	AUD_DATA_TYPE_UINT16,
	AUD_DATA_TYPE_UINT16_ARRAY,
	AUD_DATA_TYPE_INT32,
	AUD_DATA_TYPE_INT32_ARRAY,
	AUD_DATA_TYPE_UINT32,
	AUD_DATA_TYPE_UINT32_ARRAY

} AudioParmType_t;


typedef struct
{
	AudioParam_t	AudioParmID;
	AudioParmType_t parmType;
	Int16			parm_cnt;		// -1 = variable num of parameters, have not been used/code yet
	Int32			lower_range;
	Int32			upper_range;
} AudioParmEntry_t;


AudioParmEntry_t AudioParmEntryTable[AUDIO_PARM_NUMBER] =
{
	// ID										type						Cnt									L-Range	H-Range
	// ================================			=========================== =================================== ======= =======
	{ PARAM_AUDIO_AGC_DL_DECAY,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_DL_ENABLE,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_DL_HI_THRESH,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_DL_LOW_THRESH,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_DL_MAX_IDX,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_DL_MAX_STEP_DOWN,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_DL_MAX_THRESH,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_DL_MIN_IDX,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_DL_STEP_DOWN,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_DL_STEP_UP,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_UL_DECAY,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_UL_ENABLE,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_UL_HI_THRESH,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_UL_LOW_THRESH,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_UL_MAX_IDX,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_UL_MAX_STEP_DOWN,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_UL_MAX_THRESH,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_UL_MIN_IDX,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_UL_STEP_DOWN,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_AGC_UL_STEP_UP,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_ADAPT_NORM_FACTOR,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCEL_DTD_HANG,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCEL_DTD_THRESH,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCEL_FRAME_SAMPLES,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCEL_HSEC_LOOP,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCEL_HSEC_MFACT,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCEL_HSEC_STEP,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCEL_INPUT_GAIN,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCEL_MAX_HSEC,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCEL_OUTPUT_GAIN,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCEL_UPDATE_DELAY,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CANCELLING_ENABLE,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_CNG_BIAS,						AUD_DATA_TYPE_INT16,		1,									-32768,	32767 },
	{ PARAM_ECHO_CNG_ENABLE,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_COUPLING_DELAY,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_DIGITAL_INPUT_CLIP_LEVEL,		AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_DUAL_FILTER_MODE,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_EN_FAR_SCALE_FACTOR,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_EN_NEAR_SCALE_FACTOR,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_FAR_IN_FILTER,					AUD_DATA_TYPE_UINT16_ARRAY,	NUM_OF_ECHO_FAR_IN_FILTER_COEFF,	0,		65535 },
	{ PARAM_ECHO_FEED_FORWARD_GAIN,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_CNG_FILTER,				AUD_DATA_TYPE_UINT16_ARRAY,	NUM_OF_ECHO_NLP_CNG_FILTER,			0,		65535 },
	{ PARAM_ECHO_NLP_DL_ENERGY_WINDOW,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_DL_GAIN_TABLE,				AUD_DATA_TYPE_UINT16_ARRAY,	NUM_OF_ECHO_NLP_GAIN,				0,		65535 },
	{ PARAM_ECHO_NLP_DL_IDLE_UL_GAIN,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_DOWNLINK_VOLUME_CTRL,		AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_DTALK_DL_GAIN,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_DTALK_HANG_COUNT,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 }, //nned to check
	{ PARAM_ECHO_NLP_DTALK_UL_GAIN,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_ENABLE,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_GAIN,						AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_MAX_SUPP,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_MIN_DL_PWR,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_MIN_UL_PWR,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_RELATIVE_DL_ENERGY_DECAY,	AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_RELATIVE_DL_ENERGY_WINDOW,	AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_RELATIVE_DL_W_THRESH,		AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_RELATIVE_OFFSET_DL_ACTIVE,	AUD_DATA_TYPE_INT16,		1,									-32768,	32767 },
	{ PARAM_ECHO_NLP_RELATIVE_OFFSET_DTALK,		AUD_DATA_TYPE_INT16,		1,									-32768,	32767 },
	{ PARAM_ECHO_NLP_RELATIVE_OFFSET_UL_ACTIVE,	AUD_DATA_TYPE_INT16,		1,									-32768,	32767 }, //need to check
	{ PARAM_ECHO_NLP_TIMEOUT_VAL,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_UL_ACTIVE_DL_GAIN,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_UL_BRK_IN_THRESH,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_UL_ENERGY_WINDOW,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_UL_GAIN_TABLE,				AUD_DATA_TYPE_UINT16_ARRAY,	NUM_OF_ECHO_NLP_GAIN,				0,		65535 },
	{ PARAM_ECHO_NLP_UL_IDLE_DL_GAIN,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_NLP_UL_ACTIVE_HANG_COUNT,		AUD_DATA_TYPE_UINT16,		1,									0,		65535 }, //need to check
	{ PARAM_ECHO_SPKR_PHONE_INPUT_GAIN,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_STABLE_COEF_THRESH,			AUD_DATA_TYPE_UINT16_ARRAY,	NUM_OF_ECHO_STABLE_COEF_THRESH,		0,		65535 }, //need to check
	{ PARAM_MIC_GAIN,							AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_SPEAKER_GAIN,						AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_NOISE_SUPP_INPUT_GAIN,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_NOISE_SUPP_OUTPUT_GAIN,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_NOISE_SUPPRESSION_ENABLE,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_SIDETONE,							AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
//	{ PARAM_DAC_FILTER_SCALE_FACTOR,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },

	{ PARAM_VOICE_ADC,							AUD_DATA_TYPE_UINT16_ARRAY,	NUM_OF_VOICE_COEFF,					0,		65535 },
	{ PARAM_VOICE_DAC,							AUD_DATA_TYPE_UINT16_ARRAY,	NUM_OF_VOICE_COEFF,					0,		65535 },
	{ PARAM_AUDIO_CHANNEL,						AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_DEVICE_TYPE,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_NOISE_SUPP_MIN,						AUD_DATA_TYPE_INT16,		1,									-32768,	32767 },
	{ PARAM_NOISE_SUPP_MAX,						AUD_DATA_TYPE_INT16,		1,									-32768,	32767 },
	{ PARAM_VOLUME_STEP_SIZE,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_NUM_SUPPORTED_VOLUME_LEVELS,		AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
   	{ PARAM_DAC_FILTER_SCALE_FACTOR,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },

	{ PARAM_ECHO_DUAL_EC_ECLEN,			        AUD_DATA_TYPE_UINT16,		1,									1,		65535 },
	{ PARAM_ECHO_DUAL_EC_RinLpcBuffSize,		AUD_DATA_TYPE_UINT16,		1,									1,		65535 },
	{ PARAM_ECHO_DUAL_E__RinCirBuffSizeModij,	AUD_DATA_TYPE_UINT16,		1,									1,		65535 },
	{ PARAM_ECHO_DUAL_EC_DT_TH_ERL_dB,			AUD_DATA_TYPE_UINT16,		1,									-32768,	32767 },
	{ PARAM_ECHO_DUAL_EC_echo_step_size_gain,	AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_DUAL_EC_HANGOVER_CNT,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_ECHO_DUAL_EC_VAD_TH_dB,				AUD_DATA_TYPE_UINT16,		1,									1,		65535 },
	{ PARAM_ECHO_DUAL_EC_DT_HANGOVER_TIME,		AUD_DATA_TYPE_UINT16,		1,									1,		65535 },

	{ PARAM_COMPANDER_FLAG,						AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_EXPANDER_ALPHA,						AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_EXPANDER_BETA,						AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_EXPANDER_B,							AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_EXPANDER_C,							AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_COMPRESSOR_ALPHA,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_COMPRESSOR_BETA,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_COMPRESSOR_OUTPUT_GAIN,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_COMPRESSOR_GAIN,					AUD_DATA_TYPE_UINT16,		1,									6,  	12, },

	{ PARAM_AUDIO_DSP_SIDETONE,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_DL_IDLE_PGA_ADJ,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_NS_UL_IDLE_ADJ,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },

	{ PARAM_SIDETONE_OUTPUT_GAIN,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_SIDETONE_BIQUAD_SCALE_FACTOR,		AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_SIDETONE_BIQUAD_SYS_GAIN,		    AUD_DATA_TYPE_UINT16,		1,									0,		65535 },

	{ PARAM_AUDIO_EC_DE_EMP_FILT_COEF,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_EC_PRE_EMP_FILT_COEF,			AUD_DATA_TYPE_UINT16,		1,									0,		65535 },

	{ PARAM_AUDIO_HPF_ENABLE,					AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_UL_HPF_COEF_B,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_UL_HPF_COEF_A,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_DL_HPF_COEF_B,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 },
	{ PARAM_AUDIO_DL_HPF_COEF_A,				AUD_DATA_TYPE_UINT16,		1,									0,		65535 }

};


#endif //_AT_AUDTUNE_H_

