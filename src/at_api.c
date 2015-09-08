//-----------------------------------------------------------------------------
//	AT API functions.
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#define ENABLE_LOGGING

#include "mti_trace.h"
#include "at_cfg.h"
#include "ms_database.h"
#include "dlc.h"
#include "ss_api.h"
#include "at_api.h"
#include "osheap.h"
#include "util.h"
#include "timer.h"

#ifdef _WIN32
#undef MSG_LOG
#define MSG_LOG( x ) printf( "%s\n", x )
#endif

//-----------------------------------------------------------------------------
//	Include the generated command and hash tables
//-----------------------------------------------------------------------------

//
//	Buffer parameters extracted from command line.  The parameters will be
//	concatenated into 'AtCmdParmBuf'.  Unlike a 'normal' strncat, each
//	parameter is null-terminated.
//
static char		AtCmdParmBuf	[MAX_NUM_MPX_CH][AT_N_CTXT][AT_PARM_BUFFER_LEN] ;
static char		AtCmdBuf		[MAX_NUM_MPX_CH][AT_PARM_BUFFER_LEN] ;

//
//	Buffer for logging
//
static char		AtCmdLogBuf		[MAX_NUM_MPX_CH][AT_N_CTXT][AT_RSPBUF_SM];

//
//	Supports buffered response output
//
#define AT_OUTBUF_SHORT	10	//	largest string not stored in heap
#define AT_OUTBUF_NBUF	16	//	number of buffered strings

//
//	AT Command List Mode
//
static int		atCmdListMode = FALSE;

//
//	Buffered response structure
//
typedef struct {
	UInt8		ch ;						//	channel
	UInt32		seq ;						//	sequence
	UInt8		smBuf [ AT_OUTBUF_SHORT ] ;	//	short-string buffer
	UInt8*		lgBuf ;						//	long-string allocated memory
}	RspBuf_t ;

//
//	Buffered response status
//
typedef struct {
	Boolean		init ;						//	TRUE if structures initialized
	UInt32		seq ;						//	next sequence number
}	RspBufStat_t ;

//
//	Buffered response structure realization
//
static RspBuf_t			RspBuf[ AT_OUTBUF_NBUF ] ;
static RspBufStat_t		RspBufStat = { FALSE, 0 } ;

//
//	"Really safe" string concatenation; 'len' is the length of the allocated buffer
//
#define AT_STRNCAT( dst, src, len ) strncat( (char*)dst, (char*)src, len-strlen((char*)dst )-1)

//-----------------------------------------------------------------------------
//	                 P R I V A T E   F U N C T I O N S
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//	                 Immediate command support
//-----------------------------------------------------------------------------

static AT_CmdId_t ImmediateCtxtCmds[] = {
	AT_CMD_MVTS,
	AT_CMD_VTS,
	AT_CMD_MPCM,
	AT_INVALID_CMD_ID		// end list
} ;

typedef struct {
	Boolean		cmdInProgress[MAX_NUM_MPX_CH] ;
	AT_CmdId_t	cmdId[MAX_NUM_MPX_CH] ;
}	ImmedCmdStatus_t ;

static ImmedCmdStatus_t immedCmdStatus = { 0 } ;

//
//	Get command context
//
static UInt8 GetCtxt( AT_CmdId_t cmdId )
{
	AT_CmdId_t * cmd = ImmediateCtxtCmds ;
	while( AT_INVALID_CMD_ID != *cmd ) {
		if( cmdId == *cmd ) {
			return AT_CTXT_IMMED ;
		}
		++cmd ;
	}
	return AT_CTXT_NORMAL ;
}

//
//	Set immediate command status
//
static void SetImmedCmdStatus( UInt8 ch, AT_CmdId_t cmdId, Boolean inProgress )
{
	if( !inProgress ) {
		cmdId = AT_INVALID_CMD_ID ;
	}

	immedCmdStatus.cmdInProgress[ch] = inProgress ;
	immedCmdStatus.cmdId[ch] = cmdId ;
	AT_SetImmedCmdRspMpxChan( cmdId, ch ) ;
}

//
//	Get immediate command ID
//
static AT_CmdId_t GetImmedCmd( UInt8 ch )
{
	if( immedCmdStatus.cmdInProgress[ch] ) {
		return immedCmdStatus.cmdId[ch] ;
	}
	return AT_INVALID_CMD_ID ;
}

//
//	Handle error response based on normal or immediate context
//
static void CmdRspError( UInt8 ch, UInt8 ctxt, UInt16 errCode )
{
	if( AT_CTXT_NORMAL == ctxt ) {
		AT_CmdRspError( ch, errCode ) ;
	}
	else {
		AT_ImmedCmdRspError( ch, AT_INVALID_CMD_ID, AT_ERROR_RSP ) ;
	}
}


//-----------------------------------------------------------------------------
//	ConvertATParmToUInt
//-----------------------------------------------------------------------------
//	Converts a string to an integer and reports error.
//-----------------------------------------------------------------------------

static Result_t ConvertATParmToUInt(const UInt8* str, UInt32* retVal )
{
	UInt32 v = 0 ;
	char c;

	if(!*str) {
		return RESULT_ERROR ;
	}

	for( ;; ) {
		c = *str++ ;
		if( !c ) break ;
		if( c >= '0' && c <= '9' ) {
			v *= 10 ;
			v += c - '0' ;
		}
		else {
			return RESULT_ERROR ;
		}
	}

	*retVal = v ;

	return RESULT_OK ;
}

//-----------------------------------------------------------------------------
//	Truncate
//-----------------------------------------------------------------------------
//	Truncate string 'str' at first instance of any character in 'list'
//-----------------------------------------------------------------------------
static void Truncate( char * str, char *list )
{
	while( *list ) {
		char *cp = strchr( str, *list++ ) ;
		if( cp ) *cp = 0 ;
	}
}


//-----------------------------------------------------------------------------
//	FindHashedStr
//-----------------------------------------------------------------------------
//	Search the command name hash table to find the closest match to a
//	command string
//-----------------------------------------------------------------------------

//
//	To get useful hashtable search on windows simulation, #define DEBUG_HASH
#ifdef _WIN32
// #define DEBUG_HASH
#endif

static Result_t FindHashedStr(
	const AT_Cmd_t*		cmd,
	const char*			cmdStr,
	AT_CmdId_t*			cmdIdx )
{
	AT_CmdId_t		rowIdx ;
	AT_CmdId_t		colIdx ;
	AT_CmdId_t		htIdx ;
	Result_t		result ;
	const AT_HashBlk_t* atHashTbl = AT_GetHashTbl() ;

	result = RESULT_ERROR ;	//	switch to RESULT_OK when match found

	rowIdx = 0 ;
	colIdx = 0 ;
	htIdx  = 0 ;

	for( ;; ) {

		char c1 = toupper( cmdStr[colIdx] ) ;
		char c2 = toupper( cmd[AT_GetHashRedirect( rowIdx )].cmdName[colIdx] ) ;

		if( c1 == c2 ) {

			result = RESULT_OK ;
			*cmdIdx = AT_GetHashRedirect( rowIdx ) ;
#ifdef DEBUG_HASH
			printf("Hash match col %d row %d char %c\n", colIdx, rowIdx, c1 ) ;
#endif
			if(( atHashTbl[htIdx].nextBlock & ~AT_ENDHASH ) == 0 ) {
#ifdef DEBUG_HASH
				printf("Hash return RESULT_OK at col %d row %d char %c\n", colIdx, rowIdx, c1 ) ;
#endif
				return RESULT_OK ;
			}
			++colIdx ;
			++htIdx ;
			continue ;
		}

		if( atHashTbl[htIdx].nextBlock & AT_ENDHASH ) {
#ifdef DEBUG_HASH
			printf("Hash return %d(ENDHASH) at col %d row %d\n", atStatus, colIdx, rowIdx ) ;
#endif
			return result ;
		}

		htIdx += 1 +( atHashTbl[htIdx].nextBlock & ~AT_ENDHASH ) ;
		rowIdx = atHashTbl[htIdx].firstRow ;
	}
}

//-----------------------------------------------------------------------------
//	GetATCmd
//-----------------------------------------------------------------------------
//	Returns a pointer to the AT command structure for this cmdId.  Returns
//	NULL if invalid cmdId.
//-----------------------------------------------------------------------------

static const AT_Cmd_t* GetATCmd( AT_CmdId_t cmdId )
{
	const AT_Cmd_t *atCmdTbl = AT_GetCmdTbl() ;

	if( cmdId >= AT_GetNumCmds() ) return 0 ;
	return &atCmdTbl[cmdId] ;
}

//-----------------------------------------------------------------------------
//	InitParms
//-----------------------------------------------------------------------------
//	Initialize the parameter list for an AT handler call.
//-----------------------------------------------------------------------------

static void InitParms( UInt8 ch, UInt8 ctxt ) {
	UInt8 i ;
	for( i=0; i<AT_GetMaxNumParms(); i++ ) {
		AT_SetCmdParmPtr(ch, ctxt, i, 0 ) ;
	}
	AtCmdParmBuf[ch][ctxt][0] = 0 ;
}

//
//	Copy command parameter to the command buffer..Unlike a 'normal' strncat, each
//	parameter in the buffer is null-terminated.
//
static Result_t CopyTokenToCmdBuf( ParserToken_t* token, char* cmdBuf, int parmNum, char** bufPtr )
{
	char*	cmdBufPtr = cmdBuf ;
	int		bytesUsed = 0 ;
	int		i ;

	for( i=0; i<parmNum; i++ ) {
		int len = 1 + strlen( cmdBufPtr ) ;
		bytesUsed += len ;
		cmdBufPtr += len ;
	}

	if( RESULT_OK != ParserTknToStr( token, cmdBufPtr, AT_PARM_BUFFER_LEN - bytesUsed ) ) {
		return RESULT_ERROR ;
	}

	*bufPtr = cmdBufPtr ;

	return RESULT_OK ;
}


//
//	Unpack range-comparison value from UInt8 array
//
static UInt32 Unpack( UInt8 rangeType, Int32* rangeIdx )
{
	UInt32	v = 0 ;
	UInt8	i, n = 0 ;
	const UInt8 *atRangeVal = AT_GetRangeVal() ;

	switch( rangeType ) {
	case AT_RANGE_SCALAR_UINT32:
	case AT_RANGE_MINMAX_UINT32:
	case AT_RANGE_STRING:
		n=4 ;
		break ;
	case AT_RANGE_SCALAR_UINT16:
	case AT_RANGE_MINMAX_UINT16:
		n=2;
		break;
	case AT_RANGE_SCALAR_UINT8:
	case AT_RANGE_MINMAX_UINT8:
		n=1;
		break;
	}
	for( i=0; i<n; i++ ) {
		v <<= 8 ;
		v |= atRangeVal [*rangeIdx] ;
		*rangeIdx += 1 ;
	}
	return v ;
}

//-----------------------------------------------------------------------------
//	CheckParams
//-----------------------------------------------------------------------------
//	Checks command parameter values.
//-----------------------------------------------------------------------------

static Result_t CheckParams(
	AT_CmdId_t		cmdId,
	UInt8			ch,
	UInt8			ctxt )
{
	Int32			rangeIdx ;
	UInt8			rangeType ;
	UInt8			i ;
	UInt32			vParm ;
	UInt32			vMin ;
	UInt32			vMax ;
	Boolean			valid ;
	UInt32			strIdx ;
	Boolean			check ;

	const AT_Cmd_t *atCmdTbl = AT_GetCmdTbl() ;
	const AT_Cmd_t*	cmd = GetATCmd( cmdId ) ;
	const UInt8* atRangeVal = AT_GetRangeVal() ;

	if(( rangeIdx = atCmdTbl[cmdId].rangeIdx ) >= 0 ) {

		for( i=0; i<cmd->maxParms; i++ ) {
			UInt8 *parmPtr = (UInt8*)AT_GetCmdParmPtr( ch, ctxt, i ) ;

			check = parmPtr ? TRUE : FALSE ;
			valid = check    ? FALSE : TRUE ;

			while( AT_RANGE_ENDRANGE !=( rangeType = atRangeVal[rangeIdx++] )) {

				switch( rangeType ) {

				case AT_RANGE_NOCHECK:
					valid = TRUE ;
					break ;

				case AT_RANGE_SCALAR_UINT32:
				case AT_RANGE_SCALAR_UINT16:
				case AT_RANGE_SCALAR_UINT8:

					vMin = Unpack( rangeType, &rangeIdx ) ;

					if( check ) {
						if( RESULT_OK != ConvertATParmToUInt( parmPtr, &vParm ) ) {
							return RESULT_ERROR ;
						}

						if( vMin == vParm ) {
							valid = TRUE ;
						}
					}

					break ;

				case AT_RANGE_MINMAX_UINT32:
				case AT_RANGE_MINMAX_UINT16:
				case AT_RANGE_MINMAX_UINT8:

					vMin = Unpack( rangeType, &rangeIdx ) ;
					vMax = Unpack( rangeType, &rangeIdx ) ;

					if( check ) {
						if( parmPtr == NULL ) continue ;

						if( RESULT_OK != ConvertATParmToUInt( parmPtr, &vParm ) ) {
							return RESULT_ERROR ;
						}

						if(( vParm >= vMin ) &&( vParm <= vMax ) ) {
							valid = TRUE ;
						}
					}

					break ;

				case AT_RANGE_STRING:

					strIdx = Unpack( rangeType, &rangeIdx ) ;

					if( check ) {
						if( !UTIL_StrCmp_Nocase(
							(char*)AT_GetRangeStr( strIdx ),
							(char*)parmPtr ) ) {
							valid = TRUE ;
						}
					}

					break ;
				}
			}

			if( !valid ) {
				return RESULT_ERROR ;
			}
		}
	}

	return RESULT_OK ;
}

//-----------------------------------------------------------------------------
//	Log command and parameters
//-----------------------------------------------------------------------------

#ifdef ENABLE_LOGGING
static void LogCmd( UInt8 ch, UInt8 ctxt, const AT_Cmd_t* cmd, UInt8 accType )
{
	UInt8*	rspBuf = (UInt8*)AtCmdLogBuf[ch][ctxt] ;
	UInt16	rspBufLen = AT_RSPBUF_SM ;

	UInt8	p ;

	sprintf( (char*)rspBuf, "[%d] AT", ch ) ;

	AT_STRNCAT( (char*)rspBuf, (char*)cmd->cmdName, rspBufLen ) ;

	switch( accType ) {

	case AT_ACCESS_READ:
		AT_STRNCAT( (char*)rspBuf, "?", rspBufLen ) ;
		break ;

	case AT_ACCESS_TEST:
		AT_STRNCAT( (char*)rspBuf, "=?", rspBufLen ) ;
		break ;

	default:
		if( AT_CMDTYPE_EXTENDED == cmd->cmdType ) {
			if( cmd->maxParms > 0 ) {
				AT_STRNCAT( (char*)rspBuf, "=", rspBufLen ) ;

			}
		}

		for( p=0; p<cmd->maxParms; p++ ) {
			char * z = AT_GetCmdParmPtr(ch, ctxt, p);
			AT_STRNCAT( (char*)rspBuf, z?z:"", rspBufLen ) ;
			if( p<cmd->maxParms-1 ) {
				AT_STRNCAT( (char*)rspBuf, ",", rspBufLen ) ;
			}
		}

	}

	AT_STRNCAT( (char*)rspBuf, "\r\n", rspBufLen ) ;

	MSG_LOG( rspBuf ) ;

}
#endif

//-----------------------------------------------------------------------------
//	Call command handler
//-----------------------------------------------------------------------------

static AT_Status_t ExecCmd(
	AT_CmdId_t	cmdId,
	UInt8		ch,
	UInt8		ctxt,
	UInt8		accType )
{
	const AT_Cmd_t*	cmd = GetATCmd( cmdId ) ;
	SIM_PIN_Status_t pin_status;

#ifdef ENABLE_LOGGING
	LogCmd( ch, ctxt, cmd, accType ) ;
#endif

	if( AT_CTXT_NORMAL == ctxt ) {
		AT_SetCmdRspMpxChan( cmdId, ch ) ;
	}
	else {
		AT_SetImmedCmdRspMpxChan( cmdId, ch ) ;
	}

	/* Busy-wait for the SIM PIN initialization to finish before processing the
     * AT command that needs to check SIM PIN status.
	 */

	pin_status = SIM_GetPinStatus( ) ;

	if( NO_SIM_PIN_STATUS == pin_status ) {
		if( cmd->options & AT_OPTION_WAIT_PIN_INIT ) {
			UInt8 count = 0 ;
			do {
				OSTASK_Sleep( TICKS_ONE_SECOND / 5 ) ;
				if( ++count >= 25 ) {		//	wait up to 5 seconds
					CmdRspError( ch, ctxt, AT_ERROR_RSP );
					return AT_STATUS_DONE;
				}
			} while( NO_SIM_PIN_STATUS == ( pin_status = SIM_GetPinStatus( ) ) ) ;
		}
	}

	/* Check if SIM PIN status allows the command to be executed */
	if( (cmd->options & AT_OPTION_NO_PIN) == 0 )
	{
		Boolean errorFlag = TRUE;
		UInt16  errorCode;

		switch(pin_status)
		{
		case SIM_PIN_STATUS:
			errorCode = AT_CME_ERR_SIM_PIN_REQD;
			break;

		case SIM_PUK_STATUS:
			errorCode = AT_CME_ERR_SIM_PUK_REQD;
			break;

		case PH_SIM_PIN_STATUS:
		case PH_SIM_PUK_STATUS:
			errorCode = AT_CME_ERR_PH_SIM_PIN_REQD;
			break;

		case PH_NET_PIN_STATUS:
			errorCode = AT_CME_ERR_NETWORK_LOCK_PIN_REQD;
			break;

		case PH_NET_PUK_STATUS:
			errorCode = AT_CME_ERR_NETWORK_LOCK_PUK_REQD;
			break;

		case PH_NETSUB_PIN_STATUS:
			errorCode = AT_CME_ERR_SUBNET_LOCK_PIN_REQD;
			break;

		case PH_NETSUB_PUK_STATUS:
			errorCode = AT_CME_ERR_SUBNET_LOCK_PUK_REQD;
			break;

		case PH_SP_PIN_STATUS:
			errorCode = AT_CME_ERR_SP_LOCK_PIN_REQD;
			break;

		case PH_SP_PUK_STATUS:
			errorCode = AT_CME_ERR_SP_LOCK_PUK_REQD;
			break;

		case PH_CORP_PIN_STATUS:
			errorCode = AT_CME_ERR_CORP_LOCK_PIN_REQD;
			break;

		case PH_CORP_PUK_STATUS:
			errorCode = AT_CME_ERR_CORP_LOCK_PUK_REQD;
			break;

		case SIM_ABSENT_STATUS:
			errorCode = AT_CME_ERR_PH_SIM_NOT_INSERTED;
			break;

		case NO_SIM_PIN_STATUS:
			errorCode = AT_ERROR_RSP;
			break ;

		default:
			errorFlag = FALSE;
			break;
		}

		if(errorFlag)
		{
			CmdRspError(ch, ctxt, errorCode);
			return AT_STATUS_DONE;
		}
	}


	/* Check if system state allows the command to be executed */
	if( (SYS_GetSystemState() == SYSTEM_STATE_OFF) && ((cmd->options & AT_OPTION_NO_PWR) == 0) )
	{
		CmdRspError(ch,ctxt,AT_ERROR_RSP);
		return AT_STATUS_DONE;
	}

	//
	//	if accessType is 'test' the default is to output the command
	//	name and the valid-parameters string.  If the USER_DEFINED_TEST_RSP
	//	flag has been set the command's handler is called to handle the
	//	test command.  If the flag is not set, and no string has been
	//	specified, return ERROR
	//
	if( AT_ACCESS_TEST == accType ) {

		if ( ! ( cmd->options & AT_OPTION_USER_DEFINED_TEST_RSP ) ) {

			if ( !cmd->validParms ) {
				CmdRspError(ch,ctxt,AT_ERROR_RSP) ;
				return AT_STATUS_DONE ;
			}

			else {
				UInt8 outBuf[AT_RSPBUF_SM] ;
				outBuf[0] = 0 ;

				AT_STRNCAT( outBuf, AT_GetCmdName( cmdId ), AT_RSPBUF_SM ) ;
				AT_STRNCAT( outBuf, ": ", AT_RSPBUF_SM ) ;
				AT_STRNCAT( outBuf, AT_GetValidParmStr( cmdId ), AT_RSPBUF_SM ) ;

				AT_OutputStr( ch, outBuf ) ;

				AT_CmdRspOK(ch) ;

				return AT_STATUS_DONE ;
			}
		}
	}

	return AT_ExecuteCmdHandler( ch, ctxt, cmdId, accType, cmd ) ;
}

//-----------------------------------------------------------------------------
//	HandleBasicCmd
//-----------------------------------------------------------------------------
//	Handle a BASIC command.
//-----------------------------------------------------------------------------

static Result_t HandleBasicCmd(
	AT_CmdId_t		cmdId,
	UInt8			ch,
	UInt8			ctxt,
	ParserMatch_t*		match,
	AT_Status_t*	atStatus )
{
	ParserToken_t	token ;
	UInt8		nParms = 0 ;
	Boolean		moreCmds ;
	const AT_Cmd_t* cmd = GetATCmd( cmdId ) ;
	char* bufPtr ;
	char* cmdParmBuf = AtCmdParmBuf[ch][ctxt] ;

	InitParms( ch, ctxt ) ;

	//
	//	cmd[ ]?
	//
	if( RESULT_OK == ParserMatch( "~s*?", match, 0 ) ) {

		ParserMatch( "~s*~c;*~s*", match, 0 ) ;

		moreCmds =( RESULT_OK != ParserMatch( "$", match, 0 ) ) ;

		if( ( moreCmds ) && ( AT_CTXT_IMMED == ctxt ) ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		AT_SetMoreCmds( ch, moreCmds ) ;

		*atStatus = ExecCmd( cmdId, ch, ctxt, AT_ACCESS_READ ) ;

		return RESULT_OK ;
	}

	//
	//	cmd[ ]=[ ]?
	//
	if( RESULT_OK == ParserMatch( "~s*=~s*?", match, 0 ) ) {

		ParserMatch( "~s*~c;*~s*", match, 0 ) ;

		moreCmds =( RESULT_OK != ParserMatch( "$", match, 0 ) ) ;

		if( ( moreCmds ) && ( AT_CTXT_IMMED == ctxt ) ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		AT_SetMoreCmds( ch, moreCmds ) ;

		*atStatus = ExecCmd( cmdId, ch, ctxt, AT_ACCESS_TEST ) ;

		return RESULT_OK ;
	}

	//
	//	cmd[ ][=][ ]n |
	//
	if( RESULT_OK == ParserMatch( "~s*=", match, 0 ) ) {
		if( RESULT_OK == ParserMatch( "~s*$", match, 0 ) ) {
			CmdRspError( ch,ctxt,AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}
	}

	if( RESULT_OK == ParserMatch( "~s*(~d+)", match, &token ) ) {
		if( RESULT_OK != CopyTokenToCmdBuf( &token, cmdParmBuf, nParms, &bufPtr ) ) {
			CmdRspError( ch,ctxt,AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}
		AT_SetCmdParmPtr( ch, ctxt, nParms, bufPtr ) ;
		++nParms ;

	}

	if( nParms < cmd->minParms ) {
		CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
		return RESULT_ERROR ;
	}

	if( nParms > cmd->maxParms ) {
		CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
		return RESULT_ERROR ;
	}


	if( RESULT_OK != CheckParams( cmdId, ch, ctxt ) ) {
		CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
		return RESULT_ERROR ;
	}

	ParserMatch( "~s*~c;*~s*", match, 0 ) ;	//	ignore ';'

	moreCmds =( RESULT_OK != ParserMatch( "$", match, 0 ) ) ;

	if( ( moreCmds ) && ( AT_CTXT_IMMED == ctxt ) ) {
		CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
		return RESULT_ERROR ;
	}

	AT_SetMoreCmds( ch, moreCmds ) ;

	*atStatus = ExecCmd( cmdId, ch, ctxt, AT_ACCESS_REGULAR ) ;

	return RESULT_OK ;
}

//-----------------------------------------------------------------------------
//	HandleExtendedCmd
//-----------------------------------------------------------------------------
//	Handle an EXTENDED command.
//-----------------------------------------------------------------------------

static Result_t HandleExtendedCmd(
	AT_CmdId_t		cmdId,
	UInt8			ch,
	UInt8			ctxt,
	ParserMatch_t*		match,
	AT_Status_t*	atStatus )
{
	ParserToken_t	token ;
	UInt8		nParms = 0 ;
	const AT_Cmd_t *atCmdTbl = AT_GetCmdTbl() ;
	const AT_Cmd_t* cmd = &atCmdTbl[cmdId] ;
	Boolean moreCmds ;
	char* bufPtr ;
	char* cmdParmBuf = AtCmdParmBuf[ch][ctxt] ;

	InitParms( ch, ctxt ) ;

	//
	//	cmd[ ]?
	//
	if( RESULT_OK == ParserMatch( "~s*?", match, 0 ) ) {

		ParserMatch( "~s*;", match, 0 ) ;
		moreCmds =( RESULT_OK != ParserMatch( "~s*$", match, 0 ) ) ;

		if( ( moreCmds ) && ( AT_CTXT_IMMED == ctxt ) ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		AT_SetMoreCmds( ch, moreCmds ) ;

		*atStatus = ExecCmd( cmdId, ch, ctxt, AT_ACCESS_READ ) ;

		return RESULT_OK ;
	}

	//
	//	cmd[ ]=[ ]?
	//
	if( RESULT_OK == ParserMatch( "~s*=~s*?", match, 0 ) ) {

		ParserMatch( "~s*;", match, 0 ) ;
		moreCmds =( RESULT_OK != ParserMatch( "~s*$", match, 0 ) ) ;

		if( ( moreCmds ) && ( AT_CTXT_IMMED == ctxt ) ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		AT_SetMoreCmds( ch, moreCmds ) ;

		*atStatus = ExecCmd( cmdId, ch, ctxt, AT_ACCESS_TEST ) ;

		return RESULT_OK ;
	}

	//
	//	cmd[ ]=[ ]p1[ ][,...]
	//
	if( RESULT_OK == ParserMatch( "~s*=", match, 0 ) ) {

		if( RESULT_OK == ParserMatch( "~s*$", match, 0 ) ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		for( ;; ) {

			Boolean gotVal ;

			gotVal = FALSE ;

			if( RESULT_OK == ParserMatch( "~s*(~w+)", match, &token ) ) {
				gotVal = TRUE ;

				if( RESULT_OK != CopyTokenToCmdBuf( &token, cmdParmBuf, nParms, &bufPtr ) ) {
					CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
					return RESULT_ERROR ;
				}

				AT_SetCmdParmPtr( ch, ctxt, nParms, bufPtr ) ;
				++nParms ;
			}

			else if( RESULT_OK == ParserMatch( "~s*\"(~C\"*)\"", match, &token ) ) {
				gotVal = TRUE ;

				if( RESULT_OK != CopyTokenToCmdBuf( &token, cmdParmBuf, nParms, &bufPtr ) ) {
					CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
					return RESULT_ERROR ;
				}

				AT_SetCmdParmPtr( ch, ctxt, nParms, bufPtr ) ;
				++nParms ;
			}

			if( RESULT_OK == ParserMatch( "~s*,", match, 0 ) ) {
				if( !gotVal ) {
					++nParms ;
				}
				continue ;
			}

			break ;
		}

		if( nParms < cmd->minParms ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		if( nParms > cmd->maxParms ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}


		if( RESULT_OK != CheckParams( cmdId, ch, ctxt ) ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		ParserMatch( "~s*;", match, 0 ) ;
		moreCmds =( RESULT_OK != ParserMatch( "~s*$", match, 0 ) ) ;

		if( ( moreCmds ) && ( AT_CTXT_IMMED == ctxt ) ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		AT_SetMoreCmds( ch, moreCmds ) ;

		*atStatus = ExecCmd( cmdId, ch, ctxt, AT_ACCESS_REGULAR ) ;

		return RESULT_OK ;
	}

	//
	//	cmd( Note: this form is legal even when minParms > 0 )
	//


	if( RESULT_OK != CheckParams( cmdId, ch, ctxt ) ) {
		CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
		return RESULT_ERROR ;
	}

	moreCmds =( RESULT_OK != ParserMatch( "~s*$", match, 0 ) ) ;

	if( ( moreCmds ) && ( AT_CTXT_IMMED == ctxt ) ) {
		CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
		return RESULT_ERROR ;
	}

	AT_SetMoreCmds( ch, moreCmds ) ;

	ParserMatch( "~s*;", match, 0 ) ;

	*atStatus = ExecCmd( cmdId, ch, ctxt, AT_ACCESS_REGULAR ) ;

	return RESULT_OK ;
}

//-----------------------------------------------------------------------------
//	HandleSpecialCmd
//-----------------------------------------------------------------------------
//	Handle a "SPECIAL" command.
//-----------------------------------------------------------------------------

static Result_t HandleSpecialCmd(
	AT_CmdId_t		cmdId,
	UInt8			ch,
	UInt8			ctxt,
	ParserMatch_t*		match,
	AT_Status_t*	atStatus )
{
	UInt8		nParms = 0 ;
	const AT_Cmd_t *atCmdTbl = AT_GetCmdTbl() ;
	const AT_Cmd_t* cmd = &atCmdTbl[cmdId] ;
	char* cmdParmBuf = AtCmdParmBuf[ch][ctxt] ;

	InitParms( ch, ctxt ) ;

	if( RESULT_OK != ParserMatch( "~s*$", match, 0 ) ) {
		if( strlen( match->pLine ) >= AT_PARM_BUFFER_LEN ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		strcpy( (char*)cmdParmBuf, match->pLine ) ;

		Truncate( (char*)cmdParmBuf, "\n\r" ) ;

		AT_SetCmdParmPtr( ch, ctxt, nParms, cmdParmBuf ) ;
		++nParms ;
	}

	if( nParms < cmd->minParms ) {
		CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
		return RESULT_ERROR ;
	}

	if( nParms > cmd->maxParms ) {
		CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
		return RESULT_ERROR ;
	}


	if( RESULT_OK != CheckParams( cmdId, ch, ctxt ) ) {
		CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
		return RESULT_ERROR ;
	}

	AT_SetMoreCmds( ch, FALSE ) ;

	*atStatus = ExecCmd( cmdId, ch, ctxt, AT_ACCESS_REGULAR ) ;

	return RESULT_OK ;
}

//-----------------------------------------------------------------------------
//	CompareStrings
//-----------------------------------------------------------------------------
//	Compares characters in strings c1 and c2.  Returns the index of the first
//	characters that did not match.  Match is case-insensitive.
//-----------------------------------------------------------------------------

static UInt32 CompareStrings( const char* c1, const char* c2 )
{
	UInt32 n=0 ;

	while( *c2 ) {
		if( toupper( *c1 ) != toupper(*c2 ) ) {
			return 0 ;
		}
		++c1 ;
		++c2 ;
		++n  ;
	}
	return n ;
}

//-----------------------------------------------------------------------------
//	Initialize response buffer structures
//-----------------------------------------------------------------------------
static void RspBuf_Init( void )
{
	UInt8 i ;
	if( !RspBufStat.init ) {
		RspBufStat.init = TRUE ;
		for( i=0; i<AT_OUTBUF_NBUF; i++ ) {
			RspBuf_t* b = &RspBuf[i] ;
			b->lgBuf    = 0 ;
			b->seq      = 0 ;
		}
	}
}

//-----------------------------------------------------------------------------
//	Get oldest response.buffer
//-----------------------------------------------------------------------------
static RspBuf_t* RspBuf_GetOldest( void )
{
	RspBuf_t*	b = RspBuf ;
	RspBuf_t*	bOldest = 0 ;
	UInt8		i ;

	RspBuf_Init( ) ;

	for( i=0; i<AT_OUTBUF_NBUF; i++, b++ ) {
		if( b->seq ) {
			bOldest = b ;
			break ;
		}
	}

	for( ; i<AT_OUTBUF_NBUF; i++, b++ ) {
		if( b->seq ) {
			if( bOldest->seq > b->seq ) {
				bOldest = b ;
			}
		}
	}

	return bOldest ;
}

//-----------------------------------------------------------------------------
//	Get oldest response.buffer by channel
//-----------------------------------------------------------------------------
static RspBuf_t* RspBuf_GetOldestByCh( UInt8 ch )
{
	RspBuf_t*	b = RspBuf ;
	RspBuf_t*	bOldest = 0 ;
	UInt8		i ;

	RspBuf_Init( ) ;

	for( i=0; i<AT_OUTBUF_NBUF; i++, b++ ) {
		if( b->seq ) {
			if( b->ch == ch ) {
				bOldest = b ;
				break ;
			}
		}
	}

	for( ; i<AT_OUTBUF_NBUF; i++, b++ ) {
		if( b->seq ) {
			if( b->ch == ch ) {
				if( bOldest->seq > b->seq ) {
					bOldest = b ;
				}
			}
		}
	}

	return bOldest ;
}

//-----------------------------------------------------------------------------
//	Get free response buffer
//-----------------------------------------------------------------------------
static RspBuf_t* RspBuf_GetFree( void )
{
	RspBuf_t* b = RspBuf ;
	UInt8 i ;

	for( i=0; i<AT_OUTBUF_NBUF; i++, b++ ) {
		if( 0 == b->seq ) {
			b->seq = ++RspBufStat.seq ;
			return b ;
		}
	}

	return 0 ;
}

//-----------------------------------------------------------------------------
//	Free oldest buffer
//-----------------------------------------------------------------------------
static void RspBuf_Free( RspBuf_t* b )
{
	RspBuf_Init( ) ;

	if( b ) {
		if( b->lgBuf ) {
			OSHEAP_Delete( b->lgBuf ) ;
		}
		b->seq = 0 ;
	}
}

//-----------------------------------------------------------------------------
//	"Reuse" a buffer
//-----------------------------------------------------------------------------
static RspBuf_t* RspBuf_Reuse( RspBuf_t* b )
{
	RspBuf_Init( ) ;

	if( b ) {
		if( b->lgBuf ) {
			OSHEAP_Delete( b->lgBuf ) ;
		}
		b->seq = ++RspBufStat.seq ;
	}

	return b ;
}

//-----------------------------------------------------------------------------
//	Buffer a response string
//-----------------------------------------------------------------------------
static void RspBuf_InsertStr( UInt8 ch, const UInt8* str )
{
	UInt8		len = 1 + strlen( (char*)str ) ;
	RspBuf_t*	b = RspBuf ;

	RspBuf_Init( ) ;

	b = RspBuf_GetFree ( ) ;

	if( !b ) {
		b = RspBuf_Reuse( RspBuf_GetOldest( ) ) ;
	}

	if( len <= AT_OUTBUF_SHORT ) {
		strcpy( (char*)b->smBuf, (char*)str ) ;
		b->lgBuf = 0 ;
	}
	else {
		b->lgBuf = (UInt8*)OSHEAP_Alloc( len ) ;
		strcpy( (char*)b->lgBuf, (char*)str ) ;
	}

	b->ch = ch ;
}

//-----------------------------------------------------------------------------
//	Get pointer to buffered string
//-----------------------------------------------------------------------------
static UInt8* RspBuf_GetStr( RspBuf_t* b )
{
	if( !b ) {
		return 0 ;
	}

	return b->lgBuf ? b->lgBuf : b->smBuf ;
}

//-----------------------------------------------------------------------------
//	                 P U B L I C   F U N C T I O N S
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//	AT_GetCmdName
//-----------------------------------------------------------------------------
//	Returns a pointer to the AT command name string for this cmdId.  Returns
//	NULL if invalid cmdId.
//-----------------------------------------------------------------------------

const UInt8* AT_GetCmdName( AT_CmdId_t cmdId )
{
	const AT_Cmd_t *atCmdTbl = AT_GetCmdTbl() ;
	const AT_Cmd_t*	cmd = atCmdTbl ;

	if( AT_InvalidCmdId( cmdId ) ) {
		return 0 ;
	}

	return( const UInt8*) cmd[cmdId].cmdName ;
}

//-----------------------------------------------------------------------------
//	AT_GetCmdOptions
//-----------------------------------------------------------------------------
//	Returns the 'options' flag for a command..
//-----------------------------------------------------------------------------
UInt8 AT_GetCmdOptions( AT_CmdId_t cmdId )
{
	const AT_Cmd_t *atCmdTbl = AT_GetCmdTbl() ;
	const AT_Cmd_t*	cmd = atCmdTbl ;

	if( AT_InvalidCmdId( cmdId ) ) {
		return 0 ;
	}

	return cmd[cmdId].options ;
}

//-----------------------------------------------------------------------------
//	AT_GetValidParmStr
//-----------------------------------------------------------------------------
//	Returns a pointer to the AT command valid-parms string for this cmdId.
//	Returns NULL if invalid cmdId.  The valid-parms string may be null.
//-----------------------------------------------------------------------------

const UInt8* AT_GetValidParmStr( AT_CmdId_t cmdId )
{
	const AT_Cmd_t *atCmdTbl = AT_GetCmdTbl() ;
	const AT_Cmd_t*	cmd = atCmdTbl ;

	if( AT_InvalidCmdId( cmdId ) ) {
		return 0 ;
	}

	return( const UInt8*) cmd[cmdId].validParms ;
}

//-----------------------------------------------------------------------------
//	AT_CmdCloseTransaction
//-----------------------------------------------------------------------------
//	Close open transaction.  Do not output any response.
//-----------------------------------------------------------------------------

void AT_CmdCloseTransaction( UInt8 ch )
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

#ifdef ENABLE_LOGGING
	MSG_LOG( "AT trn closed" ) ;
#endif

	if( AT_GetResPending( ch ) ) {
		AT_SetResPending( ch, FALSE ) ;
		AT_SetResDone( ch, TRUE ) ;
	}

	if( !AT_GetMoreCmds( ch ) ) {
		AT_ResetState( ch ) ;
	}

	AT_ManageBufferedRsp( ch ) ;
}

//-----------------------------------------------------------------------------
//	AT_CmdRsp
//-----------------------------------------------------------------------------
//	Output a successful command response.
//-----------------------------------------------------------------------------

void AT_CmdRsp( UInt8 ch, UInt16 rspCode )
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

#ifdef ENABLE_LOGGING
	MSG_LOG( "AT trn closed" ) ;
#endif

	if( AT_GetResPending( ch ) ) {
		AT_SetResPending( ch, FALSE ) ;
		AT_SetResDone( ch, TRUE ) ;
	}

	if( !AT_GetMoreCmds( ch ) ) {
		AT_OutputRsp( ch, rspCode ) ;
		AT_ResetState( ch ) ;
	}

	else if( AT_GetResDone( ch ) ) {
		AT_ResumeCmd ( ch ) ;
	}

	AT_ManageBufferedRsp( ch ) ;
}

//-----------------------------------------------------------------------------
//	AT_CmdRspError
//-----------------------------------------------------------------------------
//	Output a command error response.
//-----------------------------------------------------------------------------
void AT_CmdRspError( UInt8 ch, UInt16 rspCode )
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

#ifdef ENABLE_LOGGING
	MSG_LOG( "AT trn closed" ) ;
#endif

	AT_OutputRsp( ch, rspCode ) ;
	AT_ResetState( ch ) ;
	AT_ManageBufferedRsp( ch ) ;

}

//-----------------------------------------------------------------------------
//	AT_CmdRspCMSError
//-----------------------------------------------------------------------------
//	Output a CMS error response.
//-----------------------------------------------------------------------------

void AT_CmdRspCMSError( UInt8 ch, UInt16 rspCode )
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

#ifdef ENABLE_LOGGING
	MSG_LOG( "AT trn closed" ) ;
#endif

	AT_OutputCMSRsp( ch, rspCode ) ;
	AT_ResetState( ch ) ;
	AT_ManageBufferedRsp( ch ) ;
}

//-----------------------------------------------------------------------------
//	AT_CmdRspOK
//-----------------------------------------------------------------------------
//	Output OKAY command response.
//-----------------------------------------------------------------------------

void AT_CmdRspOK( UInt8 ch )
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	AT_CmdRsp( ch, AT_OK_RSP ) ;
}

//-----------------------------------------------------------------------------
//	AT_CmdRspSyntaxError
//-----------------------------------------------------------------------------
//	Output a simple error indication, "ERROR".  Use for syntax errors.
//	See also AT_CmdRspError().
//-----------------------------------------------------------------------------

void AT_CmdRspSyntaxError(UInt8 ch)
{
	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	AT_CmdRspError( ch, AT_ERROR_RSP ) ;
}

//-----------------------------------------------------------------------------
//	AT_ImmedCmdRspOK
//-----------------------------------------------------------------------------
//	Called by IMMED command handlers to indicate OK response
//-----------------------------------------------------------------------------

void AT_ImmedCmdRsp( UInt8 ch, AT_CmdId_t cmdId, UInt16 rspCode )
{
	if( AT_GetCmdIdByMpxChan( ch ) == cmdId ) {
		AT_CmdRsp( ch, rspCode ) ;
	}
	else {
		SetImmedCmdStatus( ch, cmdId, FALSE ) ;
		AT_OutputRsp( ch, rspCode ) ;
	}
}

//-----------------------------------------------------------------------------
//	AT_ImmedCmdRspError
//-----------------------------------------------------------------------------
//	Called by IMMED command handlers to indicate ERROR response
//-----------------------------------------------------------------------------

void AT_ImmedCmdRspError( UInt8 ch, AT_CmdId_t cmdId, UInt16 rspCode )
{
	if( AT_GetCmdIdByMpxChan( ch ) == cmdId ) {
		AT_CmdRspError( ch, rspCode ) ;
	}
	else {
		SetImmedCmdStatus( ch, cmdId, FALSE ) ;
		AT_OutputRsp( ch, rspCode ) ;
	}
}

//-----------------------------------------------------------------------------
//	AT_OutputStr
//-----------------------------------------------------------------------------
//	Output string to a channel.
//-----------------------------------------------------------------------------

void AT_OutputStr( UInt8 ch, const UInt8* rspStr )
{
	UInt8				rspBuf[ AT_RSPBUF_LG ] ;
	UInt16				len ;
	AT_ChannelCfg_t*	cfg ;

	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	cfg = AT_GetChannelCfg(ch);

	if ( cfg->Q ) return ;    // Quiet mode

	if ( rspStr == 0 || rspStr[ 0 ] == 0 ) return;

	rspBuf[0] = 0 ;

	if(cfg->V == 1) {
		len = 0 ;
		rspBuf[len++] = cfg->S3 ;
		rspBuf[len++] = cfg->S4 ;
		rspBuf[len]   = 0 ;
	}

	strncat( (char*)rspBuf, (char*)rspStr, (AT_RSPBUF_LG)-3 ) ;
	rspBuf[(AT_RSPBUF_LG)-3] = 0;

	len = strlen( (char*)rspBuf ) ;
	rspBuf[len++] = cfg->S3 ;
	rspBuf[len++] = cfg->S4 ;
	rspBuf[len]   = 0 ;

	V24_PutTxBlock( (DLC_t)ch, (UInt8*) rspBuf, len );

	/* For Debug Only: temporarily added to print AT response to debug port */
	{
		UInt8 strBuf[40];

		sprintf((char *) strBuf, "\r\nAT-Ch%d %dTS:", ch, TIMER_GetValue());
		SIO_PutString(1, strBuf);
		SIO_PutString(1, rspBuf);
	}

}

//-----------------------------------------------------------------------------
//	AT_OutputUnsolicitedStr
//-----------------------------------------------------------------------------
//	Broadcast string to all channels.
//-----------------------------------------------------------------------------

void AT_OutputUnsolicitedStr( const UInt8* rspStr )
{
	V24OperMode_t	operMode ;
	UInt8			ch ;
	AT_ChannelCfg_t* atcCfg;

	if( MPX_ObtainMuxMode() == NONMUX_MODE ) {
		operMode = V24_GetOperationMode( (DLC_t)0 ) ;

		atcCfg = AT_GetChannelCfg(0);

		if( operMode == V24OPERMODE_AT_CMD || (operMode == V24OPERMODE_NO_RECEIVE) ) {

			if ( atcCfg->AllowUnsolOutput) AT_OutputStr( 0, rspStr ) ;
		}
	}

	else {
		for ( ch=1; ch<ATDLCCOUNT; ch++) {

			operMode = V24_GetOperationMode( (DLC_t)ch );

			atcCfg = AT_GetChannelCfg( ch );

			if( operMode == V24OPERMODE_AT_CMD || (operMode == V24OPERMODE_NO_RECEIVE) ) {
				if ( atcCfg->AllowUnsolOutput) AT_OutputStr( ch, rspStr ) ;
			}
		}
	}
}

// This is the altered version of the original function AT_OutputStr().  The <CR> before and
// after the printed string is removed for flexible formatting.
void AT_OutputStrNoCR( UInt8 ch, const UInt8* rspStr )
{
	AT_ChannelCfg_t*	cfg ;

	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	cfg = AT_GetChannelCfg(ch);

	if ( cfg->Q ) return ;    // Quiet mode

	if ( rspStr == 0 || rspStr[ 0 ] == 0 ) return;

	V24_PutTxBlock( (DLC_t)ch, (UInt8*) rspStr, strlen((char *) rspStr) );


	/* For Debug Only: temporarily added to print AT response to debug port */
	{
		UInt8 strBuf[40];
		UInt8 *rspBuf;

		rspBuf = (UInt8 *)rspStr;

		sprintf((char *) strBuf, "\r\nAT-Ch%d %dTS:", ch, TIMER_GetValue());
		SIO_PutString(1, strBuf);
		SIO_PutString(1, rspBuf);
	}
}

//-----------------------------------------------------------------------------
//	AT_OutputUnsolicitedStrNoCR
//-----------------------------------------------------------------------------
//	Broadcast string to all channels.
//-----------------------------------------------------------------------------

void AT_OutputUnsolicitedStrNoCR( const UInt8* rspStr )
{
	V24OperMode_t	operMode ;
	UInt8			ch ;
	AT_ChannelCfg_t* atcCfg;

	if( MPX_ObtainMuxMode() == NONMUX_MODE ) {
		operMode = V24_GetOperationMode( (DLC_t)0 ) ;

		atcCfg = AT_GetChannelCfg(0);

		if( operMode == V24OPERMODE_AT_CMD || (operMode == V24OPERMODE_NO_RECEIVE) ) {

			if ( atcCfg->AllowUnsolOutput) AT_OutputStrNoCR( 0, rspStr ) ;
		}
	}

	else {
		for ( ch=1; ch<ATDLCCOUNT; ch++) {

			operMode = V24_GetOperationMode( (DLC_t)ch );

			atcCfg = AT_GetChannelCfg( ch );

			if( operMode == V24OPERMODE_AT_CMD || (operMode == V24OPERMODE_NO_RECEIVE) ) {
				if ( atcCfg->AllowUnsolOutput) AT_OutputStrNoCR( ch, rspStr ) ;
			}
		}
	}
}

//Output  blockable unsolicated event
void AT_OutputBlockableUnsolicitedStr( const UInt8* rspStr ){

	if (!IsATUnsolicitedEventEnabled()) return;

	AT_OutputUnsolicitedStr(rspStr);

}

//-----------------------------------------------------------------------------
//	Output string to channel; buffer string if channel is 'busy'.
//-----------------------------------------------------------------------------
void AT_OutputStrBuffered( UInt8 ch, const UInt8* rspStr )
{
	V24OperMode_t	operMode ;
	AT_ChannelCfg_t* atcCfg = AT_GetChannelCfg( ch );

	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	operMode =  V24_GetOperationMode( (DLC_t)ch ) ;	// **FixMe** DLC or CHAN?

	if( (operMode == V24OPERMODE_AT_CMD || operMode == V24OPERMODE_NO_RECEIVE) &&
		 atcCfg->AllowUnsolOutput ) {

		AT_OutputStr( ch, rspStr ) ;
	}
	else {
		RspBuf_InsertStr( ch, rspStr ) ;
	}
}

//-----------------------------------------------------------------------------
//	Flush buffered responses on a channel
//-----------------------------------------------------------------------------
void AT_ManageBufferedRsp ( UInt8 ch )
{
	RspBuf_t*			b ;
	V24OperMode_t		operMode ;
	AT_ChannelCfg_t*	atcCfg = AT_GetChannelCfg( ch );

	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	operMode = V24_GetOperationMode( (DLC_t)ch ) ;

	if( (operMode == V24OPERMODE_AT_CMD || operMode == V24OPERMODE_NO_RECEIVE) &&
		atcCfg->AllowUnsolOutput ) {

		while( 0 != ( b = RspBuf_GetOldestByCh( ch ) ) ) {
			AT_OutputStr( ch, RspBuf_GetStr( b ) ) ;
			RspBuf_Free( b ) ;
		}
	}
}

//-----------------------------------------------------------------------------
//  ProcessATCmd
//-----------------------------------------------------------------------------
//	Scans AT command line and executes commands.
//-----------------------------------------------------------------------------

static Result_t ProcessATCmd(
	UInt8			ch,
	UInt8			ctxt,
	const char*		cmdStr )
{
	ParserMatch_t		match ;
	AT_Status_t		atStatus ;
	const AT_Cmd_t *atCmdTbl = AT_GetCmdTbl() ;

	ParserInitMatch( cmdStr, &match ) ;

	if( AT_CTXT_NORMAL == ctxt ) {
		AT_SetResPending( ch, FALSE ) ;
	}
	else {
		SetImmedCmdStatus( ch, AT_INVALID_CMD_ID, FALSE ) ;
	}

	while( RESULT_OK != ParserMatch( "~s*$", &match, 0 ) ) {
		const AT_Cmd_t*		cmd ;
		AT_CmdId_t		 	cmdId ;
		UInt32				cmdLen ;

		if( RESULT_OK != FindHashedStr( atCmdTbl, match.pLine, &cmdId ) ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		cmd = &atCmdTbl[cmdId] ;

		if( 0 ==( cmdLen = CompareStrings( match.pLine, cmd->cmdName ) ) ) {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ;
			return RESULT_ERROR ;
		}

		match.pLine += cmdLen ;

		if( AT_CMDTYPE_BASIC == cmd->cmdType ) {
			if( RESULT_OK != HandleBasicCmd( cmdId, ch, ctxt, &match, &atStatus ) ) {
				return RESULT_ERROR ;
			}
		}

		else if( AT_CMDTYPE_EXTENDED == cmd->cmdType ) {
			if( RESULT_OK != HandleExtendedCmd( cmdId, ch, ctxt, &match, &atStatus ) ) {
				return RESULT_ERROR ;
			}
		}

		else if( AT_CMDTYPE_SPECIAL == cmd->cmdType ) {
			if( RESULT_OK != HandleSpecialCmd( cmdId, ch, ctxt, &match, &atStatus ) ) {
				return RESULT_ERROR ;
			}
			match.pLine += strlen( match.pLine ) ;	//	special command only returns to end-of-line
		}

		else {
			CmdRspError( ch, ctxt, AT_ERROR_RSP ) ; // should not happen
			return RESULT_ERROR ;
		}

		if( AT_CTXT_NORMAL == ctxt ) {
			if( AT_STATUS_PENDING == atStatus ) {
				AT_SetResPending( ch, TRUE ) ;
				AT_SetNextCmdChar( ch,(const UInt8*)match.pLine ) ;
				break ;
			}
		}

		else {
			if( AT_STATUS_PENDING == atStatus ) {
				SetImmedCmdStatus( ch, cmdId, TRUE ) ;
				break ;
			}
		}
	}

	return RESULT_OK ;
}

//-----------------------------------------------------------------------------
//  AT_ProcessCmd
//-----------------------------------------------------------------------------
//	Process AT command string.  This calls ProcessATCmd, which does the
//	real work.
//-----------------------------------------------------------------------------

//
//	Handles busy response based on command type: SPECIAL commands
//	(like ATD) output BUSY and other commands output CME ERROR: +513
//
static void OutputBusyResponse( UInt8 ch,  UInt8 cmdType )
{
	if ( AT_CMDTYPE_SPECIAL == cmdType ) {
		AT_OutputRsp( ch, AT_BUSY_RSP ) ;
	}
	else {
		AT_OutputRsp( ch, AT_CME_ERR_AT_BUSY ) ;
	}
}

Result_t AT_ProcessCmd(
	UInt8			ch,
	const UInt8*	atCmdStr )
{
	ParserMatch_t	match ;
	const char*		cmdStr =(const char*)atCmdStr ;
	const AT_Cmd_t *atCmdTbl = AT_GetCmdTbl() ;
	Result_t		result ;
	AT_CmdId_t		cmdIdx ;
	UInt8			ctxt ;
	UInt8			cmdType ;

	AT_CmdId_t		currNormalCmd ;
	AT_CmdId_t		currImmedCmd ;

	if( AT_InvalidChan( ch ) ) {
		return RESULT_ERROR ;
	}

	ParserInitMatch( cmdStr, &match ) ;

	//
	//	must start with "AT"
	//
	result = ParserMatch( "~s*at~s*", &match, 0 ) ;

	//
	//	... if nothing follows "AT" this is OK; else lookup command
	//
	if( RESULT_OK == result ) {
		result = ParserMatch( "~s*$", &match, 0 ) ;
		if( RESULT_OK == result ) {
			AT_OutputRsp( ch, AT_OK_RSP ) ;
			return RESULT_OK ;
		}
	}

	//
	//	check for valid AT command - this will be done again in ProcessAtCmd()
	//	but it needs to be done here to determine whether this is 'normal'
	//	or 'immediate' command
	//

	result = FindHashedStr( atCmdTbl, match.pLine, &cmdIdx ) ;
	if( RESULT_OK != result ) {
		AT_OutputRsp( ch, AT_ERROR_RSP ) ;
		return RESULT_ERROR ;
	}

	ctxt = GetCtxt( cmdIdx ) ;
	cmdType = atCmdTbl[cmdIdx].cmdType ;

	currNormalCmd = AT_GetCmdIdByMpxChan( ch ) ;
	currImmedCmd  = GetImmedCmd( ch ) ;

	//
	//	if normal-context command make sure normal context is idle
	//
	if( AT_CTXT_NORMAL == ctxt ) {
		if( AT_INVALID_CMD_ID != currNormalCmd ) {
			MSG_LOG( (UInt8*)"AT cmd rejected (normal ctxt busy)\r\n");
			OutputBusyResponse( ch, cmdType ) ;
			return RESULT_ERROR ;
		}
	}

	//
	//	if immediate-context command make sure immediate context is idle and
	//	this is not a concatenated command
	//
	else {
		if( AT_INVALID_CMD_ID != currImmedCmd ) {
			MSG_LOG( (UInt8*)"AT cmd rejected (immed ctxt busy)\r\n");
			OutputBusyResponse( ch, cmdType ) ;
			return RESULT_ERROR ;
		}
		if( AT_GetMoreCmds( ch ) ) {
			MSG_LOG( (UInt8*)"AT cmd rejected (concatenated cmd in progress)\r\n");
			OutputBusyResponse( ch, cmdType ) ;
			return RESULT_ERROR ;
		}
	}

	//
	//	There is one more check to make:  IMMED commands cannot be
	//	concatenated; the concatenation check will be done later,
	//	as the command is being parsed
	//

	//
	//	Process rest of command line
	//
	if( AT_CTXT_NORMAL==ctxt ) {
		strcpy( (char*)(cmdStr=(const char*)AtCmdBuf[ch]), match.pLine ) ;
	}
	else {
		cmdStr = match.pLine ;
	}

	return ProcessATCmd( ch, ctxt, (char*)cmdStr ) ;
}

//-----------------------------------------------------------------------------
//  AT_ResumeCmd
//-----------------------------------------------------------------------------
//	Called to resume AT command processing that was interrupted by a long
//	command.  Parsing and command execution continues where it left off,
//	after the previous long command.
//-----------------------------------------------------------------------------

Result_t AT_ResumeCmd( UInt8 ch )
{
	const UInt8*	nextByte ;

	if( AT_InvalidChan( ch ) ) {
		return RESULT_ERROR ;
	}

	nextByte = AT_GetNextCmdChar( ch ) ;
	AT_SetResDone( ch, FALSE ) ;

	if( !AT_GetMoreCmds( ch ) ) return RESULT_ERROR ;
	if( !nextByte ) return RESULT_ERROR ;
	return ProcessATCmd( ch, AT_CTXT_NORMAL, (const char*)nextByte ) ;
}

//-----------------------------------------------------------------------------
//  AT_GetParm
//-----------------------------------------------------------------------------
//	Get pointer to AT parameter.  Long commands can use this function to
//	retrieve command parameters, since the parser prevents additional AT
//	commands from executing until the long command has been completed.
//-----------------------------------------------------------------------------

const UInt8* AT_GetParm( UInt8 ch, UInt8 parmId )
{
	if( AT_InvalidChan( ch ) ) {
		return 0 ;
	}

	if ( parmId > AT_GetMaxNumParms() ) return 0 ;
	return (const UInt8*) AT_GetCmdParmPtr( ch, AT_CTXT_NORMAL, parmId ) ;
}

//-----------------------------------------------------------------------------
//	Format CME string
//-----------------------------------------------------------------------------
static void FormatCmeStr( AT_ChannelCfg_t* cfg, UInt16 atCmdError, UInt8* strBuf )
{
	UInt8 tmpBuf[8] ;

	strBuf[0] = 0 ;

	if( cfg->CMEE == 0 ) {
		strcat( (char*)strBuf, (char*)GetATString( AT_ERROR_STR ) ) ;
	}

	else {

		if( atCmdError >= 300 && atCmdError <= 500) {
			strcat( (char*)strBuf, (char*)GetATString( AT_CMS_ERROR_STR ) ) ;
		}

		else {
			strcat( (char*)strBuf, (char*)GetATString( AT_CME_ERROR_STR ) ) ;
		}

		if( cfg->CMEE == 1 ) {
			sprintf( (char*)tmpBuf, "%d", atCmdError ) ;
			strcat( (char*)strBuf, (char*)tmpBuf ) ;
		}

		else { // CMEE == 2

			switch(atCmdError) {

				case AT_CME_ERR_PHONE_FAILURE:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_PHONE_FAILURE_STR) ) ;
					break;

				case AT_CME_ERR_NO_CONNECTION:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_NO_CONNECTION_STR) ) ;
					break;

				case AT_CME_ERR_PH_ADAPTOR_RESERVED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_PH_ADAPTOR_RESERVED_STR) ) ;
					break;

				case AT_CME_ERR_OP_NOT_ALLOWED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_OP_NOT_ALLOWED_STR) ) ;
					break;

				case AT_CME_ERR_OP_NOT_SUPPORTED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_OP_NOT_SUPPORTED_STR) ) ;
					break;

				case AT_CME_ERR_PH_SIM_PIN_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_PH_SIM_PIN_REQD_STR) ) ;
					break;

				case AT_CME_ERR_PH_FSIM_PIN_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_PH_FSIM_PIN_REQD_STR) ) ;
					break;

				case AT_CME_ERR_PH_FSIM_PUK_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_PH_FSIM_PUK_REQD_STR) ) ;
					break;

				case AT_CME_ERR_PH_SIM_NOT_INSERTED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_PH_SIM_NOT_INSERTED_STR) ) ;
					break;

				case AT_CME_ERR_SIM_PIN_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SIM_PIN_REQD_STR) ) ;
					break;

				case AT_CME_ERR_SIM_PUK_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SIM_PUK_REQD_STR) ) ;
					break;

				case AT_CME_ERR_SIM_FAILURE:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SIM_FAILURE_STR) ) ;
					break;

				case AT_CME_ERR_SIM_BUSY:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SIM_BUSY_STR) ) ;
					break;

				case AT_CME_ERR_SIM_WRONG:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SIM_WRONG_STR) ) ;
					break;

				case AT_CME_ERR_SIM_PASSWD_WRONG:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SIM_PASSWD_WRONG_STR) ) ;
					break;

				case AT_CME_ERR_SIM_PIN2_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SIM_PIN2_REQD_STR) ) ;
					break;

				case AT_CME_ERR_SIM_PUK2_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SIM_PUK2_REQD_STR) ) ;
					break;

				case AT_CME_ERR_MEMORY_FULL:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_MEMORY_FULL_STR) ) ;
					break;

				case AT_CME_ERR_INVALID_INDEX:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_INVALID_INDEX_STR) ) ;
					break;

				case AT_CME_ERR_NOT_FOUND:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_NOT_FOUND_STR) ) ;
					break;

				case AT_CME_ERR_MEMORY_FAILURE:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_MEMORY_FAILURE_STR) ) ;
					break;

				case AT_CME_ERR_TXTSTR_TOO_LONG:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_TXTSTR_TOO_LONG_STR) ) ;
					break;

				case AT_CME_ERR_INVALID_CHAR_INSTR:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_INVALID_CHAR_INSTR_STR) ) ;
					break;

				case AT_CME_ERR_DIAL_STR_TOO_LONG:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_DIAL_STR_TOO_LONG_STR) ) ;
					break;

				case AT_CME_ERR_INVALID_DIALSTR_CHAR:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_INVALID_DIALSTR_CHAR_STR) ) ;
					break;

				case AT_CME_ERR_NO_NETWORK_SVC:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_NO_NETWORK_SVC_STR) ) ;
					break;

				case AT_CME_ERR_NETWORK_TIMEOUT:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_NETWORK_TIMEOUT_STR) ) ;
					break;

				case AT_CME_ERR_NETWORK_NOT_ALLOWED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_NETWORK_NOT_ALLOWED_STR) ) ;
					break;

				case AT_CME_ERR_NETWORK_LOCK_PIN_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_NETWORK_LOCK_PIN_REQD_STR) ) ;
					break;

				case AT_CME_ERR_NETWORK_LOCK_PUK_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_NETWORK_LOCK_PUK_REQD_STR) ) ;
					break;

				case AT_CME_ERR_SUBNET_LOCK_PIN_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SUBNET_LOCK_PIN_REQD_STR) ) ;
					break;

				case AT_CME_ERR_SUBNET_LOCK_PUK_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SUBNET_LOCK_PUK_REQD_STR) ) ;
					break;

				case AT_CME_ERR_SP_LOCK_PIN_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SP_LOCK_PIN_REQD_STR) ) ;
					break;

				case AT_CME_ERR_SP_LOCK_PUK_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SP_LOCK_PUK_REQD_STR) ) ;
					break;

				case AT_CME_ERR_CORP_LOCK_PIN_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_CORP_LOCK_PIN_REQD_STR) ) ;
					break;

				case AT_CME_ERR_CORP_LOCK_PUK_REQD:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_CORP_LOCK_PUK_REQD_STR) ) ;
					break;

				case AT_CME_ERR_UNKNOWN:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_UNKNOWN_STR) ) ;
					break;

				case AT_CME_ERR_ILLEGAL_MS:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_ILLEGAL_MS_STR) ) ;
					break;

				case AT_CME_ERR_ILLEGAL_ME:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_ILLEGAL_ME_STR) ) ;
					break;

				case AT_CME_ERR_GPRS_SVC_NOT_ALLOWED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_GPRS_SVC_NOT_ALLOWED_STR) ) ;
					break;

				case AT_CME_ERR_PLMN_NOT_ALLOWED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_PLMN_NOT_ALLOWED_STR) ) ;
					break;

				case AT_CME_ERR_LA_NOT_ALLOWED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_LA_NOT_ALLOWED_STR) ) ;
					break;

				case AT_CME_ERR_ROAM_NOT_ALLOWED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_ROAM_NOT_ALLOWED_STR) ) ;
					break;

				case AT_CME_ERR_SERV_OPTION_NOT_SUPPORTED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SERV_OPTION_NOT_SUPPORTED_STR) ) ;
					break;

				case AT_CME_ERR_SERV_OPTION_NOT_SUSCRIBED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SERV_OPTION_NOT_SUSCRIBED_STR) ) ;
					break;

				case AT_CME_ERR_SERV_OPTION_OUT_OF_ORDER:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_SERV_OPTION_OUT_OF_ORDER_STR) ) ;
					break;

				case AT_CME_ERR_PDP_AUTHENTICATE_FAIL:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_PDP_AUTHENTICATE_FAIL_STR) ) ;
					break;

				case AT_CME_ERR_NO_NETWORK:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_NO_NETWORK_STR) ) ;
					break;

				case AT_CMS_ERR_ME_FAILURE:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_ME_FAILURE_STR) ) ;
					break;

				case AT_CMS_ERR_SMS_ME_SERV_RESERVED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_SMS_ME_SERV_RESERVED_STR) ) ;
					break;

				case AT_CMS_ERR_OP_NOT_ALLOWED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_OP_NOT_ALLOWED_STR) ) ;
					break;

				case AT_CMS_ERR_OP_NOT_SUPPORTED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_OP_NOT_SUPPORTED_STR) ) ;
					break;

				case AT_CMS_ERR_INVALID_PUD_MODE_PARM:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_INVALID_PUD_MODE_PARM_STR) ) ;
					break;

				case AT_CMS_ERR_INVALID_TEXT_MODE_PARM:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_INVALID_TEXT_MODE_PARM_STR) ) ;
					break;

				case AT_CMS_ERR_SIM_NOT_INSERTED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_SIM_NOT_INSERTED_STR) ) ;
					break;

				case AT_CMS_ERR_SIM_PIN_REQUIRED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_SIM_PIN_REQUIRED_STR) ) ;
					break;

				case AT_CMS_ERR_PH_SIM_PIN_REQUIRED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_PH_SIM_PIN_REQUIRED_STR) ) ;
					break;

				case AT_CMS_ERR_SIM_FAILURE:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_SIM_FAILURE_STR) ) ;
					break;

				case AT_CMS_ERR_SIM_BUSY:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_SIM_BUSY_STR) ) ;
					break;

				case AT_CMS_ERR_SIM_WRONG:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_SIM_WRONG_STR) ) ;
					break;

				case AT_CMS_ERR_SIM_PUK_REQUIRED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_SIM_PUK_REQUIRED_STR) ) ;
					break;

				case AT_CMS_ERR_SIM_PIN2_REQUIRED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_SIM_PIN2_REQUIRED_STR) ) ;
					break;

				case AT_CMS_ERR_SIM_PUK2_REQUIRED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_SIM_PUK2_REQUIRED_STR) ) ;
					break;

				case AT_CMS_ERR_MEMORY_FAILURE:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_MEMORY_FAILURE_STR) ) ;
					break;

				case AT_CMS_ERR_INVALID_MEMORY_INDEX:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_INVALID_MEMORY_INDEX_STR) ) ;
					break;

				case AT_CMS_ERR_MEMORY_FULL:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_MEMORY_FULL_STR) ) ;
					break;

				case AT_CMS_ERR_SMSC_ADDR_UNKNOWN:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_SMSC_ADDR_UNKNOWN_STR) ) ;
					break;

				case AT_CMS_ERR_NO_NETWORK_SERVICE:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_NO_NETWORK_SERVICE_STR) ) ;
					break;

				case AT_CMS_ERR_NETWORK_TIMEOUT:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_NETWORK_TIMEOUT_STR) ) ;
					break;

				case AT_CMS_ERR_NO_CNMA_EXPECTED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_NO_CNMA_EXPECTED_STR) ) ;
					break;

				case AT_CMS_ERR_UNKNOWN_ERROR:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_ERR_UNKNOWN_ERROR_STR) ) ;
					break;

				case AT_CME_ERR_AT_BUSY:
					strcat( (char*)strBuf, (char*)GetATString(ATC_CME_ERR_ATC_BUSY_STR) ) ;
					break;

				case AT_CME_ERR_AT_COMMAND_TIMEOUT:
					strcat( (char*)strBuf, (char*)GetATString(AT_CME_ERR_AT_COMMAND_TIMEOUT_STR) ) ;
					break;

				case AT_CMS_MO_SMS_IN_PROGRESS:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_MO_SMS_IN_PROGRESS_STR) ) ;
					break;

				case AT_CMS_SATK_CALL_CONTROL_BARRING:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_SATK_CALL_CONTROL_BARRING_STR) ) ;
					break;

				case AT_CMS_FDN_NOT_ALLOWED:
					strcat( (char*)strBuf, (char*)GetATString(AT_CMS_FDN_NOT_ALLOWED_STR) ) ;
					break;

				case AT_AUD_SETMODE_INVALID:
					strcat( (char*)strBuf, (char*)GetATString(AT_AUD_SETMODE_INVALID_STR) ) ;
					break;

				case AT_AUD_PARMID_MISSING:
					strcat( (char*)strBuf, (char*)GetATString(AT_AUD_PARMID_MISSING_STR) ) ;
					break;

				case AT_AUD_PARMID_INVALID:
					strcat( (char*)strBuf, (char*)GetATString(AT_AUD_PARMID_INVALID_STR) ) ;
					break;

				case AT_AUD_GETPARM_DATA_UNAVAILABLE:
					strcat( (char*)strBuf, (char*)GetATString(AT_AUD_GETPARM_DATA_UNAVAILABLE_STR) ) ;
					break;

				case AT_AUD_SETPARM_ERROR:
					strcat( (char*)strBuf, (char*)GetATString(AT_AUD_SETPARM_ERROR_STR) ) ;
					break;

				case AT_AUD_STARTTUNE_ERROR:
					strcat( (char*)strBuf, (char*)GetATString(AT_AUD_STARTTUNE_ERROR_STR) ) ;
					break;

				case AT_AUD_STOPTUNE_SAVE_MISSING:
					strcat( (char*)strBuf, (char*)GetATString(AT_AUD_STOPTUNE_SAVE_MISSING_STR) ) ;
					break;

				case AT_AUD_STOPTUNE_ERROR:
					strcat( (char*)strBuf, (char*)GetATString(AT_AUD_STOPTUNE_ERROR_STR) ) ;
					break;

				case AT_AUD_COMMAND_ERROR:
					strcat( (char*)strBuf, (char*)GetATString(AT_AUD_COMMAND_ERROR_STR) ) ;
					break;


				default:
					sprintf( (char*)tmpBuf, "%d", atCmdError ) ;
					strcat( (char*)strBuf, (char*)tmpBuf ) ;
					break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	Format CMS string
//-----------------------------------------------------------------------------
static void FormatCMSStr( AT_ChannelCfg_t* cfg, UInt16 atCmdError, UInt8* strBuf )
{
	UInt8 tmpBuf[8] ;

	strBuf[0] = 0 ;

	if( cfg->CMEE == 0 ) {
		strcat( (char*)strBuf, (char*)GetATString( AT_ERROR_STR ) ) ;
	}

	else {
		strcat( (char*)strBuf, (char*)GetATString( AT_CMS_ERROR_STR ) ) ;
		sprintf( (char*)tmpBuf, "%d", atCmdError ) ;
		strcat( (char*)strBuf, (char*)tmpBuf ) ;
	}
}

//-----------------------------------------------------------------------------
//	Format a response base on response ID
//-----------------------------------------------------------------------------
static void FormatATRspStr( AT_ChannelCfg_t* cfg, UInt16 rspId, UInt8* strBuf )
{
	strBuf[0] = 0 ;

	switch( rspId ) {

	case AT_NO_CARRIER_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_NO_CARRIER_STR ) ) ;
		break ;

	case AT_ERROR_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_ERROR_STR ) ) ;
		break ;

	case AT_NO_DIALTONE_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_NO_DIALTONE_STR ) ) ;
		break ;

	case AT_BUSY_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_BUSY_STR ) ) ;
		break ;

	case AT_NO_ANSWER_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_NO_ANSWER_STR ) ) ;
		break ;

	case AT_IDLE_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_EMPTY_STR ) ) ;
		break ;

	case AT_CONNECT_300_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CONNECT_300_STR ) ) ;
		break ;

	case AT_CONNECT_1200_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CONNECT_1200_STR ) ) ;
		break ;

	case AT_CONNECT_1200_75_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CONNECT_1200_75_STR ) ) ;
		break ;

	case AT_CONNECT_2400_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CONNECT_2400_STR ) ) ;
		break ;

	case AT_CONNECT_4800_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CONNECT_4800_STR ) ) ;
		break ;

	case AT_CONNECT_9600_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CONNECT_9600_STR ) ) ;
		break ;

	case AT_CME_ERR_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CME_ERROR_STR ) ) ;
		break ;

	case AT_CMS_ERR_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CMS_ERROR_STR ) ) ;
		break ;

	case AT_RING_VOICE_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CRING_VOICE_STR ) ) ;
		break ;

	case AT_RING_FAX_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CRING_FAX_STR ) ) ;
		break ;

	case AT_RING_ASYNC_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CRING_ASYNC_STR ) ) ;
		break ;

	case AT_RING_RELASYNC_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CRING_ASYNC_STR ) ) ;
		break ;

	case AT_RING_SYNC_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CRING_SYNC_STR ) ) ;
		break ;

	case AT_RING_RELSYNC_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CRING_REL_SYNC_STR ) ) ;
		break ;

	case AT_OK_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_OK_STR ) ) ;
		break ;

	case AT_CONNECT_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_CONNECT_STR ) ) ;
		break ;

	case AT_RING_RSP:
		strcat( (char*)strBuf, (char*)GetATString( AT_RING_STR ) ) ;
		break ;

	default:
		FormatCmeStr( cfg, rspId, strBuf ) ;
		break ;
	}
}

//-----------------------------------------------------------------------------
//	Is response code a "ring"?
//-----------------------------------------------------------------------------
static Boolean IsATRingRsp( UInt16 rspId )
{
	return ( ( rspId >= AT_RING_VOICE_RSP ) && ( rspId <= AT_RING_RELSYNC_RSP ) )
		? TRUE:FALSE ;
}

//-----------------------------------------------------------------------------
//	Is response code an "error"?
//-----------------------------------------------------------------------------
static Boolean IsATErrorRsp( UInt16 rspId )
{
	return ( ( AT_CME_ERR_RSP == rspId ) || ( AT_CMS_ERR_RSP == rspId ) )
		? TRUE:FALSE ;
}

//-----------------------------------------------------------------------------
//	Output AT response; do not close AT transaction
//-----------------------------------------------------------------------------
void AT_OutputRsp ( UInt8 ch, UInt16 rspCode )
{
	UInt8				rspBuf[ AT_RSPBUF_SM ] ;
	AT_ChannelCfg_t*	cfg ;

	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	cfg = AT_GetChannelCfg(ch);

	if ( IsATErrorRsp( rspCode ) && (cfg->CMEE == 0  || cfg->V == 0) ) {
		rspCode = AT_ERROR_RSP;
	}

	if ( IsATRingRsp( rspCode ) && (cfg->CRC == 0 || cfg->V == 0) ) {
		rspCode = AT_RING_RSP;
	}

	FormatATRspStr( cfg, rspCode, rspBuf ) ;

	AT_OutputStr( ch, rspBuf ) ;
}

//-----------------------------------------------------------------------------
//	Output CMS response; do not close AT transaction
//-----------------------------------------------------------------------------
void AT_OutputCMSRsp ( UInt8 ch, UInt16 rspCode )
{
	UInt8				rspBuf[ AT_RSPBUF_SM ] ;
	AT_ChannelCfg_t*	cfg ;

	if( AT_InvalidChan( ch ) ) {
		return ;
	}

	cfg = AT_GetChannelCfg(ch);

	FormatCMSStr( cfg, rspCode, rspBuf ) ;

	AT_OutputStr( ch, rspBuf ) ;
}

//-----------------------------------------------------------------------------
//	Broadcast response (generic response format); do not close AT transaction
//-----------------------------------------------------------------------------
typedef void (*OutputRspHandler_t)( UInt8 ch, UInt16 rspCode ) ;


static void OutputUnsolicitedRsp( UInt16 rspCode, OutputRspHandler_t outputResponse )
{
	V24OperMode_t	operMode ;
	UInt8			ch ;
	AT_ChannelCfg_t* atcCfg;

	if( MPX_ObtainMuxMode() == NONMUX_MODE ) {
		operMode = V24_GetOperationMode( DLC_NONE ) ;

		atcCfg = AT_GetChannelCfg(0);

		if( operMode == V24OPERMODE_AT_CMD || (operMode == V24OPERMODE_NO_RECEIVE) ) {
			if ( atcCfg->AllowUnsolOutput) outputResponse( DLC_NONE, rspCode ) ;
		}
	}

	else {
		for ( ch=1; ch<ATDLCCOUNT; ch++) {

			operMode = V24_GetOperationMode( (DLC_t)ch );

			atcCfg = AT_GetChannelCfg( ch );

			if( operMode == V24OPERMODE_AT_CMD || (operMode == V24OPERMODE_NO_RECEIVE) ) {
				if ( atcCfg->AllowUnsolOutput) outputResponse( ch, rspCode ) ;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	Broadcast AT response; do not close AT transaction
//-----------------------------------------------------------------------------

void AT_OutputUnsolicitedRsp( UInt16 rspCode )
{
	OutputUnsolicitedRsp( rspCode, AT_OutputRsp ) ;
}

void AT_OutputBlockableUnsolicitedRsp( UInt16 rspCode )
{

	if (!IsATUnsolicitedEventEnabled()) return;

	AT_OutputUnsolicitedRsp(rspCode);
}


//-----------------------------------------------------------------------------
//	Broadcast CMS response; do not close AT transaction
//-----------------------------------------------------------------------------
void AT_OutputUnsolicitedCMSRsp( UInt16 rspCode )
{
	OutputUnsolicitedRsp( rspCode, AT_OutputCMSRsp ) ;
}

//-----------------------------------------------------------------------------
//	Check for invalid parameters - log message if error.
//	Note: these functions should be called using macros as indicated
//	so that file and line number are shown.
//-----------------------------------------------------------------------------
Boolean _AT_InvalidChan( UInt8 ch, const char* fileName, int lineNo )
{
	if( ch >= MAX_NUM_MPX_CH ) {
		UInt8 rspBuf[ AT_RSPBUF_SM ] ;
		sprintf( (char*)rspBuf, "AT: invalid MPX ch %d ( fileName:%s lineNo:%d)\r\n",
			ch, fileName, lineNo ) ;
		return TRUE ;
	}
	return FALSE ;
}

Boolean _AT_InvalidCmdId( AT_CmdId_t cmdId, const char* fileName, int lineNo )
{
	if( cmdId >= AT_GetNumCmds() ) {
		UInt8 rspBuf[ AT_RSPBUF_SM ] ;
		sprintf( (char*)rspBuf, "AT: invalid command ID %d ( fileName:%s lineNo:%d)\r\n",
			cmdId, fileName, lineNo ) ;
		return TRUE ;
	}
	return FALSE ;
}

//-----------------------------------------------------------------------------
//	ATD support - dialstring scan
//-----------------------------------------------------------------------------

//
//	Copy 'inStr' to 'outStr', excluding any characters in 'chList'.  outStrLen
//	specifies the size of the outStr buffer (including NULL terminator).  Returns
//	RESULT_ERROR if too many non-excluded characters in inStr to copy; returns
//	RESULT_OK otherwise.
//
static Result_t RemoveNonDialChars(
	UInt8*			outStr,			//	pointer to output string
	UInt16			outStrLen,		//	sizeof output string (incl null terminator)
	UInt8*			chList,			//	list of chars to remove
	const UInt8*	inStr )			//	pointer to input string
{
	UInt8	c ;

	while( 0 != ( c = (*inStr++) ) ) {
		if( !strchr( (char*)chList, c ) ) {
			if( --outStrLen == 0 ) {
				return RESULT_ERROR ;
			}
			*outStr++ = c ;
		}
	}
	*outStr = 0 ;
	return RESULT_OK ;
}


//
//	Test last character in string for match of 'suffixChar'.  If match,
//	truncate string and return TRUE; else return FALSE.
//
static Boolean CheckSuffixChr( UInt8 * number, UInt8 suffixChar )
{
	UInt16 len = strlen( (char*)number ) ;
	if( len == 0 ) {
		return FALSE ;
	}

	if( number[len-1] == suffixChar ) {
		number[len-1] = 0 ;
		return TRUE ;
	}

	return FALSE ;

}

//
//	Add a prefix to a string..
//
static void AddPrefix( UInt8 * str, UInt8 * prefix )
{
	UInt16 prefixLen = strlen( (char*)prefix ) ;
	UInt16 stringLen = strlen( (char*)str ) ;

	UInt8 * pdst = (UInt8*)str + prefixLen + stringLen ;
	UInt8 * psrc = (UInt8*)str + stringLen ;

	do {
		*pdst-- = *psrc-- ;
	} while( stringLen-- ) ;	// incl. null

	memcpy( str, prefix, prefixLen ) ;
}

/**	Parse the string following an 'ATD' command
 */
Result_t AT_DialStrParse(
	const UInt8*		dialStr,
	CallParams_t*		callParams,
	Boolean*			voiceSuffix
	)
{
	UInt8*  theNumberPtr = (UInt8*)OSHEAP_Alloc(PHASE2_MAX_USSD_STRING_SIZE + 1);
	Boolean	tempRestrictClir ;
	Boolean	tempAllowClir ;

	//temp CLIR modes as define in 07.07
	#define TEMP_RESTRICT_CLIR_CODE 'I'
	#define TEMP_RESTRICT_CLIR_STR  "#31#"

	#define TEMP_ALLOW_CLIR_CODE	'i'
	#define TEMP_ALLOW_CLIR_STR	"*31#"

	if (!theNumberPtr)
	{
		assert(FALSE);
		return(RESULT_ERROR);
	}

	if( RESULT_OK != RemoveNonDialChars(theNumberPtr,
										PHASE2_MAX_USSD_STRING_SIZE+1,
										(UInt8*)" \t\r\n", dialStr ) )
	{
		OSHEAP_Delete((UInt8*)theNumberPtr);
		return RESULT_DIALSTR_INVALID ;
	}

	//	check for special suffix characters
	*voiceSuffix = CheckSuffixChr( theNumberPtr, VOICE_CALL_CODE ) ;
	tempRestrictClir = CheckSuffixChr( theNumberPtr, TEMP_RESTRICT_CLIR_CODE ) ;
	tempAllowClir = CheckSuffixChr( theNumberPtr, TEMP_ALLOW_CLIR_CODE ) ;

	//	handle temp CLIR suffix.
	if (tempRestrictClir)
	{
		if ((!*voiceSuffix) || (tempAllowClir))
		{
			OSHEAP_Delete((UInt8*)theNumberPtr);
			return(RESULT_ERROR);
		}
		AddPrefix( theNumberPtr, (UInt8*)TEMP_RESTRICT_CLIR_STR ) ;
	}

	if (tempAllowClir)
	{
		if ((!*voiceSuffix ) || (tempRestrictClir))
		{
			OSHEAP_Delete((UInt8*)theNumberPtr);
			return(RESULT_ERROR);
		}
		AddPrefix( theNumberPtr, (UInt8*)TEMP_ALLOW_CLIR_STR ) ;
	}

	if (CALLTYPE_UNKNOWN == DIALSTR_Parse(theNumberPtr, callParams))
	{
		OSHEAP_Delete((UInt8*)theNumberPtr);
		return RESULT_ERROR ;
	}

	OSHEAP_Delete((UInt8*)theNumberPtr);

	return RESULT_OK ;
}


/*
 *  Set up the AT command listing mode, TRUE = ALL, FALSE = HIDE_NAME
 */
void AT_DispFullCmdList(int mode)
{
	atCmdListMode = mode;
}

int AT_GetCmdListMode(void)
{
	return atCmdListMode;
}


