//-----------------------------------------------------------------------------
//	AT Command table.  This module compiles the generated AT command table
//	files and provides functions to access them.
//-----------------------------------------------------------------------------

#include "at_api.h"
#include "at_api.gen.c"

static char* AtCmdParmPtr [MAX_NUM_MPX_CH][AT_N_CTXT][AT_MAX_NUM_PARMS] ;

Result_t AT_ExecuteCmdHandler(
	UInt8			ch,
	UInt8			ctxt,
	AT_CmdId_t		cmdId,
	UInt8			accType,
	const AT_Cmd_t* cmd )

{
	UInt8** cmdParmPtr = (UInt8**)AtCmdParmPtr[ch][ctxt] ;
#include "at_arglist.gen.c"
}

void AT_SetCmdParmPtr( UInt8 ch,UInt8 ctxt, UInt8 parm, char * ptr )
{
	AtCmdParmPtr[ch][ctxt][parm] = ptr ;
}

char* AT_GetCmdParmPtr( UInt8 ch, UInt8 ctxt, UInt8 parm )
{
	return AtCmdParmPtr[ch][ctxt][parm] ;
}

UInt8 AT_GetMaxNumParms( void )
{
	return AT_MAX_NUM_PARMS ;
}

UInt16 AT_GetNumCmds( void )
{
	return TOTAL_NUM_OF_AT_CMDS ;
}

const AT_Cmd_t* AT_GetCmdTbl( void )
{
	return atCmdTbl ;
}

const UInt8 * AT_GetRangeVal( void )
{
	return atRangeVal ;
}

const AT_HashBlk_t* AT_GetHashTbl( void )
{
	return atHashTbl ;
}

const UInt8* AT_GetRangeStr( UInt32 idx )
{
	return atRangeStr[idx] ;
}

AT_CmdId_t AT_GetHashRedirect( AT_CmdId_t cmdId )
{
	return atHashRedirect[ cmdId ] ;
}
