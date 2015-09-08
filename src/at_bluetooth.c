//******************************************************************************
//
// Description:  This include file contains the functions for
//               Bluetooth test.
//
// File:            Blutetooth_test.c
//
// Description:     AT interface for bluetooth related access
//
// Note:
//
//******************************************************************************

#include "mti_trace.h"
#include "consts.h"
#include "msconsts.h"
#include "stdio.h"
#include "sio.h"
#include "serialapi.h"
#include "osinterrupt.h"
#include "ostask.h"
#include "ossemaphore.h"
#include "irqctrl.h"
#include "assert.h"
#include "at_api.h"
#include "gpio.h"
#include "atc.h"

#ifdef BLUETOOTH_INCLUDED
void BTE_TestStartBluetooth(UInt8 bStartSerialMMI);

AT_Status_t ATCmd_MBTTST_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
                               const UInt8* _P1, const UInt8* _P2, const UInt8* _P3)
{
#if CHIPVERSION >= CHIP_VERSION(BCM2132,00) /* BCM2132 and later */
#ifdef BLUETOOTH_INCLUDED
   UInt16 cmd,parm1,parm2;
   char temp_buffer[60];
   Boolean status = FALSE;
   UARTConfig_t  * p_uartCfg;


   if ( accessType == AT_ACCESS_REGULAR )
   {

	   cmd   = _P1 == NULL ? 0 : atoi( (char*)_P1 );
	   parm1 = _P2 == NULL ? 0 : atoi( (char*)_P2 );
	   parm2 = _P3 == NULL ? 0 : atoi( (char*)_P3 );

	   AT_CmdRspOK( chan );
	   OSTASK_Sleep(100);

		switch ( cmd )
		{
			case 0:
				(void)BT_BootEntry();
				break;

			case 1: //Enable Bluetooth with Serial MMI
				AT_OutputStr(chan, (const UInt8 *)"Switch port!");
				LOGIO_SelectLogIO(LOG2NULL);
				//LOGIO_SelectLogIO(LOG_UARTA);
				AT_OutputStr(chan, (const UInt8 *)"Switch port Done!");
				AT_OutputStr(chan, (const UInt8 *)"Configure output GPIOs for Bluetooth");
				Serial_CloseDevice(serialHandleforAT);
				ATC_ConfigSerialDevForATC(NULL_DEV);
				BTE_TestStartBluetooth(TRUE);
				break;

			case 2://Disable Bluetooth without Serial MMI
				BTE_TestStartBluetooth(FALSE);
                break;


			case 10:
				//test assertion logging
				assert(0);
				break;
			default:
				break;
		}

   }

   else if ( accessType == AT_ACCESS_READ )
   {
	   sprintf(temp_buffer, "%s: %d", (char *) AT_GetCmdName(cmdId), status);
	   AT_OutputStr(chan, (const UInt8 *) temp_buffer);
	   AT_CmdRspOK(chan);
   }

#else // #if CHIPVERSION >= CHIP_VERSION(BCM2132,00) /* BCM2132 and later */

   AT_CmdRspSyntaxError( chan );

#endif // #if CHIPVERSION >= CHIP_VERSION(BCM2132,00) /* BCM2132 and later */
#endif // INCL_BLUETOOTH
   return AT_STATUS_DONE ;
}
#endif //#ifdef BLUETOOTH_INCLUDED
