/****************************************************************************
*   @file   at_stk.h
*
*   @brief  This file contains prototypes for STK functions at the AT interface level.
*
****************************************************************************/
#ifndef _ATC_STK_H_
#define _ATC_STK_H_

#include "consts.h"
#include "types.h"
#include "mstypes.h"
#include "stkapi.h"
#include "stk.h"

//-------------------------------------------------
// Constant Definitions
//-------------------------------------------------


//-------------------------------------------------
// Type Definitions
//-------------------------------------------------

typedef enum
{
	CALL_SETUP_NOT_CURRENTLY_BUSY,
	CALL_SETUP_NOT_CURRENTLY_BUSY_W_REDIAL,
	CALL_SETUP_PUT_OTHER_CALL_ONHOLD,
	CALL_SETUP_PUT_OTHER_CALL_ONHOLD_W_REDIAL,
	CALL_SETUP_DISCONNECT_ALL_OTHER_CALLS,
	CALL_SETUP_DISCONNECT_ALL_OTHER_CALLS_W_REDIAL
} SATK_SIMCallSetup_t;

typedef enum
{
	TIME_UNIT_MIN,
	TIME_UNIT_SEC,
	TIME_UNIT_TENTH_SEC
} SATK_TimeUnit_t;

typedef enum
{
	SATK_CC_RESULT_NONE,
	SATK_CC_RESULT_ALLOWED_NOT_MODIFIED,
	SATK_CC_RESULT_ALLOWED_MODIFIED,
	SATK_CC_RESULT_NOT_ALLOWED,
	SATK_CC_RESULT_FAILED
} SATK_CallCntlResult_t;

//-------------------------------------------------
// Data Structure
//-------------------------------------------------


//-------------------------------------------------
// Function Prototype
//-------------------------------------------------

void AT_HandleSTKDisplayTextInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKLaunchBrowserReq(InterTaskMsg_t* inMsg);
void AT_HandleSTKGetInkeyInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKGetInputInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKPlayToneInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKSelectItemInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKRefreshInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKSetupEventList(InterTaskMsg_t* inMsg);
void AT_HandleSTKSmsSentInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKSsSentInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKUssdSentInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKCallSetupInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKSendDtmfInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKSetupMenuInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKIdleModeTextInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKDataServInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKRunAtCmdInd(InterTaskMsg_t* inMsg);
void AT_HandleSTKLangNotificationInd(InterTaskMsg_t *inMsg);
const UInt8* AtMapSTKCCResultIdToText(UInt8 resultId);
const UInt8* AtMapSTKCCTypeToText(UInt8 ccType);
Result_t AtHandleSTKCCDisplayText(InterTaskMsg_t *inMsg);

const UInt8* AtMapSTKCCFailIdToText(UInt8 failId);



#endif  // _ATC_STK_H_

