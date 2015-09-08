#ifndef _AT_CALL
#define _AT_CALL

#include "at_types.h"
#include "taskmsgs.h"
#include "callctrl.h"

#define MAX_CALLLOG_LEN		  100
#define MAX_RESPONSE_LEN	  100

typedef enum{

	CALLLOG_VOICE		= 1,
	CALLLOG_DATA		= 2,
	CALLLOG_FAX			= 4,
	CALLLOG_VIDEO		= 6, // FixMe - Arbitrary value for now

	CALLLOG_UNKNOWTY	= 9

} CallATLogType_t;

typedef enum{

	INCOMING_RING_TIMER,
	OUTGOING_CALL_TIMER,

	MS_TONE_TIMER,

	//It always is the last item
	DTMF_SIGNAL_TIMER

} CallTimerID_t;

typedef struct
{
	UInt8			chan;
	UInt16			errCode;
}ATCCmeError_t;

//MN Messgae handlers
Result_t AtHandleCallPreconnect(InterTaskMsg_t*inMsg);
Result_t AtHandleCallStatus(InterTaskMsg_t*inMsg);
Result_t AtHandleCallActionRes(InterTaskMsg_t*inMsg);
Result_t AtHandleCallRelease(InterTaskMsg_t*inMsg);
Result_t AtHandleCallConnect(InterTaskMsg_t*inMsg);
Result_t AtHandleCallIncoming(InterTaskMsg_t*inMsg);
Result_t AtHandleCallWaiting(InterTaskMsg_t*inMsg);
Result_t AtHandleDataCallStatus(InterTaskMsg_t*inMsg);
Result_t AtHandleDataEcdcLinkRsp(InterTaskMsg_t *inMsg);
Result_t AtHandleDataCallConnect(InterTaskMsg_t *inMsg);
Result_t AtHandleDataCallRelease(InterTaskMsg_t*inMsg);
Result_t AtHandleConnectedLineID(InterTaskMsg_t *inMsg);
Result_t AtHandleCallCCM(InterTaskMsg_t *inMsg);
Result_t AtHandleCallAOCStatus(InterTaskMsg_t *inMsg);
Result_t AtHandleSTKCCSetupFail(InterTaskMsg_t *inMsg);
Result_t AtHandleSTKCCDisplayText(InterTaskMsg_t *inMsg);

Result_t AtHandleCallTimer(InterTaskMsg_t *inMsg);
Result_t AtHandleCallAbort(InterTaskMsg_t *inMsg);
Result_t AtHandleEscDataCall(InterTaskMsg_t *inMsg);
Result_t AtProcessChldCmd(InterTaskMsg_t* inMsg);

void ATHandleApiClientCmdInd(InterTaskMsg_t* inMsgPtr);

void ATC_ReportCmeError(UInt8 dlc, UInt16 errCode);

//helper functions
Boolean IsDataCallActive(void);
UInt8	GetDataCallChannel(void);

#endif

