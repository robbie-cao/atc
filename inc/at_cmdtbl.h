#ifndef __AT_CMDTBL_H__
#define __AT_CMDTBL_H__

#include "at_api.h"

Result_t AT_ExecuteCmdHandler(
	UInt8			ch,
	UInt8			ctxt,
	AT_CmdId_t		cmdId,
	UInt8			accType,
	const AT_Cmd_t* cmd ) ;

void AT_SetCmdParmPtr( UInt8 ch, UInt8 ctxt, UInt8 parm, char * ptr ) ;

char* AT_GetCmdParmPtr( UInt8 ch, UInt8 ctxt, UInt8 parm ) ;

UInt8 AT_GetMaxNumParms( void ) ;

UInt16 AT_GetNumCmds( void ) ;

const AT_Cmd_t* AT_GetCmdTbl( void ) ;

const UInt8 * AT_GetRangeVal( void ) ;

const AT_HashBlk_t* AT_GetHashTbl( void ) ;

const UInt8* AT_GetRangeStr( UInt32 idx ) ;

#if defined SUPPORT_REDIRECT
AT_CmdId_t AT_CmdEnum2Idx( AT_CmdId_t enumVal ) ;
#endif

AT_CmdId_t AT_GetHashRedirect( AT_CmdId_t cmdId ) ;

#endif

