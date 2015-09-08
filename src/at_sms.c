/**
*
*   @file   at_sms.c
*
*   @brief  This file contains the AT functions for short message services.
*
****************************************************************************/

#define __STORE_SUBMITED_SMS__ 0

#include <stdio.h>
#include <stdlib.h>
#include "at_util.h"
#include "at_cfg.h"
#include "at_utsm.h"
#include "atc.h"
#include "sim_api.h"
#include "v24.h"
#include "util.h"
#include "osheap.h"
#include "log.h"


// for new AT API
#include "at_api.h"
#include "taskmsgs.h"
#include "ms_database.h"

#include "smsme.h"
/***************************
 Private data
***************************/

// the max str length is defined based on max of 160 characters for a single sms.
#define MAX_SMS_STR_LENGTH_FOR_CMGS		370
#define MAX_CMGS_Px_STR_LENGTH			30

typedef struct
{
SIMSMSMesgStatus_t SMSStatus;
UInt8              *StateFullName;
} SMSStateFullName_t;

typedef struct
{
 UInt8  da [SMS_MAX_DIGITS];
 UInt8  toda;
 UInt8  Length;
 UInt8  Status;
 UInt8  RecNo;
 UInt16 CmdId;
 UInt8  Chan;
 UInt8  Txt[MAX_SMS_STR_LENGTH_FOR_CMGS];
} atc_Cmgs_Ctx_t;

typedef struct
{
	AT_CmdId_t cmdId;
	UInt8 chan;
	UInt8 accessType;
	UInt8 _P1[MAX_CMGS_Px_STR_LENGTH];
	UInt8 _P2[MAX_CMGS_Px_STR_LENGTH];
	UInt8 _P3[MAX_CMGS_Px_STR_LENGTH];
} at_cmgsCmd_t;

const char *MemStr [NB_MAX_STORAGE] =
                   {"\"SM\"", "\"BM\"",  "\"ME\"", "\"MT\"", "\"TA\"", "\"SR\"" };
const char *MemStrNC [NB_MAX_STORAGE] =
                   {"SM", "BM",  "ME", "MT", "TA", "SR" };

// Table StorageCapability indicates which storage among those available (see
// table MemStr) are implemented.
const UInt8 StorageCapability [NB_MAX_STORAGE] =
                   {TRUE,     TRUE,      TRUE,     TRUE,     FALSE,    TRUE };


/* Number of +Cnma command wait by atc sms state machine. */
UInt8 atc_SmsWaitedCnma = 0;
UInt8 atc_NextCmdIsCnma = 0;
UInt8 atc_SendRpAck     = 0;

static atc_Cmgs_Ctx_t* Cmgs_Ctxt = NULL;

static UInt8 lastSmsCmdChan = INVALID_MPX_CHAN;

static Boolean isRspForATWriteRequest = FALSE;
static Boolean isRspForATDeleteRequest = FALSE;

// message storage wait state
SmsStorageWaitState_t storageWaitState = SMS_STORAGE_WAIT_NONE;

// CB static variables
static UInt8 Mode = 0;

// voicemail indication
static Boolean isVMindSent = FALSE;


/********************* Static function declarations **************************/
static void CleanSmsResources(void);
static UInt16 AT_SmsErrorToAtError(UInt16 err, Boolean cmgfMode);
static void AT_ManageUnsolSmsStr(UInt8 ch, const UInt8* rspStr, Boolean chSpecific);
static Result_t AtFormatCurrentMidsString(char* midsStr);
static void AtFormatCurrentLangsString(char* langsStr);
static void AtReportStoredError(SIMAccess_t result);

void AtConvertTimeToString(UInt8* timeString, SmsAbsolute_t dateTime);


//******************************************************************************
//
// Function Name:	ATC_Csdh_CB
//
// Description:		Transition on AT+CSDH=0/1
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CSDH_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1){

	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,
								   FALSE,0,&GET_CSDH);

}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CSCA_Handler (ATC_Csca_CB)
//
// Description:         Transition on AT+CSCA
//                      Service Centre Number (SCA) is saved on SMSP EF in SIM.
// Notes::
//
//------------------------------------------------------------------------------

AT_Status_t ATCmd_CSCA_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1, const UInt8* _P2)
{
	Result_t res;
	UInt8	rspBuf[AT_RSPBUF_SM];
	UInt8	clientID = AT_GetV24ClientIDFromMpxChan(chan);
	UInt8	ton;
	SmsAddress_t sca;

	switch(accessType)
	{
	//----------------
	// Write SC address
	//----------------
	case AT_ACCESS_REGULAR:
		if(_P2){
			sca.TypeOfAddress = atoi((char*)_P2);
		}
		else{
			sca.TypeOfAddress = SMS_UNKNOWN_TOA;
		}

		strcpy((char*)sca.Number, (char*)_P1);
		res = SMS_SendSMSSrvCenterNumberUpdateReq(clientID, &sca, USE_DEFAULT_SCA_NUMBER);

		if(res != RESULT_OK){
			AT_CmdRspError(chan, AT_SmsErrorToAtError(res, GET_CMGF((DLC_t)chan)));
		}
		else{
			AT_CmdRspOK(chan);
		}

		return AT_STATUS_DONE;

		break;

	//----------------
	// Read SC address
	//----------------
	case AT_ACCESS_READ:
		{
			res = SMS_GetSMSrvCenterNumber(&sca, USE_DEFAULT_SCA_NUMBER);

			if(res != RESULT_OK)
			{
				AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY);
				return AT_STATUS_DONE;
			}
			else
			{
				if (ATC_GET_TON(sca.TypeOfAddress) & InternationalTON)
				{
					strcpy((char*)rspBuf, "+CSCA: \"+");
				}
				else
				{
					strcpy((char*)rspBuf, "+CSCA: \"");
				}

				sprintf((char*)&rspBuf[strlen((char *)rspBuf)], "%s\",%d", sca.Number, sca.TypeOfAddress);

				AT_OutputStr(chan, (UInt8*)rspBuf );
				AT_CmdRspOK(chan);
			}
		}
		break;

	case AT_ACCESS_TEST:
		sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CSCA), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, (UInt8*)rspBuf );
		AT_CmdRspOK(chan);
		break;

	default:
		break;
	}

	return AT_STATUS_DONE;
}


//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CSMS_Handler (ATC_Csms_CB)
//
// Description:         Select Message Service
//
// Notes:
//
//------------------------------------------------------------------------------

AT_Status_t ATCmd_CSMS_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	switch(accessType)
	{
		case AT_ACCESS_REGULAR:
			SET_CSMS(atoi((char*)_P1));

			if (GET_CSMS == 1)
			{
				atc_SmsWaitedCnma = 0;
				atc_NextCmdIsCnma = FALSE;
				atc_SendRpAck     = 0;
			}

			sprintf((char *)rspBuf, "%s:  1, 1, 1", AT_GetCmdName(AT_CMD_CSMS));
			break;

		case AT_ACCESS_READ:
			sprintf((char *)rspBuf, "%s: %d, 1, 1, 1", AT_GetCmdName(AT_CMD_CSMS), GET_CSMS);
			break;

		case AT_ACCESS_TEST:
			sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CSMS), AT_GetValidParmStr(cmdId));
			break;

		default:
			break;
	}

	AT_OutputStr (chan, (UInt8*)rspBuf );
	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//------------------------------------------------------------------------------
// Function Name:       ATCmd_MSMS_Handler (AT_CMD_MSMS)
//
// Description:         This AT Command is created just for SS specefic
//						application. This command is used to specify the SMS
//						client (e.g. ATC port or MMI application). Utilizing
//						this command the user is capable to switch between the
//						SMS clients. The default client for the SMS is ATC (0).
//						If the user wants to switch to MMI as a client the value
//						entered shall be 1.
//------------------------------------------------------------------------------

AT_Status_t ATCmd_MSMS_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8 rspBuf[AT_RSPBUF_SM];
	Boolean updateFlag = FALSE;
	Boolean clientFlag = (Boolean)(atoi((char*)_P1));

	switch (accessType)
	{
		case AT_ACCESS_REGULAR:
			if( MS_GetMtSmsClient() != clientFlag){
				updateFlag = TRUE;
				MS_SetMtSmsClient( clientFlag );
			}

			//If the client flag is switched to ATC, then initialize the database
			if (!MS_GetMtSmsClient()  && updateFlag)
			{
				//Init SMS SIM buffer database.
				SMS_SimInit();
			}
			break;

		case AT_ACCESS_READ:
			sprintf((char *)rspBuf, "%s: %d",
					AT_GetCmdName(AT_CMD_MSMS),
					MS_GetMtSmsClient());

			AT_OutputStr (chan, (UInt8*)rspBuf );
			break;
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CSMP_Handler (ATC_Csmp_CB)
//
// Description:         Transition on AT+CSMP
//
// Notes:
//
//------------------------------------------------------------------------------

AT_Status_t ATCmd_CSMP_Handler (AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								const UInt8* _P1, const UInt8* _P2,
								const UInt8* _P3, const UInt8* _P4)
{
	UInt8			rspBuf[ AT_RSPBUF_SM ] ;

	switch(accessType)
	{
		case AT_ACCESS_REGULAR:
			if(_P1){
				if( (SMS_GET_VPF(atoi( (char*)_P1)) != 2) || (SMS_GET_MTI(atoi( (char*)_P1)) != 1) ){
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED );
					return AT_STATUS_DONE;
				}

				SMS_SetSmsTxParamRejDupl(ATC_GET_RD(atoi ( (char*)_P1)));
				SMS_SetSmsTxParamStatusRptReqFlag(ATC_GET_SRR(atoi ( (char*)_P1)));
				SMS_SetSmsTxParamReplyPath(ATC_GET_RP(atoi ( (char*)_P1)));
				SMS_SetSmsTxParamUserDataHdrInd(ATC_GET_UDHI(atoi ( (char*)_P1)));
			}
			if(_P2){
				SMS_SetSmsTxParamValidPeriod(atoi ( (char*)_P2));
			}
			if(_P3){
				SMS_SetSmsTxParamProcId(atoi ( (char*)_P3));
			}
			if(_P4){
				SmsCodingType_t codingType;
				UInt8 dcs = atoi( (char*)_P4);

				if(dcs & 0x10){
					codingType.msgClass = (SmsMsgClass_t) (dcs & 0x03);
				}
				else{
					codingType.msgClass = SMS_MSG_NO_CLASS;
				}
				codingType.alphabet = (SmsAlphabet_t)((dcs & 0x0C) >> 2);
				codingType.codingGroup = SMS_GET_CODING_GRP(dcs);
				SMS_SetSmsTxParamCodingType(&codingType);
			}
			break;

		case AT_ACCESS_READ:
			{

			SmsTxTextModeParms_t smsParms;

			SMS_GetTxParamInTextMode(&smsParms);

			sprintf( (char *)rspBuf, "%s: %d,%d,%d,%d",
				AT_GetCmdName(AT_CMD_CSMP), smsParms.fo, smsParms.vp, smsParms.pid, smsParms.dcs);

			AT_OutputStr(chan, (UInt8*)rspBuf );

			}
			break;

		case AT_ACCESS_TEST:
			sprintf((char*)rspBuf, "+CSMP: (0-?),(0-255),(0-255),(0-255)");
			AT_OutputStr (chan, rspBuf );
			break;

		default:
			break;
	}

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}


//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CMGD_Handler (ATC_Cmgd_CB)
//
// Description:         Transition on AT+CMGD
//
// Notes:
//
//------------------------------------------------------------------------------

AT_Status_t ATCmd_CMGD_Handler (AT_CmdId_t cmdId, UInt8 chan,
								UInt8 accessType, const UInt8* _P1, const UInt8* _P2)
{
	UInt16          MsgNo;
	Result_t		res;
	UInt8			rspBuf[ AT_RSPBUF_SM ] ;
	UInt8			clientID = AT_GetV24ClientIDFromMpxChan(chan);
	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg(chan);

	if(accessType == AT_ACCESS_READ){
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED );
		return AT_STATUS_DONE;
	}
	// SHOW valid locations
	else if ( accessType == AT_ACCESS_TEST ){
		UInt16          NbFree;
		UInt16          NbUsed;

		// Get number of stored messages, and number of free records
		res = SMS_GetSMSStorageStatus((SmsStorage_t) channelCfg->CPMS [0], &NbFree, &NbUsed);

		if(res != RESULT_OK){
			AT_CmdRspError(chan, AT_SmsErrorToAtError(res, GET_CMGF((DLC_t)chan)));
		}
		else{
			// Return the answer
			sprintf ((char *)rspBuf, "%s: (%d-%d)", AT_GetCmdName(AT_CMD_CMGD), 0, NbUsed + NbFree -1);

			AT_OutputStr (chan, (UInt8*)rspBuf );
			AT_CmdRspOK(chan);
		}
    }

	// DELETE messages
	else if ( accessType == AT_ACCESS_REGULAR ){

		if((!_P2) || ((_P2) && (*_P2 == '0')))
		{
			MsgNo = atoi((char*)_P1);

			res = SMS_DeleteSmsMsgByIndexReq(clientID, (SmsStorage_t)channelCfg->CPMS[0], MsgNo);

			if(res == RESULT_OK){

				isRspForATDeleteRequest = TRUE;

				return AT_STATUS_PENDING;
			}
			else{
				AT_CmdRspError(chan, AT_SmsErrorToAtError(res, GET_CMGF((DLC_t)chan)));
			}
		}
		else
		{
			// multiple delete
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);  // **FixMe**   not handled for now
			return AT_STATUS_DONE;
		}

	}

	return AT_STATUS_DONE;
}

//------------------------------------------------------------------------------
//
// Function Name:       AtHandleSmsDeleteSimRsp (ATC_SmsDelete_CB)
//
// Description:         Process result of PPMesgDelete
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleSmsDeleteSimRsp(InterTaskMsg_t* inMsg)
{
	SmsMsgDeleteResult_t *sms_update = (SmsMsgDeleteResult_t *)inMsg->dataBuf;
	UInt8	chan = AT_GetCmdRspMpxChan(AT_CMD_CMGD);
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	_TRACE(("SMS: AtHandleSmsDeleteSimRsp: isRspForATDeleteRequest = ", isRspForATDeleteRequest));

	if (sms_update->result == SIMACCESS_SUCCESS)
	{
		if(isRspForATDeleteRequest){
			AT_CmdRspOK(chan);
		}
		else{ // this is a response for a delete from MMI

			// format indication so that ATC knows the message is deleted
			sprintf ((char *)rspBuf, "%s: %s, %d", AT_GetCmdName(AT_CMD_CMGD), MemStr[sms_update->storage], sms_update->rec_no);
			AT_OutputUnsolicitedStr ((UInt8*)rspBuf);
		}
	}
	else
	{
		if(isRspForATDeleteRequest){
			AT_CmdRspError(chan, AT_CMS_ERR_MEMORY_FAILURE);
		}
	}

	isRspForATDeleteRequest = FALSE;
}

//------------------------------------------------------------------------------
//
// Function Name:       ATC_Cmgr_CB
//
// Description:         Transition on AT+CMGR
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_CMGR_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	Result_t res;
	UInt8	MsgNo   = atoi((char*)_P1);
	UInt8	capacity;
	UInt8 	rspBuf[ AT_RSPBUF_SM ] ;
	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg(chan);
	UInt8 clientID = AT_GetV24ClientIDFromMpxChan(chan);

	if(accessType == AT_ACCESS_READ)
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
		return AT_STATUS_DONE;
	}
	else if(accessType == AT_ACCESS_TEST)
	{
		if(!SIM_IsCachedDataReady()){
			AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY );
			return AT_STATUS_DONE;
		}

		capacity = SMS_GetSmsMaxCapacity((SmsStorage_t)channelCfg->CPMS[0]) - 1;  // index is "0" based.

		sprintf ((char *)rspBuf, "%s: (0-%d)", AT_GetCmdName(AT_CMD_CMGR), capacity);
		AT_OutputStr (chan, (UInt8*)rspBuf);

		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}

	res = SMS_ReadSmsMsgReq(clientID, (SmsStorage_t)channelCfg->CPMS[0], MsgNo);

	if(res == RESULT_OK){
		return AT_STATUS_PENDING;
	}
	else{
		AT_CmdRspError(chan, AT_SmsErrorToAtError(res, GET_CMGF((DLC_t)chan)));
		return AT_STATUS_DONE;
	}
}


//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CMGL_Handler (ATC_Cmgl_CB)
//
// Description:         Transition on AT+CMGL
//
// Notes:
//
//------------------------------------------------------------------------------

AT_Status_t ATCmd_CMGL_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	SIMSMSMesgStatus_t	Stat;
	UInt8				i = 0;
	Result_t			res;
	UInt8				rspBuf[ AT_RSPBUF_SM ] ;
	AT_ChannelCfg_t*	channelCfg = AT_GetChannelCfg(chan);
	UInt8				clientID = AT_GetV24ClientIDFromMpxChan(chan);

	if(accessType == AT_ACCESS_READ)
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
		return AT_STATUS_DONE;
	}
	else if(accessType == AT_ACCESS_TEST)
	{
		if (channelCfg->CMGF) // Text mode
		{
			sprintf ((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CMGL), "(\"REC UNREAD\",\"REC READ\",\"STO UNSENT\",\"STO SENT\",\"ALL\")");
		}
		else // PDU mode
		{
			sprintf ((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CMGL), "(0-4)");
		}

		AT_OutputStr (chan, (UInt8*)rspBuf);
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}

	// accessType == AT_ACCESS_REGULAR: LIST messages
	if (_P1)
	{
		if (channelCfg->CMGF)
		{
			while ((i<=8) && (strcmp((char*)_P1, atc_SmsStateText[i]))) i++;

			if (i==8)
				Stat = SIMSMSMESGSTATUS_FREE ;
			else{
				if ((i<8) && (strlen(atc_SmsStateText[i])))
					Stat = (SIMSMSMesgStatus_t)i;
				else{
					AT_CmdRspError(chan, AT_CMS_ERR_INVALID_TEXT_MODE_PARM);
					return AT_STATUS_DONE;
				}
			}
		}
		else
		{
			if( strlen((char*)_P1) > 1){
				AT_CmdRspError(chan, AT_CMS_ERR_INVALID_PUD_MODE_PARM);
				return AT_STATUS_DONE;
			}
			else{
				if( (_P1[0] < 0x30) || (_P1[0] > 0x34) ){
					AT_CmdRspError(chan, AT_CMS_ERR_INVALID_PUD_MODE_PARM);
					return AT_STATUS_DONE;
				}
			}

			while ((i<=8) && (atoi((char*)_P1) != atc_SmsStatePdu[i])) i++;

			if (i==8)
				Stat = SIMSMSMESGSTATUS_FREE ;
			else{
				if (((i<8) && (atc_SmsStatePdu[i] != 0xFF)))
					Stat = (SIMSMSMesgStatus_t)i;
				else{
					AT_CmdRspError(chan, AT_CMS_ERR_INVALID_PUD_MODE_PARM);
					return AT_STATUS_DONE;
				}
			}
		}
	} // end if (_P1)
	else
	{
		Stat = SIMSMSMESGSTATUS_FREE;
	}

	res = SMS_ListSmsMsgReq(clientID, (SmsStorage_t)channelCfg->CPMS[0], Stat);

	if(res == RESULT_OK){
		return AT_STATUS_PENDING;
	}
	else if(res == SMS_NO_MSG_TO_LIST){
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}
	else{
		AT_CmdRspError(chan, AT_SmsErrorToAtError(res, GET_CMGF((DLC_t)chan)));
		return AT_STATUS_DONE;
	}
}

//------------------------------------------------------------------------------
//
// Function Name:       AtHandleSmsReadResp (ATC_SmsRead_CB)
//
// Description:         Process the read result for +CMGR and +CMGL
//
//------------------------------------------------------------------------------
void AtHandleSmsReadResp(InterTaskMsg_t *inMsg)
{
	UInt8	rspBuf[ AT_RSPBUF_LG ] ;
	SIMAccess_t result;
	SmsSimMsg_t *sms_data = (SmsSimMsg_t *)inMsg->dataBuf;
	UInt8 chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg(chan);
	AT_CmdId_t cmdId = AT_GetCmdIdByMpxChan(chan);

	if ( (cmdId == AT_CMD_CMGL) && (inMsg->dataLength == 0) ){
		// this is the signal that there's no more msg to list
		AT_CmdRspOK(chan);
		return;
	}

	if(inMsg->msgType == MSG_SMSCB_READ_RSP){
		result = ((SIM_SMS_DATA_t *)inMsg->dataBuf)->result;
	}
	else{
		result = ((SmsSimMsg_t *)inMsg->dataBuf)->result;
		//In PDU mode, some corrupted SMS may have an invalid pduSize
 		if((!channelCfg->CMGF) && (sms_data->pduSize > SMS_MAX_PDU_STRING_LENGTH ))
			result = SIMACCESS_NO_SIM;
	}

	if (result == SIMACCESS_SUCCESS)
	{
		UInt8  ScaLen ;
		UInt16 Res    ;

		sprintf((char *)rspBuf, "%s: ", AT_GetCmdName(cmdId));

		// check if this is a CB message
		if (inMsg->msgType == MSG_SMSCB_READ_RSP)
		{
			SIM_SMS_DATA_t* cbMsg = (SIM_SMS_DATA_t *)inMsg->dataBuf;
			char * CmdName = (char*)AT_GetCmdName(cmdId);
			Res = atc_DisplaySmsCbPage(chan, CmdName,
							(UInt8 *)&rspBuf[strlen((char *)rspBuf)],
							(StoredSmsCb_t*)cbMsg->sms_mesg.mesg_data,
							cbMsg->sms_mesg.status);

			if(Res != 0){
				AT_CmdRspCMSError(chan, Res);
			}
			else{
				AT_OutputStr(chan, (UInt8 *)rspBuf );
			}
		}
		else	// non-CB msg
		{

			// whatever the message type (SUBMIT, DELIVER, STATUS REPORT)
			// CMGL starts display with index. Then the message follows
			if (cmdId == AT_CMD_CMGL){
				sprintf((char *)&rspBuf[strlen((char *)rspBuf)], "%d,", sms_data->rec_no);
			}

			// prepare buffer for string output
			if (channelCfg->CMGF)  // Text mode
			{
				// Text mode display
				Res = AT_OutMsg2Te((UInt8 *)&rspBuf[strlen((char *)rspBuf)], sms_data, cmdId);

				if(Res)
				{
					AT_CmdRspCMSError(chan, Res);
					return;
				}
			}
			else  // PDU mode
			{
				UInt8 len = strlen((char*)sms_data->msg.msgRxData.ServCenterAddress.Number);
				ScaLen = (len/2) + (len%2) + 1 + 1;	// sca length in the pdu string

				if(channelCfg->CPMS[0] == SR_STORAGE){
					ScaLen = 0;
				}

				// <status>
				sprintf((char *)&rspBuf[strlen((char *)rspBuf)], "%d,", atc_SmsStatePdu[sms_data->status]);

				// <length>
				sprintf((char *)&rspBuf[strlen((char *)rspBuf)], ",%d", sms_data->pduSize - ScaLen);

				// <CR><LF>
				sprintf((char *)&rspBuf[strlen((char *)rspBuf)], "\r\n");

				UTIL_HexDataToHexStr((UInt8 *)&rspBuf[strlen((char *)rspBuf)], sms_data->PDU, sms_data->pduSize);
			}

			// Output
			AT_OutputStr(chan, (UInt8 *)rspBuf );
		}  // end non-CB msg
	}
	else if(result == SIMACCESS_NO_SIM)  // data corrupted
	{
		if (channelCfg->CMGF)  // text
			sprintf((char *)rspBuf, "%s: %d, \"CORRUPTED\"", AT_GetCmdName(cmdId), sms_data->rec_no);
		else				   // PDU
			sprintf((char *)rspBuf, "%s: %d, 99, 0", AT_GetCmdName(cmdId), sms_data->rec_no);
		AT_OutputStr(chan, (UInt8 *)rspBuf);
//		AT_CmdRspError(chan, AT_CMS_ERR_OP_NOT_SUPPORTED);

		// may need to delete this message here
	}
	else // SIM access error
	{
		AT_CmdRspError(chan, AT_CMS_ERR_UNKNOWN_ERROR);
		_TRACE(("SMS: AtHandleSmsReadResp (SIM-acc-ERR): result = ", result));
	}

	if (cmdId == AT_CMD_CMGR){
		AT_CmdRspOK(chan);
	}
}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_MMGSR_Handler
//
// Description:         Toggle the status of sms from UNREAD to READ
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_MMGSR_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg(chan);
	SmsStorage_t storeType = (SmsStorage_t)channelCfg->CPMS[0];
	UInt8 clientID = AT_GetV24ClientIDFromMpxChan(chan);

	if ( accessType == AT_ACCESS_REGULAR )
	{
		UInt16 index = atoi((char*)_P1);

		if(SMS_ChangeSmsStatusReq(clientID, storeType, index) != RESULT_OK){
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			return AT_STATUS_DONE;
		}
		else{
			AT_CmdRspOK(chan);
			return AT_STATUS_DONE;
		}
	}
	else if(accessType == AT_ACCESS_READ)
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		return AT_STATUS_DONE;
	}
	else
	{
		sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, (UInt8*)rspBuf );
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_MMGQ_Handler
//
// Description:         List number of sms in each box
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_MMGQ_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8			type;
	msgNumInBox_t	msgNumInBox;
	UInt8			rspBuf[ AT_RSPBUF_SM ] ;
	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg((DLC_t)chan);

	if ( accessType == AT_ACCESS_REGULAR )
	{
		type = (_P1 == NULL) ? 1 : atoi((char*)_P1);

		switch(type)
		{
		case 1:
			if(!GetNumMsgsForEachBox((SmsStorage_t)channelCfg->CPMS[0], &msgNumInBox)){
				AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY );
				return AT_STATUS_DONE;
			}

			sprintf((char *)rspBuf, "%s: %d, %d, %d, %d", AT_GetCmdName(cmdId),
				msgNumInBox.num1, msgNumInBox.num2, msgNumInBox.num3, msgNumInBox.num4);

			AT_OutputStr (chan, (UInt8*)rspBuf );
			break;

		default:
			break;
		}
	}
	else if(accessType == AT_ACCESS_READ)
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		return AT_STATUS_DONE;
	}
	else
	{
		sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, (UInt8*)rspBuf );
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}


//------------------------------------------------------------------------------
//
// Function Name:       ATC_ManageBufferedURsp
//
// Description:         This function is called at the end of an AT transaction
//                      and when V24 switch from ON LINE to OFF LINE.
//                      All the buffered unsollicited response are sent to TE.
//
// Notes:
//
//------------------------------------------------------------------------------
void ATC_ManageBufferedURsp ( void )
{
 atc_BufferedURsp_t  Type;
 void *Ptr;
 Boolean Zob;
 Boolean TakeCareOfWaitedCnma = (!((SMS_GetNewMsgDisplayPref(SMS_MT) == 0)
                                 &&(SMS_GetNewMsgDisplayPref(SMS_DS) == 0)));

 Ptr = atc_GetOldestURspFromBuffer ( &Type );

 if (atc_NextCmdIsCnma == 1)
 {
  /* A transaction occured, no +Cnma was received whereas waited.          */
  /* TE is crashed, incoming sms must be stored so change cnmi parameters. */
  atc_ResetCnmaDataBase ();
  _TRACE (("Change Cnmi parameters because +Cnma not received."));
 }

 while (Ptr != NULL)
 {
  switch (Type & ~RP_ACK_MSK)
  {
   case ATC_SMSPP_IND_TYPE :
   case ATC_SR_IND_TYPE :

        if (SMS_GetNewMsgDisplayPref(SMS_MODE) != 0)  // <mode> = 0, response are buffered.
        {
         switch ( SMS_GetNewMsgDisplayPref(SMS_MT) )
         {
          case 0 : // No indication routed to the TE.
               break;

          case 1 : // Send indication to the TE.
          case 2 : // Route directly msg toward TE. (except Cl 2 & store msg).
          case 3 : // Eq <mt> = 1 except for cl 3 msg which are routed to TE.
                   // <mt> = 2 & 3 not supported.
               AT_OutputUnsolicitedStr ( (UInt8*)Ptr );
               break;
         }
        }
        OSHEAP_Delete( Ptr );

        Zob = ((Type & RP_ACK_MSK) != WAIT_CNMA);
        Zob = Zob || (TakeCareOfWaitedCnma == FALSE);
        if (Zob)
         Ptr = atc_GetOldestURspFromBuffer ( &Type );
        else
        {

         Ptr = NULL; /* Wait +Cnma before sending next stored SMSPP or SR. */

         atc_NextCmdIsCnma = TRUE;

         /* In this state Mode should be equal to 2 and atc_SmsWaitedCnma  */
         /* should be > 0.                                                 */

         if ((SMS_GetNewMsgDisplayPref(SMS_MODE) != 2)
         ||  (atc_SmsWaitedCnma == 0))
          _TRACE (("ATC_ManageBufferedURsp : unconsistent state mode %d Cnma %d Type %x:",
                    SMS_GetNewMsgDisplayPref(SMS_MODE),
                    atc_SmsWaitedCnma,
                    Type));

        }
        break;

   case ATC_SMSCB_IND_TYPE :
   case ATC_GENERICAL_TYPE :
        if (SMS_GetNewMsgDisplayPref(SMS_MODE) != 0)  // <mode> = 0, response are buffered.
        {
         switch ( SMS_GetNewMsgDisplayPref(SMS_MT) )
         {
          case 0 : // No indication routed to the TE.
               break;

          case 1 : // Send indication to the TE.
          case 2 : // Route directly msg toward TE. (except Cl 2 & store msg).
          case 3 : // Eq <mt> = 1 except for cl 3 msg which are routed to TE.
                   // <mt> = 2 & 3 not supported.
               AT_OutputUnsolicitedStr ((UInt8 *) Ptr );
               break;
         }
        }
        OSHEAP_Delete( Ptr );
        Ptr = atc_GetOldestURspFromBuffer ( &Type );

        break;

   default :
        OSHEAP_Delete( Ptr );
        Ptr = atc_GetOldestURspFromBuffer ( &Type );

        break;
  }
 }
}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CPMS_Handler (ATC_Cpms_CB)
//
// Description:         Transition on AT+CPMS
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_CPMS_Handler (AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								const UInt8* _P1, const UInt8* _P2, const UInt8* _P3)
{
	UInt8			i, j;
	const UInt8*	_P = NULL;
	UInt8			rspBuf[ AT_RSPBUF_SM ] ;
	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg(chan);

	SmsStorage_t  Mem [4] = {NO_STORAGE, NO_STORAGE, NO_STORAGE, NO_STORAGE};

	UInt16        NbMsgStored  [NB_MAX_STORAGE] = {0, 0, 0, 0, 0, 0};
	UInt16        MemAvailable [NB_MAX_STORAGE];

	if(!SIM_IsCachedDataReady())
	{
		AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY );
		return AT_STATUS_DONE;
	}

	if(!strcmp((const char*)_P1, (const char *)"ME")){
		if(!SMS_IsMeStorageEnabled()){
			AT_CmdRspError(chan, AT_CMS_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}
	}

	// First, determine the storage types for all the memory storages
	switch(accessType)
	{
	   case AT_ACCESS_REGULAR:
		// Loop on 3 potential parameters.
		for (i=0; i<3; i++)
		{
			switch(i)
			{
				case 0:
					_P = _P1;
					break;
				case 1:
					_P = _P2;
					break;
				case 2:
					_P = _P3;
					break;
				default:
					_P = NULL;
					break;
			}

			if (_P)
			{
				j = 0;
				while ( j < NB_MAX_STORAGE )
				{
					// Check what storage is it about ?
					if ((!strcmp ((char*)_P, MemStrNC[j])) && (StorageCapability[j]))
					{
						Mem[i + 1] = (SmsStorage_t) j;
						j = 0xFF;
					}
					else
						j++;
				}
			}
			else if (i > 0)            // For Mem2 & Mem3, get stored storage location
			{                          // if not user specified.
				Mem[i + 1] = (SmsStorage_t) channelCfg->CPMS[i];
			}
		}

		if (( Mem[1] == 0xFF )			// Mem1 unknown or absent.
		|| ( _P2 && (Mem[2] == 0xFF))	// Mem2 present and unknown
		|| ( _P3 && (Mem[3] == 0xFF))	// Mem3 present and unknown
		|| ( _P3 && !_P2))				// Mem2 skipped and Mem3 present
		{
			// Operation not allowed.
			AT_CmdRspError(chan, AT_CMS_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}
		break;

		case AT_ACCESS_READ:
		   // No parameters, what is current state ?
			for (i=0; i<3; i++)
				Mem[i+1] = (SmsStorage_t)channelCfg->CPMS[i];
			break;

		case AT_ACCESS_TEST:
			if(SMS_IsMeStorageEnabled()){
				sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CPMS), "(\"SM\",\"ME\",\"BM\",\"SR\"),(\"SM\"),(\"SM\")");
			}
			else{
				sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CPMS), "(\"SM\",\"BM\",\"SR\"),(\"SM\"),(\"SM\")");
			}
			AT_OutputStr (chan, (UInt8*)rspBuf );
			AT_CmdRspOK(chan);
			return AT_STATUS_DONE;

		default:
			AT_CmdRspError(chan, AT_CMS_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
	}

	// Second, check availability for all memory types.
	SMS_GetSMSStorageStatus(Mem[1],  &MemAvailable[Mem[1]], &NbMsgStored[Mem[1]]);
	SMS_GetSMSStorageStatus(Mem[2],  &MemAvailable[Mem[2]], &NbMsgStored[Mem[2]]);
	SMS_GetSMSStorageStatus(Mem[3],  &MemAvailable[Mem[3]], &NbMsgStored[Mem[3]]);

	// Third, format the output strings
	switch(accessType)
	{
		case AT_ACCESS_REGULAR:
			for (i=0; i<3; i++)
			{
				channelCfg->CPMS[i] = (SmsStorage_t)Mem[i+1];
			}

//			SMS_SetSMSPrefStorage((SmsStorage_t)channelCfg->CPMS[0]);

			sprintf ((char *)rspBuf, "%s: %d,%d,%d,%d,%d,%d",
				AT_GetCmdName(AT_CMD_CPMS),
				NbMsgStored[Mem[1]], NbMsgStored[Mem[1]] + MemAvailable[Mem[1]],
				NbMsgStored[Mem[2]], NbMsgStored[Mem[2]] + MemAvailable[Mem[2]],
				NbMsgStored[Mem[3]], NbMsgStored[Mem[3]] + MemAvailable[Mem[3]]);

			break;

		case AT_ACCESS_READ:
			for (i=0; i<3; i++)
				Mem[i+1] = (SmsStorage_t) channelCfg->CPMS[i];

			sprintf ((char *)rspBuf, "%s: %s,%d,%d,%s,%d,%d,%s,%d,%d",
				AT_GetCmdName(AT_CMD_CPMS),
				MemStr[Mem[1]], NbMsgStored[Mem[1]],
				NbMsgStored[Mem[1]] + MemAvailable[Mem[1]],
				MemStr[Mem[2]], NbMsgStored[Mem[2]],
				NbMsgStored[Mem[2]] + MemAvailable[Mem[2]],
				MemStr[Mem[3]], NbMsgStored[Mem[3]],
				NbMsgStored[Mem[3]] + MemAvailable[Mem[3]]);

			break;

		default:
			break;
	}

	AT_OutputStr (chan, (UInt8*)rspBuf );
	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CNMI_Handler (ATC_Cnmi_CB)
//
// Description:         Process command AT+CNMI.
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_CNMI_Handler (AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								const UInt8* _P1, const UInt8* _P2, const UInt8* _P3,
								const UInt8* _P4, const UInt8* _P5)
{
	UInt8			i;
	const UInt8*	_P;
	UInt8			rspBuf[ AT_RSPBUF_SM ] ;

	if(accessType == AT_ACCESS_REGULAR || accessType == AT_ACCESS_READ)
	{
		if(accessType == AT_ACCESS_REGULAR)
		{
			// Loop on 5 potential parameters.
			for (i=0; i<5; i++)
			{
				switch(i)
				{
					case 0:
						_P = _P1;
						break;
					case 1:
						_P = _P2;
						break;
					case 2:
						_P = _P3;
						break;
					case 3:
						_P = _P4;
						break;
					case 4:
						_P = _P5;
						break;
					default:
						_P = NULL;
						break;
				}

				// Get parameters and memorize them.
				if (_P)
					SMS_SetNewMsgDisplayPref((NewMsgDisplayPref_t)i, atoi((char*)_P) );
			}
		}

		if ((SMS_GetNewMsgDisplayPref(SMS_BFR) == 0) && (SMS_GetNewMsgDisplayPref(SMS_MODE) != 0))
		{
			// Flushed the buffered unsollicited result to the TE.
			ATC_ManageBufferedURsp();
		}
		else if ((SMS_GetNewMsgDisplayPref(SMS_BFR) == 1) && (SMS_GetNewMsgDisplayPref(SMS_MODE) != 0))
		{
			// Clear the buffured unsollicited result.
			atc_PurgeURspBuffer();
		}

		if (V24_GetEchoMode((DLC_t)chan)) {
			sprintf((char *)rspBuf, "%s: %d, %d, %d, %d, %d", AT_GetCmdName(AT_CMD_CNMI),
				SMS_GetNewMsgDisplayPref(SMS_MODE), SMS_GetNewMsgDisplayPref(SMS_MT),
				SMS_GetNewMsgDisplayPref(SMS_BM), SMS_GetNewMsgDisplayPref(SMS_DS),
				SMS_GetNewMsgDisplayPref(SMS_BFR) );

			AT_OutputStr(chan, (UInt8*)rspBuf );
		}
	}
	else // accessType == AT_ACCESS_TEST
	{
		sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CNMI), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, (UInt8*)rspBuf );
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CNMA_Handler (ATC_Cnma_CB)
//
// Description:         Process command AT+CNMA.
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_CNMA_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	if( accessType == AT_ACCESS_READ){
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
		return AT_STATUS_DONE;
	}
	else if( accessType == AT_ACCESS_TEST){
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}

	// accessType == AT_ACCESS_REGULAR
	if ( (GET_CSMS == 1) && (atc_SmsWaitedCnma) && (atc_NextCmdIsCnma) )
	{
		atc_SmsWaitedCnma--;
		atc_NextCmdIsCnma = FALSE;

		if (atc_SendRpAck)
		{
			// Sms acknowledgment occurs now.
			T_MN_TP_STATUS_RSP *StatusRsp = (T_MN_TP_STATUS_RSP *)OSHEAP_Alloc(sizeof(T_MN_TP_STATUS_RSP));

			atc_GetCnmaTimer();

			StatusRsp -> tp_mti   = atc_GetCnmaTpMti();
			StatusRsp -> rp_cause = MN_SMS_RP_ACK;
			StatusRsp -> tp_fcf   = FALSE;
			StatusRsp -> tp_fcs   = TP_FCS_NO_ERROR;

			MNSMS_SMResponse( StatusRsp );

			atc_SendRpAck--;
		}

		AT_CmdRspOK(chan);
	}
	else
	{
		AT_CmdRspError(chan, AT_CMS_ERR_NO_CNMA_EXPECTED );
		return AT_STATUS_DONE;
	}

	return AT_STATUS_DONE;
}

//------------------------------------------------------------------------------
//
// Function Name: atc_ManageCnmiMode
//
// Description:   Msg            : SMS or Status report content.
//                Size           : size of the Sms or SR.
//                MtType         : indicate if Msg is a SMS or a SR.
//                MsgStored      : TRUE if msg has been stored by caller. In
//                                 this case RP ACKis alaways sent.
//                SendIndication : TRUE if incation must be sent toward TA.
//
// Notes:
//
//------------------------------------------------------------------------------
void atc_ManageCnmiMode (UInt8				chan,
						 char               *Msg,
                         UInt16             Size,
                         atc_BufferedURsp_t MtType,
                         Boolean            MsgStored,
                         Boolean            SendIndication)
{
	ActionRpAck_t ActionRpAck;

	if (MtType != ATC_SMSCB_IND_TYPE)
	{
		if( (GET_CSMS != 1) || (MsgStored) )
			ActionRpAck = SEND_RP_ACK;
		else
			ActionRpAck = WAIT_CNMA;
	}
	else  // CB don't need ack to network
		ActionRpAck = (ActionRpAck_t)0;

	switch ( SMS_GetNewMsgDisplayPref(SMS_MODE) )
	{
		case 0 : // All indication buffered.
			if (SendIndication)
				AT_ManageUnsolSmsStr(chan, (UInt8*)Msg, !MsgStored);

			if (MtType != ATC_SMSCB_IND_TYPE)
				ActionRpAck = SEND_RP_ACK;     // Don't wait CNMA for ack if sms is buffured.

			break;

		case 1 : // Discard if TA-TE link reserved. Otherwise flush.
		case 2 : // Buffer until TA-TE link free, then flush.
			if ((V24_GetOperationMode ((DLC_t) chan ) != V24OPERMODE_AT_CMD)
				|| (atc_SmsWaitedCnma > 0)) // Don't send indication if +Cnma is waited.
			{
				// Indication must be queued until end of transaction
				// or v24 switch OFF LINE.
				if (SMS_GetNewMsgDisplayPref(SMS_MODE) == 2)
				{
					if (SendIndication)
						AT_ManageUnsolSmsStr(chan, (UInt8*)Msg, !MsgStored);
				}
				else{
					if ((!MsgStored) &&  (MtType != ATC_SMSCB_IND_TYPE))
						ActionRpAck = SEND_RP_ERROR;  // <mode> = 1, response is discarded, & sms Nack
				}
			}
			else if (SendIndication){
				AT_ManageUnsolSmsStr(chan, (UInt8*)Msg, !MsgStored);
			}
			break;

		case 3 : // Specif inband technique when V24 ONE LINE.
           // Not supported.
			break;
	} // End switch <mode>


	// Acknowledge management: only ack for display type indication, stored type
	// is already handled in SMS module side.
	if(!MsgStored && (chan == 0)){  // only ack for once

		SmsMti_t rptType;

		if (MtType == ATC_SMSPP_IND_TYPE)
			rptType = SMS_DELIVER;
		else if (MtType == ATC_SR_IND_TYPE)
			rptType = SMS_STATUS_REPORT;
		else
			rptType = 0xFF;

		if ( ActionRpAck == SEND_RP_ACK)
		{
			SMS_SendAckToNetwork(rptType, SMS_ACK_SUCCESS);

			if ((GET_CSMS == 1) && (!MsgStored))  // Acknowledge is sent but +cnma is expected anyway.
				atc_SmsWaitedCnma++;
		}
		else if (ActionRpAck == SEND_RP_ERROR)
		{
			SMS_SendAckToNetwork(rptType, SMS_ACK_ERROR);

			atc_ResetCnmaDataBase ();
		}
		else if (ActionRpAck == WAIT_CNMA)
		{
			// Wait +Cnma command to send RP acknowledment.
			// No more indication should be sent until +Cnma has been received.
			// The next command from TE must be a +Cnma.
			atc_NextCmdIsCnma = TRUE;

			atc_SmsWaitedCnma++;
			atc_SendRpAck++;  // RP_ACK must be sent as soon as +Cnma is received from TE.

			if (MtType == ATC_SMSPP_IND_TYPE)
			{
				atc_SetCnmaTpMti (MN_MTI_DELIVER_RESP);
				atc_SetCnmaTimer (MN_MTI_DELIVER_RESP);
			}
			else
			{
				atc_SetCnmaTpMti (MN_MTI_STATUS_REPORT_RESP);
				atc_SetCnmaTimer (MN_MTI_STATUS_REPORT_RESP);
			}
		}
	}  // end "if(!MsgStored)"
}

//------------------------------------------------------------------------------
//
// Function Name: AT_ManageUnsolSmsStr
//
// Description:   manage unsolicited sms str for all the channels.
//
// Notes:
//
//------------------------------------------------------------------------------
void AT_ManageUnsolSmsStr(UInt8 ch, const UInt8* rspStr, Boolean chSpecific)
{
	UInt8 muxMode = MPX_ObtainMuxMode();

	if (chSpecific)
	{
		if (((muxMode == NONMUX_MODE) && (ch == 0)) ||
			((muxMode == MUX_MODE) && (ch != 0)))
		{
			// AT_OutputStrBuffered(ch, rspStr);
			AT_OutputStr(ch, rspStr);
		}
	}
	else
	{
		AT_OutputUnsolicitedStr(rspStr);
	}
}


//------------------------------------------------------------------------------
//
// Function Name:	AtHandleSmsDeliverInd (atc_SmsMtRx_CB)
//
// Description:		Manage reception of SMS MT.
//
// Notes:			Only sms of display type comes to here
//
//------------------------------------------------------------------------------
void AtHandleSmsDeliverInd(InterTaskMsg_t* inMsg)
{
	char *CrLf ="\r\n";
	UInt8  ScaLen;
	UInt16 size;
	UInt8					rspBuf[ AT_RSPBUF_LG ] ;
	SmsSimMsg_t*			smsMsg = (SmsSimMsg_t*)inMsg->dataBuf;
	UInt8 ch;
	UInt8 maxChan;
	UInt8 operMode;

	if( MPX_ObtainMuxMode() == NONMUX_MODE )
		maxChan = 1;
	else
		maxChan = MAX_NORMAL_MPX_CH; // 4, ch 0-3 is the normal ch, ch 4 is DLC_STKTERMINAL

_TRACE(("SMS: AtHandleSmsDeliverInd: smsMsg->pduSize = ", smsMsg->pduSize));

	for(ch = 0; ch < maxChan; ch++)  // for all the channels
	{
		// First step, format indication.
		if(GET_CMGF(ch) == 0) // PDU mode.
		{
			UInt16 len;

			V24_SetRI(TRUE);

			len = strlen((char*)smsMsg->msg.msgRxData.ServCenterAddress.Number);
			ScaLen = (len/2) + (len%2) + 1 + 1;	// sca length in the pdu string

			sprintf ((char *)rspBuf, "+CMT: ,%d %s", smsMsg->pduSize - ScaLen, CrLf);

			UTIL_HexDataToHexStr((UInt8 *)&rspBuf[strlen((char *)rspBuf)], smsMsg->PDU, smsMsg->pduSize);

			V24_SetRI(FALSE);

			rspBuf[strlen((char*)rspBuf)] = '\0';
_TRACE(("SMS: AtHandleSmsDeliverInd: 0: strlen((char*)rspBuf) = ", strlen((char*)rspBuf) ));
		}
		else  // Text mode
		{
			UInt8 sctsString[20+1];

			V24_SetRI(TRUE);

			AtConvertTimeToString(sctsString, smsMsg->msg.msgRxData.srvCenterTime.absTime);

			// +CMT: <oa>,<scts>,<tooa>,<fo>,<pid>,<dcs>,<sca>,<tosca>,<length><cr><lf><data>
			sprintf ((char *)rspBuf, "+CMT:\"%s\",,\"%s\"", smsMsg->daoaAddress.Number, sctsString);

			if (GET_CSDH)
			{
				sprintf ((char *)&rspBuf[strlen((char *)rspBuf)], ",%d,%d,%d,%d,\"%s\",%d,%d",
						smsMsg->daoaAddress.TypeOfAddress,				// <tooa>
						smsMsg->msg.msgRxData.fo,						// <fo>
						smsMsg->msg.msgRxData.procId,					// <pid>
						smsMsg->msg.msgRxData.codingScheme.DcsRaw,		// <dcs>
						smsMsg->msg.msgRxData.ServCenterAddress.Number,	// <sca>
						smsMsg->msg.msgRxData.ServCenterAddress.TypeOfAddress, // <tosca>
						smsMsg->textLen);								// <length>
			}

			sprintf ((char *)&rspBuf[strlen((char *)rspBuf)], "%s", CrLf);

			AT_FormatUdForTe ((UInt8 *)&rspBuf[strlen((char *)rspBuf)], smsMsg->text_data, smsMsg->textLen,
								&smsMsg->msg.msgRxData.codingScheme, smsMsg->msg.msgRxData.usrDataHeaderInd,
								smsMsg->udhData[0]);

			V24_SetRI(FALSE);
_TRACE(("SMS: AtHandleSmsDeliverInd: 1: strlen((char*)rspBuf) = ", strlen((char*)rspBuf) ));
		}

		size = strlen((char*)rspBuf) + 1;
		operMode = V24_GetOperationMode((DLC_t)ch);
		//The sms indication manage does not work with mux mode. So disable it to avoid the indication on
		//data connection channel
		if ( operMode == V24OPERMODE_AT_CMD || operMode == V24OPERMODE_NO_RECEIVE || maxChan == 1/*NON mux mode*/ ){
			// Second step, buffer, discard or send formatted indication and Rp ack.
			atc_ManageCnmiMode (ch, (char*)rspBuf, size, ATC_SMSPP_IND_TYPE,FALSE,												 // Stored Msg
								(Boolean)(SMS_GetNewMsgDisplayPref(SMS_MODE) != 0)); // Send indication condition.

		}
	}
}

void AtConvertTimeToString(UInt8* timeString, SmsAbsolute_t dateTime)
{
	sprintf((char *) timeString, "%d%d/%d%d/%d%d,%d%d:%d%d:%d%d",
			SMS_GET_LDIGIT(dateTime.years), SMS_GET_RDIGIT(dateTime.years),
			SMS_GET_LDIGIT(dateTime.months), SMS_GET_RDIGIT(dateTime.months),
			SMS_GET_LDIGIT(dateTime.days), SMS_GET_RDIGIT(dateTime.days),
			SMS_GET_LDIGIT(dateTime.hours), SMS_GET_RDIGIT(dateTime.hours),
			SMS_GET_LDIGIT(dateTime.minutes), SMS_GET_RDIGIT(dateTime.minutes),
			SMS_GET_LDIGIT(dateTime.seconds), SMS_GET_RDIGIT(dateTime.seconds) );

	if (dateTime.time_zone & 0x08)
	{
		strcat((char *) timeString, "-");
	}
	else
	{
		strcat((char *) timeString, "+");
	}

	dateTime.time_zone &=~0x80;
	sprintf((char *) &timeString[strlen((char *) timeString)],"%d%d",
		SMS_GET_LDIGIT(dateTime.time_zone), SMS_GET_RDIGIT(dateTime.time_zone) );
	timeString[20] = '\0';
}

//------------------------------------------------------------------------------
//
// Function Name: AtHandleIncomingSmsStoredRes
//
// Description:   Storage MT SMS results, build indication toward TA.
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleIncomingSmsStoredRes(InterTaskMsg_t* inMsg)
{
	UInt8	ch = 0;
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	SmsIncMsgStoredResult_t *PtrRes = (SmsIncMsgStoredResult_t*)inMsg->dataBuf;

	if( (PtrRes->storage == ME_STORAGE) &&
		((PtrRes->rec_no == 0xFFFF) || (PtrRes->rec_no == 0xFFFE) ) ){
		// do not report to AT
		_TRACE(("AT SMS: msg not stored."));
		return;
	}

	if (PtrRes -> result == SIMACCESS_SUCCESS)
	{
		// First step, format indication.
		sprintf((char *)rspBuf, "+CMTI: %s,%d", MemStr[PtrRes->storage], PtrRes->rec_no);

		// Second step, buffer, discard or send formatted indication.

		atc_ManageCnmiMode (ch, (char *)rspBuf,
							(UInt16) strlen((char *)rspBuf) + 1,	// Add 1 to include NULL character terminating the string
							ATC_SMSPP_IND_TYPE,
							TRUE,									// Stored msg
							(SMS_GetNewMsgDisplayPref(SMS_MT) != 0));			// Send indication condition.
		_TRACE (( "AtHandleIncomingSmsStoredRes : storing SMS MT: ok. Result = ", PtrRes -> result));
	}
	else
	{
		AtReportStoredError(PtrRes->result);

		_TRACE(("AtHandleIncomingSmsStoredRes: storing SMS failed. Result = ", PtrRes->result));
	}
}

void AtReportStoredError(SIMAccess_t result)
{
	switch(result)
	{
		case SIMACCESS_MEMORY_EXCEEDED:
			AT_OutputUnsolicitedRsp( AT_CMS_ERR_MEMORY_FULL );
			break;

		case SIMACCESS_MEMORY_ERR:
			AT_OutputUnsolicitedRsp( AT_CMS_ERR_MEMORY_FAILURE );
			break;

		default:
			AT_OutputUnsolicitedRsp( AT_CMS_ERR_UNKNOWN_ERROR );
			break;
	}
}

//------------------------------------------------------------------------------
//
// Function Name: AtHandleSmsStatusReportInd (atc_SRMtRx_CB)
//
// Description:   Manage reception of Status Report.
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleSmsStatusReportInd(InterTaskMsg_t* inMsg)
{
	UInt8 ch = 0;
	UInt8 maxChan;
	UInt8 rspBuf[ AT_RSPBUF_SM ] ;
	SmsSimMsg_t *smsSrMsg = (SmsSimMsg_t *)inMsg->dataBuf;
	UInt8 operMode;

	if( MPX_ObtainMuxMode() == NONMUX_MODE )
		maxChan = 1;
	else
		maxChan = MAX_NORMAL_MPX_CH; // 4, ch 0-3 is the normal ch, ch 4 is DLC_STKTERMINAL

	for(ch = 0; ch < maxChan; ch++)  // for all the channels
	{
		// <ds> = 1 so manage indication and acknowlegdment.

		// First step, format indication.
		if (GET_CMGF(ch) == 0) // PDU mode.
		{
			sprintf ((char *)rspBuf, "+CDS: %d\r\n", smsSrMsg->pduSize);

			//{ // Begin Swisscom-specific: Including the SCA is non-conformant
			//	SmsAddress_t	sca;
			//	Sms_411Addr_t	sms411;
			//	Result_t		res;
			//	res = SMS_GetSMSrvCenterNumber( &sca, USE_DEFAULT_SCA_NUMBER );
			//
			//	if( res == RESULT_OK )
			//	{
			//		SMS_TeAddrTo411( &sms411, &sca.Number[0], sca.TypeOfAddress );
			//		UTIL_HexDataToHexStr(&rspBuf[strlen((char *)rspBuf)], (UInt8 *)(void *)&sms411, (UInt16)sms411.Len + 1 );
			//	}
			//	else
			//	{
			//		strcat( (char *)rspBuf, "00" );
			//	}
			//} // End Swisscom-specific

			UTIL_HexDataToHexStr(&rspBuf[strlen((char *)rspBuf)], smsSrMsg->PDU, smsSrMsg->pduSize);
		}
		else               // Text mode.
		{
			UInt8 sctsString[20+1];
			UInt8 dtString[20+1];
			AtConvertTimeToString(sctsString, smsSrMsg->msg.msgSrData.srvCenterTime.absTime);
			AtConvertTimeToString(dtString, smsSrMsg->msg.msgSrData.discardTime.absTime);

			// +CDS: <fo>,<mr>,<ra>,<tora>,<scts>,<dt>,<st>
			sprintf((char *)rspBuf, "+CDS: %d,%d,\"%s\",%d,\"%s\",\"%s\",%d",
					smsSrMsg->msg.msgSrData.fo,					// <fo>
					smsSrMsg->msg.msgSrData.msgRefNum,			// <mr>
					smsSrMsg->daoaAddress.Number,				// <ra>
					smsSrMsg->daoaAddress.TypeOfAddress,		// <tora>
					sctsString,									// <scts>
					dtString,									// <dt>
					smsSrMsg->msg.msgSrData.status);			// <st>
		}

		// Second step, buffer, discard or send formatted indication and Rp ack.
		operMode = V24_GetOperationMode((DLC_t)ch);
		if ( operMode == V24OPERMODE_AT_CMD || operMode == V24OPERMODE_NO_RECEIVE || maxChan == 1/*NON mux mode*/ )
		{
			atc_ManageCnmiMode(ch, (char *)rspBuf,
					(UInt16) strlen((char *)rspBuf) + 1, // Add 1 to include NULL character terminating the string
							ATC_SR_IND_TYPE,
							FALSE,                        // Stored Msg
							(SMS_GetNewMsgDisplayPref(SMS_DS) != 0)); // Send indication condition.
		}
	}
}


//------------------------------------------------------------------------------
//
// Function Name: AtHandleSmsStatusRptStoredRes (atc_Sr_Mt_Stored_CB)
//
// Description:   Storage status report results, build indication toward TA.
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleSmsStatusRptStoredRes(InterTaskMsg_t* inMsg)
{
	UInt8 ch = 0;
	SmsIncMsgStoredResult_t *PtrRes = (SmsIncMsgStoredResult_t*)inMsg->dataBuf;
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	if (PtrRes -> result == SIMACCESS_SUCCESS)
	{
		// First step, format indication.
		sprintf((char *)rspBuf, "+CDSI: %s,%d", MemStr[SR_STORAGE], PtrRes->rec_no);

		/* Second step, buffer, discard or send formatted indication and Rp ack. */
		atc_ManageCnmiMode (ch, (char *)rspBuf,
			(UInt16) strlen((char *)rspBuf) + 1, // Add 1 to include NULL character terminating the string
							ATC_SR_IND_TYPE,
							TRUE,                         // Stored Msg
							(SMS_GetNewMsgDisplayPref(SMS_DS) != 0)); // Send indication condition.

	}
	else
	{

		AtReportStoredError(PtrRes->result);


		_TRACE (( "AtHandleSmsStatusRptStoredRes : failed store SMS SR %x", PtrRes -> result));
	}
}

//------------------------------------------------------------------------------
//
// Function Name: AtHandleSMSCBStoredRsp (atc_CbMtStored)
//
// Description:   Handle storage SMS CB results, build indication toward TA.
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleSMSCBStoredRsp(InterTaskMsg_t* inMsg)
{
	UInt8	ch = 0;
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	SmsIncMsgStoredResult_t *PtrRes = (SmsIncMsgStoredResult_t*)inMsg->dataBuf;

	if ((PtrRes->result & MASK_SIM_ACCESS) == SIMACCESS_SUCCESS)
	{
		// format indication
		sprintf ((char *)rspBuf, "+CBMI: %s,%d", MemStr [BM_STORAGE], PtrRes->rec_no);

		// buffer, discard or send formatted indication.
		atc_ManageCnmiMode (ch, (char *)rspBuf,
			(UInt16) strlen((char *)rspBuf) + 1, // Add 1 to include NULL character terminating the string
							ATC_SMSCB_IND_TYPE,
							TRUE,                          // Stored msg
							(SMS_GetNewMsgDisplayPref(SMS_BM) != 0)); // Send indication condition.

	}
	else
		_TRACE (( "AtHandleSMSCBStoredRsp : failed store SMS CB %x", PtrRes->result));
}


//------------------------------------------------------------------------------
//
// Function Name: AtHandleCBDataInd (atc_SmsCBMtRx_CB)
//
// Description:   Manage reception of SMS Cell Broacast.
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleCBDataInd(InterTaskMsg_t* inMsg)
{
	UInt8	ch = 0;
	UInt8	maxChan;
	UInt8	rspBuf[ AT_RSPBUF_LG ] ;
	StoredSmsCb_t *Cbch = (StoredSmsCb_t*)inMsg->dataBuf;
	UInt8	operMode;

	if( MPX_ObtainMuxMode() == NONMUX_MODE )
		maxChan = 1;
	else
		maxChan = MAX_NORMAL_MPX_CH; // 4, ch 0-3 is the normal ch, ch 4 is DLC_STKTERMINAL

	for(ch = 0; ch < maxChan; ch++)  // for all the channels
	{
		sprintf ((char *)rspBuf, "+CBM: ");

		atc_DisplaySmsCbPage(ch, "+CBM", (UInt8 *)rspBuf, Cbch, SIMSMSMESGSTATUS_UNREAD);

		operMode = V24_GetOperationMode((DLC_t)ch);
		if ( operMode == V24OPERMODE_AT_CMD || operMode == V24OPERMODE_NO_RECEIVE || maxChan == 1/*NON mux mode*/ )
		{
			atc_ManageCnmiMode(ch, (char *)rspBuf,
					   (UInt16)strlen((char *)rspBuf) + 1, // Add 1 to include NULL character to terminate the string
					   ATC_SMSCB_IND_TYPE,
					   FALSE,
					   (SMS_GetNewMsgDisplayPref(SMS_BM) != 0));
		}
	}
}


//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CMGW_Handler (atc_Cmgw_CB)
//
// Description:         Process command AT+CMGW (Write Message)
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_CMGW_Handler(AT_CmdId_t cmdId,
							   UInt8 chan,
							   UInt8 accessType,
							   const UInt8* _P1,
							   const UInt8* _P2,
							   const UInt8* _P3)
{
	InterTaskMsg_t	*msg;
	at_cmgsCmd_t	*cmgs_data;

	msg = AllocInterTaskMsgFromHeap(MSG_SMS_COMPOSE_REQ, sizeof(at_cmgsCmd_t));
	cmgs_data = (at_cmgsCmd_t *) msg->dataBuf;

	cmgs_data->cmdId = cmdId;
	cmgs_data->chan = chan;
	cmgs_data->accessType = accessType;
	msg->clientID = AT_GetV24ClientIDFromMpxChan(chan);

	isRspForATWriteRequest = TRUE;

	if(_P1 != NULL)
		strcpy((char*)cmgs_data->_P1, (char*)_P1);
	else
		cmgs_data->_P1[0] = '\0';

	if(_P2 != NULL)
		strcpy((char*)cmgs_data->_P2, (char*)_P2);
	else
		cmgs_data->_P2[0] = '\0';

	if(_P3 != NULL)
		strcpy((char*)cmgs_data->_P3, (char*)_P3);
	else
		cmgs_data->_P3[0] = '\0';

	PostMsgToATCTask(msg);

	return AT_STATUS_PENDING;
}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CMGS_Handler (atc_Cmgs_CB)
//
// Description:         Process command AT+CMGS (Send Message)
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_CMGS_Handler(AT_CmdId_t cmdId,
							   UInt8 chan,
							   UInt8 accessType,
							   const UInt8* _P1,
							   const UInt8* _P2,
							   const UInt8* _P3)
{
	InterTaskMsg_t	*msg;
	at_cmgsCmd_t	*cmgs_data;

	msg = AllocInterTaskMsgFromHeap(MSG_SMS_COMPOSE_REQ, sizeof(at_cmgsCmd_t));
	cmgs_data = (at_cmgsCmd_t *) msg->dataBuf;

	cmgs_data->cmdId = cmdId;
	cmgs_data->chan = chan;
	cmgs_data->accessType = accessType;
	msg->clientID = AT_GetV24ClientIDFromMpxChan(chan);

	if(_P1 != NULL)
		strcpy((char*)cmgs_data->_P1, (char*)_P1);
	else
		cmgs_data->_P1[0] = '\0';

	if(_P2 != NULL)
		strcpy((char*)cmgs_data->_P2, (char*)_P2);
	else
		cmgs_data->_P2[0] = '\0';

	if(_P3 != NULL)
		strcpy((char*)cmgs_data->_P3, (char*)_P3);
	else
		cmgs_data->_P3[0] = '\0';

	PostMsgToATCTask(msg);

	return AT_STATUS_PENDING;
}

//------------------------------------------------------------------------------
//
// Function Name:       AT_HandleComposeCmd (atc_Cmgs_CB)
//
// Description:         Process command AT+CMGS (Send Message) and
//                      AT+CMGW (Store Message).
//
// Notes:
//
//------------------------------------------------------------------------------
void ATHandleComposeCmd(InterTaskMsg_t *msg)
{
	UInt8	        i;
	UInt8           status;
	UInt8			rspBuf[ AT_RSPBUF_SM ] ;
	at_cmgsCmd_t	*cmgs_data = (at_cmgsCmd_t *)msg->dataBuf;

	lastSmsCmdChan = cmgs_data->chan;

	switch(cmgs_data->accessType)
	{
		case AT_ACCESS_REGULAR:
		{
			if( (cmgs_data->_P1[0] == '\0') )
			{
				/* There is no data for first parameter */
				i = 0;
			}
			else
			{
				i = strlen((char*)cmgs_data->_P1);
			}

			if(GET_CMGF((DLC_t)cmgs_data->chan) == 0)
			{
				if((i == 0))
				{
					/* In PDU mode, AT+CMGS has one and only one
					 * parameter: PDU message legngth. AT+CMGW can have 2 parameters.
					 * The second parameter is allowed for compliance purposes only.
					 */
					AT_CmdRspError( cmgs_data->chan, AT_CMS_ERR_INVALID_PUD_MODE_PARM ); // 304
					return;
				}
				if ( (cmgs_data->cmdId != AT_CMD_CMGW) && (cmgs_data->_P2[0] != '\0') )
				{
					AT_CmdRspError( cmgs_data->chan, AT_CMS_ERR_INVALID_PUD_MODE_PARM ); // 304
					return;
				}
				else
				{
					Cmgs_Ctxt = (atc_Cmgs_Ctx_t*)OSHEAP_Alloc(sizeof(atc_Cmgs_Ctx_t));

					/* If we get here, P1 is non-null string */
					Cmgs_Ctxt->Length = atoi((char*)cmgs_data->_P1);

					Cmgs_Ctxt->Status = STAT_STO_UNSENT;

					Cmgs_Ctxt->Chan = cmgs_data->chan;
					Cmgs_Ctxt->Txt[0] = '\0';
				}
			}
			else
			{
				/* We are in text mode: AT+CMGS has at least one parameter and
				 * at most two parameters.
				 */
				if( (i > SMS_MAX_DIGITS) ||
					((cmgs_data->cmdId == AT_CMD_CMGS) && ((i == 0) || (cmgs_data->_P3[0] != '\0'))) )
				{
					AT_CmdRspError( cmgs_data->chan, AT_CMS_ERR_INVALID_TEXT_MODE_PARM ); // 305
					return;
				}

				/* Check the validity of status */
				if( (cmgs_data->_P3[0] != '\0') )
				{
					/* Must be AT+CMGW, search status list to match valid status */
					for(i = 0; i < 9; i++)
					{
						if (!strcmp(atc_SmsStateText[i], (char*)cmgs_data->_P3))
						{
							break;
						}
					}

					if (i >= 8)  /* i == 8 is "ALL", invalid */
					{
						AT_CmdRspError( cmgs_data->chan, AT_CMS_ERR_INVALID_TEXT_MODE_PARM ); // 305
						return;
					}

					status = atc_SmsStatePdu[i];
				}
				else
				{
					status = STAT_STO_UNSENT;
				}

				Cmgs_Ctxt = (atc_Cmgs_Ctx_t*)OSHEAP_Alloc(sizeof(atc_Cmgs_Ctx_t));

				if ( (cmgs_data->_P2[0] != '\0') )
				{
					/* Type of destination address. */
					Cmgs_Ctxt->toda = atoi((char*)cmgs_data->_P2);
				}
				else
				{
					Cmgs_Ctxt->toda = ATC_UNKNOWN_TOA;
				}

				Cmgs_Ctxt->Status = status;

				/* If destination address is not provided, default to 0 */
				strcpy( (char *) Cmgs_Ctxt->da, ((i > 0) ? (char *) cmgs_data->_P1 : "0") );
			}

			Cmgs_Ctxt->CmdId = cmgs_data->cmdId;

			Cmgs_Ctxt->Chan = cmgs_data->chan;
			Cmgs_Ctxt->Txt[0] = '\0';

			V24_SetOperationMode( (DLC_t)cmgs_data->chan, V24OPERMODE_AT_SMS );

			/* Send prompt sequence and wait text to be entered by user. */
			rspBuf [0] = 13;  // CR
			rspBuf [1] = 10;  // LF
			rspBuf [2] = '>';
			rspBuf [3] = ' ';
			rspBuf [4] = '\0';
			V24_PutTxBlock( (DLC_t)cmgs_data->chan, (BufferDatum_t *) rspBuf, (Int16) strlen((char *)rspBuf) );

			return;
		}

		case AT_ACCESS_TEST:
			AT_CmdCloseTransaction(cmgs_data->chan);
			break;

		default:
			AT_CmdRspError( cmgs_data->chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return;
			break;
	}
}


//------------------------------------------------------------------------------
//
// Function Name:       AtHandleSmsUsrDataInd (atc_Sms_Send_CB)
//
// Description:         Manage reception of sms text entered by TE user and send
//                      it over the air...
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleSmsUsrDataInd(InterTaskMsg_t* inMsg)
{
	#define NO_TERMINATOR 0xFF

	UInt8				rspBuf[ AT_RSPBUF_SM ] ;
	UInt8               Terminator;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	UInt16              RetCode = 0;
	UInt16				max_sms_pdu_str_len = 340;
	UInt8*				msg = (UInt8*)inMsg->dataBuf;
	Result_t			err;

	AT_ChannelCfg_t* channelCfg;

	if(Cmgs_Ctxt == NULL){
		// memory buffer already released earlier because input text exceeded limit.
		L1_LOG("\n\rAT SMS: Cmgs_Ctxt already released ");
		return;
	}

	channelCfg = AT_GetChannelCfg(Cmgs_Ctxt->Chan);

	Terminator = msg[inMsg->dataLength - 1];
	Cmgs_Ctxt->Chan = mpx_chan_num;

	switch ( Terminator )
	{
	case 26 : /* Text end with ^Z, so send SMS: Fall through to next case!! */
	case 13 : /* Text end with CR : it is not the end... */
		// Suppress Ctrl Z.
		msg[inMsg->dataLength - 1] = 0;
		atc_RmvBackSpace(msg);

		if (strlen((char*)msg) + strlen((char*)Cmgs_Ctxt->Txt) > max_sms_pdu_str_len)
		{
 			AT_CmdRspError(Cmgs_Ctxt->Chan, AT_CME_ERR_TXTSTR_TOO_LONG );
			V24_ForceEnterCmdMode((DLC_t)Cmgs_Ctxt->Chan);
 			CleanSmsResources();
			return;
		}
		else
		{
			strcat ((char *) Cmgs_Ctxt->Txt, (char *)msg);
		}

		if (Terminator == 13)
		{
			/* Send prompt sequence and wait text to be entered by user. */
			rspBuf [0] = 13;  // CR
			rspBuf [1] = 10;  // LF
			rspBuf [2] = '>';
			rspBuf [3] = ' ';
			rspBuf [4] = '\0';
			V24_PutTxBlock( (DLC_t)Cmgs_Ctxt->Chan, (BufferDatum_t *) rspBuf, (Int16) strlen((char *)rspBuf) );

			break;
		}

		/* Terminator is 26 : text end with ^Z, so send SMS */
		V24_SetOperationMode( (DLC_t)Cmgs_Ctxt->Chan, V24OPERMODE_AT_CMD );

		/* No break here: fall through to next case!!!! */

	case NO_TERMINATOR :

		if(Cmgs_Ctxt->CmdId != AT_CMD_CMGW)
		{
			if(SMS_IsSmsServiceAvail() == RESULT_ERROR)
			{
				// We are not registered in network, can not send SMS.
 				V24_SetOperationMode( (DLC_t) mpx_chan_num, V24OPERMODE_AT_CMD );

				AT_CmdRspError( mpx_chan_num, AT_CME_ERR_NO_NETWORK_SVC );
				CleanSmsResources();

				return;
			}
		}

		if (GET_CMGF(Cmgs_Ctxt->Chan) == 0) /* PDU mode. */
		{
			atc_411Addr_t Sca;
			UInt8        ScaSize = 0;
			UInt8 *Pdu   = (UInt8 *) OSHEAP_Alloc (SMSMESG_DATA_SZ);

			/* Look for Service center address. */
			RetCode = UTIL_HexStrToHexData((UInt8 *) &ScaSize, (UInt8 *) &Cmgs_Ctxt -> Txt [0], 2);

			if ( ((UInt16) strlen ((char *)Cmgs_Ctxt -> Txt) < (10 + ScaSize*2)) || (RetCode) )
			{
				AT_CmdRspError( Cmgs_Ctxt->Chan, AT_CMS_ERR_INVALID_PUD_MODE_PARM ) ;
				OSHEAP_Delete (Pdu);
				CleanSmsResources();
				return;
			}

			if (ScaSize != 0)
			{
				RetCode = UTIL_HexStrToHexData((UInt8 *) &Sca.Toa,
							(UInt8 *) &Cmgs_Ctxt -> Txt [2], (UInt16) (ScaSize * 2));

				Sca.Len = (UInt8) ScaSize;
			}
			else
			{
				if(SIM_GetSmsSca((UInt8 *) &Sca, MS_GetDefaultSMSParamRecNum()) != RESULT_OK)
				{
					AT_CmdRspError(Cmgs_Ctxt->Chan, AT_CME_ERR_SIM_BUSY);
					OSHEAP_Delete (Pdu);
					CleanSmsResources();
					return;
				}

				// further protection in case the SCA in SIM is corrupted
				if(SMS_ValidateSCA((UInt8*)&Sca, TRUE) != RESULT_OK){
					_TRACE(("SMS: CMGS: Invalid SCA address"));
					AT_CmdRspError(Cmgs_Ctxt->Chan, AT_CMS_ERR_SMSC_ADDR_UNKNOWN);
					OSHEAP_Delete (Pdu);
					CleanSmsResources();
					return;
				}
			}

			/* Convert pdu from Hex to Pdu (Rq : Tpdu begins after Sca). */
			RetCode |= UTIL_HexStrToHexData((UInt8 *) Pdu,
							(UInt8 *) &Cmgs_Ctxt -> Txt [(ScaSize+1) * 2],
							(UInt16) (strlen ((char *) Cmgs_Ctxt -> Txt) - (ScaSize+1) * 2));

			// send the sms buffer to SMS module
			if (Cmgs_Ctxt->CmdId == AT_CMD_CMGS)
			{
				err = SMS_SendSMSPduReq(inMsg->clientID, Cmgs_Ctxt->Length, Pdu, (Sms_411Addr_t*)&Sca);
				if(err != RESULT_OK)
					AT_CmdRspError( Cmgs_Ctxt->Chan, AT_SmsErrorToAtError(err, FALSE) );

				CleanSmsResources();
				OSHEAP_Delete (Pdu);
				return;
			}
			else if (Cmgs_Ctxt->CmdId == AT_CMD_CMGW){
				err = SMS_WriteSMSPduReq(inMsg->clientID, Cmgs_Ctxt->Length, Pdu,
											(Sms_411Addr_t*)&Sca, (SmsStorage_t)channelCfg->CPMS[0]);
				if(err != RESULT_OK)
					AT_CmdRspError( Cmgs_Ctxt->Chan, AT_SmsErrorToAtError(err, FALSE) );

				CleanSmsResources();
				OSHEAP_Delete (Pdu);
				return;
			}

			CleanSmsResources();
			OSHEAP_Delete (Pdu);
		}
		else /* Text mode. */
		{
			// send the sms buffer to SMS module
			if (Cmgs_Ctxt->CmdId == AT_CMD_CMGS)
			{
				Result_t err;

				err = SMS_SendSMSReq(inMsg->clientID, Cmgs_Ctxt->da, Cmgs_Ctxt->Txt, NULL, NULL);
				if(err != RESULT_OK)
					AT_CmdRspError( Cmgs_Ctxt->Chan, AT_SmsErrorToAtError(err, TRUE) );

				CleanSmsResources();
				return;
			}
			else if (Cmgs_Ctxt->CmdId == AT_CMD_CMGW)
			{
				err = SMS_WriteSMSReq(inMsg->clientID, Cmgs_Ctxt->da, Cmgs_Ctxt->Txt,
											NULL, NULL, (SmsStorage_t)channelCfg->CPMS[0]);
				if(err != RESULT_OK)
					AT_CmdRspError( Cmgs_Ctxt->Chan, AT_SmsErrorToAtError(err, TRUE) );

				CleanSmsResources();
				return;
			}
		}

		CleanSmsResources();
		break;

	case 27 : /* Text end with ESC, abort the command. */

		AT_CmdRspOK(Cmgs_Ctxt->Chan);

		CleanSmsResources();

		break;

	default :	 // No terminator, abnormal case.
		AT_CmdRspError( Cmgs_Ctxt->Chan, AT_CMS_ERR_UNKNOWN_ERROR); // this case might never happen
		CleanSmsResources();
       _TRACE (("No terminator."));
	}
}

//------------------------------------------------------------------------------
//
// Function Name:       AtHandleSmsReportInd
//
// Description:         Called by SMS for SMS Report Indication event.
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleSmsReportInd(InterTaskMsg_t* inMsg)
{
	UInt8			chan;
	SmsReportInd_t*	SmsRpt = (SmsReportInd_t*)inMsg->dataBuf;
	UInt8			tmpChan = AT_GetCmdRspMpxChan(AT_CMD_CMGS);

	chan = (tmpChan == INVALID_MPX_CHAN) ? AT_GetCmdRspMpxChan(AT_CMD_CMSS) : tmpChan;

	// if the command transaction is already closed, use the last sms command channel
	if(chan == INVALID_MPX_CHAN){
		chan = lastSmsCmdChan;
		lastSmsCmdChan = INVALID_MPX_CHAN;
	}

	// there's no description in 07.05 on how to display this type of message, not
	// to show to AT port for now
/*
	if ( SmsRpt->cause == MN_SMS_TIMER_EXPIRED )
		AT_CmdRspCMSError(chan, 528 );					// 0x210 TBC.
	else
		AT_CmdRspCMSError(chan, 544 + SmsRpt->cause );  // 0x220 +.. TBC.
	// Useful if RP ACK must not be sent to MN SMS in this case. TBC
	atc_NextCmdIsCnma = 0;
	atc_SendRpAck     = 0;
	atc_SmsWaitedCnma = 0;
	SMS_SetNewMsgDisplayPref(SMS_MT, 0);
	SMS_SetNewMsgDisplayPref(SMS_DS, 0);
*/

	_TRACE (("Sms submit : SMS Report Indication type = ", SmsRpt->type));
	_TRACE (("Sms submit : SMS Report Indication cause = ", SmsRpt->cause));
}

//------------------------------------------------------------------------------
//
// Function Name:       AtHandleSmsSubmitRsp
//
// Description:         Called by SMS when message has been sent.
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleSmsSubmitRsp(InterTaskMsg_t* inMsg)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	SmsSubmitRspMsg_t*   SmsRsp = (SmsSubmitRspMsg_t*)inMsg->dataBuf;
	UInt8 chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	AT_CmdId_t cmdId = AT_GetCmdIdByMpxChan(chan);

	switch(SmsRsp->submitRspType)
	{
		case SMS_SUBMIT_RSP_TYPE_INTERNAL:
			AT_CmdRspError(chan, AT_SmsErrorToAtError(SmsRsp->InternalErrCause, GET_CMGF((DLC_t)chan)));
			break;

		case SMS_SUBMIT_RSP_TYPE_SUBMIT:
			if( SmsRsp->NetworkErrCause == MN_SMS_NO_ERROR ){
				sprintf ((char *)rspBuf, "%s: %d", AT_GetCmdName(cmdId), SMS_GetLastTpMr() );

				AT_OutputStr((DLC_t)chan, (UInt8 *)rspBuf );
				AT_CmdRspOK(chan);
			}
			else{
				AT_CmdRspCMSError(chan, SmsRsp->NetworkErrCause);
			}
			break;

		case SMS_SUBMIT_RSP_TYPE_PARAMETER_CHECK:
			_TRACE(("AtHandleSmsSubmitRsp: PARAM CHECK Failed: cause = ", SmsRsp->NetworkErrCause));

			AT_CmdRspError( chan, AT_SmsErrorToAtError(SMS_INVALID_SMS_PARAM, (GET_CMGF(chan) == 1)) );

			break;

		default:
			break;
	}
}

//------------------------------------------------------------------------------
//
// Function Name: AtHandleWriteSmsStoredRes (atc_Sms_Stored_CB)
//
// Description:   Storage write SMS results, build indication toward TA.
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleWriteSmsStoredRes(InterTaskMsg_t* inMsg)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	SmsIncMsgStoredResult_t *ptrRes = (SmsIncMsgStoredResult_t*)inMsg->dataBuf;
	UInt8 chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);

	_TRACE(("SMS: AtHandleWriteSmsStoredRes: isRspForATWriteRequest = ", isRspForATWriteRequest));

	if (ptrRes -> result == SIMACCESS_SUCCESS)
	{
		//format indication.
		sprintf ((char *)rspBuf, "%s: %d", AT_GetCmdName(AT_CMD_CMGW), ptrRes->rec_no);
		AT_OutputUnsolicitedStr ((UInt8*)rspBuf);

		// only to send "OK" to AT port if the response is to a write request from AT
		if(isRspForATWriteRequest){
			AT_CmdRspOK(chan);
		}
	}
	else
	{
		AtReportStoredError(ptrRes->result);
		if(isRspForATWriteRequest){
			AT_CmdCloseTransaction(chan);
		}
	}

	isRspForATWriteRequest = FALSE;
}


//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CSAS_Handler (ATC_Csas_CB)
//
// Description:         Process command AT+CSAS : save profile
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_CSAS_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	UInt8	NoProfile = 0xFF;

	switch(accessType)
	{
		case AT_ACCESS_REGULAR:
			NoProfile = atoi ((char*)_P1 );

			if (SMS_SaveSmsServiceProfile (NoProfile) == RESULT_OK)
			{
				sprintf ((char *)rspBuf, "%s: %d", AT_GetCmdName(AT_CMD_CSAS), NoProfile);

				AT_OutputStr (chan, (UInt8*)rspBuf );
			}
			break;

		case AT_ACCESS_READ:
			AT_CmdRspError(chan, AT_CMS_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
			break;

		case AT_ACCESS_TEST:
			sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CSAS), AT_GetValidParmStr(cmdId));
			AT_OutputStr (chan, (UInt8*)rspBuf );
			break;

		default:
			break;
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CRES_Handler (ATC_Cres_CB)
//
// Description:         Process command AT+CRES. Restore profile.
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_CRES_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	UInt8	NoProfile = 0xFF;

	switch(accessType)
	{
		case AT_ACCESS_REGULAR:
			NoProfile = atoi ((char*)_P1 );

			if (SMS_RestoreSmsServiceProfile (NoProfile) == RESULT_OK)
			{
				sprintf ((char *)rspBuf, "%s: %d", AT_GetCmdName(AT_CMD_CRES), NoProfile);

				AT_OutputStr (chan, (UInt8*)rspBuf );
			}
			else
			{
				AT_CmdRspError(chan, AT_CMS_ERR_OP_NOT_ALLOWED );
				return AT_STATUS_DONE;
			}
			break;

		case AT_ACCESS_READ:
			AT_CmdRspError(chan, AT_CMS_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
			break;

		case AT_ACCESS_TEST:
			sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CRES), AT_GetValidParmStr(cmdId));
			AT_OutputStr (chan, (UInt8*)rspBuf );
			break;

		default:
			break;
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CSCB_Handler (atc_Cscb_CB)
//
// Description:         Process command AT+CSCB. Select cell broadcast messsage.
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_CSCB_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								const UInt8* _P1, const UInt8* _P2, const UInt8* _P3)
{
	UInt8	mode;
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	UInt8	clientID = AT_GetV24ClientIDFromMpxChan(chan);
	char	midsStr[100];
	char	langsStr[50];
	Result_t res;

	lastSmsCmdChan = chan;

	switch(accessType)
	{
		case AT_ACCESS_REGULAR:

			mode = atoi ( (char*)_P1 );

			if (mode == 1)
			{
				if( (_P2 == NULL) && (_P3 == NULL) ){ // stop Cell Broadcast

					res = SMS_SetCellBroadcastMsgTypeReq(clientID, mode, NULL, NULL);

					if(res == RESULT_OK){

						Mode = mode;

						return AT_STATUS_PENDING;
					}
					else{
						_TRACE(("SMS CSCB: Stop CB failed"));
						AT_CmdRspError(chan,  AT_SmsErrorToAtError(res, FALSE));
						return AT_STATUS_DONE;
					}
				}
				else{  // remove mid or lang

					res = SMS_SetCellBroadcastMsgTypeReq(clientID, mode, (UInt8*)_P2, (UInt8*)_P3);

					if(res != RESULT_OK){
						_TRACE(("SMS CSCB: remove mid/lang failed"));
						AT_CmdRspError(chan,  AT_SmsErrorToAtError(res, FALSE));
						return AT_STATUS_DONE;
					}
					else{
						return AT_STATUS_PENDING;
					}
				}
			}
			else	// start Cell Broadcast
			{
				// Remove all existing channels before add new channels (to be consistent with
				// the "overwrite" experience in Nokia/Moto reference phones
				if( _P2 != NULL ){
					SMS_RemoveAllCBChnlFromSearchList();
				}

				res = SMS_SetCellBroadcastMsgTypeReq (clientID, mode, (UInt8*)_P2, (UInt8*)_P3);

				if (res != RESULT_OK){

					_TRACE(("SMS CSCB: Start CB failed"));
					AT_CmdRspError(chan,  AT_SmsErrorToAtError(res, FALSE) );
					return AT_STATUS_DONE;
				}

				Mode = mode;

				return AT_STATUS_PENDING;
			}
			break;

		case AT_ACCESS_READ:
			AtFormatCurrentLangsString(langsStr);
			res = AtFormatCurrentMidsString(midsStr);
			if(res != RESULT_OK){
				AT_CmdRspError(chan, AT_SmsErrorToAtError(res, FALSE));
				return AT_STATUS_DONE;
			}

			sprintf((char *)rspBuf, "%s: %d,\"%s\",\"%s\"",
					AT_GetCmdName(AT_CMD_CSCB),
					Mode, midsStr, langsStr);

			AT_OutputStr (chan, (UInt8*)rspBuf );
			AT_CmdRspOK(chan);
			break;

		case AT_ACCESS_TEST:
			sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CSCB), AT_GetValidParmStr(cmdId));
			AT_OutputStr (chan, (UInt8*)rspBuf );
			AT_CmdRspOK(chan);
			break;

		default:
			break;
	}

	return AT_STATUS_DONE;
}

static Result_t AtFormatCurrentMidsString(char* midsStr)
{
	UInt8 i;
	Result_t res;
	T_MN_CB_MSG_IDS cbMids;

	res = SMS_GetCBMI(&cbMids);

	if(res != RESULT_OK)
		return res;

	midsStr[0] = '\0';

	for(i=0; i<cbMids.nbr_of_msg_id_ranges; i++){
		if(cbMids.msg_id_range_list.A[i].stop_pos > cbMids.msg_id_range_list.A[i].start_pos){
			sprintf(midsStr+strlen(midsStr), "%d-%d,", cbMids.msg_id_range_list.A[i].start_pos,
															cbMids.msg_id_range_list.A[i].stop_pos);
		}
		else{
			sprintf(midsStr+strlen(midsStr), "%d,", cbMids.msg_id_range_list.A[i].start_pos);
		}
	}

	// remove the last comma
	if(cbMids.nbr_of_msg_id_ranges > 0)
		midsStr[strlen(midsStr)-1] = '\0';

	return RESULT_OK;
}

static void AtFormatCurrentLangsString(char* langsStr)
{
	UInt8 i;
	T_MN_CB_LANGUAGES langs;

	SMS_GetCbLanguage(&langs);

	langsStr[0] = '\0';

	if(langs.nbr_of_languages > 0){
		for(i=0; i<langs.nbr_of_languages; i++){
			sprintf(langsStr+strlen(langsStr), "%d,", langs.language_list.A[i]);
		}

		// remove the last comma
		langsStr[strlen(langsStr)-1] = '\0';
	}
}

//------------------------------------------------------------------------------
//
// Function Name: AtHandleCBStartStopRsp (response to atc_Cscb_CB)
//
// Description:   Manage reception of SMS CB Start/Stop response.
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleCBStartStopRsp(InterTaskMsg_t* inMsg)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	char	midsStr[100];
	char	langsStr[50];
	SmsCBMsgRspType_t *msgType = (SmsCBMsgRspType_t*)inMsg->dataBuf;
	UInt8	chan = AT_GetCmdRspMpxChan(AT_CMD_CSCB);

	AtFormatCurrentMidsString(midsStr);
	AtFormatCurrentLangsString(langsStr);

	sprintf((char *)rspBuf, "%s: %d,\"%s\",\"%s\"",
			AT_GetCmdName(AT_CMD_CSCB),
			Mode, midsStr, langsStr);
	switch (msgType->actType)
	{
		case SMS_CB_START_CNF :
		case SMS_CB_STOP_CNF :
			AT_OutputStr (chan, (UInt8*)rspBuf );
			AT_CmdRspOK(chan);
			break;

		case SMS_CB_START_REJ :
		case SMS_CB_STOP_REJ :
			AT_CmdRspError(chan,  AT_CMS_ERR_OP_NOT_SUPPORTED );
			break;

		default:
			_TRACE(("SMS: AtHandleCBStartStopRsp: msgType->actType = ", msgType->actType));
			AT_CmdRspError(chan,  AT_CMS_ERR_OP_NOT_SUPPORTED );
			break;
	}
}

//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CMSS_Handler (ATC_Cmss_CB)
//
// Description:         Transition on AT+CMSS
//
// Notes:
//
//------------------------------------------------------------------------------
AT_Status_t ATCmd_CMSS_Handler(AT_CmdId_t cmdId,
							   UInt8 chan,
							   UInt8 accessType,
							   const UInt8* _P1,
							   const UInt8* _P2,
							   const UInt8* _P3)
{
	Result_t res;
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	UInt8	RecNo   = atoi((char*)_P1);
	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg(chan);
	UInt8	clientID = AT_GetV24ClientIDFromMpxChan(chan);

	if(accessType == AT_ACCESS_READ){
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
		return AT_STATUS_DONE;
	}
	else if(accessType == AT_ACCESS_TEST){
		sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(AT_CMD_CMSS), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, (UInt8*)rspBuf );
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}

	if ((_P2) && (strlen((char*)_P2) > SMS_MAX_DIGITS)) // string too long
	{
		AT_CmdRspError(chan, AT_CMS_ERR_SMSC_ADDR_UNKNOWN ); // Destination Address unknown
		return AT_STATUS_DONE;
	}

	res = SMS_SendStoredSmsReq (clientID, (SmsStorage_t)channelCfg->CPMS [1], RecNo);

	if(res != RESULT_OK){
		AT_CmdRspError(chan, AT_SmsErrorToAtError(res, GET_CMGF((DLC_t)chan)));
	}
	else{
		return AT_STATUS_PENDING;
	}

	return AT_STATUS_DONE;
}


//------------------------------------------------------------------------------
//
// Function Name:       ATCmd_CGSMS_Handler (ATC_Cgsms_CB)
//
// Description:         Transition on AT+CGSMS
//
// Notes:				SMS bearer type will be saved only in smsapi for
//						data consistency.
//
//------------------------------------------------------------------------------

AT_Status_t ATCmd_CGSMS_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	if (accessType == AT_ACCESS_REGULAR)
	{
		SetSMSBearerPreference((SMS_BEARER_PREFERENCE_t)atoi((char*)_P1));
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char *)rspBuf, "%s: %d", AT_GetCmdName(cmdId), GetSMSBearerPreference() );
		AT_OutputStr (chan, (UInt8*)rspBuf );
	}
	else  // accessType == AT_ACCESS_TEST
	{
		sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, (UInt8*)rspBuf );
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}


//------------------------------------------------------------------------------
//
// Function Name:       CleanSmsResources
//
// Description:         Release allocated memory and reset SMS state
//
// Notes:
//
//------------------------------------------------------------------------------
static void CleanSmsResources(void)
{
	if(Cmgs_Ctxt != NULL){
		OSHEAP_Delete(Cmgs_Ctxt);
		Cmgs_Ctxt = NULL;
	}
}

//******************************************************************************
//
// Function Name:	ATCmd_MVMIND_Handler (ATC_Mvmind_CB)
//
// Description:		Transition on AT+VMIND. message waiting indication
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MVMIND_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	Result_t res;
	SmsVoicemailInd_t vmInd;
	Boolean sendUnsolicitVMInd = FALSE;

	if (accessType == AT_ACCESS_REGULAR)
	{
		if(atoi((char*)_P1) != 2)
		{
			if( !GET_MVMIND && (atoi((char*)_P1) == 1) ){
				sendUnsolicitVMInd = TRUE;
			}

			SET_MVMIND( atoi((char*)_P1) );
			SMS_SetVMIndOnOff( atoi((char*)_P1) );
			AT_CmdRspOK(chan);

			// in the case the vm ind was not on, unsolicited vmind will be sent here
			if(sendUnsolicitVMInd && SMS_IsCachedDataReady() && !isVMindSent){

				res = SMS_GetVMWaitingStatus(&vmInd);

				if(res == RESULT_OK){
					// send unsolicited event
#if !defined(GCXX_PC_CARD)
					sprintf((char *)rspBuf, "%s: 1, %d, %d, %d, %d, %d, %d", AT_GetCmdName(AT_CMD_MVMIND),
							vmInd.staL1,vmInd.staL2,vmInd.staFax,vmInd.staData,vmInd.msgType,vmInd.msgCount);
#else
					sprintf((char *)rspBuf, "%s: 1, %d, %d, %d, %d", AT_GetCmdName(AT_CMD_MVMIND),
							vmInd.staL1,vmInd.staL2,vmInd.staFax,vmInd.staData);
#endif

					AT_OutputUnsolicitedStr((UInt8*)rspBuf);
					isVMindSent = TRUE;
				}
			}
		}
		else
		{
			res = SMS_GetVMWaitingStatus(&vmInd);

			if(res == RESULT_OK){
				sprintf((char *)rspBuf, "%s: 0, %d, %d, %d, %d", AT_GetCmdName(AT_CMD_MVMIND),
								vmInd.staL1,vmInd.staL2,vmInd.staFax,vmInd.staData);

				AT_OutputStr (chan, (UInt8*)rspBuf);
				AT_CmdRspOK(chan);
			}
			else{
				AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY );
			}
		}
	}
	else if( accessType == AT_ACCESS_READ)
	{

		sprintf((char *)rspBuf, "%s: %d", AT_GetCmdName(cmdId), GET_MVMIND);
		AT_OutputStr (chan, (UInt8*)rspBuf );
		AT_CmdRspOK(chan);
	}
	else if( accessType == AT_ACCESS_TEST)
	{
		sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, (UInt8*)rspBuf );
		AT_CmdRspOK(chan);
	}

	return AT_STATUS_DONE;
}

//******************************************************************************
//
// Function Name:		AtSendVMWaitingInd
//
// Description:			In the case the vmind was sent before but was blocked because of
//						any reason, the indication is sent again when SMS module is ready.
//
// Notes:
//
//******************************************************************************
void AtSendVMWaitingInd(void)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	SmsVoicemailInd_t vmInd;
	Result_t res;

	// in the case the vm ind was not on, unsolicited vmind will be sent here
	if(GET_MVMIND && !isVMindSent && SMS_IsCachedDataReady()){

		res = SMS_GetVMWaitingStatus(&vmInd);

		if(res == RESULT_OK){
			// send unsolicited event
#if !defined(GCXX_PC_CARD)
			sprintf((char *)rspBuf, "%s: 1, %d, %d, %d, %d, %d, %d", AT_GetCmdName(AT_CMD_MVMIND),
					vmInd.staL1,vmInd.staL2,vmInd.staFax,vmInd.staData,vmInd.msgType,vmInd.msgCount);
#else
			sprintf((char *)rspBuf, "%s: 1, %d, %d, %d, %d", AT_GetCmdName(AT_CMD_MVMIND),
					vmInd.staL1,vmInd.staL2,vmInd.staFax,vmInd.staData);
#endif

			AT_OutputUnsolicitedStr((UInt8*)rspBuf);
			isVMindSent = TRUE;
		}
		else{
			_TRACE(("SMS AT: AtSendVMWaitingInd: res = ", res));
		}
	}
	else{
		_TRACE(("SMS AT: AtSendVMWaitingInd: GET_MVMIND|isVMindSent|SMS_IsCachedDataReady() = ",
			(0xFF000000 | (GET_MVMIND << 16) | (isVMindSent << 8) | SMS_IsCachedDataReady()) ));
	}
}

//******************************************************************************
//
// Function Name:		AtHandleVMWaitingInd
//
// Description:			This handle the MSG_VM_WAITING_IND signal and display
//						the voicemail indication.
//
// Notes:
//
//******************************************************************************
void AtHandleVMWaitingInd(InterTaskMsg_t *inMsg)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	SmsVoicemailInd_t* ind = (SmsVoicemailInd_t*)inMsg->dataBuf;


	if(GET_MVMIND && SMS_IsCachedDataReady())
	{
		// send unsolicited event
#if !defined(GCXX_PC_CARD)
		sprintf((char *)rspBuf, "%s: 1, %d, %d, %d, %d, %d, %d", AT_GetCmdName(AT_CMD_MVMIND),
						ind->staL1,ind->staL2,ind->staFax,ind->staData,ind->msgType,ind->msgCount);
#else
		sprintf((char *)rspBuf, "%s: 1, %d, %d, %d, %d", AT_GetCmdName(AT_CMD_MVMIND),
						ind->staL1,ind->staL2,ind->staFax,ind->staData);
#endif

		AT_OutputUnsolicitedStr((UInt8*)rspBuf);
		isVMindSent = TRUE;
	}
}

//******************************************************************************
//
// Function Name:	ATCmd_MVMSC_Handler (ATC_Mvmsc_CB)
//
// Description:		Transition on AT*MVMSC. Voice mail service center
//
// Notes:			If a parameter is entered, only the VMSC of that line is
//					returned; if no parameter is entered, all the avaliable
//					VMSC numbers will be returned.
//
//******************************************************************************
AT_Status_t ATCmd_MVMSC_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1,
								 const UInt8* _P2, const UInt8* _P3, const UInt8* _P4)
{
	UInt8	i;
	UInt8	line;
	UInt8	numRec;
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;
	SmsAddress_t vmsc;
	Result_t res;
	UInt8	numType;
	UInt8	clientID = AT_GetV24ClientIDFromMpxChan(chan);

	if (accessType == AT_ACCESS_REGULAR)
	{
		// Update VMSC number
		//
		if( (_P2 != NULL) && (atoi((char*)_P2) == 1) ){

			_TRACE((" AT SMS: SMS_UpdateVmscNumberReq called: ", _P3[0]));

			if(_P4){
				numType = atoi((char*)_P4);
			}
			else{
				numType = 0xFF;
			}

			res = SMS_UpdateVmscNumberReq(clientID, atoi((char*)_P1), (UInt8*)_P3, numType, NULL, 0xFF, 0);

			if(res != RESULT_OK){
				AT_CmdRspError(chan, AT_SmsErrorToAtError(res, FALSE));
			}
			else{
				return AT_STATUS_PENDING;
			}

			return AT_STATUS_DONE;
		}

		// Read VMSC number
		//
		res = SMS_GetNumOfVmscNumber(&numRec);

		if(res == SMS_SIM_BUSY){
			AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY );
			return AT_STATUS_DONE;
		}

		if(_P1){
			line = atoi((char*)_P1);
			numRec = 1;
		}

		for(i = 0; i < numRec; i++){
			if(_P1)
				res = SMS_GetVmscNumber(line, &vmsc);	// index is 1-based
			else
				res = SMS_GetVmscNumber(i+1, &vmsc);	// index is 1-based

			if(res == SMS_INVALID_INDEX){
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
				return AT_STATUS_DONE;
			}
			else{
				if(res == RESULT_OK){
					if(vmsc.TypeOfAddress == INTERNA_TON_ISDN_NPI){
						sprintf((char *)rspBuf, "%s: 1, \"+%s\", %d", AT_GetCmdName(cmdId), vmsc.Number, vmsc.TypeOfAddress);
					}
					else{
						sprintf((char *)rspBuf, "%s: 1, \"%s\", %d", AT_GetCmdName(cmdId), vmsc.Number, vmsc.TypeOfAddress);
					}
				}
				else{
					sprintf((char *)rspBuf, "%s: 0", AT_GetCmdName(cmdId));
				}

				AT_OutputStr (chan, (UInt8*)rspBuf );
			}
		}

		AT_CmdRspOK(chan);
	}
	else if( accessType == AT_ACCESS_READ){

		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
	}
	else if( accessType == AT_ACCESS_TEST){

		sprintf((char *)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, (UInt8*)rspBuf );
		AT_CmdRspOK(chan);
	}

	return AT_STATUS_DONE;
}

//------------------------------------------------------------------------------
//
// Function Name:       AtHandleVmscUpdateRsp
//
// Description:         Process result of VMSC update
//
// Notes:
//
//------------------------------------------------------------------------------
void AtHandleVmscUpdateRsp(InterTaskMsg_t* inMsg)
{
	SIM_EFILE_UPDATE_RESULT_t *vmsc_update = (SIM_EFILE_UPDATE_RESULT_t *)inMsg->dataBuf;
	UInt8	chan = AT_GetCmdRspMpxChan(AT_CMD_MVMSC);

	if (vmsc_update->result == SIMACCESS_SUCCESS)
	{
		AT_CmdRspOK(chan);
	}
	else
	{
		AT_CmdRspError(chan, AT_CMS_ERR_OP_NOT_ALLOWED);
	}
}

//******************************************************************************
//
// Function Name:		AT_SmsErrorToAtError
//
// Description:			Convert SMS error type to AT error type
//
// Notes:
//
//******************************************************************************
UInt16 AT_SmsErrorToAtError(UInt16 err, Boolean cmgfMode)
{
	_TRACE(("SMS operation error = ", err));

	switch(err)
	{
		case RESULT_ERROR:
		case SMS_OPERATION_NOT_ALLOWED:
			return AT_CMS_ERR_OP_NOT_ALLOWED;

		case SMS_NO_SERVICE:
			return AT_CME_ERR_NO_NETWORK_SVC;

		case SMS_ADDR_NUMBER_STR_TOO_LONG:
			return AT_CME_ERR_DIAL_STR_TOO_LONG;

		case SMS_SIM_BUSY:
			return AT_CME_ERR_SIM_BUSY;

		case SMS_INVALID_INDEX:
			return AT_CMS_ERR_INVALID_MEMORY_INDEX;

		case SMS_INVALID_SMS_PARAM:
			if(cmgfMode == FALSE)
				return AT_CMS_ERR_INVALID_PUD_MODE_PARM;
			else
				return AT_CMS_ERR_INVALID_TEXT_MODE_PARM;

		case SMS_INVALID_TEXT_LENGTH:
			return AT_CMS_ERR_INVALID_TEXT_MODE_PARM;

		case SMS_INVALID_PDU_LENGTH:
		case SMS_INVALID_PDU_MODE_PARM:
			return AT_CMS_ERR_INVALID_PUD_MODE_PARM;

		case SMS_SCA_NOT_SUPPORTED_IN_SIM:
		case SMS_SCA_NULL_STRING:
		case SMS_INVALID_SCA_ADDRESS:
		case SMS_SCA_INVALID_CHAR_INSTR:
			return AT_CMS_ERR_SMSC_ADDR_UNKNOWN;

		case SMS_INVALID_DIALSTR_CHAR:
			return AT_CME_ERR_INVALID_DIALSTR_CHAR;

		case SMS_SIM_NOT_INSERT:
			return AT_CME_ERR_PH_SIM_NOT_INSERTED;

		case SMS_CALL_CONTROL_BARRING:
			return AT_CMS_SATK_CALL_CONTROL_BARRING;

		case SMS_FDN_NOT_ALLOWED:
			return AT_CMS_FDN_NOT_ALLOWED;

		case SMS_CB_MIDS_DOES_NOT_EXIST:
			return AT_CME_ERR_INVALID_INDEX;

		case SMS_CB_MIDS_EXCEED_RANGE_LIMIT:
			return AT_CME_ERR_MEMORY_FULL;

		case SMS_ME_SMS_NOT_SUPPORTED:
			return AT_CMS_ERR_ME_FAILURE;

		default:
			break;
	}

	return AT_CMS_ERR_UNKNOWN_ERROR;
}
