/*
*******************************************************************************
**  File      : AT_V24.C
**-----------------------------------------------------------------------------
**  Scope     : AT CB and Report functions dealing with V24
**
*******************************************************************************
*/
#define ENABLE_LOGGING

#include "types.h"
#include "mstypes.h"
#include "msconsts.h"

#include "assert.h"
#include "at_v24.h"

#include "atc.h"
#include "at_data.h"
#include "at_util.h"
#include "at_cfg.h"

#include "v24.h"
#include "mpx.h"
#include "ecdc.h"
#include "serial.h"
#include "sysparm.h"
#include "mti_build.h"
#include "ms_database.h"
#include "log.h"
#include "mti_trace.h"
#include "pchapi.h"


//******************************************************************************
// Local consts
//******************************************************************************
// The index is ms class, the value is tx slots number for this ms class
// See GSM 0502_670 ( Annex B )
const UInt8 ms_class_rx_slots[19] = {0,1,2,2,3,2,3,3,4,3,4,4,4,3,4,5,6,7,8};

const UInt8 ms_class_tx_slots[19] = {0,1,1,2,1,2,2,3,1,2,2,3,4,3,4,5,6,7,8};

const UInt8 ms_class_sum_slots[19] ={0,2,3,3,4,4,4,4,5,5,5,5,5,4,5,5,5,5,5};


extern CallConfig_t curCallCfg;
extern void PPP_PDP_Deactivate(UInt8 cid);

//******************************************************************************
//
// Function Name:	AT_ReportV24BreakSignalInd
//
// Description:		This function reports V24 Break Signal Indication.
//
// Notes:
//
//******************************************************************************

void AT_ReportV24BreakSignalInd( UInt8 chan )
{
   //assert(dlc < ATDLCCOUNT);
	// **FixMe** DLC or CHAN?
   if (chan >= ATDLCCOUNT) return;
}


//******************************************************************************
//
// Function Name:	AT_ReportV24ReleaseInd
//
// Description:		This function reports the V24 Release Indication.
// Notes:
//
//******************************************************************************

void AT_ReportV24ReleaseInd( UInt8 chan )
{

	InterTaskMsg_t*      msg;

	if (chan >= ATDLCCOUNT) return;

	msg = AllocInterTaskMsgFromHeap(MSG_AT_CALL_ABORT, sizeof(UInt8));
	*(UInt8*)msg->dataBuf = (UInt8) chan;
	PostMsgToATCTask(msg);

	/* V24 switch from ON LINE to OFF LINE so flush unsollicited response */
	/* toward TE.                                                         */
	ATC_ManageBufferedURsp ();
}


//******************************************************************************
//
// Function Name:       AT_SwitchToATModeInd
//
// Description:         This function reports switch Data connection and AT command.
//                      cause by +++
// Notes:
//
//******************************************************************************

void AT_SwitchToATModeInd( UInt8 chan )
{
	   InterTaskMsg_t*      msg;

	   if (chan >= ATDLCCOUNT) return;

		msg = AllocInterTaskMsgFromHeap(MSG_AT_ESC_DATA_CALL, sizeof(UInt8));
		*(UInt8*)msg->dataBuf = (UInt8) chan;
		PostMsgToATCTask(msg);
}



void AT_ReportV24LineStateInd( UInt8 chan, Boolean DTR_ON )
{

	   //FIXME remove later
	   AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg(chan);

	   MSG_LOGV("MSG:AT_ReportV24LineStateInd(FF/chan/DTR_ON/AmpD)=",(0xFF000000|chan<<16|DTR_ON<<8|channelCfg->AmpD));

	   if (chan >= ATDLCCOUNT) return;

	   if( DTR_ON == 0 )
	   {
		   if( channelCfg->AmpD == 1 )		 // Enter Command mode.
		   {
				AT_SwitchToATModeInd(chan);
		   }
		   else if ( channelCfg->AmpD == 2)   // Disconnect the call.
		   {
//08-12-2003 Ben: disconnect the GPRS connection if host flip DTR. Integrated from GC75
				if (PPP_connect && PDP_GetPPPModemCid()!=NOT_USED_FF && PPP_GetDlc() == chan) {
					MSG_LOG("MSG:DTR low....Close PPP...");
					PPP_PDP_Deactivate(PDP_GetPPPModemCid());
				}

				if (GetDataCallChannel() == chan)
					AT_ReportV24ReleaseInd(chan);
		   }
	   }
}


//******************************************************************************
//
// Function Name:	AT_ReportV24DataInd
//
// Description:		This function reports the V24 Data Indication.
//
// Notes:
//
//******************************************************************************
void AT_ReportV24DataInd( UInt8 chan, UInt8 *p_cmd )
{

   // if MS is in the middle of power down, drop any new AT commands

   if(IsPowerDownInProgress())
   {
	   return;
   }

   if (chan >= ATDLCCOUNT && chan != DLC_STKTERMINAL)
   {
	   return;
   }

   if(V24_GetParserState((DLC_t)chan) != PARSERSTATE_AT_SMS)
   {
		AT_ProcessCmd ( chan, p_cmd ) ;
   }

   // only SMS related cases come to the following section, otherwise it will cause
   // assertion in the SMS module.

   else
   {
	   InterTaskMsg_t*      msg;

	   msg = AllocInterTaskMsgFromHeap(MSG_SMS_USR_DATA_IND, strlen((char *) p_cmd)+1);
	   msg->clientID = AT_GetV24ClientIDFromMpxChan(chan);
	   msg->dataLength = strlen((char *) p_cmd);
	   strcpy((char*)msg->dataBuf, (char *) p_cmd);

	   PostMsgToATCTask(msg);
   }
}
//******************************************************************************
//
// Function Name:	ATC_ReportStartMuxCnf
//
// Description:		This function reports the MPX Start Mux Confirmation.
//
// Notes:
//
//******************************************************************************

void AT_ReportStartMUXCnf( UInt8 chan, MUXParam_t *muxparam )
{
}


void AT_ResetFClass(UInt8 classMode)
{
        curCallCfg.faxParam.fclass = classMode;
}

//******************************************************************************
//
// Function Name:	AT_FindAutoAnswerDlc
//
// Description:		Finds the auto answer dlc according to the following rule.
//					If auto answer is active on more than one DLC, the DLC whose
//					S0 is lowest gets the call.  If the lowest S0 is the same for
//					multiple DLCs, the lowest number DLC gets the call.  This
//					function should only be used in multiplexed mode.
//
// Notes:
//
//******************************************************************************
DLC_t AT_FindAutoAnswerDlc( void )
{
	// **FixMe** use DLC or CHAN?
	UInt8 lowest_s0 = 255,lowest_s0_chan = 0,i;

	for (i=1; i<ATDLCCOUNT; i++)
	{
		AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg((UInt8)i);

		if ((channelCfg->S0 != 0) && (channelCfg->S0 < lowest_s0) &&
			(V24_GetOperationMode((DLC_t)i) == V24OPERMODE_AT_CMD ||
			V24_GetOperationMode((DLC_t)i) == V24OPERMODE_NO_RECEIVE ))
		{
			lowest_s0_chan = i;
			lowest_s0 = channelCfg->S0;
		}
	}

	return ((DLC_t)lowest_s0_chan);

}

//******************************************************************************
//
// Function Name:       AT_StopLogging( )
//
// Description:         Stop logging on the data channel.  For now, this should
//						only be used in the multiplexed mode.
//
// Notes:
//
//******************************************************************************
void AT_StopLogging (void)
{
	AT_GetCommonCfg()->logOn = FALSE;
}

//-----------------------------------------------
// AT_SetForDAIMode(): configure ATC for DAI mode
//-----------------------------------------------
void  AT_SetForDAIMode()
{
	// at DAI mode, AT interface is no longer accessible
	AT_GetChannelCfg(0)->S0 = 2;		// set auto answer to 2 Rings	//	**FixMe** DLC=0
	AT_GetChannelCfg(0)->CREG = 1;	// enable +CREG async event		//	**FixMe** DLC=0
}
