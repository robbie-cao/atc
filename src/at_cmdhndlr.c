//-----------------------------------------------------------------------------
//	Simple AT command handlers.
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "mti_build.h"
#include "at_data.h"
#include "at_util.h"
#include "sysparm.h"

#include "at_api.h"
#include "at_cfg.h"
#include "ms_database.h"

#include "fax.h"
#include "ripproc.h"

//-----------------------------------------------------------------------------
//	ATCmd_CGMI_Handler
//-----------------------------------------------------------------------------
//	Command handler for the "CGMI" command.
//-----------------------------------------------------------------------------

AT_Status_t ATCmd_CGMI_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType )
{
	UInt8* nm = SYSPARM_GetManufacturerName() ;
	if ( *nm == 0 ) {
		nm = (UInt8*)"Mobilink" ;
	}

	switch ( accessType ) {

	case AT_ACCESS_REGULAR:
		AT_OutputStr ( chan, nm ) ;
		break ;

	case AT_ACCESS_READ:
	case AT_ACCESS_TEST:
		break ;
	}

	AT_CmdRspOK(chan) ;

	return AT_STATUS_DONE;
}

//-----------------------------------------------------------------------------
//	ATCmd_CGMM_Handler
//-----------------------------------------------------------------------------
//	Command handler for the "+CGMM" command.
//-----------------------------------------------------------------------------

AT_Status_t ATCmd_CGMM_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType )
{
	switch ( accessType ) {

	case AT_ACCESS_REGULAR:
		AT_OutputStr( chan, SYSPARM_GetModelName() );
		break ;

	case AT_ACCESS_READ:
	case AT_ACCESS_TEST:
		break ;
	}

	AT_CmdRspOK(chan) ;

	return AT_STATUS_DONE ;
}

//-----------------------------------------------------------------------------
//	ATCmd_CGMR_Handler
//-----------------------------------------------------------------------------
//	Command handler for the "+CGMR" command.
//-----------------------------------------------------------------------------

AT_Status_t ATCmd_CGMR_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType )
{
	switch ( accessType ) {

	case AT_ACCESS_REGULAR:
		AT_OutputStr( chan, SYSPARM_GetSWVersion() );
		break ;

	case AT_ACCESS_READ:
	case AT_ACCESS_TEST:
		break ;
	}

	AT_CmdRspOK(chan) ;

	return AT_STATUS_DONE ;
}



//-----------------------------------------------------------------------------
//	ATCmd_T_Handler
//-----------------------------------------------------------------------------
//	Command handler for the "T" command. This is a 'do-nothing' command
//	for compatability.
//-----------------------------------------------------------------------------

AT_Status_t ATCmd_T_Handler (AT_CmdId_t cmdId,
							 UInt8 chan,
							 UInt8 accessType )
{
	switch ( accessType ) {

	case AT_ACCESS_REGULAR:
	case AT_ACCESS_TEST:
		AT_CmdRspOK(chan) ;
		return AT_STATUS_DONE ;

	case AT_ACCESS_READ:
	default:
		AT_CmdRspError(chan,AT_ERROR_RSP) ;
		return AT_STATUS_DONE ;
	}
}

//-----------------------------------------------------------------------------
//	ATCmd_STAR_Handler
//-----------------------------------------------------------------------------
//	Command handler for the "*" command.
//-----------------------------------------------------------------------------

AT_Status_t ATCmd_STAR_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType )
{
	AT_CmdId_t i ;
	int full_list_mode;

    full_list_mode = AT_GetCmdListMode();
	for ( i=0; i<AT_GetNumCmds(); i++ ) {
		AT_CmdId_t r = AT_GetHashRedirect( i ) ;
		if( !(AT_OPTION_HIDE_NAME & AT_GetCmdOptions( r ) ) || (full_list_mode == TRUE)) {
			const UInt8* cmdName = AT_GetCmdName ( r ) ;
			if ( cmdName ) {
				AT_OutputStr ( chan, cmdName );
			    OSTASK_Sleep( TICKS_ONE_SECOND/100 );	// KLUDGE to keep buffers form overflowing
			}
		}
	}

	AT_CmdRspOK(chan) ;
	return AT_STATUS_DONE ;
}

//-----------------------------------------------------------------------------
//	AT_SimpleCmdHandler
//-----------------------------------------------------------------------------
//	The abstarct logic for simple AT command handlers
//-----------------------------------------------------------------------------


extern AT_Status_t  AT_SimpleCmdHandler(AT_CmdId_t cmdId,
										UInt8 chan,
										UInt8 accessType,
										const UInt8* _P1,
										Boolean basicAtCmd,
										UInt8 defautVal,
										UInt8* curSetVal)
{

	UInt8 atRes[20];
	 /*
	 ** ------------------------------------
	 **  Regular command  AT+XXX=...
	 ** ------------------------------------
	 */
	 if ( accessType == AT_ACCESS_REGULAR  ){

		*curSetVal = (_P1 == NULL) ? defautVal : atoi( (char*)_P1 );
	 }

	/*
	 ** ------------------------------------
	 **  READ command  AT+XXX?
	 ** ------------------------------------
	*/
	else if ( accessType == AT_ACCESS_READ )
	{
		if ( basicAtCmd )
			sprintf( (char*)atRes, "%03d", *curSetVal );
		else
			sprintf( (char*)atRes, "%s: %d", (char*)AT_GetCmdName(cmdId), *curSetVal );

		 AT_OutputStr( chan, atRes );
	}

	AT_CmdRspOK( chan );


	return AT_STATUS_DONE;

}

//At command handler ATE
AT_Status_t ATCmd_E_Handler (AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1)
{


   AT_Status_t		atStatus;

   AT_ChannelCfg_t*	channelCfg = AT_GetChannelCfg(chan);;

   atStatus = AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,TRUE,1,&channelCfg->E);

   if ( accessType == AT_ACCESS_REGULAR )
   {
      V24_SetEchoMode( (DLC_t)chan, (Boolean) channelCfg->E);
   }

   return atStatus;


}


//At command handler ATV
AT_Status_t ATCmd_V_Handler (AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1)
{

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);
	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,TRUE,0,&channelCfg->V);

}

AT_Status_t ATCmd_AND_F_Handler (AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	/*
	**  Restore factory settings
	*/

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

	AT_CmdRspOK(chan);

	AT_SetDefaultParms( FALSE );
	FAX_InitializeFaxParam();
	CC_InitCallCfgAmpF( &curCallCfg );
	/* Reset echo mode */
	V24_SetEchoMode( (DLC_t)chan, (Boolean) channelCfg->E );

	return AT_STATUS_DONE;

}

AT_Status_t ATCmd_I_Handler (AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{

	UInt8	val = 0 ;
	char	buf[64] ;

	if( _P1 ) {
		val = atoi( (char*) _P1 ) ;
	}

	switch( val ) {

	case 0:
		AT_OutputStr( chan, SYSPARM_GetModelName() );
		break ;

	case 1:
		AT_OutputStr( chan, SYSPARM_GetSWVersion() );
		break ;

	case 8:
		sprintf( buf,"%d",RIPPROC_GetVersion());
		AT_OutputStr( chan, (UInt8*)buf );
		break;

	default:
		break;

	}

	AT_CmdRspOK( chan ) ;
	return AT_STATUS_DONE ;
}


AT_Status_t ATCmd_CMUX_Handler (AT_CmdId_t	cmdId,
							UInt8		chan,
							UInt8		accessType,
							const		UInt8* _P1,
							const		UInt8* _P2,
							const		UInt8* _P3,
							const		UInt8* _P4,
							const		UInt8* _P5,
							const		UInt8* _P6,
							const		UInt8* _P7,
							const		UInt8* _P8,
							const		UInt8* _P9)
{

	MUXParam_t  muxparam;
	UInt8	tempval;
	UInt32  PortSpeed;
	UInt8	atRes[64] ;

	if ( accessType == AT_ACCESS_REGULAR )
	{
		AT_CmdRspOK(chan);

		// send atc_start_mux command
		tempval = _P1 == NULL ? 0 : atoi((char*)_P1);
		if (tempval)
			muxparam.mode = ADVANCED_MODE;
		else
			muxparam.mode = BASIC_MODE;

		tempval = _P2 == NULL ? 0 : atoi((char*)_P2);
		switch (tempval)
		{
		case 0:
			muxparam.frametype = MPX_U_FRAME;
			break;
		case 1:
			muxparam.frametype = MPX_UI_FRAME;
			break;
		case 2:
			muxparam.frametype = MPX_I_FRAME;
			break;
		default:
			muxparam.frametype = MPX_U_FRAME;
			break;
		}

		tempval = _P3 == NULL ? 5 : atoi((char*)_P3);
		switch (tempval)
		{
		case 1:
			muxparam.PortSpeed = 9600;
			break;
		case 2:
			muxparam.PortSpeed = 19200;
			break;
		case 3:
			muxparam.PortSpeed = 38400;
			break;
		case 4:
			muxparam.PortSpeed = 57600;
			break;
		case 5:
			muxparam.PortSpeed = 115200;
			break;
		default:
			muxparam.PortSpeed = 9600;
			break;
		}

		muxparam.MaxFrameSize = _P4 == NULL ? 31 : atoi((char*)_P4);
#if CHIPVERSION >= CHIP_VERSION(BCM2132,00) /* BCM2132 and later */ && defined(GCXX_PC_CARD)
		SIO_MuxFrameChanged(muxparam.MaxFrameSize);
#endif // #if CHIPVERSION >= CHIP_VERSION(BCM2132,00) /* BCM2132 and later */ && defined(GCXX_PC_CARD)
		muxparam.AckTimer = _P5 == NULL ? 10 : atoi((char*)_P5);
		muxparam.MaxNumRetry = _P6 == NULL ? 3 : atoi((char*)_P6);
		muxparam.CtrlChlRspTimer = _P7 == NULL ? 30 : atoi((char*)_P7);
		muxparam.WakeupRspTimer = _P8 == NULL ? 10 : atoi((char*)_P8);
		muxparam.ERMWinSize = _P9 == NULL ? 0 : atoi((char*)_P9);

		AT_GetCommonCfg()->logOn	= FALSE;

		MPX_ReportStartMUXReq( &muxparam );
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		muxparam = *(MPX_ObtainMuxParam());
		PortSpeed = muxparam.PortSpeed;
		switch (PortSpeed)
		{
		case 9600:
			tempval = 1;
			break;
		case 19200:
			tempval = 2;
			break;
		case 38400:
			tempval = 3;
			break;
		case 57600:
			tempval = 4;
			break;
		case 115200:
			tempval = 5;
			break;
		default:
			tempval = 1;
			break;
		}
		sprintf( (char*)atRes, "+CMUX: %d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
				 muxparam.mode,
				 muxparam.frametype,
				 tempval,
				 muxparam.MaxFrameSize,
				 muxparam.AckTimer,
				 muxparam.MaxNumRetry,
				 muxparam.CtrlChlRspTimer,
				 muxparam.WakeupRspTimer,
				 muxparam.ERMWinSize);

		AT_OutputStr(chan, atRes );

		AT_CmdRspOK(chan);
	}

	return AT_STATUS_DONE ;
}




AT_Status_t ATCmd_CFUN_Handler (AT_CmdId_t	cmdId,
								UInt8		chan,
                                UInt8		accessType,
                                const		UInt8* _P1,
                                const		UInt8* _P2)
{
	UInt8	atRes[64];
	UInt8	cFUN;
	SystemState_t	systemState;

	if ( AT_ACCESS_REGULAR == accessType )
	{
		if (!(_P1)) {
			AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}

		systemState = SYS_GetSystemState();

		if (systemState == SYSTEM_STATE_OFF_IN_PROGRESS) {
			AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );
			return AT_STATUS_DONE;
		}

		switch ( atoi( (char*)_P1 ) ) {

		case 0:
			/* Set the "power down soft reset" flag so that we do a soft reset: PMU is not powered down */
			MS_SetPowerDownToReset(TRUE);

			SYS_ProcessPowerDownReq( );
			return AT_STATUS_PENDING;

		case 1:
			if (systemState != SYSTEM_STATE_ON) {
				SYS_ProcessPowerUpReq( );
			}
			break;

		case 2:
		case 3:
		case 4:
			if (systemState != SYSTEM_STATE_ON_NO_RF) {
				SYS_ProcessNoRfReq( ) ;
			}
			break;

		case 5:
			//	the "OK" response is sent to signal the PC host
			//	that we're ready to start the download
			AT_CmdRspOK( 0 ) ;
			SysProcessDownloadReq( );
			break;

		case 6:
			SysProcessCalibrateReq( );
			break;

 		case 7:
 			// USB flash loader start
 			SYS_SetMSPowerOnCause(POWER_ON_CAUSE_USB_DL);
 			break;

		default:
			break;
		}

		AT_CmdRspOK( chan ) ;
	}

	else if ( AT_ACCESS_READ == accessType ) {
		systemState = SYS_GetSystemState();

		switch ( systemState ) {
			case SYSTEM_STATE_OFF:
			case SYSTEM_STATE_OFF_IN_PROGRESS:
				cFUN = 0;
				break;

			case SYSTEM_STATE_ON:
				cFUN = 1;
				break;

			case SYSTEM_STATE_ON_NO_RF:
				cFUN = 4;
				break;

			default:
				break;
		}

		sprintf( (char*)atRes, "%s: %d", AT_GetCmdName( AT_CMD_CFUN), cFUN);

		AT_OutputStr( chan, atRes ) ;
		AT_CmdRspOK( chan ) ;
	}

	return AT_STATUS_DONE ;
}

AT_Status_t ATCmd_CMEE_Handler (
	AT_CmdId_t	cmdId,
	UInt8		chan,
	UInt8		accessType,
	const		UInt8* _P1)
{
	UInt8 atRes[64];
	AT_ChannelCfg_t*	cfg = AT_GetChannelCfg( chan ) ;

	if ( accessType == AT_ACCESS_REGULAR  ) {
		cfg->CMEE = _P1 ? atoi( (char*)_P1 ) : 0 ;
	}

	else if ( accessType == AT_ACCESS_READ ) {
		sprintf( (char*)atRes, "%s: %d", (char*)AT_GetCmdName(cmdId), cfg->CMEE ) ;
		AT_OutputStr( chan, atRes );
	}

	AT_CmdRspOK( chan );

	return AT_STATUS_DONE;
}

AT_Status_t ATCmd_CMER_Handler (
	AT_CmdId_t	cmdId,
	UInt8		chan,
	UInt8		accessType,
	const		UInt8* _P1,
	const		UInt8* _P2,
	const		UInt8* _P3,
	const		UInt8* _P4,
	const		UInt8* _P5 )
{
	UInt8			atRes[64];
	AT_ChannelCfg_t*	cfg = AT_GetChannelCfg( chan ) ;

	if ( AT_ACCESS_REGULAR == accessType ) {
		cfg->CMER[0] = _P1? atoi( (char*)_P1 ) : 0 ;
		cfg->CMER[1] = _P2? atoi( (char*)_P2 ) : 0 ;
		cfg->CMER[2] = _P3? atoi( (char*)_P3 ) : 0 ;
		cfg->CMER[3] = _P4? atoi( (char*)_P4 ) : 0 ;
		cfg->CMER[4] = _P5? atoi( (char*)_P5 ) : 0 ;
	}

	else if ( AT_ACCESS_READ == accessType ) {
		sprintf( (char*)atRes, "%s: %d,%d,%d,%d,%d", AT_GetCmdName( AT_CMD_CMER ),
			cfg->CMER[ 0 ],cfg->CMER[ 1 ],
			cfg->CMER[ 2 ],cfg->CMER[ 3 ],
			cfg->CMER[ 4 ] );

		AT_OutputStr( chan, atRes ) ;
	}

	AT_CmdRspOK( chan ) ;
	return AT_STATUS_DONE ;
}

AT_Status_t ATCmd_CMEC_Handler (
	AT_CmdId_t	cmdId,
	UInt8		chan,
	UInt8		accessType,
	const		UInt8* _P1,
	const		UInt8* _P2,
	const		UInt8* _P3 )
{

	//	This command controls the operation of ME keypad, display and indicators,
	//	which are not supported by the card.
	AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_SUPPORTED ) ;
	return AT_STATUS_DONE ;
}

AT_Status_t ATCmd_CLAC_Handler (
	AT_CmdId_t	cmdId,
	UInt8		chan,
	UInt8		accessType )
{
    if ( AT_ACCESS_REGULAR == accessType ) {

		UInt16      i;
		int			full_list_mode;

		full_list_mode = AT_GetCmdListMode();
		for ( i = 0; i < AT_GetNumCmds(); i++ ) {
			AT_CmdId_t r = AT_GetHashRedirect( i ) ;
			if( !(AT_OPTION_HIDE_NAME & AT_GetCmdOptions( r ) ) || (full_list_mode == TRUE)) {
				OSTASK_Sleep( TICKS_ONE_SECOND / 50 );
				AT_OutputStr( chan, AT_GetCmdName( r ) ) ;
			}
		}

		AT_CmdRspOK( chan ) ;
	}
	else {
		AT_CmdRspError( chan, AT_CME_ERR_OP_NOT_ALLOWED );
	}

	return AT_STATUS_DONE ;
}


//At command handler AT+ICF
AT_Status_t ATCmd_ICF_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1,
								const UInt8* _P2){

   UInt8 atRes[32];
   AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);

   /*
   ** ------------------------------------
   **  Regular command  AT+ICF=...
   ** ------------------------------------
   */

   if ( accessType == AT_ACCESS_REGULAR)
   {

      channelCfg->ICF[ 0 ] = _P1 == NULL ? 3 : atoi( (char*)_P1 );
      channelCfg->ICF[ 1 ] = _P2 == NULL ? 3 : atoi( (char*)_P2 );

      // 8 Bits Data 1 Stop, No parity
      // No other character framing currently supported by Mobilink


   }
   else if ( accessType == AT_ACCESS_READ )
   {
      sprintf( (char*)atRes, "%s: %d,%d", (char*)AT_GetCmdName(cmdId),
                    channelCfg->ICF[ 0 ], channelCfg->ICF[ 1 ] );
      AT_OutputStr(chan,atRes);
   }

	AT_CmdRspOK(chan);

    return AT_STATUS_DONE ;
}


AT_Status_t ATCmd_AND_W_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType)
{

	if ( accessType == AT_ACCESS_REGULAR){
		/*
		**  Save Configuration to NVRAM
		*/
		//**FixMe** (to be activated)
		//NVFileWriteBlock(??, (UInt8 *) &channelCfg, ?? );
		AT_CmdRspOK( chan );

	} else if ( accessType == AT_ACCESS_READ ){

		AT_CmdRspError( chan,AT_ERROR_RSP );

	}

	return AT_STATUS_DONE ;

}


//At command handler AT*ECIPC
AT_Status_t ATCmd_ECIPC_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1)
{


	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);
	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,FALSE,0,&channelCfg->ECIPC);

}


//At command handler AT+IPR
AT_Status_t ATCmd_IPR_Handler (AT_CmdId_t cmdId,
								UInt8 chan,
								UInt8 accessType,
								const UInt8* _P1)
{
  UInt8 atRes[32];

   /*
   ** ------------------------------------
   **  Regular command  AT+IPR=...
   ** ------------------------------------
   */
   if ( accessType == AT_ACCESS_REGULAR )
   {

      AT_GetCommonCfg()->IPR = _P1 == NULL ? 9600 : atoi( (char*)_P1 );

#if 0
	  //Reconfig the port --- diable now
      V24_ConfigLink(	chan, AT_GetCommonCfg()->IPR,
						stop_bit,	// Stop bit, 1 = 1 stop bit, 2 = 2 stop bits
						data_bit,	// Data bit, 8 = 8 data bits, 7 = 7 data bits
						parity		// Parity);
#endif
   }


   /*
   ** ------------------------------------
   **  READ command  AT+IPR?
   ** ------------------------------------
   */
   else if ( accessType == AT_ACCESS_READ )
   {
      sprintf( (char*)atRes, "%s: %ld", (char*)AT_GetCmdName(cmdId), AT_GetCommonCfg()->IPR );
      AT_OutputStr( chan, atRes );
   }

   AT_CmdRspOK( chan );

   return AT_STATUS_DONE ;

}


//At command handler ATQ
AT_Status_t ATCmd_Q_Handler(AT_CmdId_t cmdId,
							UInt8 chan,
							UInt8 accessType,
							const UInt8* _P1)
{
	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);
	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,TRUE,0,&channelCfg->Q);
}


//At command handler AT+WS46
AT_Status_t ATCmd_WS46_Handler(AT_CmdId_t cmdId,
							UInt8 chan,
							UInt8 accessType,
							const UInt8* _P1)
{

	AT_ChannelCfg_t*  channelCfg = AT_GetChannelCfg(chan);
	return AT_SimpleCmdHandler(cmdId,chan,accessType,_P1,TRUE,0,&channelCfg->WS46);

}
