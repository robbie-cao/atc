//-----------------------------------------------------------------------------
//	AT utility definitions.
//-----------------------------------------------------------------------------


#ifndef _AT_UTIL_H_
#define _AT_UTIL_H_

#include "mstypes.h"
#include "logapi.h"

/*
** -----------------------------------
**  Function prototypes
** -----------------------------------
*/
void      AT_Trace( char *format, ... );


UInt16 ConvertNWcause2CMEerror(NetworkCause_t inNetworkCause);

void ConvertRxSignalInfo2Csq(UInt8* rssi, UInt8* ber);

/*
** -----------------------------------
**  Macro instructions
** -----------------------------------
*/

#define _TRACE(_X_)   if (Log_IsLoggingEnable(P_ATC)) { AT_Trace _X_ ; }

#endif // _AT_UTIL_H_

