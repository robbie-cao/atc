//-----------------------------------------------------------------------------
//	AT utility functions.
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdarg.h>
#include "at_data.h"
#include "at_util.h"
#include "v24.h"
#include "phonebk.h"
#include "mstypes.h"
#include "sysparm.h"
#include "mti_build.h"
#include "misc.h"
#include "sdltrace.h"
#include "util.h"
#include "ms_database.h"

UInt8 atc_Trace;

//******************************************************************************
//                          GLOBAL FUNCTION
//******************************************************************************

//******************************************************************************
//
// Function Name:       AT_Trace( )
//
// Description:         Tracing function
//
// Notes:
//
//******************************************************************************

void AT_Trace( char * fmt, ... )
{
   va_list  Args;

atc_Trace=1;
   if ( !atc_Trace ) return;

   va_start( Args, fmt );

#ifdef WIN32
   vfprintf( stdout, fmt, Args );
#elif 1
   SDLTRACE_TraceString((UInt8*)fmt, va_arg( Args, int ));
#endif

   va_end( Args );
}


//******************************************************************************
//
// Function Name:      AT_GetV24ClientIDFromMpxChan
//
// Description:        Get V24 Client ID (to be passed to API functions) from
//					   the MPX channel number.
//
// Notes:
//
//******************************************************************************
UInt8 AT_GetV24ClientIDFromMpxChan(UInt8 mpxChanNum)
{
	UInt8 client_id;

	/* Valid MPX channel number is 0-3 */
	switch(mpxChanNum)
	{
	case 0:
		client_id = CLIENT_ID_V24_0;
		break;
	case 1:
		client_id = CLIENT_ID_V24_1;
		break;
	case 2:
		client_id = CLIENT_ID_V24_2;
		break;
	case 3:
		client_id = CLIENT_ID_V24_3;
		break;
	case 4:
		client_id = CLIENT_ID_V24_4;
		break;

	default:
		client_id = INVALID_CLIENT_ID;
		break;
	}

	return client_id;
}


//******************************************************************************
//
// Function Name:      AT_GetMpxChanFromV24ClientID
//
// Description:        Get MPX channel number from the V24 client ID which is
//					   returned in the response as a result of calling an API function.
//
// Notes:
//
//******************************************************************************
UInt8 AT_GetMpxChanFromV24ClientID(UInt8 clientID)
{
	UInt8 mpx_chan_num;

	switch(clientID)
	{
	case CLIENT_ID_V24_0:
		mpx_chan_num = 0;
		break;
	case CLIENT_ID_V24_1:
		mpx_chan_num = 1;
		break;
	case CLIENT_ID_V24_2:
		mpx_chan_num = 2;
		break;
	case CLIENT_ID_V24_3:
		mpx_chan_num = 3;
		break;
	case CLIENT_ID_V24_4:
		mpx_chan_num = 4;
		break;

	default:
		mpx_chan_num = INVALID_MPX_CHAN;
		break;
	}

	return mpx_chan_num;
}


//******************************************************************************
// Function Name:	ConvertNWcause2CMEerror
// Description:		Converts Network Cause to CME error
//******************************************************************************
UInt16 ConvertNWcause2CMEerror(NetworkCause_t inNetworkCause)
{
	switch(inNetworkCause)
	{
		case GSMCAUSE_ERROR_PASSWD_REG_FAILURE:
		case GSMCAUSE_ERROR_NEGATIVE_PASSWD_CHECK:
			return (AT_CME_ERR_SIM_PASSWD_WRONG);

		case GSMCAUSE_ERROR_BEARER_SVC_NOT_PROVISIONED:
		case GSMCAUSE_ERROR_TELE_SVC_NOT_PROVISIONED:
			return (AT_CME_ERR_OP_NOT_SUPPORTED);

		/************************************************************************
		 * Same as default case, commented out to save code space.
		 ************************************************************************
		case GSMCAUSE_ERROR_UNKNOWN_SUBSCRIBER:
		case GSMCAUSE_ERROR_ILLEGAL_SUBSCRIBER:
		case GSMCAUSE_ERROR_ILLEGAL_EQUIPMENT:
		case GSMCAUSE_ERROR_SS_NOT_AVAILABLE:
		case GSMCAUSE_ERROR_SS_SUBSCRIPTION_VIOLATION:
		case GSMCAUSE_ERROR_FACILITY_NOT_SUPPORT:
		case GSMCAUSE_ERROR_UNKNOWN_ALPHABET:
		case GSMCAUSE_ERROR_ABSENT_SUBSCRIBER:
		case GSMCAUSE_ERROR_SYSTEM_FAILURE:
		case GSMCAUSE_ERROR_DATA_MISSING:
		case GSMCAUSE_ERROR_UNEXPECT_DATA_VALUE:
		case GSMCAUSE_ERROR_MAX_MPTY_EXCEEDED:
		case GSMCAUSE_ERROR_MAX_PASSWD_ATTEMPTS_VOILATION:
		case GSMCAUSE_ERROR_SS_INCOMPATIBILITY:
		case GSMCAUSE_ERROR_SS_ERROR_STATUS:
		case GSMCAUSE_ERROR_USSD_BUSY:
		case GSMCAUSE_ERROR_RESOURCES_NOT_AVAIL:
		case GSMCAUSE_ERROR_ILLEGAL_SS_OPERATION:
		case GSMCAUSE_ERROR_CALL_BARRED:
		*************************************************************************/

		default:
			return (AT_CME_ERR_OP_NOT_ALLOWED);
	}
}

//******************************************************************************
// Function Name:	ConvertRxSignalInfo2Csq
// Description:		Converts values in RxSignalInfo into values for at+csq
//******************************************************************************
void ConvertRxSignalInfo2Csq(UInt8 *rssi, UInt8 *ber)
{
	UInt8 rat;

	rat = MS_GetCurrentRAT();

	if (*rssi != RX_SIGNAL_INFO_UNKNOWN && rat != RAT_NOT_AVAILABLE)
	{
		if (rat == RAT_GSM)
			*rssi = (*rssi >= 59) ? 31 : (*rssi >> 1) + 2;
		else // RAT_UMTS
			*rssi = (*rssi >= 65) ? 31 : (*rssi <= 3) ? 0 : (*rssi >> 1) - 1;
	}
	else
	{
		*rssi = 99;
	}

	if (*ber != RX_SIGNAL_INFO_UNKNOWN && rat == RAT_GSM)
	{
		*ber  = (*ber > 7) ? 99 : *ber;
	}
	else
	{
		*ber = 99;
	}
}
