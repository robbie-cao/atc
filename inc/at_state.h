#ifndef _AT_STATE_H_
#define _AT_STATE_H_

#include "at_types.h"
#include "dlc.h"

typedef struct {
	Boolean			moreCmds	[MAX_NUM_MPX_CH] ;	//	true if more commands in string
	Boolean			resPending	[MAX_NUM_MPX_CH] ;	//	true if results pending on long cmd
	AT_CmdId_t		cmdId		[MAX_NUM_MPX_CH] ;	//	command ID being processed
													//	set to TOTAL_NUM_OF_AT_CMDS if
													//	channel is idle
	const UInt8*	atCmd		[MAX_NUM_MPX_CH] ;	//	ptr to next char in cmd str
	Boolean			resDone		[MAX_NUM_MPX_CH] ;	//	true if long results done
	UInt16			transId		[MAX_NUM_MPX_CH] ;	//	transaction ID
}	AT_State_t ;

extern void				AT_InitState			( void ) ;

extern void				AT_SetMoreCmds ( UInt8 chan, Boolean moreCmds ) ;
extern Boolean			AT_GetMoreCmds ( UInt8 chan ) ;

extern void				AT_SetResPending ( UInt8 chan, Boolean resPending )  ;
extern Boolean			AT_GetResPending ( UInt8 chan ) ;

extern void				AT_SetNextCmdChar ( UInt8 chan, const UInt8* nextByte ) ;
extern const UInt8*		AT_GetNextCmdChar ( UInt8 chan ) ;

extern Boolean			AT_CmdInProgress ( UInt8 chan ) ;

extern void				AT_SetCmdRspMpxChan(AT_CmdId_t cmdId, UInt8 ch);
extern void				AT_SetImmedCmdRspMpxChan( AT_CmdId_t cmdId, UInt8 ch ) ;

extern AT_CmdId_t		AT_GetCmdIdByMpxChan(UInt8 ch);
extern UInt8			AT_GetCmdRspMpxChan(AT_CmdId_t cmdId);

extern void				AT_ResetState ( UInt8 chan ) ;

extern void				AT_SetResDone ( UInt8 chan, Boolean resDone ) ;
extern Boolean			AT_GetResDone ( UInt8 chan ) ;

extern UInt16			AT_GetTransactionID( UInt8 ch );

extern UInt16			AT_NewTransactionID( UInt8 ch );


#endif // _AT_STATE_H_

