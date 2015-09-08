/**
*
*   @file   at_pbk.c
*
*   @brief  This file contains the AT phone book handling functions.
*
****************************************************************************/
#define ENABLE_LOGGING

#include "stdlib.h"
#include "phonebk.h"
#include "mstypes.h"
#include "at_util.h"
#include "at_data.h"
#include "at_cfg.h"
#include "log.h"
#include "sim_api.h"
#include "mti_build.h"
#include "ucs2.h"
#include "phonebk_api.h"
#include "osheap.h"
//#include "log.h"
#include "util.h"

/******************************************************************************
								Macros and Definitions
*******************************************************************************/
/* Number type definition for AT*ESNU and AT*MSNU commands. */
#define ESNU_MSISDN_TYPE_VOICE_L1	0
#define ESNU_MSISDN_TYPE_VOICE_L2	1
#define ESNU_MSISDN_TYPE_FAX		2
#define ESNU_MSISDN_TYPE_DATA		3


/* Maximum number of phonebook entries we can read at one time due to the
 * limited size of the ATC message queues. This is because the phonebook
 * task posts a message to the requestor's task queue to return the data
 * of a phonebook entry.
 */
#define ATC_MAX_PBK_READ_SIZE (QUEUESIZE_ATC > 6 ? QUEUESIZE_ATC - 4 : 1)


/* The number of free bytes required before we decide to put phonebook data
 * into the output buffer of a multiplex channel. Note that we need to take
 * into account the multiplex channel message overhead in addition to the
 * phonebook data and AT command data. 200 is more than enough.
 */
#define REQ_FREE_BUFFER_SIZE 200


/******************************************************************************
								Globals Datas
*******************************************************************************/
static const struct phbk
{
	PBK_Id_t	Id;
	char		Name[3];
}
phonebk[] =
{
   {PB_EN,	"EN"},
   {PB_FDN,	"FD"},
   {PB_ADN,	"SM"},
   {PB_SDN,	"SD"},
   {PB_LND, "LD"}
};

#define ATC_NB_PBK    (sizeof(phonebk) / sizeof(struct phbk))

#define INVALID_HOST_PBK_INDEX 0xFFFF

/* These are the first and last phonebook indices requested in AT+CPBR command */
static UInt16 Host_Req_Pbk_Start_Index;
static UInt16 Host_Req_Pbk_End_Index;

/* This is the last phonebook index of the current read session (there may be multiple
 * read sessions for a AT+CPBR command due to the limited size of the ATC message queues.
 */
static UInt16 Curr_Req_Pbk_End_Index;


extern const UInt8* atc_GetTeToGsmConvTbl(void);


//******************************************************************************
//								Static Functions
//*****************************************************************************/
static const char* ATC_GetPbkName(PBK_Id_t pbk_id)
{
	UInt8 i;
	const char *j = "";

	for(i = 0; i < ATC_NB_PBK; i++)
	{
		if(phonebk[i].Id == pbk_id)
		{
			return phonebk[i].Name;
		}
	}

	return j;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CPBF_Handler
*
*	Description:	This function handles the AT+CPBF command (find phonebook entries
*                   that match a particular alpha pattern).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CPBF_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	AT_ChannelCfg_t* atc_config_ptr = AT_GetChannelCfg(chan);
	AT_Status_t status = AT_STATUS_DONE;

	if( (accessType == AT_ACCESS_READ) ||
		((atc_config_ptr->CPBS != PB_ME) && (atc_config_ptr->CPBS != PB_ADN) && (atc_config_ptr->CPBS != PB_FDN)) )
	{
		/* There is no alpha text for SIM Emergency Call Code and AT+CPBF? not valid. We do not support
		 * alpha finding for SDN phonebook. So operation is not relevant and not supported.
		 */
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
	}
	else if( !PBK_IsReady() )
	{
		AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY);
	}
	else if(accessType == AT_ACCESS_TEST)
	{
		PBK_SendInfoReq(AT_GetV24ClientIDFromMpxChan(chan), atc_config_ptr->CPBS, PostMsgToATCTask);

		status = AT_STATUS_PENDING;
	}
	else /* Must be AT_ACCESS_REGULAR */
	{
		if( (_P1 == NULL) || (_P1[0] == '\0') )
		{
			AT_CmdRspError(chan, AT_CME_ERR_NOT_FOUND);
		}
		else
		{
			UInt16 alpha_size;
			UInt8 alpha_data[60];
			ALPHA_CODING_t alpha_coding;

			if( UCS2_ConvertToAlphaData( ATC_IsCharacterSetUcs2(), (char *) _P1, &alpha_coding, alpha_data, &alpha_size ) )
			{
				PBK_SendFindAlphaMatchMultipleReq( AT_GetV24ClientIDFromMpxChan(chan), atc_config_ptr->CPBS,
						alpha_coding, alpha_size, alpha_data, PostMsgToATCTask );

				status = AT_STATUS_PENDING;
			}
			else
			{
				AT_CmdRspError(chan, AT_CME_ERR_INVALID_CHAR_INSTR);
			}
		}
	}

	return status;
}


/************************************************************************************
*
*	Function Name:	AtHandlePbkInfoResp
*
*	Description:	This function processes the received MSG_GET_PBK_INFO_RSP
*					response in the ATC task.
*
*	Notes:
*
*************************************************************************************/
void AtHandlePbkInfoResp(InterTaskMsg_t* inMsg)
{
	PBK_INFO_RSP_t *pbk_info_rsp = (PBK_INFO_RSP_t *) inMsg->dataBuf;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	char *cmd_ptr = (char *) AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num));
	char temp_buffer[40];
	Boolean send_rsp = TRUE;

	if(!pbk_info_rsp->result)
	{
		AT_CmdRspError(mpx_chan_num, AT_CME_ERR_SIM_BUSY);
		return;
	}

	switch(AT_GetCmdIdByMpxChan(mpx_chan_num))
	{
	case AT_CMD_CPBF:
		sprintf( temp_buffer, "%s: (%d),(%d)", cmd_ptr,
				 pbk_info_rsp->mx_digit_size, pbk_info_rsp->mx_alpha_size );

		break;

	case AT_CMD_CPBR:
		if( (Host_Req_Pbk_Start_Index == INVALID_HOST_PBK_INDEX) &&
			(Host_Req_Pbk_End_Index == INVALID_HOST_PBK_INDEX) )
		{
			/* We are processing AT+CPBR? command */
			if(pbk_info_rsp->total_entries != 0)
			{
				sprintf( temp_buffer, "%s: (1-%d),%d,%d", cmd_ptr,
					 pbk_info_rsp->total_entries, pbk_info_rsp->mx_digit_size, pbk_info_rsp->mx_alpha_size );
			}
			else
			{
				AT_CmdRspError(mpx_chan_num, AT_CME_ERR_NOT_FOUND);
				send_rsp = FALSE;
			}
		}
		else
		{
			/* We are processing AT+CPBR=<x> command */
			if( (Host_Req_Pbk_Start_Index >= pbk_info_rsp->total_entries) ||
				(Host_Req_Pbk_End_Index >= pbk_info_rsp->total_entries) )
			{
				AT_CmdRspError(mpx_chan_num, AT_CME_ERR_INVALID_INDEX);
			}
			else
			{
				/* Limit the number of phonebook entries to read */
				if( (Host_Req_Pbk_End_Index - Host_Req_Pbk_Start_Index + 1) > ATC_MAX_PBK_READ_SIZE)
				{
					Curr_Req_Pbk_End_Index = Host_Req_Pbk_Start_Index + ATC_MAX_PBK_READ_SIZE - 1;
				}
				else
				{
					Curr_Req_Pbk_End_Index = Host_Req_Pbk_End_Index;
				}

				PBK_SendReadEntryReq( inMsg->clientID, pbk_info_rsp->pbk_id,
							   Host_Req_Pbk_Start_Index, Curr_Req_Pbk_End_Index, PostMsgToATCTask );
			}

			send_rsp = FALSE;
		}

		break;

	case AT_CMD_CPBW:
		if(pbk_info_rsp->total_entries != 0)
		{
			sprintf( temp_buffer, "%s: (1-%d),%d,(%d,%d,%d),%d", cmd_ptr,
				pbk_info_rsp->total_entries, pbk_info_rsp->mx_digit_size,
				UNKNOWN_TON_UNKNOWN_NPI, UNKNOWN_TON_ISDN_NPI, INTERNA_TON_ISDN_NPI, pbk_info_rsp->mx_alpha_size );
		}
		else
		{
			AT_CmdRspError(mpx_chan_num, AT_CME_ERR_NOT_FOUND);
			send_rsp = FALSE;
		}

		break;

	case AT_CMD_CPBS:
		sprintf( temp_buffer, "%s: \"%s\",%d,%d", cmd_ptr, ATC_GetPbkName(pbk_info_rsp->pbk_id),
				 pbk_info_rsp->total_entries - pbk_info_rsp->free_entries, pbk_info_rsp->total_entries );

		break;

	case AT_CMD_ESNU:
	{
		Boolean voice_l2_supported = SIM_IsALSEnabled();

		switch(pbk_info_rsp->total_entries)
		{
		case 0:
			AT_CmdRspError(mpx_chan_num, AT_CME_ERR_NOT_FOUND);
			return;

		case 1:
			sprintf(temp_buffer, "%s: (%d)", cmd_ptr, ESNU_MSISDN_TYPE_VOICE_L1);
			break;

		case 2:
			if(voice_l2_supported)
			{
				/* Location 1 stores Voice L2 number: according to R520 reference phone */
				sprintf(temp_buffer, "%s: (%d,%d)", cmd_ptr, ESNU_MSISDN_TYPE_VOICE_L1, ESNU_MSISDN_TYPE_VOICE_L2);
			}
			else
			{
				/* Voice L2 number is not supported in the SIM.
				 * Location 1 stores Fax Number: according to R520 reference phone.
				 */
				sprintf(temp_buffer, "%s: (%d,%d)", cmd_ptr, ESNU_MSISDN_TYPE_VOICE_L1, ESNU_MSISDN_TYPE_FAX);
			}
			break;

		case 3:
			if(voice_l2_supported)
			{
				/* Data number stored in location 2, Fax number not supported:
				 * according to R520 reference phone.
				 */
				sprintf( temp_buffer, "%s: (%d,%d,%d)", cmd_ptr, ESNU_MSISDN_TYPE_VOICE_L1,
						 ESNU_MSISDN_TYPE_VOICE_L2, ESNU_MSISDN_TYPE_DATA );
			}
			else
			{
				/* Data number stored in location 2, Fax number stored in location 1:
				 * according to R520 reference phone.
				 */
				sprintf( temp_buffer, "%s: (%d,%d,%d)", cmd_ptr, ESNU_MSISDN_TYPE_VOICE_L1,
						 ESNU_MSISDN_TYPE_FAX, ESNU_MSISDN_TYPE_DATA );
			}
			break;

		case 4:
		default:
			/* Data number stored in location 2, Fax number stored in location 3:
			 * according to R520 reference phone.
			 */
			if(voice_l2_supported)
			{
				sprintf( temp_buffer, "%s: (%d,%d,%d,%d)", cmd_ptr, ESNU_MSISDN_TYPE_VOICE_L1,
						 ESNU_MSISDN_TYPE_VOICE_L2, ESNU_MSISDN_TYPE_FAX, ESNU_MSISDN_TYPE_DATA );
			}
			else
			{
				sprintf( temp_buffer, "%s: (%d,%d,%d)", cmd_ptr, ESNU_MSISDN_TYPE_VOICE_L1,
						 ESNU_MSISDN_TYPE_FAX, ESNU_MSISDN_TYPE_DATA );
			}
			break;
		}
		break;
	}

	default:
		send_rsp = FALSE;

		break;
	}

	if(send_rsp)
	{
		AT_OutputStr( mpx_chan_num, (UInt8*)temp_buffer ) ;
		AT_CmdRspOK(mpx_chan_num);
	}
}


/************************************************************************************
*
*	Function Name:	AtHandlePbkEntryDataResp
*
*	Description:	This function processes the received MSG_PBK_ENTRY_DATA_RSP
*					response in the ATC task.
*
*	Notes:
*
*************************************************************************************/
void AtHandlePbkEntryDataResp(InterTaskMsg_t* inMsg)
{
	PBK_ENTRY_DATA_RSP_t *entry_data = (PBK_ENTRY_DATA_RSP_t *) inMsg->dataBuf;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	AT_CmdId_t at_cmd = AT_GetCmdIdByMpxChan(mpx_chan_num);
	char *cmd_ptr = (char *) AT_GetCmdName(at_cmd);
	char temp_buffer[200];
	UInt8 i;

	switch(at_cmd)
	{
	case AT_CMD_CPBF:
	case AT_CMD_CPBR:
	case AT_CMD_CNUM:
	case AT_CMD_ESNU:
		switch(entry_data->data_result)
		{
		case PBK_ENTRY_NOT_ACCESSIBLE:
			AT_CmdRspOK(mpx_chan_num);
			break;

		case PBK_SIM_BUSY:
			AT_CmdRspError(mpx_chan_num, AT_CME_ERR_SIM_BUSY);
			break;

		case PBK_ENTRY_INDEX_INVALID:
			AT_CmdRspError(mpx_chan_num, AT_CME_ERR_INVALID_INDEX);
			break;

		case PBK_ENTRY_INVALID_IS_LAST:
			if( (at_cmd == AT_CMD_CPBR) && (Curr_Req_Pbk_End_Index < Host_Req_Pbk_End_Index) )
			{
				goto NEXT_PBK_READ_SESSION;
			}

			/* The last record data has been output, now send OK out */
			AT_CmdRspOK(mpx_chan_num);
			break;

		case PBK_ENTRY_VALID_NOT_LAST:
		case PBK_ENTRY_VALID_IS_LAST:
			entry_data->pbk_rec.ton = 0x80 | (entry_data->pbk_rec.ton << 4) | entry_data->pbk_rec.npi;

			/* Make sure the type of number sent out for AT is 128, 129 or 145. */
			if( (entry_data->pbk_rec.ton != INTERNA_TON_ISDN_NPI) && (entry_data->pbk_rec.ton != UNKNOWN_TON_ISDN_NPI) &&
				(entry_data->pbk_rec.ton != UNKNOWN_TON_UNKNOWN_NPI) )
			{
				entry_data->pbk_rec.ton =
					(entry_data->pbk_rec.number[0] == INTERNATIONAL_CODE) ? INTERNA_TON_ISDN_NPI : UNKNOWN_TON_ISDN_NPI;
			}

			if( (at_cmd == AT_CMD_CPBF) || (at_cmd == AT_CMD_CPBR) )
			{
				char *name_buffer = UCS2_ConvertToAlphaStr( ATC_IsCharacterSetUcs2(), entry_data->pbk_rec.alpha_data.alpha_coding,
					entry_data->pbk_rec.alpha_data.alpha, entry_data->pbk_rec.alpha_data.alpha_size );

				sprintf( temp_buffer, "%s: %d,\"%s\",%d,\"%s\"", cmd_ptr,
					entry_data->pbk_rec.location + 1, entry_data->pbk_rec.number,
					entry_data->pbk_rec.ton,
					(name_buffer != NULL ? name_buffer : "")  );

				if(name_buffer != NULL)
				{
					OSHEAP_Delete(name_buffer);
				}
			}
			else if(at_cmd == AT_CMD_CNUM)
			{
				char *name_buffer = UCS2_ConvertToAlphaStr( ATC_IsCharacterSetUcs2(), entry_data->pbk_rec.alpha_data.alpha_coding,
					entry_data->pbk_rec.alpha_data.alpha, entry_data->pbk_rec.alpha_data.alpha_size );

				sprintf( temp_buffer, "%s: \"%s\",\"%s\",%d", cmd_ptr,
					(name_buffer != NULL ? name_buffer : ""),
					entry_data->pbk_rec.number, entry_data->pbk_rec.ton );

				if(name_buffer != NULL)
				{
					OSHEAP_Delete(name_buffer);
				}
			}
			else if(at_cmd == AT_CMD_ESNU)
			{
				UInt8 number_type;
				Boolean voice_l2_supported = SIM_IsALSEnabled();

				/* MSISDN numbers are stored in location 0-3. If Voice L2 is not supported and
				 * there are only two or three storage locations, then location 1 is used to store
				 * Fax number.
				 */
				if( (entry_data->pbk_rec.location < 4) &&
					((entry_data->pbk_rec.location != 1) || voice_l2_supported ||
					 (entry_data->total_entries == 2) || (entry_data->total_entries == 3)) )
				{
					switch(entry_data->pbk_rec.location)
					{
					case 0:	/* Voice L1	*/
						number_type = ESNU_MSISDN_TYPE_VOICE_L1;
						break;

					case 1: /* If Voice L2 is not supported and
							 * there are only two or three storage locations, then location 1
							 * is used to store Fax number.
							 */
						number_type = voice_l2_supported ? ESNU_MSISDN_TYPE_VOICE_L2 : ESNU_MSISDN_TYPE_FAX;
						break;

					case 2: /* Data number */
						number_type = ESNU_MSISDN_TYPE_DATA;
						break;

					case 3:
						number_type = ESNU_MSISDN_TYPE_FAX;
						break;
					}

					sprintf( temp_buffer, "%s: %d,\"%s\",%d", cmd_ptr,
						number_type, entry_data->pbk_rec.number, entry_data->pbk_rec.ton );
				}
				else
				{
					/* Do not send the invalid entry data */
					temp_buffer[0] = '\0';
				}
			}

			/* Busy wait for the SIO buffer to have at least required free bytes. If we do not
			 * do the busy wait here, it will overflow the SIO buffer in a large phonebook
			 * read, such as for the AT+CPBR=1,250 command.
			 */
			while( V24_GetTxBufferFree(mpx_chan_num) <= REQ_FREE_BUFFER_SIZE )
			{
				OSTASK_Sleep(TICKS_ONE_SECOND / 50);
			}

			AT_OutputStr( mpx_chan_num, (UInt8*) temp_buffer ) ;

			if(entry_data->data_result == PBK_ENTRY_VALID_IS_LAST)
			{
				if( (at_cmd == AT_CMD_CPBR) && (Curr_Req_Pbk_End_Index < Host_Req_Pbk_End_Index) )
				{
					goto NEXT_PBK_READ_SESSION;
				}

				/* The last record data has been output, now send OK out */
				AT_CmdRspOK(mpx_chan_num);
			}

			break;

		default:
			assert(FALSE);
			break;
		}

		break;

	default:
		assert(FALSE);
		break;
	}

	return;

	/* The following begins another session to read phonebook entries.
	 * There may be multiple read sessions for a AT+CPBR command due to the
	 * limited size of the ATC message queues.
	 */
NEXT_PBK_READ_SESSION:
	i = Curr_Req_Pbk_End_Index;

	/* Limit the number of phonebook entries to read */
	if( (Host_Req_Pbk_End_Index - Curr_Req_Pbk_End_Index) > ATC_MAX_PBK_READ_SIZE)
	{
		Curr_Req_Pbk_End_Index = Curr_Req_Pbk_End_Index + ATC_MAX_PBK_READ_SIZE;
	}
	else
	{
		Curr_Req_Pbk_End_Index = Host_Req_Pbk_End_Index;
	}

	PBK_SendReadEntryReq( inMsg->clientID, AT_GetChannelCfg(mpx_chan_num)->CPBS,
						   i + 1, Curr_Req_Pbk_End_Index, PostMsgToATCTask );
}


/************************************************************************************
*
*	Function Name:	AtHandleWritePbkResp
*
*	Description:	This function processes the received MSG_WRT_PBK_ENTRY_RSP
*					message in the ATC task.
*
*	Notes:
*
*************************************************************************************/
void AtHandleWritePbkResp(InterTaskMsg_t* inMsg)
{
	PBK_WRITE_ENTRY_RSP_t *write_rsp = (PBK_WRITE_ENTRY_RSP_t *) inMsg->dataBuf;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	AT_CmdId_t at_cmd = AT_GetCmdIdByMpxChan(mpx_chan_num);

	if( (at_cmd == AT_CMD_CPBW) || (at_cmd == AT_CMD_ESNU) )
	{
		Boolean send_error = TRUE;
		UInt16 error_code;

		/* Response for AT+CPBW or AT*ESNU (or AT*MSNU) command to write phonebook entry */
		switch(write_rsp->write_result)
		{
		case PBK_WRITE_SUCCESS:
			AT_CmdRspOK(mpx_chan_num);
			send_error = FALSE;
			break;

		case PBK_WRITE_FDN_PIN2_REQUIRED:
			error_code = AT_CME_ERR_SIM_PIN2_REQD;
			break;

		case PBK_WRITE_REJECTED:
			error_code = AT_CME_ERR_OP_NOT_ALLOWED;
			break;

		case PBK_WRITE_INDEX_INVALID:
			error_code = AT_CME_ERR_INVALID_INDEX;
			break;

		case PBK_WRITE_DATA_INVALID:
			error_code = AT_CME_ERR_INVALID_CHAR_INSTR;
			break;

		case PBK_WRITE_NUMBER_TOO_LONG:
			error_code = AT_CME_ERR_DIAL_STR_TOO_LONG;
			break;

		default:
			error_code = AT_CME_ERR_UNKNOWN;
			break;
		}

		if(send_error)
		{
			AT_CmdRspError(mpx_chan_num, error_code);
		}
	}
}


/************************************************************************************
*
*	Function Name:	AtHandlePbkReadyInd
*
*	Description:	This function processes the received MSG_PBK_READY_IND
*					unsolicited command in the ATC task.
*
*	Notes:
*
*************************************************************************************/
void AtHandlePbkReadyInd(void)
{
	// send unsolicited event
	ATC_ReportMrdy(MODULE_ALL_AT_READY_READY);
}


/************************************************************************************
*
*	Function Name:	ATCmd_CPBR_Handler
*
*	Description:	This function handles the AT+CPBR command (read phonebook entries).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CPBR_Handler( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								const UInt8* _P1, const UInt8* _P2 )
{
	AT_Status_t status = AT_STATUS_DONE;
	AT_ChannelCfg_t *atc_config_ptr = AT_GetChannelCfg(chan);

	if(accessType == AT_ACCESS_READ)
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
	}
	else if( (SIM_GetPresentStatus() != SIMPRESENT_INSERTED) && (atc_config_ptr->CPBS != PB_EN) )
	{
		/* When SIM is not present, we only allow the reading of ECC number */
		AT_CmdRspError(chan, AT_CME_ERR_PH_SIM_NOT_INSERTED);
	}
	else if( !SIM_IsPinOK() && (atc_config_ptr->CPBS != PB_EN) )
	{
		/* When SIM PIN not verified, we only allow the reading of ECC number */
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
	}
	else if( !PBK_IsReady() && (atc_config_ptr->CPBS != PB_EN) )
	{
		AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY);
	}
	else if(accessType == AT_ACCESS_TEST)
	{
		Host_Req_Pbk_Start_Index = INVALID_HOST_PBK_INDEX;
		Host_Req_Pbk_End_Index = INVALID_HOST_PBK_INDEX;

		PBK_SendInfoReq(AT_GetV24ClientIDFromMpxChan(chan), atc_config_ptr->CPBS, PostMsgToATCTask);

		status = AT_STATUS_PENDING;
	}
	else /* Must be AT_ACCESS_REGULAR */
	{
		Host_Req_Pbk_Start_Index = ((_P1 == NULL) || (_P1[0] == '\0')) ? 0 : atoi((char *) _P1);
		Host_Req_Pbk_End_Index = ((_P2 == NULL) || (_P2[0] == '\0')) ? Host_Req_Pbk_Start_Index : atoi((char *) _P2);

		if( (Host_Req_Pbk_Start_Index == 0) || (Host_Req_Pbk_End_Index == 0) || (Host_Req_Pbk_Start_Index > Host_Req_Pbk_End_Index) )
		{
			AT_CmdRspError(chan, AT_CME_ERR_INVALID_INDEX);
		}
		else
		{
			/* Indices are 0 based */
			Host_Req_Pbk_Start_Index--;
			Host_Req_Pbk_End_Index--;

			/* First query the phonebook information so that we can verify the validity of the
			 * passed indices.
			 */
			PBK_SendInfoReq(AT_GetV24ClientIDFromMpxChan(chan), atc_config_ptr->CPBS, PostMsgToATCTask);

			status = AT_STATUS_PENDING;
		}
	}

	return status;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CNUM_Handler
*
*	Description:	This function handles the AT+CNUM command (read MSISDN phonebook entries).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CNUM_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	AT_Status_t status = AT_STATUS_DONE;

	if(accessType == AT_ACCESS_READ)
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
	}
	else if(accessType == AT_ACCESS_TEST)
	{
		AT_CmdRspOK(chan);
	}
	else if( !PBK_IsReady() )
	{
		AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY);
	}
	else
	{
		/* Read all entries in MSISDN phonebooks:
		 * tart_index = 0; end_index = LAST_PHONEBK_INDEX.
		 */
		PBK_SendReadEntryReq( AT_GetV24ClientIDFromMpxChan(chan), PB_MSISDN,
						   0, LAST_PHONEBK_INDEX, PostMsgToATCTask );

		status = AT_STATUS_PENDING;
	}

	return status;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CPBS_Handler
*
*	Description:	This function handles the AT+CPBS command (select phonebook storage).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CPBS_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	AT_Status_t status = AT_STATUS_DONE;
	char temp_buffer[40];
	UInt8 i;
	AT_ChannelCfg_t *atc_config_ptr = AT_GetChannelCfg(chan);

	if(accessType == AT_ACCESS_REGULAR)
	{
		for(i = 0; i < ATC_NB_PBK; i++)
		{
			if( !strcmp((char *) _P1, phonebk[i].Name) )
			{
				break;
			}
		}

		if(i >= ATC_NB_PBK)
		{
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
		}
		else
		{
			atc_config_ptr->CPBS = phonebk[i].Id;
			AT_CmdRspOK(chan);
		}
	}
	else if(accessType == AT_ACCESS_READ)
	{
		if ( (SIM_GetPresentStatus() != SIMPRESENT_INSERTED) && (atc_config_ptr->CPBS != PB_EN) )
		{
			/* When SIM is not present, we only allow the reading of ECC number */
			AT_CmdRspError(chan, AT_CME_ERR_PH_SIM_NOT_INSERTED);
		}
		else if ( !SIM_IsPinOK() && (atc_config_ptr->CPBS != PB_EN) )
		{
			/* When SIM PIN not verified, we only allow the reading of ECC number */
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		}
		else if( !PBK_IsReady() && (atc_config_ptr->CPBS != PB_EN) )
		{
			AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY);
		}
		else
		{
			PBK_SendInfoReq(AT_GetV24ClientIDFromMpxChan(chan), atc_config_ptr->CPBS, PostMsgToATCTask);

			status = AT_STATUS_PENDING;
		}
	}
	else
	{
		sprintf(temp_buffer, "%s: (\"EN\",\"FD\",\"SM\",\"SD\",\"LD\")", (char *) AT_GetCmdName(cmdId));
		AT_OutputStr( chan, (UInt8*)temp_buffer ) ;

		AT_CmdRspOK(chan);
	}

	return status;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CPBW_Handler
*
*	Description:	This function handles the AT+CPBW command (Write phonebook entry).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CPBW_Handler( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
						const UInt8* _P1, const UInt8* _P2, const UInt8* _P3, const UInt8* _P4 )
{
	UInt8 num_offset;
	UInt8 theType = UNKNOWN_TON_ISDN_NPI;
	AT_Status_t status = AT_STATUS_DONE;
	AT_ChannelCfg_t *atc_config_ptr = AT_GetChannelCfg(chan);

	if( (atc_config_ptr->CPBS == PB_EN) || (atc_config_ptr->CPBS == PB_SDN) )
	{
		/* Emergency Call numbers and Service Dialling numbers can not be updated */
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
	}
	else if(accessType == AT_ACCESS_READ)
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
	}
	else if( !PBK_IsReady() )
	{
		AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY);
	}
	else if(accessType == AT_ACCESS_TEST)
	{
		PBK_SendInfoReq(AT_GetV24ClientIDFromMpxChan(chan), atc_config_ptr->CPBS, PostMsgToATCTask);

		status = AT_STATUS_PENDING;
	}
	else /* Must be AT_ACCESS_REGULAR */
	{
		if( ((_P1 == NULL) || (_P1[0] == '\0')) &&  ((_P2 == NULL) || (_P2[0] == '\0')) )
		{
			/* Phonebook entry index must be present for removing an entry */
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		}
		else
		{
			/* If index not present, we write to the first available entry slot. Also entry is
			 * 1 based in AT+CPBW, change it to 0 based.
			 */
			UInt16 index = ((_P1 == NULL) || (_P1[0] == '\0')) ? FIRST_PHONEBK_FREE_INDEX : atoi((char *) _P1) - 1;
			UInt16 alpha_size;
			UInt16 i;
			UInt8 alpha_data[60];
			ALPHA_CODING_t alpha_coding;

			if( UCS2_ConvertToAlphaData( ATC_IsCharacterSetUcs2(), (char *) _P4, &alpha_coding, alpha_data, &alpha_size ) )
			{
				/* Convert to GSM alphabet depending on the selected character set */
				if(alpha_coding == ALPHA_CODING_GSM_ALPHABET)
				{
					const UInt8* convert_table = atc_GetTeToGsmConvTbl();

					/* Convert to GSM alphabet format if the current character set is
					 * not set to GSM alphabet.
					 */
					if(convert_table != NULL)
					{
						for(i = 0; i < alpha_size; i++)
						{
							alpha_data[i] = convert_table[alpha_data[i]];
						}
					}
				}

				// GSM 07.07 section 8.14
				// <type>: type of address octet in integer format
				// (refer GSM 04.08 [8] subclause 10.5.4.7);
				// default 145 when dialling string includes international
				// access code character "+", otherwise 129
				if (_P3 && (_P3[0] != '\0'))
				{
					theType = atoi((char*)_P3);

					switch (theType)
					{
						case UNKNOWN_TON_ISDN_NPI:
						case INTERNA_TON_ISDN_NPI:
						case UNKNOWN_TON_UNKNOWN_NPI:
							break;

						default:
							//MSG_LOG("Wrong Type for CPBW");
							AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
							return(status);
					}
				}

				PBK_SendWriteEntryReq(	AT_GetV24ClientIDFromMpxChan(chan),
										atc_config_ptr->CPBS,
										FALSE,
										index,
										theType,
										(char *) _P2,
										alpha_coding,
										alpha_size,
										alpha_data,
										PostMsgToATCTask );

				status = AT_STATUS_PENDING;
			}
			else
			{
				AT_CmdRspError(chan, AT_CME_ERR_INVALID_CHAR_INSTR);
			}
		}
	}

	return status;
}


/************************************************************************************
*
*	Function Name:	ATCmd_ESNU_Handler
*
*	Description:	This function handles the AT*ESNU (or AT*MSNU) command
*					(Read/Write MSISDN phonebook entry).
*
*
*************************************************************************************/
AT_Status_t ATCmd_ESNU_Handler( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								const UInt8* _P1, const UInt8* _P2, const UInt8* _P3 )
{
	AT_Status_t status = AT_STATUS_DONE;

	if( !PBK_IsReady() )
	{
		AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY);
	}
	else if(accessType == AT_ACCESS_READ)
	{
		/* Read all entries in MSISDN phonebooks:
		 * start_index = 0; end_index = LAST_PHONEBK_INDEX.
		 */
		PBK_SendReadEntryReq( AT_GetV24ClientIDFromMpxChan(chan), PB_MSISDN,
						   0, LAST_PHONEBK_INDEX, PostMsgToATCTask );

		status = AT_STATUS_PENDING;
	}
	else if(accessType == AT_ACCESS_TEST)
	{
		PBK_SendInfoReq(AT_GetV24ClientIDFromMpxChan(chan), PB_MSISDN, PostMsgToATCTask);
		status = AT_STATUS_PENDING;
	}
	else /* Must be AT_ACCESS_REGULAR */
	{
		UInt16 index;
		const char *alpha_ptr;
		Boolean	voice_l2_supported = SIM_IsALSEnabled();

		const char voice_l1_alpha[] = "Voice L1";
		const char voice_l2_alpha[] = "Voice L2";
		const char fax_alpha[] = "Fax";
		const char data_alpha[] = "Data";

		if( (_P2 != NULL) && (strlen((char *) _P2) > SIM_PBK_ASCII_DIGIT_SZ) )
		{
			/* Number is too long */
			AT_CmdRspError(chan, AT_CME_ERR_INVALID_CHAR_INSTR);
			return AT_STATUS_DONE;
		}

		switch( atoi((char *) _P1) )
		{
		case ESNU_MSISDN_TYPE_VOICE_L1:
			index = 0;
			alpha_ptr = voice_l1_alpha;

			break;

		case ESNU_MSISDN_TYPE_VOICE_L2:
			if(voice_l2_supported)
			{
				index = 1;
				alpha_ptr = voice_l2_alpha;
			}
			else
			{
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			break;

		case ESNU_MSISDN_TYPE_FAX:
			/* R520 reference phone stores Fax number in Location 3. If location 3 does not
			 * exist and Voice L2 number not supported, R520 stores Fax number in location 1.
			 * We need to follow R520. We have this logic in function
			 * PBK_HandleWritePbkReq in phonebk.c. For now we just assume we will store
			 * in location 3.
			 */
			index = 3;
			alpha_ptr = fax_alpha;

			break;

		case ESNU_MSISDN_TYPE_DATA:
			index = 2;
			alpha_ptr = data_alpha;

			break;

		default:
			AT_CmdRspError(chan, AT_CME_ERR_INVALID_INDEX);
			return AT_STATUS_DONE;
		}

		/* Send the request to phonebook to update MSISDN number */
		PBK_SendWriteEntryReq( AT_GetV24ClientIDFromMpxChan(chan), PB_MSISDN, TRUE, index,
				((_P3 == NULL) || (_P3[0] == '\0')) ? UNKNOWN_TON_ISDN_NPI : atoi((char *) _P3),
				(char *) _P2,
				ALPHA_CODING_GSM_ALPHABET,
				((_P2 == NULL) || (_P2[0] == '\0')) ? 0 : strlen((char *) alpha_ptr),
				((_P2 == NULL) || (_P2[0] == '\0')) ? NULL : (UInt8 *) alpha_ptr,
				PostMsgToATCTask );

		status = AT_STATUS_PENDING;
	}

	return status;
}
