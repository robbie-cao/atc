#ifndef _AT_API_H_
#define _AT_API_H_

#include "at_types.h"
#include "at_string.h"
#include "at_rspcode.h"
#include "at_state.h"
#include "at_api.gen.h"
#include "parser.h"
#include "dialstr.h"
#include "mncallbk.h"
#include "rrmprim.h"

#include "at_cmdtbl.h"

/** Support for 'normal' and 'immediate' command contexts
 */
#define AT_N_CTXT		2			//	number of contexts
#define AT_CTXT_NORMAL	0			//	"normal" context
#define AT_CTXT_IMMED	1			//	"immediate" context

/** Small AT response buffer size for local buffer allocations
 */
#define AT_RSPBUF_SM	128		//	"small" response buffer

/** Large AT response buffer size for local buffer allocations
 */
#define AT_RSPBUF_LG	420		//	"large" response buffer

/** Process an AT command string
 */
extern Result_t			AT_ProcessCmd		(
	UInt8 ch,				/**< the multiplexer channel */
	const UInt8* atString	/**< command line */
	) ;

/** Resume concatenated AT command line after completion of asynchronous command
 */
extern Result_t			AT_ResumeCmd			(
	UInt8 ch				/**< the multiplexer channel */
	) ;

/** Get name of AT command from command ID */
extern const UInt8*		AT_GetCmdName		(
	AT_CmdId_t cmdId		/**< command ID */
	) ;

/** Get command options */
extern UInt8			AT_GetCmdOptions (
	AT_CmdId_t	cmdId		/**< command ID */
	) ;

/** Get the 'valid parameter string' based on command ID.  The 'valid parameter
 *	string is typically used to form the AT command test response.
 */
extern const UInt8*		AT_GetValidParmStr	(
	AT_CmdId_t cmdId		/**< command ID */
	) ;

/** Output AT command response and close transaction with OK status.
 *  The command response is based on the rspCode argument
 */
extern void	 AT_CmdRsp (
	UInt8 ch,			/**< the multiplexer channel */
	UInt16 rspCode		/**< the response code */
	);

/** Output AT command response and close transaction with ERROR status.
 *  The command response is based on the rspCode argument
 */
extern void	 AT_CmdRspError(
	UInt8 ch,			//< the multiplexer channel
	UInt16 rspCode		//< the response code
	) ;

/** Output AT command response and close transaction with ERROR status.
 *  The command response is a CMS error.
 */
extern void AT_CmdRspCMSError(
	UInt8 ch, 			/**< the multiplexer channel */
	UInt16 rspCode		/**< the response code */
	) ;
															// respond "+CMS ERROR: nn"
/** Output AT command response and close transaction with OK status.
 *  The command response is "OK"
 */
extern void AT_CmdRspOK(
	UInt8 ch 			/**< the multiplexer channel */
	) ;

/** Output AT command response and close transaction with ERROR status.
 *  The command response is "ERROR".
 */
extern void AT_CmdRspSyntaxError(
	UInt8 ch 			/**< the multiplexer channel */
	) ;

/** Close transaction but do not present response string..
 */
extern void AT_CmdCloseTransaction(
	UInt8 ch 			/**< the multiplexer channel */
	) ;

/** Output response without closing transaction.
 *  The response is based on rspCode..
 */
extern void AT_OutputRsp(
	UInt8 ch, 			/**< the multiplexer channel */
	UInt16 rspCode 		/**< the response code */
	) ;

/** Broadcast response without closing transaction.
 *  The response is based on rspCode..
 */

extern void AT_OutputBlockableUnsolicitedRsp(
	UInt16 rspCode 		/**< the response code */
	) ;

extern void AT_OutputUnsolicitedRsp(
	UInt16 rspCode 		/**< the response code */
	) ;

/** Output CMS response without closing transaction.
 *  The response is based on rspCode..
 */
extern void AT_OutputCMSRsp(
	 UInt8 ch,		 	/**< the multiplexer channel */
	UInt16 rspCode 		/**< the response code */
	) ;

/** Broadcast CMS response without closing transaction.  The response is based on rspCode..
 */
extern void  AT_OutputUnsolicitedCMSRsp(
	UInt16 rspCode 		/**< the response code */
	) ;

/** Output a string */
extern void AT_OutputStr(
	UInt8 ch, /** multiplexer channel */
	const UInt8* rspStr  /** the string */
	) ;

/** Output a string without <CR> formatting*/
extern void AT_OutputStrNoCR(
	UInt8 ch, /** multiplexer channel */
	const UInt8* rspStr  /** the string */
	) ;


/** Output a string using buffer control.*/
extern void AT_OutputStrBuffered(
	UInt8 ch, /** multiplexer channel */
	const UInt8* rspStr  /** the string */
	) ;


extern void AT_OutputBlockableUnsolicitedStr(
	const UInt8* rspStr  /** the string */
	) ;


/** Broadcast a string */
extern void AT_OutputUnsolicitedStr(
	const UInt8* rspStr  /** the string */
	) ;

/** Broadcast a string with AT_OutputStrNoCR */
extern void AT_OutputUnsolicitedStrNoCR(
	const UInt8* rspStr  /** the string */
	) ;

/** Manage buffered response */
extern void AT_ManageBufferedRsp(
	UInt8 ch /** multiplexer channel */
	) ;

/** Get parameter associated with current command */
extern const UInt8* AT_GetParm(
	UInt8 ch, /** multiplexer channel */
	UInt8 parmId /**< parameter id (index is 0-based) */
	) ;

/** Simple command handler. Simple commands have a single parameter. */
extern AT_Status_t AT_SimpleCmdHandler(
	AT_CmdId_t cmdId,	/**< command ID */
	UInt8 ch, /**< multiplexer channel */
	UInt8 accessType,	/**< access type */
	const UInt8* _P1, /**< parameter */
	Boolean basicAtCmd, /**< TRUE if basic cmd */
	UInt8 defautVal,  /**< default value */
	UInt8* CurSetVal /**< current val, returned */
	);

/** Parse ATD dial string */
Result_t AT_DialStrParse(
	const UInt8*		dialStr,
	CallParams_t*		callParams,
	Boolean*			voiceSuffix
	) ;

void AT_DispFullCmdList(int mode);
int AT_GetCmdListMode(void);

//
//	validate parameters
//

Boolean _AT_InvalidChan( UInt8 ch, const char* fileName, int lineNo );
Boolean _AT_InvalidCmdId( AT_CmdId_t cmdId, const char* fileName, int lineNo );

#define AT_InvalidChan( x )  _AT_InvalidChan ( (x), __FILE__, __LINE__ )
#define AT_InvalidCmdId( x ) _AT_InvalidCmdId( (x), __FILE__, __LINE__ )

void AT_ImmedCmdRsp( UInt8 ch, AT_CmdId_t cmdId, UInt16 rspCode );
void AT_ImmedCmdRspError( UInt8 ch, AT_CmdId_t cmdId, UInt16 rspCode );

#endif // _AT_API_H_

