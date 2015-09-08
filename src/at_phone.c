//=============================================================================
// File:			at_phone.c
//
// Description:		AT interface for mobile phone related access.
//
// Note:
//
//==============================================================================
#include <stdlib.h>	// atoi()
#include "types.h"
#include "mti_build.h"
#include "taskmsgs.h"
#include "string.h"
#include "log.h"
#include "sysparm.h"
#include "at_api.h"
#include "at_data.h"
#include "at_cfg.h"
#include "resultcode.h"
#include "msconfig.h"
#include "mti_trace.h"
#include "ms_database.h"
#include "rtc.h"
#include "audioapi.h"
#include "util.h"

//-------------------------------------------------
// Import Functions and Variables
//-------------------------------------------------
extern Result_t	MS_SetMEPowerClass(UInt8 band, UInt8 pwrClass);

//-------------------------------------------------
// Local Definitions
//-------------------------------------------------



/*=========== Forward AT Command Handlers ===================*/
//----------------------------------------------------------------------
// ATCmd_CGSN_Handler():	+CGSN AT comand handler.
//-----------------------------------------------------------------------
AT_Status_t ATCmd_CGSN_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	UInt8 imei_str[20];

	if (accessType == AT_ACCESS_REGULAR)
	{
		UTIL_ExtractImei(imei_str);
		AT_OutputStr(chan, imei_str);
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}



//----------------------------------------------------------------------
// ATCmd_CSQ_Handler():	+CSQ AT comand handler.
//-----------------------------------------------------------------------
AT_Status_t ATCmd_CSQ_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	UInt8		rssi, ber;
	char		tempBuf[30];

	switch(accessType){
		case AT_ACCESS_REGULAR:
			SYS_GetRxSignalInfo(&rssi, &ber);
			ConvertRxSignalInfo2Csq(&rssi, &ber);

			sprintf(tempBuf, "%s: %d,%d", AT_GetCmdName(cmdId), rssi, ber);
			AT_OutputStr(chan, (const UInt8 *)tempBuf);
			AT_CmdRspOK(chan);

			break;

		default:
			AT_CmdRspSyntaxError(chan);
			break;
	}

	return AT_STATUS_DONE;
}

//----------------------------------------------------------------------
// ATCmd_MCSQ_Handler():	*MCSQ AT comand handler.
//-----------------------------------------------------------------------
AT_Status_t ATCmd_MCSQ_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{

	AT_CommonCfg_t*  channelCfg = AT_GetCommonCfg();
	return AT_SimpleCmdHandler(cmdId, chan, accessType,_P1,
								   FALSE,0,&channelCfg->MCSQ);
}


//----------------------------------------------------------------------
// ATCmd_GCAP_Handler():	+GCAP AT comand handler.
//-----------------------------------------------------------------------
AT_Status_t ATCmd_GCAP_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	char	tempBuf[40];

	switch(accessType){
		case AT_ACCESS_REGULAR:
			sprintf(tempBuf, "%s: +CGSM, +FCLASS, +DS, +ES", AT_GetCmdName(cmdId));
			AT_OutputStr(chan, (const UInt8 *)tempBuf);
			AT_CmdRspOK(chan);
			break;

		case AT_ACCESS_TEST:
			AT_CmdRspOK(chan);
			break;

		case AT_ACCESS_READ:
		default:
			AT_CmdRspSyntaxError(chan);
			break;
	}

	return AT_STATUS_DONE ;
}


//----------------------------------------------------------------------
// ATCmd_CPWC_Handler():	+CPWC AT comand handler.
//-----------------------------------------------------------------------
AT_Status_t ATCmd_CPWC_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
							   const UInt8* _P1, const UInt8* _P2)
{
	UInt8		band;
	UInt8		pwrClass;
	Result_t	ret;
	char		tBuf[80];
	char		tempBuf[30];

//	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg(chan) ;
	MsConfig_t* msCfg = MS_GetCfg( ) ;

 	switch(accessType){		// AT+CPWC=x,y
		case AT_ACCESS_REGULAR:
			pwrClass = atoi((char*)_P1);
			band = atoi((char*)_P2);

			ret = MS_SetMEPowerClass(band, pwrClass);
			if(ret == RESULT_OK){
				msCfg->CPWC[band] = pwrClass;	//**FixMe** (should not be part of ATC)
				AT_CmdRspOK(chan);
			}
			else
			{
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED );
			}
			break;

		case AT_ACCESS_READ:	// AT+CPWC?
			sprintf(tBuf, "%s: ", AT_GetCmdName(cmdId));

			if(SYSPARM_GetClassmark()->pgsm_supported )
			{
				sprintf(tempBuf, "%d, %d, 0,  ", msCfg->CPWC[0],
						SYSPARM_GetClassmark()->pgsm_pwr_class+1);
				strcat(tBuf, tempBuf);
			}

			if(SYSPARM_GetClassmark()->egsm_supported )
			{
				sprintf(tempBuf, "%d, %d, 0,  ", msCfg->CPWC[0],
						SYSPARM_GetClassmark()->egsm_pwr_class+1);
				strcat(tBuf, tempBuf);
			}

			if ( SYSPARM_GetClassmark()->dcs_supported )
			{
				sprintf(tempBuf, "%d, %d, 1,  ",  msCfg->CPWC[1],
						SYSPARM_GetClassmark()->dcs_pwr_class+1);
				strcat(tBuf, tempBuf);
			}

			if ( SYSPARM_GetClassmark()->pcs_supported )
			{
				sprintf(tempBuf, "%d, %d, 2", msCfg->CPWC[2],
						SYSPARM_GetClassmark()->pcs_pwr_class+1);
				strcat(tBuf, tempBuf);
			}
			AT_OutputStr(chan, (const UInt8 *)tBuf);
			AT_CmdRspOK(chan);
			break;

		default:
			AT_CmdRspSyntaxError(chan);
			break;

	} // end of switch(accessType)-loop

	return AT_STATUS_DONE ;
}


//******************************************************************************
// Function Name:	ATCmd_CSCS_Handler (ATC_Cscs_CB)
//
// Description:		Transition on AT+CSCS="alphabet"
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CSCS_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8 rspBuf[ AT_RSPBUF_SM ] ;
	MsConfig_t* msCfg = MS_GetCfg( ) ;

	switch(accessType)
	{
		case AT_ACCESS_REGULAR:
			if ( _P1 == NULL )
			{
				_P1 = (const UInt8 *) "IRA";
			}

			if ( strcmp((char*)_P1, "IRA") && strcmp((char*)_P1, "PCCP437") &&
					strcmp((char*)_P1, "GSM") && strcmp((char*)_P1, "UCS2") &&
					strcmp((char*)_P1, "8859-1") && strcmp((char*)_P1, "8859-2") &&
					strcmp((char*)_P1, "8859-3") && strcmp((char*)_P1, "8859-4") &&
					strcmp((char*)_P1, "8859-5") && strcmp((char*)_P1, "8859-6") )
			{
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED );
				return AT_STATUS_DONE;
			}
			else
			{
					strcpy( (char*)msCfg->CSCS, (char*)_P1 ) ;
			}
			break;

		case AT_ACCESS_READ:
			sprintf( (char*)rspBuf, "+CSCS: \"%s\"", msCfg->CSCS );
			AT_OutputStr (chan, rspBuf );
			break;

		case AT_ACCESS_TEST:
			sprintf((char*)rspBuf, "+CSCS: (\"IRA\",\"GSM\",\"UCS2\",\"PCCP437\",\"8859-1\",\"8859-2\",\"8859-3\",\"8859-4\",\"8859-5\",\"8859-6\")");
			AT_OutputStr (chan, rspBuf );
			break;

		default:
			AT_CmdRspError(chan, AT_INVALID_STR);
			return AT_STATUS_DONE;
	}

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATC_IsCharacterSetUcs2
//
// Description:		Return TRUE if the current character set is UCS2; Return
//					FALSE otherwise.
//
//******************************************************************************
Boolean ATC_IsCharacterSetUcs2(void)
{
	return ( strcmp((char *) MS_GetCfg()->CSCS, "UCS2") == 0 );
}


//******************************************************************************
//
// Function Name:	ATC_Cmgf_CB
//
// Description:		Transition on AT+CMGF=0/1
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CMGF_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1)
{
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	return AT_SimpleCmdHandler(cmdId, chan, accessType, _P1, FALSE, 0, &channelCfg->CMGF);
}


//******************************************************************************
//
// Function Name:	ATCmd_CCLK_Handler
//
// Description:		Transition on AT+CCLK command (read/set real-time clock)
//
//					AT+CCLK=<time>. For example, for "03/05/06,22:10:01" or
//					"03/05/06,22:10:01+02" where timezone "+02" is optional.
//
//					03 - the year of 2003
//					05 - the month of May
//					06 - 6th day of the month
//					22 - 22 O'clock
//					10 - 10 minutes past the hour
//					01 - 1 second past the mimute
//					+02 - Timezone is half an hour past Greenwich Mean Time.
//
//******************************************************************************
AT_Status_t ATCmd_CCLK_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	RTCTime_t rtc_time;
	char temp_buffer[60];
	UInt8 str_len = (_P1 == NULL) ? 0 : strlen((char *) _P1);
	Int8 time_zone = INVALID_TIMEZONE_VALUE;
	Boolean time_zone_ok = TRUE;

	switch(accessType)
	{
	case AT_ACCESS_REGULAR:
		if( ((str_len != 17) && (str_len != 20)) || (_P1[2] != '/') || (_P1[5] != '/') ||
			(_P1[8] != ',') || (_P1[11] != ':') || (_P1[14] != ':') )
		{
			AT_CmdRspError(chan, AT_CME_ERR_INVALID_CHAR_INSTR);
		}
		else
		{
			rtc_time.Year = ((_P1[0] - '0') * 10) + (_P1[1] - '0');

			if(rtc_time.Year >= (RTC_YEARBASE % 100))
			{
				rtc_time.Year += RTC_YEARBASE - (RTC_YEARBASE % 100);
			}
			else
			{
				rtc_time.Year += (RTC_YEARBASE - (RTC_YEARBASE % 100)) + 100;
			}

			rtc_time.Month = ((_P1[3] - '0') * 10) + (_P1[4] - '0');
			rtc_time.Day = ((_P1[6] - '0') * 10) + (_P1[7] - '0');

			rtc_time.Hour = ((_P1[9] - '0') * 10) + (_P1[10] - '0');
			rtc_time.Min = ((_P1[12] - '0') * 10) + (_P1[13] - '0');
			rtc_time.Sec = ((_P1[15] - '0') * 10) + (_P1[16] - '0');

			if(str_len == 20)
			{
				time_zone = atoi((char *) (_P1 + 17));

				/* Timezone is indicated in quarters of an hour relative to Greenwich Mean Time,
				 * the valid range is from -47 to +48.
				 */
				if( (time_zone < MIN_TIMEZONE_VALUE) || (time_zone > MAX_TIMEZONE_VALUE) )
				{
					time_zone_ok = FALSE;
				}
			}

			/* When we update real-time clock below, rtc_time.Week will be
			 * automatically calculated.
			 */
			if( time_zone_ok && RTC_CheckTime(&rtc_time) )
			{
				RTC_SetTimeZone(time_zone);
				RTC_SetTime(&rtc_time);
				AT_CmdRspOK(chan);
			}
			else
			{
				AT_CmdRspError(chan, AT_CME_ERR_INVALID_CHAR_INSTR);
			}
		}

		break;

	case AT_ACCESS_READ:
		RTC_GetTime(&rtc_time);
		time_zone = RTC_GetTimeZone();

		sprintf( temp_buffer, "+CCLK: %2.2d/%2.2d/%2.2d,%2.2d:%2.2d:%2.2d", rtc_time.Year % 100,
			rtc_time.Month, rtc_time.Day, rtc_time.Hour, rtc_time.Min, rtc_time.Sec );

		if( (time_zone >= MIN_TIMEZONE_VALUE) && (time_zone <= MAX_TIMEZONE_VALUE) )
		{
			str_len = strlen(temp_buffer);

			if(time_zone >= 0)
			{
				temp_buffer[str_len] = '+';
				sprintf(temp_buffer + str_len + 1, "%2.2d", time_zone);
			}
			else
			{
				sprintf(temp_buffer + str_len, "%2.2d", time_zone);
			}
		}

		AT_OutputStr(chan, (const UInt8 *) temp_buffer);
		AT_CmdRspOK(chan);

		break;

	default: /* Must be AT_ACCESS_TEST */
		AT_CmdRspOK(chan);
		break;
	}

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	atc_Cmut_CB
//
// Description:		Transition on AT+CMUT: Mute control during a voice call
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CMUT_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8			rspBuf[ AT_RSPBUF_SM ] ;
	static UInt8	MuteFlag = 0;

	if ( accessType == AT_ACCESS_REGULAR)
	{
		MuteFlag = atoi( (char*)_P1 );

		//Only allow mute during a voice call based on GSM 07.07
		if (CC_MuteCall(MuteFlag) == RESULT_ERROR ){

			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;

		}

	}
	else if( accessType == AT_ACCESS_READ )
	{
		if (CC_IsThereVoiceCall()){

			if(	AUDIO_IsAudioEnable(AUDIO_CALL_ID,AUDIO_MICROPHONE))
				sprintf( (char*)rspBuf, "%s: %s", AT_GetCmdName( cmdId ), "0" );
			else
				sprintf( (char*)rspBuf, "%s: %s", AT_GetCmdName( cmdId ), "1" );

			AT_OutputStr (chan, rspBuf );

		} else {

			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;

		}
	}
	else   // accessType == AT_ACCESS_TEST
	{
		sprintf((char*)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, rspBuf );
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//******************************************************************************
//
// Function Name:	ATC_Vgr_CB
//
// Description:		Transition on AT+VGR. Set and get volumn of the speakers
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_VGR_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	Result_t res;

	if ( accessType == AT_ACCESS_REGULAR )
	{
		if(_P1)
		{
			res = AUDIO_SetSpeakerVol(atoi((char*)_P1));

			if(res != RESULT_OK){
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
				return AT_STATUS_DONE;
			}
		}
		else
		{
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}
	}
	else if( accessType == AT_ACCESS_READ)
	{
		sprintf((char*)rspBuf, "%s: %d", AT_GetCmdName( cmdId ), SPEAKER_GetVolume());
		AT_OutputStr (chan, rspBuf );
	}
	else   // accessType == AT_ACCESS_TEST
	{
		sprintf((char*)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, rspBuf );
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//******************************************************************************
//
// Function Name:	ATC_Vgt_CB
//
// Description:		Transition on AT+VGT. Set and get gain of the microphone
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_VGT_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	Result_t res;

	if ( accessType == AT_ACCESS_REGULAR )
	{
		if(_P1)
		{
			res = AUDIO_SetMicrophoneGain(atoi((char*)_P1));

			if(res != RESULT_OK){
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
				return AT_STATUS_DONE;
			}
		}
		else
		{
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}
	}
	else if( accessType == AT_ACCESS_READ)
	{
		sprintf((char*)rspBuf, "%s: %d", AT_GetCmdName( cmdId ), MICROPHONE_GetGain());
		AT_OutputStr (chan, rspBuf );
	}
	else   // accessType == AT_ACCESS_TEST
	{
		sprintf((char*)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, rspBuf );
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//******************************************************************************
// Function Name:	ATCmd_MPCM_Handler
//
// Description:		PCM interface control
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MPCM_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8			rspBuf[ AT_RSPBUF_SM ] ;
	UInt8			on_off;
	MsConfig_t*		msCfg = MS_GetCfg();


	if ( accessType == AT_ACCESS_REGULAR )
	{
		on_off = (_P1 == NULL) ? 0 : atoi( (char*)_P1 );

		AUDIO_SetPCMOnOff((Boolean)on_off);

		msCfg->pcmMode = on_off;

	}
	else if( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)rspBuf, "%s: %d", AT_GetCmdName(cmdId), msCfg->pcmMode);
		AT_OutputStr (chan, rspBuf );
	}
	else	// accessType == AT_ACCESS_TEST
	{
		sprintf((char*)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, rspBuf );
	}

	AT_ImmedCmdRsp( chan, AT_CMD_MPCM, AT_OK_RSP ) ;

	return AT_STATUS_DONE;
}





static AudioChnl_t	 audio_chnl = AUDIO_CHNL_INTERNAL;
//******************************************************************************
//
// Function Name:	ATC_Mvchn_CB
//
// Description:		Transition on AT*MVCHN. Speaker/Microphone selection
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MVCHN_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	if ( accessType == AT_ACCESS_REGULAR )
	{
		if(_P1 != NULL)
		{
			audio_chnl = (AudioChnl_t)(atoi((char*)_P1));

			if(AUDIO_ASIC_SetAudioChannel(audio_chnl) != RESULT_OK){
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
				return AT_STATUS_DONE;
			}
		}
		else
		{
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}
	}
	else if( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)rspBuf, "%s: %d", AT_GetCmdName(cmdId), audio_chnl);
		AT_OutputStr (chan, rspBuf );
	}
	else  // accessType == AT_ACCESS_TEST
	{
		sprintf((char*)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, rspBuf );
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}




/* ============= End of File: at_phone.c ==================== */
