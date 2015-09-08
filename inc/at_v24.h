//-----------------------------------------------------------------------------
//	AT V24 definitions
//-----------------------------------------------------------------------------


#ifndef _AT_V24_H_
#define _AT_V24_H_

#include "types.h"
#include "mpx.h"		//	for MUXParam_t

extern void AT_ReportV24BreakSignalInd( UInt8 chan );
extern void AT_ReportV24ReleaseInd( UInt8 chan );
void AT_SwitchToATModeInd ( UInt8 chan );
extern void AT_SwitchToATModeInd( UInt8 chan );
extern void AT_ReportV24LineStateInd( UInt8 chan, Boolean DTR_ON );
extern void AT_ReportV24DataInd( UInt8 chan, UInt8 *p_cmd );
extern void AT_ReportStartMUXCnf( UInt8 chan, MUXParam_t *muxparam ) ;
extern void AT_ResetFClass(UInt8 classMode);
extern DLC_t AT_FindAutoAnswerDlc( void );
extern void AT_StopLogging (void);
extern void  AT_SetForDAIMode(void);

#endif // _AT_V24_H_

