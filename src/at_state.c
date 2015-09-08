//=============================================================================
//
// File:			at_state.c
//
// Description:		AT module state.
//
// Note:
//
//==============================================================================

#include <stdio.h>

// #define ENABLE_LOGGING

#include "at_data.h"
#include "at_util.h"
#include "mti_trace.h"

#include "at_api.h"

//-------------------------------------------------
// Import Functions and Variables
//-------------------------------------------------


//-------------------------------------------------
// Local Definitions
//-------------------------------------------------

static AT_State_t atState ;

static AT_CmdId_t atImmedCmdByMpxCh[ MAX_NUM_MPX_CH ] ;

//----------------------------------------------------------------------
//
//-----------------------------------------------------------------------

void  AT_InitState(void)
{
	UInt8 ch ;

	for( ch=0; ch<MAX_NUM_MPX_CH; ch++ ) {
		AT_ResetState( ch ) ;
	}

	for( ch=0; ch<MAX_NUM_MPX_CH; ch++ ) {
		atState.transId[ch] = 0 ;
	}

	for( ch=0; ch<MAX_NUM_MPX_CH; ch++ ) {
		atImmedCmdByMpxCh[ch] = AT_INVALID_CMD_ID ;
	}
}

//---------------------------------------------------------------------------
// AT_SetCmdRspMpxChan(): set a long AT cmd MPX channel for later response
//---------------------------------------------------------------------------

void AT_SetCmdRspMpxChan(AT_CmdId_t cmdId, UInt8 ch)
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	atState.cmdId[ch] = cmdId ;
}

void AT_SetImmedCmdRspMpxChan( AT_CmdId_t cmdId, UInt8 ch )
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	atImmedCmdByMpxCh[ch] = cmdId ;
}

//---------------------------------------------------------------------------
// AT_GetCmdIdByMpxChan(): get the command ID by MPX channel
//---------------------------------------------------------------------------

AT_CmdId_t AT_GetCmdIdByMpxChan(UInt8 ch)
{
	if( AT_InvalidChan( ch ) ) {
		return AT_INVALID_CMD_ID ;
	}

	return atState.cmdId[ch] ;
}


//-----------------------------------------------------------------------------------
// AT_GetCmdRspMpxChan(): get MPX channel for sending a long AT cmd rsp back
//-----------------------------------------------------------------------------------

UInt8 AT_GetCmdRspMpxChan(AT_CmdId_t cmdId)
{
	UInt8 ch ;

	for( ch=0; ch<MAX_NUM_MPX_CH; ch++ ) {
		if( atState.cmdId[ch] == cmdId ) {
			return ch ;
		}
	}

	for( ch=0; ch<MAX_NUM_MPX_CH; ch++ ) {
		if( atImmedCmdByMpxCh[ch] == cmdId ) {
			return ch ;
		}
	}

	return INVALID_MPX_CHAN  ;
}

//-----------------------------------------------------------------------------------
// AT_ResetState():  reset AT parsing state variables for one channel
//-----------------------------------------------------------------------------------

void AT_ResetState( UInt8 ch )
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	atState.moreCmds[ch]	= FALSE ;
	atState.resPending[ch]  = FALSE ;
	atState.cmdId[ch]		= AT_INVALID_CMD_ID ;	// indicates idle
	atState.atCmd[ch]		= 0 ;
	atState.resDone[ch]		= FALSE ;
}

//-----------------------------------------------------------------------------------
// AT_GetMoreCmds():  set true if more concatenated AT commands are pending
//-----------------------------------------------------------------------------------

void AT_SetMoreCmds( UInt8 ch, Boolean moreCmds )
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	atState.moreCmds[ch] = moreCmds ;
}

//-----------------------------------------------------------------------------------
// AT_GetMoreCmds():  return true if more concatenated AT commands are pending
//-----------------------------------------------------------------------------------

Boolean AT_GetMoreCmds( UInt8 ch )
{
	if( AT_InvalidChan( ch ) ) {
		return FALSE ;
	}

	return atState.moreCmds[ch] ;
}

//-----------------------------------------------------------------------------------
// AT_GetResPending():  set true if long command result is pending
//-----------------------------------------------------------------------------------

void AT_SetResPending( UInt8 ch, Boolean resPending )
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	atState.resPending[ch] = resPending ;
}

//-----------------------------------------------------------------------------------
// AT_GetResPending():  return true if long command result is pending
//-----------------------------------------------------------------------------------

Boolean AT_GetResPending( UInt8 ch )
{
	if( AT_InvalidChan( ch ) ) {
		return FALSE ;
	}

	return atState.resPending [ch] ;
}

//-----------------------------------------------------------------------------------
// AT_SetNextCmdChar(): set pointer to next character in concatenated AT command string;
//				  used for parsing concatenated long commands
//-----------------------------------------------------------------------------------

void AT_SetNextCmdChar( UInt8 ch, const UInt8* nextByte )
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	atState.atCmd[ch] = nextByte ;
}

//-----------------------------------------------------------------------------------
// AT_GetNextCmdChar(): get pointer to next character in concatenated AT command string;
//				  used for parsing concatenated long commands
//-----------------------------------------------------------------------------------

const UInt8* AT_GetNextCmdChar( UInt8 ch )
{
	if( AT_InvalidChan( ch ) ) {
		return 0 ;
	}

	return atState.atCmd[ch] ;
}

//-----------------------------------------------------------------------------------
// AT_CmdInProgress(): return TRUE if long command in progress on mpx channel
//-----------------------------------------------------------------------------------

Boolean AT_CmdInProgress( UInt8 ch )
{
	if( AT_InvalidChan( ch ) ) {
		return FALSE ;
	}

	return((AT_GetResPending(ch)||AT_GetResDone(ch))) ;
}

//-----------------------------------------------------------------------------------
// AT_SetResDone():  set true if long command result is done
//-----------------------------------------------------------------------------------

void AT_SetResDone( UInt8 ch, Boolean resDone )
{

	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	atState.resDone[ch] = resDone ;
}

//-----------------------------------------------------------------------------------
// AT_GetResDone():  get long command result done flag
//-----------------------------------------------------------------------------------

Boolean AT_GetResDone( UInt8 ch )
{
	if( AT_InvalidChan( ch ) ) {
		return FALSE ;
	}

	return atState.resDone[ch] ;
}

UInt16 AT_GetTransactionID( UInt8 ch )
{
	return atState.transId[ ch ] ;
}

UInt16 AT_NewTransactionID( UInt8 ch )
{
	return ++atState.transId[ ch ] ;
}

/* ============= End of File: at_state.c ==================== */

