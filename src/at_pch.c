//******************************************************************************
//
// Description:
//
// $RCSfile: atc_pch.c $
//
//******************************************************************************

#define ENABLE_LOGGING

#include "mti_trace.h"

#include "stdio.h"
#include "stdlib.h"
#include "v24.h"
#include "nvram.h"
#include "sysparm.h"
#include "at_util.h"
#include "at_data.h"
#include "at_cfg.h"
#include "at_call.h"
#include "at_plmn.h"
#include "at_pch.h"
#include "atc.h"
#include "serial.h"

#include "mstypes.h"
#include "pchtypes.h"

#include "pchfuncs.h"
#include "log.h"
#include "mti_build.h"
#include "pchapi.h"
#include "ostask.h"
#include "at_state.h"

#include "ms_database.h"

#define TFT_MAX_PORT_VAL        65535
#define TFT_MAX_PORT_RANGE      5
#define TFT_MAX_TOS_VAL         255
#define TFT_MAX_TOS_RANGE       3
#define TFT_MAX_ADDR_MASK       255

extern void PPP_ReportPPPCloseInd(					// Open indication from ATC
	UInt8   cid										// Data link cconnection
	);
extern Boolean PPP_Set_AuthProt(
	UInt8 option
	);
extern UInt8 PPP_Get_AuthProt(
	void
	);



extern	UInt8	atcClientID;


static Result_t parse_dot(UInt8 range, UInt32 max, char* in, UInt16* pVal1, UInt16* pVal2);

//******************************************************************************
// Function Name: 	convertSDUErrRatio
//
// Description:	  	Convert the ATC SDU Error Ratio value to stack value.
// Notes:
//******************************************************************************
UInt8 convertSDUErrRatio(char *string)
{
	if (!strcmp(string, "1E2")) {
		return (0x01);
	}
	else if (!strcmp(string, "7E3")) {
		return (0x02);
	}
	else if (!strcmp(string, "1E3")) {
		return (0x03);
	}
	else if (!strcmp(string, "1E4")) {
		return (0x04);
	}
	else if (!strcmp(string, "1E5")) {
		return (0x05);
	}
	else if (!strcmp(string, "1E6")) {
		return (0x06);
	}
	else if (!strcmp(string, "1E1")) {
		return (0x07);
	}

	return (SUB_SDU_ERROR_RATIO);

}

//******************************************************************************
// Function Name: 	revertSDUErrRatio
//
// Description:	  	Revert the stack SDU Error Ratio value to ATC value.
// Notes:
//******************************************************************************
const UInt8* revertSDUErrRatio(UInt8 value)
{
	const UInt8*	str = 0;

	MSG_LOGV("ATC:revertSDUErrRatio:value=",value);

		switch (value) {
		case 0x01:
			str = (const UInt8*)"1E2";
			break;

		case 0x02:
			str = (const UInt8*)"7E3";
			break;

		case 0x03:
			str = (const UInt8*)"1E3";
			break;

		case 0x04:
			str = (const UInt8*)"1E4";
			break;

		case 0x05:
			str = (const UInt8*)"1E5";
			break;

		case 0x06:
			str = (const UInt8*)"1E6";
			break;

		case 0x07:
			str = (const UInt8*)"1E1";
			break;

		default:
			str = (const UInt8*)"0E0";
			break;

		}

	return(str);

}


//******************************************************************************
// Function Name: 	convertResidualBER
//
// Description:	  	Convert the ATC Residual BER value to stack value.
// Notes:
//******************************************************************************
UInt8 convertResidualBER(char *string)
{
	if (!strcmp(string, "5E2")) {
		return (0x01);
	}
	else if (!strcmp(string, "1E2")) {
		return (0x02);
	}
	else if (!strcmp(string, "5E3")) {
		return (0x03);
	}
	else if (!strcmp(string, "4E3")) {
		return (0x04);
	}
	else if (!strcmp(string, "1E3")) {
		return (0x05);
	}
	else if (!strcmp(string, "1E4")) {
		return (0x06);
	}
	else if (!strcmp(string, "1E5")) {
		return (0x07);
	}
	else if (!strcmp(string, "1E6")) {
		return (0x08);
	}
	else if (!strcmp(string, "6E8")) {
		return (0x09);
	}

	return (SUB_RESIDUAL_BER);

}


//******************************************************************************
// Function Name: 	revertResidualBER
//
// Description:	  	Revert the stack Residual BER value to ATC value.
// Notes:
//******************************************************************************
const UInt8* revertResidualBER(UInt8 value)
{
	const UInt8*	str = 0;

	MSG_LOGV("ATC:revertResidualBER:value=",value);
		switch (value) {
		case 0x01:
			str = (const UInt8*)"5E2";
			break;

		case 0x02:
			str = (const UInt8*)"1E2";
			break;

		case 0x03:
			str = (const UInt8*)"5E3";
			break;

		case 0x04:
			str = (const UInt8*)"4E3";
			break;

		case 0x05:
			str = (const UInt8*)"1E3";
			break;

		case 0x06:
			str = (const UInt8*)"1E4";
			break;

		case 0x07:
			str = (const UInt8*)"1E5";
			break;

		case 0x08:
			str = (const UInt8*)"1E6";
			break;

		case 0x09:
			str = (const UInt8*)"6E8";
			break;

		default:
			str = (const UInt8*)"0E0";
			break;

		}
	return(str);

}

//******************************************************************************
// Function Name: 	convertTrafficClass
//
// Description:	  	Convert the ATC Traffic Class value to stack value.
// Notes:
//******************************************************************************
UInt8 convertTrafficClass(AT_EqosTrafficClass_t atTrafficClass)
{
	MSG_LOGV("ATC:convertTrafficClass:atTrafficClass=",atTrafficClass);
	switch (atTrafficClass) {
		case AT_EQOS_TRAFFIC_CLASS_CONVERSATIONAL:
			return ((UInt8)TRAFFIC_CLASS_CC);
		case AT_EQOS_TRAFFIC_CLASS_STREAMING:
			return ((UInt8)TRAFFIC_CLASS_SC);
		case AT_EQOS_TRAFFIC_CLASS_INTERACTIVE:
			return ((UInt8)TRAFFIC_CLASS_IC);
		case AT_EQOS_TRAFFIC_CLASS_BACKGROUND:
			return ((UInt8)TRAFFIC_CLASS_BC);
		default:
			return ((UInt8)TRAFFIC_CLASS_S);
	}
}

//******************************************************************************
// Function Name: 	revertTrafficClass
//
// Description:	  	Revert the stack Traffic Class value back to ATC value.
// Notes:
//******************************************************************************
UInt8 revertTrafficClass(UInt8 stackTrafficClass)
{
	MSG_LOGV("ATC:convertTrafficClass:stackTrafficClass=",stackTrafficClass);
	switch (stackTrafficClass) {
		case TRAFFIC_CLASS_CC:
			return ((UInt8)AT_EQOS_TRAFFIC_CLASS_CONVERSATIONAL);
		case TRAFFIC_CLASS_SC:
			return ((UInt8)AT_EQOS_TRAFFIC_CLASS_STREAMING);
		case TRAFFIC_CLASS_IC:
			return ((UInt8)AT_EQOS_TRAFFIC_CLASS_INTERACTIVE);
		case TRAFFIC_CLASS_BC:
			return ((UInt8)AT_EQOS_TRAFFIC_CLASS_BACKGROUND);
		default:
			return ((UInt8)AT_EQOS_TRAFFIC_CLASS_SUB);
	}
}


//******************************************************************************
// Function Name: 	convertDeliveryOrder
//
// Description:	  	Convert the ATC Delivery Order value to stack value.
// Notes:
//******************************************************************************
UInt8 convertDeliveryOrder(AT_EqosDeliveryOrder_t atDeliveryOrder)
{
	switch (atDeliveryOrder) {
		case AT_EQOS_DELIVERY_ORDER_NO:
			return ((UInt8)DELIVERY_ORDER_N);
		case AT_EQOS_DELIVERY_ORDER_YES:
			return ((UInt8)DELIVERY_ORDER_Y);
		default:
			return ((UInt8)DELIVERY_ORDER_S);
	}
}

//******************************************************************************
// Function Name: 	revertDeliveryOrder
//
// Description:	  	Revert the stack Delivery Order value back to ATC value.
// Notes:
//******************************************************************************
UInt8 revertDeliveryOrder(UInt8 stackDeliveryOrder)
{
	switch (stackDeliveryOrder) {
		case DELIVERY_ORDER_N:
			return ((UInt8)AT_EQOS_DELIVERY_ORDER_NO);
		case DELIVERY_ORDER_Y:
			return ((UInt8)AT_EQOS_DELIVERY_ORDER_YES);
		default:
			return ((UInt8)AT_EQOS_DELIVERY_ORDER_SUB);
	}
}


//******************************************************************************
// Function Name: 	convertDeliveryErrSDU
//
// Description:	  	Convert the ATC Delivery Error SDU value to stack value.
// Notes:
//******************************************************************************
UInt8 convertDeliveryErrSDU(AT_EqosDeliveryErrSDU_t atDeliveryErrSDU)
{
	switch (atDeliveryErrSDU) {
		case AT_EQOS_DELIVERY_ERR_SDU_NO:
			return ((UInt8)ERR_SDU_DEL_N);
		case AT_EQOS_DELIVERY_ERR_SDU_YES:
			return ((UInt8)ERR_SDU_DEL_Y);
		case AT_EQOS_DELIVERY_ERR_SDU_NO_DETECT:
			return ((UInt8)ERR_SDU_DEL_ND);
		default:
			return ((UInt8)ERR_SDU_DEL_S);
	}
}

//******************************************************************************
// Function Name: 	revertDeliveryErrSDU
//
// Description:	  	Revert the stack Delivery Error SDU value back to ATC value.
// Notes:
//******************************************************************************
UInt8 revertDeliveryErrSDU(UInt8 stackDeliveryErrSDU)
{
	switch (stackDeliveryErrSDU) {
		case ERR_SDU_DEL_N:
			return ((UInt8)AT_EQOS_DELIVERY_ERR_SDU_NO);
		case ERR_SDU_DEL_Y:
			return ((UInt8)AT_EQOS_DELIVERY_ERR_SDU_YES);
		case ERR_SDU_DEL_ND:
			return ((UInt8)AT_EQOS_DELIVERY_ERR_SDU_NO_DETECT);
		default:
			return ((UInt8)AT_EQOS_DELIVERY_ERR_SDU_SUB);
	}
}

//******************************************************************************
// Function Name: 	convertBitRate
//
// Description:	  	Convert the ATC Bit Rate value to stack value.
// Notes:
//******************************************************************************
UInt8 convertBitRate(UInt16 atBitRate)
{
	UInt8	stackBitRate;

	if (atBitRate<64) {
		stackBitRate = (UInt8)atBitRate;
	}
	else if (atBitRate<=568) {
		stackBitRate = ((UInt8)((atBitRate-64)>>3))|0x40;
	}
	else if (atBitRate<=8640) {
		stackBitRate = ((UInt8)((atBitRate-576)>>6))|0x80;
	}
	else {
		stackBitRate = NOT_USED_0;
	}

	return (stackBitRate);
}


//******************************************************************************
// Function Name: 	revertBitRate
//
// Description:	  	Revert the stack Bit Rate value back to ATC value.
// Notes:
//******************************************************************************
UInt16 revertBitRate(UInt8 stackBitRate)
{
	UInt16 atBitRate;

	if (!(stackBitRate&0xC0)) {
		atBitRate = (UInt16)stackBitRate;
	}
	else if ( (stackBitRate&0xC0)==0x40) {
		atBitRate = (UInt16)(stackBitRate&0x3F)*8+64;
	}
	else if ( (stackBitRate&0xC0)==0x80) {
		atBitRate = (UInt16)(stackBitRate&0x7F)*64+576;
	}
	else {
		atBitRate = NOT_USED_0;
	}

	return (atBitRate);

}

//******************************************************************************
// Function Name: 	convertMaxSDUSize
//
// Description:	  	Convert the ATC Max SDU Size value to stack value.
// Notes:
//******************************************************************************
UInt8 convertMaxSDUSize(UInt16 value)
{
	if (value <= 1500) {
		return (value/10);
	}
	else if (value == 1502) {
		return (0x97);
	}
	else if (value == 1510) {
		return (0x98);
	}
	else if (value == 1520) {
		return (0x99);
	}
	else {
		return (SUB_SDU_SIZE);
	}
}

//******************************************************************************
// Function Name: 	convertMaxSDUSize
//
// Description:	  	Convert the ATC Max SDU Size value to stack value.
// Notes:
//******************************************************************************
UInt16 revertMaxSDUSize(UInt8 value)
{
	if (value <= 0x96) {
		return (value*10);
	}
	else if (value == 0x97) {
		return (1502);
	}
	else if (value == 0x98) {
		return (1510);
	}
	else if (value == 0x99) {
		return (1520);
	}
	else {
		return (SUB_SDU_SIZE);
	}
}



//******************************************************************************
//
// Function Name: 	pchActivateCb
//
// Description:	  	Callback associated with an Activate Request
//
// Notes:
//
//******************************************************************************
static void pchActivateCb(
	PCHResponseType_t			response,
	PCHPDPActivatedContext_t	*actContext,
	Result_t					cause
	)
{
	UInt8	chan = AT_GetCmdRspMpxChan(AT_CMD_CGACT);

	if( response == PCH_REQ_ACCEPTED ) {
		AT_CmdRspOK(chan);
	}
  	else {
		ATC_ReportCmeError(chan, cause);
  	}

}

//******************************************************************************
//
// Function Name: 	pchActivateSecCb
//
// Description:	  	Callback associated with an Activate Secondary Request
//
// Notes:
//
//******************************************************************************
#if	defined(STACK_edge) || defined(STACK_wedge)
static void pchActivateSecCb(
	PCHResponseType_t			response,
	PCHPDPActivatedSecContext_t	*actContext,
	Result_t					cause
	)
{
	UInt8	chan = AT_GetCmdRspMpxChan(AT_CMD_CGACT);

	if( response == PCH_REQ_ACCEPTED ) {
		AT_CmdRspOK(chan);
	}
  	else {
		ATC_ReportCmeError(chan, cause);
  	}

}
#endif


//******************************************************************************
//
// Function Name: 	pchDeactivateCb
//
// Description:	  	Callback associated with a Deactivate Request
//
// Notes:
//
//******************************************************************************
static void pchDeactivateCb(
	UInt8						cid,
	PCHResponseType_t			response,
	Result_t					cause
	)
{
	UInt8	chan = AT_GetCmdRspMpxChan(AT_CMD_CGACT);

	if( response == PCH_REQ_ACCEPTED ) {
		AT_CmdRspOK(chan);
	}
  	else {
		ATC_ReportCmeError(chan, cause);
  	}

}

//******************************************************************************
//
// Function Name: 	pchDataStateCb
//
// Description:	  	Callback associated with an Enter Data State Request
//
// Notes:
//
//******************************************************************************
static void pchDataStateCb(
	PCHResponseType_t		response,
	PCHNsapi_t				nsapi,
	PCHPDPType_t			pdpType,
	PCHPDPAddress_t			pdpAddress,
	Result_t				cause
	)
{
	UInt8	chan = AT_GetCmdRspMpxChan(AT_CMD_CGDATA);

	PCH_LOGV("Ben:pchDataStateCb..(response,chan)=",(response<<16|chan) );

	if( response == PCH_REQ_ACCEPTED ) {

		AT_CmdRsp(chan, AT_CONNECT_RSP );

//		serial_set_v24_flag( V24_DATA );
 		V24_SetOperationMode((DLC_t)chan, V24OPERMODE_AT_CMD );
		V24_SetDataTxMode((DLC_t)chan,TRUE);

		MS_SetChanGprsCallActive(chan, TRUE);
	}
  	else {
		AT_CmdRspError(chan, cause);
  	}
}





//******************************************************************************
//
// Function Name:	ATCmd_CGEREP_Handler
//
// Description:		Transition on AT+CGEREP
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CGEREP_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1,
			const UInt8* _P2 )
{
   	UInt8			val1, val2;
	char			output[50];
	MsConfig_t*		msCfg = MS_GetCfg( ) ;

	switch ( accessType ) {
	case AT_ACCESS_REGULAR:
		val1 = (_P1==NULL) ? 0:atoi((char *)_P1);
		val2 = (_P2==NULL) ? 0:atoi((char *)_P2);
      	msCfg->CGEREP[0] = val1;
      	msCfg->CGEREP[1] = val2;
		break;

	case AT_ACCESS_READ:
	    sprintf( output, "%s: %d,%d", AT_GetCmdName(cmdId),
             	 msCfg->CGEREP[ 0 ], msCfg->CGEREP[ 1 ] );
		AT_OutputStr( chan, (const UInt8 *)output );
		break;

	case AT_ACCESS_TEST:
	default:
		break;
	}

	AT_CmdRspOK(chan) ;
	return AT_STATUS_DONE;
}

void  ATC_SetContextDeactivation(UInt8 chan)
{

	//Put the check point for CSD call to avoid the assertion
	//when the PPP deactive confirmation comes after the CSD data
	//call due to the host software allows the CSD call sneak in before
	//the GPRS is gracefully shut down.
	if (!IsDataCallActive() || GetDataCallChannel() != chan){
		V24_SetOperationMode((DLC_t)chan, V24OPERMODE_AT_CMD );		// **FixMe** CHAN vs DLC
		V24_SetDataTxMode((DLC_t)chan,TRUE);
		//09-12-2003 Ben: not need to call serial_set_xxx here
//		serial_set_v24_flag( V24_AT_CMD );
	}
}


//******************************************************************************
//
// Function Name:	ATCmd_MPPPAUTH_Handler
//
// Description:		Transition on AT*MPPPAUTH: PPP authentication
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MPPPAUTH_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1 )
{
	UInt8		pppAuth;
	char		output[50];

	switch ( accessType ) {
	case AT_ACCESS_REGULAR:
		pppAuth = atoi((char *)_P1 );

		if(PPP_Set_AuthProt(pppAuth))
			AT_CmdRspOK(chan);
		else
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);

		break;

	case AT_ACCESS_READ:
		pppAuth = PPP_Get_AuthProt();

		sprintf( output, "*MPPPAUTH: %d", pppAuth );
		AT_OutputStr(chan, (const UInt8 *)output );

		AT_CmdRspOK(chan);
		break;

	case AT_ACCESS_TEST:
	default:
		break;
	}

//	AT_CmdRspOK(chan) ;
	return AT_STATUS_DONE;
}


//=============================================================
//	AT Handler function for InterTask messages
//	Ben: 10-17-2002
//=============================================================
//******************************************************************************
//
// Function Name:	AtHandleRegGsmGprsInd
//
// Description:		Handles the MSG_REG_GSM_IND and MSG_REG_GPRS_IND messages
//
// Notes:			API will send one of the above message at one time, and only
//					send message over when there's difference from the last such message.
//
//******************************************************************************
void AtHandleRegGsmGprsInd(InterTaskMsg_t *inMsg)
{
	MSRegInfo_t*		msRegInd = (MSRegInfo_t *)inMsg->dataBuf;
	MsConfig_t*			msCfg = MS_GetCfg( ) ;

	if(inMsg->msgType == MSG_REG_GSM_IND)
	{
		AtOutputCregStatus(msRegInd->regState, FALSE, msRegInd->lac, msRegInd->cell_id);
	}
	else // MSG_REG_GPRS_IND
	{
		AtOutputCgregStatus(msRegInd->regState, FALSE, msRegInd->lac, msRegInd->cell_id);
		if (msRegInd->regState==REG_STATE_LIMITED_SERVICE||msRegInd->regState==REG_STATE_NO_SERVICE) {
			if( msCfg->CGEREP[0] == 2 && msCfg->CGEREP[1] == 1 ) {
				AT_OutputUnsolicitedStr( (UInt8*)"+CGEV: ME DETACH" );
			}
		}

		if( SYS_GetGPRSRegistrationStatus() == REGISTERSTATUS_SERVICE_DISABLED ) {
			if( msCfg->CGEREP[0] == 2 && msCfg->CGEREP[1] == 1 ) {
				AT_OutputUnsolicitedStr( (UInt8*)"+CGEV: NW DETACH");
			}
		}

	}

	AtOutputNetworkInfo(msRegInd->netInfo);

}

//******************************************************************************
//
// Function Name:	AtHandlePlmnSelectCnf
//
// Description:		Handles the MSG_PLMN_SELECT_CNF
//
// Notes:
//
//******************************************************************************
void AtHandlePlmnSelectCnf(InterTaskMsg_t *inMsg)
{
	UInt8					chan;
	UInt16					*plmnSelectResp;

	chan = AT_GetCmdRspMpxChan(AT_CMD_COPS);
	PCH_LOGV("PCH:AtHandlePlmnSelectCnf-plmnSelectResp-chan=",chan);
	plmnSelectResp = (UInt16 *)inMsg->dataBuf;

	if (*plmnSelectResp == OPERATION_SUCCEED) {
		AT_CmdRspOK( chan );
	}
	else if (*plmnSelectResp == NO_NETWORK_SERVICE){
		AT_CmdRspError(chan, AT_CME_ERR_NO_NETWORK_SVC);
	}

}



//******************************************************************************
// Function Name:	AtHandleGPRSDeactivateInd
// Description:
// Notes:
//
//******************************************************************************
void AtHandleGPRSDeactivateInd(InterTaskMsg_t *InterMsg)
{
	char					output[50];
	MsConfig_t*				msCfg = MS_GetCfg( ) ;
	GPRSDeactInd_t*			pGPRSDeactInd;

	MSG_LOG( "ATC:AtHandleGPRSDeactivateInd--");
	pGPRSDeactInd = (GPRSDeactInd_t *)InterMsg->dataBuf;

	if( msCfg->CGEREP[0] == 2 && msCfg->CGEREP[1] == 1 ) {
		sprintf( output, "+CGEV: NW DEACT IP, %s, %d",
				 NOT_USED_STRING, pGPRSDeactInd->cid );
		AT_OutputUnsolicitedStr((UInt8*)output);
	}

}

//******************************************************************************
// Function Name:	AtHandleActivateInd
// Description:
// Notes:
//
//******************************************************************************
void AtHandleGPRSActivateInd(InterTaskMsg_t *InterMsg)
{
	char					output[50];
	//UInt8					cid;
	//PCHPDPType_t			pdpType;
	//PCHPDPAddress_t			pdpAddress;
	MsConfig_t*				msCfg = MS_GetCfg( ) ;
	GPRSActInd_t*			pGPRSActInd;

	MSG_LOG( "ATC:AtHandleActivateInd--");
	pGPRSActInd = (GPRSActInd_t *)InterMsg->dataBuf;

	if( msCfg->CGEREP[0] == 2 && msCfg->CGEREP[1] == 1 ) {
		sprintf( output, "+CGEV: NW REACT %s, %s, %d",
				 pGPRSActInd->pdpType, pGPRSActInd->pdpAddress, pGPRSActInd->cid );
		AT_OutputUnsolicitedStr((UInt8*)output);
	}

}

//******************************************************************************
// Function Name:	AtHandleActivateInd
// Description:
// Notes:
//
//******************************************************************************
void AtHandleGPRSModifyInd(InterTaskMsg_t *InterMsg)
{
	Inter_ModifyContextInd_t	*pModifyInd;

	pModifyInd = (Inter_ModifyContextInd_t *)InterMsg->dataBuf;

	MSG_LOGV( "ATC:AtHandleGPRSModifyInd(Nsapi/Sapi)=",(pModifyInd->nsapi<<16|pModifyInd->sapi) );
	MSG_LOGV( "ATC:AtHandleGPRSModifyInd(prec,delay,rel,peak,mean)=",
		0xFF000000|(pModifyInd->qosProfile.precedence << 20)|(pModifyInd->qosProfile.delay << 16) |
		(pModifyInd->qosProfile.reliability << 12)|(pModifyInd->qosProfile.peak << 8) | pModifyInd->qosProfile.mean);


}

//******************************************************************************
// Function Name:	AtHandleGPRSReActInd
// Description:
// Notes:
//
//******************************************************************************
void AtHandleGPRSReActInd(InterTaskMsg_t *InterMsg)
{
	GPRSReActInd_t	*pReActInd;
	char			output[50];

	pReActInd = (GPRSReActInd_t *)InterMsg->dataBuf;

	MSG_LOGV( "ATC:AtHandleGPRSReActInd(cid)=", pReActInd->cid );

	// only report the status if the new status is different from the current status
	sprintf( output, "%s: %d", "*MREACT", pReActInd->cid );
	AT_OutputUnsolicitedStr( (UInt8*)output );

}

//==============================================================================
// New ATCmd Handler
//==============================================================================
//******************************************************************************
//
// Function Name:	ATCmd_CGATT_Handler
//
// Description:		Transition on GPRS Attach / Detach Request
//					AT+CGATT=state
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CGATT_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1 )
{
	AttachState_t		AttachState;
	InterTaskMsg_t		*InterTaskMsg;
	ATCGATTData_t		*pCGATTData;



	AttachState = (AttachState_t)(_P1 == NULL) ? DETACHED : atoi((char *)_P1) ? ATTACHED:DETACHED;

	// Generate CGATT process request msg to ATC task
	InterTaskMsg = AllocInterTaskMsgFromHeap(MSG_AT_CGATT_CMD, sizeof(ATCGATTData_t));

	pCGATTData = (ATCGATTData_t *)InterTaskMsg->dataBuf;
	pCGATTData->accessType = accessType;
	pCGATTData->AttachState = (AttachState_t)(_P1 == NULL)? DETACHED:atoi((char *)_P1)? ATTACHED:DETACHED;
	pCGATTData->cmdId = cmdId;

    // send CGATT request to ATC task
	PostMsgToATCTask(InterTaskMsg);

	return AT_STATUS_DONE;
}

//******************************************************************************
//
// Function Name:	ATCmd_CGCLASS_Handler
//
// Description:		Transition on Enter Data State Request
//					AT+CGCLASS=<class>
//
//
//******************************************************************************
AT_Status_t ATCmd_CGCLASS_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1 )
{
	UInt8			msClass;
	char			string[10];
	char			output[50];

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			strcpy( string, ( _P1 == NULL ) ? NOT_USED_STRING : (char*)_P1 );
			if (!strcmp(string,"A")) {
				msClass = GPRS_CLASS_A;
			}
			else if (!strcmp(string,"B")) {
				msClass = GPRS_CLASS_B;
			}
			else if (!strcmp(string,"CG")) {
				msClass = GPRS_CLASS_CG;
			}
			else if (!strcmp(string,"CC")) {
				msClass = GPRS_CLASS_CC;
			}
			else if (!strcmp(string,"C")) {
				msClass = GPRS_CLASS_C;
			}
			else {
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			PDP_SetMSClass((MSClass_t)msClass);

			break;

		case AT_ACCESS_READ:
			msClass = PDP_GetMSClass();

			sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
			if (msClass==GPRS_CLASS_A)
				strcat( output, "A" );
			else if (msClass==GPRS_CLASS_B)
				strcat( output, "B" );
			else if (msClass==GPRS_CLASS_C)
				strcat( output, "C" );
			else if (msClass==GPRS_CLASS_CG)
				strcat( output, "CG" );
			else if (msClass==GPRS_CLASS_CC)
				strcat( output, "CC" );

			AT_OutputStr( chan, (const UInt8 *)output );

			break;
		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"+CGCLASS: (A,B,C,CG,CC)" );
			break;
		default:
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			return AT_STATUS_DONE;
	}

	MSG_LOGV("ATCmd_CGCLASS_Handler:MsClass=",msClass);

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

//******************************************************************************
// Function Name:	ATCmd_CGQREQ_Handler
//
// Description:		Transition on Quality of Service Profile (Required) Request
//					AT+CGQREQ=[cid[,prec[,delay[,rel[,peak[,mean]]]]]]
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CGQREQ_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1,
			const UInt8* _P2,
			const UInt8* _P3,
			const UInt8* _P4,
			const UInt8* _P5,
			const UInt8* _P6)
{
	PCHCid_t			cid;
	UInt8				precedence,delay,reliability,peak,mean;
	Result_t			resp;
	UInt8				numContexts=0;
	UInt8				idx;
	char				temp[10];
	char				output[50];

	PCH_LOGV("ATCmd_CGQREQ_Handler:accessType=",accessType);


	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			cid 			= (_P1 == NULL ) ? NOT_USED_0  : atoi((char*)_P1 );
			precedence 		= (_P2 == NULL ) ? NOT_USED_FF : atoi((char*)_P2 );
			delay			= (_P3 == NULL ) ? NOT_USED_FF : atoi((char*)_P3 );
			reliability		= (_P4 == NULL ) ? NOT_USED_FF : atoi((char*)_P4 );
			peak			= (_P5 == NULL ) ? NOT_USED_FF : atoi((char*)_P5 );
			mean			= (_P6 == NULL ) ? NOT_USED_FF : atoi((char*)_P6 );

	   		if( _P1 != NULL		&&
	   			_P2 == NULL		&&
	   			_P3 == NULL		&&
		   		_P4 == NULL		&&
		   		_P5 == NULL		&&
				_P6 == NULL )
			{
				numContexts = 0;
			}
			else {
				numContexts = 5;
			}

 			resp = PDP_SetGPRSQoS(cid,numContexts,precedence,delay,reliability,peak,mean);

			if (resp != RESULT_OK) {
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			break;
		case AT_ACCESS_READ:
			for( idx = MIN_CID; idx <= MAX_CID; idx++ ) {
				resp = PDP_GetGPRSQoS(idx,&precedence,&delay,&reliability,&peak,&mean);
				if( resp == RESULT_OK ) {
					numContexts++;

					sprintf( output, "%s: %d, ", AT_GetCmdName(cmdId), idx );

					if( precedence != NOT_USED_FF ) {
			 			sprintf( temp, "%d", precedence ); strcat( output, temp );
					}
					strcat( output, ", " );

					if( delay != NOT_USED_FF ) {
						sprintf( temp, "%d", delay ); strcat( output, temp );
					}
					strcat( output, ", " );

					if( reliability != NOT_USED_FF ) {
						sprintf( temp, "%d", reliability ); strcat( output, temp );
					}
					strcat( output, ", " );

					if( peak != NOT_USED_FF ) {
						sprintf( temp, "%d", peak ); strcat( output, temp );
					}
					strcat( output, ", " );

					if( mean != NOT_USED_FF ) {
						sprintf( temp, "%d", mean ); strcat( output, temp );
					}
					AT_OutputStr( chan, (const UInt8 *)output );
				}

  			}

			if (numContexts==0) {
				sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
				AT_OutputStr( chan, (const UInt8 *)output );
			}

			break;
		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"+CGQREQ: \"IP\",(0-3),(0-4),(0-5),(0-9),(0-18,31)" );
			break;
		default:
			break;
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;

}

//******************************************************************************
// Function Name:	ATCmd_CGQMIN_Handler
//
// Description:		Transition on Minimum Acceptable Quality of Service Profile
//					AT+CGQMIN=[cid[,prec[,delay[,rel[,peak[,mean]]]]]]
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CGQMIN_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1,
			const UInt8* _P2,
			const UInt8* _P3,
			const UInt8* _P4,
			const UInt8* _P5,
			const UInt8* _P6)
{
	PCHCid_t			cid;
	UInt8				precedence,delay,reliability,peak,mean;
	Result_t			resp;
	UInt8				numContexts=0;
	UInt8				idx;
	char				temp[10];
	char				output[50];

	PCH_LOGV("ATCmd_CGQMIN_Handler:accessType=",accessType);


	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			cid 			= (_P1 == NULL ) ? NOT_USED_0  : atoi((char*)_P1 );
			precedence 		= (_P2 == NULL ) ? NOT_USED_FF : atoi((char*)_P2 );
			delay			= (_P3 == NULL ) ? NOT_USED_FF : atoi((char*)_P3 );
			reliability		= (_P4 == NULL ) ? NOT_USED_FF : atoi((char*)_P4 );
			peak			= (_P5 == NULL ) ? NOT_USED_FF : atoi((char*)_P5 );
			mean			= (_P6 == NULL ) ? NOT_USED_FF : atoi((char*)_P6 );

	   		if( _P1 != NULL		&&
	   			_P2 == NULL		&&
	   			_P3 == NULL		&&
		   		_P4 == NULL		&&
		   		_P5 == NULL		&&
				_P6 == NULL )
			{
				numContexts = 0;
			}
			else {
				numContexts = 5;
			}

 			resp = PDP_SetGPRSMinQoS(cid,numContexts,precedence,delay,reliability,peak,mean);

			if (resp != RESULT_OK) {
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			break;
		case AT_ACCESS_READ:
			for( idx = MIN_CID; idx <= MAX_CID; idx++ ) {
				resp = PDP_GetGPRSMinQoS(idx,&precedence,&delay,&reliability,&peak,&mean);
				if( resp == RESULT_OK ) {
					numContexts++;

					sprintf( output, "%s: %d, ", AT_GetCmdName(cmdId), idx );

					if( precedence != NOT_USED_FF ) {
			 			sprintf( temp, "%d", precedence ); strcat( output, temp );
					}
					strcat( output, ", " );

					if( delay != NOT_USED_FF ) {
						sprintf( temp, "%d", delay ); strcat( output, temp );
					}
					strcat( output, ", " );

					if( reliability != NOT_USED_FF ) {
						sprintf( temp, "%d", reliability ); strcat( output, temp );
					}
					strcat( output, ", " );

					if( peak != NOT_USED_FF ) {
						sprintf( temp, "%d", peak ); strcat( output, temp );
					}
					strcat( output, ", " );

					if( mean != NOT_USED_FF ) {
						sprintf( temp, "%d", mean ); strcat( output, temp );
					}
					AT_OutputStr( chan, (const UInt8 *)output );
				}
  			}

			if (numContexts==0) {
				sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
				AT_OutputStr( chan, (const UInt8 *)output );
			}

			break;
		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"+CGQMIN: \"IP\",(0-3),(0-4),(0-5),(0-9),(0-18,31)" );
			break;
		default:
			break;
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;

}

//******************************************************************************
// Function Name:	ATCmd_CGEQREQ_Handler
//
// Description:		Transition on 3G R99 Quality of Service Profile Requested
//					AT+CGEQREQ=	[cid
//								[,Traffic_class
//								[,Maximum_bitrate_UL
//								[,Maximum_bitrate_DL
//								[,Guaranteed_botrate_UL
//								[,Guaranteed_bitrate_DL
//								[,Delivery_order
//								[,Maximum_SDU_size
//								[,SDU_error_ratio
//								[,Residual_bit_error_ratio
//								[,Delivery_of_erroneous_SDUs
//								[,Transfer_delay
//								[,Trraffic_handling_priority]]]]]]]]]]]]]
// Notes:
//
//******************************************************************************
#if	defined(STACK_edge) || defined(STACK_wedge)
AT_Status_t ATCmd_CGEQREQ_Handler (
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
			const UInt8* _P8,
			const UInt8* _P9,
			const UInt8* _P10,
			const UInt8* _P11,
			const UInt8* _P12,
			const UInt8* _P13)
{
	PCHCid_t			cid;
	UInt8				numContexts=0;
	char				output[100];
	UInt8				idx;
	Result_t			resp;
	PCHR99QosProfile_t	r99Qos;
	char				temp[10], *string;

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			cid 							=(_P1==NULL) ? NOT_USED_0  : atoi((char*)_P1 );
			r99Qos.present_3g				= SUB_PRESENT_3G; //Initialiazed to be TRUE
			r99Qos.traffic_class			=(_P2==NULL) ? SUB_TRAFFIC_CLASS : convertTrafficClass((AT_EqosTrafficClass_t)atoi((char*)_P2 ));
			r99Qos.max_bit_rate_up			=(_P3==NULL) ? SUB_BIT_RATE_UP : convertBitRate(atoi((char*)_P3 ));
			r99Qos.max_bit_rate_down		=(_P4==NULL) ? SUB_BIT_RATE_DOWN : convertBitRate(atoi((char*)_P4 ));
			r99Qos.guaranteed_bit_rate_up	=(_P5==NULL) ? SUB_GUARANTEED_BIT_RATE_UP : convertBitRate(atoi((char*)_P5 ));
			r99Qos.guaranteed_bit_rate_down	=(_P6==NULL) ? SUB_GUARANTEED_BIT_RATE_DOWN : convertBitRate(atoi((char*)_P6 ));
			r99Qos.delivery_order			=(_P7==NULL) ? SUB_DELIVERY_ORDER : convertDeliveryOrder((AT_EqosDeliveryOrder_t)atoi((char*)_P7 ));
			r99Qos.max_sdu_size				=(_P8==NULL) ? SUB_SDU_SIZE : convertMaxSDUSize(atoi((char*)_P8 ));
			r99Qos.sdu_error_ratio			=(_P9==NULL) ? SUB_SDU_ERROR_RATIO : convertSDUErrRatio((char*)_P9 );
			r99Qos.residual_ber				=(_P10==NULL) ? SUB_RESIDUAL_BER : convertResidualBER((char*)_P10 );
			r99Qos.error_sdu_delivery		=(_P11==NULL) ? SUB_ERROR_SDU_DELIVERY : convertDeliveryErrSDU((AT_EqosDeliveryErrSDU_t)atoi((char*)_P11 ));
			r99Qos.transfer_delay			=(_P12==NULL) ? SUB_TRANSFER_DELAY : atoi((char*)_P12 );
			r99Qos.traffic_priority			=(_P13==NULL) ? SUB_TRAFFIC_PRIORITY : atoi((char*)_P13 );

 			resp = PDP_SetR99UMTSQoS(cid,&r99Qos);

			if (resp != RESULT_OK) {
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}
			break;

		case AT_ACCESS_READ:
			for( idx = MIN_CID; idx <= MAX_CID; idx++ ) {
				resp = PDP_GetR99UMTSQoS(idx,&r99Qos);
				if( resp == RESULT_OK ) {
					numContexts++;

					sprintf( output, "%s:%d,", AT_GetCmdName(cmdId), idx );
		 			sprintf( temp, "%d,", revertTrafficClass(r99Qos.traffic_class) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertBitRate(r99Qos.max_bit_rate_up) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertBitRate(r99Qos.max_bit_rate_down) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertBitRate(r99Qos.guaranteed_bit_rate_up) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertBitRate(r99Qos.guaranteed_bit_rate_down) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertDeliveryOrder(r99Qos.delivery_order) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertMaxSDUSize(r99Qos.max_sdu_size) );
					strcat( output, temp );

					string = (char *)revertSDUErrRatio(r99Qos.sdu_error_ratio);
		 			sprintf( temp, "\"%s\",", string );
					strcat( output, temp );
					string = (char *)revertResidualBER(r99Qos.residual_ber);
		 			sprintf( temp, "\"%s\",", string );
					strcat( output, temp );

					sprintf( temp, "%d,", revertDeliveryErrSDU(r99Qos.error_sdu_delivery) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", r99Qos.transfer_delay );
					strcat( output, temp );
		 			sprintf( temp, "%d", r99Qos.traffic_priority );
					strcat( output, temp );

					AT_OutputStr( chan, (const UInt8 *)output );
				}

  			}

			if (numContexts==0) {
				sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
				AT_OutputStr( chan, (const UInt8 *)output );
			}

			break;

		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"+CGEQREQ: \"IP\",(0-4),(0-8640),(0-8640),(0-8640),(0-8640),(0-2),(0-153),(?),(?),(0-3),(0-62),(0-3)" );
			break;

		default:
			break;
	}

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}

//******************************************************************************
// Function Name:	ATCmd_CGEQMIN_Handler
//
// Description:		Transition on 3G R99 minimum Quality of Service Profile Requested
//					AT+CGEQMIN=	[cid
//								[,Traffic_class
//								[,Maximum_bitrate_UL
//								[,Maximum_bitrate_DL
//								[,Guaranteed_botrate_UL
//								[,Guaranteed_bitrate_DL
//								[,Delivery_order
//								[,Maximum_SDU_size
//								[,SDU_error_ratio
//								[,Residual_bit_error_ratio
//								[,Delivery_of_erroneous_SDUs
//								[,Transfer_delay
//								[,Trraffic_handling_priority]]]]]]]]]]]]]
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CGEQMIN_Handler (
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
			const UInt8* _P8,
			const UInt8* _P9,
			const UInt8* _P10,
			const UInt8* _P11,
			const UInt8* _P12,
			const UInt8* _P13)
{
	PCHCid_t			cid;
	UInt8				numContexts=0;
	char				output[100];
	Result_t			resp;
	UInt8				idx;
	PCHR99QosProfile_t	r99Qos;
	char				temp[10], *string;

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			cid 							=(_P1==NULL) ? NOT_USED_0  : atoi((char*)_P1 );
			r99Qos.traffic_class			=(_P2==NULL) ? SUB_TRAFFIC_CLASS : convertTrafficClass((AT_EqosTrafficClass_t)atoi((char*)_P2 ));
			r99Qos.max_bit_rate_up			=(_P3==NULL) ? SUB_BIT_RATE_UP : convertBitRate(atoi((char*)_P3 ));
			r99Qos.max_bit_rate_down		=(_P4==NULL) ? SUB_BIT_RATE_DOWN : convertBitRate(atoi((char*)_P4 ));
			r99Qos.guaranteed_bit_rate_up	=(_P5==NULL) ? SUB_GUARANTEED_BIT_RATE_UP : convertBitRate(atoi((char*)_P5 ));
			r99Qos.guaranteed_bit_rate_down	=(_P6==NULL) ? SUB_GUARANTEED_BIT_RATE_DOWN : convertBitRate(atoi((char*)_P6 ));
			r99Qos.delivery_order			=(_P7==NULL) ? SUB_DELIVERY_ORDER : convertDeliveryOrder((AT_EqosDeliveryOrder_t)atoi((char*)_P7 ));
			r99Qos.max_sdu_size				=(_P8==NULL) ? SUB_SDU_SIZE : atoi((char*)_P8 );
			r99Qos.sdu_error_ratio			=(_P9==NULL) ? SUB_SDU_ERROR_RATIO : convertSDUErrRatio((char*)_P9 );
			r99Qos.residual_ber				=(_P10==NULL) ? SUB_RESIDUAL_BER : convertResidualBER((char*)_P10 );
			r99Qos.error_sdu_delivery		=(_P11==NULL) ? SUB_ERROR_SDU_DELIVERY : convertDeliveryErrSDU((AT_EqosDeliveryErrSDU_t)atoi((char*)_P11 ));
			r99Qos.transfer_delay			=(_P12==NULL) ? SUB_TRANSFER_DELAY : atoi((char*)_P12 );
			r99Qos.traffic_priority			=(_P13==NULL) ? SUB_TRAFFIC_PRIORITY : atoi((char*)_P13 );

 			resp = PDP_SetR99UMTSMinQoS(cid,&r99Qos);

			if (resp != RESULT_OK) {
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}
			break;

		case AT_ACCESS_READ:
			for( idx = MIN_CID; idx <= MAX_CID; idx++ ) {
				resp = PDP_GetR99UMTSMinQoS(idx,&r99Qos);
				if( resp == RESULT_OK ) {
					numContexts++;

					sprintf( output, "%s:%d,", AT_GetCmdName(cmdId), idx );

		 			sprintf( temp, "%d,", revertTrafficClass(r99Qos.traffic_class) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertBitRate(r99Qos.max_bit_rate_up) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertBitRate(r99Qos.max_bit_rate_down) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertBitRate(r99Qos.guaranteed_bit_rate_up) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertBitRate(r99Qos.guaranteed_bit_rate_down) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", revertDeliveryOrder(r99Qos.delivery_order) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", r99Qos.max_sdu_size );
					strcat( output, temp );

					string = (char *)revertSDUErrRatio(r99Qos.sdu_error_ratio);
		 			sprintf( temp, "\"%s\",", string );
					strcat( output, temp );

					string = (char *)revertResidualBER(r99Qos.residual_ber);
		 			sprintf( temp, "\"%s\",", string );
					strcat( output, temp );

					sprintf( temp, "%d,", revertDeliveryErrSDU(r99Qos.error_sdu_delivery) );
					strcat( output, temp );
		 			sprintf( temp, "%d,", r99Qos.transfer_delay );
					strcat( output, temp );
		 			sprintf( temp, "%d", r99Qos.traffic_priority );
					strcat( output, temp );

					AT_OutputStr( chan, (const UInt8 *)output );
				}
  			}

			if (numContexts==0) {
				sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
				AT_OutputStr( chan, (const UInt8 *)output );
			}
			break;

		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"+CGEQMIN: \"IP\",(0-4),(0-8640),(0-8640),(0-8640),(0-8640),(0-2),(0-153),(?),(?),(0-3),(0-62),(0-3)" );
			break;

		default:
			break;
	}

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;

}

//******************************************************************************
// Function Name:	ATCmd_CGEQNEG_Handler
//
// Description:		Transition on 3G R99 negotiated Quality of Service Profile Requested
//					AT+CGEQMIN=	[cid [,cid[, ...]]]
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CGEQNEG_Handler (
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
			const UInt8* _P8,
			const UInt8* _P9,
			const UInt8* _P10)
{
	UInt8				idx;
	UInt8				numContexts=0;
	char				temp[10];
	char				output[50];

	// This is only used for holding the input parameters, maximum 10 PDP Contexts for now
	// NOTE: 05/05/05 jhwang cid[] needs to hold _P1 through _P10.  If MAX_PDP_CONTEXTS
	//       changes, more _Px will have to be added.  Ugly codes!

	PCHCid_t			cid[10];
	Result_t			resp;
	PCHR99QosProfile_t	r99Qos;

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			cid[0]	=(_P1==NULL) ? NOT_USED_0  : atoi((char*)_P1 );
			cid[1]	=(_P2==NULL) ? NOT_USED_0  : atoi((char*)_P2 );
			cid[2]	=(_P3==NULL) ? NOT_USED_0  : atoi((char*)_P3 );
			cid[3]	=(_P4==NULL) ? NOT_USED_0  : atoi((char*)_P4 );
			cid[4]	=(_P5==NULL) ? NOT_USED_0  : atoi((char*)_P5 );
			cid[5]	=(_P6==NULL) ? NOT_USED_0  : atoi((char*)_P6 );
			cid[6]	=(_P7==NULL) ? NOT_USED_0  : atoi((char*)_P7 );
			cid[7]	=(_P8==NULL) ? NOT_USED_0  : atoi((char*)_P8 );
			cid[8]	=(_P9==NULL) ? NOT_USED_0  : atoi((char*)_P9 );
			cid[9]	=(_P10==NULL) ? NOT_USED_0  : atoi((char*)_P10 );

			for (idx = 0; idx < 10; idx++) {
				if ((MIN_CID <= cid[idx]) && (cid[idx] <= MAX_CID)) {
					if (PDP_IsPDPContextActive(cid[idx]) ) {

						resp = PDP_GetR99UMTSQoS(cid[idx],&r99Qos);
						if( resp == RESULT_OK ) {
							numContexts++;

							sprintf( output, "%s: %d, ", AT_GetCmdName(cmdId), cid[idx] );

		 					sprintf( temp, "%d, ", r99Qos.traffic_class );
							strcat( output, temp );
		 					sprintf( temp, "%d, ", r99Qos.max_bit_rate_up );
							strcat( output, temp );
		 					sprintf( temp, "%d, ", r99Qos.max_bit_rate_down );
							strcat( output, temp );
		 					sprintf( temp, "%d, ", r99Qos.guaranteed_bit_rate_up );
							strcat( output, temp );
		 					sprintf( temp, "%d, ", r99Qos.guaranteed_bit_rate_down );
							strcat( output, temp );
		 					sprintf( temp, "%d, ", r99Qos.delivery_order );
							strcat( output, temp );
		 					sprintf( temp, "%d, ", r99Qos.max_sdu_size );
							strcat( output, temp );
		 					sprintf( temp, "%d, ", r99Qos.sdu_error_ratio );
							strcat( output, temp );
		 					sprintf( temp, "%d, ", r99Qos.residual_ber );
							strcat( output, temp );
		 					sprintf( temp, "%d, ", r99Qos.error_sdu_delivery );
							strcat( output, temp );
		 					sprintf( temp, "%d, ", r99Qos.transfer_delay );
							strcat( output, temp );
		 					sprintf( temp, "%d ", r99Qos.traffic_priority );
							strcat( output, temp );

							AT_OutputStr( chan, (const UInt8 *)output );
						}
					}
				}
			}	//end for

			if (numContexts==0) {
				sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
				AT_OutputStr( chan, (const UInt8 *)output );
			}
			break;

		case AT_ACCESS_READ:
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			return AT_STATUS_DONE;
			break;

		case AT_ACCESS_TEST:
			sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
			for (idx = MIN_CID; idx <= MAX_CID; idx++) {
				if (PDP_IsPDPContextActive(idx) ) {
					if (numContexts) {
		 				sprintf( temp, ", %d", idx );
						strcat( output, temp );
					}
					else {
		 				sprintf( temp, "%d", idx );
						strcat( output, temp );
					}
					numContexts++;
				}
			}
			AT_OutputStr( chan, (const UInt8 *)output );
			break;

		default:
			break;
	}

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;

}

static Result_t parse_dot(UInt8 range, UInt32 max, char* in,UInt16* pVal1, UInt16* pVal2)
{
	UInt8 i;
	UInt32 tmp1, tmp2;

	tmp1 = 0;

	for(i=0; i < (range+1);i++)
	{
   		if( (*in >= '0') && (*in <= '9'))
		{
			tmp1 = tmp1*10 + *in - '0';
		}
		else if( (*in == '.') || !*in)
		{
			++in;
			break;
		}
		else
		{
		    MSG_LOGV ("ATC: unexpected character I, shall be 0-9 and .", i );
			return RESULT_ERROR;
		}
		++in;
	}

	if( (i <1) || (*(in-1) != '.')  || (tmp1> max))
	{
	    MSG_LOGV("ATC: format fail I",*(in-1));
		return RESULT_ERROR;
	}

	tmp2 = 0;

	for(i=0; i < range;i++)
	{
		if( (*in >= '0') && (*in <= '9'))
		{
			tmp2 = tmp2*10 + *in - '0';
		}
		else if( !*in)
		{
			break;
		}
		else
		{
		    MSG_LOGV("ATC: unexpected character II, shall be 0-9 and .", i );
			return RESULT_ERROR;
		}
		++in;
	}
	if((i <1) || (*in && (i == (range-1)))  || (tmp2 > max))
	{
		MSG_LOG("ATC: format fail II");
		return RESULT_ERROR;
	}

	*pVal1 = tmp1;
	*pVal2 = tmp2;

	return RESULT_OK;
}
//******************************************************************************
// Function Name:	ATCmd_CGTFT_Handler
//
// Description:		Transition on 3G R99 Traffic Flow Template.
//					AT+CGTFT=	[ cid
//								[,Packet_filter_id
//								[,Evaluation_precedence_idx
//								[,Source_address_subnet_mask
//								[,Protocol_number
//								[,Destination_port_range
//								[,Source_port_range
//								[,Ipsec_security_param_idx
//								[,Type_of_service
//								[,Flow_label ]]]]]]]]]]
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CGTFT_Handler (
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
			const UInt8* _P8,
			const UInt8* _P9,
			const UInt8* _P10)
{
	PCHCid_t					cid;
	UInt8						numContexts=0;
	char						output[120];
	char						temp[100];
	Result_t					resp;
	UInt8						idx,loop, idx_cid;
	PCHTrafficFlowTemplate_t	r99Tft;
    PCHPacketFilter_T           pkt_filter;
	UInt16 						tmpVal1, tmpVal2;


	switch ( accessType ) {
		case AT_ACCESS_REGULAR:

			/* Check for special format at+cgtft=cid */
			if( (_P1 != NULL) && (_P2==NULL) && (_P3==NULL) && (_P4==NULL) && (_P5==NULL)
				&& (_P6==NULL) && (_P7==NULL) && (_P8==NULL) && (_P9==NULL) && (_P10==NULL)  )
			{
 		        cid = atoi((char*)_P1 );
				resp = PDP_DeleteUMTSTft(cid);

				if (resp != RESULT_OK) {
					MSG_LOG("ATC: fail to delete TFT");
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}
				else {
					AT_CmdRspOK(chan);
					return AT_STATUS_DONE;
				}
			}
		    /* only address type I is currently supported */
			else if( (_P8 != NULL) || (_P10 != NULL) )
			{
				MSG_LOG("ATC: Unsupported parameters");
                AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;

			}
            else if( (_P1 == NULL) || (_P2 == NULL) || (_P3 == NULL) ||
			         ( (_P4 == NULL) && (_P5 == NULL) && (_P6 == NULL) && (_P7 == NULL) && (_P9 == NULL) ) )
            {
                MSG_LOG("ATC: Incorrect parameters");
                AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
            }

			memset(&pkt_filter, 0, sizeof(PCHPacketFilter_T));

			cid 							= atoi((char*)_P1 );
			pkt_filter.packet_filter_id		= atoi((char*)_P2);
			pkt_filter.evaluation_precedence_idx	= atoi((char*)_P3 );

			if(_P5 != NULL)
			{
				pkt_filter.present_prot_num = TRUE;
				pkt_filter.protocol_number	= atoi((char*)_P5 );
			}

			if(_P6 != NULL)
			{
				if( (parse_dot(TFT_MAX_PORT_RANGE, TFT_MAX_PORT_VAL, (char*)_P6,&pkt_filter.destination_port_low, &pkt_filter.destination_port_high) == RESULT_OK)
				    && (pkt_filter.destination_port_low<=pkt_filter.destination_port_high) )
				{
					pkt_filter.present_dst_port_range = TRUE;
				}
				else
				{
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}
			}

			if(_P7 != NULL)
			{
				if( (parse_dot(TFT_MAX_PORT_RANGE, TFT_MAX_PORT_VAL,(char*)_P7,&pkt_filter.source_port_low, &pkt_filter.source_port_high) == RESULT_OK )
				    && (pkt_filter.source_port_low<=pkt_filter.source_port_high) )
				{
					pkt_filter.present_src_port_range = TRUE;
				}
				else
				{
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}
			}


			if(_P9 != NULL)
			{
				if( parse_dot(TFT_MAX_TOS_RANGE,TFT_MAX_TOS_VAL,(char*)_P9,&tmpVal1, &tmpVal2) == RESULT_OK )
				{
					pkt_filter.present_tos = TRUE;
					pkt_filter.tos_addr = tmpVal1;
					pkt_filter.tos_mask	= tmpVal2;
				}
				else
				{
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}
			}


			if(_P4 != NULL)
			{
				pkt_filter.present_SrcAddrMask = TRUE;

				for(loop=0;loop<LEN_SOURCE_ADDR_SUBNET_MASK;loop++)
				{
					if( (*_P4 >= '0') && (*_P4<= '9') )
					{
						pkt_filter.source_addr_subnet_mask[loop] = *_P4-'0';
					}
					else
					{
				     	MSG_LOG("ATC: invalid src addr/mask - unexpected character");
						AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
						return AT_STATUS_DONE;
					}
					_P4++;

					if ( (*_P4 >= '0') && (*_P4<= '9') )
					{
						pkt_filter.source_addr_subnet_mask[loop] = pkt_filter.source_addr_subnet_mask[loop]*10 + *_P4-'0';
						_P4++;
						if ( (*_P4 >= '0') && (*_P4<= '9') )
						{
							pkt_filter.source_addr_subnet_mask[loop] = pkt_filter.source_addr_subnet_mask[loop]*10 + *_P4-'0';
							_P4++;
							if( (*_P4 != '.') && *_P4 && (loop != LEN_SOURCE_ADDR_SUBNET_MASK-1))
							{
								MSG_LOG("ATC: invalid src addr/mask - expeccting a dot I ");
								AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
								return AT_STATUS_DONE;
							}
						}
						else if ( (*_P4 != '.') && *_P4 && (loop != LEN_SOURCE_ADDR_SUBNET_MASK-1) )
						{
							MSG_LOG("ATC: invalid src addr/mask - expeccting a dot II ");
							AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
							return AT_STATUS_DONE;
						}

					}
					else  if ( (*_P4 != '.') && *_P4 && (loop != LEN_SOURCE_ADDR_SUBNET_MASK-1) )
					{
						MSG_LOG("ATC: invalid src addr/mask - expeccting a dot III ");
						AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
						return AT_STATUS_DONE;
					}
					_P4++;

					if(pkt_filter.source_addr_subnet_mask[loop] > TFT_MAX_ADDR_MASK)
					{
						MSG_LOG("ATC: Invalid addr/mask val");
						AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
						return AT_STATUS_DONE;
					}
				}	// end for

			}	//end if _p4 not NULL

 			resp = PDP_TftAddFilter(cid,&pkt_filter);

			if (resp != RESULT_OK) {
				MSG_LOG("ATC: fail to set TFT");
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}
			break;

		case AT_ACCESS_READ:
			for( idx_cid = MIN_CID; idx_cid <= MAX_CID; idx_cid++ ) {
				resp = PDP_GetUMTSTft(idx_cid,&r99Tft);
				if( (resp == RESULT_OK ) && (r99Tft.num_filters > 0) ) {
					numContexts++;

					for(idx = 0; idx < r99Tft.num_filters; idx++)
					{
						sprintf( output, "%s: %d, ", AT_GetCmdName(cmdId), idx_cid );

		 				sprintf( temp, "%d, ", r99Tft.pkt_filters[idx].packet_filter_id );
						strcat( output, temp );
		 				sprintf( temp, "%d, ", r99Tft.pkt_filters[idx].evaluation_precedence_idx );
						strcat( output, temp );

						if(r99Tft.pkt_filters[idx].present_SrcAddrMask)
						{
							for(loop=0;loop<LEN_SOURCE_ADDR_SUBNET_MASK;loop++) {
								if (!loop) {
									sprintf( temp, "%d", r99Tft.pkt_filters[idx].source_addr_subnet_mask[loop] );
									strcat( output, temp );
								}
								else {
									sprintf( temp, ".%d", r99Tft.pkt_filters[idx].source_addr_subnet_mask[loop] );
									strcat( output, temp );
								}
							}
							strcat(output,", ");
						}
						else
						{
							strcat(output,", ");
						}


						if(r99Tft.pkt_filters[idx].present_prot_num)
						{
		 					sprintf( temp, "%d, ", r99Tft.pkt_filters[idx].protocol_number );
							strcat( output, temp );
						}
						else
						{
							strcat(output,", ");
						}
						if(r99Tft.pkt_filters[idx].present_dst_port_range)
						{
		 					sprintf( temp, "%d.%d, ", r99Tft.pkt_filters[idx].destination_port_low,r99Tft.pkt_filters[idx].destination_port_high );
							strcat( output, temp );
						}
						else
						{
							strcat(output,", ");
						}
						if(r99Tft.pkt_filters[idx].present_src_port_range)
						{
		 					sprintf( temp, "%d.%d, ", r99Tft.pkt_filters[idx].source_port_low,r99Tft.pkt_filters[idx].source_port_high );
							strcat( output, temp );
						}
						else
						{
							strcat(output,", ");
						}

						//spi not supported
					 	strcat(output,", ");

						if(r99Tft.pkt_filters[idx].present_tos)
						{
			 				sprintf( temp, "%d.%d, ", r99Tft.pkt_filters[idx].tos_addr, r99Tft.pkt_filters[idx].tos_mask );
							strcat( output, temp );
						}
						else
						{
							strcat(output,", ");
						}

						//flow label not supported
						AT_OutputStr( chan, (const UInt8 *)output );
					}
				}
			}


			if (numContexts==0) {
				sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
				AT_OutputStr( chan, (const UInt8 *)output );
			}
			break;

		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"+CGTFT: (1-8),(0-255),(a1.a2.a3.a4.m1.m2.m3.m4),(0-255),(f.t),(f.t),(?),(t.m),(?)" );
			break;

		default:
			break;
	}

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;

}
#endif		//end of #ifdef STACK_edge


//******************************************************************************
// Function Name:	ATCmd_CGDCONT_Handler
//
// Description:		This is called by ATC/MMI for defining PDP Context
//					AT+CGDCONT=[cid[,PDP_type[,APN[,PDP_addr
//					[,d_comp[,h_comp]]]]]
// Notes:
//******************************************************************************
AT_Status_t ATCmd_CGDCONT_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1,
			const UInt8* _P2,
			const UInt8* _P3,
			const UInt8* _P4,
			const UInt8* _P5,
			const UInt8* _P6 )
{
	UInt8					numParms = 0;
	UInt8					idx;
	UInt8					dataComp, hdrComp;
	UInt8					cid;
	char					pdpType[20];
	char					apn[101];
	char 					reqPdpAddress[20];
	Result_t				resp;
	char					output[160];

	const PDPDefaultContext_t		*pDefaultContext;

	PCH_LOGV("ATCmd_CGDCONT_Handler:accessType=",accessType);

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			cid						= ( _P1 == NULL ) ? NOT_USED_0      : atoi((char*)_P1 );
			strcpy( pdpType,       ( _P2 == NULL ) ? PDP_TYPE_IP	   : (char*)_P2 );
			strcpy( apn,           ( _P3 == NULL ) ? NOT_USED_STRING : (char*)_P3 );
			strcpy( reqPdpAddress, ( _P4 == NULL ) ? NOT_USED_STRING : (char*)_P4 );

			dataComp = ( _P5 == NULL ) ? 0 : atoi((char*)_P5 );
			hdrComp  = ( _P6 == NULL ) ? 0 : atoi((char*)_P6 );

			dataComp  = ( dataComp == 0 ) ? COMPRESSION_OFF : COMPRESSION_ON;
			hdrComp	 = ( hdrComp == 0 ) ? COMPRESSION_OFF : COMPRESSION_ON;

			numParms++;
			if( _P2 != NULL )	numParms++;
			if( _P3 != NULL )	numParms++;
			if( _P4 != NULL )	numParms++;
			if( _P5 != NULL )	numParms++;
			if( _P6 != NULL )	numParms++;

			resp = PDP_SetPDPContext(cid,numParms,(UInt8 *)pdpType,(UInt8 *)apn,(UInt8 *)reqPdpAddress,dataComp,hdrComp);

			if (resp != RESULT_OK) {
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			break;

		case AT_ACCESS_READ:

			for( idx = MIN_CID; idx <= MAX_CID; idx++ ) {
				if (!PDP_IsSecondaryPdpDefined(idx)) {
					pDefaultContext = PDP_GetPDPContext(idx);
					if( pDefaultContext != NULL ) {
						numParms++;

						strncpy(pdpType,pDefaultContext->pdpType,sizeof(pdpType)-1);
						strncpy(apn,pDefaultContext->apn,sizeof(apn)-1);
						strncpy(reqPdpAddress,pDefaultContext->reqPdpAddress,sizeof(reqPdpAddress)-1);

						sprintf( output, "%s: %1d, \"%s\", \"%s\", \"%s\", %1d, %1d",
							 AT_GetCmdName(cmdId),
							 pDefaultContext->cid,
							 pdpType,
							 apn,
							 reqPdpAddress,
							 pDefaultContext->pchXid.dataComp == COMPRESSION_OFF ? 0 : 1,
							 pDefaultContext->pchXid.hdrComp  == COMPRESSION_OFF ? 0 : 1 );

						AT_OutputStr( chan, (const UInt8 *)output );
					}
				}
  			}

			if (numParms==0) {
				sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
				AT_OutputStr( chan, (const UInt8 *)output );
			}

			break;

		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"+CGDCONT: (1-10),(\"IP\"),,,(0,1),(0,1)" );
		default:
			break;
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;

}

//******************************************************************************
// Function Name:	ATCmd_CGDSCONT_Handler
//
// Description:		This is called by ATC/MMI for defining secondary PDP Context
//					AT+CGDSCONT=[cid[,primary_cid[,d_comp[,h_comp]]]]
// Notes:
//******************************************************************************
#if	defined(STACK_edge) || defined(STACK_wedge)
AT_Status_t ATCmd_CGDSCONT_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1,
			const UInt8* _P2,
			const UInt8* _P3,
			const UInt8* _P4)
{
	UInt8					numParms = 0;
	UInt8					idx;
	UInt8					dataComp, hdrComp;
	UInt8					cid, priCid;
	Result_t				resp;
	char					output[100];
	const PDPDefaultContext_t		*pDefaultContext;

	PCH_LOGV("ATCmd_CGDSCONT_Handler:accessType=",accessType);

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			cid						= ( _P1 == NULL ) ? NOT_USED_0      : atoi((char*)_P1 );
			priCid					= ( _P2 == NULL ) ? NOT_USED_0      : atoi((char*)_P2 );
			dataComp				= ( _P3 == NULL ) ? 0 : atoi((char*)_P3 );
			hdrComp					= ( _P4 == NULL ) ? 0 : atoi((char*)_P4 );

			numParms++;
			if( _P2 != NULL )	numParms++;
			if( _P3 != NULL )	numParms++;
			if( _P4 != NULL )	numParms++;

			dataComp  = ( dataComp == 2 ) ? COMPRESSION_ON : COMPRESSION_OFF;
			hdrComp	 = ( hdrComp == 2 ) ? COMPRESSION_ON : COMPRESSION_OFF;

			resp = PDP_SetSecPDPContext(cid,numParms,priCid,dataComp,hdrComp);

			if (resp != RESULT_OK) {
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			break;

		case AT_ACCESS_READ:
			for( idx = MIN_CID; idx <= MAX_CID; idx++ ) {

				if (PDP_IsSecondaryPdpDefined(idx)) {
					pDefaultContext = PDP_GetPDPContext(idx);
					if( pDefaultContext != NULL ) {
						numParms++;

						sprintf( output, "%s: %d, %d, %d, %d",
							 AT_GetCmdName(cmdId),
							 pDefaultContext->cid,
							 pDefaultContext->priCid,
							 pDefaultContext->pchXid.dataComp == COMPRESSION_OFF ? 0 : 2,
							 pDefaultContext->pchXid.hdrComp  == COMPRESSION_OFF ? 0 : 2 );
						AT_OutputStr( chan, (const UInt8 *)output );
					}
				}
  			}
			if (numParms==0) {
				sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
				AT_OutputStr( chan, (const UInt8 *)output );
			}

			break;

		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"+CGDSCONT: (1-10),(1-10),(0,1),(0,1)" );
		default:
			break;
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;

}
#endif			//end of #ifdef STACK_edge
//******************************************************************************
//
// Function Name:	ATC_Cgpaddr_CB
//
// Description:		Transition on Show PDP Address Request
//					AT+CGPADDR=[cid[,cid ... ]]
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_CGPADDR_Handler (
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
			const UInt8* _P8,
			const UInt8* _P9,
			const UInt8* _P10 )
{
	UInt8				idx,cid;
	UInt8				numContexts=0;
	Result_t			resp;
	char				temp[16];  //FIXME - increasing to 16 for IP addr for now
	const UInt8			*input[10];
	char				output[50];
	UInt8				cidList[MAX_PDP_CONTEXTS];

	PCH_LOGV("ATCmd_CGPADDR_Handler:accessType=",accessType);

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			input[0] = _P1;
			input[1] = _P2;
			input[2] = _P3;
			input[3] = _P4;
			input[4] = _P5;
			input[5] = _P6;
			input[6] = _P7;
			input[7] = _P8;
			input[8] = _P9;
			input[9] = _P10;

			for (idx = 0; idx < 10; idx++) {
				if (input[idx] == NULL)	break;
				cid	= atoi((char*)input[idx] );
				if ((MIN_CID <= cid) && (cid <= MAX_CID)) {
					resp = PDP_GetPDPAddress(cid, (UInt8 *)temp);
					if (resp == RESULT_OK) {
						numContexts++;
						sprintf( output, "%s: %d, \"%s\"", AT_GetCmdName(cmdId),
						 		  cid, temp );
						AT_OutputStr( chan, (const UInt8 *)output );
					}
				}
			}
			if( numContexts == 0 ) {
				sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
				AT_OutputStr( chan, (const UInt8 *)output );
			}

			break;

		case AT_ACCESS_READ:

			break;
		case AT_ACCESS_TEST:
			PDP_GetDefinedPDPContextCidList(&numContexts,cidList);

			sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
			for( idx = 0; idx < numContexts; idx++ ) {
				if( idx == numContexts-1 )
					sprintf( temp, "%d ", cidList[idx] );
				else
					sprintf( temp, "%d, ", cidList[idx] );
				strcat( output, temp );
			}
			AT_OutputStr( chan, (const UInt8 *)output );
			break;
		default:
			break;
	}

	AT_CmdRspOK(chan) ;
	return AT_STATUS_DONE;
}
//******************************************************************************
//
// Function Name:	ATCmd_CGACT_Handler
//
// Description:		Transition on PDP Context Activate / Deactivate Request
//					AT+CGACT=state[,cid]
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CGACT_Handler (
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
			const UInt8* _P8,
			const UInt8* _P9 )
{

	PCHActivateState_t	ActivateState;
	InterTaskMsg_t		*InterTaskMsg;
	ATCGACTData_t		*pCGACTData;

	PCH_LOGV("ATCmd_CGACT_Handler:accessType=",( accessType<<16|cmdId) );
	PCH_LOGV("ATCmd_CGACT_Handler:_P1=",(UInt32)_P1 );

	if( (_P1==NULL||_P2==NULL) && accessType == AT_ACCESS_REGULAR) {
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		return AT_STATUS_DONE;
	}

	ActivateState = (PCHActivateState_t)atoi((char *)_P1);

	// Generate CGATT process request msg to ATC task
	InterTaskMsg = AllocInterTaskMsgFromHeap(MSG_AT_CGACT_CMD, sizeof(ATCGACTData_t));

	pCGACTData = (ATCGACTData_t *)InterTaskMsg->dataBuf;
	pCGACTData->accessType = accessType;
	pCGACTData->cid = (UInt8)atoi((char *)_P2);
	pCGACTData->ActivateState = ActivateState;

	AT_SetCmdRspMpxChan(cmdId, chan);

    // send CGACT request to ATC task
	PostMsgToATCTask(InterTaskMsg);

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATCmd_CGDATA_Handler
//
// Description:		Transition on Enter Data State Request
//					AT+CGATA=[cid[,cid ... ]]
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CGDATA_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1,
			const UInt8* _P2 )
{
	PCHL2P_t	l2p;
	PCHCid_t	cid;
	Result_t	result;

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			strcpy( l2p, (_P1==NULL) ? NOT_USED_STRING:(char *)_P1 );
			cid = ( _P2 == NULL ) ? NOT_USED_0 : atoi((char *)_P2 );
			result = PDP_ActivateSNDCPConnection(AT_GetV24ClientIDFromMpxChan(chan), cid, l2p, pchDataStateCb);

			PCH_LOGV("Ben:ATCmd_CGDATA_Handler..result=",result);

			if (result == RESULT_ERROR) {
				return AT_STATUS_DONE;
			}
			return AT_STATUS_PENDING;

		case AT_ACCESS_READ:
			break;
		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"+CGDATA: (\"PPP\")" );
			break;
		default:
			break;
	}
	AT_CmdRspOK(chan) ;
	return AT_STATUS_DONE;
}

//******************************************************************************
//
// Function Name:	ATCmd_CGSEND_Handler
//
// Description:		Transition on sending continuous User Data.
//					AT*CGSEND=cid,number_of_bytes
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_CGSEND_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1,
			const UInt8* _P2 )
{
	UInt8		cid;
	UInt32		numberBytes;
	Result_t	result;

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			cid			= ( _P1 == NULL ) ? NOT_USED_0 : atoi((char *)_P1 );
			numberBytes = ( _P2 == NULL ) ? NOT_USED_0 : atoi((char *)_P2 );

			result = PDP_SendTBFData(cid,numberBytes);

			MSG_LOGV("ATC:ATCmd_CGSEND_Handler..size=",numberBytes);

			if (result == RESULT_ERROR) {
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}
			break;

		case AT_ACCESS_READ:
		case AT_ACCESS_TEST:
			break;
		default:
			break;
	}
	AT_CmdRspOK(chan) ;
	return AT_STATUS_DONE;
}

//******************************************************************************
// Function Name:	ATCmd_MSCLASS_Handler
//
// Description:		Set/Get GPRS/EDGE/HSCSD multi-slot class from sysparm.
//
// Notes:
//******************************************************************************
AT_Status_t ATCmd_MSCLASS_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1,
			const UInt8* _P2 )
{
	UInt8		type;
	UInt16		msClass;
	//Result_t	result;
	char		output[100];

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			type		= ( _P1 == NULL ) ? NOT_USED_0 : atoi((char *)_P1 );
			msClass		= ( _P2 == NULL ) ? NOT_USED_0 : atoi((char *)_P2 );

			MSG_LOGV("ATC:ATCmd_MSCLASS_Handler..(type/msClass)=",(type<<16|msClass) );
			if (SYS_GetSystemState() != SYSTEM_STATE_OFF) {
				MSG_LOG("ATC:*MSCLASS should be entered only in POWER-OFF state.." );
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			if (type == MS_CLASS_GPRS_TYPE) {
				SYSPARM_SetGPRSMSClass(msClass);
			}
			else if (type == MS_CLASS_EGPRS_TYPE) {
				SYSPARM_SetEGPRSMSClass(msClass);
			}
			else if (type == MS_CLASS_HSCSD_TYPE) {
				MSG_LOG("ATC:discard setting HSCSD ms class now.." );
			}

			break;

		case AT_ACCESS_READ:
			for (type=1;type<=3;type++) {
				msClass =	(type==MS_CLASS_GPRS_TYPE)? SYSPARM_GetMSClass():
							(type==MS_CLASS_EGPRS_TYPE)? SYSPARM_GetEGPRSMSClass(): SYSPARM_GetHSCSDMSClass();

				sprintf( output, "%s: %d, %d",
						 AT_GetCmdName(cmdId),
						 type,
						 msClass
						);
				AT_OutputStr( chan, (const UInt8 *)output );
			}

		case AT_ACCESS_TEST:
			break;
		default:
			break;
	}
	AT_CmdRspOK(chan) ;
	return AT_STATUS_DONE;
}

//==============================================================================
// New ATCmd Process Functions
//==============================================================================
//******************************************************************************
//
// Function Name:	ATProcessCGATTCmd
//
// Description:		This function is for ATC Task to process CGATT command.
//
// Notes:
//
//******************************************************************************
Result_t ATProcessCGATTCmd(InterTaskMsg_t* inMsg)
{
	AttachState_t		AttachState,CurrentState;
	ATCGATTData_t		*pCGATTData;
	UInt8				chan;
	char				output[30];

	pCGATTData = (ATCGATTData_t *)inMsg->dataBuf;

	CurrentState = MS_GetGPRSAttachStatus();
	AttachState = pCGATTData->AttachState;
	chan = AT_GetCmdRspMpxChan(AT_CMD_CGATT);

	MSG_LOGV("ATProcessCGATTCmd:accessType=",( pCGATTData->accessType<<24|chan<<16|CurrentState<<8|AttachState) );

	switch ( pCGATTData->accessType ) {

	case AT_ACCESS_REGULAR:

		if (IsDataCallActive()) {			// Ben:08/20/02: CGATT is not allowed
											// during CSD/Voice call, Here only
											// check CSD status for GC75. Have to
											// check the voice status in the future project.

			AT_CmdRsp( chan, AT_BUSY_RSP ) ;	// **FixMe** this clears the AT parser state
											// -- is this the desired behavior? (gdheinz)
			return RESULT_OK;
		}

		if (CurrentState == AttachState) {
			MSG_LOGV("ATCmd_CGATT_Handler: Already Attach/Detach..Current=",CurrentState);
			AT_CmdRspOK(chan);
			return RESULT_OK;
		}

		if( AttachState == ATTACHED )
		{
			if (MS_GetAttachMode() == ATTACH_MODE_GSM_ONLY) {
				MS_SetAttachMode(ATTACH_MODE_GSM_GPRS);
			}

			MS_SendCombinedAttachReq(atcClientID, SIM_IsPinOK(), SIM_GetSIMType(), REG_GPRS_ONLY, getSelectedPlmn());
			AT_CmdRspOK(chan);
		}
		else if( AttachState == DETACHED )
		{
			MS_SendDetachReq(atcClientID, DetachService, REG_GPRS_ONLY);
			AT_CmdRspOK(chan);
		}
		else
		{
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		}

		break;
	case AT_ACCESS_READ:
		sprintf( output, "%s: %d",
				 AT_GetCmdName(pCGATTData->cmdId ), CurrentState );
		AT_OutputStr(chan, (const UInt8 *)output );
		AT_CmdRspOK(chan);

		break;
	case AT_ACCESS_TEST:
		break;
	default:
		break;
	}

	return (RESULT_OK);
}


//******************************************************************************
//
// Function Name:	ATProcessCGACTCmd
//
// Description:		This function is for ATC Task to process CGACT command.
//
// Notes:
//
//******************************************************************************
Result_t ATProcessCGACTCmd(InterTaskMsg_t* inMsg)
{
	ATCGACTData_t		*pCGACTData;
	PCHActivateState_t	ActivateState;
	UInt8				cid;
	PCHProtConfig_t		cie;
	UInt8				numContexts;
	UInt8				idx;
	UInt8				chan;
	GPRSActivate_t		readActivate[MAX_PDP_CONTEXTS];
	char				output[50];


	pCGACTData = (ATCGACTData_t *)inMsg->dataBuf;
	cid = pCGACTData->cid;
	ActivateState = pCGACTData->ActivateState;
	chan = AT_GetCmdRspMpxChan(AT_CMD_CGACT);

	MSG_LOGV("ATProcessCGACTCmd:accessType=",( pCGACTData->accessType<<16|cid<<8|ActivateState) );

	switch ( pCGACTData->accessType ) {
		case AT_ACCESS_REGULAR:
			if( ActivateState == PDP_CONTEXT_ACTIVATED ) {
				if ( !PDP_IsSecondaryPdpDefined(cid)) {
					PDP_SendPDPActivateReq(AT_GetV24ClientIDFromMpxChan(chan),cid, pchActivateCb, ACTIVATE_ONLY, &cie);
				}
				else {

#if	defined(STACK_edge) || defined(STACK_wedge)
					PDP_SendPDPActivateSecReq(AT_GetV24ClientIDFromMpxChan(chan), cid, pchActivateSecCb);
#endif

				}

			}
			else if( ActivateState == PDP_CONTEXT_DEACTIVATED ) {
				PDP_SendPDPDeactivateReq(AT_GetV24ClientIDFromMpxChan(chan), cid, pchDeactivateCb);
			}
			else {
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			}
			break;

		case AT_ACCESS_READ:
			PDP_GetGPRSActivateStatus(&numContexts, readActivate);

			if( numContexts == 0 ) {
					sprintf( output, "%s: ", AT_GetCmdName(AT_CMD_CGACT) );
					AT_OutputStr(chan, (const UInt8 *)output );
			}
			else {
				for( idx = 0; idx < numContexts; idx++ ) {
					sprintf( output, "%s: %d, %d",
							 AT_GetCmdName(AT_CMD_CGACT),
			 	 			 readActivate[idx].cid,
			 	 			 readActivate[idx].state );
					AT_OutputStr(chan, (const UInt8 *)output );
				}
			}
			AT_CmdRspOK(chan);

			break;

		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"+CGACT: (0,1)" );
			break;

		default:
			break;
	}


	return (RESULT_OK);
}


/*==================== End of atc_pch.c ==================*/
