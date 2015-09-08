//-----------------------------------------------------------------------------
//	AT types
//-----------------------------------------------------------------------------

#ifndef _AT_TYPES_H_
#define _AT_TYPES_H_

#include "types.h"

/** Maximum length of an AT command input line */
#define AT_PARM_BUFFER_LEN 512

//-----------------------------------------------------------------------------
//	Type definitions
//-----------------------------------------------------------------------------

//
//	AT function completion status
//
/** AT command handler status is DONE */
#define AT_STATUS_DONE		1
/** AT command handler status is PENDING */
#define AT_STATUS_PENDING	2

/** AT command handler status.  Either AT_STATUS_DONE or AT_STATUS_PENDING */
typedef UInt32 AT_Status_t ;

/** AT command ID.  A unique ID is assigned to each AT command using the AT Command
  * table generator utility.
  */
typedef UInt16 AT_CmdId_t ;

#define AT_INVALID_CMD_ID	((AT_CmdId_t)(~0))	//	invalid AT command ID

/** Command access types */
enum {
	AT_ACCESS_UNKNOWN,	/**< unknown */
	AT_ACCESS_REGULAR,	/**< invoke or set; examples: at+cops=1,2; ate; ate0 */
	AT_ACCESS_READ,		/**< status or query: examples: at+cops?; ate? */
	AT_ACCESS_TEST		/**< get help; examples: at+cops=?; ate=? */
} ;

/** Command types */
enum {
	AT_CMDTYPE_UNKNOWN,	/**< unknown */
	AT_CMDTYPE_BASIC,	/**< basic command; examples: ATE, ATE0 */
	AT_CMDTYPE_EXTENDED, /**< extended command; examples: AT+COPS=1,2,3 */
	AT_CMDTYPE_SPECIAL	/**< special command accepts everything as arg; example: ATD12345; */
} ;

/** Command options, may be or'ed together */
enum {
	AT_OPTION_NONE					= 0x00,	/**< no options */
	AT_OPTION_NO_PIN				= 0x01,	/**< PIN not required */
	AT_OPTION_NO_PWR				= 0x02, /**< +CFUN=1 not required */
	AT_OPTION_USER_DEFINED_TEST_RSP	= 0x04, /**< user-defined TEST response implemented in handler */
	AT_OPTION_WAIT_PIN_INIT			= 0x08, /**< wait for PIN initialization */
	AT_OPTION_HIDE_NAME				= 0x10	/**< hide name in AT* and similar cmds */
} ;

// parameter range types - describes elements in generated range-spec array

#define AT_RANGE_NOCHECK		1		//	do not check range
#define	AT_RANGE_SCALAR_UINT8	2		//	value is a scalar UInt8
#define	AT_RANGE_SCALAR_UINT16	3		//	value is a scalar UInt16
#define	AT_RANGE_SCALAR_UINT32	4		//	value is a scalar UInt32
#define AT_RANGE_MINMAX_UINT8	5		//	two consecutive values are UInt8  min and max
#define AT_RANGE_MINMAX_UINT16	6		//	two consecutive values are UInt16 min and max
#define AT_RANGE_MINMAX_UINT32	7		//	two consecutive values are UInt32 min and max
#define AT_RANGE_STRING			8		//	value is an index in string table
#define AT_RANGE_ENDRANGE		9		//	end of range list
#define AT_RANGE_NOVALIDPARMS (-1)		//	'validParms' index when no range specified

//
//	AT command handler ( generic number of parameters )
//
typedef AT_Status_t ( *AT_CmdHndlr_t) (
	AT_CmdId_t			cmdId,
	UInt8				chan,
	UInt8				accessType, ... ) ;

//
//	AT Command Table entry
//
typedef struct {
	const char*			cmdName ;
	const char*			validParms ;
	UInt8				cmdType ;
	AT_CmdHndlr_t		cmdHndlr ;
	Int32				rangeIdx ;			//	range index; -1 = no range spec
	UInt8				minParms ;
	UInt8				maxParms ;
	UInt8				options ;
}	AT_Cmd_t ;

typedef struct {
	AT_CmdId_t			firstRow ;
	AT_CmdId_t			nextBlock ;
}	AT_HashBlk_t ;

#define AT_ENDHASH (AT_CmdId_t) 0x8000

//
//	describes a hash table for command processing
//
typedef struct {
	UInt32				nCol ;				//	number of columns
	const UInt32**		colCount ;			//	column count
	const char**		colChar ;			//	column chars
}	AT_HashTbl_t ;

typedef enum
{
	AT_FCLASSSERVICEMODE_DATA = 0,
	AT_FCLASSSSERVICEMODE_FAX_2 = 2,
	AT_FCLASSSSERVICEMODE_FAX_2dot0 = 20
} AT_FClassServiceMode_t;

#endif // _AT_TYPES_H_

