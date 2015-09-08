/****************************************************************************
*   @file   atc.c
*
*   @brief  This file contains ATC task functions.
*
****************************************************************************/

#define ENABLE_LOGGING

#include "atc.h"
#include "at_api.h"
#include "at_call.h"
#include "at_cfg.h"
#include "at_data.h"
#include "at_netwk.h"
#include "at_pch.h"
#include "at_plmn.h"
#include "at_sim.h"
#include "at_ss.h"
#include "at_stk.h"
#include "at_util.h"
#include "at_tst.h"

#include "mti_trace.h"
#include "mti_build.h"
#include "types.h"
#include "mstypes.h"
#include "msconsts.h"
#include "sdltrace.h"
#include "proc_id.h"

#include "assert.h"
#include "xassert.h"


#include "mti_mn.h"
#include "sim_api.h"


#include "v24.h"
#include "nvram.h"
#include "mpx.h"
#include "rrmprim.h"
#include "phonebk.h"
#include "phonebk_api.h"
#include "omprim.h"
#include "microphone.h"
#include "speaker.h"

#include "ecdc.h"

#include "pchtypes.h"
#include "pchfuncs.h"
#include "mnatdstypes.h"
#include "serial.h"
#include "bandselect.h"
#include "memmap.h"

#include "stkapi.h"
#include "mti_mn.h"

#include "fax.h"

#include "pppmain.h"
#ifdef MULTI_PDP
#include "dataCommInt.h"  //DC_ReportCallStatus_t
#endif

#include "ostimer.h"
#include "timer.h"
#include "misc.h"


#include "log.h"
#include "taskmsgs.h"
#include "pchapi.h"

#include "mstruct.h"

#include "ms_database.h"


#include "serialapi.h"
#include "device_drv.h"
#include "devapi.h"

#include "dataacct.h"

#if CHIPVERSION >= CHIP_VERSION(BCM2132,00) /* BCM2132 and later */
#include "usb_drv_mep.h"
#endif // #if CHIPVERSION >= CHIP_VERSION(BCM2132,00) /* BCM2132 and later */

//Handle for AT
SerialHandle_t serialHandleforAT = -1;

//******************************************************************************
//              Prototypes
//******************************************************************************
void Soft_download( void ) ;			//	see dev/src/startup.s

static void ProcessATCMsg(InterTaskMsg_t* inMsg);

static void ATC_Task(void);


//-------- Globals -----------------
UInt8				atcClientID;

extern DLC_t PppDlc;

//******************************************************************************
//              Variables
//******************************************************************************
static 	Task_t 	atc_task;

Queue_t			atQueue;


//******************************************************************************
//
// Function Name:       ATC_Init
//
// Description:         Initialize the ATC
//
// Notes:
//
//******************************************************************************
void ATC_Init( void )
{

	//Set defaut AT serial device to the UARTA
	ATC_ConfigSerialDevForATC(UART_A);

	AT_InitState() ;

	MPX_Init( );

	V24_Init( );

	// new AT msg queue (pass by pointer for speed)
	atQueue = OSQUEUE_Create(QUEUESIZE_ATC, sizeof(InterTaskMsg_t*), OSSUSPEND_PRIORITY);

	/*
	 ** ---------------------------------------------
	 **  Initialize static and global variables here
	 ** -----------------------------------------------
	 */
	atc_Trace        = FALSE;

	// set default ATC parameters
	AT_SetDefaultParms( TRUE );

	// register to receive MS events
	atcClientID = SYS_RegisterForMSEvent(PostMsgToATCTask, 0);
	if(atcClientID == INVALID_CLIENT_ID){
		assert(0);
	}

}


//******************************************************************************
//
// Function Name:       ATC_Run
//
// Description:         Run the ATC
//
// Notes:
//
//******************************************************************************
void ATC_Run( void )
{
	atc_task = OSTASK_Create( ATC_Task, TASKNAME_ATC, TASKPRI_ATC, STACKSIZE_ATC );

	// Need to startup MPX and V24 so AT commands can be received via the serial port.
	MPX_Run();
	V24_Run();

	// Inform module that we are now ready to receive AT command.
	OSTASK_Sleep( TICKS_ONE_SECOND/100 );	// allow all V24_Run() job to finish.

	ATC_ReportMrdy(MODULE_GSM_GPRS_READY);
}


//******************************************************************************
//
// Function Name:       ATC_Register( )
//
// Description:         Register call-back functions.
//
// Notes:
//
//******************************************************************************

void ATC_Register( void )
{
   MN_Register();

   MPX_Register();

   V24_Register();

   PPP_Register(
		ATC_ReportPPPOpen,
		ATC_PutPPPCmdString,
		ATC_ReportPPPCloseReq
		);
}


/************************************************************************************
*
*	Function Name:	AtHandleSimDetectionInd
*
*	Description:	This function handles the SIM Insert/Removal detection status.
*
*
*************************************************************************************/
static void AtHandleSimDetectionInd(InterTaskMsg_t* inMsg)
{
	UInt8 rspBuf[AT_RSPBUF_SM];

	if( AT_GetCommonCfg()->ESIMC )	 // SIM presence indication
	{
		sprintf( (char*) rspBuf, "*ESIMM: %d", (*((Boolean *) inMsg->dataBuf) ? 1: 0));
		AT_OutputUnsolicitedStr( rspBuf );
	}

	if( *((Boolean *) inMsg->dataBuf) ){
		ATC_ReportMrdy(MODULE_SIM_INSERT_IND);
	}
	else{
		ATC_ReportMrdy(MODULE_SIM_REMOVE_IND);
	}
}


/************************************************************************************
*
*	Function Name:	AtHandleSimEfileInfoResp
*
*	Description:	This function handles Efile information returned in a Generic
*					SIM Accesss in the ATC task.
*
*
*************************************************************************************/
void AtHandleSimEfileInfoResp(InterTaskMsg_t* inMsg)
{
	switch( AT_GetCmdIdByMpxChan(AT_GetMpxChanFromV24ClientID(inMsg->clientID)) )
	{
	case AT_CMD_CPOL:
#ifdef GCXX_PC_CARD
	case AT_CMD_EPNR:
	case AT_CMD_EPNW:
#endif
		AtHandleEfileInfoForPlmn(inMsg);
		break;

	default:
		MSG_LOGV("ATC:MSG_SIM_EFILE_INFO_RSP (SMS access) inMsg = ",(UInt32)inMsg);
		break;
	}
}


/************************************************************************************
*
*	Function Name:	AtHandleSimEfileDataResp
*
*	Description:	This function handles Efile data contents returned in a Generic
*					SIM Accesss in the ATC task.
*
*
*************************************************************************************/
void AtHandleSimEfileDataResp(InterTaskMsg_t* inMsg)
{
	switch( AT_GetCmdIdByMpxChan(AT_GetMpxChanFromV24ClientID(inMsg->clientID)) )
	{
	case AT_CMD_CPOL:
#ifdef GCXX_PC_CARD
	case AT_CMD_EPNR:
#endif
		AtHandleEfileDataForPlmn(inMsg);
		break;

	default:
		MSG_LOGV("ATC:MSG_SIM_EFILE_DATA_RSP (VM Ind read) inMsg = ",(UInt32)inMsg);
		break;
	}
}


/************************************************************************************
*
*	Function Name:	AtHandleRxSignalChgInd
*
*	Description:	This function handles MSG_RX_SIGNAL_INFO_CHG_IND message received
*					in the ATC task.
*
*
*************************************************************************************/
void AtHandleRxSignalChgInd(InterTaskMsg_t* inMsg)
{
	AT_ChannelCfg_t* atc_config_ptr = AT_GetChannelCfg(0);	//	**FixMe** hard-coded chan
	char temp_buffer[30];

	if( ((RX_SIGNAL_INFO_CHG_t *) inMsg->dataBuf)->signal_qual_changed &&
		(atc_config_ptr->CMER[0] == 2) && (atc_config_ptr->CMER[3] == 1) )
	{
		UInt8 signal_qual; /* Signal quality: bit error rate parameter */

		SYS_GetRxSignalInfo(NULL, &signal_qual);

		if( (signal_qual > 7) || (signal_qual == RX_SIGNAL_INFO_UNKNOWN) )
		{
			signal_qual = 99; /* Unknown signal quality */
		}

		sprintf(temp_buffer, "+CIEV: %d,%d", atc_config_ptr->CMER[3], signal_qual);
		AT_OutputUnsolicitedStr((UInt8*)temp_buffer);
	}
}

/************************************************************************************
*
*	Function Name:	AtHandleRSSIInd
*
*	Description:	This function handles MSG_RSSI_IND message received
*					in the ATC task.
*
*
*************************************************************************************/
void AtHandleRSSIInd(InterTaskMsg_t* inMsg)
{
	AT_CommonCfg_t*  channelCfg = AT_GetCommonCfg();
	UInt8 rspBuf[ AT_RSPBUF_SM ];
	UInt8 rssi = 99, ber = 99;

	if( channelCfg->MCSQ == 1 )
	{
		if(SYS_GetSystemState() != SYSTEM_STATE_ON_NO_RF)
		{
			rssi = ((RxSignalInfo_t *) inMsg->dataBuf)->rssi;
			ber = ((RxSignalInfo_t *) inMsg->dataBuf)->qual;

			ConvertRxSignalInfo2Csq(&rssi, &ber);
		}

		sprintf((char*)rspBuf, "%s: %d,%d", AT_GetCmdName(AT_CMD_MCSQ), rssi, ber);
		AT_OutputUnsolicitedStr((UInt8*)rspBuf);
	}
}

//******************************************************************************
//
// Function Name: 	AtHandleDCReportCallStatus
//
// Description:		DC broadcasting: report data comm status
//
// Notes:
//
//******************************************************************************
void AtHandleDCReportCallStatus(InterTaskMsg_t* inMsg)
{
	DC_ReportCallStatus_t* callStatusMsg;

	callStatusMsg = (DC_ReportCallStatus_t*)inMsg->dataBuf;


#ifdef MULTI_PDP
	if (callStatusMsg->acctId == DATA_ACCOUNT_ID_FOR_ATC) {

		MSG_LOGV("ATC:AtHandleDCReportCallStatus acctId = ",callStatusMsg->acctId);
		MSG_LOGV("ATC:AtHandleDCReportCallStatus status = ",callStatusMsg->status);
		_TRACE(("AtHandleCallStatus:(DC broadcasting) rejectCode = ", callStatusMsg->rejectCode));
		_TRACE(("AtHandleCallStatus:(DC broadcasting) srcIP = ", callStatusMsg->srcIP));
		if(callStatusMsg->status == DC_STATUS_ERR_NO_CARRIER) {
			ATC_PutPPPCmdString( AT_NO_CARRIER_RSP, 0, PppDlc );
			ATC_ReportCmeError(PppDlc, callStatusMsg->rejectCode);
		}
		else if(callStatusMsg->status == DC_STATUS_CONNECTED) {
			MSG_LOG("DC: AtHandleDCReportCallStatus(): DC_STATUS_CONNECTED");
		}
		else if(callStatusMsg->status == DC_STATUS_DISCONNECTED) {
			MSG_LOG("DC: AtHandleDCReportCallStatus(): DC_STATUS_DISCONNECTED");
			ATC_ReportPPPCloseReq( PppDlc );
			PppDlc = INVALID_DLC;
		}
	}
	else {
		MSG_LOGV("ATC:AtHandleDCReportCallStatus ignored, acctId= ",callStatusMsg->acctId);
	}
#else
	MSG_LOGV("ATC:AtHandleDCReportCallStatus dcId = ",callStatusMsg->dcId);
	MSG_LOGV("ATC:AtHandleDCReportCallStatus status = ",callStatusMsg->status);
	if(callStatusMsg->status == DC_STATUS_ERR_NO_CARRIER)
		ATC_PutPPPCmdString( AT_NO_CARRIER_RSP, 0, PppDlc );
#endif

}

//******************************************************************************
// Function Name:       ATC_Task
//
// Description:         This is the entry point for the ATC task.
//
// Notes:
//
//******************************************************************************
static void ATC_Task( void )
{
   STATUS			qStatus;
   InterTaskMsg_t	*pMsg;


	// task never returns
	while( TRUE )
	{
		qStatus = OSQUEUE_Pend(atQueue, (QMsg_t *)&pMsg, TICKS_FOREVER);

		if(qStatus == OSSTATUS_SUCCESS){
			ProcessATCMsg(pMsg);		// process ATC msgs
			if(pMsg != NULL){
				FreeInterTaskMsg(pMsg);	// de-allocate msg
			}
		}
		pMsg = NULL;
	}  // end while(1)-loop

}



void ATC_ReportPPPOpen( DLC_t dlc )
{
//Integrated CL27615
//Remove the unnecessary serial function calls to fix the CS call cancellation disturbs the GPRS data transfer
//	SERIAL_SetAttachedDLC( dlc );
//	serial_set_v24_flag( V24_DATA ); //  Data Mode
	V24_SetOperationMode( dlc,V24OPERMODE_DATA );
	V24_SetDataTxMode(dlc, FALSE );

	_TRACE(( "ATC_ReportPPPOpen dlc=\n", dlc ));
}

void ATC_PutPPPCmdString( UInt32 stridx, int code,DLC_t dlc )
{
	if (stridx ==  AT_CME_ERR_RSP) {
		AT_CmdRsp( dlc, code ) ;
	}
	else {
		if (stridx ==  AT_CONNECT_RSP) {
			PPP_connect = TRUE;
		}

		AT_CmdRsp( dlc, stridx );
	}
}



void ATC_ReportPPPCloseReq( DLC_t dlc )
{
	//TEMP FIX, PPP should just use this callback function to post
	//an event to ATC. ATC will handle this racing condition
	short  atc_reportpppclose_cnt = 0;

	while (V24_GetTxBufferUsed(dlc) && atc_reportpppclose_cnt<50) {
		atc_reportpppclose_cnt++;
		OSTASK_Sleep(100);
	}
	ATC_SetContextDeactivation(dlc);
	V24_ResetTxBuffer(dlc);
	PPP_connect = FALSE;
    _TRACE(( "ATC_ReportPPPCloseReq dlc=\n", dlc ));
}


//******************************************************************************
//
// Function Name: 	ATC_ReportMrdy
//
// Description:		Report MRDY status
//
// Notes:
//
//******************************************************************************

void ATC_ReportMrdy( UInt8 status )
{
	InterTaskMsg_t*		msg;

	msg = AllocInterTaskMsgFromHeap(MSG_MS_READY_IND, sizeof(MODULE_READY_STATUS_t));

	*(MODULE_READY_STATUS_t*)msg->dataBuf = (MODULE_READY_STATUS_t)status;

	PostMsgToATCTask(msg);
}


//******************************************************************************
//
// Function Name:	AtHandlePowerDownCnf
//
// Description: This function handles the MSG_POWER_DOWN_CNF message received.
//
// Notes:
//
//******************************************************************************
static void AtHandlePowerDownCnf(InterTaskMsg_t *inMsg)
{
	AT_OutputUnsolicitedStr((UInt8*)"*MRDY: 0");
	AT_CmdRspOK( AT_GetCmdRspMpxChan(AT_CMD_CFUN)) ;
}



//----------------------------------------------------------------------------------
// PostMsgToATCTask():  send msg to ATC task.
//----------------------------------------------------------------------------------
void PostMsgToATCTask(InterTaskMsg_t* inMsg)
{
	OSStatus_t status = OSQUEUE_Post(atQueue, (QMsg_t *) &inMsg, TICKS_FOREVER);

	xassert(status == OSSTATUS_SUCCESS, status);
}


//----------------------------------------------------------------------------------
// ProcessATCMsg():  new ATC task message processor.
//----------------------------------------------------------------------------------
static void ProcessATCMsg(InterTaskMsg_t* inMsg)
{
	AT_CmdId_t cmdId;

	// new ATC msg processor
	switch(inMsg->msgType){
		//------- Process ATC Cmd/Request Msgs ----------------
		case MSG_MS_READY_IND:
			MSG_LOGV("ATC:MSG_MS_READY_IND status = ",*(MODULE_READY_STATUS_t*)inMsg->dataBuf);
			AtHandleMsReadyInd(inMsg);
			break;

		case MSG_RX_SIGNAL_INFO_CHG_IND:
			AtHandleRxSignalChgInd(inMsg);
			break;

		case MSG_RSSI_IND:
			AtHandleRSSIInd(inMsg);
			break;

		case MSG_AT_COPS_CMD:
			ATProcessCOPSCmd(inMsg);
			break;
		case MSG_AT_CGATT_CMD:
			ATProcessCGATTCmd(inMsg);
			break;

		case MSG_AT_CGACT_CMD:
			ATProcessCGACTCmd(inMsg);
			break;

		case MSG_AT_CME_ERROR:
			ATProcessCmeError(inMsg);
			break;

		case MSG_SIM_DETECTION_IND:
			AtHandleSimDetectionInd(inMsg);
			break;

		case MSG_SIM_CHANGE_CHV_RSP:
		case MSG_SIM_UNBLOCK_CHV_RSP:
		case MSG_SIM_VERIFY_CHV_RSP:
		case MSG_SIM_ENABLE_CHV_RSP:
		case MSG_SIM_SET_FDN_RSP:
		case MSG_SIM_PLMN_WRITE_RSP:
		case MSG_SIM_ACM_UPDATE_RSP:
		case MSG_SIM_ACM_MAX_UPDATE_RSP:
		case MSG_SIM_PUCT_UPDATE_RSP:
			AtHandleSimAccessStatusResp( (MsgType_t)inMsg->msgType,		// **FixMe** check for valid cast
				AT_GetMpxChanFromV24ClientID(inMsg->clientID), *((SIMAccess_t *) inMsg->dataBuf) );

			break;

		case MSG_SIM_EFILE_INFO_RSP:
			AtHandleSimEfileInfoResp(inMsg);
			MSG_LOGV("ATC:MSG_SIM_EFILE_INFO_RSP inMsg = ",(UInt32)inMsg);
			break;

		case MSG_SIM_SMS_SEARCH_STATUS_RSP:
		case MSG_SIM_SMS_PARAM_DATA_RSP:
				MSG_LOGV("ATC:MSG_SIM_SMS_PARAM_DATA_RSP inMsg = ",(UInt32)inMsg);
			break;

		case MSG_SIM_EFILE_DATA_RSP:
			AtHandleSimEfileDataResp(inMsg);
			break;

		case MSG_SIM_GENERIC_ACCESS_RSP:
			AtHandleSimGenericAccessResp(inMsg);
			break;

		case MSG_SIM_RESTRICTED_ACCESS_RSP:
			AtHandleSimRestrictedAccessResp(inMsg);
			break;

		case MSG_SIM_ACM_VALUE_RSP:
			AtHandleAcmValueResp(inMsg);
			break;

		case MSG_SIM_MAX_ACM_RSP:
			AtHandleMaxAcmValueResp(inMsg);
			break;

		case MSG_SIM_PUCT_DATA_RSP:
			AtHandlePuctDataResp(inMsg);
			break;

		case MSG_GET_PBK_INFO_RSP:
			AtHandlePbkInfoResp(inMsg);
			break;

		case MSG_PBK_ENTRY_DATA_RSP:
			AtHandlePbkEntryDataResp(inMsg);
			break;

		case MSG_WRT_PBK_ENTRY_RSP:
			AtHandleWritePbkResp(inMsg);
			break;

		case MSG_PBK_READY_IND:
			AtHandlePbkReadyInd();
			break;

		/* This indicates a phonebook entry has been updated. Ignore it since we do not need to do anything */
		case MSG_WRT_PBK_ENTRY_IND:
			break;

		case MSG_SIM_SVC_PROV_NAME_RSP:
			AtHandleEPROResp(inMsg);
			break;

		//------- Process ATC Response/Indication Msgs ---------
		case MSG_CIPHER_IND:
			AtHandleCipherIndication(inMsg);
			break;

		case MSG_PLMNLIST_IND:
			AtHandlePlmnListInd(inMsg);
			break;

		case MSG_PLMN_SELECT_CNF:
			AtHandlePlmnSelectCnf(inMsg);
			break;

		case MSG_REG_GSM_IND:
		case MSG_REG_GPRS_IND:
			AtHandleRegGsmGprsInd(inMsg);
			break;

		case MSG_GPRS_DEACTIVATE_IND:
			MSG_LOGV("ATC:MSG_GPRS_DEACTIVATE_IND inMsg = ",(UInt32)inMsg);
			AtHandleGPRSDeactivateInd(inMsg);
			break;

		case MSG_GPRS_ACTIVATE_IND:
			MSG_LOGV("ATC:MSG_GPRS_ACTIVATE_IND inMsg = ",(UInt32)inMsg);
			AtHandleGPRSActivateInd(inMsg);
			break;

		case MSG_GPRS_MODIFY_IND:
			AtHandleGPRSModifyInd(inMsg);
			break;

		case MSG_GPRS_REACT_IND:
			AtHandleGPRSReActInd(inMsg);
			break;

		case MSG_CALL_MONITOR_STATUS:
			AtHandleGPRSCallMonitorStatus(inMsg);
			break;

		case MSG_DATE_TIMEZONE_IND:
			MSG_LOGV("ATC:MSG_DATE_TIMEZONE_IND inMsg = ",(UInt32)inMsg);
			AtHandleTimeZoneInd(inMsg);
			break;

		case MSG_TIMEZONE_IND:
			MSG_LOGV("ATC:MSG_TIMEZONE_IND inMsg = ",(UInt32)inMsg);
			// AtHandleTimezoneInd(inMsg);
			break;

		case MSG_NETWORK_NAME_IND:
			MSG_LOGV("ATC:MSG_NETWORK_NAME_IND inMsg = ",(UInt32)inMsg);
			AtHandleNetworkNameInd(inMsg);
			break;

		// STK messages
		case MSG_SATK_EVENT_DISPLAY_TEXT:
			AT_HandleSTKDisplayTextInd(inMsg);
			break;

		case MSG_SATK_EVENT_IDLEMODE_TEXT:
			MSG_LOGV("ATC:MSG_SATK_EVENT_IDLEMODE_TEXT inMsg = ",(UInt32)inMsg);
			AT_HandleSTKIdleModeTextInd(inMsg);
			break;

		case MSG_SATK_EVENT_LAUNCH_BROWSER:
			AT_HandleSTKLaunchBrowserReq(inMsg);
			break;

		case MSG_SATK_EVENT_GET_INKEY:
			AT_HandleSTKGetInkeyInd(inMsg);
			break;

		case MSG_SATK_EVENT_GET_INPUT:
			AT_HandleSTKGetInputInd(inMsg);
			break;

		case MSG_SATK_EVENT_PLAY_TONE:
			AT_HandleSTKPlayToneInd(inMsg);
			break;

		case MSG_SATK_EVENT_SELECT_ITEM:
			AT_HandleSTKSelectItemInd(inMsg);
			break;

		case MSG_SATK_EVENT_SEND_SS:
			AT_HandleSTKSsSentInd(inMsg);
			break;

		case MSG_SATK_EVENT_SEND_USSD:
			AT_HandleSTKUssdSentInd(inMsg);
			break;

		case MSG_SATK_EVENT_SETUP_CALL:
			AT_HandleSTKCallSetupInd(inMsg);
			break;

		case MSG_SATK_EVENT_SEND_DTMF:
			AT_HandleSTKSendDtmfInd(inMsg);
			break;

		case MSG_SATK_EVENT_SETUP_MENU:
			MSG_LOGV("ATC:MSG_SATK_EVENT_SETUP_MENU inMsg = ",(UInt32)inMsg);
			AT_HandleSTKSetupMenuInd(inMsg);
			break;

		case MSG_SATK_EVENT_MENU_SELECTION:
			break;

		case MSG_SATK_EVENT_REFRESH:
			MSG_LOGV("ATC:MSG_SATK_EVENT_REFRESH inMsg = ",(UInt32)inMsg);
			AT_HandleSTKRefreshInd(inMsg);
			break;

		case MSG_SIM_MMI_SETUP_EVENT_IND:
			AT_HandleSTKSetupEventList(inMsg);
			break;

		case MSG_SATK_EVENT_SEND_SHORT_MSG:
			AT_HandleSTKSmsSentInd(inMsg);
			break;

		case MSG_SATK_EVENT_PROV_LOCAL_LANG:
			AT_OutputUnsolicitedStr((UInt8*)"*MTLANG: 1");
			break;

		case MSG_SATK_EVENT_DATA_SERVICE_REQ:
			AT_HandleSTKDataServInd(inMsg);
			break;

		case MSG_STK_RUN_AT_IND:
			AT_HandleSTKRunAtCmdInd(inMsg);
			break;

		case MSG_STK_LANG_NOTIFICATION_IND:
			AT_HandleSTKLangNotificationInd(inMsg);
			break;

		case MSG_SATK_EVENT_STK_SESSION_END:
			AT_OutputUnsolicitedStr((UInt8*)"*MSTKEV: 1");
			break;

		case MSG_SATK_EVENT_CALL_CONTROL_RSP:
			MSG_LOGV("ATC:MSG_SATK_EVENT_CALL_CONTROL_RSP inMsg = ",(UInt32)inMsg);
			// **FixMe** TBD
			break;

		// SMS messages
		case MSG_SMS_READY_IND:
			MSG_LOGV("ATC:MSG_SMS_READY_IND inMsg = ",(UInt32)inMsg);
			AtSendVMWaitingInd();
			_TRACE(("SMS READY: SIM SMS status = ", ((smsModuleReady_t *)inMsg->dataBuf)->simSmsStatus ));
			_TRACE(("SMS READY: ME SMS status = ", ((smsModuleReady_t *)inMsg->dataBuf)->meSmsStatus ));
			break;

		case MSG_SMS_COMPOSE_REQ:
			MSG_LOGV("ATC:MSG_SMS_COMPOSE_REQ inMsg = ",(UInt32)inMsg);
			ATHandleComposeCmd(inMsg);
			break;

		case MSG_SMSPP_DELIVER_IND:
			MSG_LOGV("ATC:MSG_SMSPP_DELIVER_IND inMsg = ",(UInt32)inMsg);
			AtHandleSmsDeliverInd(inMsg);
			break;

		case MSG_SMSPP_OTA_IND:
			MSG_LOGV("ATC:MSG_SMSPP_OTA_IND inMsg = ",(UInt32)inMsg);
			AtHandleSmsDeliverInd(inMsg);
			break;

		case MSG_SMSPP_REGULAR_PUSH_IND:
			MSG_LOGV("ATC:MSG_SMSPP_REGULAR_PUSH_IND inMsg = ",(UInt32)inMsg);
			AtHandleSmsDeliverInd(inMsg);
			break;

		case MSG_SMSPP_SECURE_PUSH_IND:
			MSG_LOGV("ATC:MSG_SMSPP_SECURE_PUSH_IND inMsg = ",(UInt32)inMsg);
			AtHandleSmsDeliverInd(inMsg);
			break;

		case MSG_SMSPP_STORED_IND:
			MSG_LOGV("ATC:MSG_SMSPP_STORED_IND  inMsg = ",(UInt32)inMsg);
			AtHandleIncomingSmsStoredRes(inMsg);
			break;

		case MSG_SMS_WRITE_RSP_IND:
			MSG_LOGV("ATC:MSG_SMS_WRITE_RSP_IND  inMsg = ",(UInt32)inMsg);
			AtHandleWriteSmsStoredRes(inMsg);
			break;

		case MSG_SMSSR_STORED_IND:
			MSG_LOGV("ATC:MSG_SMSSR_STORED_IND  inMsg = ",(UInt32)inMsg);
			AtHandleSmsStatusRptStoredRes(inMsg);
			break;

		case MSG_SMSCB_STORED_IND:
			MSG_LOGV("ATC:MSG_SMSCB_STORED_IND  inMsg = ",(UInt32)inMsg);
			AtHandleSMSCBStoredRsp(inMsg);
			break;

		case MSG_SMSCB_READ_RSP:  // SIM SMSCB read response
			MSG_LOGV("ATC:MSG_SMSCB_READ_RSP (CMGR CB) inMsg = ",(UInt32)inMsg);
			AtHandleSmsReadResp(inMsg);
			break;

		case MSG_SIM_SMS_DATA_RSP:  // SIM SMS read response (CMGR/CMGL or CMSS)
			cmdId = AT_GetCmdIdByMpxChan(AT_GetMpxChanFromV24ClientID(inMsg->clientID));
			switch(cmdId)
			{
				case AT_CMD_CMSS:
					MSG_LOGV("ATC:MSG_SIM_SMS_DATA_RSP (CMSS) inMsg = ",(UInt32)inMsg);
					AtHandleSmsUsrDataInd(inMsg);
					break;

				case AT_CMD_CMGR:
					MSG_LOGV("ATC:MSG_SIM_SMS_DATA_RSP (CMGR) inMsg = ",(UInt32)inMsg);
					AtHandleSmsReadResp(inMsg);
					break;

				case AT_CMD_CMGL:
					MSG_LOGV("ATC:MSG_SIM_SMS_DATA_RSP (CMGL) inMsg = ",(UInt32)inMsg);
					AtHandleSmsReadResp(inMsg);
					break;

				default:
					MSG_LOGV("ATC:MSG_SIM_SMS_DATA_RSP (invalid msg) inMsg = ",(UInt32)inMsg);
					break;
			}
			break;

		case MSG_SMS_SUBMIT_RSP:
			MSG_LOGV("ATC:MSG_SMS_SUBMIT_RSP inMsg = ",(UInt32)inMsg);
			AtHandleSmsSubmitRsp(inMsg);
			break;

		case MSG_SMS_USR_DATA_IND:
			MSG_LOGV("ATC:MSG_SMS_USR_DATA_IND inMsg = ",(UInt32)inMsg);
			AtHandleSmsUsrDataInd(inMsg);
			break;

		case MSG_SMSSR_REPORT_IND:
			MSG_LOGV("ATC:MSG_SMSSR_REPORT_IND inMsg = ",(UInt32)inMsg);
			AtHandleSmsStatusReportInd(inMsg);
			break;

		case MSG_SMS_REPORT_IND:
			MSG_LOGV("ATC:MSG_SMS_REPORT_IND inMsg = ",(UInt32)inMsg);
			AtHandleSmsReportInd(inMsg);
			break;

		case MSG_SMS_MEM_AVAIL_RSP:
			MSG_LOGV("ATC:MSG_SMS_MEM_AVAIL_RSP inMsg = ",(UInt32)inMsg);
			// nothing need to be done in ATC for this message.
			break;

		case MSG_SMS_COMMAND_RSP:
			MSG_LOGV("ATC:MSG_SMS_COMMAND_RSP inMsg = ",(UInt32)inMsg);
			// nothing need to be done in ATC for this message.
			break;

		case MSG_SIM_SMS_STATUS_UPD_RSP:	// SIM/ME delete response
			MSG_LOGV("ATC:MSG_SIM_SMS_STATUS_UPD_RSP(D) inMsg = ",(UInt32)inMsg);
			AtHandleSmsDeleteSimRsp(inMsg);
			break;

		case MSG_SMS_CNMA_TIMER_IND:
			MSG_LOGV("ATC:MSG_SMS_CNMA_TIMER_IND inMsg = ",(UInt32)inMsg);
			AtHandleSmsCnmaTimerEvent(inMsg);
			break;

		case MSG_SMS_CB_START_STOP_RSP:
			MSG_LOGV("ATC:MSG_SMS_CB_START_STOP_RSP inMsg = ",(UInt32)inMsg);
			AtHandleCBStartStopRsp(inMsg);
			break;

		case MSG_SMSCB_DATA_IND:
			MSG_LOGV("ATC:MSG_SMSCB_DATA_IND inMsg = ",(UInt32)inMsg);
			AtHandleCBDataInd(inMsg);
			break;

		case MSG_VM_WAITING_IND:  // unsolicied voicemail indication
			MSG_LOGV("ATC:MSG_VM_WAITING_IND inMsg = ",(UInt32)inMsg);
			AtHandleVMWaitingInd(inMsg);
			break;

		case MSG_SIM_PIN_ATTEMPT_RSP:
			AtHandleSimPinAttemptRsp(inMsg);
			break;

		case MSG_VMSC_UPDATE_RSP:
			MSG_LOGV("ATC:MSG_VMSC_UPDATE_RSP inMsg = ",(UInt32)inMsg);
			AtHandleVmscUpdateRsp(inMsg);
			break;

		//Handle Call control message
		case MSG_CALL_STATUS_IND:
			MSG_LOGV("ATC:MSG_CALL_STATUS_IND inMsg = ",(UInt32)inMsg);
			AtHandleCallStatus(inMsg);
			break;

		case MSG_VOICECALL_ACTION_RSP:
			MSG_LOGV("ATC:MSG_VOICECALL_ACTION_RSP inMsg = ",(UInt32)inMsg);
			AtHandleCallActionRes(inMsg);
			break;

		case MSG_VOICECALL_PRECONNECT_IND:
			MSG_LOGV("ATC:MSG_VOICECALL_PRECONNECT_IND inMsg = ",(UInt32)inMsg);
			AtHandleCallPreconnect(inMsg);
			break;

		case MSG_VOICECALL_CONNECTED_IND:
			MSG_LOGV("ATC:MSG_VOICECALL_CONNECTED_IND inMsg = ",(UInt32)inMsg);
			AtHandleCallConnect(inMsg);
			break;

		case MSG_VOICECALL_RELEASE_IND:
			MSG_LOGV("ATC:MSG_VOICECALL_RELEASE_IND inMsg = ",(UInt32)inMsg);
			AtHandleCallRelease(inMsg);
			break;

		case MSG_INCOMING_CALL_IND:
			MSG_LOGV("ATC:MSG_INCOMING_CALL_IND inMsg = ",(UInt32)inMsg);
			AtHandleCallIncoming(inMsg);
			break;

		case MSG_API_CLIENT_CMD_IND:
			MSG_LOGV("ATC:MSG_API_CLIENT_CMD_IND inMsg = ",(UInt32)inMsg);
			ATHandleApiClientCmdInd(inMsg);
			break;

		case MSG_VOICECALL_WAITING_IND:
			MSG_LOGV("ATC:MSG_VOICECALL_WAITING_IND inMsg = ",(UInt32)inMsg);
			AtHandleCallWaiting(inMsg);
			break;

		case MSG_DATACALL_STATUS_IND:
			MSG_LOGV("ATC:MSG_DATACALL_STATUS_IND inMsg = ",(UInt32)inMsg);
			AtHandleDataCallStatus(inMsg);
			break;


		case MSG_DATACALL_ECDC_IND:
			MSG_LOGV("ATC:MSG_DATACALL_ECDC_IND inMsg = ",(UInt32)inMsg);
			AtHandleDataEcdcLinkRsp(inMsg);
			break;

		case MSG_DATACALL_CONNECTED_IND:
			MSG_LOGV("ATC:MSG_DATACALL_CONNECTED_IND inMsg = ",(UInt32)inMsg);
			AtHandleDataCallConnect(inMsg);
			break;

		case MSG_DATACALL_RELEASE_IND:
			MSG_LOGV("ATC:MSG_DATACALL_RELEASE_IND inMsg = ",(UInt32)inMsg);
			AtHandleDataCallRelease(inMsg);
			break;

		case MSG_CALL_CONNECTEDLINEID_IND:
			MSG_LOGV("ATC:MSG_CALL_CONNECEDLINEID_IND inMsg = ",(UInt32)inMsg);
			AtHandleConnectedLineID(inMsg);
			break;

		case MSG_CALL_CCM_IND:

			MSG_LOGV("ATC:MSG_CALL_CCM_IND inMsg = ",(UInt32)inMsg);
			AtHandleCallCCM(inMsg);
			break;

		case MSG_CALL_AOCSTATUS_IND:

			MSG_LOGV("ATC:MSG_CALL_AOCSTATUS_IND inMsg = ",(UInt32)inMsg);
			AtHandleCallAOCStatus(inMsg);
			break;

		case MSG_AT_CALL_TIMER:
			MSG_LOGV("ATC:MSG_AT_CALL_TIMER inMsg = ",(UInt32)inMsg);
			AtHandleCallTimer(inMsg);
			break;

		case MSG_AT_CALL_ABORT:
			MSG_LOGV("ATC:MSG_AT_CALL_ABORT inMsg = ",(UInt32)inMsg);
			AtHandleCallAbort(inMsg);
			break;

		case MSG_AT_ESC_DATA_CALL:
			MSG_LOGV("ATC:MSG_AT_ESC_DATA_CALL inMsg = ",(UInt32)inMsg);
			AtHandleEscDataCall(inMsg);
			break;

		case MSG_AT_HANDLE_CHLD_CMD:
			MSG_LOGV("ATC:MSG_AT_CHLD_PROCESS inMsg = ",(UInt32)inMsg);
			AtProcessChldCmd(inMsg);
			break;

		case MSG_USSD_DATA_IND:
			MSG_LOGV("ATC:MSG_USSD_DATA_IND inMsg = ",(UInt32)inMsg);
			AT_HandleUssdDataInd(inMsg);
			break;

		case MSG_USSD_DATA_RSP:
			MSG_LOGV("ATC:MSG_USSD_DATA_RSP inMsg = ",(UInt32)inMsg);
			AT_HandleUssdDataRsp(inMsg);
			break;

		case MSG_USSD_SESSION_END_IND:
			MSG_LOGV("ATC:MSG_USSD_SESSION_END_IND inMsg = ",(UInt32)inMsg);
			AT_HandleUssdSessionEndInd(inMsg);
			break;

		case MSG_SS_CALL_FORWARD_RSP:
		case MSG_SS_CALL_BARRING_RSP:
		case MSG_SS_CALL_BARRING_PWD_CHANGE_RSP:
		case MSG_SS_CALL_WAITING_RSP:
			AT_HandleSetSuppSvcStatusRsp(inMsg);
			break;

		case MSG_SS_CALL_FORWARD_STATUS_RSP:
			AT_HandleCallForwardStatusRsp(inMsg);
			break;

		case MSG_SS_CALLING_LINE_ID_STATUS_RSP:
		case MSG_SS_CONNECTED_LINE_STATUS_RSP:
		case MSG_SS_CALLING_LINE_RESTRICTION_STATUS_RSP:
 		case MSG_SS_CONNECTED_LINE_RESTRICTION_STATUS_RSP:
		case MSG_SS_CALLING_NAME_PRESENT_STATUS_RSP:
			AT_HandleProvisionStatusRsp(inMsg);
			break;

		case MSG_SS_INTERNAL_PARAM_SET_IND:
			AT_HandleIntParamSetIndMsg(inMsg);
			break;

		case MSG_SS_CALL_BARRING_STATUS_RSP:
			AT_HandleCallBarringStatusRsp(inMsg);
			break;

		case MSG_SS_CALL_WAITING_STATUS_RSP:
			AT_HandleCallWaitingStatusRsp(inMsg);
			break;

		case MSG_USSD_CALLINDEX_IND:
			MSG_LOGV("ATC:MSG_USSD_CALLINDEX_IND inMsg = ",(UInt32)inMsg);
			AT_HandleSSCallIndexInd(inMsg);
			break ;

		case MSG_POWER_DOWN_CNF:
			MSG_LOGV("ATC:MSG_POWER_DOWN_CNF inMsg = ",(UInt32)inMsg);
			AtHandlePowerDownCnf(inMsg);
			break ;

 		case MSG_SS_CALL_NOTIFICATION:
		case MSG_SS_NOTIFY_CLOSED_USER_GROUP:
		case MSG_SS_NOTIFY_EXTENDED_CALL_TRANSFER:
		case MSG_SS_NOTIFY_CALLING_NAME_PRESENT:
 			MSG_LOG("ATC: NOTIFICATION");
 			At_HandleSsNotificationInd(inMsg);
 			break ;

		case MSG_MEASURE_REPORT_PARAM_IND:
 			MSG_LOG("ATC: MSG_MEASURE_REPORT_PARAM_IND");
			At_HandleMeasReportParamInd(inMsg);
			break;

		// DC broadcasting message
		case MSG_DC_REPORT_CALL_STATUS:
			MSG_LOGV("ATC:MSG_DC_REPORT_CALL_STATUS inMsg = ",(UInt32)inMsg);
			AtHandleDCReportCallStatus(inMsg);
			break;

		case MSG_STK_CC_SETUPFAIL_IND:
			MSG_LOGV("ATC:MSG_STK_CC_SETUPFAIL_IND inMsg = ",(UInt32)inMsg);
			AtHandleSTKCCSetupFail(inMsg);
			break;

		case MSG_STK_CC_DISPLAY_IND:
			MSG_LOGV("ATC:MSG_STK_CC_DISPLAY_IND inMsg = ",(UInt32)inMsg);
			AtHandleSTKCCDisplayText(inMsg);
			break;

		case MSG_SS_CALL_REQ_FAIL:
			MSG_LOGV("ATC:MSG_SS_CALL_REQ_FAIL inMsg = ",(UInt32)inMsg);
			AT_HandleSsCallReqFail(inMsg);
			break;

		default:
			break;
	}

}

void SysProcessDownloadReq( void )
{
	OSTASK_Sleep( TICKS_ONE_SECOND );
	OSINTERRUPT_DisableAll();
#if (CHIPVERSION < CHIP_VERSION(BCM2133,00)) || defined(DSP2133_TEST)
	if ( IsCardInsidePCSlot() )		// if PC card is inside PC slot
	{		// In PCMCIA mode, after reset the chip,
			// UART C doesn't work if PC driver doesn't initilize it again
		extern void Soft_download(void);
		Soft_download();
	}
	else						// PC card or MS is stand-alone
	{
		*(UInt8*)PUMR_REG = SEL_SOFT_DOWNLOAD;
		RebootProcessor();
	}
#else // #if (CHIPVERSION < CHIP_VERSION(BCM2133,00)) || defined(DSP2133_TEST)
	Soft_download();
#endif // #if (CHIPVERSION < CHIP_VERSION(BCM2133,00)) || defined(DSP2133_TEST)
}


void SysProcessCalibrateReq( void )
{
	OSTASK_Sleep(TICKS_ONE_SECOND );
#ifndef FPGA_VERSION				// HW reset
	SELECT_CALIBRATION;
#else								// SW reset when FPGA used
	OSINTERRUPT_DisableAll();

	*(volatile UInt8 *)PUMR_REG = SEL_CALIBRATION;
	SecureDownload();
#endif

//#if (CHIPVERSION < CHIP_VERSION(BCM2133,00)) || defined(DSP2133_TEST)
//	SELECT_CALIBRATION;
//#else // #if (CHIPVERSION < CHIP_VERSION(BCM2133,00)) || defined(DSP2133_TEST)
//	OSINTERRUPT_DisableAll();
//	*(volatile UInt8 *)PUMR_REG = SEL_CALIBRATION;
//	SecureDownload();
//#endif // #if (CHIPVERSION < CHIP_VERSION(BCM2133,00)) || defined(DSP2133_TEST)

}

