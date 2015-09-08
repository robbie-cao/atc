//=============================================================================
// File:            at_irda.c
//
// Description:     AT interface for IrDA related access
//
// Note:
//
//==============================================================================
#include <stdio.h>
#include "stdlib.h"

#include "mti_build.h"

#include "atc.h"
#include "at_data.h"
#include "sdltrace.h"
#include "at_api.h"
#include "at_cfg.h"
#include "sysparm.h"
#include "logapi.h"


#ifdef INCL_IRDA

//------- Local Defines -----------


//--------- Local Variables ------------



//******************************************************************************
//
// Function Name:   ATCmd_IRDA_Handler
//
// Description:     This function handles the received AT*IRDA command for IRDA testing
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_IRDA_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
                               const UInt8* _P1, const UInt8* _P2, const UInt8* _P3)
{
#if defined(INCL_IRDA)
    extern void IRDUT_ProcessCmd(UInt8 atChan,UInt32 cmd, UInt32 parm1, UInt32 parm2);
    Boolean status = FALSE;
    UInt16 cmd,parm1,parm2;
    char temp_buffer[60];

    switch(accessType)
    {
      case AT_ACCESS_REGULAR:
        cmd   = _P1 == NULL ? 0 : atoi( (char*)_P1 );
        parm1 = _P2 == NULL ? 0 : atoi( (char*)_P2 );
        parm2 = _P3 == NULL ? 0 : atoi( (char*)_P3 );


			//Test IrComm for the IRDA serial port
		if (cmd == 99){

			//Disable log
			Log_EnableRange(0,511,FALSE);
			Log_EnableLogging(LOG_MISC,TRUE);
			//Switch log port to UARTA
			LOGIO_SelectLogIO(LOG_UARTA);
			//And then configiure AT port using IRDA serial port
			ATC_ConfigSerialDevForATC(IRDA_SERIAL);

		} else if (cmd == 100){

			//Switch log port back to UARTB
			LOGIO_SelectLogIO(LOG_UARTB);
			//Close IrComm and configiure AT port back to UARTA
			ATC_ConfigSerialDevForATC(UART_A);

		} else {
				//Disable log
			Log_EnableRange(0,511,FALSE);
			Log_EnableLogging(LOG_MISC,TRUE);
				//Switch log port to UARTA
			LOGIO_SelectLogIO(LOG_UARTA);
			    //Switch AT port to UARTC
            ATC_ConfigSerialDevForATC(UART_C);

			sprintf( temp_buffer, "%d %d %d", (int)cmd, (int)parm1, (int)parm2);
			AT_OutputStr(chan, (const UInt8 *) temp_buffer);
			IRDUT_ProcessCmd(chan,(UInt32)cmd,(UInt32)parm1,(UInt32)parm2);
			AT_CmdRspOK(chan);

		}

           break;

      case AT_ACCESS_READ:
        sprintf(temp_buffer, "%s: %d", (char *) AT_GetCmdName(cmdId), status);
        AT_OutputStr(chan, (const UInt8 *) temp_buffer);
        AT_CmdRspOK(chan);
           break;

      default: /* Must be AT_ACCESS_TEST */
        AT_CmdRspOK(chan);

           break;
    }

#else
	AT_CmdRspError(chan,AT_ERROR_RSP);
#endif

    return AT_STATUS_DONE;
}


#endif  // #ifdef INCL_IRDA
