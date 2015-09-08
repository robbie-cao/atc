/**
*
*   @file   at_plmn.c
*
*   @brief  This file contains ATC functions that are related to network registration.
*
****************************************************************************/

//******************************************************************************
//							include block
//******************************************************************************
#define ENABLE_LOGGING

#include "at_plmn.h"
#include "mti_trace.h"
#include "mti_build.h"
#include <stdio.h>
#include "stdlib.h"
#include "mstypes.h"
#include "v24.h"
#include "nvram.h"
#include "sysparm.h"
#include "bandselect.h"
#include "at_data.h"
#include "at_util.h"
#include "at_cfg.h"
#include "sim_mi.h"
#include "sim_api.h"
#include "pchfuncs.h"
#include "pchtypes.h"

#include "memmap.h"
#include "mstruct.h"

#include "ms_database.h"
#include "systemapi.h"

#include "at_api.h"
#include "at_string.h"
#include "pchapi.h"
#include "pchdef.h"
#include "log.h"
#include "plmnselect.h"
#include "plmn_table.h"
#include "osheap.h"

//******************************************************************************
//							define
//******************************************************************************


//******************************************************************************
//							prototypes block
//******************************************************************************
static AT_Status_t ATCmd_BandSelection_Handler (
				AT_CmdId_t cmdId,
				UInt8 chan,
				UInt8 accessType,
				const UInt8* _P1 );


//******************************************************************************
//							Variables
//******************************************************************************
UInt8	atGsmRadioInfoState		=	0xFF;
UInt8	atUmtsRadioInfoState	=	0xFF;
UInt8	atGsmRegStatusInfo		=	0;
UInt8	atGprsRegStatusInfo		=	0;

extern	UInt8	atcClientID;


//******************************************************************************
//							Functions
//******************************************************************************







//===================================================================
/* ============ AT Command Handlers (Forward Path) ================*/
//===================================================================

//------------------------------------------------------------------------
// ATCmd_COPS_Handler:	invoke by AT parser for +COPS at V24 task context
//------------------------------------------------------------------------
AT_Status_t	 ATCmd_COPS_Handler(AT_CmdId_t cmdID,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* p1,
								const UInt8* p2,
								const UInt8* p3   )
{
	InterTaskMsg_t*		msg;
	ATCOPSData_t*		pCopsData;

	// Generate COPS process request msg to ATC task
	msg = AllocInterTaskMsgFromHeap(MSG_AT_COPS_CMD, sizeof(ATCOPSData_t));

	pCopsData = (ATCOPSData_t *)msg->dataBuf;
	pCopsData->accessType = accessType;
	pCopsData->copsSelection = (p1 == NULL) ? 0 : atoi((char*)p1);
	pCopsData->copsFormat = (p2 == NULL)? 0 : atoi((char*)p2);

	if(p3 != NULL)
	{
		strncpy(pCopsData->plmnValue, (char *) p3, sizeof(pCopsData->plmnValue) - 1);
		pCopsData->plmnValue[sizeof(pCopsData->plmnValue) - 1] = '\0';
	}
	else
	{
		pCopsData->plmnValue[0] = '\0';
	}

    // send to COPS request to ATC task
	PostMsgToATCTask(msg);

	return AT_STATUS_PENDING;
}


//------------------------------------------------------------------------
// ATProcessCOPSCmd: process +COPS cmd req from AT parser
//------------------------------------------------------------------------
void	ATProcessCOPSCmd(InterTaskMsg_t* inMsg)
{
	/* Allocate a large buffer to format output: PLMN name in UCS2 format takes a lot of space */
	char*			output = OSHEAP_Alloc(200);
	ATCOPSData_t*	pCopsData = (ATCOPSData_t *) inMsg->dataBuf;
	char*			cmd_str = (char *) AT_GetCmdName(AT_CMD_COPS);
	UInt8			chan = AT_GetCmdRspMpxChan(AT_CMD_COPS);
	Result_t		result;
	Boolean			netReqSent;
	PlmnSelectMode_t plmn_mode;

	switch(pCopsData->accessType)
	{
		case AT_ACCESS_REGULAR:
			result = MS_PlmnSelect(
						AT_GetV24ClientIDFromMpxChan(chan),
						ATC_IsCharacterSetUcs2(),
						pCopsData->copsSelection,
						pCopsData->copsFormat,
						pCopsData->plmnValue, &netReqSent);

			if (result == RESULT_ERROR) {
      	    	AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			}
			else if( (result == RESULT_OK) && !netReqSent )
			{
 				AT_CmdRspOK(chan);
			}
			/* Else: we will wait for the network response to send out the result to the host */

			break;

		case AT_ACCESS_READ:		// read - AT+COPS?
			plmn_mode = MS_GetPlmnMode();

   	   		if( !MS_IsGSMRegistered() && !MS_IsGPRSRegistered() ){

				/* No operator selected, return <mode>: according to GSM 07.07 */
				sprintf(output, "%s: %d", cmd_str, plmn_mode);
				AT_OutputStr(chan, (UInt8 *)output);
				AT_CmdRspOK( chan ) ;
   	   		}
   	   		else
			{
				PlmnSelectFormat_t plmn_format = MS_GetPlmnFormat();
				UInt16 raw_mcc = MS_GetPlmnMCC();
				UInt8 raw_mnc = MS_GetPlmnMNC();
				UInt8 plmn_str[7];

				if(plmn_format == PLMN_FORMAT_NUMERIC)
				{
					MS_ConvertRawPlmnToHer(raw_mcc, raw_mnc, plmn_str);

   	   	       		sprintf(output, "%s: %d,%d,%s", cmd_str, plmn_mode, plmn_format, plmn_str);
					AT_OutputStr(chan, (UInt8 *) output);
   	   			}
				else
				{    // Alphanum short or long
					PLMN_NAME_t plmn_name;
					char *plmn_name_str = NULL;

					if( MS_GetPLMNNameByCode( raw_mcc, raw_mnc, MS_GetRegisteredLAC(), ATC_IsCharacterSetUcs2(),
							(plmn_format == PLMN_FORMAT_LONG) ? &plmn_name : NULL,
							(plmn_format == PLMN_FORMAT_LONG) ? NULL : &plmn_name) )
					{
						plmn_name_str = MS_ConvertPLMNNameStr(ATC_IsCharacterSetUcs2(), &plmn_name);
					}

					if(plmn_name_str == NULL)
					{
						/* We can not find the corresponding PLMN name from our table for the registered
						 * PLMN. We return the numerical PLMN value in a string, e.g. "310170".
						 */
						MS_ConvertRawPlmnToHer(raw_mcc, raw_mnc, plmn_str);
   	   	       			sprintf(output, "%s: %d,%d,\"%s\"", cmd_str, plmn_mode, plmn_format, plmn_str);
					}
					else
					{
   	   					sprintf(output, "%s: %d,%d,\"%s\"", cmd_str, plmn_mode, plmn_format, plmn_name_str);
						OSHEAP_Delete(plmn_name_str);
					}

					AT_OutputStr(chan, (UInt8 *) output);
				}

				AT_CmdRspOK(chan);
			}

			break;

		case AT_ACCESS_TEST:	// for AT+COPS=?
   	   		MS_SearchAvailablePLMN();
			break ;
	}

	OSHEAP_Delete(output);
}


/************************************************************************************
*
*	Function Name:	ATCmd_CPOL_Handler
*
*	Description:	This function handles the AT+CPOL command (Preferred PLMN network read/write).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CPOL_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1,
								const UInt8* _P2, const UInt8* _P3 )
{
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	if ( accessType == AT_ACCESS_REGULAR )
   	{
      	UInt8  index;
		UInt8  format;
		PLMNId_t plmn_id;
		PLMN_ID_t sim_plmn_id;
		UInt16 mcc;
		UInt16 mnc;

		index = ((_P1 == NULL) || (_P1[0] == '\0')) ? NO_INDEX : atoi((char *) _P1);

		if(index == 0)
		{
      		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			return AT_STATUS_DONE;
		}

		if( (_P3 != NULL) && (_P3[0] != '\0') )
		{
			sim_plmn_id.order = index;

			/* If <format> is not provided in P2, we use the saved format */
			format = ((_P2 == NULL) || (_P2[0] == '\0')) ? channelCfg->CPOL : atoi((char *) _P2);

			switch(format)
			{
				case LONG_FORMAT:
				case SHORT_FORMAT:
					if( !MS_GetPLMNCodeByName( ATC_IsCharacterSetUcs2(), format == LONG_FORMAT, (char *)_P3, &mcc, &mnc) )
					{
						/* Can not find the PLMN from the supported PLMN list */
						AT_CmdRspError(chan, AT_CME_ERR_NOT_FOUND);
						return AT_STATUS_DONE;
					}
					else
					{
						MS_ConvertMccMncToRawPlmnid(mcc, mnc, &plmn_id);
					}
					break;

				case NUMERIC_FORMAT:
				default:
					// get plmn id
					if(!MS_ConvertStringToRawPlmnId((UInt8 *) _P3, &plmn_id))
					{
      	    			AT_CmdRspError(chan, AT_CME_ERR_INVALID_CHAR_INSTR);
						return AT_STATUS_DONE;
					}
					break;
			}

			sim_plmn_id.mcc = plmn_id.mcc;
			sim_plmn_id.mnc = plmn_id.mnc;

			// update Prefer PLMN list
			SIM_SendUpdatePrefListReq(AT_GetV24ClientIDFromMpxChan(chan), &sim_plmn_id, SIMPLMNACTION_INSERT, PostMsgToATCTask);
			return AT_STATUS_PENDING;
		}
		else
		{
			// delete the entry indicated by index
			if(index != NO_INDEX)
			{
				sim_plmn_id.order = index;

				SIM_SendUpdatePrefListReq(AT_GetV24ClientIDFromMpxChan(chan), &sim_plmn_id, SIMPLMNACTION_DELETE, PostMsgToATCTask);
				return AT_STATUS_PENDING;
			}
			else
			{
				// only set format
				if(_P2 != NULL)
				{
					AT_GetChannelCfg(chan)->CPOL = atoi((char *) _P2);
				}

   	   			AT_CmdRspOK(chan);
				return AT_STATUS_DONE;
			}
		}
	}
   	else if ( accessType == AT_ACCESS_READ )
	{
		SIM_SendWholeBinaryEFileReadReq( AT_GetV24ClientIDFromMpxChan(chan), APDUFILEID_EF_PLMNSEL,
			(SIM_GetApplicationType() == SIM_APPL_2G) ? APDUFILEID_DF_GSM : APDUFILEID_USIM_ADF,
			SIM_GetMfPathLen(), SIM_GetMfPath(), PostMsgToATCTask );

		return AT_STATUS_PENDING;
	}
   	else
   	{

		SIM_SendEFileInfoReq( AT_GetV24ClientIDFromMpxChan(chan), APDUFILEID_EF_PLMNSEL,
						  (SIM_GetApplicationType() == SIM_APPL_2G) ? APDUFILEID_DF_GSM : APDUFILEID_USIM_ADF,
						  SIM_GetMfPathLen(), SIM_GetMfPath(), PostMsgToATCTask );

		return AT_STATUS_PENDING;
   	}
}


/************************************************************************************
*
*	Function Name:	ATCmd_CGREG_Handler
*
*	Description:	This function handles the AT+CGREG command .
*
*
*************************************************************************************/
AT_Status_t ATCmd_CGREG_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1 )
{
	AT_ChannelCfg_t*			channelCfg = AT_GetChannelCfg(chan);
	const UInt8*				cmd_name = AT_GetCmdName(cmdId);
	char						output[50];
	MSRegState_t				netRegStatus;

	netRegStatus = MS_GetGPRSRegState();



	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			channelCfg->CGREG = (_P1 == NULL ) ? NOT_USED_0 : atoi((char*)_P1);

			AT_CmdRspOK(chan);

			if(channelCfg->CGREG == 2)
			{
				if( SYS_GetGPRSRegistrationStatus() == REGISTERSTATUS_NORMAL )
				{
					sprintf( output, "%s: %d,\"%04X\",\"%04X\"", cmd_name, netRegStatus,
							  MS_GetRegisteredLAC(), MS_GetRegisteredCellInfo() );
				}
				else
				{
					sprintf( output, "%s: %d,\"\",\"\"", cmd_name, netRegStatus );
				}

				AT_OutputStr( chan, (const UInt8 *)output );
			}
			else if(channelCfg->CGREG == 1)
			{
				sprintf( output, "%s: %d", cmd_name, netRegStatus );
				AT_OutputStr( chan, (const UInt8 *)output );
			}

			break;

		case AT_ACCESS_READ:
			if(channelCfg->CGREG == 2)
			{
				if(SYS_GetGPRSRegistrationStatus() == REGISTERSTATUS_NORMAL)
				{
	    			sprintf( output,"%s: %d,%d,\"%04X\",\"%04X\"",
							cmd_name, channelCfg->CGREG, netRegStatus,
							MS_GetRegisteredLAC(), MS_GetRegisteredCellInfo() );
				}
				else
				{
					sprintf( output,"%s: %d,%d,\"\",\"\"",
							cmd_name, channelCfg->CGREG, netRegStatus );
				}

				AT_OutputStr( chan, (const UInt8 *)output );
			}
			else
			{
				sprintf( output,"%s: %d,%d", cmd_name, channelCfg->CGREG, netRegStatus );
				AT_OutputStr( chan, (const UInt8 *)output );
			}

			AT_CmdRspOK(chan) ;
			break;

		case AT_ACCESS_TEST:
		default:
			break;
	}

	return AT_STATUS_DONE;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CREG_Handler
*
*	Description:	This function handles the AT+CREG command .
*
*
*************************************************************************************/
AT_Status_t ATCmd_CREG_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1 )
{
	AT_ChannelCfg_t*			channelCfg = AT_GetChannelCfg(chan);
	const UInt8*				cmd_name = AT_GetCmdName(cmdId);
	char						output[50];
	MSRegState_t				netRegStatus;

	netRegStatus = MS_GetGSMRegState();

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			channelCfg->CREG = (_P1 == NULL ) ? NOT_USED_0:atoi((char*)_P1);

			AT_CmdRspOK(chan);

			if(channelCfg->CREG == 1)
			{
				sprintf( output, "%s: %d", cmd_name, netRegStatus );
				AT_OutputStr( chan, (const UInt8 *)output );
			}
			else if(channelCfg->CREG == 2 )
			{
				if( SYS_GetGSMRegistrationStatus() == REGISTERSTATUS_NORMAL )
				{
					sprintf( output, "%s: %d,\"%04X\",\"%04X\"", cmd_name, netRegStatus,
									MS_GetRegisteredLAC(), MS_GetRegisteredCellInfo() );
				}
				else
				{
					sprintf( output, "%s: %d,\"\",\"\"", cmd_name, netRegStatus );
				}

				AT_OutputStr( chan, (const UInt8 *)output );
			}

			break;

		case AT_ACCESS_READ:
			if(channelCfg->CREG == 2)
			{
				if(SYS_GetGSMRegistrationStatus() == REGISTERSTATUS_NORMAL)
				{
	    			sprintf( output,"%s: %d,%d,\"%04X\",\"%04X\"",
							cmd_name, channelCfg->CREG, netRegStatus,
							MS_GetRegisteredLAC(), MS_GetRegisteredCellInfo() );
				}
				else
				{
					sprintf( output,"%s: %d,%d,\"\",\"\"",
							cmd_name, channelCfg->CREG, netRegStatus );
				}
			}
			else
			{
				sprintf(output,"%s: %d,%d", cmd_name, channelCfg->CREG, netRegStatus );
			}

			AT_OutputStr(chan, (const UInt8 *)output );
			AT_CmdRspOK(chan) ;
			break;

		case AT_ACCESS_TEST:
		default:
			break;
	}

	return AT_STATUS_DONE;
}


//******************************************************************************
// Function Name:	ATCmd_COPN_Handler
//
// Description:		Transition on AT+COPN
//
// Notes:
//******************************************************************************
AT_Status_t ATCmd_COPN_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType )
{
	switch ( accessType )
	{
		case AT_ACCESS_REGULAR:
		{
			/* Allocate a large buffer to format output: PLMN name in UCS2 format takes a lot of space */
			char						*output = OSHEAP_Alloc(200);
			char						*long_name_str;
			PLMN_NAME_t					*long_name = OSHEAP_Alloc(sizeof(PLMN_NAME_t));
			const UInt8					*cmd_name = AT_GetCmdName(cmdId);
			UInt16						i;
			UInt16						plmn_list_size = MS_GetPLMNListSize();
			UInt16						mcc;
			UInt16						mnc;
			Boolean						ucs2_flag = ATC_IsCharacterSetUcs2();

			for(i = 0; i < plmn_list_size; i++)
			{
				MS_GetPLMNEntryByIndex(i, ucs2_flag, &mcc, &mnc, long_name, NULL);
				long_name_str = MS_ConvertPLMNNameStr(ucs2_flag, long_name);

				/* Check two-digit or 3-digit MNC and display accordingly */
				if(mnc & 0x0F00) {
					sprintf( output, "%s: %03X%3X,\"%s\"", cmd_name,
							 mcc, mnc, long_name_str );
				}
				else {
					sprintf( output, "%s: %03X%02X,\"%s\"", cmd_name,
							 mcc, mnc, long_name_str );
				}

				OSHEAP_Delete(long_name_str);

				// KLUDGE to keep output buffers from overflowing
				OSTASK_Sleep( TICKS_ONE_SECOND / 100 );
				AT_OutputStr( chan, (const UInt8 *)output );
			}

			OSHEAP_Delete(output);
			OSHEAP_Delete(long_name);

			break;
		}

		case AT_ACCESS_READ:
		case AT_ACCESS_TEST:
		default:
			break;
	}

	AT_CmdRspOK(chan) ;
	return AT_STATUS_DONE;
}


//******************************************************************************
// Function Name:	ATCmd_EBSE_Handler
//
// Description:		Transition on AT*EBSE
//
// Notes:
//******************************************************************************
AT_Status_t ATCmd_EBSE_Handler (
				AT_CmdId_t cmdId,
				UInt8 chan,
				UInt8 accessType,
				const UInt8* _P1 )
{
	AT_Status_t res;

	res = ATCmd_BandSelection_Handler(cmdId, chan, accessType, _P1);

	return res;
}


//******************************************************************************
// Function Name:	ATCmd_MBSEL_Handler
//
// Description:		Transition on AT*MBSEL
//
// Notes:
//******************************************************************************
AT_Status_t ATCmd_MBSEL_Handler (
				AT_CmdId_t cmdId,
				UInt8 chan,
				UInt8 accessType,
				const UInt8* _P1 )
{
	AT_Status_t res;

	res = ATCmd_BandSelection_Handler(cmdId, chan, accessType, _P1);

	return res;
}


//******************************************************************************
// Function Name:	ATCmd_BandSelection_Handler
//
// Description:		Transition on AT*EBSE or AT*MBSEL
//
// Notes:
//******************************************************************************
AT_Status_t ATCmd_BandSelection_Handler (
				AT_CmdId_t cmdId,
				UInt8 chan,
				UInt8 accessType,
				const UInt8* _P1 )
{
	UInt8			ebse;
	BandSelect_t	bandSelect;
	MsConfig_t*		msCfg = MS_GetCfg( ) ;

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			ebse = (_P1 == NULL) ? 7 : atoi( (char*)_P1 );

			switch (ebse) {
				case 0:
					bandSelect = BAND_GSM900_ONLY;
					break;
				case 1:
					bandSelect = BAND_DCS1800_ONLY;
					break;
				case 2:
					bandSelect = BAND_GSM900_DCS1800;
					break;
				case 3:
					bandSelect = BAND_PCS1900_ONLY;
					break;
				case 4:
					bandSelect = BAND_AUTO;
					break;
				case 5:
					bandSelect = BAND_GSM850_ONLY;
					break;
				case 6:
					bandSelect = BAND_PCS1900_GSM850;
					break;
#ifdef STACK_wedge
				case 7:
					bandSelect = BAND_UMTS2100_ONLY;
					break;
				case 8:
					bandSelect = BAND_UMTS1900_ONLY;
					break;
#endif // #ifdef STACK_wedge
				default:
	      			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
					return AT_STATUS_DONE;

			}

			if ( SYS_SelectBand(bandSelect) == RESULT_OK) {
				msCfg->EBSE = ebse;
				AT_CmdRspOK( chan );
			}
			else {
      			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
			}

			break;

		case AT_ACCESS_READ:
		case AT_ACCESS_TEST:
			AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,FALSE,5,&msCfg->EBSE);
	default:
		break;
	}

	return AT_STATUS_DONE;
}


#ifdef GCXX_PC_CARD

//******************************************************************************
//
// Function Name:	ATCmd_EPNR_Handler
//
// Description:		This function handles the AT*EPNR command (Preferred PLMN read)
//
//******************************************************************************
AT_Status_t ATCmd_EPNR_Handler( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
							const UInt8* _P1, const UInt8* _P2, const UInt8* _P3 )
{
	if(accessType == AT_ACCESS_REGULAR)
   	{
		if(_P1 != NULL && _P1[0] != '\0')
		{
			/* We only support numeric format: 2 */
			if( (atoi((char *) _P1)) != NUMERIC_FORMAT )
			{
      			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
				return AT_STATUS_DONE;
			}
		}

		/* _P2 is the starting index, _P3 is the ending index. we will access
		 * these two parameters after the SIM access.
		 */
		SIM_SendWholeBinaryEFileReadReq( AT_GetV24ClientIDFromMpxChan(chan), APDUFILEID_EF_PLMNSEL,
									 (SIM_GetApplicationType() == SIM_APPL_2G) ? APDUFILEID_DF_GSM : APDUFILEID_USIM_ADF,
									 SIM_GetMfPathLen(), SIM_GetMfPath(), PostMsgToATCTask );

		return AT_STATUS_PENDING;
	}
   	else if(accessType == AT_ACCESS_READ)
	{
 		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
		return AT_STATUS_DONE;
	}
	else
	{
		SIM_SendEFileInfoReq( AT_GetV24ClientIDFromMpxChan(chan), APDUFILEID_EF_PLMNSEL,
						  (SIM_GetApplicationType() == SIM_APPL_2G) ? APDUFILEID_DF_GSM : APDUFILEID_USIM_ADF,
						  SIM_GetMfPathLen(), SIM_GetMfPath(), PostMsgToATCTask );

		return AT_STATUS_PENDING;
	}
}


//******************************************************************************
//
// Function Name:	ATCmd_EPNW_Handler
//
// Description:		This function handles the AT*EPNW command (Preferred PLMN write)
//
//******************************************************************************
AT_Status_t ATCmd_EPNW_Handler( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
						const UInt8* _P1, const UInt8* _P2, const UInt8* _P3 )
{
	if(accessType == AT_ACCESS_REGULAR)
   	{
		PLMNId_t plmn_id;
		PLMN_ID_t sim_plmn_id;

		if( (_P2 != NULL) && (_P2[0] != '\0') )
		{
			/* We only support numeric format: 2 */
			if( atoi((char *) _P2) != NUMERIC_FORMAT )
			{
      			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
				return AT_STATUS_DONE;
			}
		}

		sim_plmn_id.order = ((_P1 == NULL) || (_P1[0] == '\0')) ? NO_INDEX : atoi((char *) _P1);

		if(sim_plmn_id.order == 0)
		{
      	    AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			return AT_STATUS_DONE;
		}

		if( (_P3 != NULL) && (_P3[0] != '\0') )
		{
			if(!MS_ConvertStringToRawPlmnId((UInt8 *) _P3, &plmn_id))
			{
      	    	AT_CmdRspError(chan, AT_CME_ERR_INVALID_CHAR_INSTR);
				return AT_STATUS_DONE;
			}

			sim_plmn_id.mcc = plmn_id.mcc;
			sim_plmn_id.mnc = plmn_id.mnc;

			SIM_SendUpdatePrefListReq(AT_GetV24ClientIDFromMpxChan(chan), &sim_plmn_id, SIMPLMNACTION_INSERT, PostMsgToATCTask);
			return AT_STATUS_PENDING;
		}
		else   // P3 has no information, i.e. <oper> field empty
		{
			// delete the entry indicated by index
			if(sim_plmn_id.order != NO_INDEX)
			{
				SIM_SendUpdatePrefListReq(AT_GetV24ClientIDFromMpxChan(chan), &sim_plmn_id, SIMPLMNACTION_DELETE, PostMsgToATCTask);
				return AT_STATUS_PENDING;
			}
			else
			{
				/* Only set format */
				AT_CmdRspOK(chan);
				return AT_STATUS_DONE;
			}
		}
	}
   	else if (accessType == AT_ACCESS_READ)
	{
 		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
		return AT_STATUS_DONE;
	}
	else
	{
		SIM_SendEFileInfoReq( AT_GetV24ClientIDFromMpxChan(chan), APDUFILEID_EF_PLMNSEL,
						  (SIM_GetApplicationType() == SIM_APPL_2G) ? APDUFILEID_DF_GSM : APDUFILEID_USIM_ADF,
						  SIM_GetMfPathLen(), SIM_GetMfPath(), PostMsgToATCTask );

		return AT_STATUS_PENDING;
	}
}

#endif // #ifdef GCXX_PC_CARD


/************************************************************************************
*
*	Function Name:	ATCmd_MGBAND_Handler
*
*	Description:	This function handles the AT*MGBAND command .
*
*
*************************************************************************************/
AT_Status_t ATCmd_MGBAND_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1 )
{
	UInt8						goodBand;
	char						output[50];

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			goodBand = (_P1 == NULL ) ? NOT_USED_0:atoi((char*)_P1);
			MS_SetStartBand(goodBand);
			AT_CmdRspOK(chan) ;
			break;

		case AT_ACCESS_READ:
			sprintf(output,"%s: %d", AT_GetCmdName(cmdId), MS_GetStartBand() );
			AT_OutputStr(chan, (const UInt8 *)output );
			AT_CmdRspOK(chan) ;
			break;

		case AT_ACCESS_TEST:
		default:
			break;
	}

	return AT_STATUS_DONE;
}

/************************************************************************************
*
*	Function Name:	ATCmd_MRINFO_Handler
*
*	Description:	This function handles the AT*MRINFO command for Radio Access
*					information.
*
*
*************************************************************************************/
AT_Status_t ATCmd_MRINFO_Handler (
			AT_CmdId_t cmdId,
			UInt8 chan,
			UInt8 accessType,
			const UInt8* _P1 )
{
	UInt8						mode;
	char						output[50];
	AT_CommonCfg_t*				AtCfg = AT_GetCommonCfg();

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			mode = (_P1 == NULL ) ? NOT_USED_0:atoi((char*)_P1);
			AtCfg->MRINFO = mode;

			if (mode) {
				sprintf(output,"%s: %d, %d", AT_GetCmdName(cmdId), atGsmRadioInfoState, atUmtsRadioInfoState);
				AT_OutputStr(chan, (const UInt8 *)output );
			}

			AT_CmdRspOK(chan) ;
			break;

		case AT_ACCESS_READ:
			sprintf(output,"%s: %d, %d", AT_GetCmdName(cmdId), atGsmRadioInfoState, atUmtsRadioInfoState);
			AT_OutputStr(chan, (const UInt8 *)output );
			AT_CmdRspOK(chan) ;
			break;

		case AT_ACCESS_TEST:
		default:
			break;
	}

	return AT_STATUS_DONE;
}

//===============================================================
/* ============ AT Async Event/Response Event Handlers ========*/
//===============================================================

//------------------------------------------------------------------------
// AtHandlePlmnListInd:	handle MSG_PLMNLIST_IND msg to ATC task.
//------------------------------------------------------------------------
Result_t	AtHandlePlmnListInd(InterTaskMsg_t* msg)
{
	SEARCHED_PLMN_LIST_t *list_ptr = (SEARCHED_PLMN_LIST_t *) msg->dataBuf;
	UInt8 chan = AT_GetCmdRspMpxChan(AT_CMD_COPS);

	if(list_ptr->plmn_cause == PLMNCAUSE_SUCCESS)
	{
		UInt8 i;
		UInt8 stat;
		UInt16 converted_mcc;
		UInt16 converted_mnc;
		char *long_name_str;
		char *short_name_str;
		char *output = NULL;
		SEARCHED_PLMNId_t *searched_plmn;
		const UInt8	*cmd_name = AT_GetCmdName(AT_CMD_COPS);
		PLMN_NAME_t long_name;
		PLMN_NAME_t short_name;

		/* Allocate a large buffer to format string for AT output */
		if(list_ptr->num_of_plmn > 0)
		{
			output = OSHEAP_Alloc(300);
		}

  		// show the PLMN list
    	for(i = 0, searched_plmn = plmn_select_list.list; i < list_ptr->num_of_plmn; i++, searched_plmn++)
    	{
			converted_mcc = MS_PlmnConvertRawMcc(searched_plmn->mcc);
			converted_mnc = MS_PlmnConvertRawMnc(searched_plmn->mcc, searched_plmn->mnc);

			if( MS_GetPLMNNameByCode( searched_plmn->mcc, searched_plmn->mnc,
					searched_plmn->lac, ATC_IsCharacterSetUcs2(), &long_name, &short_name ) )
			{
				if(searched_plmn->is_forbidden)
				{
					stat = 3; /* Forbidden network */
				}
				else if( (searched_plmn->mcc == MS_GetPlmnMCC() ) &&
						 (searched_plmn->mnc == MS_GetPlmnMNC() ) &&
						 SYS_IsRegisteredGSMOrGPRS() )
				{
					stat = 2; /* Current registered network */
				}
				else
				{
					stat = 1; /* Available network */
				}

				long_name_str = MS_ConvertPLMNNameStr(ATC_IsCharacterSetUcs2(), &long_name);
				short_name_str = MS_ConvertPLMNNameStr(ATC_IsCharacterSetUcs2(), &short_name);

      			if ( ((converted_mnc & 0x0F00) != 0) || ((searched_plmn->mcc & 0x00F0) != 0x00F0) )
    	  		{
					sprintf( output, "%s: (%d,\"%s\",\"%s\",\"%03X%03X\"),",
				   		cmd_name, stat,
						long_name_str == NULL ? "" : long_name_str,
						short_name_str == NULL ? "" : short_name_str,
						converted_mcc, converted_mnc );
				}
				else
				{
                   sprintf( output, "%s: (%d,\"%s\",\"%s\",\"%03X%02X\"),",
				   		cmd_name, stat,
						long_name_str == NULL ? "" : long_name_str,
						short_name_str == NULL ? "" : short_name_str,
						converted_mcc, converted_mnc );
				}
#ifdef STACK_wedge
				if (searched_plmn->rat == RAT_GSM)
				{
					strcat(output, "0");
				}
				else if (searched_plmn->rat == RAT_UMTS)
				{
					strcat(output, "2");
				}
#endif
				strcat( output, ",(0,1,2,3,4),(0,1,2)");

				if(long_name_str != NULL)
				{
					OSHEAP_Delete(long_name_str);
				}

				if(short_name_str != NULL)
				{
					OSHEAP_Delete(short_name_str);
				}
    		}
			else
			{
				// for unknown network
				stat = 0;

      			if ( ((converted_mnc & 0x0F00) != 0) || ((searched_plmn->mcc & 0x00F0) != 0x00F0) )
    	  		{
		   			sprintf( output, "%s: (%d,\"\",\"\",\"%03X%03X\"),,(0,1,2,3,4),(0,1,2)",
    	  				cmd_name, stat, converted_mcc, converted_mnc );
				}
				else
				{
      	   			sprintf( output, "%s: (%d,\"\",\"\",\"%03X%02X\"),,(0,1,2,3,4),(0,1,2)",
    	  				cmd_name, stat, converted_mcc, converted_mnc );
				}
			}

    	  	AT_OutputStr(chan, (UInt8*) output);
    	}

		if(output != NULL)
		{
			OSHEAP_Delete(output);
		}

		// show OK and close the channel
		AT_CmdRspOK(chan);
	}
	else
	{
		if (list_ptr->plmn_cause == PLMNCAUSE_NETWORK_NOT_FOUND)
		{
	        AT_CmdRspError(chan, AT_CME_ERR_NOT_FOUND);
		}
		else
		{
	        AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		}
	}

	return 	RESULT_OK;
}


/************************************************************************************
*
*	Function Name:	AtOutputCregStatus
*
*	Description:	This function sends out the unsolicited CREG status based upon
*					the CREG setting of the individual V24 channels.
*
*	Note:			When "cell_ind" is TRUE, CREG status is sent out only when the
*					CREG setting of the invididual V24 channels is set to 2.
*
*************************************************************************************/
void AtOutputCregStatus(MSRegState_t state, Boolean cell_ind, LACode_t lac, CellInfo_t cell_info)
{
	AT_ChannelCfg_t	*channelCfg;
	const UInt8	*cmd_name = AT_GetCmdName(AT_CMD_CREG);
	UInt8 dlcCount, begDLCCount, maxDLCCount;
	char output[50];
	V24OperMode_t	operMode ;
	UInt8 muxMode = MPX_ObtainMuxMode();

	if (muxMode == NONMUX_MODE)
	{
		begDLCCount = 0;
		maxDLCCount = 1;
	}
	else
	{
		begDLCCount = 1;
		maxDLCCount = ATDLCCOUNT; // 4, ch 0-3 is the normal ch, ch 4 is DLC_STKTERMINAL
	}

	for (dlcCount = begDLCCount; dlcCount < maxDLCCount; dlcCount++)
	{
		operMode = V24_GetOperationMode( (DLC_t)dlcCount ) ;
		channelCfg = AT_GetChannelCfg(dlcCount);

		if( (operMode == V24OPERMODE_AT_CMD) || (operMode == V24OPERMODE_NO_RECEIVE) ) {
			if(channelCfg->CREG == 2)
			{
				if( (state == REG_STATE_NORMAL_SERVICE) || (state == REG_STATE_ROAMING_SERVICE) )
				{
					sprintf(output, "%s: %d,\"%04X\",\"%04X\"", cmd_name, state, lac, cell_info);
				}
				else
				{
					sprintf(output, "%s: %d,\"\",\"\"", cmd_name, state);
				}

				AT_OutputStr(dlcCount, (const UInt8 *) output);
			}
			else if( (channelCfg->CREG == 1) && (!cell_ind) )
			{
				if (atGsmRegStatusInfo != state) {
					sprintf(output, "%s: %d", cmd_name, state);
					AT_OutputStr(dlcCount, (const UInt8 *) output);
				}
			}
		}
	}

	atGsmRegStatusInfo = state;
}


/************************************************************************************
*
*	Function Name:	AtOutputCgregStatus
*
*	Description:	This function sends out the unsolicited CGREG status based upon
*					the CGREG setting of the individual V24 channels.
*
*	Note:			When "cell_ind" is TRUE, CREG status is sent out only when the
*					CREG setting of the invididual V24 channels is set to 2.
*
*************************************************************************************/
void AtOutputCgregStatus(MSRegState_t state, Boolean cell_ind, LACode_t lac, CellInfo_t cell_info)
{
	AT_ChannelCfg_t	*channelCfg;
	const UInt8	*cmd_name = AT_GetCmdName(AT_CMD_CGREG);
	UInt8 dlcCount, begDLCCount, maxDLCCount;
	char output[50];
	V24OperMode_t	operMode ;
	UInt8 muxMode = MPX_ObtainMuxMode();

	if (muxMode == NONMUX_MODE)
	{
		begDLCCount = 0;
		maxDLCCount = 1;
	}
	else
	{
		begDLCCount = 1;
		maxDLCCount = ATDLCCOUNT; // 4, ch 0-3 is the normal ch, ch 4 is DLC_STKTERMINAL
	}

	for (dlcCount = begDLCCount; dlcCount < maxDLCCount; dlcCount++)
	{
		operMode = V24_GetOperationMode( (DLC_t)dlcCount ) ;
		channelCfg = AT_GetChannelCfg(dlcCount);

		if( (operMode == V24OPERMODE_AT_CMD) || (operMode == V24OPERMODE_NO_RECEIVE) ) {

			if(channelCfg->CGREG == 2)
			{
				if( (state == REG_STATE_NORMAL_SERVICE) || (state == REG_STATE_ROAMING_SERVICE) )
				{
					sprintf(output, "%s: %d,\"%04X\",\"%04X\"", cmd_name, state, lac, cell_info);
				}
				else
				{
					sprintf(output, "%s: %d,\"\",\"\"", cmd_name, state);
				}

				AT_OutputStr(dlcCount, (const UInt8 *) output);
			}
			else if( (channelCfg->CGREG == 1) && (!cell_ind) )
			{
				if (atGprsRegStatusInfo != state) {
					sprintf(output, "%s: %d", cmd_name, state);
					AT_OutputStr(dlcCount, (const UInt8 *) output);
				}
			}
		}
	}

	atGprsRegStatusInfo = state;
}

/************************************************************************************
*	Function Name:	AtOutputNetworkInfo
*
*	Description:	This function sends out the unsolicited *MNINFO status based upon
*					the network information from stack.
*					*MRINFO:<GSM RINFO>,<UMTS RINFO>
*					<GSM RINFO> - GSM/GPRS/EDGE Radio Technology Information
*						0: No GPRS or EDGE available
*						1: GPRS service available
*						2: EDGE service available
*						3: Both GPRS and EDGE service available
*					<UMTS RINFO> - UMTS Radio Technology Information
*						0: No UMTS service available
*						1: UMTS CS service available
*						2: <not used>
*						3: UMTS CS and PS service available
*
*************************************************************************************/
#define	AT_NINFO_GPRS_BIT			0x01
#define	AT_NINFO_EGPRS_BIT			0x02

#define	AT_NINFO_UMTS_BIT			0x01
#define	AT_NINFO_UMTS_PS_BIT		0x02

void AtOutputNetworkInfo(MSNetworkInfo_t netInfo)
{
	char						output[50];
	AT_CommonCfg_t*				atCfg = AT_GetCommonCfg();
	UInt8						gsmNetState=0;
	UInt8						umtsNetState=0;

	if (netInfo.gprs_supported == SUPPORTED) {
		if (MS_GetCurrentRAT() == RAT_GSM)
			gsmNetState	|=	AT_NINFO_GPRS_BIT;
		else if (MS_GetCurrentRAT() == RAT_UMTS)
			umtsNetState |= AT_NINFO_UMTS_PS_BIT;
	}
	if (MS_GetCurrentRAT() == RAT_GSM && netInfo.egprs_supported == SUPPORTED) {
		gsmNetState	|=	AT_NINFO_EGPRS_BIT;
	}
	if (MS_GetCurrentRAT() == RAT_UMTS) {
		umtsNetState |= AT_NINFO_UMTS_BIT;
	}

	if (atGsmRadioInfoState != gsmNetState || atUmtsRadioInfoState!=umtsNetState) {
		atGsmRadioInfoState = gsmNetState;
		atUmtsRadioInfoState = umtsNetState;
		if (atCfg->MRINFO) {
			sprintf( output, "*MRINFO: %d, %d", gsmNetState, umtsNetState);
			AT_OutputUnsolicitedStr((UInt8*) output );
		}
	}


}
