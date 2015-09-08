#ifndef _AT_SS_H_
#define _AT_SS_H_

#include "mti_build.h"
#include "at_api.h"
#include "resultcode.h"
#include "taskmsgs.h"


/* ATC Service Class values defined in GSM 07.07 */
typedef enum
{
	ATC_SERVICE_VOICE = 1,
	ATC_SERVICE_DATA = 2,
	ATC_SERVICE_FAX = 4,
	ATC_SERVICE_SMS = 8,
	ATC_SERVICE_DATA_CIRCUIT_SYNC = 16,
	ATC_SERVICE_DATA_CIRCUIT_ASYNC = 32,
	ATC_SERVICE_DPA = 64, /* Dedicated Packet Access */
	ATC_SERVICE_DPAD = 128, /* Dedicated PAD Access */
	ATC_SERVICE_INVALID = 255 /* Not defined in GSM 07.07, but used in our code */
} ATC_SERVICE_CLASS_t;


Boolean		AT_SoliciteCM(void);
Boolean		AT_CallMeterWarning(void);
AT_Status_t	AT_ProcessSuppSvcCallCmd( UInt8 chan, const UInt8* dialStr ) ;
void		AT_HandleUssdDataInd(InterTaskMsg_t *inMsg);
void		AT_HandleUssdDataRsp(InterTaskMsg_t *inMsg);
void		AT_HandleSetSuppSvcStatusRsp(InterTaskMsg_t *inMsg);
void		AT_HandleCallForwardStatusRsp(InterTaskMsg_t *inMsg);
void		AT_HandleProvisionStatusRsp(InterTaskMsg_t *inMsg);
void 		At_HandleSsNotificationInd(InterTaskMsg_t *inMsgPtr);
void		AT_HandleCallBarringStatusRsp(InterTaskMsg_t *inMsg);
void		AT_HandleCallWaitingStatusRsp(InterTaskMsg_t *inMsg);
void		AT_HandleSSCallIndexInd(InterTaskMsg_t *inMsg);
void		AT_HandleUssdSessionEndInd(InterTaskMsg_t *inMsg);
void		AT_HandleSsCallReqFail( InterTaskMsg_t* inMsg ) ;
void		AT_HandleIntParamSetIndMsg(InterTaskMsg_t* inMsgPtr);
#endif // _AT_SS_H_

