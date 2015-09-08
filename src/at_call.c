//********************************************************************
// Description:  This file defines AT handlers for call-relared at commands.
//********************************************************************
#define ENABLE_LOGGING

#include <ctype.h>
#include "at_util.h"
#include "mti_trace.h"
#include "mti_build.h"
#include "types.h"
#include "osheap.h"
#include "stdlib.h"
#include "string.h"
#include "mti_mn.h"
#include "mstypes.h"
#include "at_data.h"
#include "at_v24.h"
#include "ostask.h"
#include "ostimer.h"
#include "serial.h"
#include "sio.h"
#include "v24.h"
#include "logapi.h"
#include "ecdc.h"
#include "fax.h"
#include "mnatdstypes.h"
#include "mstruct.h"
#include "at_call.h"
#include "ecdcutil.h"
#include "at_api.h"
#include "at_cfg.h"
#include "ms_database.h"
#include "pchapi.h"
#include "callparser.h"
#include "phonebk.h"
#include "phonebk_api.h"
#include "dsapi.h"
#include "dialstr.h"
#include "atc.h"
#include "at_stk.h"
#include "at_ss.h"
#include "dataacct.h"
#include "ss_api.h"
#include "log.h"
#include "osheap.h"
#include "audioapi.h"

#define MANUFACTURE_SPECIFIC_DURATION	100		// 100 ms


#ifdef GCXX_PC_CARD
#define CALL_MONITOR_CMD_STR "*ECAV"
#else
#define CALL_MONITOR_CMD_STR "*MCAM"
#endif

static void ProcessCNAP(UInt8 chan, UInt8 callIndex);

//AT command processes
static Result_t AtProcessDataCallCmd (UInt8 chan, UInt8* phoneNum );
static Result_t AtProcessVoiceCallCmd (UInt8 chan, CallParams_VCSD_t* inVcsdPtr);
static AT_Status_t AtProcessIncomingCall(UInt8 chan,UInt8 callIndex);
static AT_Status_t AtProceessEndCallCmd(UInt8 callIdx);

/////////////////////////////////////////////////////////////

//util functions for formating data call AT report
extern void CallTransparentStr(UInt8 trVal,UInt8* trStr);
extern void DataCompressionStr(UInt8 dcVal,UInt8* dcStr);
extern void ErrorCorrectionStr(UInt8 ecVal,UInt8* ecStr);
extern void UserDataRateStr(UInt8 aiur,UInt8* aiurStr);
extern void LocalRateStr(UInt8 lrVal,UInt8* lrStr);
extern void DataRateStr(UInt8 coding, UInt8 drVal,
						UInt8* aiur,UInt8* drStr);


extern UInt8 AT_GetV24ClientIDFromMpxChan(UInt8 mpxChanNum);
extern UInt8 AT_GetMpxChanFromV24ClientID(UInt8 clientID);

////////////////////////////////////////////////////////////////

extern CallConfig_t			curCallCfg;
extern const DtmfAction_t	DTMFActions[];

extern DLC_t				PppDlc;
extern PCHCid_t				PPP_cid;

extern	UInt8	atcClientID;

static Boolean  isMonitorCalls = FALSE;

static CCallIndexList_t heldCallsList;
static UInt8			heldCallsNum;

static Cause_t	curCallRelCause = MNCAUSE_NORMAL_CALL_CLEARING;

//static variables for handling the incoming call
static Boolean		 incomingCallAuxiliary = FALSE;
static UInt8		 incomingCallIndex = INVALID_CALL;
static UInt8		 incomingRingingCnt= 0;
static Timer_t		 incomingCallTimer = NULL;

static Timer_t		 outgoingCallTimer = NULL;
static UInt8		 outgoingCallIndex = INVALID_CALL;

//Timer for DTMF
static Timer_t dtmfTimer[MAX_CALLS_NO] =
{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

static AT_Status_t AT_Handle_VTS_Command (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,
								const UInt8* _P2,
								const UInt8* _P3,
								Boolean onOff);



//This is declared but not used, therefore it is out commented
//static Timer_t	msKeytoneTimer = NULL;	 //Timer for local MS key tone

//static variables for current at call setup
static UInt8			 curDefaultCSTA	  = UNKNOWN_TON_ISDN_NPI;
static DialString_t		 curCallPhoneNum;


//static variables for the fax setup
static Boolean fax_atds_fco_return = FALSE;
static Boolean fax_atds_fis_return = FALSE;
static Boolean fax_atds_init_connect = TRUE;

//Static util functions
static void UpdateCallLog(UInt8 callIndex,  CCallState_t callSt){

	static UInt8 callLog[MAX_CALLLOG_LEN+1];

	UInt8			phNum[MAX_DIGITS+2];///< (Note: 1 byte for null termination and the other for int code '+')
 	CallATLogType_t callLogType;
	static Boolean	updateLog[MAXNUM_CALLS] = {TRUE,TRUE,TRUE,TRUE,TRUE,TRUE};

	callLog[0] = '\0';
	callLogType = CALLLOG_UNKNOWTY;

	if (isMonitorCalls){

		switch(CC_GetCallType(callIndex)){

			case MOVOICE_CALL:
			case MTVOICE_CALL:
				callLogType = CALLLOG_VOICE;
				break;

			case MODATA_CALL:
			case MTDATA_CALL:
				callLogType = CALLLOG_DATA;
				break;

			case MOVIDEO_CALL:
			case MTVIDEO_CALL:
				callLogType = CALLLOG_VIDEO;
				break;

			case MOFAX_CALL:
			case MTFAX_CALL:
				callLogType = CALLLOG_FAX;
				break;
			default:
				break;
		}


		if (callLogType != CALLLOG_UNKNOWTY){

			switch (callSt){

				case CC_CALL_BEGINNING:
					return;

				//For CC_CALL_CALLING, log the phone number
				case CC_CALL_CALLING:

					if (CC_GetCallNumber(callIndex,phNum) && updateLog[callIndex]){
						sprintf((char*)callLog,"%s: %d,%d,%d,,,%s,%d", CALL_MONITOR_CMD_STR,
							callIndex + 1,callSt,callLogType,
							(char*)(phNum[0] == INTERNATIONAL_CODE ? &phNum[1] : &phNum[0]),
							(phNum[0] == INTERNATIONAL_CODE ? INTERNA_TON_ISDN_NPI : UNKNOWN_TON_ISDN_NPI));

						updateLog[callIndex] = FALSE;

					} else {

						updateLog[callIndex] = TRUE;
						return;
					}


					break;

				case CC_CALL_DISCONNECT:

					updateLog[callIndex] = TRUE;
					//For CC_CALL_DISCONNECT, log the release cause
					sprintf ((char*)callLog, "%s: %d,%d,%d,,%d", CALL_MONITOR_CMD_STR, callIndex + 1,
							callSt,callLogType, curCallRelCause);

					break;


				default:

					sprintf ((char*)callLog, "%s: %d,%d,%d,,", CALL_MONITOR_CMD_STR, callIndex + 1,
							   callSt, callLogType);


					break;

			}
		}

		_TRACE( ( (char*)callLog, callIndex) );

		AT_OutputBlockableUnsolicitedStr(callLog);

	}
}


static void ProcessCallTimer(TimerID_t callTimerID){

	InterTaskMsg_t	*atcMsg;
	CallTimerID_t*	timerID;

	atcMsg = AllocInterTaskMsgFromHeap(MSG_AT_CALL_TIMER, sizeof(CallTimerID_t));
	timerID = (CallTimerID_t*) atcMsg->dataBuf;

	*timerID = callTimerID;

	PostMsgToATCTask(atcMsg);

}

static Timer_t StartCallTimer(CallTimerID_t callTimerID,Ticks_t tk_time){

	Timer_t callTimer = NULL;

	_TRACE(("StartCallTimer: CallTimerID_t = 0x",(UInt8)callTimerID));

	switch (callTimerID){

		case INCOMING_RING_TIMER:

			callTimer = OSTIMER_Create(ProcessCallTimer,(TimerID_t)INCOMING_RING_TIMER,
										0,tk_time);
			break;

		case OUTGOING_CALL_TIMER:

			callTimer = OSTIMER_Create(ProcessCallTimer,(TimerID_t)OUTGOING_CALL_TIMER,
										tk_time,0);
			break;

		default:

			callTimer = OSTIMER_Create(ProcessCallTimer,(TimerID_t)callTimerID,
										tk_time,0);
			break;
	}

	if (callTimer) OSTIMER_Start(callTimer);

	return callTimer;
}

static void StopCallTimer(Timer_t* callTimer){

	if (*callTimer != NULL)
		OSTIMER_Destroy(*callTimer);

	*callTimer = NULL;

}

//******************************************************************************
// Function Name:	ResetIncomingCallRing
//
// Description:		Reset the incoming call ring.
//******************************************************************************
static void ResetIncomingCallRing(UInt8 inCallIndex)
{
	if ((incomingCallIndex == inCallIndex) && incomingCallTimer)
	{
		StopCallTimer(&incomingCallTimer);
		incomingCallIndex = INVALID_CALL;
		incomingRingingCnt = 0;
		V24_SetRI(FALSE);
	}
}

static void EndOutgoingCall(){

	UInt8				callIndex,chan;
	CCallType_t			callType;
	AT_ChannelCfg_t*	atcCfg;
	Result_t			res;

	callIndex = CC_GetCurrentCallIndex();
	callType = CC_GetCallType(callIndex);

	chan = AT_GetMpxChanFromV24ClientID(CC_GetCallClientID(callIndex));
	atcCfg = AT_GetChannelCfg( chan );

	res = CC_EndCall(callIndex);

	if( res == CC_END_CALL_SUCCESS ){

		//AT_OutputRsp( chan, AT_NO_ANSWER_RSP ) ;
		//Close the ATD channel if the timer is up
		AT_CmdCloseTransaction(chan);

		atcCfg->AllowUnsolOutput = TRUE;
		V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );

		if( (callType == MODATA_CALL ) || (callType == MOVIDEO_CALL ) ||
				(callType == MOFAX_CALL) )
		{

 			V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );
			serial_set_v24_flag( V24_AT_CMD ); // AT Cmd Mode
 			V24_SetDataTxMode((DLC_t)chan, TRUE );

		}
	}

}

static void StopGenDTMF(UInt8 callIndex){

	Result_t res;

	_TRACE(("StopGenDTMF: callIndex = 0x",(UInt8)callIndex));

	StopCallTimer(&dtmfTimer[callIndex]);

	res = CC_StopDTMF(callIndex);
	_TRACE(("StopGenDTMF: res = 0x",res));

	//AT_CmdRspOK( AT_GetCmdRspMpxChan(AT_CMD_VTS) );
	AT_ImmedCmdRsp( AT_GetCmdRspMpxChan(AT_CMD_VTS),AT_CMD_VTS,AT_OK_RSP  );
}

/* This function is declared but not used, therefore it is out commented
static void StopGenMsKeyTone(){


	SPEAKER_StopTone( );

	//if there is no voice call, turn off the speaker
	if( !CC_IsThereVoiceCall() ){
		SPEAKER_Off();
	}

	StopCallTimer(&msKeytoneTimer);

	AT_CmdRspOK( AT_GetCmdRspMpxChan(AT_CMD_MKEYTONE) );

}
*/

static Boolean IsDataCall(UInt8 callIndex){

	Boolean		isDataCall = FALSE;
	CCallType_t callType;

	if (callIndex != INVALID_CALL){

		callType = CC_GetCallType(callIndex);

		if (callType != MTVOICE_CALL && callType != MOVOICE_CALL){

			isDataCall = TRUE;
		}
	}

	return isDataCall;
}

Boolean IsDataCallActive(){


	Boolean isDCActive = FALSE;

	UInt8 callIdx = CC_GetCurrentCallIndex();

	if (IsDataCall(callIdx) &&
		CC_GetCallState(callIdx) == CC_CALL_ACTIVE)

		isDCActive = TRUE;


	return isDCActive;
}

UInt8 GetDataCallChannel(){

	UInt8 callIndex,callChan = INVALID_MPX_CHAN;

	callIndex = CC_GetCurrentCallIndex();

	if (IsDataCall(callIndex))
		callChan = AT_GetMpxChanFromV24ClientID(CC_GetCallClientID(callIndex));

	return callChan;

}


static void ProcessCNAP(UInt8 chan, UInt8 callIndex)
{
	const UInt8*	cnapName;
	UInt8			nameLength;
	ALPHA_CODING_t	nameCoding;
	UInt8			atRes[PHASE1_MAX_USSD_STRING_SIZE];


	CC_GetCNAPName(callIndex, &cnapName, &nameLength, &nameCoding);

	MSG_LOGV("ProcessCNAP CNAP name=", cnapName);

	//**FixMe** Robert 3/15/2006 (no UCS2 support for CNAP yet)
	if( (cnapName != NULL) && (nameCoding == ALPHA_CODING_GSM_ALPHABET) )
	{
		sprintf((char*) atRes, "*MCNAP:,, \"%s\"", (char*) cnapName);
		if (chan != INVALID_MPX_CHAN)
		{
			AT_OutputStr(chan, atRes);
		}
		else
		{
            AT_OutputUnsolicitedStr(atRes);
		}
	}
}


static void ProcessCLIP_CWAIT(UInt8 callIndex){


  UInt8  atRes[MAX_RESPONSE_LEN+2];
  UInt8  phNum[MAX_DIGITS+2];///< (Note: 1 byte for null termination and the other for int code '+')

  PBK_API_Name_t pbkAlpha;
  Boolean alphaExist = FALSE;

  //Get the coming phone number
  CC_GetCallNumber(callIndex, phNum);

 /*
  ** -----------------------------------------------------------------------
  **  Handle CLIP supplementary service
  **  If enabled +CLIP: number,type,[subaddr,satype[,alpha,CLI validity]]
  ** -----------------------------------------------------------------------
  */
  if ( MS_GetCfg()->CLIP == 1 )
  {
     if (  phNum[0] != '\0' ){

				/* FixMe: need to add UCS2 support!!! */
                PBK_GetAlpha((char *) phNum, &pbkAlpha);

				if( pbkAlpha.alpha_size >= sizeof(pbkAlpha.alpha) )
				{
					pbkAlpha.alpha_size = sizeof(pbkAlpha.alpha) - 1;
				}

				pbkAlpha.alpha[pbkAlpha.alpha_size] = '\0';

                alphaExist= TRUE;

				sprintf((char*)atRes, "+CLIP: \"%s\",%d,,,\"%s\",%d", (char*)phNum,
					(phNum[0] == INTERNATIONAL_CODE ? INTERNA_TON_ISDN_NPI : UNKNOWN_TON_ISDN_NPI),
					(char *) pbkAlpha.alpha, 0);
     } else{

				sprintf((char*)atRes, "+CLIP: \"\",%d,,,,2", UNKNOWN_TON_UNKNOWN_NPI);
     }

     AT_OutputUnsolicitedStr(atRes);
  }


 /*
  ** -----------------------------------------------------------------------
  **  Handle CWAIT supplementary service
  **  If enabled +CCWA: number,type,class,[alpha][,CLI validity]
  ** -----------------------------------------------------------------------
  */

  if ( CC_GetNextWaitCallIndex() != INVALID_CALL && (MS_GetCfg()->CCWA == 1) ){

     if ( phNum[0]!= '\0' ){

		 if(!alphaExist)
		 {
			 PBK_GetAlpha((char *) phNum, &pbkAlpha);

			 if( pbkAlpha.alpha_size >= sizeof(pbkAlpha.alpha) )
			 {
				 pbkAlpha.alpha_size = sizeof(pbkAlpha.alpha) - 1;
			 }

			 pbkAlpha.alpha[pbkAlpha.alpha_size] = '\0';
		 }

         sprintf((char*)atRes, "+CCWA: \"%s\",%d,,,\"%s\",%d", (char*)phNum,
			 (phNum[0] == INTERNATIONAL_CODE ? INTERNA_TON_ISDN_NPI : UNKNOWN_TON_ISDN_NPI),
			 (char *) pbkAlpha.alpha, 0);

     } else{

        sprintf((char*)atRes , "+CCWA: \"\",%d, 1,,2", UNKNOWN_TON_UNKNOWN_NPI);
     }

     AT_OutputUnsolicitedStr(atRes);
  }
}

static void IncomingCallRing(){

   UInt8 atRes[MAX_RESPONSE_LEN+1];
   CCallType_t				callTy;
   UInt8					S0;
   DLC_t 					autoAnswerDlc = DLC_NONE;


    //If the incomingCallTimer is NULL, we stop the
    //the ring assuming the call already been handled.
    if (incomingCallTimer == NULL ) return;

	if (MPX_ObtainMuxMode() == MUX_MODE)
          autoAnswerDlc = AT_FindAutoAnswerDlc();
	else autoAnswerDlc = 0;

	S0 = AT_GetChannelCfg(autoAnswerDlc)->S0;

	callTy = CC_GetCallType(incomingCallIndex);

	MSG_LOGV("IncomingCallRing()   CCallType_t=", callTy);

	incomingRingingCnt++;

	//when there are no active/held call and ring == S0, auto answer the call
	if ( (incomingRingingCnt == S0) &&
			CC_GetNumofActiveCalls() == 0 &&CC_GetNumofHeldCalls() == 0)
	{

		if (autoAnswerDlc == INVALID_MPX_CHAN &&
			MPX_ObtainMuxMode() == MUX_MODE){
				//No auto answer channel. restart the ring count
				incomingRingingCnt = 0;
		} else {

			AtProcessIncomingCall((UInt8) autoAnswerDlc, incomingCallIndex);
		}

	} else {

			V24_SetRI(TRUE);
			//ring again
			if ( callTy== MTVOICE_CALL )
			{
				AT_OutputUnsolicitedRsp( AT_RING_VOICE_RSP );
				if (incomingCallAuxiliary){
					sprintf ( (char*)atRes, "LINE:%d", incomingCallAuxiliary + 1);
					AT_OutputUnsolicitedStr(atRes);
				}
			}
			else
			{
				if ( curCallCfg.curr_ce == 1 )
				{
					AT_OutputUnsolicitedRsp( AT_RING_RELASYNC_RSP );
				}
				else
				{
					AT_OutputUnsolicitedRsp( AT_RING_ASYNC_RSP );
				}
			}
	}

	if(AT_GetCommonCfg()->CSSU){	// show CNAP after RING if setting permits
		ProcessCNAP(INVALID_MPX_CHAN, incomingCallIndex);
	}

	ProcessCLIP_CWAIT(incomingCallIndex);

	return;
}

#ifndef MULTI_PDP
void ATC_NotifyDataConnectionBroken_t(UInt8					acctId,
									  DC_ConnectionStatus_t	status)
{
	if (acctId == DATA_ACCOUNT_ID_FOR_ATC) {
		ATC_ReportPPPCloseReq( PppDlc );
  		ATC_ReportCmeError(PppDlc, PppRejCause);
		PppDlc = INVALID_DLC;
	}
	else {
		MSG_LOGV("ATC_NotifyDataConnectionBroken_t-acctId not matched, discard:(acctId/status)=",(acctId<<16|status) );
	}
}

void ATC_ReportDataLinkUpStatus_t(	UInt8					acctId,
									DC_ConnectionResult_t	status,
									DC_FailureCause_t		cause,
									DC_ConnectionAddr_t		*addr)
{
	if (acctId == DATA_ACCOUNT_ID_FOR_ATC) {
		if(status == DC_SUCCEED) {
			ATC_ReportPPPOpen( PppDlc );					// Respond to "open" indication
			ATC_PutPPPCmdString( AT_CONNECT_RSP, 0 , PppDlc ); // Tell ATC we're connected
		}
		else {
  			ATC_ReportPPPCloseReq( PppDlc );
  			ATC_ReportCmeError(PppDlc, PppRejCause);
  			PppDlc = INVALID_DLC;
		}
	}
	else {
		MSG_LOGV("ATC_ReportDataLinkUpStatus_t-acctId not matched, discard:(acctId/status)=",(acctId<<16|status) );
	}
}
#endif



void ATProcessCmeError(InterTaskMsg_t *inMsg){
	ATCCmeError_t		*pCmeError;
	UInt16				cmeError;

	pCmeError = inMsg->dataBuf;
	cmeError = pCmeError->errCode;
	MSG_LOGV("ATC:ATProcessCmeError:(dlc/error)=",(pCmeError->chan<<16|pCmeError->errCode) );

	switch (pCmeError->errCode)//11/10/2003 Ben: map the Result_t code into CME ERROR code
	{
		case PDP_INSUFFICIENT_RESOURCES:			//26
			cmeError = AT_CME_ERR_INSUF_RESOURCES;
			break;
		case PDP_MISSING_OR_UNKNOWN_APN:			//27
			cmeError = AT_CME_ERR_MISSING_OR_UNKNOWN_APN;
			break;
		case PDP_UNKNOWN_PDP_ADDRESS:				//28
			cmeError = AT_CME_ERR_UNKNOWN_PDP_ADDRESS;
			break;
		case PDP_USER_AUTH_FAILED:					//29
			cmeError = AT_CME_ERR_PDP_AUTHENTICATE_FAIL;
			break;
		case PDP_ACTIVATION_REJECTED_BY_GGSN:		//30
			cmeError = AT_CME_ERR_ACTIVATION_REJECTED_BY_GGSN;
			break;
		case PDP_ACTIVATION_REJECTED_UNSPECIFIED:	//31
			cmeError = AT_CME_ERR_ACTIVATION_REJECTED;
			break;
		case PDP_SERVICE_OPT_NOT_SUPPORTED:			//32
			cmeError = AT_CME_ERR_SERV_OPTION_NOT_SUPPORTED;
			break;
		case PDP_REQ_SERVICE_NOT_SUBSCRIBED:		//33
			cmeError = AT_CME_ERR_SERV_OPTION_NOT_SUSCRIBED;
			break;
		case PDP_SERVICE_TEMP_OUT_OF_ORDER:			//34
			cmeError = AT_CME_ERR_SERV_OPTION_OUT_OF_ORDER;
			break;
		case PDP_NSAPI_ALREADY_USED:				//35
			cmeError = AT_CME_ERR_NSAPI_ALREADY_USED;
			break;
		case PDP_REGULAR_DEACTIVATION:				//36
			cmeError = AT_CME_ERR_REGULAR_DEACT;
			break;
		case PDP_QOS_NOT_ACCEPTED:					//37
			cmeError = AT_CME_ERR_QOS_NOT_ACCEPTED;
			break;
		case PDP_NETWORK_FAILURE:					//38
			cmeError = AT_CME_ERR_PDP_NETWORK_FAILURE;
			break;
		case PDP_REACTIVATION_REQUIRED:				//39
			cmeError = AT_CME_ERR_REACTIVATION_REQUIRED;
			break;

		case PCH_OPERATION_NOT_ALLOWED:
		default:
			cmeError = AT_CME_ERR_OP_NOT_ALLOWED;
			break;
	}
//CEER error report
	curCallRelCause = cmeError;

//CSP 9323: not send CME ERROR:136 to avoid the further confusion on PC side.
	if (cmeError != AT_CME_ERR_REGULAR_DEACT) {
		AT_CmdRspError(pCmeError->chan, cmeError);
	}
}

void ATC_ReportCmeError(UInt8 dlc, UInt16 errCode)
{
	InterTaskMsg_t		*msg;
	ATCCmeError_t		*pCmeError;

	msg = AllocInterTaskMsgFromHeap(MSG_AT_CME_ERROR, sizeof(ATCCmeError_t));
	if (msg == NULL) {
		MSG_LOG("ATC:ATC_ReportCmeError() got NULL dataBuf");
		return;
	}
	msg->clientID = INVALID_CLIENT_ID; //Broadcast
	msg->dataLength = 2;
	pCmeError = msg->dataBuf;
	pCmeError->chan = dlc;
	pCmeError->errCode = errCode;
	PostMsgToATCTask(msg);
}

/*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/

/************************************************************************************
*
*	Function Name:	ProcessCallReq
*
*	Description:	This function processes a call dialling request.
*
*	UInt8 chan				- MPX channel number.
*	UInt8 *dialStr			- NULL terminated dialing string.
*
*	Notes:
*
*************************************************************************************/
static AT_Status_t ProcessCallReq(UInt8 chan, const UInt8 *dialStr)
{
	CallParams_t			callParams ;
	Boolean					isVoice ;
	AT_Status_t				status;
	Result_t				result = RESULT_OK;

    if ( RESULT_OK == AT_DialStrParse((UInt8*) dialStr, &callParams, &isVoice) )
	{
		switch( callParams.callType )
		{
			case CALLTYPE_SUPPSVC:
				if (callParams.params.ss.ssApiReq.svcCls  ==  SS_SVCCLS_UNKNOWN)
				{
					status = AT_STATUS_DONE;//Return Error
				}
				else
				{
					switch (SS_SsApiReqDispatcher(	AT_GetV24ClientIDFromMpxChan(chan),
													&callParams.params.ss.ssApiReq))
					{
						case RESULT_OK:
						return(AT_STATUS_PENDING);

						case RESULT_DONE:
							AT_CmdRspOK(chan);
							return(AT_STATUS_DONE);

						case STK_DATASVRC_BUSY:
							AT_CmdRsp( chan, AT_BUSY_RSP );
							return(AT_STATUS_DONE);
						case RESULT_ERROR:
						default:
							AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
							return(AT_STATUS_DONE);
					}
				}
				break;

			case CALLTYPE_MOUSSDSUPPSVC:
				{
				USSDString_t ussd;
				UInt8 len;

				len = strlen((char*)callParams.params.ussd.number);
				ussd.used_size = (len > PHASE2_MAX_USSD_STRING_SIZE)? PHASE2_MAX_USSD_STRING_SIZE:len;
				memcpy((void*)ussd.string, (void*)callParams.params.ussd.number, ussd.used_size );
				ussd.dcs = 0x0F;  // This is entered through ATD command, there's no way of entering dcs, use network convention.

				result = SS_SendUSSDConnectReq( AT_GetV24ClientIDFromMpxChan(chan), &ussd);

				if( result == RESULT_OK )
					status = AT_STATUS_PENDING;
				else
					status = AT_STATUS_DONE;
				}
				break ;

			case CALLTYPE_SPEECH:

				if( isVoice ) {

					result = AtProcessVoiceCallCmd( chan, &callParams.params.cc );

				}

				else {

					result = AtProcessDataCallCmd( chan, callParams.params.cc.number );
				}


				if (result == CC_MAKE_CALL_SUCCESS)
				{
						status = AT_STATUS_PENDING;

				}else  {

						status = AT_STATUS_DONE;

				}

				break ;

			case CALLTYPE_GPRS:

				if (PPP_connect) {
					if (MS_IsGprsCallActive(chan)) {
						MSG_LOGV("Ben:Dial GPRS:Close PPP...", (MS_IsGprsCallActive(chan)<<24|PPP_connect<<16|callParams.params.gprs.context_id<<8|chan) );
						PPP_ReportPPPCloseInd( callParams.params.gprs.context_id ) ;
					}
					else {
						MSG_LOGV("Ben:Dial GPRS:PPP.is closing, reject Dial...", (MS_IsGprsCallActive(chan)<<24|PPP_connect<<16|callParams.params.gprs.context_id<<8|chan) );
						ATC_PutPPPCmdString( AT_NO_CARRIER_RSP, 0, chan );

					}
				}
				else {
					PppDlc = chan;
					PPP_cid = callParams.params.gprs.context_id;
					MS_SetChanGprsCallActive(chan,TRUE);
					MS_SetCidForGprsActiveChan(chan, callParams.params.gprs.context_id);

#ifdef MULTI_PDP
					MSG_LOG("DC: DC_SetupDataConnection() -- multiple pdp");
					if ( DC_SetupDataConnection(	atcClientID,
													DATA_ACCOUNT_ID_FOR_ATC,
													DC_MODEM_INITIATED) != RESULT_OK) {
#else
					if ( DC_SetupDataConnection(	DATA_ACCOUNT_ID_FOR_ATC,
													DC_MODEM_INITIATED,
													ATC_ReportDataLinkUpStatus_t,
													ATC_NotifyDataConnectionBroken_t) != RESULT_OK) {
#endif
						MSG_LOGV("ATC:DC_SetupDataConnection failed:", (MS_IsGprsCallActive(chan)<<24|PPP_connect<<16|callParams.params.gprs.context_id<<8|chan) );
						ATC_PutPPPCmdString( AT_BUSY_RSP, 0, chan );
						status = AT_STATUS_DONE ;
						break ;
					}

				}

				status = AT_STATUS_PENDING ;
				break ;

#ifdef ALLOW_PPP_LOOPBACK
			case CALLTYPE_PPP_LOOPBACK:
					//	todo FixMe - integration is pending
				if (IsLocalPPPAvailable() ) {
					MSG_LOGV("ATC:Setup Local PPP Connection,dlc=",chan);
					SetLocalPPPConnection(TRUE);
					PppDlc = chan;
					PPP_cid = LOCAL_PPP_CID;
					MS_SetChanGprsCallActive(chan,TRUE);

#ifdef MULTI_PDP
					MSG_LOG("DC: DC_SetupDataConnection() -- multiple pdp");
					if ( DC_SetupDataConnection(	atcClientID,
													DATA_ACCOUNT_ID_FOR_ATC,
													DC_MODEM_INITIATED) != RESULT_OK) {
#else
					if ( DC_SetupDataConnection(	DATA_ACCOUNT_ID_FOR_ATC,
													DC_MODEM_INITIATED,
													ATC_ReportDataLinkUpStatus_t,
													ATC_NotifyDataConnectionBroken_t) != RESULT_OK) {
#endif
						MSG_LOGV("ATC:DC_SetupDataConnection failed:", (MS_IsGprsCallActive(chan)<<24|PPP_connect<<16|callParams.params.gprs.context_id<<8|chan) );
						status = AT_STATUS_DONE ;
						break ;
					}

					status = AT_STATUS_PENDING ;
				}
				else {
					MSG_LOGV("ATC:Failed to setup Local PPP Connection,dlc=",chan);
					ATC_PutPPPCmdString( AT_NO_CARRIER_RSP, 0, PppDlc );
					status = AT_STATUS_DONE;
				}

				break ;
#endif

			case CALLTYPE_UNKNOWN:

				status = AT_STATUS_DONE ;
				break ;

			default:
				assert(0);
				break;
		}
	}
	else {
		status = AT_STATUS_DONE ;	// DialStrScan returned error
	}

	if (status == AT_STATUS_DONE)
	{
		if (result == CC_FDN_BLOCK_MAKE_CALL)
		{
			AT_CmdRsp(chan, AT_CMS_FDN_NOT_ALLOWED);
		}
		else if (result == STK_DATASVRC_BUSY )
		{
			AT_CmdRsp( chan, AT_BUSY_RSP );

		} else
		{
			AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
		}
	}

	return status;
}


////////////////////AT call message handlers//////////////////////////
Result_t AtHandleCallStatus(InterTaskMsg_t*inMsg){

	 CallStatusMsg_t* callStatusMsg;
	 UInt8			  callIndex,chan;
	 CCallType_t	  callTy;
	 Result_t		  res = RESULT_OK;
	 AT_ChannelCfg_t*	  channelCfg ;


	 chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	 callStatusMsg = (CallStatusMsg_t*)inMsg->dataBuf;
	 callIndex = callStatusMsg->callIndex;

	 _TRACE(("AtHandleCallStatus: callIndex = 0x",(UInt8)callIndex));

	 _TRACE(("AtHandleCallStatus: callStatus = 0x",callStatusMsg->callstatus ));

	 if (chan == INVALID_MPX_CHAN) return res;

	 callTy = CC_GetCallType(callIndex);

	 //Start the timer for the MO call completion
	 if ( (callTy == MOVOICE_CALL || callTy == MODATA_CALL
		   || callTy == MOFAX_CALL || callTy == MOVIDEO_CALL)
		   && chan != INVALID_MPX_CHAN
		   && CC_GetCallState(callIndex) == CC_CALL_CALLING ){

		 channelCfg = AT_GetChannelCfg(chan);

		 //Start the MO call timer if S7 time > 0
		 if ( (outgoingCallTimer == NULL) && (channelCfg->S7 > 0) ){
		    outgoingCallTimer = StartCallTimer(OUTGOING_CALL_TIMER,
											channelCfg->S7*TICKS_ONE_SECOND);
		 }

		 outgoingCallIndex = callIndex;

	 }

	 //Reset the dlc channel back to the original AT mode as long
	 //as the call is disconnected
	 if (CC_GetCallState(callIndex) == CC_CALL_DISCONNECT &&
		 outgoingCallIndex == callIndex){

		V24_SetOperationMode(chan,V24OPERMODE_AT_CMD);
	 }

	 if (callStatusMsg->callstatus != CC_CALL_DISCONNECT){

		UpdateCallLog(callIndex,callStatusMsg->callstatus);
	 }

	 //Update the held call list
	if ( CC_GetCallState(callIndex) == CC_CALL_HOLD ){
		 CC_GetAllHeldCallIndex(&heldCallsList,&heldCallsNum);
	}

	//As long as the call pre-connected, we close the AT call setup transaction to
	//allow the user to send the  early DTMF tone.
	if ( CC_GetCallState(callIndex) == CC_CALL_CONNECTED || CC_GetCallState(callIndex) == CC_CALL_ALERTING)
	{

		_TRACE( ( "AtHandleCallStatus switch V24 mode for immediate AT commands ", chan) );
		//Kludeg which will violate the immediate command rule and allow any AT commands.
		AT_CmdCloseTransaction(chan);
		if (V24_GetOperationMode((DLC_t)chan) == V24OPERMODE_NO_RECEIVE)
			V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );

	}

	return res;
}



Result_t AtHandleCallPreconnect(InterTaskMsg_t*inMsg){

	 VoiceCallPreConnectMsg_t* callPreConnectMsg;
	 UInt8			  callIndex,chan;
	 Result_t		  res = RESULT_OK;

	 chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	 callPreConnectMsg = (VoiceCallPreConnectMsg_t*)inMsg->dataBuf;
	 callIndex = callPreConnectMsg->callIndex;

	 _TRACE(("AtHandleCallPreconnect: callIndex = 0x",(UInt8)callIndex));

	 _TRACE(("AtHandleCallPreconnect: callStatus = 0x",CC_CALL_CONNECTED ));

	 if (chan == INVALID_MPX_CHAN) return res;

	 //Update the call monitor log
	 UpdateCallLog(callIndex,CC_CALL_CONNECTED);

	 _TRACE( ( "AtHandleCallPreconnect switch V24 mode for immediate AT commands ", chan) );
		//Kludeg which will violate the immediate command rule and allow any AT commands.
	AT_CmdCloseTransaction(chan);
	if (V24_GetOperationMode((DLC_t)chan) == V24OPERMODE_NO_RECEIVE)
			V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );

	return res;
}


Result_t AtHandleCallActionRes(InterTaskMsg_t*inMsg){


	VoiceCallActionMsg_t*	actionMsg;
	Result_t				res = RESULT_OK;
 	UInt8					chan;
	CCallIndexList_t		allCallIdx;
	UInt8					totalCalls,i,idx;
	CCallState_t			callState;


	chan =	AT_GetMpxChanFromV24ClientID(inMsg->clientID);

	actionMsg = (VoiceCallActionMsg_t*)inMsg->dataBuf;


	_TRACE(("AtHandleCallActionRes: callIndex = 0x",
							(UInt8)actionMsg->callIndex));

	_TRACE(("AtHandleCallActionRes: Result_t = 0x",
							(UInt8)actionMsg->callResult));

	_TRACE(("AtHandleCallActionRes: chan = 0x",
							(UInt8)chan));

		//If the chld is not init. from AT, ignore it.
	if ( chan == INVALID_MPX_CHAN) return res;

	switch(actionMsg->callResult){

		case CC_SPLIT_CALL_SUCCESS:

			callState = CC_GetCallState((UInt8)actionMsg->callIndex);
			//Since the MN only send call split result back for the
			//active call. So the upper layer needs to print the all calls
			//status for the call monitor purpose.
			CC_GetAllCallIndex(&allCallIdx,&totalCalls);
			for (i = 0; i < totalCalls; i++){
					idx = allCallIdx[i];
				    //skip some calls
					if (idx == VALID_CALL || !CC_IsMultiPartyCall(idx) ||
						(callState != CC_CALL_HOLD && callState != CC_CALL_ACTIVE) ||
						idx == (UInt8)actionMsg->callIndex) {
							continue;
					}
					UpdateCallLog(idx,CC_GetCallState(idx));
			}

		case CC_JOIN_CALL_SUCCESS:
		case CC_SWAP_CALL_SUCCESS:
		case CC_HOLD_CALL_SUCCESS:
		case CC_RESUME_CALL_SUCCESS:
		case CC_TRANS_CALL_SUCCESS:


			AT_CmdRspOK(chan);

			break;

		case CC_JOIN_CALL_FAIL:
		case CC_SPLIT_CALL_FAIL:
		case CC_SWAP_CALL_FAIL:
		case CC_HOLD_CALL_FAIL:
		case CC_RESUME_CALL_FAIL:
		case CC_TRANS_CALL_FAIL:


			//Reponse Error and clear AT pending status
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);

			break;

		//Ignore these messages now
		case CC_ACCEPT_CALL_SUCCESS:
		case CC_ACCEPT_CALL_FAIL:

			if (AT_GetCmdRspMpxChan(AT_CMD_CHLD) != INVALID_MPX_CHAN){
				AT_CmdRspOK(chan);
			}

			break;

		case CC_HANGUP_CALL_SUCCESS:
		case CC_HANGUP_CALL_FAIL:
		case CC_END_CALL_SUCCESS:
		case CC_END_CALL_FAIL:

			AT_CmdRspOK(chan);

			break;

		default:
			break;
	}

	return res;

}

Result_t AtHandleCallConnect(InterTaskMsg_t*inMsg){

	VoiceCallConnectMsg_t	*connectMsg;
	CCallType_t				callType;
	UInt8					 callIndex,chan;
	Result_t				 res = RESULT_OK;

	chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	connectMsg = (VoiceCallConnectMsg_t*)inMsg->dataBuf;
	callIndex = connectMsg->callIndex;

	UpdateCallLog(callIndex,CC_CALL_ACTIVE);

	//Stop the outgoing call timer
	if ( chan != INVALID_MPX_CHAN
		&& outgoingCallIndex == callIndex ){

		if (outgoingCallTimer != NULL) StopCallTimer(&outgoingCallTimer);
		outgoingCallIndex = INVALID_CALL;
	}

	ResetIncomingCallRing(callIndex);

	_TRACE(( "AtHandleCallConnect index = ", callIndex));
	_TRACE(( "AtHandleCallConnect chan = ", chan));

	//No CONNECT for the Chld command which should be handled
	//by the AtHandleCallActionRes
  	if (chan != INVALID_MPX_CHAN && chan == AT_GetCmdRspMpxChan(AT_CMD_CHLD)) return res;

	if (chan != INVALID_MPX_CHAN){
		V24_SetOperationMode((DLC_t)chan,  V24OPERMODE_AT_CMD );
  		AT_OutputRsp( chan,AT_CONNECT_RSP );

		//Only close the transaction on the same channel
  		if ( (chan == AT_GetCmdRspMpxChan(AT_CMD_D)) ||
  			 (chan == AT_GetCmdRspMpxChan(AT_CMD_A)) ){

  			 _TRACE( ( "AtHandleCallConnect close channel on ", chan) );
  			 AT_CmdCloseTransaction(chan);
  		}
	} else {
		AT_OutputUnsolicitedRsp(AT_CONNECT_RSP);
	}

	// show CNAP for MO call (only) after CONNECT if user setting permits
	callType = CC_GetCallType(callIndex);
	if( (AT_GetCommonCfg()->CSSI) && (callType == MOVOICE_CALL) )
	{
		ProcessCNAP(chan, callIndex);
	}

	return res;
}


Result_t AtHandleCallRelease(InterTaskMsg_t*inMsg){

	VoiceCallReleaseMsg_t*	releaseMsg;
	UInt8					idx, callIndex,chan;
	UInt16					report;
	CCallType_t				callTy;
	V24OperMode_t			v24Mode;
	Result_t				res = RESULT_OK;


	chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);

	releaseMsg = (VoiceCallReleaseMsg_t*)inMsg->dataBuf;
	callIndex = releaseMsg->callIndex;
	callTy =  CC_GetCallType(callIndex);

	_TRACE( ( "AtHandleCallRelease index =  ", callIndex) );
	_TRACE( ( "AtHandleCallRelease chan =  ", chan) );


	curCallRelCause = releaseMsg->exitCause;

	//Update the call monitor log
	UpdateCallLog(callIndex,CC_CALL_DISCONNECT);

	//Stop the outgoing call timer
	if ( chan != INVALID_MPX_CHAN &&
		outgoingCallIndex == callIndex){

		if (outgoingCallTimer != NULL) StopCallTimer(&outgoingCallTimer);
		outgoingCallIndex = INVALID_CALL;

	}

	ResetIncomingCallRing(callIndex);

	if ( chan != INVALID_MPX_CHAN ){

		v24Mode = V24_GetOperationMode((DLC_t)chan);

		if ( v24Mode == V24OPERMODE_DATA ||
			 v24Mode == V24OPERMODE_FAX_DATA || outgoingCallIndex == INVALID_CALL){

			_TRACE( ( "AtHandleCallRelease Set AT mode on ", chan) );
			V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );
		}


	}


	// If the release is on a held call  and CSSN active,
	// Need to output CSSU
	// Release call on hold and CSSN active
	if ( AT_GetCommonCfg()->CSSU){

		for (idx = 0; idx < heldCallsNum; idx++){

			if (callIndex == heldCallsList[idx]){
					//Update the held list and held number
					CC_GetAllHeldCallIndex(&heldCallsList,&heldCallsNum);

					if (callTy == MTVOICE_CALL)//Only apply to MT voice call
						AT_OutputStr( chan, (UInt8*)"+CSSU: 5" );
					break;
			}
		}
	}

	//Then release report fo the exit cause
	switch ( curCallRelCause )
	{
		case  MNCAUSE_USER_BUSY :
			report = AT_BUSY_RSP;
			break;
		case  MNCAUSE_NO_USER_RESPONDING :
		case  MNCAUSE_USER_ALERTING_NO_ANSWR :
			report = AT_NO_ANSWER_RSP;
			break;
		default:
			report = AT_NO_CARRIER_RSP;
			break;
	}


	//For non-estalished MT voice call, the report should be
	//boardcast to all channels.
	if ( chan == INVALID_MPX_CHAN ){
          AT_OutputUnsolicitedRsp(report);
	} else {
	      AT_OutputRsp( chan, report );

	}

	// if the callee is busy (cause BUSY tone) and the user did not hangup the call early
	// enough then the network will release the call connection, and the BUSY tone will be
	// played forever (because there will not be a valid call session and there will be no
	// way to stop the tone by the ATH).  To fix this problem, we need to force tone off
	// when call is released.
	if(report == AT_BUSY_RSP){
		AUDIO_StopPlaytone();
	}

	//put here to guarantee the correct AT transaction/mode, but it is sort of overkill.
	//If there is outstanding ATD, close it and set back to normal mode.
	if (chan == AT_GetCmdRspMpxChan(AT_CMD_D) ){
		_TRACE( ( "AtHandleCallRelease AT_CmdCloseTransaction on ", chan) );
		AT_CmdCloseTransaction(chan);
		v24Mode = V24_GetOperationMode((DLC_t)chan);

		if (v24Mode == V24OPERMODE_NO_RECEIVE){

			_TRACE( ( "AtHandleCallRelease set AT mode on ATD channel ", chan) );
			V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );
		  }
	}


	//If there is  the ATH/+CHUP/ATA hanging on, close the transaction
	if ( (chan = AT_GetCmdRspMpxChan(AT_CMD_H)) != INVALID_MPX_CHAN ||
		 (chan = AT_GetCmdRspMpxChan(AT_CMD_CHUP)) != INVALID_MPX_CHAN ||
		 (chan = AT_GetCmdRspMpxChan(AT_CMD_A)) != INVALID_MPX_CHAN ||
		 (chan = AT_GetCmdRspMpxChan(AT_CMD_CHLD)) != INVALID_MPX_CHAN ){

		_TRACE( ( "AtHandleCallRelease close channel on ", chan) );
		AT_CmdCloseTransaction(chan);
	}

	return res;
}



Result_t AtHandleCallIncoming(InterTaskMsg_t*inMsg){

	UInt8				atRes[MAX_RESPONSE_LEN+1];
	CallReceiveMsg_t*	incomingMsg;
	UInt8				callIndex;
	CCallType_t			callTy;
	Result_t			res = RESULT_OK;

	incomingMsg = (CallReceiveMsg_t*)inMsg->dataBuf;
	callIndex = incomingMsg->callIndex;

	callTy = CC_GetCallType(callIndex);

	if (callTy != MTVOICE_CALL && callTy != MTVIDEO_CALL &&
		callTy != MTDATA_CALL && callTy != MTFAX_CALL) {
		_TRACE(("WARNING: AtHandleCallIncoming: mismatched callty,index = ", (0xFF0000 | (callTy << 8) |  callIndex) ));
		return RESULT_ERROR;
	}
	//Update the call monitor log
	UpdateCallLog(callIndex,CC_CALL_ALERTING);

	V24_SetRI(TRUE);

	AT_OutputUnsolicitedRsp( callTy == MTVOICE_CALL ? AT_RING_VOICE_RSP : AT_RING_ASYNC_RSP );

	if (incomingMsg->auxiliarySpeech && callTy == MTVOICE_CALL){

		sprintf ( (char*)atRes, "LINE:%d", incomingMsg->auxiliarySpeech + 1);
		AT_OutputUnsolicitedStr(atRes);
	}

	//Keep the call index for the incoming call timer
	incomingCallAuxiliary = incomingMsg->auxiliarySpeech;
	incomingCallIndex = callIndex;
	incomingCallTimer = StartCallTimer(INCOMING_RING_TIMER,3*TICKS_ONE_SECOND);

	return res;
}

//******************************************************************************
// Function Name:	ATHandleApiClientCmdInd
//
// Description:		The function would reset the incoming call paramter, if the
//					client who respond to the incoming call was ATC task.
//******************************************************************************
void ATHandleApiClientCmdInd(InterTaskMsg_t* inMsgPtr)
{
	ApiClientCmdInd_t*		theClientCmdPtr;
	ApiClientCmd_CcParam_t*	theCcParamPtr;

	theClientCmdPtr = (ApiClientCmdInd_t*)inMsgPtr->dataBuf;

	if (atcClientID != inMsgPtr->clientID)
	{
		switch (theClientCmdPtr->clientCmdType)
		{
			case CC_CALL_ACCEPT:
				theCcParamPtr = (ApiClientCmd_CcParam_t*)theClientCmdPtr->paramPtr;

				if (theCcParamPtr->result == CC_ACCEPT_CALL_SUCCESS)
				{
					ResetIncomingCallRing(theCcParamPtr->callIndex);
				}
				break;

			case CC_CALL_REJECT:
				theCcParamPtr = (ApiClientCmd_CcParam_t*)theClientCmdPtr->paramPtr;

				if (theCcParamPtr->result == CC_END_CALL_SUCCESS)
				{
					ResetIncomingCallRing(theCcParamPtr->callIndex);
				}
				break;

			default:
				break;
		}
	}

	if (theClientCmdPtr->paramPtr)
	{
		OSHEAP_Delete(theClientCmdPtr->paramPtr);
	}
}


Result_t AtHandleCallWaiting(InterTaskMsg_t*inMsg){

	UInt8					atRes[MAX_RESPONSE_LEN+1];
	VoiceCallWaitingMsg_t*  waitingMsg;
	UInt8					callIndex;
	CCallType_t				callTy;
	Result_t				res = RESULT_OK;

	waitingMsg = (VoiceCallWaitingMsg_t*)inMsg->dataBuf;
	callIndex = waitingMsg->callIndex;

	//Update the call monitor log
	UpdateCallLog(callIndex,CC_CALL_WAITING);
	callTy = CC_GetCallType(callIndex);

	V24_SetRI(TRUE);

	AT_OutputUnsolicitedRsp( (callTy == MOVOICE_CALL || callTy == MTVOICE_CALL) ?
				    AT_RING_VOICE_RSP : AT_RING_ASYNC_RSP );

	if (waitingMsg->auxiliarySpeech && callTy == MTVOICE_CALL){
		sprintf ( (char*)atRes, "LINE:%d", waitingMsg->auxiliarySpeech + 1);
		AT_OutputUnsolicitedStr(atRes);
	}

	//Keep the call index for the incoming call timer
	incomingCallAuxiliary = waitingMsg->auxiliarySpeech;
	incomingCallIndex = callIndex;
	incomingCallTimer = StartCallTimer(INCOMING_RING_TIMER,3*TICKS_ONE_SECOND);

	return res;
}

Result_t AtHandleDataCallRelease(InterTaskMsg_t*inMsg){

	DataCallReleaseMsg_t*	releaseMsg;
	UInt8					callIndex,chan;
	UInt16					causeReport;
	Cause_t					cause;
	Result_t				res = RESULT_OK;
	V24OperMode_t			v24Mode;
	AT_ChannelCfg_t*		atcCfg;

	chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	releaseMsg = (DataCallReleaseMsg_t*)inMsg->dataBuf;
	callIndex = releaseMsg->callIndex;

	_TRACE(("AtHandleDataCallRelease: callIndex = 0x",(UInt8)callIndex));
	_TRACE(("AtHandleDataCallRelease: chan = 0x",(UInt8)chan));

	//Stop the outgoing call timer
	if (chan != INVALID_MPX_CHAN){
		if (outgoingCallTimer != NULL) StopCallTimer(&outgoingCallTimer);
		outgoingCallIndex = INVALID_CALL;
	}

	ResetIncomingCallRing(callIndex);

	cause = CC_GetLastCallExitCause();

	switch ( curCallRelCause = cause )
	{
		case  MNCAUSE_USER_BUSY :
			causeReport = AT_BUSY_RSP;
			break;
		case  MNCAUSE_NO_USER_RESPONDING :
		case  MNCAUSE_USER_ALERTING_NO_ANSWR :
			causeReport = AT_NO_ANSWER_RSP;
			break;
		default:
			causeReport = AT_NO_CARRIER_RSP;
			break;
	}

	//Update the call monitor log
	UpdateCallLog(callIndex,CC_CALL_DISCONNECT);

	//Data call-related clean up
	serial_set_v24_flag( V24_AT_CMD ); // AT Cmd Mode

	if ( chan != INVALID_MPX_CHAN){

		V24_SetDataTxMode((DLC_t) chan, TRUE );

		v24Mode = V24_GetOperationMode((DLC_t)chan);

		if ( v24Mode == V24OPERMODE_DATA ||
			 v24Mode == V24OPERMODE_NO_RECEIVE ||
			 v24Mode == V24OPERMODE_FAX_DATA ){

			V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );
		}

		OSTASK_Sleep( TICKS_ONE_SECOND /1000);
		V24_ResetRxBuffer( (DLC_t)chan);
		V24_FlushTxDataBuf( (DLC_t)chan);

		SetV24DCD (chan, TRUE );

	}

	// Sleep for 50 ms to allow switching to the right buffers
	// in an error corrected / compressed mode.
	OSTASK_Sleep( TICKS_ONE_SECOND / 20 );

	//Response release cause report
	if ( chan != INVALID_MPX_CHAN){
		 atcCfg = AT_GetChannelCfg( chan );
		 atcCfg->AllowUnsolOutput = TRUE;
	  	 AT_OutputRsp( chan, causeReport );
		 //If there is outstanding ATD, close
		 if (chan == AT_GetCmdRspMpxChan(AT_CMD_D)){
			 AT_CmdCloseTransaction(chan);
		 }
	}else AT_OutputUnsolicitedRsp(causeReport);

	/* If we set AT_NO_RECEIVE_MODE before, set it to Command mode */
	if( serial_get_v24_flag() == V24_NO_RECEIVE )
	{
		serial_set_v24_flag( V24_AT_CMD );
	}


	//If there is  the ATH/+CHUP/ATA hanging on, close the transaction
	if  ( (chan = AT_GetCmdRspMpxChan(AT_CMD_H)) != INVALID_MPX_CHAN ||
		  (chan = AT_GetCmdRspMpxChan(AT_CMD_CHUP)) != INVALID_MPX_CHAN ||
		  (chan = AT_GetCmdRspMpxChan(AT_CMD_A)) != INVALID_MPX_CHAN ){

		_TRACE( ( "AtHandleDataCallRelease close channel on ", chan) );
		AT_CmdCloseTransaction(chan);
	}

	return res;
}


Result_t AtHandleDataCallStatus(InterTaskMsg_t *inMsg){


	UInt8					atRes[MAX_RESPONSE_LEN+1];
	DataCallStatusMsg_t*	dataCallStatusMsg;
	MNATDSMsg_t*    		mnatds_msg;
	UInt8					call_index,chan;
	Result_t				res = RESULT_OK;

	Stack_Fax_Param_t*	fax_param = FAX_GetRefreshFaxParam();
	AT_ChannelCfg_t*		channelCfg;

	chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	dataCallStatusMsg = (DataCallStatusMsg_t*) inMsg->dataBuf;
	call_index = dataCallStatusMsg->callIndex;
	mnatds_msg = &dataCallStatusMsg->mnatds_msg;

	_TRACE(("AtHandleDataCallStatus: callIndex = 0x",call_index));
	_TRACE(("AtHandleDataCallStatus: StatusMsg type= 0x",mnatds_msg->type));

	//Update the call monitor log
	//UpdateCallLog(call_index,CC_GetCallState(call_index));

	if ( chan == INVALID_MPX_CHAN ) return res;

	channelCfg =  AT_GetChannelCfg(chan);

	switch(mnatds_msg->type)
	{
		//MO data call connect indication
		case MNATDSMSG_ATDS_CONNECT_IND:
		//MT data call confirm
		case MNATDSMSG_ATDS_SETUP_CNF:
			break;

		case MNATDSMSG_ATDS_STATUS_IND:
			break;

		//For Fax response
		case MNATDSMSG_ATDS_FET_IND:
			sprintf( (char*) atRes, "+FET:%d",mnatds_msg->parm.fet_ind.ppm );
			AT_OutputStr(chan, atRes );
			break;

		case MNATDSMSG_ATDS_FIS_IND:
			if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2dot0) {
				sprintf( (char*) atRes, "+FIS:%d,%d,%d,%d,%d,%d,%d,%d",
						 fax_param->vr_current,
						 fax_param->br_current,
						 fax_param->wd_current,
						 fax_param->ln_current,
						 fax_param->df_current,
						 fax_param->ec_current,
						 fax_param->bf_current,
						 fax_param->st_current);
			} else if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2) {
				sprintf( (char*) atRes, "+FDIS:%d,%d,%d,%d,%d,%d,%d,%d",
						 fax_param->vr_current,
						 fax_param->br_current,
						 fax_param->wd_current,
						 fax_param->ln_current,
						 fax_param->df_current,
						 fax_param->ec_current,
						 fax_param->bf_current,
						 fax_param->st_current);
			}
			AT_OutputStr(chan, atRes );
			fax_atds_fis_return = TRUE;
			break;

		case MNATDSMSG_ATDS_FCO_IND:
			if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2dot0)
			{
				sprintf( (char*) atRes, "+FCO" );
			}
			else if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2)
			{
				sprintf( (char*) atRes, "+FCON" );
			}
			AT_OutputStr(chan, atRes );
			fax_atds_fco_return = TRUE;
			break;

		case MNATDSMSG_ATDS_FCI_IND:
			if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2dot0)
			{
				sprintf( (char*) atRes, "+FCI:%c%s%c",'"',fax_param->fci,'"' );
			}
			else if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2)
			{
				sprintf( (char*) atRes, "+FCSI:%c%s%c",'"',fax_param->fci,'"' );
			}
			AT_OutputStr(chan, atRes );
			break;

		case MNATDSMSG_ATDS_CONNECT:

			AT_CmdRsp( chan,AT_CONNECT_RSP );

			/*Set fax data mode to v24 now*/
			/*attach to the serial ECDC first time*/
			if (fax_atds_init_connect)
			{
				_TRACE( ("fax_atds_init_connect now ") );
				fax_atds_init_connect = FALSE;
				SERIAL_SetAttachedDLC( (DLC_t)chan );
 				serial_set_v24_flag( V24_DATA );
				V24_SetDataTxMode((DLC_t) chan, FALSE );
			}

			/*Set fax data mode to v24 now*/
 			V24_SetOperationMode((DLC_t)chan,V24OPERMODE_FAX_DATA );
			break;

		case MNATDSMSG_ATDS_OK:
			/*Set back to AT command mode. So The FDT, FDR or other fax commands can be sent after OK*/
			V24_SetOperationMode((DLC_t)chan,V24OPERMODE_FAX_CMD);
			OSTASK_Sleep(TICKS_ONE_SECOND / 50);

			AT_CmdRspOK(chan);

			break;

		case MNATDSMSG_ATDS_FCFR_IND:
			if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2)
			{
				AT_OutputStr(chan, (UInt8*)"+FCFR" );
			}

			break;

		case MNATDSMSG_ATDS_FCS_IND:
			if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2dot0)
			{
				sprintf( (char*) atRes, "+FCS:%d,%d,%d,%d,%d,%d,%d,%d",
					 fax_param->vr_current,
					 fax_param->br_current,
					 fax_param->wd_current,
					 fax_param->ln_current,
					 fax_param->df_current,
					 fax_param->ec_current,
					 fax_param->bf_current,
					 fax_param->st_current);
			}
			else if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2)
			{
				sprintf( (char*) atRes, "+FDCS:%d,%d,%d,%d,%d,%d,%d,%d",
					 fax_param->vr_current,
					 fax_param->br_current,
					 fax_param->wd_current,
					 fax_param->ln_current,
					 fax_param->df_current,
					 fax_param->ec_current,
					 fax_param->bf_current,
					 fax_param->st_current);
			}
			AT_OutputStr(chan, atRes );
			break;

		case MNATDSMSG_ATDS_FHS_IND:
			if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2dot0)
			{
				sprintf( (char*) atRes,"+FHS:%x",fax_param->hangup_cause);
			}
			else if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2)
			{
				sprintf( (char*) atRes,"+FHNG:%x",fax_param->hangup_cause);
			}
			AT_OutputStr(chan, atRes );
			fax_atds_init_connect = TRUE;
			break;

		case MNATDSMSG_ATDS_FPS_IND:
			if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2dot0)
			{
				sprintf( (char*) atRes,"+FPS:%d,%d,0,0,0",fax_param->fps.current_value,df2_get_line_counter());
			}
			else if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2)
			{
				sprintf( (char*) atRes,"+FPTS:%d,%d,0,0,0",fax_param->fps.current_value,df2_get_line_counter());
			}
			AT_OutputStr(chan, atRes );
			break;

		case MNATDSMSG_ATDS_FTI_IND:
			if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2dot0)
			{
			   sprintf( (char*) atRes, "+FTI:%c%s%c",'"',fax_param->fti,'"');
			}
			else if (fax_param->fclass.current_value == AT_FCLASSSSERVICEMODE_FAX_2)
			{
				sprintf( (char*) atRes, "+FTSI:%c%s%c",'"',fax_param->fti,'"');
			}
			AT_OutputStr(chan, atRes );
			break;

		case MNATDSMSG_ATDS_FVO_IND:
			sprintf( (char*) atRes, "+FVO");
			AT_OutputStr(chan, atRes );
			break;

#if !defined(STACK_wedge)
		//For HSCSD modificatio response
		case MNATDSMSG_ATDS_MODIFY_CNF :
			// This will respond on the DLC on which the request was received.
			AT_CmdRspOK( AT_GetCmdRspMpxChan(AT_CMD_CHSN) );
			break;

		case MATDSMSG_ATDS_MODIFY_REJ :
			// This will respond on the DLC on which the request was received.
			AT_CmdRspError( AT_GetCmdRspMpxChan(AT_CMD_CHSN),AT_CME_ERR_OP_NOT_ALLOWED );
			break;
#endif // #if !defined(STACK_wedge)

		default:
			break;
	}

	return res;
}

Result_t AtHandleDataEcdcLinkRsp(InterTaskMsg_t *inMsg){

	DataECDCLinkMsg_t*	ecdcLinkMsg;
	UInt8				callIndex,chan;
	Result_t			res = RESULT_OK;

	AT_ChannelCfg_t*		channelCfg;
	Stack_Fax_Param_t*	fax_param = FAX_GetRefreshFaxParam();

	chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	ecdcLinkMsg = (DataECDCLinkMsg_t*)inMsg->dataBuf;
	callIndex = ecdcLinkMsg->callIndex;

	if (chan != INVALID_MPX_CHAN ){
		channelCfg = AT_GetChannelCfg(chan);

		// Update the fax param class with the value that has been stored in the stack.
		curCallCfg.faxParam.fclass = fax_param->fclass.current_value;
	}

	if( ecdcLinkMsg->ecdcResult == ECDCSTATUS_SUCCESS )
	{
		//SetV24DCD (chan, FALSE );
		//SERIAL_SetAttachedDLC( chan );
	 	//serial_set_v24_flag( V24_DATA );
	}

	else
	{
		if ( callIndex != INVALID_CALL ) AtProceessEndCallCmd(callIndex);
	}

	return res;
}


Result_t AtHandleDataCallConnect(InterTaskMsg_t *inMsg){

	UInt8 atRes[MAX_RESPONSE_LEN+1];
	DataCallConnectMsg_t*	connectMsg;
	UInt8					callIndex,chan;
	CCallType_t				callType;
	Result_t				res = RESULT_OK;

	AT_ChannelCfg_t*			channelCfg;

	chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	connectMsg	= (DataCallConnectMsg_t*)inMsg->dataBuf;
	callIndex	= connectMsg->callIndex;
	callType	= CC_GetCallType(callIndex);
	atRes[0]	= '\0';

	_TRACE(("AtHandleDataCallConnect: chan,index = ", (0xFF0000 | chan << 8) |  callIndex ));

	UpdateCallLog(callIndex,CC_CALL_ACTIVE);

	ResetIncomingCallRing(callIndex);

	if (chan != INVALID_MPX_CHAN){

		channelCfg = AT_GetChannelCfg(chan);

		//SERIAL_SetAttachedDLC( chan);
		//SetV24DCD(chan, FALSE );

		//Stop the outgoing call timer
		if (outgoingCallTimer != NULL)
			StopCallTimer(&outgoingCallTimer);

		outgoingCallIndex = INVALID_CALL;


		if( callType != MOFAX_CALL && callType != MTFAX_CALL )
		{

			//V24_SetDataTxMode((DLC_t)chan, FALSE );
			//V24_SetOperationMode((DLC_t)chan,V24OPERMODE_DATA );

			//Various CSD call reports ......

			//Report Transparent/NON Transparent
			if ( channelCfg->CR == 1 )
			{
				CallTransparentStr(curCallCfg.curr_ce,atRes);
				AT_OutputStr(chan, (const UInt8*)atRes );
			}

			// Report Error Correction Values.
			if( channelCfg->ER ==1 || channelCfg->ER == 3 )
			{
				ErrorCorrectionStr(curCallCfg.EC,atRes);
				AT_OutputStr(chan, (const UInt8*)atRes  );
			}

			// Report Data Compression Values
			if( channelCfg->DR == 1 )
			{
				DataCompressionStr(curCallCfg.DC,atRes);
				AT_OutputStr(chan, (const UInt8*)atRes );
			}

#if !defined(STACK_wedge)
			if( curCallCfg.hscsd_call && channelCfg->CHSR )
			{

				sprintf( (char*)atRes, "%s: %d,%d,%d,%d", AT_GetCmdName( AT_CMD_CHSR ), curCallCfg.rx,
					curCallCfg.tx, curCallCfg.aiur, curCallCfg.curr_coding );
				AT_OutputStr(chan, (const UInt8*)atRes );
			}
#endif // #if !defined(STACK_wedge)

			// Report local data rate
			if( channelCfg->ILRR == 1 )
			{
				LocalRateStr(V24_ReturnBaudRate( ),atRes);
				AT_OutputStr(chan, (const UInt8*)atRes );
			}


			if ( channelCfg->V == 1 && channelCfg->X)
			{
				if( curCallCfg.hscsd_call )
				{

					DataRateStr(curCallCfg.curr_coding,
								curCallCfg.rx,
								&curCallCfg.aiur,atRes);

					AT_OutputStr(chan, (const UInt8*)atRes );

				}
				else
				{
					AT_CmdRsp(chan, AT_CONNECT_9600_RSP );
				}

			}
			else
			{
				AT_CmdRsp(chan, AT_CONNECT_RSP );
			}

		}

		SetV24DCD(chan, FALSE );

	} else {

			AT_OutputUnsolicitedRsp(AT_CONNECT_RSP);
	}

   return res;
}

Result_t AtHandleConnectedLineID(InterTaskMsg_t *inMsg)
{
	UInt8	callIdx, chan;
	char	theStrBuf[MAX_RESPONSE_LEN + 1];

	chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);

	if (chan == INVALID_MPX_CHAN) return RESULT_OK;

	callIdx	= *(UInt8*)inMsg->dataBuf;

	if (CC_IsConnectedLineIDPresentAllowed(callIdx))
	{
		if (MS_GetCfg()->COLP)
		{
			sprintf(theStrBuf,
					"%s: %s",
					AT_GetCmdName(AT_CMD_COLP),
					CC_GetConnectedLineID(callIdx));
			AT_OutputStr(chan, (UInt8*)theStrBuf);
		}
	}

	return RESULT_OK;
}

Result_t AtHandleCallCCM(InterTaskMsg_t *inMsg){


	UInt8			atRes[MAX_RESPONSE_LEN+1];
	CallCCMMsg_t*	ccmMsg;
	UInt8			chan;

	chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	if (chan == INVALID_MPX_CHAN) return RESULT_OK;

	ccmMsg = (CallCCMMsg_t*) inMsg->dataBuf;

	if (AT_SoliciteCM())
	{

		sprintf( (char*)atRes, "+CCCM: \"%06x\"",(int)ccmMsg->callCCM );

		AT_OutputUnsolicitedStr( atRes );
	}

	if (ccmMsg->callRelease && AT_CallMeterWarning())
	{
		sprintf( (char*)atRes, "+CCWV");
		AT_OutputUnsolicitedStr( atRes );
	}

	return RESULT_OK;
}

Result_t AtHandleCallAOCStatus(InterTaskMsg_t *inMsg){

	//TBD

	return RESULT_OK;
}


Result_t AtHandleCallTimer(InterTaskMsg_t *inMsg){


	CallTimerID_t*	timerID;
	UInt8			callIndex;
	Result_t		res = RESULT_OK;

	timerID = (CallTimerID_t*)inMsg->dataBuf;

	//Special handle for DTMF_SIGNAL_TIMER
	if ( *timerID >= DTMF_SIGNAL_TIMER ){

		callIndex = *timerID - DTMF_SIGNAL_TIMER;
		*timerID = DTMF_SIGNAL_TIMER;
	}


	switch (*timerID){

		case INCOMING_RING_TIMER:

			IncomingCallRing();
			break;

		case OUTGOING_CALL_TIMER:

			EndOutgoingCall();
			break;

		case DTMF_SIGNAL_TIMER:

			StopGenDTMF(callIndex);
			break;

		default:
			break;
	}


	return res;

}


Result_t AtHandleSTKCCSetupFail(InterTaskMsg_t *inMsg){

	//Handle the fail from the SIM Stk call control
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	UInt8 chan;

	StkCallSetupFail_t* failRes;

	failRes = (StkCallSetupFail_t*) inMsg->dataBuf;

	chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);

	//Clean up the AT call due to the fail
	if ( chan != INVALID_MPX_CHAN ){

		sprintf( (char*)atRes, "*MSTKCC: %d, %d, \"%s\"", SATK_CC_RESULT_FAILED,
								failRes->failRes, AtMapSTKCCFailIdToText(failRes->failRes));

		_TRACE(("STK AT: Call Setup Failed during CC by SIM:"));
		_TRACE(( (char*)atRes ));

		//Close the ATD
		if (chan == AT_GetCmdRspMpxChan(AT_CMD_D)){

			AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
		}


		if (V24_GetOperationMode((DLC_t)chan) == V24OPERMODE_NO_RECEIVE)
					V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );
	}

	return RESULT_OK;
}

////////////////////////AT command handles////////////////////////////

//At command handler AT_CMD_ECAM/AT_CMD_MCAM
AT_Status_t ATCmd_CAM_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1 ){


	 UInt8 atRes[MAX_RESPONSE_LEN+1];
	 atRes[0] = '\0';
	 switch ( accessType ) {

		 case AT_ACCESS_REGULAR:

				if (atoi((char*)_P1)){
					isMonitorCalls = TRUE;
				} else {
					isMonitorCalls = FALSE;
				}

				break;

		 case AT_ACCESS_READ:

				sprintf( (char*)atRes, "%s: %d", (char*)AT_GetCmdName(cmdId), (UInt8) isMonitorCalls );
				AT_OutputStr(chan, atRes);

				break;

		default:
				break;
	}

	AT_CmdRspOK(chan) ;
	return AT_STATUS_DONE;

}

//******************************************************************************
// Function Name:	ATCmd_A_Handler
//
// Description:		This function is called once the user enters ATA command in
//					order to respond to the alert/ring of and incoming call.
//******************************************************************************
AT_Status_t ATCmd_A_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const UInt8* _P1		//	anything following ATA
													//	shows up here and should
													//	be ignored
							) {

	AT_Status_t	 atStatus;

	if (incomingCallIndex == INVALID_CALL ) {

		AT_CmdRspError( chan,AT_CME_ERR_OP_NOT_ALLOWED );

		atStatus = AT_STATUS_DONE;

	} else {


		V24_SetOperationMode((DLC_t)chan, V24OPERMODE_NO_RECEIVE );

		AtProcessIncomingCall(chan,incomingCallIndex);

		atStatus = AT_STATUS_PENDING;
	}

	return atStatus;
}

//At command handler ATH
AT_Status_t ATCmd_H_Handler (	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t		atStatus = AT_STATUS_DONE;
    UInt8			curCallIndex = INVALID_CALL;
	UInt8			curCallChan = INVALID_MPX_CHAN;
	CCallType_t		callType;
	UInt8			apiClientId;
	CCallState_t	callState;

	if( chan == PPP_GetDlc() ){
		//When PDP-DEACTIVATION in progress, treat ATH in data channel as hanging up PPP only.
		if ( MS_IsGprsCallActive(chan) ) {
			if (PPP_connect) {
				SendPPPCloseReq(PPP_cid, NULL);
			}
			MS_SetChanGprsCallActive(chan, FALSE);
		}
		AT_CmdRspOK( chan );
		return atStatus;
	}

	//if there is data call on this channel, release the data call other than the current call such as waiting call
	if ((curCallIndex = CC_GetDataCallIndex()) != INVALID_CALL &&
		(curCallChan = AT_GetMpxChanFromV24ClientID(CC_GetCallClientID(curCallIndex))) == chan){

		atStatus = AtProceessEndCallCmd(curCallIndex);

	} else {
		curCallIndex = CC_GetCurrentCallIndex();

		if (curCallIndex == INVALID_CALL)
		{
			curCallIndex = CC_GetNextHeldCallIndex();
		}

		if (curCallIndex != INVALID_CALL)
		{
			callType = CC_GetCallType(curCallIndex);
			apiClientId = CC_GetCallClientID(curCallIndex);
			callState = CC_GetCallState(curCallIndex);

			if ((((callState == CC_CALL_ALERTING) || (callState == CC_CALL_WAITING))	&&
				(callType == MTVOICE_CALL)			||
				(callType == MTDATA_CALL)			||
				(callType == MTVIDEO_CALL)			||
				(callType == MTFAX_CALL))				||
				((apiClientId == CLIENT_ID_V24_0)	||
				(apiClientId == CLIENT_ID_V24_1)	||
				(apiClientId == CLIENT_ID_V24_2)	||
				(apiClientId == CLIENT_ID_V24_3)))
			{
				AtProceessEndCallCmd(curCallIndex);
				curCallChan = AT_GetMpxChanFromV24ClientID(CC_GetCallClientID(curCallIndex));
			}
		}
		//End of Cabrio specific change
	}

	if( atStatus == AT_STATUS_DONE ||
		(curCallIndex != INVALID_CALL &&
		 curCallChan != INVALID_MPX_CHAN &&
		 curCallChan != chan) ){

		AT_CmdRspOK( chan );

		//return the DONE while do RspOK. Otherwise will lock the port
		//due to the inconsitence between CmdRsp and AT status
		return(AT_STATUS_DONE);
	}

	return(atStatus);
}


//At command handler ATO
AT_Status_t ATCmd_O_Handler (	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1){

	AT_Status_t	 atStatus = AT_STATUS_DONE;
	UInt8 callIndex;

	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg(chan);

	UInt8 Val1 = _P1 == NULL ? 0 : atoi( (char*)_P1 );

	callIndex = CC_GetCurrentCallIndex();

	_TRACE(("ATCmd_O_Handler: chan,index = ", (0xFF0000 | (chan << 8) |  callIndex) ));

	if (!IsDataCallActive() ||
		chan !=  AT_GetMpxChanFromV24ClientID(CC_GetCallClientID(callIndex))){

			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
	} else {

		if ( accessType == AT_ACCESS_REGULAR ){
	 		channelCfg->O = Val1;

			AT_CmdRsp(chan, AT_CONNECT_RSP );

			SERIAL_SetAttachedDLC( chan );
			serial_set_v24_flag( V24_DATA ); // Start Data request
			V24_SetOperationMode((DLC_t)chan,V24OPERMODE_DATA);
			V24_SetDataTxMode((DLC_t)chan,FALSE);
			channelCfg->AllowUnsolOutput = FALSE;


		}
		else if ( accessType == AT_ACCESS_READ ){
			AT_CmdRspError(chan,AT_CME_ERR_OP_NOT_ALLOWED);
		}

	}

	return atStatus;
}

//At command handler ATZ
AT_Status_t ATCmd_Z_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType){

   AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

   AT_Status_t atStatus = AT_STATUS_DONE;

   //Result_t res = CC_EndAllCalls();
   CC_EndAllCalls();

   if (chan == (UInt8)PPP_GetDlc() && PPP_connect ) {
	   SendPPPCloseReq(PDP_GetPPPModemCid(), NULL);
   }

   AT_SetDefaultParms( FALSE );
   CC_InitCallCfg(&curCallCfg);
   FAX_InitializeFaxParam();

   /* Reset echo mode */
   V24_SetEchoMode((DLC_t) chan, (Boolean) channelCfg->E );

   AT_CmdRspOK(chan);

   return atStatus;
}

//At command handler ATS0
AT_Status_t ATCmd_S0_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1){

	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if ( accessType == AT_ACCESS_REGULAR )
    {
       channelCfg->S0 = _P1 == NULL ? 0 : atoi( (char*)_P1 );
    }
    else if ( accessType == AT_ACCESS_READ )
    {
       sprintf( (char*)atRes, "%03d", channelCfg->S0 );
       AT_OutputStr( chan ,atRes );
    }

	AT_CmdRspOK( chan );
	return AT_STATUS_DONE;
}

//At command handler ATS3
AT_Status_t ATCmd_S3_Handler (
	AT_CmdId_t cmdId,
	UInt8 chan,
	UInt8 accessType,
	const UInt8* _P1)
{
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if( AT_ACCESS_REGULAR == accessType ) {
		channelCfg->S3 = _P1 ? atoi( (char*)_P1 ) : 13 ;
	}
	else if( AT_ACCESS_READ == accessType ) {
		sprintf( (char*)atRes, "%03d", channelCfg->S3 ) ;
		AT_OutputStr( chan, atRes ) ;
	}

	AT_CmdRspOK ( chan ) ;
	return AT_STATUS_DONE ;
}

//At command handler ATS4
AT_Status_t ATCmd_S4_Handler (
	AT_CmdId_t cmdId,
	UInt8 chan,
	UInt8 accessType,
	const UInt8* _P1)
{
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if( AT_ACCESS_REGULAR == accessType ) {
		channelCfg->S4 = _P1 ? atoi( (char*)_P1 ) : 10 ;
	}
	else if( AT_ACCESS_READ == accessType ) {
		sprintf( (char*)atRes, "%03d", channelCfg->S4 ) ;
		AT_OutputStr( chan, atRes ) ;
	}

	AT_CmdRspOK ( chan ) ;
	return AT_STATUS_DONE ;
}

//At command handler ATS5
AT_Status_t ATCmd_S5_Handler (
	AT_CmdId_t cmdId,
	UInt8 chan,
	UInt8 accessType,
	const UInt8* _P1)
{
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if( AT_ACCESS_REGULAR == accessType ) {
		channelCfg->S5 = _P1 ? atoi( (char*)_P1 ) : 8 ;
	}
	else if( AT_ACCESS_READ == accessType ) {
		sprintf( (char*)atRes, "%03d", channelCfg->S5 ) ;
		AT_OutputStr( chan, atRes ) ;
	}

	AT_CmdRspOK ( chan ) ;
	return AT_STATUS_DONE ;
}

//At command handler ATS6
AT_Status_t ATCmd_S6_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1){

	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if ( accessType == AT_ACCESS_REGULAR )
    {
       channelCfg->S6 = _P1 == NULL ? 2 : atoi( (char*)_P1 );
    }
    else if ( accessType == AT_ACCESS_READ )
    {
       sprintf( (char*)atRes, "%03d", channelCfg->S6 );
       AT_OutputStr( chan ,atRes );
    }

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;

}

//At command handler ATS7
AT_Status_t ATCmd_S7_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1){

	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if ( accessType == AT_ACCESS_REGULAR )
    {
       channelCfg->S7 = _P1 == NULL ? DEF_S7 : atoi( (char*)_P1 );
    }
    else if ( accessType == AT_ACCESS_READ )
    {
       sprintf( (char*)atRes, "%03d", channelCfg->S7 );
       AT_OutputStr( chan ,atRes );
    }

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;

}

//At command handler ATS8
AT_Status_t ATCmd_S8_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1){

	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if ( accessType == AT_ACCESS_REGULAR )
    {
       channelCfg->S8 = _P1 == NULL ? 2 : atoi( (char*)_P1 );
    }
    else if ( accessType == AT_ACCESS_READ )
    {
       sprintf( (char*)atRes, "%03d", channelCfg->S8 );
       AT_OutputStr( chan ,atRes );
    }

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;
}


//At command handler ATS10
AT_Status_t ATCmd_S10_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1){

	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if ( accessType == AT_ACCESS_REGULAR )
    {
       channelCfg->S10 = _P1 == NULL ? 10 : atoi( (char*)_P1 );
    }
    else if ( accessType == AT_ACCESS_READ )
    {
       sprintf( (char*)atRes, "%03d", channelCfg->S10 );
       AT_OutputStr( chan ,atRes );
    }

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;
}


//At command handler ATL
AT_Status_t ATCmd_L_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1){

  UInt8 atRes[MAX_RESPONSE_LEN+1];
  AT_Status_t	atStatus = AT_STATUS_DONE;
  UInt8			volumn;

  if ( accessType == AT_ACCESS_REGULAR )
  {
          volumn = atoi((char*) _P1 );

          if( volumn <= 8 )
          {
                  AUDIO_SetSpeakerVol ( volumn );
				  curCallCfg.L = volumn;

                  AT_CmdRspOK( chan );
          }
          else
          {
				  atStatus = AT_STATUS_DONE;
                  // volumn larger than 8 is not supported
                  AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
          }
  }
  else if ( accessType == AT_ACCESS_READ )
  {
          curCallCfg.L = SPEAKER_GetVolume();
          sprintf( (char*) atRes, "%s: %d", "ATL", curCallCfg.L );
          AT_OutputStr( chan,atRes );

          AT_CmdRspOK( chan );
  }
  else
  {
          AT_CmdRspOK( chan );
  }


  return atStatus;

}



//At command handler ATM
AT_Status_t ATCmd_M_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1){


	UInt8 atRes[MAX_RESPONSE_LEN+1];

	if ( accessType == AT_ACCESS_REGULAR )
    {
       curCallCfg.M = _P1 == NULL ? 0 : atoi( (char*)_P1 );
    }
    else if ( accessType == AT_ACCESS_READ )
    {
       sprintf( (char*)atRes, "%03d", curCallCfg.M );
       AT_OutputStr( chan ,atRes );
    }

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;
}


//At command handler ATP
AT_Status_t ATCmd_P_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1){

	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);


	if ( accessType == AT_ACCESS_REGULAR )
    {
       channelCfg->P = _P1 == NULL ? 0 : atoi( (char*)_P1 );
    }
    else if ( accessType == AT_ACCESS_READ )
    {
       sprintf( (char*)atRes, "%03d", channelCfg->P );
       AT_OutputStr( chan ,atRes );
    }

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;
}


//At command handler ATX
AT_Status_t ATCmd_X_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1){


   	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_Status_t atStatus = AT_STATUS_DONE;
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

   	if ( accessType == AT_ACCESS_REGULAR ){

		   UInt8 x = _P1 == NULL ? 0 : atoi( (char*)_P1 );
		   if (x <= 4 && x > 0){
				channelCfg->X = x;
				AT_CmdRspOK( chan );
		   }else {
				AT_CmdRspError(chan,AT_CME_ERR_OP_NOT_SUPPORTED);
			}

	}
	else if ( accessType == AT_ACCESS_READ )
	{
           sprintf((char*)atRes, "%d", channelCfg->X);
           AT_OutputStr( chan, atRes);
		   AT_CmdRspOK( chan );
	}

	return atStatus;
}

//AT command handler AT+CSTA
AT_Status_t ATCmd_CSTA_Handler(AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1){


   	UInt8 atRes[MAX_RESPONSE_LEN+1];

   	if ( accessType == AT_ACCESS_REGULAR ){

           curDefaultCSTA = _P1 == NULL ? UNKNOWN_TON_ISDN_NPI : atoi( (char*)_P1 );
	}
	else if ( accessType == AT_ACCESS_READ )
	{
           sprintf((char*)atRes, "%s: %d", (char*)AT_GetCmdName(cmdId), curDefaultCSTA);
           AT_OutputStr( chan, atRes);
	}

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;
}


//At command handler AT+CHUP
AT_Status_t ATCmd_CHUP_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType){

	AT_Status_t	atStatus = AT_STATUS_DONE;
	Boolean		hasCallInChan = FALSE;
	UInt8		i;

	//Need to check all the calls to see if
	//there is a call in this channel
	for (i = 0;i<MAX_CALLS_NO;i++){
		if (chan ==
			AT_GetMpxChanFromV24ClientID(CC_GetCallClientID(i))){

			hasCallInChan = TRUE;
			break;
		}
	}

	switch(accessType) {

		case AT_ACCESS_READ:
			AT_CmdRspError( chan,AT_CME_ERR_OP_NOT_ALLOWED );
			return atStatus;

		case AT_ACCESS_REGULAR:

			if (CC_EndAllCalls() == CC_END_CALL_SUCCESS){
				atStatus = AT_STATUS_PENDING;
			}
			curDefaultCSTA = UNKNOWN_TON_ISDN_NPI;
			break;
		default:
			break;
   }


	if( atStatus == AT_STATUS_DONE || !hasCallInChan){

		AT_CmdRspOK( chan );

		//return the DONE while do RspOK. Otherwise will lock the port
		//due to the inconsitence between CmdRsp and AT status
		return AT_STATUS_DONE;
	}

	return atStatus;
}

//At command handler AT+CPAS
AT_Status_t ATCmd_CPAS_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType)
{

   UInt8 atRes[MAX_RESPONSE_LEN+1];
   UInt8 totalCallNum;

   CC_GetAllCallIndex(NULL,&totalCallNum);

   // TBD : When can we process this cmd and answer not ready ???
   if(CC_IsThereAlertingCall())
   {
   		sprintf( (char*)atRes, "%s: %d", (char*)AT_GetCmdName(cmdId), 3 );
   }
   else if( totalCallNum > 0 )
   {
   		sprintf( (char*)atRes, "%s: %d", (char*)AT_GetCmdName(cmdId), 4 );
   }
   else
   {
   		sprintf( (char*)atRes, "%s: %d", (char*)AT_GetCmdName(cmdId), 0 );
   }

   AT_OutputStr( chan, atRes );

   AT_CmdRspOK( chan );

   return AT_STATUS_DONE;
}

#if !defined(STACK_wedge)
//At command handler AT+CHSC
//HSCSD current call parameters
AT_Status_t ATCmd_CHSC_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType){



	 UInt8		atRes[MAX_RESPONSE_LEN+1];
	 UInt8		callIndex;

	 atRes[0] = '\0';
	 callIndex = CC_GetCurrentCallIndex();

	 switch ( accessType ) {

		 case AT_ACCESS_REGULAR:

			if( callIndex != INVALID_CALL && IsDataCall(callIndex) &&
				CC_GetCallState(callIndex)  == CC_CALL_ACTIVE)
			{
				if( curCallCfg.hscsd_call )
				{
					sprintf( (char*)atRes, "%s: %d,%d,%d,%d", (char*)AT_GetCmdName(cmdId), curCallCfg.rx,
						curCallCfg.tx, curCallCfg.aiur, curCallCfg.curr_coding );
				}
				else
				{
					sprintf( (char*)atRes, "+CHSC: 1,1,1,4" );
				}
			}
			else
			{
				sprintf( (char*)atRes, "+CHSC: 0,0,0,0" );
			}

			AT_OutputStr( chan,atRes );

			break;

		 default:

			break;
	 }

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;

}

//At command handler AT+CHSD
AT_Status_t ATCmd_CHSD_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType){

   UInt8 atRes[MAX_RESPONSE_LEN+1];

   if ( accessType == AT_ACCESS_REGULAR )
   {

	  sprintf( (char*)atRes, "%s: %d,%d,%d,%d,%d", (char*)AT_GetCmdName(cmdId),
					curCallCfg.mclass,curCallCfg.MaxRx,curCallCfg.MaxTx,
					curCallCfg.Sum,curCallCfg.codings );

      AT_OutputStr( chan, atRes );

   }

   AT_CmdRspOK( chan );

   return AT_STATUS_DONE;

}

//At command handler AT+CHST
//HSCSD transparent call configuration
AT_Status_t ATCmd_CHST_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,const UInt8* _P2)
{

	UInt8 atRes[MAX_RESPONSE_LEN+1];

	if ( accessType == AT_ACCESS_REGULAR  )
	{


      curCallCfg.wRx = _P1 == NULL ? 0 : atoi( (char*)_P1 );
      curCallCfg.codings = _P2 == NULL ? 0 : atoi( (char*)_P2 );

	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%s: %d,%d",(char*)AT_GetCmdName(cmdId),
						curCallCfg.wRx,curCallCfg.codings );

		AT_OutputStr( chan,atRes );
	}

   AT_CmdRspOK( chan );

   return AT_STATUS_DONE;

}

//At command handler AT+CHSN
AT_Status_t ATCmd_CHSN_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,const UInt8* _P2,
								const UInt8* _P3,const UInt8* _P4 ){

	UInt8		atRes[MAX_RESPONSE_LEN+1];
	UInt16		Val[4];
	AT_Status_t	atStatus = AT_STATUS_DONE;

    switch (accessType){

		case AT_ACCESS_REGULAR:

			Val[ 0 ] = _P1 == NULL ? 0 : atoi( (char*)_P1 );
			Val[ 1 ] = _P2 == NULL ? 0 : atoi( (char*)_P2 );
			Val[ 2 ] = _P3 == NULL ? 0 : atoi( (char*)_P3 );
			Val[ 3 ] = _P4 == NULL ? 12 : atoi( (char*)_P4 );

			if( ( Val[0] == 1 && Val[3] == 8 ) || // Illegal combination of 9600 wAiur and 14400 Codings.
				( Val[0] == 2 && Val[3] == 4 ) ){ // Illegal combination of 14400 wAiur and 9600 Codings.

					AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );

			}else
			{

					curCallCfg.wAiur = Val[ 0 ];
					curCallCfg.wRx = Val[ 1 ];
					curCallCfg.topRx = Val[ 2 ];
					curCallCfg.codings = Val[ 3 ];
					if( !curCallCfg.codings  ) curCallCfg.codings = 12;
			}

			// modify the current data call with the new parameters.
			if( curCallCfg.topRx  && IsDataCallActive() )
			{
				MNATDSParmModifyReq_t modify_params;
				memset ( &modify_params, 0, sizeof( MNATDSParmModifyReq_t ));

				modify_params.service_mode				= SINGLE_DATA;
				modify_params.hscsd_call_config.waiur	= curCallCfg.wAiur;
				modify_params.hscsd_call_config.wrx		= curCallCfg.wRx;
				modify_params.hscsd_call_config.toprx	= curCallCfg.topRx;
				modify_params.hscsd_call_config.codings	= curCallCfg.codings;

				MN_AlterHSCSDCall( &modify_params );

				//It is still a problem if the network does not respond to the
				//Upgrade / Downgrade request in new AT

				atStatus = AT_STATUS_PENDING;

			}
			else
			{
				AT_CmdRspOK( chan );
			}

			break;

		case AT_ACCESS_READ:

				sprintf( (char*)atRes, "%s: %d,%d,%d,%d",(char*)AT_GetCmdName(cmdId),
						curCallCfg.wAiur,curCallCfg.wRx,
						curCallCfg.topRx,curCallCfg.codings );

				AT_OutputStr( chan, atRes );
				AT_CmdRspOK( chan );

				break;

		default:

				break;
	}

	return atStatus;
}

//At command handler AT+CHSR
//HSCSD parameters report
AT_Status_t ATCmd_CHSR_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1){


	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if ( accessType == AT_ACCESS_REGULAR )
	{
		channelCfg->CHSR = _P1 == NULL ? 0 : atoi( (char*)_P1 );

	}
	else if ( accessType == AT_ACCESS_READ )
	{
			sprintf( (char*) atRes, "%s: %d",(char*)AT_GetCmdName(cmdId),channelCfg->CHSR );
			AT_OutputStr( chan, atRes );
	}

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;
}
#endif // #if !defined(STACK_wedge)

//At command handler AT+ES
AT_Status_t ATCmd_ES_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,const UInt8* _P2,
								const UInt8* _P3){

   UInt8 atRes[MAX_RESPONSE_LEN+1];

   if ( accessType == AT_ACCESS_REGULAR )
   {
      curCallCfg.ES0 = _P1 == NULL ? 1 : atoi( (char*)_P1 );
      curCallCfg.ES1 = _P2 == NULL ? 0 : atoi( (char*)_P2 );
      curCallCfg.ES2 = _P3 == NULL ? 1 : atoi( (char*)_P3 );
   }
   else if ( accessType == AT_ACCESS_READ )
   {

      sprintf( (char*)atRes, "%s: %d,%d,%d", (char*)AT_GetCmdName(cmdId),
                    curCallCfg.ES0, curCallCfg.ES1,
                    curCallCfg.ES2);

      AT_OutputStr( chan, atRes );
   }

   AT_CmdRspOK( chan );

   return AT_STATUS_DONE;
}

//At command handler AT+DS
AT_Status_t ATCmd_DS_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,const UInt8* _P2,
								const UInt8* _P3,const UInt8* _P4){

   UInt8 atRes[MAX_RESPONSE_LEN+1];

   if ( accessType == AT_ACCESS_REGULAR )
   {
      curCallCfg.ds_req_datacomp.direction = _P1 == NULL ?
												0 : atoi( (char*)_P1 );
      curCallCfg.ds_req_datacomp.max_dict = _P3 == NULL ?
												512 : atoi( (char*)_P3 );
	  curCallCfg.ds_req_datacomp.max_string = _P4 == NULL ?
												32 : atoi( (char*)_P4 );

	  curCallCfg.ds_req_success_neg = _P2 == NULL ? 0 : atoi( (char*)_P2 );

	  if( curCallCfg.ds_req_success_neg )
	  {
		 curCallCfg.CRLP[0].ver = 1;	// V1 required for compression
	  }
	  else
	  {
		 curCallCfg.CRLP[0].ver = 0;	// V0 sufficient for non-compressed
	  }

   }
   else if ( accessType == AT_ACCESS_READ )
   {

      sprintf( (char*)atRes, "%s: %d,%d,%d,%d", (char*)AT_GetCmdName(cmdId),
                    curCallCfg.ds_req_datacomp.direction, curCallCfg.ds_req_success_neg,
                    curCallCfg.ds_req_datacomp.max_dict, curCallCfg.ds_req_datacomp.max_string);

      AT_OutputStr( chan, atRes );
   }

   AT_CmdRspOK( chan );

   return AT_STATUS_DONE;
}


//FIXME Temp move CHLD and CLCC to GC75 for testing
//#ifdef GCXX_PC_CARD


//At command handler AT+CHLD
AT_Status_t ATCmd_CHLD_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1)
{
	InterTaskMsg_t*      msg;

	if ( accessType == AT_ACCESS_READ ){
		AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );
		return AT_STATUS_DONE;
	}

	//MSG_AT_CHLD_PROCESS
    msg = AllocInterTaskMsgFromHeap(MSG_AT_HANDLE_CHLD_CMD,
					(_P1 == NULL ? 1 : (strlen((const char*)_P1)+1)));

	if (_P1 != NULL)
		strcpy(msg->dataBuf,(const char*) _P1);
	else
		*(UInt8*)msg->dataBuf = 0;

	PostMsgToATCTask(msg);

	return  AT_STATUS_PENDING;
}



//At command handler AT+CLCC
AT_Status_t ATCmd_CLCC_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType){

	UInt8 atRes[MAX_RESPONSE_LEN+1];

	CCallType_t		callTy;
	CCallState_t	callSt;
	UInt8			callNum[MAX_DIGITS+2];///< (Note: 1 byte for null termination and the other for int code '+')
	CCallIndexList_t allCallIdx;
	UInt8			 i, idx, callDir, callMode,clccState, totalCalls;

	if ( accessType == AT_ACCESS_READ || accessType == AT_ACCESS_REGULAR  )
	{
			CC_GetAllCallIndex(&allCallIdx,&totalCalls);

			for (i = 0; i < totalCalls; i++){

				idx = allCallIdx[i];
				//skip call without alloced call index
				if (idx == VALID_CALL) {
					continue;
				}

				callTy = CC_GetCallType(idx);
				callSt = CC_GetCallState(idx);
				CC_GetCallNumber(idx,callNum);

				switch (callTy){

					case MTVOICE_CALL:
						callMode = 0;
						callDir  = 1;
						break;

					case MTDATA_CALL:
						callMode = 1;
						callDir  = 1;
						break;

					case MTFAX_CALL:
						callMode = 2;
						callDir  = 1;
						break;

					case MOVOICE_CALL:

						callMode = 0;
						callDir  = 0;
						break;

					case MODATA_CALL:

						callMode = 1;
						callDir  = 0;
						break;

					case MOFAX_CALL:
						callMode = 2;
						callDir  = 0;
						break;

					default:
						//Unknow dir and mode
						callDir  = 9;
						callMode = 9;
						break;
				}

				switch (callSt){

					case CC_CALL_ACTIVE:

						clccState = 0;
						break;

					case CC_CALL_HOLD:

						clccState = 1;
						break;

					case CC_CALL_CALLING:

						clccState = 2;
						break;

					case CC_CALL_ALERTING:
					case CC_CALL_CONNECTED:

						if (callTy == MOVOICE_CALL || callTy == MODATA_CALL ||
						    callTy == MOFAX_CALL || callTy == MOVIDEO_CALL )

							clccState = 3;
						else
							clccState = 4;

						break;

					case CC_CALL_WAITING:

						clccState = 5;
						break;

					default:
						//Unknow state
						clccState = 9;
						break;
				}

				sprintf((char*) atRes, "%s: %d,%d,%d,%d,%d,\"%s\"\n",
						(char*)AT_GetCmdName(cmdId), idx+1, callDir,clccState,
						callMode, CC_IsMultiPartyCall(idx), callNum);

				AT_OutputStr( chan, atRes );
			}
	}


	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;
}

//At command handler AT+VTD
AT_Status_t ATCmd_VTD_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1){

	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	AT_CommonCfg_t*  commonCfg = AT_GetCommonCfg();

	switch(accessType)
	{
		case AT_ACCESS_REGULAR:
			commonCfg->VTD = atoi((char*)_P1);
			break;

		case AT_ACCESS_READ:
			sprintf( (char*)rspBuf, "%s: %d", (char*)AT_GetCmdName(AT_CMD_VTD), commonCfg->VTD);
			AT_OutputStr( chan, rspBuf );
			break;

		case AT_ACCESS_TEST:
		default:
			break;

	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//At command handler AT*MVTS
AT_Status_t ATCmd_MVTS_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,	// DTMF
								const UInt8* _P2,	// DTMF on/off
								const UInt8* _P3)	// DTMF is silent: 0 - silent, 1 - not silent (default)
{
	return AT_Handle_VTS_Command(cmdId, chan, accessType, _P1, NULL, _P3, (Boolean)atoi((char*)_P2));
}

//At command handler AT+VTS
AT_Status_t ATCmd_VTS_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,	// DTMF
								const UInt8* _P2,	// DTMF duration
								const UInt8* _P3){	// is DTMF silent

	return AT_Handle_VTS_Command(cmdId, chan, accessType, _P1, _P2, _P3, FALSE);
}

AT_Status_t AT_Handle_VTS_Command (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,
								const UInt8* _P2,
								const UInt8* _P3,
								Boolean mode){

	Result_t		res;
	AT_Status_t		atStatus = AT_STATUS_DONE;
	UInt8			callIndex;
	CCallState_t	callState;
	CCallType_t     callType;
	UInt8			rspBuf[ AT_RSPBUF_SM ] ;

	char		dtmfDigital;
	Ticks_t		duration;
	Boolean		isSilentDTMF = FALSE;

	AT_CommonCfg_t*  commonCfg = AT_GetCommonCfg();

	if ( accessType == AT_ACCESS_REGULAR  ){

		// only allow single digit for DTMF
		if(strlen((char*)_P1) > 1){
			//AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );
			AT_ImmedCmdRspError(chan,cmdId,AT_CME_ERR_OP_NOT_ALLOWED);
			atStatus = AT_STATUS_DONE;
			return AT_STATUS_DONE;
		}

		dtmfDigital = (char) _P1[ 0 ];

		duration = _P2 == NULL
           ? commonCfg->VTD == 0 ? (Ticks_t)MANUFACTURE_SPECIFIC_DURATION : (Ticks_t)(commonCfg->VTD*MANUFACTURE_SPECIFIC_DURATION)
           : atoi( (char*)_P2 ) == 0 ? (Ticks_t)MANUFACTURE_SPECIFIC_DURATION : (Ticks_t)(atoi( (char*)_P2 )*10);

		if(cmdId != AT_CMD_VTS){
			duration = 0;
		}

		_TRACE(("ATCmd_VTS_Handler: DTMF = ", dtmfDigital));
		_TRACE(("ATCmd_VTS_Handler: duration = ", duration));

		// check if user want the DTMF to be silent
		if( (_P3 != NULL) && (atoi( (char*)_P3) == 0) ){
			isSilentDTMF = TRUE;
		}

		callIndex = CC_GetCurrentCallIndex();
		callState = CC_GetCallState(callIndex);
		callType  = CC_GetCallType(callIndex);

		//If there is MT call coming, check if there is MO call
		//We do DTMF for that MO call. We have to put fuzzy logic here
		//due to the limit of VTS command(without call index)
		if (callType == MTVOICE_CALL && callState !=  CC_CALL_ACTIVE) {

				callIndex = CC_GetNextActiveCallIndex();

		}

		_TRACE(("ATCmd_VTS_Handler: Index, State = ",(0xFF0000 | (callIndex << 8) |  callState) ));

		if( (cmdId == AT_CMD_MVTS) && (mode == FALSE) ){

			res = CC_StopDTMF(callIndex);

			if(res == CC_STOP_DTMF_FAIL){
				//AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );
				AT_ImmedCmdRspError(chan,cmdId,AT_CME_ERR_OP_NOT_ALLOWED);
			}
			else{
				//AT_CmdRspOK(chan);
				AT_ImmedCmdRsp( chan,cmdId,AT_OK_RSP  );
			}
			return AT_STATUS_DONE;
		}

		// Start DTMF local tone timer, so that the time spend executing CC_SendDTMF() is properly counted.
		if(cmdId == AT_CMD_VTS){
			dtmfTimer[callIndex] = StartCallTimer((TimerID_t)(DTMF_SIGNAL_TIMER+callIndex),duration);
			atStatus = AT_STATUS_PENDING;
		}

		res = CC_SendDTMF(dtmfDigital,callIndex,isSilentDTMF);
		_TRACE(("CC_SendDTMF returned! "));

		if (res == CC_SEND_DTMF_FAIL){

			_TRACE(("CC_SEND_DTMF_FAIL: callIndex = 0x",(UInt8)callIndex));

			CC_StopDTMF(callIndex);

			StopCallTimer(&dtmfTimer[callIndex]);

			//AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );
			AT_ImmedCmdRspError(chan,cmdId,AT_CME_ERR_OP_NOT_ALLOWED);
			atStatus = AT_STATUS_DONE;
		} else {

			if(cmdId == AT_CMD_VTS){
				if(dtmfTimer[callIndex] == NULL){  // timer for local tone has already expired.
					_TRACE(("DTMF Local timer already released" ));

					// because the START_DTMF_CNF status was not back yet when the timer expired,
					// the DTMF will not be stopped, do it again here.
					CC_StopDTMF(callIndex);

					atStatus = AT_STATUS_DONE;
				}
				else{
					_TRACE(("DTMF Local timer not expired yet" ));
				}
			}
			else{
				// send back "OK" right away so we can enter *MVTS to stop the DTMF.
				// callctrl has block to prevent another DTMF from being entered before the
				// earlier one is stopped.
				//AT_CmdRspOK( chan );
				AT_ImmedCmdRsp( chan,cmdId,AT_OK_RSP  );
				atStatus = AT_STATUS_DONE;
			}
		}
	}
	else if(accessType == AT_ACCESS_TEST){

		if(cmdId == AT_CMD_VTS){
			sprintf((char*)rspBuf, "+VTS: (0-9, \"*\",\"#\"), (0-500)");
		}
		else{
			sprintf((char*)rspBuf, "*MVTS:  (0-9, \"*\",\"#\"), (0,1), (0,1)");
		}

		AT_OutputStr (chan, rspBuf );
		//AT_CmdRspOK(chan);
		AT_ImmedCmdRsp( chan,cmdId ,AT_OK_RSP );
	}
	else {

		//AT_CmdRspOK(chan);
		AT_ImmedCmdRsp( chan,cmdId ,AT_OK_RSP );
	}

	return atStatus;
}

//At command handler AT*MKeytony
AT_Status_t ATCmd_MKEYTONE_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,
								const UInt8* _P2){

	AT_Status_t	atStatus = AT_STATUS_DONE;

	UInt8		i;
	char		dtmfDigit;
	Ticks_t		duration;
	UInt8 rspBuf[ AT_RSPBUF_SM ] ;

	AT_CommonCfg_t*  commonCfg = AT_GetCommonCfg();

	if ( accessType == AT_ACCESS_REGULAR  ){

		// only allow single digit for Keytone
		if(strlen((char*)_P1) > 1){
			AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}

		dtmfDigit = (char) _P1[ 0 ];

		duration = _P2 == NULL
           ? commonCfg->VTD == 0 ? (Ticks_t)MANUFACTURE_SPECIFIC_DURATION : (Ticks_t)(commonCfg->VTD*MANUFACTURE_SPECIFIC_DURATION)
           : atoi( (char*)_P2 ) == 0 ? (Ticks_t)MANUFACTURE_SPECIFIC_DURATION : (Ticks_t)(atoi( (char*)_P2)*10);

		_TRACE(("ATCmd_MKEYTONE_Handler: DTMF = ", dtmfDigit));
		_TRACE(("ATCmd_MKEYTONE_Handler: duration = ", duration));

		for ( i = 0; i < MAX_DTMF_NUM && (DTMFActions[i].dtmfDigit != dtmfDigit); i++ );

		if ( i >= MAX_DTMF_NUM  )
		{
			// Not a valid DTMF char
			AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}
		else {

			AUDIO_PlayTone( AUDIO_CHNL_ANY, DTMFActions[i].dtmfAction - CALLACTION_START_DTMF_0, duration);

			atStatus = AT_STATUS_DONE;
		}
	}
	else if(accessType == AT_ACCESS_TEST){
		sprintf((char*)rspBuf, "*MKEYTONE: (0-9, \"*\",\"#\"), (0-500)");
		AT_OutputStr (chan, rspBuf );
	}

	AT_CmdRspOK(chan);
	return atStatus;
}

//#endif

//At command handler AT+MSTTY
AT_Status_t ATCmd_MSTTY_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1){

	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_Status_t	atStatus = AT_STATUS_DONE;

	if ( accessType == AT_ACCESS_REGULAR  ){

		CC_SetTTYCall(atoi( (char*)_P1 ));


	}  else if( accessType == AT_ACCESS_READ ){

        sprintf( (char*)atRes, "%s: %d", (char*)AT_GetCmdName(cmdId),CC_IsTTYEnable());
		AT_OutputStr( chan, atRes );
	}

	AT_CmdRspOK( chan );
	return atStatus;
}

//At command handler AT+MSTPTN- STOP TONE when there is tone play
AT_Status_t ATCmd_MSTPTN_Handler (	AT_CmdId_t cmdId,
									UInt8 chan,
									UInt8 accessType)
{

	AT_Status_t	atStatus = AT_STATUS_DONE;
	UInt8 rspBuf[ AT_RSPBUF_SM ] ;

	if ( accessType == AT_ACCESS_REGULAR  ){

	 AUDIO_StopPlaytone() ;

	}  else if( accessType == AT_ACCESS_READ ){

        sprintf( (char*)rspBuf, "%s", (char*)"*MSTPTN" );
		AT_OutputStr( chan, rspBuf );
	}

	AT_CmdRspOK( chan );
	return atStatus;


}


//At command handler AT+CEER
AT_Status_t ATCmd_CEER_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType){


	UInt8 atRes[MAX_RESPONSE_LEN+1];
	AT_Status_t  atStatus = AT_STATUS_DONE;
	char		*ptrReport;

	if ( accessType == AT_ACCESS_REGULAR ){

	switch ( curCallRelCause ){
		case	1 : ptrReport = "Number not assigned"; break;
		case	3 : ptrReport = "No route to destination"; break;
		case	6 : ptrReport = "Channel unacceptable"; break;
		case	8 : ptrReport = "Operator determined barring"; break;
		case	16 : ptrReport = "Normal call clearing"; break;
		case	17 : ptrReport = "User busy"; break;
		case	18 : ptrReport = "No user responding"; break;
		case	19 : ptrReport = "User alerting, no answer"; break;
		case	21 : ptrReport = "Call rejected"; break;
		case	22 : ptrReport = "Number changed"; break;
		case	27 : ptrReport = "Destination out of order"; break;
		case	28 : ptrReport = "Invalid or incomplete number"; break;
		case	29 : ptrReport = "Facility rejected"; break;
		case	31 : ptrReport = "Normal unspecified"; break;
		case	34 : ptrReport = "No circuit or channel available"; break;
		case	38 : ptrReport = "Network out of order"; break;
		case	41 : ptrReport = "Temporary failure"; break;
		case	42 : ptrReport = "Switching equipment congestion"; break;
		case	43 : ptrReport = "Access information discarded"; break;
		case	44 : ptrReport = "Requested circuit/channel not available"; break;
		case	47 : ptrReport = "Resource not available, unspecified"; break;
		case	49 : ptrReport = "Quality of service unavailable"; break;
		case	50 : ptrReport = "Requested facility not subscribed"; break;
		case	55 : ptrReport = "Incoming calls barred within CUG"; break;
		case	57 : ptrReport = "Bearer capability not authorized"; break;
		case	58 : ptrReport = "Bearer capability not available"; break;
		case	63 : ptrReport = "Service or option not available"; break;
		case	65 : ptrReport = "Bearer service not implemented"; break;
		case	68 : ptrReport = "ACM greater or equal than ACMmax"; break;
		case	79 : ptrReport = "Service or option not implemented"; break;
		case	87 : ptrReport = "User not member of CUG"; break;
		case	88 : ptrReport = "Incompatible destination"; break;
		case	127: ptrReport = "Missing or unknown APN"; break;
		case 	128: ptrReport = "Unknown PDP address"; break;
		case 	130: ptrReport = "Acitvation rejected by GGSN"; break;
		case 	131: ptrReport = "Activation rejected, unspecified"; break;
		case 	132: ptrReport = "Service option not supported"; break;
		case 	133: ptrReport = "Requested service option not subscribed"; break;
		case 	134: ptrReport = "Service option temporarily out of order"; break;
		case 	135: ptrReport = "NSAPI already used"; break;
		case 	136: ptrReport = "Regular deactivation"; break;
		case 	137: ptrReport = "QOS not accepted"; break;
		case 	138: ptrReport = "PDP network failure"; break;
		case 	139: ptrReport = "Reactivation required"; break;
		case 	149: ptrReport = "PDP authentication failure"; break;
		case	MNCAUSE_RADIO_LINK_FAILURE_APPEARED: ptrReport = "Radio link failure"; break;

		default: ptrReport = curCallRelCause >= 256 ? "Protocol error" : "Unknown"; break;
	}

	sprintf( (char*)atRes, "%s: %s", (char*)AT_GetCmdName(cmdId), ptrReport );
	AT_OutputStr( chan, atRes );
	AT_CmdRspOK(chan);

	} else {

		AT_CmdRspError(chan,AT_CME_ERR_OP_NOT_ALLOWED);
		atStatus = AT_STATUS_DONE;
	}

   return atStatus;
}

//At command handler AT+ETBM
AT_Status_t ATCmd_ETBM_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,
								const UInt8* _P2,
								const UInt8* _P3){

	UInt8 atRes[MAX_RESPONSE_LEN+1];

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if ( accessType == AT_ACCESS_REGULAR  ){

		channelCfg->ETBM[0] = (_P1 == NULL) ? 0 : atoi((char*)_P1);
		channelCfg->ETBM[1] = (_P2 == NULL) ? 0 : atoi((char*)_P2);
		channelCfg->ETBM[2] = (_P3 == NULL) ? 20 : atoi((char*)_P3);
	}
	else if ( accessType == AT_ACCESS_READ ){


		sprintf( (char*)atRes, "%s: %d,%d,%d", (char*)AT_GetCmdName(cmdId),
            channelCfg->ETBM[0], channelCfg->ETBM[1], channelCfg->ETBM[2] );

		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}


//At command handler AT+CBST
AT_Status_t ATCmd_CBST_Handler(AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,
								const UInt8* _P2,
								const UInt8* _P3){

	UInt8 atRes[MAX_RESPONSE_LEN+1];

	if ( accessType == AT_ACCESS_REGULAR ){

		curCallCfg.CBST.speed = _P1 == NULL ? 7 : atoi( (char*)_P1 );
		curCallCfg.CBST.name  = _P2 == NULL ? 0 : atoi( (char*)_P2 );
		curCallCfg.CBST.ce    = _P3 == NULL ? 1 : atoi( (char*)_P3 );
	}
	else if (accessType == AT_ACCESS_READ ){

		sprintf( (char*)atRes, "%s: %d,%d,%d", (char*)AT_GetCmdName(cmdId),
                  curCallCfg.CBST.speed, curCallCfg.CBST.name,
                  curCallCfg.CBST.ce );

		AT_OutputStr( chan, atRes );
	}

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;
}


//At command handler AT+CRLP
AT_Status_t ATCmd_CRLP_Handler(AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,
								const UInt8* _P2,
								const UInt8* _P3,
								const UInt8* _P4,
								const UInt8* _P5,
								const UInt8* _P6){

		UInt8 atRes[MAX_RESPONSE_LEN+1];

		if ( accessType == AT_ACCESS_REGULAR ){

				UInt8 ver,index;

				ver = _P5 == NULL ?  0 : atoi( (char*)_P5 );   // rlp version number
				index = ver == 2 ? 1: 0;

				curCallCfg.CRLP[ index ].iws = _P1 == NULL ? 61 : atoi((char*)_P1);   // iws
				curCallCfg.CRLP[ index ].mws = _P2 == NULL ? 61 : atoi((char*)_P2);   // mws
				curCallCfg.CRLP[ index ].t1  = _P3 == NULL ? 48 : atoi((char*)_P3);   // t1
				curCallCfg.CRLP[ index ].n2  = _P4 == NULL ?  6 : atoi((char*)_P4);   // n2
				// The t2 value can't be set by this command,
				// just use the default value
				curCallCfg.CRLP[ index ].t4 = _P6 == NULL ?  0 : atoi((char*)_P6);	  // t4

				if( ver !=2 ){
					curCallCfg.CRLP[0].ver = ver;
				}

		} else if ( accessType == AT_ACCESS_READ ){


					//Print the rlp parameters for both rlp version
					sprintf((char*)atRes, "%s: %d,%d,%d,%d,%d,%d\n\r%s: %d,%d,%d,%d,%d,%d",
						(char*)AT_GetCmdName(cmdId),curCallCfg.CRLP[ 0 ].iws, curCallCfg.CRLP[ 0 ].mws,
						curCallCfg.CRLP[ 0 ].t1, curCallCfg.CRLP[ 0].n2,
						/*verwsion*/curCallCfg.CRLP[0].ver, curCallCfg.CRLP[ 0 ].t4,
						(char*)AT_GetCmdName(cmdId),
						curCallCfg.CRLP[ 1 ].iws, curCallCfg.CRLP[ 1 ].mws,
						curCallCfg.CRLP[ 1 ].t1, curCallCfg.CRLP[ 1].n2, /*verwsion*/2,
						curCallCfg.CRLP[ 1 ].t4);

					AT_OutputStr( chan, atRes );
				}

		AT_CmdRspOK(chan);

		return AT_STATUS_DONE;

}


//At command handler AT+CR
AT_Status_t ATCmd_CR_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1){

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);
	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,
								   FALSE,0,&channelCfg->CR);

}

//At command handler AT+CRC
AT_Status_t ATCmd_CRC_Handler(AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1){

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);
	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,
								   FALSE,0,&channelCfg->CRC);

}

//At command handler AT+ER
AT_Status_t ATCmd_ER_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1){

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);
	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,
								   FALSE,0,&channelCfg->ER);

}

//At command handler AT+DR
AT_Status_t ATCmd_DR_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1){

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);
	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,
								   FALSE,0,&channelCfg->DR);

}


//At command handler AT+ILRR
AT_Status_t ATCmd_ILRR_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1){

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);
	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,
								   FALSE,0,&channelCfg->ILRR);

}

//At command handler AT&D
AT_Status_t ATCmd_AND_D_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1)
{

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	/*
	** ------------------------------
	**  Set circuit 108/2 behaviour
	** ------------------------------
	*/
	return AT_SimpleCmdHandler(cmdId, chan, accessType, _P1,
								   FALSE, 0, &channelCfg->AmpD);

}

//At command handler AT&C
AT_Status_t ATCmd_AND_C_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1){

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	/*
	** ------------------------------
	**  Set circuit 109 behaviour
	** ------------------------------
	*/
	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,
								   FALSE,0,&channelCfg->AmpC);

}

//At command handler AT+IFC
AT_Status_t ATCmd_IFC_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,
								const UInt8* _P2){

	 UInt8 atRes[MAX_RESPONSE_LEN+1];
     AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	 /*
      ** ------------------------------------
      **  Regular command  AT+IFC=...
      ** ------------------------------------
      */

      if ( accessType == AT_ACCESS_REGULAR)
      {
         UInt8   Val1, Val2;
         Val1 = _P1 == NULL ? 2 : atoi( (char*)_P1 );
         Val2 = _P2 == NULL ? 2 : atoi( (char*)_P2 );

         channelCfg->IFC[ 0 ] = Val1;
         channelCfg->IFC[ 1 ] = Val2;

         V24_SetDTEFlowCntrlType( chan, Val1 == 0 ? V24FLOWCNTRLTYPE_NONE :
                                        Val1 == 1 ? V24FLOWCNTRLTYPE_SW :
                                                    V24FLOWCNTRLTYPE_HW );

         V24_SetDCEFlowCntrlType( chan, Val2 == 0 ? V24FLOWCNTRLTYPE_NONE :
                                        Val2 == 1 ? V24FLOWCNTRLTYPE_SW :
                                                    V24FLOWCNTRLTYPE_HW,
   													V24FLOWCNTRLPRIORITY_NORMAL );
      }
      else if ( accessType == AT_ACCESS_READ )
      {
         sprintf( (char*) atRes, "%s: %d,%d", (char*)AT_GetCmdName(cmdId),
                       channelCfg->IFC[ 0 ], channelCfg->IFC[ 1 ] );

         AT_OutputStr(chan, atRes);
      }

	  AT_CmdRspOK( chan );

	  return AT_STATUS_DONE;
}

/////////////////////////////////////////////////////////////////////////


Result_t AtHandleCallAbort(InterTaskMsg_t *inMsg){


	UInt8			chan;
	UInt8			callIndex;
	Result_t		res = RESULT_OK;
	chan = *(UInt8*)inMsg->dataBuf;

   	if (curCallCfg.faxParam.fclass == AT_FCLASSSERVICEMODE_DATA)
		V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );
	else V24_SetOperationMode((DLC_t) chan, V24OPERMODE_FAX_CMD );

	//For voice and csd calls
	callIndex = CC_GetCurrentCallIndex();

 	if (callIndex != INVALID_CALL)
	{
		AtProceessEndCallCmd(callIndex);
	} else {

		AT_CmdRspOK(chan);
	}

	return res;
}



Result_t AtHandleEscDataCall(InterTaskMsg_t *inMsg){

	UInt8				chan;
	UInt8				callIndex;
	AT_ChannelCfg_t*	atcCfg;

	chan = *(UInt8*)inMsg->dataBuf;

	AT_CmdRspOK(chan);

	if (curCallCfg.faxParam.fclass == AT_FCLASSSERVICEMODE_DATA)
		V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );
	else V24_SetOperationMode((DLC_t) chan, V24OPERMODE_FAX_CMD );

	//Reset the ATC log mpx channel
	if (chan == V24_GetLogDlc()){
		V24_SetDataTxMode(chan, TRUE );
		return RESULT_OK;
	}

	//For voice and csd calls
	callIndex = CC_GetCurrentCallIndex();

	_TRACE(("AtHandleEscDataCall: chan,index = ", (0xFF0000 | (chan << 8) |  callIndex) ));

 	if (callIndex != INVALID_CALL &&
		chan == AT_GetMpxChanFromV24ClientID(CC_GetCallClientID(callIndex))){

		if( IsDataCall(callIndex) && CC_GetCallState(callIndex) == CC_CALL_ACTIVE){

			//For active data call, switch to
			//AT command mode for accepting the commands.
			serial_set_v24_flag( V24_AT_CMD );
			V24_SetDataTxMode((DLC_t) chan, TRUE );

			atcCfg = AT_GetChannelCfg( chan );
			atcCfg->AllowUnsolOutput = TRUE;
		}
	}

	if (MS_IsGprsCallActive(chan)) {

		//FixMe Ben:11-19-2002 has to Close PPP if active
		if ( PPP_GetDlc() == chan) {
			serial_set_v24_flag( V24_AT_CMD );
			V24_SetDataTxMode((DLC_t) chan, TRUE );
		}
	}

	return RESULT_OK;

}


static AT_Status_t AtProceessEndCallCmd(UInt8 callIndex){

	UInt8	callChan;

	Result_t	res;
	AT_Status_t	atStatus = AT_STATUS_DONE ;

	callChan = AT_GetMpxChanFromV24ClientID(CC_GetCallClientID(callIndex));

	res = CC_EndCall(callIndex);

	if( res == CC_END_CALL_SUCCESS  )
	{
	   if( IsDataCall(callIndex) )
	   {
			if ( callChan != INVALID_MPX_CHAN)
		  		V24_SetDataTxMode((DLC_t) callChan, TRUE );
	   }


	   curDefaultCSTA = UNKNOWN_TON_ISDN_NPI;

	   atStatus = AT_STATUS_PENDING;

	} else {

	    atStatus = AT_STATUS_DONE;
	}

   return atStatus;
}

static Result_t AtProcessVoiceCallCmd(UInt8 chan, CallParams_VCSD_t* inVcsdPtr)
{
	Result_t	res;

	if (inVcsdPtr->number[0] == INTERNATIONAL_CODE){
		strncpy((char*)curCallPhoneNum.callNum,
				(char*)&inVcsdPtr->number[1],MAX_DIGITS);
		curCallPhoneNum.callType = INTERNA_TON_ISDN_NPI;
		curDefaultCSTA = INTERNA_TON_ISDN_NPI;
	} else {
		strncpy((char*)curCallPhoneNum.callNum,
				(char*)inVcsdPtr->number,MAX_DIGITS);
		curCallPhoneNum.callType = UNKNOWN_TON_ISDN_NPI;
		curDefaultCSTA = UNKNOWN_TON_ISDN_NPI;
	}

	inVcsdPtr->voiceCallParam.cug_info.cug_index = CUGINDEX_NONE; //Shall be fixed (ABDI)
	inVcsdPtr->voiceCallParam.auxiliarySpeech = SIM_GetAlsDefaultLine() == 1 ? TRUE : FALSE;
	inVcsdPtr->voiceCallParam.isEmergency = PBK_IsEmergencyCallNumber((char *) inVcsdPtr->number);
	inVcsdPtr->voiceCallParam.isFdnChkSkipped = FALSE;
	inVcsdPtr->voiceCallParam.subAddr = defaultSubAddress;

	res = CC_MakeVoiceCall(	AT_GetV24ClientIDFromMpxChan(chan),
							inVcsdPtr->number,
							inVcsdPtr->voiceCallParam);

	/* Set V24_NO_RECEIVE mode so that the command can be aborted */
	if( res == CC_MAKE_CALL_SUCCESS &&
		  V24_GetOperationMode((DLC_t)chan) == V24OPERMODE_AT_CMD ){
				 V24_SetOperationMode((DLC_t)chan, V24OPERMODE_NO_RECEIVE );
	}

	return res;
}


//Process the CSD data call command
static Result_t AtProcessDataCallCmd (UInt8 chan, UInt8* phoneNum ){

	Result_t			 res = RESULT_ERROR;
	AT_ChannelCfg_t* atcCfg = AT_GetChannelCfg( chan );


	if (phoneNum[0] == INTERNATIONAL_CODE){
			strncpy((char*)curCallPhoneNum.callNum,
					(char*)&phoneNum[1],MAX_DIGITS);
			curCallPhoneNum.callType = INTERNA_TON_ISDN_NPI;
			curDefaultCSTA = INTERNA_TON_ISDN_NPI;
	} else {
			strncpy((char*)curCallPhoneNum.callNum,
					(char*)&phoneNum[0],MAX_DIGITS);
			curCallPhoneNum.callType = UNKNOWN_TON_ISDN_NPI;
			curDefaultCSTA = UNKNOWN_TON_ISDN_NPI;
	}

	if (curCallCfg.faxParam.fclass != AT_FCLASSSERVICEMODE_DATA){
			//Force the dafault CBST for FAX
			curCallCfg.CBST.speed	= 7;
			curCallCfg.CBST.name	= 1;
			curCallCfg.CBST.ce		= 0;
			curCallCfg.curr_ce		= 0;

			//ComposeDataCallParam(&curCallCfg,&curCallPhoneNum,&dataCallParam.dataCallReqParam);
			res = CC_MakeFaxCall(AT_GetV24ClientIDFromMpxChan(chan),phoneNum);

	} else {

			//ComposeDataCallParam(&curCallCfg,&curCallPhoneNum,&dataCallParam.dataCallReqParam);
			res = CC_MakeDataCall(AT_GetV24ClientIDFromMpxChan(chan),phoneNum);
			curCallCfg.curr_ce = curCallCfg.CBST.ce;
	}

	/* Set V24_NO_RECEIVE mode so that the command can be aborted */
	if( res == CC_MAKE_CALL_SUCCESS &&
		  ((V24_GetOperationMode((DLC_t)chan) == V24OPERMODE_AT_CMD) ||
		   (V24_GetOperationMode((DLC_t)chan) == V24OPERMODE_FAX_CMD)) ){

		 //Not allow the unsolicited event now for the data call
		 atcCfg->AllowUnsolOutput = FALSE;
		 V24_SetOperationMode((DLC_t)chan, V24OPERMODE_NO_RECEIVE );

	}

	return res;
}

static AT_Status_t AtProcessIncomingCall(UInt8 chan, UInt8 callIndex){

	CCallType_t				callTy;
	AT_ChannelCfg_t* atcCfg = AT_GetChannelCfg( chan );

	callTy = CC_GetCallType(callIndex);

	if( (callTy == MTDATA_CALL ) || (callTy == MTFAX_CALL))
	{
			//Not allow the unsolicited event now for the data call
		    atcCfg->AllowUnsolOutput = FALSE;
			//ComposeDataCallRespParam(&curCallCfg,&dataCallResp.dataCallRespParam);
			CC_AcceptDataCall(AT_GetV24ClientIDFromMpxChan(chan),callIndex);

	}
	else if( callTy == MTVIDEO_CALL )
	{
		CC_AcceptVideoCall( AT_GetV24ClientIDFromMpxChan(chan),callIndex );
	}
	else
	{
		CC_AcceptVoiceCall(AT_GetV24ClientIDFromMpxChan(chan),callIndex);
	}

	ResetIncomingCallRing(callIndex);

	return AT_STATUS_DONE;
}


Result_t AtProcessChldCmd(InterTaskMsg_t* inMsg){

	UInt8           callIndex, waitIndex,heldIndex, chan, *_P1;
	Result_t		res;
	UInt8           len;
	int				x;

	res = RESULT_ERROR;
	_P1 = (UInt8*) inMsg->dataBuf;
	chan = AT_GetCmdRspMpxChan(AT_CMD_CHLD);

	if ( _P1[0] == 0 )
	{

		_TRACE(( "NULL SEND" ));
      // ------------------------------------------------------
      //  Place active call on hold or retrieve a held call
      // ------------------------------------------------------
      if ( (callIndex = CC_GetNextActiveCallIndex()) != INVALID_CALL )
      {
		 _TRACE(( "Put active call index on hold-1 ", callIndex ));

		 if (CC_GetNumofHeldCalls() >0 ){
			res = CC_SwapCall(CC_GetNextHeldCallIndex());
		 }else {
			res = CC_HoldCall(callIndex);
		 }

		 if (res == CC_SWAP_CALL_SUCCESS || CC_HOLD_CALL_SUCCESS)
				res = RESULT_OK;

      }
      else if ( (callIndex = CC_GetNextHeldCallIndex()) != INVALID_CALL )
      {
         _TRACE(( "Retrieve held call index", callIndex ));


		 res = CC_SwapCall(callIndex);

		 if ( res == CC_SWAP_CALL_SUCCESS )
			 res = RESULT_OK;

      } else {

			 res = RESULT_ERROR;
	  }

	} else {//_P1 != NULL

		len = strlen( (char*)_P1 );
		res = RESULT_ERROR;

		switch ( _P1[ 0 ] ){

			case '0' :
				// ------------------------------------------------------
				//  Release Held calls or set UDUB for call waiting
				// ------------------------------------------------------
				_TRACE(( "0 SEND\n" ));
	            if ( (callIndex = CC_GetNextWaitCallIndex()) != INVALID_CALL ){
					_TRACE(( "Release call index", callIndex ));
					CC_EndCall(callIndex);
					res = RESULT_OK;
				}
				else
				{
					while ( (callIndex = CC_GetNextHeldCallIndex()) != INVALID_CALL ){
						/* Loop through all the multiparty calls that are held and
						 * disconnect each of them.
						 */
							_TRACE(( "Release call ", callIndex ));
							CC_EndCall(callIndex);

						   /* Put in some delays to wait for some memory to
						    * release as from MN_SetCallActionReq on, quite a bit
						    * of memory is dynamically allocated.
						    */
					       OSTASK_Sleep(TICKS_ONE_SECOND / 100);
						   res = RESULT_OK;
					}

				 }

			break;

			case '1' :
				if ( len == 1 ){

					UInt8 preIdx = INVALID_CALL;

					// ------------------------------------------------------
					//  Release Active calls and accept held or waiting
					// ------------------------------------------------------
					_TRACE(( "1 SEND ",1 ));

					waitIndex = CC_GetNextWaitCallIndex();
					heldIndex = CC_GetNextHeldCallIndex();

					/* Loop through all the active calls (including multiparty
					* calls that are active) and disconnect each of them.
					*/
					while ( (callIndex = CC_GetNextActiveCallIndex()) != INVALID_CALL &&
							 callIndex != preIdx ){

                       _TRACE(( "Release call - call index", callIndex ));
                       CC_EndCall(callIndex);

					   /* Put in some delays to wait for some memory to
					    * release as from MN_SetCallActionReq on, quite a bit
					    * of memory is dynamically allocated.
						*
						* MN takes much longer time to finally release the call,
						* but it updates the call status to the upper layer before
						* final clean-up which causes the following call operation in the
						* upper layer fail. Need to more investigation on MN layer
						* Now put longer sleep time to avoid the inconsistent between
						* MN status and upply layer status
					    */
				       	OSTASK_Sleep(TICKS_ONE_SECOND);

						//Avoid to release the same call twice due to the status update
						//difference between layers
						preIdx = callIndex;

					    res = RESULT_OK;
					}


					//Accept the waiting call
					if ( CC_AcceptVoiceCall(AT_GetV24ClientIDFromMpxChan(chan),waitIndex) == CC_ACCEPT_CALL_FAIL ){
						if (CC_RetrieveCall(heldIndex) == CC_RESUME_CALL_SUCCESS){
							_TRACE(( "Retrieve call " ));
							res = RESULT_OK;
						}

					} else {

						_TRACE(( "Accept waiting call " ));
						ResetIncomingCallRing(incomingCallIndex);

						res = RESULT_OK;
					}


				} else {//len > 1

						// ------------------------------------------------------
						//  Release Specific call X
						// ------------------------------------------------------
						//AT user use CLCC call index for the x in chld=1x
						//So the index should minus 1 to match the
						//callindex definition.

						*(char*)_P1 = '0';
						x = atoi( (char*)_P1);

						_TRACE(( "1x SEND x=", x ));

						if ( x < 1){
								res = RESULT_ERROR;
						} else {

							callIndex = x - 1;

							if (CC_GetCallState(callIndex) == CC_CALL_ACTIVE){

								CC_EndCall(callIndex);
								res = RESULT_OK;
							}
						}
				 }

			break;

			case '2' :

				if ( len == 1 ){

				// ------------------------------------------------------
				//  Place Active calls on hold and accept held or waiting
				// ------------------------------------------------------

					_TRACE(( "2 SEND",2 ));

					waitIndex = CC_GetNextWaitCallIndex();

					if (CC_GetNextActiveCallIndex() != INVALID_CALL ||
						waitIndex == INVALID_CALL)
					{

						//If there is active call, swap with a held call first
						//or no waiting call, swap a held call anyway

						if (CC_SwapCall(CC_GetMPTYCallIndex()) == CC_SWAP_CALL_SUCCESS)
						{
								res = RESULT_OK;
								_TRACE(( "Swap-2 multi-party held call on hold-2", 1 ));

						} else
							if (CC_SwapCall(CC_GetNextHeldCallIndex()) == CC_SWAP_CALL_SUCCESS)
							{

								res = RESULT_OK;
								_TRACE(( "Swap-2 calls",2));
							} else
								if (waitIndex == INVALID_CALL){
								//not waiting call and held call, only hold active call
									if ( CC_HoldCall(CC_GetMPTYCallIndex()) == CC_HOLD_CALL_SUCCESS )
									{
										res = RESULT_OK;
										_TRACE(( "Put multi-party active call on hold-2",3));

									} else
										if (CC_HoldCall(CC_GetNextActiveCallIndex()) == CC_HOLD_CALL_SUCCESS ){
											res = RESULT_OK;
											_TRACE(( "Put active call on hold-2",4));
										}
								}

					}

					if (res == RESULT_ERROR) {

						//No active call, accept the waiting call first,
						//or No held call, accept the waiting call

						if (CC_AcceptWaitingCall(AT_GetV24ClientIDFromMpxChan(chan)) == CC_ACCEPT_CALL_SUCCESS ){

							ResetIncomingCallRing(incomingCallIndex);

							res = RESULT_OK;
							_TRACE(( "Accept waiting call index",waitIndex ));

						}
					}

				}else { //len > 1

					// ------------------------------------------------------
					//  Place Active calls on hold except call X
					// ------------------------------------------------------

					*(char*)_P1 = '0';
					x = atoi( (char*)_P1);
					_TRACE(( "2x SEND x=", x));
					if ( x < 0 ) {
						res = RESULT_ERROR;
					} else {
						callIndex = x - 1;
						if ( CC_SplitCall(callIndex) == CC_SPLIT_CALL_SUCCESS ){
							res = RESULT_OK;
						}
					}
			  }

			break;

			case '3' :

				callIndex = CC_GetNextActiveCallIndex();

				// ------------------------------------------------------
				//  Always add an non multi-party active call to the conversation
				// ------------------------------------------------------
				if ( len == 1 ){
						if ( CC_JoinCall(callIndex) == CC_JOIN_CALL_SUCCESS ){

							res = RESULT_OK;
           					_TRACE(( "Build MPTY call", 3));
						}
				}

			break;

			case '4' :

				// ------------------------------------------------------
				//  Request Explicit Call Transfer
				// ------------------------------------------------------
				if ( len == 1 &&/*If there is active call, ECT it*/
						CC_TransferCall(CC_GetNextActiveCallIndex()) == CC_TRANS_CALL_SUCCESS ){

							res = RESULT_OK;
           					_TRACE(( "ECT call transfer", 4));
				} else if (len == 1 &&/*Otherwise, if there is a held call, ECT it*/
							CC_TransferCall(CC_GetNextHeldCallIndex()) == CC_TRANS_CALL_SUCCESS ){

							res = RESULT_OK;
           					_TRACE(( "ECT call transfer", 4));

				}

			break;
		}
	}

	if (res == RESULT_ERROR) AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );

	return res;

}


//-----------------------------------------------------------------------------
//	Command handler for ATD.
//-----------------------------------------------------------------------------
AT_Status_t ATCmd_D_Handler (
	AT_CmdId_t			cmdId,
	UInt8				chan,
	UInt8				accessType,
	const				UInt8* dialStr )
{
	AT_Status_t status;

	// Skip leading whitespace. Also check for ATDT & ATDP (tone & pulse dialing), both modifiers are ignored
	while( isspace(*dialStr) || (toupper(*dialStr) == 'T') || (toupper(*dialStr) == 'P') )
	{
		++dialStr;
	}

	if (!DIALSTR_IsValidString(dialStr))
	{
		AT_CmdRspError( chan, AT_ERROR_RSP ) ;
		status = AT_STATUS_DONE ;
	}
	else
	{
		status = ProcessCallReq(chan, dialStr);
	}

	return status;
}

//At command handler AT+CSSN
AT_Status_t ATCmd_CSSN_Handler(AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1,
							const		UInt8* _P2){

	UInt8 atRes[MAX_RESPONSE_LEN+1];

	if ( accessType == AT_ACCESS_REGULAR )
    {
		AT_GetCommonCfg()->CSSI = _P1 ? atoi( (char*)_P1 ) : 0 ;
		AT_GetCommonCfg()->CSSU = _P2 ? atoi( (char*)_P2 ) : 0 ;
    }
    else if ( accessType == AT_ACCESS_READ )
    {
		sprintf( (char*)atRes, "%s: %d, %d", AT_GetCmdName( AT_CMD_CSSN ),
                    AT_GetCommonCfg()->CSSI, AT_GetCommonCfg()->CSSU );
		AT_OutputStr( chan ,atRes );
    }

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;
}

//******************************************************************************
//
// Function Name:	AtHandleGPRSCallMonitorStatus
//
// Description:		Handle GPRS call monitor status event.
//
// Notes:
//
//******************************************************************************
void AtHandleGPRSCallMonitorStatus( InterTaskMsg_t *inMsg )
{
#ifdef PDA_SUPPORT
	UInt8			output[50];
	UInt8			*pData;
	UInt8			cid,type,status;

	if (isMonitorCalls) {
		pData = (UInt8 *)inMsg->dataBuf;

		cid = *pData++;
		type = *pData++;
		status = *pData;

		MSG_LOGV("ATC:AtHandleGPRSCallMonitorStatus(cid/status/type)=", (cid<<16|status<<8|type) );

		sprintf( (char*)output, "*MCAM: %d,%d,%d", cid, status, type);

		AT_OutputBlockableUnsolicitedStr( output );
	}
#endif

}


