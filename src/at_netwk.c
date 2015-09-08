//=============================================================================
// File:			at_netwk.c
//
// Description:		AT interface for network related functions.
//
// Note:
//
//==============================================================================
#include <stdlib.h>
#include "at_netwk.h"
#include "types.h"
#include "osheap.h"
#include "taskmsgs.h"
#include "string.h"
#include "log.h"
#include "plmn_table.h"
#include "ms_database.h"
#include "sim_api.h"
#include "sim_mi.h"

#include "at_data.h"		//**FixMe** Robert (temporary)
#include "at_cfg.h"

//-------------------------------------------------
// Import Functions and Variables
//-------------------------------------------------

//-------------------------------------------------
// Local Definitions
//-------------------------------------------------



//----------------------------------------------------------------------
// AtHandleCipherIndication:	handle MSG_CIPHER_IND msg to ATC task.
//								for the given data length for payload.
//-----------------------------------------------------------------------
Result_t	AtHandleCipherIndication(InterTaskMsg_t* msg)
{
   	Boolean		isCiphOn = *(Boolean *)msg->dataBuf;
	char		tempBuf[12];
	AT_ChannelCfg_t* channelCfg = AT_GetChannelCfg( 0 ) ;	// **FixMe** 0
																	// is hard coded

	if(channelCfg->ECIPC == 1){
		sprintf(tempBuf, "*ECIPM: %1d", isCiphOn);
		AT_OutputUnsolicitedStr((UInt8*)tempBuf);
	}

	return RESULT_OK;
}


//===================================================================================
// AT Handle Functions for Network Related Messages
//===================================================================================

/************************************************************************************
*
*	Function Name:	AtHandleEfileInfoForPlmn
*
*	Description:	This function handles Efile information returned in a Generic
*					SIM Accesss for the Preferred PLMN entries.
*
*
*************************************************************************************/
void AtHandleEfileInfoForPlmn(InterTaskMsg_t* inMsg)
{
	SIM_EFILE_INFO_t *efile_info = (SIM_EFILE_INFO_t *) inMsg->dataBuf;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	char temp_buffer[30];

	if(efile_info->result == SIMACCESS_SUCCESS)
	{
		switch( AT_GetCmdIdByMpxChan(mpx_chan_num) )
		{
		case AT_CMD_CPOL:
			sprintf( temp_buffer, "%s: (1-%d),(0,1,2)",
				(char *) AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num)),
				efile_info->file_size / PLMN_ARR_LEN );
			break;

#ifdef GCXX_PC_CARD
		case AT_CMD_EPNR:
		case AT_CMD_EPNW:

			sprintf( temp_buffer, "%s: (1-%d),(2)",
				     (char *) AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num)),
					 efile_info->file_size / PLMN_ARR_LEN );
			break;
#endif

		default:
			assert(FALSE);
			break;
		}

		AT_OutputStr(mpx_chan_num, (UInt8 *)temp_buffer);
		AT_CmdRspOK(mpx_chan_num);
	}
	else
	{
		AT_CmdRspError(mpx_chan_num, AT_CME_ERR_OP_NOT_ALLOWED);
	}
}


/************************************************************************************
*
*	Function Name:	AtHandleEfileDataForPlmn
*
*	Description:	This function handles Efile data contents returned in a Generic
*					SIM Accesss for the Preferred PLMN entries.
*
*
*************************************************************************************/
void AtHandleEfileDataForPlmn(InterTaskMsg_t* inMsg)
{
	SIM_EFILE_DATA_t *efile_data_ptr = (SIM_EFILE_DATA_t *) inMsg->dataBuf;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	char *cmd_ptr = (char *) AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num));
	UInt8 *ptr;
	UInt8 format = AT_GetChannelCfg(mpx_chan_num)->CPOL;
	UInt8 i;
	UInt8 start_index, stop_index;
	UInt8 oper_str[7];
	char temp_buffer[50];
	PLMN_ID_t plmn_id;

	if(efile_data_ptr->result != SIMACCESS_SUCCESS)
	{
   		AT_CmdRspError(mpx_chan_num, AT_CME_ERR_OP_NOT_ALLOWED);
		return;
	}

	/* Get the number of PLMN entries */
	efile_data_ptr->data_len /= PLMN_ARR_LEN;

#ifdef GCXX_PC_CARD
	if( AT_GetCmdIdByMpxChan(mpx_chan_num) == AT_CMD_EPNR )
	{
		ptr = (UInt8 *) AT_GetParm(mpx_chan_num, 1); /* Get pointer to P2 */
		start_index = (ptr == NULL) ? 1 : atoi((char *) ptr);

		ptr = (UInt8 *) AT_GetParm(mpx_chan_num, 2); /* Get pointer to P3 */
		stop_index = (ptr == NULL) ? efile_data_ptr->data_len : atoi((char *) ptr);

		if( (start_index == 0) || (stop_index == 0) || (start_index > efile_data_ptr->data_len) ||
			(stop_index > efile_data_ptr->data_len) || (start_index > stop_index) )
		{
			AT_CmdRspError(mpx_chan_num, AT_CME_ERR_INVALID_INDEX);
			return;
		}

		/* Make index 0-based */
		start_index--;
		stop_index--;
	}
#endif

	if(efile_data_ptr->data_len > 0)
	{
		for(i = 0; i < efile_data_ptr->data_len; i++, efile_data_ptr->ptr += PLMN_ARR_LEN)
		{
			if( (efile_data_ptr->ptr[0] == SIM_RAW_EMPTY_VALUE) &&
				(efile_data_ptr->ptr[1] == SIM_RAW_EMPTY_VALUE) &&
				(efile_data_ptr->ptr[2] == SIM_RAW_EMPTY_VALUE) )
			{
				/* The entry is not filled, skip it */
				continue;
			}

			plmn_id.mcc = (efile_data_ptr->ptr[0] << 8) | efile_data_ptr->ptr[1];
			plmn_id.mnc = efile_data_ptr->ptr[2];

			if( AT_GetCmdIdByMpxChan(mpx_chan_num) == AT_CMD_CPOL )
			{
				if(format == NUMERIC_FORMAT)
				{
					MS_ConvertRawPlmnToHer(plmn_id.mcc, plmn_id.mnc, oper_str);
					sprintf(temp_buffer, "%s: %d,%d,\"%s\"", cmd_ptr, i + 1, format, oper_str);
				}
				else
				{
					PLMN_NAME_t *plmn_name = OSHEAP_Alloc(sizeof(PLMN_NAME_t));

					ptr = NULL;

					if( MS_GetPLMNByCode( ATC_IsCharacterSetUcs2(), MS_PlmnConvertRawMcc(plmn_id.mcc), MS_PlmnConvertRawMnc(plmn_id.mcc, plmn_id.mnc),
						   ((format == LONG_FORMAT) ? plmn_name : NULL), ((format == SHORT_FORMAT) ? plmn_name : NULL), NULL ) )
					{
						ptr = (UInt8 *) UCS2_ConvertToAlphaStr(ATC_IsCharacterSetUcs2(), plmn_name->coding, plmn_name->name, plmn_name->name_size);

					}

					sprintf(temp_buffer, "%s: %d,%d,\"%s\"", cmd_ptr, i + 1, format, ((ptr != NULL) ? ((char *) ptr) : ""));

					if(ptr != NULL)
					{
						OSHEAP_Delete(ptr);
					}

					OSHEAP_Delete(plmn_name);
				}

				AT_OutputStr(mpx_chan_num, (UInt8 *)temp_buffer);
			}
			/* Else: must be AT_CMD_EPNR: numeric format only and we need to output
			 *       the entries specified */
			else if( (i >= start_index) && (i <= stop_index) )
			{
				MS_ConvertRawPlmnToHer(plmn_id.mcc, plmn_id.mnc, oper_str);

				sprintf(temp_buffer, "%s: %d,\"%s\"", cmd_ptr, i + 1, oper_str);
				AT_OutputStr(mpx_chan_num, (UInt8 *)temp_buffer);
			}
		}
	}

	AT_CmdRspOK(mpx_chan_num);
}




/* ============= End of File: at_netwk.c ==================== */
