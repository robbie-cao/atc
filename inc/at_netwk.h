#ifndef _AT_NETWK_H_
#define _AT_NETWK_H_

#include "mstypes.h"
#include "taskmsgs.h"

Result_t AtHandleCipherIndication(InterTaskMsg_t* msg);
void	 AtHandleEfileInfoForPlmn(InterTaskMsg_t* inMsg);
void	 AtHandleEfileDataForPlmn(InterTaskMsg_t* inMsg);

#endif // _AT_NETWK_H_

