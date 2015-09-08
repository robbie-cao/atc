#ifndef AT_TST_H_
#define AT_TST_H_

#include "types.h"
#include "at_types.h"
#include "taskmsgs.h"


//******************************************************************************
// Function Prototypes
//******************************************************************************

AT_Status_t ATCmd_MeasReport_Handler(AT_CmdId_t		cmdId,
   									UInt8			chan,
   									UInt8			accessType,
   									const UInt8* 	_P1,
   									const UInt8* 	_P2 );

void At_HandleMeasReportParamInd(InterTaskMsg_t *inMsgPtr);

#endif  //AT_TST_H_

