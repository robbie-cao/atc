/**
*
*   @file   at_ss.c
*
*   @brief  This file defines the AT functions for supplementary services.
*
****************************************************************************/
//-----------------------------------------------------------------------------
//	Supplementary services.
//
//	Implemented handlers:	todo tbd fixme this information can be deleted
//	after integration:
//
//		ATCmd_CCFC_Handler			- handle CCFC command
//		ATCmd_CCWA_Handler			- handle CCWA command
//		ATCmd_CLIP_Handler			- handle CLIP command
//		ATCmd_CLIR_Handler			- handle CLIR command
//		ATCmd_COLP_Handler			- handle COLP command
//		AT_ProcessSuppSvcCallCmd	- handle SS dial command
//
//-----------------------------------------------------------------------------
#define ENABLE_LOGGING

#include "stdlib.h"
#include "v24.h"
#include "bcd.h"
#include "atc.h"
#include "util.h"
#include "at_ss.h"
#include "at_cfg.h"
#include "mti_trace.h"
#include "ms_database.h"
#include "ss_api.h"
#include "bcd.h"
#include "mnssprim.h"
#include "callconfig.h"
#include "osheap.h"
#include "mstypes.h"

#define MAX_RESPONSE_LEN		100

static	Boolean			isUssdTypeMT = TRUE;
static  CallIndex_t		ussd_id = CALLINDEX_INVALID;
static  CallIndex_t		previousUssdId = CALLINDEX_INVALID;

static  UInt8 ussd_chan;

static void	AT_ProcessUssdDataForDisplay(InterTaskMsg_t *inMsg);

//******************************************************************************
// Convert supplementary service BS code to class number.
//******************************************************************************
static ATC_SERVICE_CLASS_t SuppSvcClassToATCClass(SS_SvcCls_t ss_class)
{
        ATC_SERVICE_CLASS_t class_no;

        switch(ss_class)
        {
			case SS_SVCCLS_SPEECH:
			case SS_SVCCLS_ALT_SPEECH:
			case SS_SVCCLS_ALL_TELE_SERVICES:
			case SS_SVCCLS_ALL_TELESERVICE_EXCEPT_SMS:
				class_no = ATC_SERVICE_VOICE;
				break;
			case SS_SVCCLS_DATA:
				class_no = ATC_SERVICE_DATA;
				break;
			case SS_SVCCLS_FAX:
				class_no = ATC_SERVICE_FAX;
				break;
			case SS_SVCCLS_SMS:
				class_no = ATC_SERVICE_SMS;
				break;
#ifndef GCXX_PC_CARD
			case SS_SVCCLS_DATA_CIRCUIT_SYNC:
				class_no = ATC_SERVICE_DATA_CIRCUIT_SYNC;
				break;
			case SS_SVCCLS_DATA_CIRCUIT_ASYNC:
				class_no = ATC_SERVICE_DATA_CIRCUIT_ASYNC;
				break;
			case SS_SVCCLS_DEDICATED_PACKET:
				class_no = ATC_SERVICE_DPA;
				break;
			case SS_SVCCLS_DEDICATED_PAD:
				class_no = ATC_SERVICE_DPAD;
				break;
			case SS_SVCCLS_ALL_SYNC_SERVICES:
			case SS_SVCCLS_ALL_ASYNC_SERVICES:
			case SS_SVCCLS_ALL_BEARER_SERVICES:
				class_no = ATC_SERVICE_DATA;
				break;
#else //GCXX_PC_CARD
			//Certain PC SW does not recognize 16,32,64,128 as
			//data classes, instead looks only for class 2 as a
			//data class. So here we force the data class returned
			//by the network to Data for compatibility.
			case SS_SVCCLS_DATA_CIRCUIT_SYNC:
			case SS_SVCCLS_DATA_CIRCUIT_ASYNC:
			case SS_SVCCLS_DEDICATED_PACKET:
			case SS_SVCCLS_DEDICATED_PAD:
			case SS_SVCCLS_ALL_SYNC_SERVICES:
			case SS_SVCCLS_ALL_ASYNC_SERVICES:
			case SS_SVCCLS_ALL_BEARER_SERVICES:
				class_no = ATC_SERVICE_DATA;
				break;
#endif //GCXX_PC_CARD
			case SS_SVCCLS_PLMN_SPEC_TELE_SVC: //Not defined in GSM 07.07
			default:
				class_no = ATC_SERVICE_INVALID;
				break;
		}

        return (class_no);
}


//******************************************************************************
//
// Function Name:	SuppSvcGetV24Chan
//
// Description:		This function gets the V24 channel number of the active
//					Supplementary Service session.
//
// Return:			TRUE if a valid V24 channel number is found and written to the passed "chan".
//					FALSE otherwise.
//
//******************************************************************************
static Boolean SuppSvcGetV24Chan(UInt8 *chan)
{
	UInt8 clientID = SS_GetClientID();
	UInt8 temp_chan;

	/* Reset SS client ID for next SS session */
	SS_ResetClientID();

	if(INVALID_CLIENT_ID == clientID)
	{
		return FALSE;
	}

	/* Get the V24 channel number based on client ID */
	temp_chan = AT_GetMpxChanFromV24ClientID(clientID);

	if(temp_chan == INVALID_MPX_CHAN)
	{
		return FALSE;
	}

	if( MPX_ObtainMuxMode() == NONMUX_MODE ||
	    (MPX_ObtainMuxMode() == MUX_MODE && V24_GetOperationMode((DLC_t) temp_chan) != V24OPERMODE_DATA) )
	{
		V24_SetOperationMode((DLC_t) temp_chan, V24OPERMODE_AT_CMD);
	}

	/* If we get here, the V24 channel number is valid */
	*chan = temp_chan;

	return TRUE;
}

//******************************************************************************
// Function Name:	SS_ProcessCmdResult
//
// Description:		Process the return result.
//******************************************************************************
static AT_Status_t SS_ProcessCmdResult(UInt8 inChannel, Result_t inResult)
{
	switch (inResult)
	{
		case RESULT_OK:
			return(AT_STATUS_PENDING);

		case SS_OPERATION_IN_PROGRESS:
			AT_CmdRspError(inChannel, AT_CME_ERR_AT_BUSY);
			return(AT_STATUS_DONE);

		case STK_DATASVRC_BUSY:
			AT_CmdRsp( inChannel, AT_BUSY_RSP );
			return(AT_STATUS_DONE);;
		default:
			AT_CmdRspError(inChannel, AT_CME_ERR_PHONE_FAILURE);
			return(AT_STATUS_DONE);
	}
}

//******************************************************************************
//
// Function Name:	AT_SoliciteCM
//
// Description:		Returns if CCM solicitation is turned on.
//
// Notes:
//
//******************************************************************************
Boolean AT_SoliciteCM(void)
{
	return (AT_GetCommonCfg()->Unsoliciated_CCMReport);
}

//******************************************************************************
//
// Function Name:	AT_CallMeterWarning
//
// Description:		Returns if Call Meter Warning is turned on.
//
// Notes:
//
//******************************************************************************
Boolean AT_CallMeterWarning(void)
{
	return (AT_GetCommonCfg()->CallMeterWarning);
}

//-----------------------------------------------------------------------------
//	AT_ProcessSuppSvcCallCmd - called by AT parser to handle a 'dialed' SS
//	command
//-----------------------------------------------------------------------------

AT_Status_t AT_ProcessSuppSvcCallCmd( UInt8 chan, const UInt8* dialStr )
{
	AT_CmdRspOK( chan ) ;
	return AT_STATUS_DONE ;
}

//-----------------------------------------------------------------------------
//	AT_HandleUssdDataInd - handle network initiated USSD event (MT USSD)
//-----------------------------------------------------------------------------
void AT_HandleUssdDataInd(InterTaskMsg_t *inMsg)
{
	if( !(AT_GetChannelCfg(ussd_chan)->CUSD) )
	{
		return;
	}

	AT_ProcessUssdDataForDisplay(inMsg);
}

//-----------------------------------------------------------------------------
//	AT_HandleUssdDataRsp - handle network response to MO USSD
//-----------------------------------------------------------------------------
void AT_HandleUssdDataRsp(InterTaskMsg_t *inMsg)
{
	AT_ProcessUssdDataForDisplay(inMsg);
}

//-----------------------------------------------------------------------------
//	AT_HandleUssdDataInd - handle network initiated USSD event
//-----------------------------------------------------------------------------
void AT_ProcessUssdDataForDisplay(InterTaskMsg_t *inMsg)
{
	char*	string1;
	char*	output;
	USSDataInfo_t *ussd_data_info;
	UInt8	len1;
	const char cusd_cmd_str[] = "+CUSD";
	UInt8	dcs;

	ussd_data_info = (USSDataInfo_t *) inMsg->dataBuf;
	ussd_id = ussd_data_info->call_index;
	dcs = ussd_data_info->ussd_data.code_type;

	len1 = ussd_data_info->ussd_data.used_size;

	_TRACE(("ATC: USSD: service_type = ", ussd_data_info->ussd_data.service_type));

	if( IS_NON_UCS2_CODING(ussd_data_info->ussd_data.code_type) ){
		string1 = (char*)OSHEAP_Alloc(len1 + 1);
		output = (char*)OSHEAP_Alloc(len1 + 30);
		memcpy((UInt8*)string1, ussd_data_info->ussd_data.string, len1);
		string1[len1] = '\0';
	}
	else{ // UNICODE type, convert to hex for output
		string1 = (char*)OSHEAP_Alloc(2*len1 + 1);
		output = (char*)OSHEAP_Alloc(2*len1 + 30);
		dcs = 0x48;  // The code_type in UCS2 case is not the dcs, need to convert according to 3GPP3.38
		UTIL_HexDataToHexStr((UInt8*)string1, ussd_data_info->ussd_data.string, len1);
	}

	switch(ussd_data_info->ussd_data.service_type)
	{
		case USSD_RESEND:
			// update the ussd id
			if(ussd_id == ussd_data_info->ussd_data.oldindex)
				ussd_id = ussd_data_info->ussd_data.newindex;
			else{
				// something is not right
			}
			break;

		case USSD_FACILITY_RETURN_RESULT:
			if(ussd_data_info->ussd_data.used_size > 0)
			{
				sprintf( (char *)output, "%s: %d, \"%s\", %d", cusd_cmd_str, 1,
					string1, dcs);
			}
			else
			{
				sprintf( (char *)output, "%s: %d", cusd_cmd_str, 0 );
			}

			AT_OutputUnsolicitedStr ((UInt8*)output );
			break;

		case USSD_REGISTRATION:
		case USSD_REQUEST:
			sprintf( (char *)output, "%s: %d, \"%s\", %d", cusd_cmd_str, 1,
					string1, dcs);

			AT_OutputUnsolicitedStr ( (UInt8*)output );	// wait for user to response
			break;

		case USSD_NOTIFICATION:
			sprintf( (char *)output, "%s: %d, \"%s\", %d", cusd_cmd_str, 0,
					string1, dcs);

  			AT_OutputUnsolicitedStr ( (UInt8*)output );

			// response back to network is done on the API side.

			break;

		case USSD_RELEASE_COMPLETE_RETURN_RESULT:
			if(ussd_data_info->ussd_data.used_size > 0)
			{
				sprintf( (char *)output, "%s: %d, \"%s\", %d", cusd_cmd_str, 2,
					string1, dcs);
			}
			else
			{
   				sprintf( (char *)output, "%s: %d", cusd_cmd_str, 2);
			}

			AT_OutputUnsolicitedStr ( (UInt8*)output );
			ussd_id = CALLINDEX_INVALID;
			break;

		case USSD_RELEASE_COMPLETE_RETURN_ERROR:	// ?
		case USSD_FACILITY_RETURN_ERROR:
			sprintf( (char *)output, "%s: %d, \"%s\", %d", cusd_cmd_str, 5,	 // **FixMe** GSM 07.07 does not support these error cases, "5" is the best fit.
				string1, dcs);

			AT_OutputUnsolicitedStr ( (UInt8*)output );
			ussd_id = CALLINDEX_INVALID;
			break;

		case USSD_RELEASE_COMPLETE_REJECT:
		case USSD_FACILITY_REJECT:
			sprintf( (char *)output, "%s: %d", cusd_cmd_str, 5 );	// **FixMe** GSM 07.07 does not support these error cases, "5" is the best fit.

			AT_OutputUnsolicitedStr ( (UInt8*)output );
			ussd_id = CALLINDEX_INVALID;
			break;

		default:
			break;
	}

	if((inMsg->clientID != INVALID_CLIENT_ID) &&
 		(ussd_data_info->ussd_data.service_type != USSD_RESEND) ){
		AT_CmdRspOK(AT_GetMpxChanFromV24ClientID(inMsg->clientID));
	}
	else{
		_TRACE(("ATC: USSD: Invalid ClientID"));
	}

	OSHEAP_Delete(string1);
	OSHEAP_Delete(output);
}


//******************************************************************************
//
// Function Name:	AT_HandleSetSuppSvcStatusRsp
//
// Description:		Handle the return result from the network for setting
//					the status of a Supplementary Service feature.
//
//******************************************************************************
void AT_HandleSetSuppSvcStatusRsp(InterTaskMsg_t *inMsg)
{
	UInt8			chan;
	NetworkCause_t	netCause = *((NetworkCause_t*)inMsg->dataBuf);

	MSG_LOGV("AT_HandleSetSuppSvcStatusRsp()    netCause =", netCause);

	SsApi_ResetSsAlsFlag();

	/* Only send out the result to the host if we can get a valid channel:
	 * We may come here because MMI initiates the SS request, we need to ignore
	 * the result in this case since the SS response is broadcast.
	 */
	if (SuppSvcGetV24Chan(&chan))
	{
		if (netCause == GSMCAUSE_SUCCESS)
		{
			AT_CmdRspOK(chan);
		}
		else
		{
			AT_CmdRspError(chan, ConvertNWcause2CMEerror(netCause));
		}
	}
}


//******************************************************************************
// Function Name:	AT_HandleCallForwardStatusRsp
//
// Description:		Handle the call forward status information queried from
//					the network.
//******************************************************************************
void AT_HandleCallForwardStatusRsp(InterTaskMsg_t *inMsg)
{
	UInt8					theIndex;
	UInt8 					theChannel;
	UInt8					theLoopCounter = 0;
	ATC_SERVICE_CLASS_t		theAtcClass;
	CallForwardStatus_t*	theCfStatusPtr;
	CallForwardClassInfo_t* theCfClassPtr;
	char					theStrBuf[MAX_RESPONSE_LEN + 1];
	const UInt8*			theCmdPtr = AT_GetCmdName(AT_CMD_CCFC);

	MSG_LOG("AT_HandleCallForwardStatusRsp()");

	SsApi_ResetSsAlsFlag();

	// Only send out the result to the host if we can get a valid
	//  channel: if the command is initiated from AT port and not MMI.
	if (!SuppSvcGetV24Chan(&theChannel)) return;

	theCfStatusPtr = (CallForwardStatus_t *) inMsg->dataBuf;

	MSG_LOGV("Cause =", theCfStatusPtr->netCause);

	if (theCfStatusPtr->netCause == GSMCAUSE_SUCCESS)
	{
		theCfClassPtr = theCfStatusPtr->call_forward_class_info_list;

		for (theIndex = 0; theIndex < theCfStatusPtr->class_size; theIndex++)
		{
			theAtcClass = SuppSvcClassToATCClass(theCfClassPtr->ss_class);

			if (theCfClassPtr->forwarded_to_number.number[0] == '\0')
			{
				sprintf(theStrBuf, "%s: %d,%d",
						theCmdPtr,
						theCfClassPtr->activated,
						theAtcClass);
			}
			else
			{
				//Precaution, in order to avoid assertion if the network sends
				//unexpected message length
				((char*)theCfClassPtr->forwarded_to_number.number)[
				sizeof(theCfClassPtr->forwarded_to_number.number) - 1 ] = '\0';

				sprintf(theStrBuf, "%s: %d,%d,\"%s\",%d",
						theCmdPtr,
						theCfClassPtr->activated,
						theAtcClass,
						theCfClassPtr->forwarded_to_number.number,
						theCfClassPtr->forwarded_to_number.ton ==
						InternationalTON ? INTERNA_TON_ISDN_NPI : UNKNOWN_TON_ISDN_NPI);
			}

			AT_OutputStr(theChannel, (UInt8 *) theStrBuf);
			theCfClassPtr++;
		}

		AT_CmdRspOK(theChannel);
	}
	else
	{
		AT_CmdRspError(	theChannel,
						ConvertNWcause2CMEerror(theCfStatusPtr->netCause));
	}
}

//******************************************************************************
//
// Function Name:	AT_HandleProvisionStatusRsp
//
// Description:		Handle the Supplementary Service provision status information
//					queried from the network. The applicable SS services include
//					calling line ID; calling line ID restriction; connected line
//					ID.
//
//******************************************************************************
void AT_HandleProvisionStatusRsp(InterTaskMsg_t *inMsg)
{
	UInt8 chan;
	SS_ProvisionStatus_t* theProvStatusPtr;

	MSG_LOG("AT_HandleProvisionStatusRsp()");

	SsApi_ResetSsAlsFlag();

	/* Only send out the result to the host if we can get a valid channel:
	 * We may come here because MMI initiates the SS request, we need to ignore
	 * the result in this case since the SS response is broadcast.
	 */
	if( !SuppSvcGetV24Chan(&chan) )
	{
		return;
	}

	theProvStatusPtr = (SS_ProvisionStatus_t*)inMsg->dataBuf;

	MSG_LOGV("Cause =", theProvStatusPtr->netCause);

	if (theProvStatusPtr->netCause == GSMCAUSE_SUCCESS)
	{
		AT_CmdId_t cmd_id;
		char strBuf[MAX_RESPONSE_LEN + 1];
		UInt8 thePresentStatus = theProvStatusPtr->provision_status;
		MsConfig_t *ms_cfg = MS_GetCfg();

		switch(inMsg->msgType)
		{
		case MSG_SS_CALLING_LINE_ID_STATUS_RSP:
			cmd_id = AT_CMD_CLIP;
			thePresentStatus = ms_cfg->CLIP;
			break;

		case MSG_SS_CONNECTED_LINE_STATUS_RSP:
			cmd_id = AT_CMD_COLP;
			thePresentStatus = ms_cfg->COLP;
			break;

		case MSG_SS_CALLING_NAME_PRESENT_STATUS_RSP:
			cmd_id = AT_CMD_MCNAP;
			break;

		case MSG_SS_CONNECTED_LINE_RESTRICTION_STATUS_RSP:
			sprintf(strBuf, "%s: %d",
					AT_GetCmdName(AT_CMD_MCOLR),
					theProvStatusPtr->provision_status);
			break;

		case MSG_SS_CALLING_LINE_RESTRICTION_STATUS_RSP:
		default:
			cmd_id = AT_CMD_CLIR;
			thePresentStatus = ms_cfg->CLIR;
			break;
		}

		if (inMsg->msgType != MSG_SS_CONNECTED_LINE_RESTRICTION_STATUS_RSP)
		{
		  sprintf( 	strBuf, "%s: %d, %d", AT_GetCmdName(cmd_id),
		  			thePresentStatus, theProvStatusPtr->serviceStatus);
		}

		AT_OutputStr(chan, (UInt8 *) strBuf);
		AT_CmdRspOK(chan);
	}
	else
	{
		AT_CmdRspError(	chan,
						ConvertNWcause2CMEerror(theProvStatusPtr->netCause));
	}
}


//******************************************************************************
//
// Function Name:	At_HandleCallNitificationInd
//
// Description: This function handles the MSG_SS_CALL_NOTIFICATION message.
//              Handle the Supplementary Service notification information
//				sent by the network.
//
// Note:		This fucntion shall only be activated for the testing purpose.
//
//******************************************************************************
void At_HandleSsNotificationInd(InterTaskMsg_t *inMsgPtr)
{
	UInt8					theIndex;
	UInt8					output[PHASE1_MAX_USSD_STRING_SIZE+1];
	CCallType_t				theCallType;
	SS_CallNotification_t*	theSsNotifyPtr = (SS_CallNotification_t *)inMsgPtr->dataBuf;
	SsNotifyParam_t*		theNotifyParamPtr = &theSsNotifyPtr->notify_param;

	theCallType = CC_GetCallType(theSsNotifyPtr->index);
	memset(output, 0x00, PHASE1_MAX_USSD_STRING_SIZE+1);

	MSG_LOGV("ATC: NotifySS_Oper: ", theSsNotifyPtr->NotifySS_Oper);

	if (AT_GetCommonCfg()->CSSI &&
		((theCallType == MOVOICE_CALL) ||
		 (theCallType == MODATA_CALL) ||
		 (theCallType == MOFAX_CALL)))
	{
		switch (theSsNotifyPtr->NotifySS_Oper)
		{
			case CALLNOTIFYSS_CFU_ACTIVE:
			case CALLNOTIFYSS_INCALL_FORWARDED:
				sprintf((char*)output, "+CSSI: 0");
				break;

			case CALLNOTIFYSS_CFC_ACTIVE:
				sprintf((char*)output, "+CSSI: 1");
				break;

			case CALLNOTIFYSS_OUTCALL_FORWARDED:
				sprintf((char*)output, "+CSSI: 2");
				break;

			case CALLNOTIFYSS_CALL_WAITING:
				sprintf((char*)output, "+CSSI: 3");
				break;

			case CALLNOTIFYSS_CUGINDEX:
				sprintf((char*)output, "+CSSI: 4,%d", (int)theNotifyParamPtr->cug_index);
				break;

			case CALLNOTIFYSS_OUTCALL_BARRED:
			case CALLNOTIFYSS_BAOC:
			case CALLNOTIFYSS_BOIC:
			case CALLNOTIFYSS_BOIC_EX_HC:
				sprintf((char*)output, "+CSSI: 5");
				break;

			case CALLNOTIFYSS_INCALL_BARRED:	//Incomming call barred
			case CALLNOTIFYSS_INCOMING_BARRED: //All incoming calls barred
			case CALLNOTIFYSS_BAIC_ROAM:
				sprintf((char*)output, "+CSSI: 6");
				break;

			case CALLNOTIFYSS_BAC:
				sprintf((char*)output, "+CSSI: 5");
				AT_OutputUnsolicitedStr((UInt8*)output);
				sprintf((char*)output, "+CSSI: 6");
				break;

			case CALLNOTIFYSS_CLIRSUPREJ:
				sprintf((char*)output, "+CSSI: 7");
				break;

			case CALLNOTIFYSS_CALL_DEFLECTED:
				sprintf((char*)output, "+CSSI: 8");
				break;

			default:
				return;
		}
		AT_OutputUnsolicitedStr((UInt8*)output);
	}

	if (AT_GetCommonCfg()->CSSU &&
		((theCallType == MTVOICE_CALL) ||
		 (theCallType == MTDATA_CALL) ||
		 (theCallType == MTFAX_CALL)))
	{
		switch (theSsNotifyPtr->NotifySS_Oper)
		{
			case CALLNOTIFYSS_FORWARDED_CALL:
				sprintf((char*)output, "+CSSU: 0");
				break;

			case CALLNOTIFYSS_CUGINDEX:
				sprintf((char*)output, "+CSSU: 1,%d", (int)theNotifyParamPtr->cug_index);
				break;

			case CALLNOTIFYSS_CALLONHOLD:
				if (theCallType == MTVOICE_CALL)
				{
					sprintf((char*)output, "+CSSU: 2");
				}
				break;

			case CALLNOTIFYSS_CALLRETRIEVED:
				if (theCallType == MTVOICE_CALL)
				{
					sprintf((char*)output, "+CSSU: 3");
				}
				break;

			case CALLNOTIFYSS_MPTYIND:
				if (theCallType == MTVOICE_CALL)
				{
					sprintf((char*)output, "+CSSU: 4");
				}
				break;

			case CALLNOTIFYSS_CALL_ON_HOLD_RELEASED://this is not a SS notification
				if (theCallType == MTVOICE_CALL)
				{
					sprintf((char*)output, "+CSSU: 5");
				}
				break;

			case CALLNOTIFYSS_FORWARD_CHECK_SS_MSG:
				sprintf((char*)output, "+CSSU: 6");
				break;

			case CALLNOTIFYSS_ECT_INDICATION:
				if (theSsNotifyPtr->notify_param.ect_rdn_info.call_state ==
					ECTSTATE_ALERTING)
				{
					theIndex = 7;
				}
				else
				{
					theIndex = 8;
				}

				if (theSsNotifyPtr->notify_param.ect_rdn_info.present_allowed_add ||
					theSsNotifyPtr->notify_param.ect_rdn_info.present_restricted_add)
				{
					sprintf((char*)output, "+CSSU: %d, %s",
							theIndex,
							(char*)theSsNotifyPtr->notify_param.ect_rdn_info.phone_number.number);
				}
				else
				{
					sprintf((char*)output, "+CSSU: %d", theIndex);
				}
				break;

			case CALLNOTIFYSS_DEFLECTED_CALL:
				sprintf((char*)output, "+CSSU: 9");
				break;

			default:
				return;
		}
		AT_OutputUnsolicitedStr((UInt8*)output);
	}
}

//******************************************************************************
// Function Name:	AT_HandleCallBarringStatusRsp
//
// Description:		Handle the call barring status queried from the network.
//******************************************************************************
void AT_HandleCallBarringStatusRsp(InterTaskMsg_t *inMsg)
{
	UInt8						theIndex;
	UInt8 						theChannel;
	ATC_SERVICE_CLASS_t			theAtcClass;
	CallBarringStatus_t*		theCbStatusPtr;
	SS_ActivationClassInfo_t*	theCbClassPtr;
	char						theStrBuf[MAX_RESPONSE_LEN + 1];
	const UInt8*				theCmdPtr = AT_GetCmdName(AT_CMD_CLCK);

	MSG_LOG("AT_HandleCallBarringStatusRsp()");

	SsApi_ResetSsAlsFlag();

	// Only send out the result to the host if we can get a valid
	//  channel: if the command is initiated from AT port and not MMI.
	if (!SuppSvcGetV24Chan(&theChannel)) return;

	theCbStatusPtr = (CallBarringStatus_t*)inMsg->dataBuf;

	MSG_LOGV("Cause =", theCbStatusPtr->netCause);

	if (theCbStatusPtr->netCause == GSMCAUSE_SUCCESS)
	{
		theCbClassPtr = theCbStatusPtr->ss_activation_class_info;

		for (theIndex = 0; theIndex < theCbStatusPtr->class_size; theIndex++)
		{
			theAtcClass = SuppSvcClassToATCClass(theCbClassPtr->ss_class);

			sprintf(theStrBuf, "%s: %d,%d",
					theCmdPtr, theCbClassPtr->activated, theAtcClass);

			AT_OutputStr(theChannel, (UInt8 *) theStrBuf);
			theCbClassPtr++;
		}

		AT_CmdRspOK(theChannel);
	}
	else
	{
		AT_CmdRspError(	theChannel,
						ConvertNWcause2CMEerror(theCbStatusPtr->netCause));
	}
}

//******************************************************************************
// Function Name:	AT_HandleCallWaitingStatusRsp
//
// Description:		Handle the call waiting status queried from the network.
//******************************************************************************
void AT_HandleCallWaitingStatusRsp(InterTaskMsg_t *inMsg)
{
	UInt8						theIndex;
	UInt8 						theChannel;
	UInt8						theLoopCounter = 0;
	ATC_SERVICE_CLASS_t			theAtcClass;
	SS_ActivationStatus_t*		theActStatusPtr;
	SS_ActivationClassInfo_t*	theActClassPtr;
	char						theStrBuf[MAX_RESPONSE_LEN + 1];
	const UInt8*				theCmdPtr = AT_GetCmdName(AT_CMD_CCWA);


	SsApi_ResetSsAlsFlag();

	// Only send out the result to the host if we can get a valid
	// channel: if the command is initiated from AT port and not MMI.
	if (!SuppSvcGetV24Chan(&theChannel)) return;

	theActStatusPtr = (SS_ActivationStatus_t*)inMsg->dataBuf;

	MSG_LOGV("AT_HandleCallWaitingStatusRsp()   cause =", theActStatusPtr->netCause);

	if (theActStatusPtr->netCause == GSMCAUSE_SUCCESS)
	{
		theActClassPtr = theActStatusPtr->ss_activation_class_info;

		for (theIndex = 0; theIndex < theActStatusPtr->class_size; theIndex++)
		{
			theAtcClass = SuppSvcClassToATCClass(theActClassPtr->ss_class);

			sprintf(theStrBuf, "%s: %d,%d",
					theCmdPtr, theActClassPtr->activated, theAtcClass);

			AT_OutputStr(theChannel, (UInt8 *) theStrBuf);
			theActClassPtr++;
		}

		AT_CmdRspOK(theChannel);
	}
	else
	{
		AT_CmdRspError(	theChannel,
						ConvertNWcause2CMEerror(theActStatusPtr->netCause));
	}
}


//-----------------------------------------------------------------------------
//	ATCmd_CCFC_Handler - called by AT parser to handle the +CCFC command
//-----------------------------------------------------------------------------
AT_Status_t ATCmd_CCFC_Handler (
	AT_CmdId_t cmdId,
	UInt8 chan,
	UInt8 accessType,
	const UInt8* _P1,
	const UInt8* _P2,
	const UInt8* _P3,
	const UInt8* _P4,
	const UInt8* _P5,
	const UInt8* _P6,
	const UInt8* _P7,
	const UInt8* _P8 )
{
	UInt8				clientID = AT_GetV24ClientIDFromMpxChan( chan );
	SS_Mode_t			mode;
	SS_CallFwdReason_t	reason;
	SS_SvcCls_t			svcCls;
	UInt8				waitToFwdSec;
	UInt8				number[22];	//Note: GSM Spec 04.08,  10.5.4.7
									//The called party BCD number is a type 4
									//information element with a minimum length
									//of 3 octets and a maximum length of 43
									//octets. For PCS 1900 the maximum length
									//is 19 octets.

	switch( accessType ) {

	case AT_ACCESS_REGULAR:

		//	check network status
		if( SYS_GetGSMRegistrationStatus() != REGISTERSTATUS_NORMAL ) {
			AT_CmdRspError( chan, AT_CME_ERR_NO_NETWORK_SVC ) ;
			return AT_STATUS_DONE ;
		}

		mode   = SS_MODE_NOTSPECIFIED ;
		reason = SS_CALLFWD_REASON_NOTSPECIFIED ;
		svcCls = SS_SVCCLS_NOTSPECIFIED ;
		waitToFwdSec = 0 ;
		number[0] = '\0';

		//	MODE
		if( _P2 ) {

			switch( atol( (char*)_P2) ) {

			case 0 :
				mode = SS_MODE_DISABLE ;
				break;

			case 1 :
				mode = SS_MODE_ENABLE;
				break;

			case 2 :
				mode = SS_MODE_INTERROGATE ;
				break;

			case 3 :
				mode = SS_MODE_REGISTER ;
				break;

			case 4 :
				mode = SS_MODE_ERASE ;
				break;

			default:
				AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
				return AT_STATUS_DONE ;
			}
		}

		else {
			AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
			return AT_STATUS_DONE ;
		}

		//	REASON
		if( _P1 ) {

			switch( atol( (char*)_P1 ) ) {

			case 0 :
				reason = SS_CALLFWD_REASON_UNCONDITIONAL ;
				break ;

			case 1 :
				reason = SS_CALLFWD_REASON_BUSY ;
				break ;

			case 2 :
				reason = SS_CALLFWD_REASON_NO_REPLY ;
				break ;

			case 3 :
				reason = SS_CALLFWD_REASON_NOT_REACHABLE ;
				break ;

			case 4 :
				reason = SS_CALLFWD_REASON_ALL_CF ;
				break ;

			case 5 :
				reason = SS_CALLFWD_REASON_ALL_CONDITIONAL ;
				break ;

			default:
				AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
				return AT_STATUS_DONE ;
			}
		}

		else {
			AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
			return AT_STATUS_DONE ;
		}

		//	NUMBER
		if( _P3 )
		{
			//Make sure the passed number can be converted to packed BCD format
			if( !BCD_CheckString2PBCD((UInt8*)_P3) )
			{
				AT_CmdRspError( chan, AT_CME_ERR_INVALID_DIALSTR_CHAR ) ;
				return AT_STATUS_DONE ;
			}

			if( _P3[0] == INTERNATIONAL_CODE )
			{
				number[0] = INTERNATIONAL_CODE;
			}
			else if ( (_P4 != NULL) && (atol((char *) _P4) == INTERNA_TON_ISDN_NPI) )
			{
				/* Insert '+' to the beginning of forwarded-to-number */
				number[0] = INTERNATIONAL_CODE;
			}

			strncpy( (number[0] == INTERNATIONAL_CODE) ? (char *) number + 1 : (char *) number,
					 (_P3[0] == INTERNATIONAL_CODE) ? (char *) _P3 + 1 : (char *) _P3,
					 sizeof(number) - 1 );

			number[sizeof(number) - 1] = '\0';

			if (mode == SS_MODE_ENABLE)
			{
				mode = SS_MODE_REGISTER;
			}
		}

		//	SERVICE CLASS: convert AT parameter
		// refer GSM 2.30_6.10 for codes
		// IWD Diana spec says case 2 should be all bearer service
		if( _P5 ) {

			switch( atol( (char*)_P5 ) ) {

			case   1 :
				svcCls = (SIM_GetAlsDefaultLine() == 0) ? SS_SVCCLS_SPEECH : SS_SVCCLS_ALT_SPEECH;
				break;

			case   2 :
				svcCls = SS_SVCCLS_DATA ;
				break;

			case   4 :
				svcCls = SS_SVCCLS_FAX ;
				break;

			case   5 :
				svcCls = SS_SVCCLS_ALL_TELE_SERVICES ;
				break;

			case   7 :
				svcCls = SS_SVCCLS_NOTSPECIFIED ;
				break;

			case   8 :
				svcCls = SS_SVCCLS_SMS ;
				break;

			case  16 :
				svcCls = SS_SVCCLS_DATA_CIRCUIT_SYNC ;
				break;

			case  32 :
				svcCls = SS_SVCCLS_DATA_CIRCUIT_ASYNC ;
				break;

			case  64 :
				svcCls = SS_SVCCLS_DEDICATED_PACKET ;
				break;

			case  80 :
				svcCls = SS_SVCCLS_ALL_SYNC_SERVICES ;
				break;

			case 128 :
				svcCls = SS_SVCCLS_DEDICATED_PAD ;
				break;

			case 135 :
				break;	// Certain PC s/w sends this. No code in GSM spec.

			case 160 :
				svcCls = SS_SVCCLS_ALL_ASYNC_SERVICES ;
				break;

			case 240 :
				svcCls = SS_SVCCLS_ALL_BEARER_SERVICES ;
				break;

			default  :
				AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
				return AT_STATUS_DONE ;
			}
		}

		//	WAIT-TO-FORWARD
		if( _P8 ) {
			waitToFwdSec = atol( (char*)_P8 ) ;

			if ( waitToFwdSec > 30 || waitToFwdSec == 0 ) {
				AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
				return AT_STATUS_DONE ;
			}
		}
		return(SS_ProcessCmdResult(	chan,
									SS_SendCallForwardReq(	clientID,
															mode,
															reason,
															svcCls,
															waitToFwdSec,
															number)));

	case AT_ACCESS_READ:
	case AT_ACCESS_TEST:
	default:
		AT_CmdRspSyntaxError(chan) ;
		return AT_STATUS_DONE ;
	}
}

//-----------------------------------------------------------------------------
//	ATCmd_CCWA_Handler - called by AT parser to handle the +CCWA command
//-----------------------------------------------------------------------------

AT_Status_t ATCmd_CCWA_Handler (
	AT_CmdId_t cmdId,
	UInt8 chan,
	UInt8 accessType,
	const UInt8* _P1,
	const UInt8* _P2,
	const UInt8* _P3 )
{
	UInt8		clientID = AT_GetV24ClientIDFromMpxChan( chan );
	SS_Mode_t	mode;
	SS_SvcCls_t	svcCls = SS_SVCCLS_NOTSPECIFIED;
	char		strBuf [MAX_RESPONSE_LEN] ;
	MsConfig_t*	msCfg = MS_GetCfg( );
	UInt8		temp_CCWA = msCfg->CCWA;

	switch( accessType ) {

	case AT_ACCESS_REGULAR:

		//	P1 required if any other parameter specified
		if(!_P1) {
			AT_CmdRspSyntaxError( chan ) ;
			return AT_STATUS_DONE ;
		}
		else {
			msCfg->CCWA = atol((char*)_P1);

			if (!((msCfg->CCWA == 0x01) || (msCfg->CCWA == 0x00)))
			{
				msCfg->CCWA = temp_CCWA;
				AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
				return AT_STATUS_DONE ;
			}
		}

		if ( _P2 ) {
			switch( atol( (char*)_P2 ) ) {

			case 0 :
				mode = SS_MODE_DISABLE ;
				break ;

			case 1 :
				mode = SS_MODE_ENABLE ;
				break ;

			case 2 :
				mode = SS_MODE_INTERROGATE ;
				break ;

			default:
				msCfg->CCWA = temp_CCWA;
				AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
				return AT_STATUS_DONE ;
			}

			if ( SYS_GetGSMRegistrationStatus() != REGISTERSTATUS_NORMAL ) {
				AT_CmdRspError( chan, AT_CME_ERR_NO_NETWORK_SVC ) ;
				return AT_STATUS_DONE ;
			}

		}
		else
		{
			AT_CmdRspOK( chan ) ;
			return AT_STATUS_DONE ;
		}

		//
		//	SERVICE CLASS: convert AT parameter
		//
		// refer GSM 2.30_6.10 for codes
		// IWD Diana spec says case 2 should be all bearer service
		//
		if( _P3 ) {

			switch( atol( (char*)_P3 ) ) {

			case   1 :
				svcCls = (SIM_GetAlsDefaultLine() == 0) ? SS_SVCCLS_SPEECH : SS_SVCCLS_ALT_SPEECH ;
				break;

			case   2 :
				svcCls = SS_SVCCLS_DATA ;
				break;

			case   4 :
				svcCls = SS_SVCCLS_FAX ;
				break;

			case   5 :
				svcCls = SS_SVCCLS_ALL_TELE_SERVICES ;
				break;

			case   7 :
				svcCls = SS_SVCCLS_NOTSPECIFIED ;
				break;

			case   8 :
				svcCls = SS_SVCCLS_SMS ;
				break;

			case  16 :
				svcCls = SS_SVCCLS_DATA_CIRCUIT_SYNC ;
				break;

			case  32 :
				svcCls = SS_SVCCLS_DATA_CIRCUIT_ASYNC ;
				break;

			case  64 :
				svcCls = SS_SVCCLS_DEDICATED_PACKET ;
				break;

			case  80 :
				svcCls = SS_SVCCLS_ALL_SYNC_SERVICES ;
				break;

			case 128 :
				svcCls = SS_SVCCLS_DEDICATED_PAD ;
				break;

			case 135 :
				//break;	// Certain PC s/w sends this. No code in GSM spec.

			case 160 :
				//svcCls = SS_SVCCLS_ALL_ASYNC_SERVICES ;
				//break;

			case 240 :
				//svcCls = SS_SVCCLS_ALL_BEARER_SERVICES ;
				//break;

			default  :
				msCfg->CCWA = temp_CCWA;
				AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED ) ;
				return AT_STATUS_DONE ;
			}
		}

		return(SS_ProcessCmdResult(	chan,
									SS_SendCallWaitingReq(clientID, mode, svcCls)));

	case AT_ACCESS_READ:
		sprintf( strBuf, "%s: %d", AT_GetCmdName( cmdId ), msCfg->CCWA );
		AT_OutputStr( chan, (UInt8*)strBuf ) ;
		AT_CmdRspOK( chan ) ;
		return AT_STATUS_DONE ;

	case AT_ACCESS_TEST:
	default:
		AT_CmdRspSyntaxError(chan) ;
		return AT_STATUS_DONE ;
	}
}

//******************************************************************************
//
// Function Name:	AT_HandleIntParamSetIndMsg
//
// Description:		Handle the the response to the local setting response.
//
//******************************************************************************
void AT_HandleIntParamSetIndMsg(InterTaskMsg_t* inMsgPtr)
{
	UInt8				theChan;
	SS_IntParSetInd_t*	theIntParSetIndPtr = (SS_IntParSetInd_t*)inMsgPtr->dataBuf;

	if (!SuppSvcGetV24Chan(&theChan))
	{
		return;
	}

	switch(theIntParSetIndPtr->ssSvcType)
	{
		case SUPPSVCTYPE_CLIP:
			MS_GetCfg()->CLIP = theIntParSetIndPtr->cfgParamValue;
			break;

		case SUPPSVCTYPE_CLIR:
			MS_GetCfg()->CLIR = theIntParSetIndPtr->cfgParamValue;
			break;

		case SUPPSVCTYPE_COLP:
			 MS_GetCfg()->COLP = theIntParSetIndPtr->cfgParamValue;
			break;

		default:
			AT_CmdRspError(theChan, AT_CME_ERR_RSP);
			return;
	}

	AT_CmdRspOK(theChan);
}

//******************************************************************************
// Function Name:	Handle_CLIR_CLIP_COLP
// Description:		.
//******************************************************************************
static AT_Status_t Handle_CLIR_CLIP_COLP(	AT_CmdId_t		inCmdId,
											UInt8			inChan,
											UInt8			inAccessType,
											const UInt8*	inParamPtr,
											SS_ApiReq_t		inSsApiReq)
{
	char		theStrBuf[MAX_RESPONSE_LEN];
	UInt8		theClientId = AT_GetV24ClientIDFromMpxChan(inChan);
	MsConfig_t*	theMsCfgPtr = MS_GetCfg();
	SS_Mode_t	theMode;

	if (inAccessType == AT_ACCESS_REGULAR)
	{
		if (inParamPtr)
		{
			switch (inSsApiReq)
			{
				case SS_API_CLIP_SERVICE_REQUEST:
				case SS_API_COLP_SERVICE_REQUEST:
					theMode = (atol((char*)inParamPtr) == 0) ? SS_MODE_DISABLE : SS_MODE_ENABLE;
					break;

				case SS_API_CLIR_SERVICE_REQUEST:
					switch (atol((char*)inParamPtr))
					{
						case 0x00:
							theMode = SS_MODE_ERASE;
							break;
						case  0x01:
							theMode = SS_MODE_ENABLE;
							break;
						case  0x02:
							theMode = SS_MODE_DISABLE;
							break;
					}
					break;
			}

			return(SS_ProcessCmdResult(	inChan,
										SsApi_SendSsCallReq(theClientId,
															inSsApiReq,
															theMode,
															SS_CALLFWD_REASON_NOTSPECIFIED,
															SS_SVCCLS_NOTSPECIFIED,
															0x00,
															NULL,
															SS_CALLBAR_TYPE_NOTSPECIFIED,
															NULL,
															NULL,
															NULL)));
		}
	}
	else if (inAccessType == AT_ACCESS_READ)
	{
		//	If there's no network service, return 2: unknown network
		if (SYS_GetGSMRegistrationStatus() != REGISTERSTATUS_NORMAL)
		{
			switch (inSsApiReq)
			{
				case SS_API_CLIP_SERVICE_REQUEST:
					sprintf((char*)theStrBuf, "%s: %d,2", AT_GetCmdName(inCmdId), theMsCfgPtr->CLIP);
					break;
				case SS_API_COLP_SERVICE_REQUEST:
					sprintf((char*)theStrBuf, "%s: %d,2", AT_GetCmdName(inCmdId), theMsCfgPtr->COLP);
					break;
				case SS_API_CLIR_SERVICE_REQUEST:
					sprintf((char*)theStrBuf, "%s: %d,2", AT_GetCmdName(inCmdId), theMsCfgPtr->CLIR);
					break;
			}
			AT_OutputStr(inChan, (UInt8*)theStrBuf);
			AT_CmdRspOK(inChan);
			return(AT_STATUS_DONE);
		}

		switch (inSsApiReq)
		{
			case SS_API_CLIP_SERVICE_REQUEST:
				return(SS_ProcessCmdResult(	inChan,
											SS_QueryCallingLineIDStatus(theClientId)));
				break;
			case SS_API_COLP_SERVICE_REQUEST:
				return(SS_ProcessCmdResult(	inChan,
											SS_QueryConnectedLineIDStatus(theClientId)));
				break;
			case SS_API_CLIR_SERVICE_REQUEST:
				return(SS_ProcessCmdResult(	inChan,
											SS_QueryCallingLineRestrictionStatus(theClientId)));
		}
	}

	AT_CmdRspSyntaxError(inChan);

	return(AT_STATUS_DONE);
}

//******************************************************************************
// Function Name:	ATCmd_CLIP_Handler
// Description:		called by AT parser to handle the +CLIP command.
//******************************************************************************
AT_Status_t ATCmd_CLIP_Handler(AT_CmdId_t		cmdId,
								UInt8			chan,
								UInt8			accessType,
								const UInt8*	_P1 )
{
	return (Handle_CLIR_CLIP_COLP(	cmdId,
									chan,
									accessType,
									_P1,
									SS_API_CLIP_SERVICE_REQUEST));
}

//-----------------------------------------------------------------------------
//	ATCmd_CLIR_Handler - called by AT parser to handle the +CLIR command
//-----------------------------------------------------------------------------
AT_Status_t ATCmd_CLIR_Handler(	AT_CmdId_t		cmdId,
								UInt8 			chan,
								UInt8			accessType,
								const UInt8*	_P1)
{
	return (Handle_CLIR_CLIP_COLP(	cmdId,
									chan,
									accessType,
									_P1,
									SS_API_CLIR_SERVICE_REQUEST));
}

//-----------------------------------------------------------------------------
//	ATCmd_COLP_Handler - called by AT parser to handle the +COLP command
//-----------------------------------------------------------------------------
AT_Status_t ATCmd_COLP_Handler(	AT_CmdId_t		cmdId,
								UInt8			chan,
								UInt8			accessType,
								const UInt8*	_P1)
{
	return (Handle_CLIR_CLIP_COLP(	cmdId,
									chan,
									accessType,
									_P1,
									SS_API_COLP_SERVICE_REQUEST));
}

//-----------------------------------------------------------------------------
//	ATCmd_CAOC_Handler - called by AT parser to handle the +CAOC command
//-----------------------------------------------------------------------------
AT_Status_t ATCmd_CAOC_Handler (AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const UInt8* _P1 )
{
	UInt8			atRes[MAX_RESPONSE_LEN+1];
	const UInt8*	cmdName = AT_GetCmdName( AT_CMD_CAOC ) ;

	if( AT_ACCESS_REGULAR == accessType ) {

		switch( atoi( (char*)_P1 ) ) {

		case 0: // query CCM value
			sprintf( (char*)atRes, "%s: \"%06lx\"", cmdName,  curCallCfg.CCM );
			AT_OutputStr( chan, atRes ) ;
			AT_CmdRspOK( chan ) ;
			break ;

		case 1: // deactivate the unsolicited reporting of CCM value
			AT_GetCommonCfg()->Unsoliciated_CCMReport = FALSE;
			AT_CmdRspOK( chan ) ;
			break ;

		case 2: // activate the unsolicited reporting of CCM value
			AT_GetCommonCfg()->Unsoliciated_CCMReport = TRUE;
			AT_CmdRspOK( chan ) ;
			break ;

		default:
			AT_CmdRspError( chan, AT_CME_ERR_RSP );
			break ;
		}
	}

	else { // ( AT_ACCESS_READ == accessType )
		sprintf( (char*)atRes, "%s: %d", cmdName, AT_GetCommonCfg()->Unsoliciated_CCMReport+1 );
		AT_OutputStr( chan, atRes ) ;
		AT_CmdRspOK( chan ) ;
	}

	return AT_STATUS_DONE ;
}

/*  Implemented by Abdi for feature use
//-----------------------------------------------------------------------------
//	ATCmd_CCUG_Handler - called by AT parser to handle the +CCUG command
//-----------------------------------------------------------------------------

AT_Status_t ATCmd_CCUG_Handler (AT_CmdId_t		cmdId,
								UInt8			chan,
								UInt8			accessType,
								const UInt8*	_P1,
								const UInt8*	_P2,
								const UInt8*	_P3 )
{
	UInt8			theCugMode;
	UInt8			theCugIndex;
	UInt8			theCugInfo = 0;
	UInt8			atRes[MAX_RESPONSE_LEN+1];
	const UInt8*	cmdName = AT_GetCmdName( AT_CMD_CCUG ) ;

	if (AT_ACCESS_REGULAR == accessType)
	{
		if (_P1 && _P2)
		{
			theCugMode  = atoi((char*)_P1);
			theCugIndex = atoi((char*)_P2);

			if (_P3)
			{
				theCugInfo = atoi((char*)_P3);
			}

			if ((theCugMode < 2) && (theCugIndex < 10) && (theCugInfo < 4))
			{
				AT_CmdRspOK( chan ) ;
			}
		}
	}
	else if (AT_ACCESS_READ == accessType )
	{
		sprintf( (char*)atRes, "+%s: %d", cmdName, AT_GetCommonCfg()->Unsoliciated_CCMReport+1 );
		AT_OutputStr( chan, atRes ) ;
		AT_CmdRspOK( chan ) ;
	}

	AT_CmdRspError( chan, AT_CME_ERR_RSP );
	return AT_STATUS_DONE ;
}

*/




//-----------------------------------------------------------------------------
//	ATCmd_CCWE_Handler - called by AT parser to handle the +CCWE command
//-----------------------------------------------------------------------------

AT_Status_t ATCmd_CCWE_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const UInt8* _P1 )
{
	UInt8 atRes[MAX_RESPONSE_LEN+1];

	if ( accessType == AT_ACCESS_REGULAR ) {
		switch( atoi( (char*)_P1 ) ) {
		case 0:
			AT_GetCommonCfg()->CallMeterWarning = FALSE;
			AT_CmdRspOK( chan ) ;
			break;
		case 1:
			AT_GetCommonCfg()->CallMeterWarning = TRUE;
			AT_CmdRspOK( chan ) ;
			break;
		default:
			AT_CmdRspError( chan, AT_CME_ERR_RSP );
			break;
		}
	}

	else if (accessType == AT_ACCESS_READ ) {
		sprintf( (char*)atRes, "%s: %d", AT_GetCmdName( AT_CMD_CCWE),
			AT_GetCommonCfg()->CallMeterWarning );
		AT_OutputStr( chan, atRes ) ;
		AT_CmdRspOK( chan ) ;
	}

	return AT_STATUS_DONE ;
}

//******************************************************************************
//
// Function Name:	ATC_Cusd_CB
//
// Description:		Process AT+CUSD command (USSD)
//
// Notes:
//******************************************************************************
AT_Status_t ATCmd_CUSD_Handler (
								AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const UInt8* _P1,
								const UInt8* _P2,
								const UInt8* _P3 )
{
	UInt8	dcs;
	UInt8	len = 0;
	char	tBuf[PHASE1_MAX_USSD_STRING_SIZE+1];
	UInt8	clientID = AT_GetV24ClientIDFromMpxChan( chan );

	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg(chan);

	_TRACE(("AT USSD: ussd_id = ", ussd_id));
	_TRACE(("AT USSD: isUssdTypeMT = ", isUssdTypeMT));

	if(accessType == AT_ACCESS_REGULAR)
	{
		if( (_P1 != NULL) && (atoi((char*)_P1) < 2) ){
			ussd_chan = chan;
			channelCfg->CUSD = atoi((char*)_P1);
		}

		if(_P2 != NULL)
		{
			strcpy( (char *)tBuf, (char *)_P2 );
			len = strlen(tBuf);

			// We only need to validate the USSD string for USSD request (not response)
			if(ussd_id == CALLINDEX_INVALID)
			{
				UInt8  i = 0;
/*
				// validate USSD string
				if( (len > 2) && (tBuf[len-1] != '#') )
				{
					_TRACE(("AT USSD: validate USSD string failed1 "));
      				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}

				while(i < len-1)
				{
					if( ((tBuf[i] < '0' || tBuf[i] > '9')) && ((tBuf[i] != '#') && (tBuf[i] != '*')) )
					{
						_TRACE(("AT USSD: validate USSD string failed2 "));
      					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
						return AT_STATUS_DONE;
					}
					i++;
				}
*/
			}
		}
		else{
			tBuf[0] = '\0';

			if(atoi((char*)_P1) < 2){
				AT_CmdRspOK(chan) ;
				return AT_STATUS_DONE;
			}
		}

		dcs = (_P3 == NULL) ? 0x0F : atoi((char*)_P3);

		if( atoi((char*)_P1) == 2)
		{
			if(ussd_id != CALLINDEX_INVALID)
			{
				// cancel the session
				if( SS_EndUSSDConnectReq(clientID, ussd_id) != RESULT_OK ){
					_TRACE(("AT USSD: SS_EndUSSDConnectReq failed "));
   					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}
				else{
					ussd_id = CALLINDEX_INVALID;

					if(previousUssdId != CALLINDEX_INVALID){
						ussd_id = previousUssdId;
						previousUssdId = CALLINDEX_INVALID;
					}
					AT_CmdRspOK(chan) ;
					return AT_STATUS_DONE;
				}
			}
			else  // there's no on-going ussd session, can not do cancel
			{
				_TRACE(("USSD: CUSD=2 not allowed because there's no on-going ussd session"));
   				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}
		}
		else
		{
			if(ussd_id == CALLINDEX_INVALID)
			{
				if(_P2 != NULL)	// USSD request
				{
					USSDString_t ussd;
					UInt8 len;
					Result_t res;

					len = strlen((char*)tBuf);
					ussd.used_size = (len > PHASE2_MAX_USSD_STRING_SIZE)? PHASE2_MAX_USSD_STRING_SIZE:len;
					memcpy((void*)ussd.string, (void*)tBuf, ussd.used_size );
					ussd.dcs = dcs;

					res = SS_SendUSSDConnectReq( clientID, &ussd );
					if( res != RESULT_OK ){
						_TRACE(("AT USSD: SS_SendUSSDConnectReq Failed: res = ", res));

						if (res == STK_DATASVRC_BUSY)
						{
							_TRACE(("AT USSD: SS_SendUSSDConnectReq STK data service busy"));
   							AT_CmdRsp( chan, AT_BUSY_RSP );

						} else
						{
							_TRACE(("AT USSD: SS_SendUSSDConnectReq Failed"));
   							AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
						}
						return AT_STATUS_DONE;
					}
					else{
						return AT_STATUS_PENDING ;
					}
				}
				else
				{
					// just to enable/disable the result code representation in TA
					AT_CmdRspOK(chan) ;
					return AT_STATUS_DONE;
				}
			}
			else
			{
				if(_P2 != NULL)  // response to network request
				{
					if( SS_SendUSSDData(clientID, ussd_id, dcs, len, (UInt8 *)tBuf) != RESULT_OK ){
						_TRACE(("AT USSD: SS_SendUSSDData failed"));
   						AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
						return AT_STATUS_DONE;
					}
					else{
						if(isUssdTypeMT){
							_TRACE(("AT USSD: response to MT"));
							// If the response is to a MT USSD, then we consider the AT transaction as done.
							return AT_STATUS_DONE;
						}
						else{
							_TRACE(("AT USSD: response to MO"));
							return AT_STATUS_PENDING;
						}
					}
				}
				else			// response can not be empty
				{
					_TRACE(("USSD: response can not be empty: P2 == NULL!! "));
      				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}
			}
		}
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( tBuf, "%s: %d", AT_GetCmdName(cmdId), channelCfg->CUSD );
		AT_OutputStr (chan, (UInt8*)tBuf );

/*		//testing
		if(ussd_circular_index == 0){
			MNSSPRIM_RxTest_2();
			ussd_circular_index = 1;
		}
		else if(ussd_circular_index == 1){
			MNSSPRIM_RxTest_1();
			ussd_circular_index = 2;
		}
		else{
			MNSSPRIM_RxTest();
			ussd_circular_index = 0;
		}
*/
		AT_CmdRspOK(chan) ;
		return AT_STATUS_DONE;
	}

	return AT_STATUS_DONE;
}

//******************************************************************************
// Function Name:	AT_HandleSSCallIndexInd
//
// Description:
//******************************************************************************
void AT_HandleSSCallIndexInd(InterTaskMsg_t*inMsg)
{
	StkReportCallStatus_t *call_status = (StkReportCallStatus_t*)inMsg->dataBuf;

	if( (call_status->call_type == CALLTYPE_MTUSSDSUPPSVC) || (call_status->call_type == CALLTYPE_MOUSSDSUPPSVC)){

		if( (call_status->status == CALLSTATUS_MT_CI_ALLOC) && (ussd_id != CALLINDEX_INVALID) ){
			// if there's on-going ussd session, still allow MT USSD event to come in
			previousUssdId = ussd_id;
		}

		ussd_id = call_status->index;

		if(call_status->call_type == CALLTYPE_MTUSSDSUPPSVC) {
			_TRACE(("AT USSD: MT"));
			isUssdTypeMT = TRUE;
		}
		else{
			_TRACE(("AT USSD: MO"));
			isUssdTypeMT = FALSE;
		}
	}
}

void AT_HandleUssdSessionEndInd(InterTaskMsg_t* inMsg)
{
	CallIndex_t index = *(CallIndex_t*)inMsg->dataBuf;

	if(index == ussd_id){
		_TRACE(("AT_SS: AT_HandleUssdSessionEndInd: ussd_id == released index"));
		ussd_id = CALLINDEX_INVALID;
	}
	else{
		_TRACE(("AT_SS: !AT_HandleUssdSessionEndInd: ussd_id =", ussd_id));
	}

	if(previousUssdId != CALLINDEX_INVALID){
		ussd_id = previousUssdId;
		previousUssdId = CALLINDEX_INVALID;
	}
}



//******************************************************************************
// Function Name:	AT_HandleSsCallReqFail
//
// Description:		Process SS related procedure failure.
//******************************************************************************
void AT_HandleSsCallReqFail(InterTaskMsg_t* inMsg)
{
	UInt8 theChannel = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	UInt16 errorCode;

	switch ( ((SsCallReqFail_t*) inMsg->dataBuf)->result )
	{
		case SS_OPERATION_IN_PROGRESS:
			errorCode = AT_CME_ERR_AT_BUSY;
			break;

		case SS_INVALID_SS_REQUEST:
			errorCode = AT_CME_ERR_OP_NOT_ALLOWED;
			break;

		case SS_FDN_BLOCK_SS_REQUEST:
			errorCode = AT_CMS_FDN_NOT_ALLOWED;
			break;

		default:
			errorCode = AT_CME_ERR_PHONE_FAILURE;
			break;
	}

	AT_CmdRspError(theChannel, errorCode);

	if(errorCode != AT_CME_ERR_AT_BUSY)
	{
		/* Reset client ID if there is no ongoing SS operation */
		SS_ResetClientID();
	}
}


//-----------------------------------------------------------------------------
// Name:         ATCmd_MCNAP_Handler
// Description:  This function is called by the AT command parser to handle
//               the *MCNAP command. ("Calling NAme Presentation")
//               This supplementary service provides for the ability to indicate
//               the name information of the callling party to the called party
//               at call setup time for all incoming calls.
//               The calling party takes no action to activate, initiate, or in
//               any manner provide CAlling Name Identification Presentation.
//               However, the delivery of the calling name to the called party
//               may be affected by other services subscribed to by the calling
//               party. For example, if the calling party has subscribed to
//               CAlling Line Identification Restriction (CLIR), then the
//               calling line identity as well as the calling anme identity
//               shall not be presented to the callied party.
//               For more information on CNAP, refer to 3G TS 22.096
//-----------------------------------------------------------------------------
AT_Status_t ATCmd_MCNAP_Handler(AT_CmdId_t cmdId,
                                UInt8      chan,
                                UInt8      accessType )
{
	UInt8	clientID = AT_GetV24ClientIDFromMpxChan(chan);

	switch (accessType)
	{
		case AT_ACCESS_READ:
			if (SYS_GetGSMRegistrationStatus() != REGISTERSTATUS_NORMAL)
			{
				AT_CmdRspError(chan, AT_CME_ERR_NO_NETWORK_SVC);
				return AT_STATUS_DONE;
			}
			return(SS_ProcessCmdResult(	chan,
										SS_QueryCallingNAmePresentStatus(clientID)));

		default:
			AT_CmdRspSyntaxError(chan);
			return(AT_STATUS_DONE);
	}
}


//-----------------------------------------------------------------------------
// Name:         ATCmd_MCOLR_Handler
// Description:  This function is called by the AT command parser to handle
//               the *MCOLR command. For the network response, refer to
//				 HandleProvisionStatusRsp();
//				 AT*MCOLR?
//				 *MCOLR: <n>
//				 OK
//				 <n>
//				 0 - COLR service inactive
//				 1 - COLR service active
//-----------------------------------------------------------------------------
AT_Status_t ATCmd_MCOLR_Handler(AT_CmdId_t cmdId,
                                UInt8      chan,
                                UInt8      accessType )
{
	UInt8 clientID = AT_GetV24ClientIDFromMpxChan(chan);

	switch (accessType)
	{
		case AT_ACCESS_READ:
			if (SYS_GetGSMRegistrationStatus() != REGISTERSTATUS_NORMAL)
			{
				AT_CmdRspError(chan, AT_CME_ERR_NO_NETWORK_SVC);
				return AT_STATUS_DONE;
			}
			return(SS_ProcessCmdResult(	chan,
										SS_QueryConnectedLineRestrictionStatus(clientID)));

		default:
			AT_CmdRspSyntaxError(chan);
			return(AT_STATUS_DONE);
	}
}

//-----------------------------------------------------------------------------
//	ATCmd_LOG_Handler - called by AT parser to handle the LOG command
//-----------------------------------------------------------------------------
AT_Status_t ATCmd_LOG_Handler (
								AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const UInt8* _P1 )
{
	UInt8 atRes[MAX_RESPONSE_LEN+1];

	if( AT_ACCESS_REGULAR == accessType ) {
		AT_GetCommonCfg()->logOn = atoi((char*)_P1);

		if (AT_GetCommonCfg()->logOn) {

			if (AT_GetCommonCfg()->presetAllowTrace){
				V24_SetLogChannelOn(TRUE,(DLC_t)chan);
				SDLTRACE_AllowSDLTrace();
			}else
				SDLTRACE_DisallowSDLTrace();
		}

		else {

			if ( MPX_ObtainMuxMode() != MUX_MODE )
				SDLTRACE_DisallowSDLTrace();

			V24_SetLogChannelOn(FALSE,INVALID_MPX_CHAN);
		}

		AT_CmdRspOK( chan ) ;
	}
	else if ( AT_ACCESS_READ == accessType )
	{
		sprintf( (char*)atRes, "%s: %d", AT_GetCmdName( AT_CMD_LOG), AT_GetCommonCfg()->logOn );
		AT_OutputStr( chan, atRes ) ;
		AT_CmdRspOK( chan ) ;
	}

	return AT_STATUS_DONE ;
}


/************************************************************************************
*
*	Function Name:	ATCmd_MQCFI_Handler
*
*	Description:	This function handles the AT*MQCFI command
*					(Query Call Forward Unconditional Indication status in SIM/USIM).
*
*************************************************************************************/
AT_Status_t ATCmd_MQCFI_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	if (accessType == AT_ACCESS_TEST)
	{
		AT_CmdRspOK(chan);
	}
	else
	{
		char temp_buffer[40];
		Boolean sim_file_exist;
		SIM_CALL_FORWARD_UNCONDITIONAL_FLAG_t call_forward_flag = SIM_GetCallForwardUnconditionalFlag(&sim_file_exist);

		if (sim_file_exist)
		{
			sprintf( temp_buffer, "*MQCFI: %d, %d, %d, %d", call_forward_flag.call_forward_l1, call_forward_flag.call_forward_l2,
						call_forward_flag.call_forward_fax, call_forward_flag.call_forward_data	);

			AT_OutputStr(chan, (UInt8 *) temp_buffer);
			AT_CmdRspOK(chan);
		}
		else
		{
			AT_CmdRspError(chan, AT_CME_ERR_NOT_FOUND);
		}
	}

	return AT_STATUS_DONE;
}

