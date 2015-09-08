#ifndef _AT_PLMN_H_
#define _AT_PLMN_H_

#include "mstypes.h"
#include "pchdef.h"
#include "taskmsgs.h"

Result_t	AtHandlePlmnListInd(InterTaskMsg_t* msg);

Result_t	AtHandleRegStatusInd(InterTaskMsg_t* msg);

void AtOutputCregStatus(MSRegState_t state, Boolean cell_ind, LACode_t lac, CellInfo_t cell_info);

void AtOutputCgregStatus(MSRegState_t state, Boolean cell_ind, LACode_t lac, CellInfo_t cell_info);

void AtOutputNetworkInfo(MSNetworkInfo_t netInfo);

#endif // _AT_PLMN_H_

