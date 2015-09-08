#ifndef _AT_PCH_H_
#define _AT_PCH_H_

#include "mstypes.h"
#include "taskmsgs.h"
#include "pchapi.h"

typedef enum
{
	AT_EQOS_TRAFFIC_CLASS_CONVERSATIONAL,
	AT_EQOS_TRAFFIC_CLASS_STREAMING,
	AT_EQOS_TRAFFIC_CLASS_INTERACTIVE,
	AT_EQOS_TRAFFIC_CLASS_BACKGROUND,
	AT_EQOS_TRAFFIC_CLASS_SUB
}AT_EqosTrafficClass_t;

typedef enum
{
	AT_EQOS_DELIVERY_ORDER_NO,
	AT_EQOS_DELIVERY_ORDER_YES,
	AT_EQOS_DELIVERY_ORDER_SUB
}AT_EqosDeliveryOrder_t;

typedef enum
{
	AT_EQOS_DELIVERY_ERR_SDU_NO,
	AT_EQOS_DELIVERY_ERR_SDU_YES,
	AT_EQOS_DELIVERY_ERR_SDU_NO_DETECT,
	AT_EQOS_DELIVERY_ERR_SDU_SUB
}AT_EqosDeliveryErrSDU_t;



void  ATC_SetContextDeactivation(UInt8 chan);
void AtHandleRegGsmGprsInd(InterTaskMsg_t *inMsg);
void AtHandleGPRSDeactivateInd(InterTaskMsg_t *InterMsg);
void AtHandleGPRSActivateInd(InterTaskMsg_t *InterMsg);
void AtHandleGPRSModifyInd(InterTaskMsg_t *InterMsg);
Result_t ATProcessCGATTCmd(InterTaskMsg_t* inMsg) ;
Result_t ATProcessCGACTCmd(InterTaskMsg_t* inMsg) ;

#endif // _AT_PCH_H_

