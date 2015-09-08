//-----------------------------------------------------------------------------
//	AT configuration functions.
//-----------------------------------------------------------------------------

#define ENABLE_LOGGING
#include "mti_trace.h"
#include "at_cfg.h"
#include "mti_build.h"
#include "sysparm.h"
#include "devcfg.h"

static AT_ChannelCfg_t  atcconfig_db[MAX_NUM_MPX_CH];
static AT_CommonCfg_t	common_atcconfig_db;

//
AT_ChannelCfg_t* AT_GetChannelCfg(UInt8 ch)
{
	if( AT_InvalidChan( ch ) ) {
		xassert(FALSE, ch);
	}

	return &atcconfig_db[ch];
}

AT_CommonCfg_t*  AT_GetCommonCfg(void)
{
	return &common_atcconfig_db;
}

void AT_SetDefaultParms( Boolean init_flag )
{
   UInt16    chan_mode;
   UInt8	 coding, i;

   for (i=0; i<MAX_NUM_MPX_CH; i++)
   {

	   if ( init_flag )
	   {
			memset( &atcconfig_db[i], 0, sizeof( AT_ChannelCfg_t ) );
	   }

	   atcconfig_db[i].AmpC      =   1;                    // Circuit 109 changes
	   atcconfig_db[i].AmpD      =   0;                    // Circuit 108/2 behaviour -- common default value = 0
	   atcconfig_db[i].CR        =   0;
	   atcconfig_db[i].CRC       =   DEF_CRC;              // 0 = Disable extended +CRING
	   atcconfig_db[i].CREG      =   DEF_CREG;             // 0 = Disable unsolicited +CREG
	   atcconfig_db[i].CGREG     =   DEF_CGREG;            // 0 = Disable unsolicited +CGREG
	   atcconfig_db[i].MTREPORT  =   0;            		   // 0 = Disable Measurement Report *MTREPORT
	   atcconfig_db[i].E         =   DEF_E;				   // Echo ON/OFF
	   atcconfig_db[i].O         =   0;
	   atcconfig_db[i].X         =   1;
	   atcconfig_db[i].P         =   0;
	   atcconfig_db[i].Q         =   0;                    // Quiet OFF
	   atcconfig_db[i].S0        =   0;                    // Disable auto answer
	   atcconfig_db[i].S3        =  13;
	   atcconfig_db[i].S4        =  10;
	   atcconfig_db[i].S5        =   8;
	   atcconfig_db[i].S6        =   2;                    // 2 seconds before blind dial
	   atcconfig_db[i].S7        =   DEF_S7;               // default: 50 s to complete call
	   atcconfig_db[i].S8        =   2;                    // 2 seconds dialling pause
	   atcconfig_db[i].S10       =   10;
	   atcconfig_db[i].V         =   1;
	   atcconfig_db[i].CMEE      =   1;                    // +CME ERROR ON
	   atcconfig_db[i].CSTA      = 129;					   // Unknown type of number
	   atcconfig_db[i].ILRR      =   0;
	   atcconfig_db[i].IFC[ 0 ]  =   2;					   // RTS flow control on ( Hardware )
	   atcconfig_db[i].IFC[ 1 ]  =   2;					   // CTS flow control on ( Hardware )
	   atcconfig_db[i].ICF[ 0 ]  =   3;                    // Framing 8 bits data 1 stop
	   atcconfig_db[i].ICF[ 1 ]  =   3;                    // Framing no parity

	   atcconfig_db[i].CVHU      =   0;
	   atcconfig_db[i].CPBS      =   PB_ADN;    			// ADN

	   atcconfig_db[i].CPOL	     =   2;						// numeric format

	   atcconfig_db[i].ER      =     0;
	   atcconfig_db[i].DR      =     0;
	   atcconfig_db[i].ETBM[0]  =    0;
	   atcconfig_db[i].ETBM[1]  =    0;
	   atcconfig_db[i].ETBM[2]  =    20;
	   atcconfig_db[i].CMER[0]  =    0;
	   atcconfig_db[i].CMER[1]  =    0;
	   atcconfig_db[i].CMER[2]  =    0;
	   atcconfig_db[i].CMER[3]  =    0;
	   atcconfig_db[i].CMER[4]  =    0;


	   atcconfig_db[i].CPMS [0] = SM_STORAGE;              // Default Mem1 is Sim.
	   atcconfig_db[i].CPMS [1] = SM_STORAGE;              // Default Mem2 is Sim.
	   atcconfig_db[i].CPMS [2] = SM_STORAGE;              // Default Mem3 is Sim.

	   atcconfig_db[i].CHSR		=    0;	// disable
//	   atcconfig_db[i].CHSU		=    0;	// disable
	   atcconfig_db[i].WS46		=    12;// GSM

	   atcconfig_db[i].ECIPC	=    0;

	   chan_mode = SYSPARM_GetChanMode();

	   coding = 0;

		atcconfig_db[i].CUSD = 0;	// USSD disabled

		atcconfig_db[i].AllowUnsolOutput = TRUE;
   }

   if( init_flag )
   {

		memset( &common_atcconfig_db, 0, sizeof( AT_CommonCfg_t ) );

		common_atcconfig_db.ESIMC = 0;			// 0 = disable SIM Detection message

		//Fixme Here temp for SS

		common_atcconfig_db.IPR = 0;	// Autobauding
		common_atcconfig_db.Unsoliciated_CCMReport = FALSE;
		common_atcconfig_db.CallMeterWarning = FALSE;


		common_atcconfig_db.presetAllowTrace = SDLTRACE_IsSDLTraceAllow();
		common_atcconfig_db.logOn	= TRUE;

		SET_MVMIND(DEF_MVMIND);  // MVMIND: 1 = enabled; 0 = disabled


		SET_CSDH(0);

		common_atcconfig_db.MCSQ = 0;	// default to disable the unsolicited rssi, ber reporting

		common_atcconfig_db.VTD  = 1;	// 100 ms DTMF duration
   }
}


Result_t ATC_ConfigSerialDevForATC(SerialDeviceID_t  inDevID){

	Result_t result = RESULT_OK;

	if ( inDevID >= MAX_DEVICE_NUM ||inDevID < 0 ) {
		result = RESULT_ERROR;
	}

	//If there is open device with ATC, close it first
	if ( serialHandleforAT != -1 ) {
		Serial_RegisterEventCallBack(serialHandleforAT,NULL);
		Serial_CloseDevice(serialHandleforAT);
	}

	serialHandleforAT =
		Serial_OpenDevice(inDevID,Serial_GetDefaultCfg(inDevID));

	if ( serialHandleforAT == -1 ) {
		result = RESULT_ERROR;
	}else {

		//After switch device, need to re-register the event handler
		Serial_RegisterEventCallBack(serialHandleforAT,MPX_HandleInputDeviceEvent);
	}

	return result;
}

PortId_t ATC_GetCurrentSIOPort(void)
{

	SerialDeviceID_t atcCurrentDevice = Serial_GetDeviceID(serialHandleforAT);

	if (atcCurrentDevice == UART_A) {
		return (PortA);
	}
	else if (atcCurrentDevice == UART_C) {
		return (PortC);
	}
	else {
		return (InvalidPort);
	}
}
