/**
 *
 *   @file   at_stk.c
 *
 *   @brief  This file contains the interface functions for AT-STK commands.
 *
 ****************************************************************************/

#include <stdio.h>
#include "string.h"
#include "stdlib.h"
#include "v24.h"
#include "mstypes.h"
#include "atc.h"
#include "at_util.h"
#include "at_data.h"
#include "sdltrace.h"
#include "ostask.h"
#include "osinterrupt.h"
#include "sysparm.h"
#include "at_stk.h"
#include "stkapi.h"
#include "osheap.h"
#include "macros.h"


#define ENABLE_ICON

#include "at_api.h"
#include "taskmsgs.h"
#include "log.h"


#define MAX_NUMBER_OF_ITEMS		100

#define STK_DISPLAY_TEXT_ADD_INFO_BYTES		25		// Space for additional info provided in response to handling an STK Display Text Request.

UInt8	itemId[MAX_NUMBER_OF_ITEMS];

/* Event values for STK Event Download AT Interface in AT*MSEL and AT*MEDL commands */
#define USER_ACTIVITY_EVENT			0
#define IDLE_SCREEN_AVAIL_EVENT		1
#define LANGUAGE_SELECTION_EVENT	2
#define BROWSER_TERMINATION_EVENT	3


// =========
// prototype
// =========
void AT_StkStringPreProcessing(UInt8* outStr, UInt8* inStr,
								UInt16 len, Unicode_t codeType);

extern void STKPRIM_TestDtmf(void);
//extern void TProcessIdleModeTextReq(void);

extern UInt8* atc_GetGsmToTeConvTbl(void);

//******************************************************************************
//
// Function Name:	AT_HandleSTKDisplayTextInd
//
// Description:		Display Text request from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKDisplayTextInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8*				rspBuf;
	UInt8*				str;

	if(stkData->u.display_text == NULL){
		_TRACE(("ATC_SATK: DisplayText: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	AT_StkStringPreProcessing(str, stkData->u.display_text->stkStr.string,
		stkData->u.display_text->stkStr.len, stkData->u.display_text->stkStr.unicode_type);

	rspBuf = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1 + STK_DISPLAY_TEXT_ADD_INFO_BYTES);

#ifdef ENABLE_ICON
	sprintf( (char*)rspBuf, "%s: %d, %d, %d, %d, %d, %d\n\r\"%s\"", "*MTDISP",
							stkData->u.display_text->isHighPrio,
							stkData->u.display_text->isDelay,
							!IS_NON_UCS2_CODING(stkData->u.display_text->stkStr.unicode_type),
							stkData->u.display_text->icon.IsExist,
							stkData->u.display_text->icon.IsSelfExplanatory,
							stkData->u.display_text->icon.Id,
							str );
#else
	sprintf( (char*)rspBuf, "%s: %d, %d, %d\n\r\"%s\"", "*MTDISP",
							stkData->u.display_text->isHighPrio,
							stkData->u.display_text->isDelay,
							!IS_NON_UCS2_CODING(stkData->u.display_text->stkStr.unicode_type),
							str );
#endif

	AT_OutputUnsolicitedStr(rspBuf);

	OSHEAP_Delete(rspBuf);
	OSHEAP_Delete(str);
}

//******************************************************************************
//
// Function Name:	AT_HandleSTKIdleModeTextInd
//
// Description:		Idle Mode Text request from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKIdleModeTextInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt8*				str;

	if(stkData->u.idlemode_text == NULL){
		_TRACE(("ATC_SATK: idlemode_text: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	AT_StkStringPreProcessing(str, stkData->u.idlemode_text->stkStr.string,
		stkData->u.idlemode_text->stkStr.len, stkData->u.idlemode_text->stkStr.unicode_type);

#ifdef ENABLE_ICON
	sprintf( (char*)rspBuf, "%s: %d, %d, %d, %d\n\r\"%s\"", "*MTITXT",
							!IS_NON_UCS2_CODING(stkData->u.idlemode_text->stkStr.unicode_type),
							stkData->u.idlemode_text->icon.IsExist,
							stkData->u.idlemode_text->icon.IsSelfExplanatory,
							stkData->u.idlemode_text->icon.Id,
							str );
#else
	sprintf( (char*)rspBuf, "%s: %d\n\r\"%s\"", "*MTITXT",
							!IS_NON_UCS2_CODING(stkData->u.idlemode_text->stkStr.unicode_type),
							str );
#endif
	AT_OutputUnsolicitedStr(rspBuf);

	OSHEAP_Delete(str);
}


//******************************************************************************
//
// Function Name:	AT_HandleSTKLaunchBrowserReq
//
// Description:		Launch Browser request from SIM
//
// Notes:
//
//******************************************************************************
void AT_HandleSTKLaunchBrowserReq(InterTaskMsg_t* inMsg)
{
	LaunchBrowserReq_t *launch_browser = ((SATK_EventData_t *) inMsg->dataBuf)->u.launch_browser;
	UInt8	*text_str;
	UInt8	*alpha_str;
	UInt8	rspBuf[AT_RSPBUF_LG];
	UInt8	length;
	char	bearer_str[40];
	char	provFileRef[60];

	/* Convert Bearer List data to Hex String format */
	UTIL_HexDataToHexStr((UInt8 *) bearer_str, launch_browser->bearer, launch_browser->bearer_length);

	/* For now, just output the first provision file data */
	if(launch_browser->prov_length > 0)
	{
		length = MIN(launch_browser->prov_file[0].length, sizeof(provFileRef) - 1);

		memcpy(provFileRef, launch_browser->prov_file[0].prov_file_data, length);
		provFileRef[length] = '\0';
	}
	else
	{
		provFileRef[0] = '\0';
	}

	text_str = OSHEAP_Alloc(2 * STK_MAX_ALPHA_TEXT_LEN + 1);
	alpha_str = OSHEAP_Alloc(2 * STK_MAX_ALPHA_TEXT_LEN + 1);

	/* Gateway/Proxy data */
	AT_StkStringPreProcessing( text_str, launch_browser->text.string,
		launch_browser->text.len, launch_browser->text.unicode_type );

	/* Alpha Identifier */
	AT_StkStringPreProcessing( alpha_str, launch_browser->alpha_id.string,
		launch_browser->alpha_id.len, launch_browser->alpha_id.unicode_type);

#ifdef ENABLE_ICON

	/* Launch Browser Notification message format:
	 *
	 *			*MTLBR: <action>, <url>, <bearer>,<provFileRef><LF><CR>
	 *					<textCodeType>,<textStr><LF><CR>
	 *					<alphaCodeType>,<alphaID>,<iconExist>,<iconSelfExp>,<iconId>
	 *
	 *			<bearer> - Bearer list in order of priority: SMS: "00"; CSD: "01";
	 *					   USSD: "02"; GPRS: "03" (see Section 12.49 of GSM 11.14)
	 *
	 ***********************************************************/
	sprintf( (char*) rspBuf, "*MTLBR: %d,\"%s\",\"%s\",\"%s\"\n\r%d,\"%s\"\n\r%d,\"%s\",%d,%d,%d",
			launch_browser->browser_action, launch_browser->url,
			bearer_str, provFileRef,
			!IS_NON_UCS2_CODING(launch_browser->text.unicode_type),
			text_str,
			!IS_NON_UCS2_CODING(launch_browser->alpha_id.unicode_type),
			alpha_str,
			launch_browser->icon_id.IsExist,
			launch_browser->icon_id.IsSelfExplanatory,
			launch_browser->icon_id.Id );
#else
	/* Launch Browser Notification message format:
	 *
	 *			*MTLBR: <action>, <url>, <bearer>,<provFileRef><LF><CR>
	 *					<textCodeType>,<textStr><LF><CR>
	 *					<alphaCodeType>,<alphaID>
	 *
	 *			<bearer> - Bearer list in order of priority: SMS: "00"; CSD: "01";
	 *					   USSD: "02"; GPRS: "03" (see Section 12.49 of GSM 11.14)

	 *
	 ***********************************************************/
	sprintf( (char*) rspBuf, "*MTLBR: %d,\"%s\",\"%s\",\"%s\"\n\r%d,\"%s\"\n\r%d,\"%s\"",
			launch_browser->browser_action, launch_browser->url,
			bearer_str, provFileRef,
			!IS_NON_UCS2_CODING(launch_browser->text.unicode_type),
			text_str,
			!IS_NON_UCS2_CODING(launch_browser->alpha_id.unicode_type),
			alpha_str );
#endif

	OSHEAP_Delete(text_str);
	OSHEAP_Delete(alpha_str);

	AT_OutputUnsolicitedStr(rspBuf);
}


//******************************************************************************
//
// Function Name:	AT_HandleSTKGetInkeyInd
//
// Description:		Get inkey request from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKGetInkeyInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt8*				str;

	if(stkData->u.get_inkey == NULL){
		_TRACE(("ATC_SATK: GetInkey: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	AT_StkStringPreProcessing(str, stkData->u.get_inkey->stkStr.string,
		stkData->u.get_inkey->stkStr.len, stkData->u.get_inkey->stkStr.unicode_type);

#ifdef ENABLE_ICON
	sprintf( (char*)rspBuf, "%s: %d, %d, %d, %d, %d, %d\n\r\"%s\"", "*MTKEY",
							stkData->u.get_inkey->inKeyType,
							!IS_NON_UCS2_CODING(stkData->u.get_inkey->stkStr.unicode_type),
							stkData->u.get_inkey->isHelpAvailable,
							stkData->u.get_inkey->icon.IsExist,
							stkData->u.get_inkey->icon.IsSelfExplanatory,
							stkData->u.get_inkey->icon.Id,
							str );
#else
	sprintf( (char*)rspBuf, "%s: %d, %d\n\r\"%s\"", "*MTKEY",
							stkData->u.get_inkey->inKeyType,
							!IS_NON_UCS2_CODING(stkData->u.get_inkey->stkStr.unicode_type),
							str );
#endif

	AT_OutputUnsolicitedStr( rspBuf );

	OSHEAP_Delete(str);
}

//******************************************************************************
//
// Function Name:	AT_HandleSTKGetInputInd
//
// Description:		Get input request from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKGetInputInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt8*				str;

	if(stkData->u.get_input == NULL){
		_TRACE(("ATC_SATK: GetInput: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	AT_StkStringPreProcessing(str, stkData->u.get_input->stkStr.string,
		stkData->u.get_input->stkStr.len, stkData->u.get_input->stkStr.unicode_type);

#ifdef ENABLE_ICON
	sprintf( (char*)rspBuf, "%s: %d, %d, %d, %d, %d, %d, %d, %d, %d\n\r\"%s\"", "*MTGIN",
							stkData->u.get_input->inPutType,
							!IS_NON_UCS2_CODING(stkData->u.get_input->stkStr.unicode_type),
							stkData->u.get_input->isEcho,
							stkData->u.get_input->minLen,
							stkData->u.get_input->maxLen,
							stkData->u.get_input->isHelpAvailable,
							stkData->u.get_input->icon.IsExist,
							stkData->u.get_input->icon.IsSelfExplanatory,
							stkData->u.get_input->icon.Id,
							str );
#else
	sprintf( (char*)rspBuf, "%s: %d, %d, %d, %d, %d\n\r\"%s\"", "*MTGIN",
							stkData->u.get_input->inPutType,
							!IS_NON_UCS2_CODING(stkData->u.get_input->stkStr.unicode_type),
							stkData->u.get_input->isEcho,
							stkData->u.get_input->minLen,
							stkData->u.get_input->maxLen,
							str );
#endif

	if(stkData->u.get_input->defaultSATKStr.len != 0){

		AT_StkStringPreProcessing(str, stkData->u.get_input->defaultSATKStr.string,
			stkData->u.get_input->defaultSATKStr.len, stkData->u.get_input->defaultSATKStr.unicode_type);

		sprintf((char*)rspBuf + strlen((char*)rspBuf), "\n\r\"%s\"", str);
	}
	else{
		sprintf((char*)rspBuf + strlen((char*)rspBuf), "\n\r\"%s\"", "");
	}

	AT_OutputUnsolicitedStr( rspBuf );

	OSHEAP_Delete(str);
}

//******************************************************************************
//
// Function Name:	AT_HandleSTKPlayToneInd
//
// Description:		Play tone request from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKPlayToneInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt8*				str;

	if(stkData->u.play_tone == NULL){
		_TRACE(("ATC_SATK: Playtone: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	if(stkData->u.play_tone->stkStr.string==NULL)
	{
#ifdef ENABLE_ICON
		sprintf( (char*)rspBuf, "%s: \"%s\", , %d, %ld, %d, %d, %d", "*MTTONE",
							"",
							stkData->u.play_tone->toneType,
							stkData->u.play_tone->duration,
							FALSE, 0, 0);
#else
		sprintf( (char*)rspBuf, "%s: \"%s\", 0, %d, %ld", "*MTTONE",
							"",
							stkData->u.play_tone->toneType,
							stkData->u.play_tone->duration );
#endif
	}
	else
	{
		AT_StkStringPreProcessing(str, stkData->u.play_tone->stkStr.string,
				stkData->u.play_tone->stkStr.len, stkData->u.play_tone->stkStr.unicode_type);

#ifdef ENABLE_ICON
		sprintf( (char*)rspBuf, "%s: \"%s\", %d, %d, %ld, %d, %d, %d", "*MTTONE",
							(char*)str,
							!IS_NON_UCS2_CODING(stkData->u.play_tone->stkStr.unicode_type),
							stkData->u.play_tone->toneType,
							stkData->u.play_tone->duration,
							stkData->u.play_tone->icon.IsExist,
							stkData->u.play_tone->icon.IsSelfExplanatory,
							stkData->u.play_tone->icon.Id);
#else
		sprintf( (char*)rspBuf, "%s: \"%s\", %d, %d, %ld", "*MTTONE",
							(char*)str,
							!IS_NON_UCS2_CODING(stkData->u.play_tone->stkStr.unicode_type),
							stkData->u.play_tone->toneType,
							stkData->u.play_tone->duration );
#endif
	}

	AT_OutputUnsolicitedStr( rspBuf );

	OSHEAP_Delete(str);
}

//******************************************************************************
//
// Function Name:	AT_HandleSTKSelectItemInd
//
// Description:		Select Item request from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKSelectItemInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8*				rspBuf = NULL;
	UInt8				numItems;
	UInt8				i;
	UInt8*				str;
	Boolean				isUCS2 = FALSE;

	if(stkData->u.select_item == NULL){
		_TRACE(("ATC_SATK: Select Item: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	numItems = stkData->u.select_item->listSize;
	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	// determine the coding type based on the title type
	if( !IS_NON_UCS2_CODING(stkData->u.select_item->title.unicode_type) ){
		isUCS2 = TRUE;
	}

	rspBuf = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+50);

	// temp debug:
	SIM_LOG_ARRAY("ATC Select Item Title: ", stkData->u.select_item->title.string, stkData->u.select_item->title.len);
	SIM_LOGV("ATC Select Item Title u_type: ", stkData->u.select_item->title.unicode_type);

	AT_StkStringPreProcessing(str, stkData->u.select_item->title.string,
		stkData->u.select_item->title.len, stkData->u.select_item->title.unicode_type);

#ifdef ENABLE_ICON
	sprintf( (char*)rspBuf, "\r\n%s: \"%s\", %d, %d, %d, %d, %d, %d", "*MTITEM", str, isUCS2, numItems,
										stkData->u.select_item->isHelpAvailable,
										stkData->u.select_item->titleIcon.IsExist,
										stkData->u.select_item->titleIcon.IsSelfExplanatory,
										stkData->u.select_item->titleIcon.Id);
#else
	sprintf( (char*)rspBuf, "\r\n%s: \"%s\", %d, %d", "*MTITEM", str, isUCS2, numItems);
#endif

	AT_OutputUnsolicitedStrNoCR(rspBuf);

	for(i=0; i<numItems; i++)
	{
		AT_StkStringPreProcessing(str, stkData->u.select_item->pItemList[i].string,
			stkData->u.select_item->pItemList[i].len, stkData->u.select_item->pItemList[i].unicode_type);

		isUCS2 = !IS_NON_UCS2_CODING(stkData->u.select_item->pItemList[i].unicode_type);

#ifdef ENABLE_ICON
		sprintf((char*)rspBuf, "\n\r%d:\"%s\",%d,%d,%d,%d,%d,%d", i+1, str,
			stkData->u.select_item->pIconList.IsExist,stkData->u.select_item->pIconList.IsSelfExplanatory,
			stkData->u.select_item->pIconList.Id[i],
			stkData->u.select_item->pNextActIndList.IsExist,stkData->u.select_item->pNextActIndList.Id[i], isUCS2);
		itemId[i+1] = stkData->u.select_item->pItemIdList[i];
#else
		sprintf((char*)rspBuf, "\n\r%d:\"%s\", %d", i+1, str, isUCS2);
		itemId[i+1] = stkData->u.select_item->pItemIdList[i];
#endif

		AT_OutputUnsolicitedStrNoCR( rspBuf );
	}

	AT_OutputUnsolicitedStrNoCR((UInt8 *) "\r\n");

	OSHEAP_Delete(str);
	OSHEAP_Delete(rspBuf);
}


//******************************************************************************
//
// Function Name:	AT_HandleSTKRefreshInd
//
// Description:		Refresh request from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKRefreshInd(InterTaskMsg_t* inMsg)
{
	Refresh_t			*refresh_ptr = ((SATK_EventData_t *) inMsg->dataBuf)->u.refresh;
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt8				i;
	UInt8				j;
	UInt8				file_id_data[MAX_SIM_FILE_PATH_LEN * 2];
	Boolean				isFileChanged = FALSE;

	if(refresh_ptr == NULL){
		_TRACE(("ATC_SATK: Refresh: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

#ifdef STACK_wedge
	/* New AT*MTRSH command format that is defined to work with USIMAP */
	sprintf((char *) rspBuf, "%s: %d,%d\n\r", "*MTRSH", refresh_ptr->refreshType, refresh_ptr->FileIdList.number_of_file);

	for (i = 0; i < refresh_ptr->FileIdList.number_of_file; i++)
	{
		strcat((char *) rspBuf, "\"");

		for (j = 0; j < refresh_ptr->FileIdList.changed_file[i].path_len; j++)
		{
			/* First transform the file path data to a byte array */
			file_id_data[j << 1] = (refresh_ptr->FileIdList.changed_file[i].file_path[j] >> 8);
			file_id_data[(j << 1) + 1] = (refresh_ptr->FileIdList.changed_file[i].file_path[j] & 0x00FF);
		}

		UTIL_HexDataToHexStr(rspBuf + strlen((char *) rspBuf), file_id_data, j << 1);

		strcat((char *) rspBuf, "\"\n\r");
	}

#else
	/* Old AT*MTRSH command format that is defined to work with old SIMAP */
	sprintf( (char*)rspBuf, "%s: %d\n\r", "*MTRSH", refresh_ptr->refreshType);

	if( (refresh_ptr->refreshType != SMRT_RESET) &&
		(refresh_ptr->refreshType != SMRT_INIT_FULLFILE_CHANGED) ){
		// print out EF files that have changed
		for(i=0; i<SMFN_FILE_NUM; i++){
			if(refresh_ptr->FileList[i])
			{
				if(isFileChanged)
				{
					strcat((char *) rspBuf, ", ");
				}
				else
				{
					isFileChanged = TRUE;
				}

				sprintf( (char*)rspBuf + strlen((char*)rspBuf), "%d", i);
			}
		}

		if(!isFileChanged){
			sprintf((char*)rspBuf + strlen((char*)rspBuf), "%d\n\r", SMFN_FILE_NONE);
		}
	}
#endif

	AT_OutputUnsolicitedStr( rspBuf );
}


//******************************************************************************
//
// Function Name:	AT_HandleSTKSetupEventList
//
// Description:		Send the *MSEL command to host if the Setup Event List indicates
//					one of the MMI Event Downloads is enabled. The MMI Event Downloads
//					include the following events:
//					User Activity Event; Idle Screen Available Event;
//					Language Selection Event; Browser Termination Event.
//
//******************************************************************************
void AT_HandleSTKSetupEventList(InterTaskMsg_t* inMsg)
{
	UInt16 setup_event_list = *((UInt16 *) inMsg->dataBuf);
	char temp_buf[50];
	char output_buf[60];
    UInt8 num_events = 0;

	static Boolean AT_Setup_Event_Sent = FALSE;

	if ((setup_event_list & BIT_MASK_USER_ACTIVITY_EVENT) != 0)
	{
		num_events++;
		sprintf(temp_buf, ",%d", USER_ACTIVITY_EVENT);
	}

	if ((setup_event_list & BIT_MASK_IDLE_SCREEN_AVAIL_EVENT) != 0)
	{
		num_events++;
		sprintf(temp_buf + strlen(temp_buf), ",%d", IDLE_SCREEN_AVAIL_EVENT);
	}

	if ((setup_event_list & BIT_MASK_LAUGUAGE_SELECTION_EVENT) != 0)
	{
		num_events++;
		sprintf(temp_buf + strlen(temp_buf), ",%d", LANGUAGE_SELECTION_EVENT);
	}

	if ((setup_event_list & BIT_MASK_BROWSER_TERMINATION_EVENT) != 0)
	{
		num_events++;
		sprintf(temp_buf + strlen(temp_buf), ",%d", BROWSER_TERMINATION_EVENT);
	}

	/* We need to send "*MSEL" commands in the following two situations:
	 * 1. if the number of events is not zero: this means there are MMI events enabled.
	 * 2. if we have sent "*MSEL" command before and the number of events is zero, this means SIM
	 *	  wants to cancel the previously sent events.
	 */
	if ( (num_events != 0) || AT_Setup_Event_Sent)
	{
		sprintf(output_buf, "*MSEL: %d", num_events);

		if (num_events != 0)
		{
			strcat(output_buf, temp_buf);
		}

		AT_OutputUnsolicitedStr((UInt8 *) output_buf);

		AT_Setup_Event_Sent = TRUE;
	}
}


//******************************************************************************
//
// Function Name:	AT_HandleSTKSmsSentInd
//
// Description:		STK Send SMS notification from SIM. This function just tells
//					the host the Alpha ID for STK Send SMS. It does not have information
//					about whether the SMS sending is successful or not. GSM 11.14
//					does not require the ME to tell the user whether SMS sending is
//					successful or not.
//
//******************************************************************************

void AT_HandleSTKSmsSentInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt8*				str;

	if(stkData->u.send_short_msg == NULL){
		_TRACE(("ATC_SATK: Send SMS: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	if( stkData->u.send_short_msg->text.len != 0 )	// AlphaId notification
	{
		AT_StkStringPreProcessing(str, stkData->u.send_short_msg->text.string,
			stkData->u.send_short_msg->text.len, stkData->u.send_short_msg->text.unicode_type);

#ifdef ENABLE_ICON
		sprintf( (char*)rspBuf, "%s: \"%s\", %d, %d, %d, %d", "*MTSMS", str,
				!IS_NON_UCS2_CODING(stkData->u.send_short_msg->text.unicode_type),
				stkData->u.send_short_msg->icon.IsExist,
				stkData->u.send_short_msg->icon.IsSelfExplanatory,
				stkData->u.send_short_msg->icon.Id);
#else
		sprintf( (char*)rspBuf, "%s: \"%s\", %d", "*MTSMS", str,
				!IS_NON_UCS2_CODING(stkData->u.send_short_msg->text.unicode_type));
#endif

		AT_OutputUnsolicitedStr( rspBuf );
	}

	/* Based on stkapi, this should not happen:
	 * Else if(stkData->u.send_short_msg->text.len == 0)
	 */
	OSHEAP_Delete(str);
}

//******************************************************************************
//
// Function Name:	AT_HandleSTKSsSentInd
//
// Description:		SS Sent notif from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKSsSentInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt8*				str;

	if(stkData->u.send_ss == NULL){
		_TRACE(("ATC_SATK: Send SS: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	if(stkData->u.send_ss->isAlphaIdProvided)
	{
		if( strlen((char*)stkData->u.send_ss->text.string) != 0 )
		{
			AT_StkStringPreProcessing(str, stkData->u.send_ss->text.string,
					stkData->u.send_ss->text.len, stkData->u.send_ss->text.unicode_type);

#ifdef ENABLE_ICON
			sprintf( (char*)rspBuf, "%s: \"%s\", %d, \"%s\", %d, %d, %d", "*MTSS",
								str, !IS_NON_UCS2_CODING(stkData->u.send_ss->text.unicode_type),
								stkData->u.send_ss->num.Num,
								stkData->u.send_ss->icon.IsExist,
								stkData->u.send_ss->icon.IsSelfExplanatory,
								stkData->u.send_ss->icon.Id);
#else
			sprintf( (char*)rspBuf, "%s: \"%s\", %d, \"%s\"", "*MTSS",
								str, !IS_NON_UCS2_CODING(stkData->u.send_ss->text.unicode_type),
								stkData->u.send_ss->num.Num);
#endif

			AT_OutputUnsolicitedStr( rspBuf );
		}
		/* Else: empty string, do not sent AT command to PDA */
	}
	else
	{
#ifdef ENABLE_ICON
		sprintf( (char*)rspBuf, "%s: , , \"%s\", 0, 0, 0", "*MTSS", stkData->u.send_ss->num.Num);
#else
		sprintf( (char*)rspBuf, "%s: , , \"%s\"", "*MTSS", stkData->u.send_ss->num.Num);
#endif
		AT_OutputUnsolicitedStr( rspBuf );
	}

	OSHEAP_Delete(str);
}

//******************************************************************************
//
// Function Name:	AT_HandleSTKUssdSentInd
//
// Description:		USSD Sent notif from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKUssdSentInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8*				alpha_str;
	UInt8*				ussd_str;
	Unicode_t			ussd_unicode_type;
	UInt8				rspBuf[AT_RSPBUF_LG];

	if(stkData->u.send_ss == NULL){
		_TRACE(("ATC_SATK: Send USSD: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	alpha_str = OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);
	ussd_str = OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	/* UNICODE_UCS1 is for 8-bit data */
	ussd_unicode_type = (SMS_DecodeSmsDcsByte(stkData->u.send_ss->num.dcs) == SMS_CODING_UCS2) ? UNICODE_UCS2 : UNICODE_UCS1;

	AT_StkStringPreProcessing(ussd_str, stkData->u.send_ss->num.Num, stkData->u.send_ss->num.len, ussd_unicode_type);

	if(stkData->u.send_ss->isAlphaIdProvided)
	{
		if( strlen((char*)stkData->u.send_ss->text.string) != 0 )
		{
			AT_StkStringPreProcessing(alpha_str, stkData->u.send_ss->text.string,
					stkData->u.send_ss->text.len, stkData->u.send_ss->text.unicode_type);

#ifdef ENABLE_ICON
			sprintf( (char*)rspBuf, "%s: \"%s\", %d, \"%s\", %d, %d, %d, %d", "*MTUSSD",
								alpha_str, !IS_NON_UCS2_CODING(stkData->u.send_ss->text.unicode_type),
								ussd_str,
								!IS_NON_UCS2_CODING(ussd_unicode_type),
								stkData->u.send_ss->icon.IsExist,
								stkData->u.send_ss->icon.IsSelfExplanatory,
								stkData->u.send_ss->icon.Id);
#else
			sprintf( (char*)rspBuf, "%s: \"%s\", %d, \"%s\", %d", "*MTUSSD",
								alpha_str, !IS_NON_UCS2_CODING(stkData->u.send_ss->text.unicode_type),
								ussd_str,
								!IS_NON_UCS2_CODING(ussd_unicode_type) );
#endif
			AT_OutputUnsolicitedStr( rspBuf );
		}
		/* Else: empty string , do not send command to PDA */
	}
	else
	{
#ifdef ENABLE_ICON
		sprintf( (char*)rspBuf, "%s: , , \"%s\", %d, 0, 0, 0", "*MTUSSD", ussd_str, !IS_NON_UCS2_CODING(ussd_unicode_type));
#else
		sprintf( (char*)rspBuf, "%s: , , \"%s\", %d", "*MTUSSD", ussd_str, !IS_NON_UCS2_CODING(ussd_unicode_type));
#endif
		AT_OutputUnsolicitedStr( rspBuf );
	}

	OSHEAP_Delete(alpha_str);
	OSHEAP_Delete(ussd_str);
}

//******************************************************************************
//
// Function Name:	AT_HandleSTKCallSetupInd
//
// Description:		Call Setup init by SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKCallSetupInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt8*				str;

	if(stkData->u.setup_call == NULL){
		_TRACE(("ATC_SATK: CallSetup: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	if(stkData->u.setup_call->IsSetupAlphaIdProvided)
	{
		if(stkData->u.setup_call->setupPhaseStr.len > 0)
		{
			AT_StkStringPreProcessing(str, stkData->u.setup_call->setupPhaseStr.string,
				stkData->u.setup_call->setupPhaseStr.len, stkData->u.setup_call->setupPhaseStr.unicode_type);

			sprintf( (char*)rspBuf, "%s: %d, \"%s\", %d, \"%s%s\", %ld, 0", "*MTCALL",
										stkData->u.setup_call->callType,
										str, !IS_NON_UCS2_CODING(stkData->u.setup_call->setupPhaseStr.unicode_type),
										(stkData->u.setup_call->num.Ton == InternationalTON) ? "+":"",
										stkData->u.setup_call->num.Num,
										stkData->u.setup_call->duration);
		}
		else
		{
			sprintf( (char*)rspBuf, "%s: %d, \"\", 0, \"%s%s\", %ld, 0", "*MTCALL",
										stkData->u.setup_call->callType,
										(stkData->u.setup_call->num.Ton == InternationalTON) ? "+":"",
										stkData->u.setup_call->num.Num,
										stkData->u.setup_call->duration);
		}
	}
	else
	{
		sprintf( (char*)rspBuf, "%s: %d, \"\", 0, \"%s%s\", %ld, 0", "*MTCALL",
										stkData->u.setup_call->callType,
										(stkData->u.setup_call->num.Ton == InternationalTON) ? "+":"",
										stkData->u.setup_call->num.Num,
										stkData->u.setup_call->duration);
	}

#ifdef ENABLE_ICON
	sprintf( (char*)rspBuf + strlen((char*)rspBuf), ", %d, %d, %d",
										stkData->u.setup_call->setupPhaseIcon.IsExist,
										stkData->u.setup_call->setupPhaseIcon.IsSelfExplanatory,
										stkData->u.setup_call->setupPhaseIcon.Id);
#endif

	AT_OutputUnsolicitedStr( rspBuf );

	if(stkData->u.setup_call->IsConfirmAlphaIdProvided)
	{
		if(stkData->u.setup_call->confirmPhaseStr.len > 0)
		{
			AT_StkStringPreProcessing(str, stkData->u.setup_call->confirmPhaseStr.string,
				stkData->u.setup_call->confirmPhaseStr.len, stkData->u.setup_call->confirmPhaseStr.unicode_type);

			sprintf( (char*)rspBuf, "%s: %d, \"%s\", %d, \"%s%s\", %ld, 1", "*MTCALL",
										stkData->u.setup_call->callType,
										str, !IS_NON_UCS2_CODING(stkData->u.setup_call->confirmPhaseStr.unicode_type),
										(stkData->u.setup_call->num.Ton == InternationalTON) ? "+":"",
										stkData->u.setup_call->num.Num,
										stkData->u.setup_call->duration);
		}
		else
		{
			sprintf( (char*)rspBuf, "%s: %d, \"\", 0, \"%s%s\", %ld, 1", "*MTCALL",
										stkData->u.setup_call->callType,
										(stkData->u.setup_call->num.Ton == InternationalTON) ? "+":"",
										stkData->u.setup_call->num.Num,
										stkData->u.setup_call->duration);
		}
	}
	else
	{
		sprintf( (char*)rspBuf, "%s: %d, \"\", 0, \"%s%s\", %ld, 1", "*MTCALL",
										stkData->u.setup_call->callType,
										(stkData->u.setup_call->num.Ton == InternationalTON) ? "+":"",
										stkData->u.setup_call->num.Num,
										stkData->u.setup_call->duration);
	}

#ifdef ENABLE_ICON
	sprintf( (char*)rspBuf + strlen((char*)rspBuf), ", %d, %d, %d",
										stkData->u.setup_call->confirmPhaseIcon.IsExist,
										stkData->u.setup_call->confirmPhaseIcon.IsSelfExplanatory,
										stkData->u.setup_call->confirmPhaseIcon.Id);
#endif

	AT_OutputUnsolicitedStr( rspBuf );

	OSHEAP_Delete(str);
}

//******************************************************************************
//
// Function Name:	AT_HandleSTKSendDtmfInd
//
// Description:		Send DTMF string init by SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKSendDtmfInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt8*				str;

	if(stkData->u.send_dtmf == NULL){
		_TRACE(("ATC_SATK: Send DTMF: msg already being consumed: ClientId = ", inMsg->clientID));
		return;
	}

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	if(stkData->u.send_dtmf->isAlphaIdProvided)
	{
		AT_StkStringPreProcessing(str, stkData->u.send_dtmf->alphaString.string,
			stkData->u.send_dtmf->alphaString.len, stkData->u.send_dtmf->alphaString.unicode_type);

#ifdef ENABLE_ICON
		sprintf( (char*)rspBuf, "%s: \"%s\", %d, \"%s\", %d, %d, %d", "*MTDTMF",
						str, !IS_NON_UCS2_CODING(stkData->u.send_dtmf->alphaString.unicode_type),
						stkData->u.send_dtmf->dtmf,
						stkData->u.send_dtmf->dtmfIcon.IsExist,
						stkData->u.send_dtmf->dtmfIcon.IsSelfExplanatory,
						stkData->u.send_dtmf->dtmfIcon.Id);
#else
		sprintf( (char*)rspBuf, "%s: \"%s\", %d, \"%s\"", "*MTDTMF",
						str, !IS_NON_UCS2_CODING(stkData->u.send_dtmf->alphaString.unicode_type),
						stkData->u.send_dtmf->dtmf);
#endif
	}
	else
	{
		// do not display any info about the proactive DTMF
	}

	AT_OutputUnsolicitedStr( rspBuf );

	OSHEAP_Delete(str);
}

//******************************************************************************
//
// Function Name:	AT_HandleSTKSetupMenuInd
//
// Description:		Setup menu request from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKSetupMenuInd(InterTaskMsg_t* inMsg)
{
	SATK_EventData_t*	stkData = (SATK_EventData_t *)inMsg->dataBuf;
	UInt8*				rspBuf = NULL;
	UInt8				numMenus;
	UInt8				i;
	UInt8*				str;
	Boolean				isUCS2 = FALSE;

	if(stkData->u.setup_menu == NULL){
		_TRACE(("ATC_SATK: SetupMenu: msg already being consumed: ClientId = ", inMsg->clientID));
		_TRACE(("ATC_SATK: SetupMenu: msg already being consumed: usageCount = ", inMsg->usageCount));
		_TRACE(("ATC_SATK: SetupMenu: msg intack: msgType = ", stkData->msgType));
		return;
	}

	_TRACE(("ATC_SATK: SetupMenu: ClientId = ", inMsg->clientID));

	numMenus = stkData->u.setup_menu->listSize;
	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	rspBuf = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+50);

	AT_StkStringPreProcessing(str, stkData->u.setup_menu->title.string,
		stkData->u.setup_menu->title.len, stkData->u.setup_menu->title.unicode_type);

#ifdef ENABLE_ICON
	sprintf( (char*)rspBuf, "\r\n%s: \"%s\", %d, %d, %d, %d, %d, %d", "*MTSMENU", str,
		!IS_NON_UCS2_CODING(stkData->u.setup_menu->title.unicode_type), numMenus,
		stkData->u.setup_menu->isHelpAvailable,
		stkData->u.setup_menu->titleIcon.IsExist,
		stkData->u.setup_menu->titleIcon.IsSelfExplanatory,
		stkData->u.setup_menu->titleIcon.Id);
#else
	sprintf( (char*)rspBuf, "\r\n%s: \"%s\", %d, %d", "*MTSMENU", str,
		!IS_NON_UCS2_CODING(stkData->u.setup_menu->title.unicode_type), numMenus);
#endif

	AT_OutputUnsolicitedStrNoCR(rspBuf);

	// Print out item list
	for(i=0; i<numMenus; i++)
	{
		AT_StkStringPreProcessing(str, stkData->u.setup_menu->pItemList[i].string,
			stkData->u.setup_menu->pItemList[i].len, stkData->u.setup_menu->pItemList[i].unicode_type);

		isUCS2 = !IS_NON_UCS2_CODING(stkData->u.setup_menu->pItemList[i].unicode_type);

#ifdef ENABLE_ICON
		sprintf((char*)rspBuf, "\n\r%d:\"%s\",%d,%d,%d,%d,%d,%d", i+1, str,
			stkData->u.setup_menu->pIconList.IsExist,stkData->u.setup_menu->pIconList.IsSelfExplanatory,
			stkData->u.setup_menu->pIconList.Id[i],
			stkData->u.setup_menu->pNextActIndList.IsExist,stkData->u.setup_menu->pNextActIndList.Id[i], isUCS2);
#else
		sprintf((char*)rspBuf, "\n\r%d:\"%s\",%d", i+1, str, isUCS2);
#endif
		AT_OutputUnsolicitedStrNoCR(rspBuf);
	}

	AT_OutputUnsolicitedStrNoCR((UInt8 *) "\r\n");

	OSHEAP_Delete(str);
	OSHEAP_Delete(rspBuf);
}


//******************************************************************************
//
// Function Name:	AT_HandleSTKDataServInd
//
// Description:		Data Service notif from SIM
//
// Notes:
//
//******************************************************************************

void AT_HandleSTKDataServInd(InterTaskMsg_t* inMsg)
{
	StkDataService_t*	stkData = (StkDataService_t *)inMsg->dataBuf;
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt8*				str;

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	if( stkData->text.len != 0 )	// AlphaId notification
	{
		AT_StkStringPreProcessing(str, stkData->text.string,
			stkData->text.len, stkData->text.unicode_type);

#ifdef ENABLE_ICON
		sprintf( (char*)rspBuf, "%s: %d, %d, \"%s\", %d, %d, %d, %d, %d, %d, %d", "*MTCHEVT",
				stkData->chanID, stkData->actionType, str,
				!IS_NON_UCS2_CODING(stkData->text.unicode_type),
				stkData->icon.IsExist,
				stkData->icon.IsSelfExplanatory,
				stkData->icon.Id,
				stkData->isLoginNeeded, stkData->isPasswdNeeded, stkData->isApnNeeded);
#else
		sprintf( (char*)rspBuf, "%s: %d, %d, \"%s\", %d, %d, %d, %d", "*MTCHEVT",
				stkData->chanID, stkData->actionType, str,
				!IS_NON_UCS2_CODING(stkData->text.unicode_type),
				stkData->isLoginNeeded, stkData->isPasswdNeeded, stkData->isApnNeeded);
#endif

		AT_OutputUnsolicitedStr( rspBuf );
	}

	/* Delete dynamically allocated memory */
	if(stkData->text.string != NULL)
	{
		OSHEAP_Delete(stkData->text.string);
	}

	OSHEAP_Delete(str);
}


//******************************************************************************
//
// Function Name:	AT_HandleSTKRunAtCmdInd
//
// Description:		This function processes the STK Run AT Command indication
//					by posting the Alpha ID and Icon ID information to the host.
//
//******************************************************************************
void AT_HandleSTKRunAtCmdInd(InterTaskMsg_t* inMsg)
{
	StkRunAtCmd_t	*run_at_cmd = (StkRunAtCmd_t *) inMsg->dataBuf;
	UInt8			rspBuf[ AT_RSPBUF_LG ] ;
	UInt8*			str;

	str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

	if( run_at_cmd->text.len != 0 )	// AlphaId notification
	{
		AT_StkStringPreProcessing(str, run_at_cmd->text.string,
			run_at_cmd->text.len, run_at_cmd->text.unicode_type);

#ifdef ENABLE_ICON
		sprintf( (char*)rspBuf, "%s: \"%s\", %d, %d, %d, %d", "*MTRUNAT",
				str, !IS_NON_UCS2_CODING(run_at_cmd->text.unicode_type),
				run_at_cmd->icon.IsExist,
				run_at_cmd->icon.IsSelfExplanatory,
				run_at_cmd->icon.Id);
#else
		sprintf( (char*)rspBuf, "%s: \"%s\", %d", "*MTRUNAT",
				str, !IS_NON_UCS2_CODING(run_at_cmd->text.unicode_type));
#endif

		AT_OutputUnsolicitedStr( rspBuf );
	}

	/* Delete dynamically allocated memory */
	if(run_at_cmd->text.string != NULL)
	{
		OSHEAP_Delete(run_at_cmd->text.string);
	}

	OSHEAP_Delete(str);
}


//******************************************************************************
//
// Function Name:	AT_HandleSTKLangNotificationInd
//
// Description:		This function processes the STK Language Notification indication
//					by posting the language information to the host.
//
//					There is no need for the host to send back a response since
//					USIMAP has sent the Terminal Response when the Language
//					Notification is received.
//
//******************************************************************************
void AT_HandleSTKLangNotificationInd(InterTaskMsg_t* inMsg)
{
	StkLangNotification_t *lang_data = (StkLangNotification_t *) inMsg->dataBuf;
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	if(lang_data->language[0] == '\0')
	{
		/* No language is specified. The SIM wants to cancel the effect of
		 * a previous "Language Notification" command.
		 */
		sprintf( (char*)rspBuf, "%s: \"\"", "*MTLANGNT");
	}
	else
	{
		/* A specific language is specified, the host shall try to use this language
		 * in text string in the proactive command response or in the
		 * text string in the Envelope command.
		 */
		sprintf( (char*)rspBuf, "%s: \"%s\"", "*MTLANGNT", lang_data->language);
	}

	AT_OutputUnsolicitedStr( rspBuf );
}


//******************************************************************************
//
// Function Name:	ATCmd_MTRES_Handler
//
// Description:		Response command from TE back to SIM
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_MTRES_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								const UInt8* _P1, const UInt8* _P2, const UInt8* _P3,
								const UInt8* _P4, const UInt8* _P5, const UInt8* _P6)
{
	UInt8			rspBuf[ AT_RSPBUF_SM ] ;
	UInt8			commId;
	UInt8			result1;
	UInt8			result2;
	UInt8			id;
	UInt16			clientID = AT_GetV24ClientIDFromMpxChan(chan);

	if ( accessType == AT_ACCESS_REGULAR)
	{
		SATKString_t inText;

		commId = atoi( (char*)_P1 );
		result1 = atoi ( (char*)_P2 );
		result2 = _P3 == NULL ? 0 : atoi ( (char*)_P3 );
		id = (_P5 == NULL) ? 0 : atoi ( (char*)_P5 );

		// invoke function/method at satk.c to process the response to the proactive sim command
		if(_P4 == NULL)
		{
			inText.string = NULL;
			inText.len = 0;
			inText.unicode_type = UNICODE_GSM;
		}
		else
		{
			inText.len = strlen( (char*)_P4 );
			inText.string = (UInt8*)OSHEAP_Alloc(inText.len+1);
			memcpy((char *) inText.string, (char*)_P4, inText.len);
			inText.string[inText.len] = '\0';

			/* Use P3 to determine the encoding type for "Get Input" and "Get Inkey" */
			if (inText.len == 0)
			{
				inText.unicode_type = UNICODE_GSM;
			}
			else
			{
				inText.unicode_type = (result2 == 0) ? UNICODE_GSM : UNICODE_80;
			}

			if (commId == SATK_EVENT_GET_INKEY) //exception for get inkey
			{
				if(!strcmp((char *)inText.string,"YES")){
		         	inText.string[0] = 1;
		         	inText.string[1] = '\0';
		         	inText.len = 1;
				}
				else if(!strcmp((char *)inText.string,"NO")){
		         	inText.string[0] = 0;
		         	inText.string[1] = '\0';
		         	inText.len = 1;
				}
			}
		}

		if(commId == SATK_EVENT_DATA_SERVICE_REQ){

			StkCmdRespond_t cmdRsp;
			UInt8			len;

			cmdRsp.clientID = clientID;
			cmdRsp.event = SATK_EVENT_DATA_SERVICE_REQ;
			cmdRsp.result1 = result1;
			cmdRsp.result2 = result2;

			/* P4 is for user login ID of CSD/GPRS call */
			if(_P4 == NULL){
				cmdRsp.textStr1[0] = '\0';
			}
			else{
				if( (len = strlen((char*)_P4)) > SATK_LOGIN_PASSWD_LEN){
					_TRACE(("STK AT: Login str len = ", len));
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
					return AT_STATUS_DONE;
				}
				else{
					strcpy((char*)cmdRsp.textStr1, (char*)_P4);	// Login
				}
			}

			/* P5 is for login password of CSD/GPRS call */
			if(_P5 == NULL){
				cmdRsp.textStr2[0] = '\0';
			}
			else{
				if( (len = strlen((char*)_P5)) > SATK_LOGIN_PASSWD_LEN){
					_TRACE(("STK AT: Passwd str len = ", len));
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
					return AT_STATUS_DONE;
				}
				else{
					strcpy((char*)cmdRsp.textStr2, (char*)_P5);	// passwd
				}
			}

			/* P6 is for APN of GPRS call */
			if(_P6 == NULL){
				cmdRsp.textStr3[0] = '\0';
			}
			else{
				if( (len = strlen((char*)_P6)) > SATK_MAX_APN_LEN){
					_TRACE(("STK AT: GPRS APN len = ", len));
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
					return AT_STATUS_DONE;
				}
				else{
					strcpy((char*)cmdRsp.textStr3, (char*)_P6);	// passwd
				}
			}

			if(SATKDataServCmdResp(&cmdRsp) != TRUE)
			{
				// response format incorrect
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
				return AT_STATUS_DONE;
			}
		}
		else{
			if(SATKCmdResp( clientID, commId, result1, result2, &inText, itemId[id]) == TRUE)
			{
				_TRACE(("AT*MTRES cmd=%d", commId));
				_TRACE(("result1=%d", result1));
				_TRACE(("result2=%d", result2));
				_TRACE(("text len=%d", inText.len));
				_TRACE(("id=%d", id));
			}
			else
			{
				// response format incorrect
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
				return AT_STATUS_DONE;
			}
		}

		if(inText.string != NULL)
			OSHEAP_Delete(inText.string);

		if( (SATK_EVENTS_t)commId == SATK_EVENT_SELECT_ITEM ){
			// reset
			memset(itemId, 0, MAX_NUMBER_OF_ITEMS);
		}
	}
	else if( accessType == AT_ACCESS_READ )
	{
//		TProcessIdleModeTextReq();  // testing code
//		STKPRIM_TestDtmf();  // testing code

		// not supported
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
		return AT_STATUS_DONE;
	}
	else   // accessType == AT_ACCESS_TEST
	{
		sprintf((char*)rspBuf, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, rspBuf );
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATCmd_MEDL_Handler
//
// Description:		Process the AT*MEDL (STK Event Download) command.
//
//******************************************************************************
AT_Status_t ATCmd_MEDL_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1, const UInt8* _P2)
{
	Boolean status_ok = TRUE;

	if (accessType == AT_ACCESS_REGULAR)
	{
		switch ( atoi((const char *) _P1) )
		{
		case USER_ACTIVITY_EVENT:
			SATK_SendUserActivityEvent();
			break;

		case IDLE_SCREEN_AVAIL_EVENT:
			SATK_SendIdleScreenAvaiEvent();
			break;

		case LANGUAGE_SELECTION_EVENT:
			if ( (_P2 != NULL) && (_P2[0] != '\0') )
			{
				SATK_SendLangSelectEvent(atoi((const char *) _P2));
			}
			else
			{
				status_ok = FALSE;
			}

			break;

		case BROWSER_TERMINATION_EVENT:
			if ( (_P2 != NULL) && (_P2[0] != '\0') )
			{
				int i = atoi((const char *) _P2);

				if ( (i == 0) || (i == 1) )
				{
					SATK_SendBrowserTermEvent(i == 0);
				}
				else
				{
					status_ok = FALSE;
				}
			}
			else
			{
				status_ok = FALSE;
			}

			break;

		default:
			status_ok = FALSE;
			break;
		}
	}
	else if (accessType == AT_ACCESS_TEST)
	{
		AT_OutputStr(chan, (const UInt8 *) "*MEDL: (0-3)");
	}

	if (status_ok)
	{
		AT_CmdRspOK(chan);
	}
	else
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
	}

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATCmd_MTMENU_Handler
//
// Description:		Response command from TE back to SIM
//
// Notes:			Response to Set Up Menu command
//
//******************************************************************************

AT_Status_t ATCmd_MTMENU_Handler(AT_CmdId_t cmdId, UInt8 chan,
								 UInt8 accessType, const UInt8* _P1, const UInt8* _P2)
{
	UInt8			i;
	SATK_EVENTS_t	commId = SATK_EVENT_MENU_SELECTION;
	UInt8			rspBuf0[ AT_RSPBUF_SM ] ;
	SetupMenu_t*	pMenu = SATK_GetCachedRootMenuPtr();
	UInt16			clientID = AT_GetV24ClientIDFromMpxChan(chan);
	UInt8*			rspBuf = NULL;
	UInt8			help;
	Boolean			isUCS2 = FALSE;

	if ( accessType == AT_ACCESS_REGULAR)
	{
		if(pMenu == NULL){
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}
		else{
			if(_P2){
				// only 0 and 1 are supported: 0-no help; 1-request help for that menuID.
				help = (atoi((char*)_P2) > 0) ? 1 : 0;
			}
			else{
				help = 0;
			}

			// invoke function at stkapi.c to select menu from "SET UP MENU"
			if(!SATKCmdResp( clientID, commId, help, 0, NULL, pMenu->pItemIdList[atoi((char*)_P1)-1])){
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
				return AT_STATUS_DONE;
			}
		}
	}
	else if( accessType == AT_ACCESS_READ )
	{
		UInt8* str;

		if(pMenu == NULL){ // this is not a STK enabled SIM
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}

		str = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+1);

		rspBuf = (UInt8*)OSHEAP_Alloc(2*STK_MAX_ALPHA_TEXT_LEN+50);

		AT_StkStringPreProcessing(str, pMenu->title.string, pMenu->title.len, pMenu->title.unicode_type);

#ifdef ENABLE_ICON
		sprintf( (char*)rspBuf, "\r\n%s: \"%s\", %d, %d, %d, %d, %d, %d", "*MTSMENU", str,
			!IS_NON_UCS2_CODING(pMenu->title.unicode_type), pMenu->listSize,
			pMenu->isHelpAvailable,
			pMenu->titleIcon.IsExist,
			pMenu->titleIcon.IsSelfExplanatory,
			pMenu->titleIcon.Id);
#else
		sprintf( (char*)rspBuf, "\r\n%s: \"%s\", %d, %d", "*MTSMENU", str,
			!IS_NON_UCS2_CODING(pMenu->title.unicode_type), pMenu->listSize);
#endif

		AT_OutputStrNoCR (chan, rspBuf);

		for(i=0; i < pMenu->listSize; i++)
		{
			AT_StkStringPreProcessing(str, pMenu->pItemList[i].string, pMenu->pItemList[i].len, pMenu->pItemList[i].unicode_type);

			isUCS2 = !IS_NON_UCS2_CODING(pMenu->pItemList[i].unicode_type);

#ifdef ENABLE_ICON
			sprintf( (char*) rspBuf, "\n\r%d:\"%s\",%d,%d,%d,%d,%d,%d", i+1, str,
				pMenu->pIconList.IsExist, pMenu->pIconList.IsSelfExplanatory,
				pMenu->pIconList.Id[i], pMenu->pNextActIndList.IsExist, pMenu->pNextActIndList.Id[i],isUCS2 );
#else
			sprintf((char*) rspBuf, "\n\r%d:\"%s\",%d", i+1, str, isUCS2);
#endif

			AT_OutputStrNoCR (chan, rspBuf);
		}

		AT_OutputStrNoCR (chan, (UInt8 *) "\r\n");

		OSHEAP_Delete(str);
		OSHEAP_Delete(rspBuf);
	}
	else   // accessType == AT_ACCESS_TEST
	{
		sprintf((char*)rspBuf0, "%s: %s", AT_GetCmdName(cmdId), AT_GetValidParmStr(cmdId));
		AT_OutputStr (chan, rspBuf0);
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	AT_HandleDoubleQuoteStr
//
// Description:		replace '"' characters within the string with single quote, if any.
//
// Notes:			inStr will be modified!
//
//******************************************************************************
void AT_HandleDoubleQuoteStr(UInt8* inStr, UInt16 len)
{
	UInt16 i;

	for(i = 0; i < len; i++)
	{
		if(inStr[i] == 34){				// '"' double-quote character
			inStr[i] = 39;				// convert to single quote
		}
	}
}

//******************************************************************************
//
// Function Name:	AT_StkStringPreProcessing
//
// Description:		(1) convert string to hex if it is a UNICODE string
//					(2) replace double-quote char with single-quote char in the string, if any.
//
// Notes:
//
//******************************************************************************
void AT_StkStringPreProcessing(UInt8* outStr, UInt8* inStr,
								UInt16 len, Unicode_t codeType)
{
	if( !IS_NON_UCS2_CODING(codeType) )
	{
		UTIL_HexDataToHexStr(outStr, inStr, len);
	}
	else{
		// If the STK text is received as 7-bit GSM Default Alphabet, we need to convert the
		// text data according to the current character set (set by AT+CSCS command). If the data
		// is received as 8-bit, it is user-defined data (see Section 6.2.2 of GSM 03.38) and no
		// conversion should be performed.
		const UInt8 *convert_table = atc_GetGsmToTeConvTbl();
		UInt16 i;

		for(i = 0; i < len; i++)
		{
			if( (convert_table != NULL) && (codeType == UNICODE_GSM) )
			{
				/* Convert GSM alphabet to the current selected character set. Clear
				 * the most significant bit to be safe.
				 */
				outStr[i] = convert_table[inStr[i] & 0x7F];
			}
			else
			{
				outStr[i] = inStr[i];
			}
		}

		outStr[len] = '\0';
		AT_HandleDoubleQuoteStr(outStr, len);
	}
}



const UInt8* AtMapSTKCCFailIdToText(UInt8 failId){

	switch (failId){

		case STK_NOT_ALLOWED:
			return (const UInt8*)"Not Allow Call";
		case STK_STK_BUSY:
			return (const UInt8*)"STK Busy to Setup Call";
		case STK_SIM_ERROR:
			return (const UInt8*)"SIM Error to Setup Call";
		case STK_CALL_BUSY:
			return (const UInt8*)"No More Active Call";
		default:
			return (const UInt8*)"Unknown Fail";
	}
}


//The following for AT to handle STK tetx display
const UInt8* AtMapSTKCCResultIdToText(UInt8 resultId){

	switch (resultId){

		case CC_ALLOWED_NOT_MODIFIED:
			return (const UInt8*)"Call Allowed without Modification";
		case CC_NOT_ALLOWED:
			return (const UInt8*)"Call Not Allowed";
		case CC_ALLOWED_MODIFIED:
			return (const UInt8*)"Call Allowed with Modification";
		case CC_STK_BUSY:
			return (const UInt8*)"STK Busy to Setup Call";
		case CC_SIM_ERROR:
			return (const UInt8*)"SIM Error to Setup Call";
		default:
			return (const UInt8*)"Unknown Result";
	}
}


const UInt8* AtMapSTKCCTypeToText(UInt8 ccType){


	switch (ccType){

		case CALL_CONTROL_CS_TYPE:
			return (const UInt8*)"Normal Call";
		case CALL_CONTROL_SS_TYPE:
			return (const UInt8*)"Supplementary Service call";
		case CALL_CONTROL_USSD_TYPE:
			return (const UInt8*)"USSD Call";
		case CALL_CONTROL_MO_SMS_TYPE:
			return (const UInt8*)"MO SMS Call";
		case CALL_CONTROL_UNDEFINED_TYPE:
		default:
			return (const UInt8*)"Unknown Call Type";
	}


}


Result_t AtHandleSTKCCDisplayText(InterTaskMsg_t *inMsg){

	//Print out the stk cc display

	UInt8 atRes[AT_RSPBUF_SM+1];
	UInt8 chan;
	SATK_CallCntlResult_t resultType;
	StkCallSetupFailResult_t errType;

	StkCallControlDisplay_t* dispRes;

	dispRes = (StkCallControlDisplay_t*) inMsg->dataBuf;

	chan = AT_GetMpxChanFromV24ClientID(inMsg->clientID);

	// convert result types
	switch(dispRes->ccResult)
	{
		case CC_ALLOWED_NOT_MODIFIED:
			resultType = SATK_CC_RESULT_ALLOWED_NOT_MODIFIED;
			break;

		case CC_ALLOWED_MODIFIED:
			resultType = SATK_CC_RESULT_ALLOWED_MODIFIED;
			break;

		case CC_NOT_ALLOWED:
			resultType = SATK_CC_RESULT_NOT_ALLOWED;
			break;

		default:
			resultType = SATK_CC_RESULT_FAILED;
			break;
	}

	// convert error types
	if(resultType == SATK_CC_RESULT_FAILED)
	{
		switch(dispRes->ccResult)
		{
			case CC_STK_BUSY:
				errType = STK_STK_BUSY;
				break;

			case CC_SIM_ERROR:
				errType = STK_SIM_ERROR;
				break;

			default:
				errType = STK_NOT_ALLOWED;
				break;
		}
	}

	//Display the text targeting to the AT
	if ( chan != INVALID_MPX_CHAN ){

		if ( dispRes->oldCCType != dispRes->newCCType ){

			AT_CmdCloseTransaction(chan);

			if (V24_GetOperationMode((DLC_t)chan) == V24OPERMODE_NO_RECEIVE)
					V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );
		}

		if (dispRes->displayTextLen > 0){
			//Display the whatever the sim send to us
			dispRes->displayText[dispRes->displayTextLen] = '\0';

			if(resultType != SATK_CC_RESULT_FAILED){
				sprintf( (char*)atRes, "*MSTKCC: %d, %d, %d, \"%s\"",  resultType,
										dispRes->oldCCType, dispRes->newCCType, dispRes->displayText);
			}
			else{
				sprintf( (char*)atRes, "*MSTKCC: %d, %d, %d, \"%s\"",  resultType, errType,
										dispRes->oldCCType, dispRes->displayText);
			}
			AT_OutputStr(chan, atRes);
		} else {
			//construct the client display text
			if(resultType != SATK_CC_RESULT_FAILED){
				sprintf( (char*)atRes, "*MSTKCC: %d, %d, %d, \"Convert %s to %s\"",
												resultType, dispRes->oldCCType, dispRes->newCCType,
												AtMapSTKCCTypeToText(dispRes->oldCCType),
												AtMapSTKCCTypeToText(dispRes->newCCType));
			}
			else{
				sprintf( (char*)atRes, "*MSTKCC: %d, %d, %d, \"\"",  resultType, errType,
										dispRes->oldCCType);
			}
			AT_OutputStr(chan, atRes);
		 }

		if( (dispRes->ccResult != CC_ALLOWED_NOT_MODIFIED) && (dispRes->ccResult != CC_ALLOWED_MODIFIED) ){
			if( (dispRes->oldCCType == CALL_CONTROL_USSD_TYPE) || (dispRes->oldCCType == CALL_CONTROL_SS_TYPE) ){
				if (V24_GetOperationMode((DLC_t)chan) == V24OPERMODE_NO_RECEIVE)
					V24_SetOperationMode((DLC_t) chan, V24OPERMODE_AT_CMD );

				AT_CmdRspOK(chan);
			}
		}
	}

	return RESULT_OK;

}


//******************************************************************************
//
// Function Name:	ATCmd_MSTK_Handler
//
//******************************************************************************
AT_Status_t ATCmd_MSTK_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	char rspBuf[AT_RSPBUF_SM];

	switch(accessType)
	{
		case AT_ACCESS_REGULAR:
			MS_SetStkPlatformControl(atoi((char *) _P1) == 1);
			break;

		case AT_ACCESS_READ:
			sprintf(rspBuf, "%s: %d", AT_GetCmdName(AT_CMD_MSTK), (MS_IsStkPlatformControlSupported() ? 1 : 0));
			AT_OutputStr(chan, (UInt8 *) rspBuf);
			break;

		default:
			break;
	}

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}
