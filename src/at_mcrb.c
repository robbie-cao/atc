#define ENABLE_LOGGING

#include <stdio.h>
#include "stdlib.h"

#include "irqctrl.h"
#include "osinterrupt.h"
#include "memmap.h"
#include "mti_build.h"

#include "v24.h"
#include "mstypes.h"
#include "atc.h"
#include "at_data.h"
#include "sdltrace.h"
#include "misc.h"
#include "at_api.h"
#include "at_cfg.h"
#include "sysparm.h"
#include "platform.h"
#include "timezone.h"
#include "sio.h"
#include "ms_database.h"
#include "log.h"
#include "mti_trace.h"
#include "l1cmd.h"
#include "audioapi.h"
#include "pchdef.h"
#include "bandselect.h"
#include "ostask.h"
#include "at_audtune.h"
#include "osheap.h"
#include "macros.h"
#include "sleep.h"


//------- Local Defines -----------
typedef enum
{
	NONE = 0,
	TOGGLE_UNSOLICITED_EVENT,
	TOGGLE_SMS_READ_STATUS_CHANGE,
	SET_GSM_ONLY_ATTACH_MODE,
	SET_PDA_STORAGE_OVERFLOW_FLAG,
	SET_SPECIAL_CGSEND_MODE,
	SET_RAT_AND_BAND,
	SET_GPIO_WAKEUP_RUNTIME,
	SET_HSDPA_CFG,
	SET_DARP_CFG
} ConfigUnsolicit_t;

//--------- Local Variables ------------
#ifdef PDA_SUPPORT
static Boolean	atSendUnsolicitedEvent = TRUE;
#else
static Boolean	atSendUnsolicitedEvent = FALSE;   // default is off for non-PDA product
#endif

//******************************************************************************
//
// Function Name:	ATCmd_MQSC_Handler
//
// Description:		This function handles the received AT*MQSC command
//					(query SIM Class, i.e. whether the inserted SIM is 2G SIM or 3G USIM)
//
//******************************************************************************
AT_Status_t ATCmd_MQSC_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	char temp_buffer[40];

	if (accessType == AT_ACCESS_TEST)
	{
		AT_CmdRspOK(chan);
	}
	else
	{
		if ( SIM_GetPresentStatus() == SIMPRESENT_INSERTED )
		{
			sprintf(temp_buffer, "*MQSC: %d", ((SIM_GetApplicationType() == SIM_APPL_2G) ? 0 : 1));

			AT_OutputStr(chan, (UInt8 *) temp_buffer);
			AT_CmdRspOK(chan);
		}
		else
		{
			AT_CmdRspError(chan, AT_CME_ERR_PH_SIM_NOT_INSERTED);
		}
	}

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATCmd_MQPIN_Handler
//
// Description:		This function handles the received AT*MQPIN command
//					(query PIN1/PIN2/PUK1/PUK2 remaining attempts)
//
//******************************************************************************
AT_Status_t ATCmd_MQPIN_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	AT_Status_t status;

	if (accessType == AT_ACCESS_TEST)
	{
		AT_CmdRspOK(chan);
		status = AT_STATUS_DONE;
	}
	else
	{
		SIM_SendRemainingPinAttemptReq(AT_GetV24ClientIDFromMpxChan(chan), PostMsgToATCTask);
		status = AT_STATUS_PENDING;
	}

	return status;
}


//******************************************************************************
//
// Function Name:	ATCmd_MDNPN_Handler
//
// Description:		This function deletes the NITZ Network Name saved in file system.
//					This is required to pass NITZ GCF TC 44.2.9.1.2.
//
//******************************************************************************
AT_Status_t ATCmd_MDNPN_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	TIMEZONE_DeleteNetworkName();
	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATCmd_MFTD_Handler
//
// Description:		This function handles the received AT*MFTD command
//					(read/write Factory Tracking Data in flash)
//
// Command Syntax:
//	 1. Read Factory Tracking Data Configuration:
//		Command: AT*MFTD?
//		Response: *MFTD: <fact_str_len>,<max_str_len>
//		<fact_str_len>: the length of the string stored in Factory Tracking Data block in flash (can be 0)
//		<max_str_len>: maximum string length for Factory Tracking Data block in flash (now 1024)
//
//   2. Read Factory Tracking Data:
//	    Command: AT*MFTD=0 [,<offset>, <length>]
//		<offset>: 0-based offset for the first byte to read
//		<length>: number of bytes to read
//		If <offset> & <length> are not passed, the whole string stored in Factory Tracking Data block is returned.
//
//		Response:
//		*MFTD: <factory_data_str>
//		<factory_data_str>
//		.....
//		<factory_data_str>
//
//		Each <factory_data_str> contains up to 240 bytes of characters in quotes. If the Factory Tracking Data string
//		contains more than 240 characters, multiple <factory_data_str>'s are returned due to the limitation of our
//		SIO buffer size.
//
//	 3. Write Factory Tracking Data:
//		Two commands are used because our SIO buffers can not receive 1024 bytes of Factory Tracking Data
//		in one single AT command.
//
//		3.1. AT*MFTD=1,<write_flag>,<offset>,<fact_str>
//		This command stores the passed Factory Tracking Data string in a working buffer to be written to flash later.
//		After this command, this data is not saved to flash. This is to be done by the AT*MFTD=2 command.
//		<write_flag>: 0 for partially replacing the original string starting at <offset>
//					  1 for completely replacing the original string starting at <offset>
//
//		<fact_str>: Factory Tracking Data String in quotes with 240 characters maximum.
//
//		For example, the original Factory Tracking Data String is "abcdef".
//		If AT*MFTD=1,0,2,"12" is entered, the new  Factory Tracking Data String is "ab12ef".
//		If AT*MFTD=1,1,2,"12" is entered, the new  Factory Tracking Data String is "ab12".
//
//		3.2 AT*MFTD=2
//		This command flushes the Factory Tracking Data buffered in our working buffer to flash.
//		An AT*MFTD=1,<write_flag>,<offset>,<fact_str> must have been entered before.
//
//******************************************************************************
#define MAX_MFTD_STR_SIZE 240

AT_Status_t ATCmd_MFTD_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1, const UInt8* _P2, const UInt8* _P3, const UInt8* _P4)
{
	/* Static data for the intermediate steps in writing Factory Tracking Data */
	static UInt8 *write_data_buf = NULL;
	static Boolean first_write_cmd = TRUE;

	UInt32 track_data_addr = MS_GetFactTrackDataAddr();
	UInt32 track_data_len = MS_GetFactTrackDataSize();
	UInt16 str_len;
	UInt16 offset;
	char temp_buffer[MAX_MFTD_STR_SIZE + 50];
	UInt8 *read_data_buf;

	if (accessType == AT_ACCESS_REGULAR)
	{
		UInt16 read_len;
		Boolean error_flag = FALSE;

		switch ( atoi((char *) _P1) )
		{
		case 0:	/* Read Factory Tracking Data command */
		{
			read_data_buf = OSHEAP_Alloc(track_data_len + 1);	/* Add one for NULL terminator */

			if ( FlashLoadImage(track_data_addr, track_data_len, read_data_buf) )
			{
				/* New flash has 0xFF by default */
				if (read_data_buf[0] == 0xFF)
				{
					read_data_buf[0] = '\0';
				}

				read_data_buf[track_data_len] = '\0';
				str_len = strlen((char *) read_data_buf);

				if ( (_P2 == NULL) && (_P3 == NULL) )
				{
					offset = 0;
					read_len = str_len;
				}
				else if ( (_P2 != NULL) && (_P3 != NULL) )
				{
					offset = atoi((char *) _P2);
					read_len = atoi((char *) _P3);

					if (offset > str_len)
					{
						error_flag = TRUE;
					}
					else if ( ((offset + read_len) > str_len) || (read_len == 0) )
					{
						read_len = str_len - offset;
					}
				}
				else
				{
					error_flag = TRUE;
				}

				if (error_flag)
				{
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				}
				else if (read_len == 0)
				{
					sprintf(temp_buffer, "*MFTD: \"\"");
					AT_OutputStr(chan, (UInt8 *) temp_buffer);

					AT_CmdRspOK(chan);
				}
				else
				{
					Boolean first_rsp = TRUE;
					char track_data_str[MAX_MFTD_STR_SIZE + 1];
					UInt16 copy_len;
					UInt8 *ptr = read_data_buf + offset;

					while (read_len > 0)
					{
						copy_len = MIN(MAX_MFTD_STR_SIZE, read_len);

						memcpy(track_data_str, ptr, copy_len);
						track_data_str[copy_len] = '\0';

						read_len -= copy_len;
						ptr += copy_len;

						if (first_rsp)
						{
							sprintf(temp_buffer, "*MFTD: \"%s\"", track_data_str);
						}
						else
						{
							sprintf(temp_buffer, "\"%s\"", track_data_str);
						}

						if (first_rsp)
						{
							first_rsp = FALSE;
						}
						else
						{
							/* Sleep some time so that we do not overflow the SIO buffer */
							OSTASK_Sleep(20);
						}

						AT_OutputStr(chan, (UInt8 *) temp_buffer);
					}

					AT_CmdRspOK(chan);
				}
			}
			else
			{
				AT_CmdRspError(chan, AT_CME_ERR_PHONE_FAILURE);
			}

			OSHEAP_Delete(read_data_buf);

			break;
		}

		case 1: /* Buffer Factory Tracking Data command for writing */
		{
			UInt16 write_len;
			UInt8 overwrite_flag;

			if ( (_P2 == NULL) || (_P3 == NULL) || (_P4 == NULL) )
			{
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			overwrite_flag = atoi((char *) _P2);
			offset =  atoi((char *) _P3);
			write_len = strlen((char *) _P4);

			if ( ((offset + write_len) > track_data_len) || (write_len > MAX_MFTD_STR_SIZE) )
			{
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			if (first_write_cmd)
			{
				assert(write_data_buf == NULL);
				write_data_buf = OSHEAP_Alloc(track_data_len + 1); /* Add one for NULL terminator */

				if ( FlashLoadImage(track_data_addr, track_data_len, write_data_buf) )
				{
					if (write_data_buf[0] == 0xFF)
					{
						write_data_buf[0] = '\0';
					}

					write_data_buf[track_data_len] = '\0';
				}
				else
				{
					OSHEAP_Delete(write_data_buf);
					write_data_buf = NULL;

					AT_CmdRspError(chan, AT_CME_ERR_PHONE_FAILURE);
					return AT_STATUS_DONE;
				}
			}

			if ( offset > strlen((char *) write_data_buf) )
			{
				if (first_write_cmd)
				{
					OSHEAP_Delete(write_data_buf);
					write_data_buf = NULL;
				}

				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			first_write_cmd = FALSE;

			str_len = strlen((char *) write_data_buf);

			memcpy(write_data_buf + offset, _P4, write_len);

			if (overwrite_flag == 0)
			{
				if ((offset + write_len) > str_len)
				{
					write_data_buf[offset + write_len] = '\0';
				}
			}
			else
			{
				write_data_buf[offset + write_len] = '\0';
			}

			AT_CmdRspOK(chan);

			break;
		}

		case 2:	/* Write the buffered Factory Tracking Data */
			if (write_data_buf == NULL)
			{
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			}
			else
			{
				if ( FlashSaveImage(track_data_addr, track_data_len, write_data_buf) )
				{
					AT_CmdRspOK(chan);
				}
				else
				{
					AT_CmdRspError(chan, AT_CME_ERR_PHONE_FAILURE);
				}

				OSHEAP_Delete(write_data_buf);
				write_data_buf = NULL;
			}

			first_write_cmd = TRUE;

			break;

		default:
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);

			break;
		}
	}
	else if (accessType == AT_ACCESS_READ)
	{
		read_data_buf = OSHEAP_Alloc(track_data_len + 1);	/* Add one for NULL terminator */

		if ( FlashLoadImage(track_data_addr, track_data_len, read_data_buf) )
		{
			if (read_data_buf[0] == 0xFF)
			{
				str_len = 0;
			}
			else
			{
				read_data_buf[track_data_len] = '\0';
				str_len = strlen((char *) read_data_buf);
			}

			sprintf(temp_buffer, "*MFTD: %d, %d", str_len, track_data_len);
			AT_OutputStr(chan, (UInt8 *) temp_buffer);

			AT_CmdRspOK(chan);
		}
		else
		{
			AT_CmdRspError(chan, AT_CME_ERR_PHONE_FAILURE);
		}

		OSHEAP_Delete(read_data_buf);
	}
	else
	{
		AT_CmdRspOK(chan);
	}

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATCmd_MQPV_Handler
//
// Description:		This function reads one of the following parameters from the phone.
//					1. Software Version
//					2. Ind Sysparm File Version
//					3. Chip ID and Chip ID Revision
//
//******************************************************************************
AT_Status_t ATCmd_MQPV_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	extern UInt8 const *pjRevision;

	char temp_buffer[100];
	Boolean result_ok = TRUE;

	if (accessType == AT_ACCESS_REGULAR)
	{
		switch (atoi((char *) _P1))
		{
		case 0:
			sprintf(temp_buffer, "*MQPV: \"%s\"", (char *)  pjRevision);
			break;

		case 1:
			sprintf(temp_buffer, "*MQPV: %d", SYSPARM_GetSysparmIndPartFileVersion());
			break;

		case 2:
			sprintf(temp_buffer, "*MQPV: %d, %d", GET_CHIPID(), GET_CHIP_REVISION_ID());
			break;

		case 3:
			sprintf(temp_buffer, "*MQPV: \"%s\"", PATCH_GetRevision());
			break;

		default:
			result_ok = FALSE;
			break;
		}
	}

	if (result_ok)
	{
		AT_OutputStr(chan, (UInt8 *) temp_buffer);
		AT_CmdRspOK(chan);
	}
	else
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
	}

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATCmd_MSALS_Handler
//
// Description:		This function selects the Alternative Line Service (ALS)
//					active line if ALS is enabled. Otherwise CME ERROR 3 is
//					returned.
//
//******************************************************************************
AT_Status_t ATCmd_MSALS_Handler (AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	if(SIM_IsALSEnabled())
	{
		if(accessType == AT_ACCESS_READ)
		{
			char temp_buffer[30];

			sprintf(temp_buffer, "*MSALS: %d", SIM_GetAlsDefaultLine());
			AT_OutputStr(chan, (UInt8 *) temp_buffer);
		}
		else if(accessType == AT_ACCESS_REGULAR)
		{
			SIM_SetAlsDefaultLine(atoi((char *) _P1));
		}
		else
		{
			AT_OutputStr(chan, (UInt8 *) "*MSALS: (0-1)");
		}

		AT_CmdRspOK(chan);
	}
	else
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
	}

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	AtHandleSimPinAttemptRsp
//
// Description:		This function handles the PIN1/PIN2/PUK1/PUK2 remaining attempts
//					returned from the SIM task.
//
// Notes:
//
//******************************************************************************
void AtHandleSimPinAttemptRsp(InterTaskMsg_t* inMsg)
{
	PIN_ATTEMPT_RESULT_t *pin_attempt = (PIN_ATTEMPT_RESULT_t *) inMsg->dataBuf;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	char temp_buffer[60];

	if(pin_attempt->result == SIMACCESS_SUCCESS)
	{
		sprintf( temp_buffer, "%s: %d, %d, %d, %d", AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num)),
					pin_attempt->pin1_attempt_left, pin_attempt->puk1_attempt_left,
					pin_attempt->pin2_attempt_left, pin_attempt->puk2_attempt_left );

		AT_OutputStr(mpx_chan_num, (UInt8*) temp_buffer) ;
		AT_CmdRspOK(mpx_chan_num);
	}
	else
	{
		AT_CmdRspError( mpx_chan_num,
			(pin_attempt->result == SIMACCESS_NO_SIM) ? AT_CME_ERR_PH_SIM_NOT_INSERTED : AT_CME_ERR_SIM_FAILURE );
	}
}


#ifdef GCXX_PC_CARD

//******************************************************************************
//
// Function Name:	ATCmd_MCUSID_Handler
//
// Description:		This callback function handles the AT*MCUSID command.
//                  The command is to query the Customization (DSF file) ID String.
//
//******************************************************************************
AT_Status_t ATCmd_MCUSID_Handler( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	char temp_buffer[60];

	sprintf(temp_buffer, "*MCUSID: \"%s\"", SECDATA_GetCustomizationID());
	AT_OutputStr(chan, (UInt8 *) temp_buffer) ;
	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATCmd_MSDNLD_Handler
//
// Description:		This function handles the received AT*MSDNLD command
//					(go into download mode)
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MSDNLD_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	extern void __call_via_r0(UInt32);
	extern void SecureDownload(void);
	IRQMask_t mask;

	AT_CmdRspOK(chan);
	*((UInt8 *) PUMR_REG) = SEL_SECURITY_DOWNLOAD;

	OSTASK_Sleep(TICKS_ONE_SECOND);
	OSINTERRUPT_DisableAll();
	IRQ_DisableAll(&mask);

	/* We are now running at 52 MHz. Need to set the MSP clock back to 19.5 MHz.
     * This is needed as when we jump to the secure boot loader below, the SRAM
     * access cycle setting is based on the assumption that we are running at 19.5 MHz.
     */
#if CHIPVERSION < CHIP_VERSION(BCM2132,00) /* chips earlier than BCM2132 */
	*((volatile UInt32 *) ACR_REG) = *((volatile UInt32 *) ACR_REG) & ~0x00001C00; // SET MSP CLK TO 19.5 MHZ
	__call_via_r0(ROM_BASE);	// Soft-reboot (not reset) the unit
#else // #if CHIPVERSION < CHIP_VERSION(BCM2132,00) /* chips earlier than BCM2132 */
	SecureDownload();
#endif // #if CHIPVERSION < CHIP_VERSION(BCM2132,00) /* chips earlier than BCM2132 */

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATCmd_ESIMC_Handler
//
// Description:		This function handles the received AT*ESIMC command.
//					If ESIMC is set to 1, the unsolicited SIM Detection message
//					"*ESIMM: (0,1)" is sent out to the host.
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_ESIMC_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	char temp_buffer[30];

	if(accessType == AT_ACCESS_READ)
	{
		sprintf(temp_buffer, "%s: %d", (char *) AT_GetCmdName(cmdId), AT_GetCommonCfg()->ESIMC);
		AT_OutputStr( chan, (UInt8*)temp_buffer ) ;
	}
	else if(accessType == AT_ACCESS_REGULAR)
	{
		AT_GetCommonCfg()->ESIMC = ((_P1 == NULL) || (_P1[0] == '\0')) ? 0 : atoi((char *) _P1);
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}

#endif // GCXX_PC_CARD

static MODULE_READY_STATUS_t	curr_status = MODULE_INVALID_IND;


//******************************************************************************
//
// Function Name:	ATC_ReadMrdy_CB
//
// Description:		Process the unsolicited ms ready status
//
// Notes:
//
//******************************************************************************
void AtHandleMsReadyInd( InterTaskMsg_t *inMsg )
{
	MODULE_READY_STATUS_t status = *(MODULE_READY_STATUS_t*)inMsg->dataBuf;
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	// Only report the status if the new status is different from the current status
	// For phonebook ready status, do not filter it because this may be the indication
	// of the end of phonebook refresh so that the host can begin re-reading phonebook.
	if( (status != curr_status) || (status == MODULE_ALL_AT_READY_READY) )
	{
		sprintf( (char*)rspBuf, "%s: %d", "*MRDY", status );
		AT_OutputUnsolicitedStr( rspBuf );

		curr_status = status;
	}
}



//******************************************************************************
// Function Name: 	ATCmd_MTEST_Handler
//
// Description:		Handle AT*MTEST command
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MTEST_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								const UInt8* _P1, const UInt8* _P2, const UInt8* _P3)
{

	TestCommand_t	testCmd;		// test parameter
	UInt16			inputParm1;		// input 1
	UInt8*			inputParm2;		// input 2
	UInt16			err = 1;


	switch(accessType){
		case AT_ACCESS_REGULAR:
			testCmd = (TestCommand_t)atoi((char*)_P1);
			inputParm1 = atoi((char*)_P2);
			inputParm2 = (UInt8*)_P3;

			// run the test command
			err = ExecuteTest(testCmd, inputParm1, inputParm2);
			break;

		default:	// ignore query
			break;

	} // end switch(accessType)-loop

	if(err){
		AT_CmdRspSyntaxError( chan ) ;
	}
	else{
		AT_CmdRspOK(chan);
	}

	return AT_STATUS_DONE;
}


//******************************************************************************
// Function Name: 	ATCmd_MICID_Handler
//
// Description:		Handle AT*MICID command (read SIM Chip ID).
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MICID_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	if(accessType != AT_ACCESS_TEST)
	{
		ICCID_ASCII_t iccid;
		char temp_buffer[40];

		(void) SIM_GetIccid(&iccid);

		sprintf(temp_buffer, "%s: \"%s\"", (char *) AT_GetCmdName(cmdId), (char *) &iccid);
		AT_OutputStr(chan, (UInt8 *) temp_buffer);
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATCmd_MCNFG_Handler
//
// Description:		Transition on AT*MCNFG
//
// Notes:			This is a configuration command. When type set to 1, mode
//					will turns on/off all unsolicited AT events by
//					calling the function SetATUnsolicitedEvent().
//
//					type = 1: turn on/off unsolicited event
//						   2: ...
//						   3: Manual GPRS attach mode
//						   4:
//						   5:
//						   6: set RAT and band
//						   7: change GPIO WAKEUP setting. 1:Enable, 0:Disable
//						   8: turn on/off HSDPA with catagory
//
//					mode = 0: turn off; = 1: turn on
//
//******************************************************************************

AT_Status_t ATCmd_MCNFG_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								 const UInt8* _P1, const UInt8* _P2, const UInt8* _P3)
{
	Boolean mode;
	RATSelect_t RATselect;
	BandSelect_t bandselect;

	ConfigUnsolicit_t	type;

	type = (ConfigUnsolicit_t)((_P1 != NULL) ? atoi((char*)_P1): 0);

	if(type == 0){
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
		return AT_STATUS_DONE;
	}

	if ( accessType == AT_ACCESS_REGULAR )
	{
		_TRACE(("MCNFG: type = ", type));
		switch(type)
		{
			case TOGGLE_UNSOLICITED_EVENT:
				if(_P2){
					mode = atoi((char*)_P2);
					SetATUnsolicitedEvent(mode);

					// set up CNMI to receive unsolicited sms indication
					SMS_SetNewMsgDisplayPref(SMS_MT, 1);

					_TRACE(("MCNFG: mode = ", mode));
				}
				else{
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
					return AT_STATUS_DONE;
				}
				break;

			case TOGGLE_SMS_READ_STATUS_CHANGE:
				if(_P2){
					mode = atoi((char*)_P2);
					SMS_SetSmsReadStatusChangeMode(mode);
				}
				else{
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
					return AT_STATUS_DONE;
				}
				break;

			case SET_GSM_ONLY_ATTACH_MODE:
				if(_P2){
					mode = atoi((char*)_P2);
					if (mode) {
						MS_SetAttachMode(ATTACH_MODE_GSM_ONLY);
					}
					else {
						MS_SetAttachMode(ATTACH_MODE_GSM_GPRS);
					}
				}
				else{
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
					return AT_STATUS_DONE;
				}
				break;

			case SET_PDA_STORAGE_OVERFLOW_FLAG:
				if(_P2){
					mode = atoi((char*)_P2);

					if( SMS_SetPDAStorageOverFlowFlag(mode) != RESULT_OK ){
						AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
						return AT_STATUS_DONE;
					}
				}
				break;

			case SET_SPECIAL_CGSEND_MODE:
				if(_P2){
					mode = atoi((char*)_P2);

					if( mode ){
						isSpecialCGSENDEnabled = TRUE;
					}
					else {
						isSpecialCGSENDEnabled = FALSE;
					}
				}
				break;
			case SET_RAT_AND_BAND:
				if (_P2)
				{
					RATselect = atoi((char*)_P2);
				}
				else
				{
					RATselect = GSM_ONLY;
				}

				if (_P3)
				{
					bandselect = atoi((char*)_P3);
				}
				else
				{
					bandselect = BAND_NULL;
				}
				if (MS_SetSupportedRATandBand(RATselect, bandselect) != RESULT_OK)
				{
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
					return AT_STATUS_DONE;
				}
				break;

			case SET_GPIO_WAKEUP_RUNTIME:
				if(_P2){
					mode = atoi((char*)_P2);

					if (!MS_SetGPIOWakeup(mode)) {
						AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
						return AT_STATUS_DONE;
					}
				}
				else{
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED );
					return AT_STATUS_DONE;
				}
				break;

#ifdef  UMTS_HSDPA
			case SET_HSDPA_CFG:
				if (SYS_GetSystemState() != SYSTEM_STATE_OFF)
				{
					MSG_LOG("AT+mcnfg=8,x should be entered before at+cfun=1." );
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}
				SYSPARM_SetHSDPAPHYCategory(atoi((char*)_P2));
				break;
#endif // UMTS_HSDPA


			case SET_DARP_CFG:
				if (SYS_GetSystemState() != SYSTEM_STATE_OFF)
				{
					MSG_LOG("AT+mcnfg=9,x should be entered before at+cfun=1." );
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}
				SYSPARM_SetDARPCfg(atoi((char*)_P2));
				break;


			default:
				break;
		}
	}

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}


//----------------------------------------------------------------------
// ATCmd_MVERS_Handler(): AT*MVERS Cmd Handler for return version info
//----------------------------------------------------------------------
AT_Status_t ATCmd_MVERS_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	UInt8	tempBuf[AT_RSPBUF_SM+1];

	if(accessType == AT_ACCESS_REGULAR)
	{
		UInt8	BootLoaderVersion[80];
		UInt8	DSFVersion[80];

		// formuate phone version info (firmware/DSP patch/sysparm)
		sprintf( (char*)tempBuf, "%s/DSP Patch: %s/SysparmRev: 0x%03X\r\n",
			     SYSPARM_GetSWVersion(), (char*)PATCH_GetRevision(),
				 SYSPARM_GetSysparmIndPartFileVersion() );
		AT_OutputStr(chan, tempBuf);

		// formulate boot loader and DSF versions info
		SYS_GetBootLoaderVersion(BootLoaderVersion);
		SYS_GetDSFVersion(DSFVersion);
		sprintf( (char*)tempBuf, "BootLoaderRev: %s/DSFRev: %s\r\n",
			     BootLoaderVersion, DSFVersion);
		AT_OutputStr(chan, tempBuf);

		AT_CmdRspOK(chan);
	}
	else{
		// return syntax error
		AT_CmdRspSyntaxError(chan) ;
	}

	return AT_STATUS_DONE;
}
//******************************************************************************
//
// Function Name:	ATCmd_MPSEL_Handler
//
// Description:		Transition on AT*MPSEL
//
// Notes:			Select ATC port
//					AT*MPSEL="A"
//					AT*MPSEL="C"
//******************************************************************************

AT_Status_t ATCmd_MPSEL_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								 const UInt8* _P1)
{
	UInt8			selectPort;
	char			string[10];
	char			output[50];

	switch ( accessType ) {
		case AT_ACCESS_REGULAR:
			strcpy( string, ( _P1 == NULL ) ? NOT_USED_STRING : (char*)_P1 );
			if (!strcmp(string,"A")) {
				ATC_ConfigSerialDevForATC(UART_A);
			}
			else if (!strcmp(string,"C")) {
				ATC_ConfigSerialDevForATC(UART_C);
			}
			else {
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			break;

		case AT_ACCESS_READ:
			selectPort = (UInt8)ATC_GetCurrentSIOPort();

			sprintf( output, "%s: ", AT_GetCmdName(cmdId) );
			if (selectPort==PortA)
				strcat( output, "A" );
			else if (selectPort==PortC)
				strcat( output, "C" );

			AT_OutputStr( chan, (const UInt8 *)output );
			break;

		case AT_ACCESS_TEST:
			AT_OutputStr( chan, (const UInt8 *)"*MPSEL: (A,C)" );
			break;
		default:
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			return AT_STATUS_DONE;
	}

	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}

//******************************************************************************
//
// Function Name:	AtHandleNetworkNameInd
//
// Description:		Handle Network Name indication
//
// Notes:
//
//******************************************************************************
void AtHandleNetworkNameInd( InterTaskMsg_t *inMsg )
{
	nitzNetworkName_t* networkName = (nitzNetworkName_t*)inMsg->dataBuf;
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	if(strlen((char*)networkName->shortName) > 0){

		// prevent from writing beyond buffer
		if(strlen((char*)networkName->shortName) > AT_RSPBUF_SM-20){
			networkName->shortName[AT_RSPBUF_SM-20] = '\0';
		}

		sprintf( (char*)rspBuf, "%s: %d, \"%s\"", "*MTZ", 1, networkName->shortName);
	}
	else{  // invalid ShortName from network

		if(strlen((char*)networkName->longName) > 0){

			// prevent from writing beyond buffer
			if(strlen((char*)networkName->longName) > AT_RSPBUF_SM-20){
				networkName->shortName[AT_RSPBUF_SM-20] = '\0';
			}

			sprintf( (char*)rspBuf, "%s: %d, \"%s\"", "*MTZ", 1, networkName->longName);
		}
		else{
			sprintf( (char*)rspBuf, "%s: %d, \"\"", "*MTZ", 1);
		}
	}

	AT_OutputUnsolicitedStr( rspBuf );
}

//******************************************************************************
//
// Function Name:	AtHandleTimeZoneInd
//
// Description:		Handle Network Timezone indication
//
// Notes:
//
//******************************************************************************
void AtHandleTimeZoneInd( InterTaskMsg_t *inMsg )
{
	TimeZoneDate_t* timeZoneDate = (TimeZoneDate_t*)inMsg->dataBuf;
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	sprintf( (char*)rspBuf, "%s: %d, \"%02d/%02d/%4d, %02d:%02d:%02d%s%02d\", %d", "*MTZ", 2,
				timeZoneDate->adjustedTime.Month,
				timeZoneDate->adjustedTime.Day,
				timeZoneDate->adjustedTime.Year,
				timeZoneDate->adjustedTime.Hour,
				timeZoneDate->adjustedTime.Min,
				timeZoneDate->adjustedTime.Sec,
				(timeZoneDate->timeZone >= 0) ? "+" : "-",
				(timeZoneDate->timeZone < 0) ? -timeZoneDate->timeZone : timeZoneDate->timeZone,
				timeZoneDate->dstAdjust);

	AT_OutputUnsolicitedStr( rspBuf );
}

//******************************************************************************
//
// Function Name:	ATCmd_MTZ_Handler
//
// Description:		1. Set/Get nwtwork time update mode.
//					2. Comfirm or deny the update request of Network Time
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MTZ_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
							  const UInt8* _P1, const UInt8* _P2)
{
	UInt8	rspBuf[ AT_RSPBUF_SM ] ;

	if(accessType == AT_ACCESS_REGULAR)
	{
		if(_P2){
			TIMEZONE_UpdateRTC((Boolean)(atoi((char *) _P2)));
		}

		if(_P1){
			TIMEZONE_SetTZUpdateMode(atoi((char*)_P1));
		}
	}
	else if(accessType == AT_ACCESS_READ)
	{
		sprintf( (char*)rspBuf, "%s: %d", "*MTZ", TIMEZONE_GetTZUpdateMode());
		AT_OutputUnsolicitedStr( rspBuf );
	}

	AT_CmdRspOK(chan);
	return AT_STATUS_DONE;
}



//-----------------------------------------------------------------
// SetATUnsolicitedEvent(): enable/disable unsolicited event to host
//------------------------------------------------------------------
void SetATUnsolicitedEvent(Boolean on_off)
{
	atSendUnsolicitedEvent = on_off;
}


//-----------------------------------------------------------------
// IsATUnsolicitedEventEnabled():
//------------------------------------------------------------------
Boolean IsATUnsolicitedEventEnabled()
{
	return atSendUnsolicitedEvent;
}



//******************************************************************************
//
// Function Name:	ATCmd_MECNS_Handler
//
// Description:		Echo cancellation and Noise Suppression control.
//
// Notes:
//
//******************************************************************************
#define  ECNS_AT_CMD_ENABLE_EC				1
#define  ECNS_AT_CMD_ENABLE_NS				2




AT_Status_t ATCmd_MECNS_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								const UInt8* _P1, const UInt8* _P2)

{
	short		cmd;
	Boolean		on_off;
	Boolean		isCmdSuccessful = TRUE;
	char		rspBuf[AT_RSPBUF_SM] ;


	cmd = (_P1 == NULL) ? 0 : atoi((char*)_P1);

	if(accessType == AT_ACCESS_REGULAR)			// AT*MECNS=<cmd>, <p1>, ...
	{
		switch(cmd){
			case ECNS_AT_CMD_ENABLE_EC:			// enable/disable EC
				on_off = (Boolean) ((char*)_P2 == NULL)? FALSE: atoi((char*)_P2);
				AUDIO_EnableEC(on_off);
				break;

			case ECNS_AT_CMD_ENABLE_NS:			// enable/disable NS
				on_off = (Boolean) (_P2 == NULL)? FALSE: atoi((char*)_P2);
				AUDIO_EnableNS(on_off);
				break;

			default:
				isCmdSuccessful = FALSE;
				break;

		} // end switch(cmd)-loop
	}
	else if(accessType == AT_ACCESS_READ){	// AT*MECNS?
		// query EC and NS on/off state
		sprintf(rspBuf, "*MECNS: %d, %d",
				 (AUDIO_IsECEnabled() ? 1: 0),			// EC enabled
				 (AUDIO_IsNSEnabled() ? 1: 0)  );		// NS state

		AT_OutputStr(chan, (UInt8*)rspBuf);

	}
	else{
		isCmdSuccessful = FALSE;
	}  // end if(accessType)-loop


	if(isCmdSuccessful){
		AT_CmdRspOK(chan);
	}
	else{
		AT_CmdRspSyntaxError(chan);
	}


	return AT_STATUS_DONE;

}

//******************************************************************************
// Function Name: 	ATCmd_MPROF_Handler
//
// Description:		Handle AT*MPROF command
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MPROF_Handler (
	AT_CmdId_t cmdId,
	UInt8 ch,
	UInt8 accessType,
	const UInt8* _P1
	)
{
	int		profCmd;
	UInt16	err = 0;
	char	rspBuf[AT_RSPBUF_SM];

#ifdef HISTORY_LOGGING

	switch(accessType){
		case AT_ACCESS_REGULAR:
			profCmd = atoi((char*)_P1);
			switch (profCmd)
			{
				case 0:
					OSTASK_DisableProfile();
					break;
				case 1:
					OSTASK_ResetProfile();
					break;
				case 2:
                    if ( OSTASK_IsProfileOn() )
					{
                    	OSTASK_ProfileDump();
					}
					else
					{
						err = 1;
					}
					break;
				default:
					err = 1;
					break;
			}
			break;

		// AT*MPROF?
		case AT_ACCESS_READ:
			sprintf(rspBuf, "*MPROF: %d", (OSTASK_IsProfileOn() ? 1 : 0) );
			AT_OutputStr(ch, (UInt8*)rspBuf);
			break;

		default:
			err = 1;
			break;
	}

	if(err){
		AT_CmdRspSyntaxError( ch ) ;
	}
	else{
		AT_CmdRspOK(ch);
	}

#else

	AT_CmdRspSyntaxError( ch ) ;

#endif
	return AT_STATUS_DONE;
}

//******************************************************************************
// Function Name: 	ATCmd_MSLEEP_Handler
//
// Description:		Handle AT*MSLEEP command
//					AT*MSLEEP=0		disable deep sleep
//					AT*MSLEEP=1		enable deep sleep
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MSLEEP_Handler (
	AT_CmdId_t cmdId,
	UInt8 ch,
	UInt8 accessType,
	const UInt8* _P1
	)
{
	int		profCmd;
	UInt16	err = 0;
	char	rspBuf[AT_RSPBUF_SM] = {0};

	switch(accessType){
		case AT_ACCESS_REGULAR:
			profCmd = atoi((char*)_P1);
			switch (profCmd)
			{
				case 0:
					/* FixMe: comment out until function is defined */
					// SLEEP_DisableDeepSleepFunc();
					break;
				case 1:
					/* FixMe: comment out until function is defined */
					// SLEEP_EnableDeepSleepFunc();
					break;
				default:
					err = 1;
					break;
			}
			break;
		case AT_ACCESS_READ: //  AT*MSLEEP?
			sprintf(rspBuf, "*MSLEEP: %d", (IsDeepSleepEnabled() ? 1 : 0) );
			AT_OutputStr(ch, (UInt8*)rspBuf);
			break;
		default:
			err = 1;
			break;
	}

	if(err){
		AT_CmdRspSyntaxError( ch ) ;
	}
	else{
		AT_CmdRspOK(ch);
	}

	return AT_STATUS_DONE;
}

//******************************************************************************
// Function Name: 	ATCmd_MRESET_Handler
//
// Description:		Handle AT*MRESET command
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_MRESET_Handler (
	AT_CmdId_t cmdId,
	UInt8 ch,
	UInt8 accessType,
	const UInt8* _P1
	)
{
	OSTASK_Sleep(TICKS_ONE_SECOND);
	RebootProcessor();
}

//***********************************************************************************
//
// Following are functions related to Audio Tuning AT commands
//
//***********************************************************************************

#define _AT_AUDTUNE_TST_
//comment following line for debugging
#undef _AT_AUDTUNE_TST_

//local functions prototype
static	UInt8 this_MAUDTUNE_GetParmType(AudioParam_t audioParam);
static	Int16 this_MAUDTUNE_GetParmCnt(AudioParam_t audioParam);
static	Int32 this_MAUDTUNE_GetParmLrange(AudioParam_t audioParam);
static	Int32 this_MAUDTUNE_GetParmUrange(AudioParam_t audioParam);

static	void* this_MAUDTUNE_GetParm(AudioParam_t audioParam, UInt8 chan);
static	Result_t this_MAUDTUNE_SetParm(AudioParam_t audioParam, UInt8 chan, char *parm);

//used for both
// - Getparm: Convert AUDIO_GetAudioParam() format and output to AT port
// - SetParm: convert AT input parms to AUDIO_SetAudioParam() format
__align(4) static	char this_MAUDTUNE_TempBuffer[512];

//***********************************************************************************
// Function Name:	ATCmd_MAUDTUNE_Handler
//
// Description:		Audio tune
//
// Notes:			AT*MAUDTUNE=<cmd>,<NULL|mode|parm-index|save>,<NULL|parm-list>
//
// Output:			*MAUDTUNE: [more|list of param]
//
//***********************************************************************************
AT_Status_t ATCmd_MAUDTUNE_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								   const UInt8* _P1, const UInt8* _P2, const UInt8* _P3)
{
	AudioTuneCommand_t	cmd;
	AudioMode_t mode;
	Boolean save=FALSE;
	AudioParam_t audioParam;
	char temp_buffer[50];

#ifdef _AT_AUDTUNE_TST_
	sprintf(temp_buffer, "_P3 len = %d, _P3 = ", strlen( (char *)_P3));
	AT_OutputStr(chan, (UInt8 *)temp_buffer);
	AT_OutputStr(chan, (UInt8 *)_P3);
#endif //_AT_AUDTUNE_TST_

	cmd = (_P1 == NULL) ? 0 : atoi((char*)_P1);

	switch (cmd)
	{
		case AUD_CMD_GETMODE:
			mode = AUDIO_GetAudioMode();
			sprintf(this_MAUDTUNE_TempBuffer, "*MAUDTUNE: [%d]", mode);
			AT_OutputStr(chan, (UInt8 *) this_MAUDTUNE_TempBuffer);
			AT_CmdRspOK(chan);
			break;

		case AUD_CMD_SETMODE:
			//default AUDIO_MODE_HEADSET
			mode =  (_P2 == NULL) ? 0 : atoi((char*)_P2);

			if ( AUDIO_MODE_NUMBER <= mode )
			{
				AT_CmdRspError(chan, AT_AUD_SETMODE_INVALID);
				return AT_STATUS_DONE;
			}

			AUDIO_SetAudioMode( mode );
			AT_CmdRspOK(chan);
			break;

		case AUD_CMD_GETPARM:

			if ( NULL == _P2 )
			{
				AT_CmdRspError(chan, AT_AUD_PARMID_MISSING);
				return AT_STATUS_DONE;
			}

			audioParam =  atoi((char*)_P2);

			if ( AUDIO_PARM_NUMBER <= audioParam )
			{
				AT_CmdRspError(chan, AT_AUD_PARMID_INVALID);
				return AT_STATUS_DONE;
			}

			if ( NULL == this_MAUDTUNE_GetParm(audioParam, chan) )
			{
				AT_CmdRspError(chan, AT_AUD_GETPARM_DATA_UNAVAILABLE);
				return AT_STATUS_DONE;
			}

			AT_CmdRspOK(chan);
			break;

		case AUD_CMD_SETPARM:
			if ( NULL == _P2 || NULL == _P3)
			{
				AT_CmdRspError(chan, AT_AUD_PARMID_MISSING);
				return AT_STATUS_DONE;
			}

			audioParam = atoi((char*)_P2);
			if ( RESULT_ERROR == this_MAUDTUNE_SetParm(audioParam, chan, (char *)_P3) )
			{
				AT_CmdRspError(chan, AT_AUD_SETPARM_ERROR);
			}
			else
			{
				AT_CmdRspOK(chan);
			}
			break;

		case AUD_CMD_STARTTUNE:
			if ( RESULT_ERROR ==  AUDIO_StartTuning() )
			{	//usually it means MS is already in tuning mode
				AT_CmdRspError(chan, AT_AUD_STARTTUNE_ERROR);
			}
			else
			{
				AT_CmdRspOK(chan);
			}
			break;

		case AUD_CMD_STOPTUNE:
			if ( NULL == _P2 )
			{
				AT_CmdRspError(chan, AT_AUD_STOPTUNE_SAVE_MISSING);
				return AT_STATUS_DONE;
			}

			save = atoi((char*)_P2);
			if ( RESULT_ERROR ==  AUDIO_StopTuning( save ) )
			{	//usually it means MS is NOT in tuning mode
				AT_CmdRspError(chan, AT_AUD_STARTTUNE_ERROR);
			}
			else
			{
				AT_CmdRspOK(chan);
			}
			break;

		default:
			AT_CmdRspError(chan, AT_AUD_COMMAND_ERROR);
			break;

	}

	return AT_STATUS_DONE;

} //ATCmd_MAUDTUNE_Handler()


//***********************************************************************************
//
// Local Function:	this_MAUDTUNE_SetParm
//
//***********************************************************************************
static	Result_t this_MAUDTUNE_SetParm(AudioParam_t audioParam, UInt8 chan, char *parm)
{
	AudioParmType_t type;
	Int16 i;
	Int16 cnt;
	char *thisPtr=this_MAUDTUNE_TempBuffer;
	char *tmpStrPtr;

	Int8 thisInt8;
	UInt8 thisUInt8;
	Int16 thisInt16;
	UInt16 thisUInt16;
	Int32 thisInt32;
	UInt32 thisUInt32;

	UInt8 token_cnt;

	Result_t	ret;

	thisPtr=this_MAUDTUNE_TempBuffer;

	type = this_MAUDTUNE_GetParmType( audioParam );
	cnt = this_MAUDTUNE_GetParmCnt( audioParam );


//extern void *memcpy(void * /*s1*/, const void * /*s2*/, size_t /*n*/);

	switch (type)
	{
		case AUD_DATA_TYPE_INT8:
			thisInt8 = atoi((char*)parm);
			memcpy( thisPtr, (void *)&thisInt8, sizeof(Int8));
			break;

		case AUD_DATA_TYPE_INT8_ARRAY:
			tmpStrPtr = strtok(parm,AUDIO_TUNE_PARM_DELIMITERS);
			while (NULL != tmpStrPtr)
			{
				thisInt8 = atoi((char*)tmpStrPtr);
				memcpy( thisPtr, (void *)&thisInt8, sizeof(Int8));
				thisPtr+=sizeof(Int8);
				tmpStrPtr = strtok(NULL,AUDIO_TUNE_PARM_DELIMITERS);
			}
			break;

		case AUD_DATA_TYPE_UINT8:
			thisUInt8 = atoi((char*)parm);
			memcpy( (void *)thisPtr, (void *)&thisUInt8, sizeof(UInt8));
			break;

		case AUD_DATA_TYPE_UINT8_ARRAY:
			tmpStrPtr = strtok(parm,AUDIO_TUNE_PARM_DELIMITERS);
			while (NULL != tmpStrPtr)
			{
				thisUInt8 = atoi((char*)tmpStrPtr);
				memcpy( thisPtr, (void *)&thisUInt8, sizeof(UInt8));
				thisPtr+=sizeof(UInt8);
				tmpStrPtr = strtok(NULL,AUDIO_TUNE_PARM_DELIMITERS);
			}
			break;

		case AUD_DATA_TYPE_INT16:
			thisInt16 = atoi((char*)parm);
			memcpy( (void *)thisPtr, (void *)&thisInt16, sizeof(Int16));
			break;

		case AUD_DATA_TYPE_INT16_ARRAY:
			tmpStrPtr = strtok(parm,AUDIO_TUNE_PARM_DELIMITERS);
			while (NULL != tmpStrPtr)
			{
				thisInt16 = atoi((char*)tmpStrPtr);
				memcpy( thisPtr, (void *)&thisInt16, sizeof(Int16));
				thisPtr+=sizeof(Int16);
				tmpStrPtr = strtok(NULL,AUDIO_TUNE_PARM_DELIMITERS);
			}
			break;

		case AUD_DATA_TYPE_UINT16:
			thisUInt16 = atoi((char*)parm);
			memcpy( (void *)thisPtr, (void *)&thisUInt16, sizeof(UInt16));
			break;

		case AUD_DATA_TYPE_UINT16_ARRAY:
			tmpStrPtr = strtok(parm,AUDIO_TUNE_PARM_DELIMITERS);
			while (NULL != tmpStrPtr)
			{
				thisUInt16 = atoi((char*)tmpStrPtr);
				memcpy( thisPtr, (void *)&thisUInt16, sizeof(UInt16));
				thisPtr+=sizeof(UInt16);
				tmpStrPtr = strtok(NULL,AUDIO_TUNE_PARM_DELIMITERS);
			}
			break;

		case AUD_DATA_TYPE_INT32:
			thisInt32 = atoi((char*)parm);
			memcpy( (void *)thisPtr, (void *)&thisInt32, sizeof(Int32));
			break;

		case AUD_DATA_TYPE_INT32_ARRAY:
			tmpStrPtr = strtok(parm,AUDIO_TUNE_PARM_DELIMITERS);
			while (NULL != tmpStrPtr)
			{
				thisInt32 = atoi((char*)tmpStrPtr);
				memcpy( thisPtr, (void *)&thisInt32, sizeof(Int32));
				thisPtr+=sizeof(Int32);
				tmpStrPtr = strtok(NULL,AUDIO_TUNE_PARM_DELIMITERS);
			}
			break;

		case AUD_DATA_TYPE_UINT32:
			thisUInt32 = atoi((char*)parm);
			memcpy( (void *)thisPtr, (void *)&thisUInt32, sizeof(UInt32));
			break;

		case AUD_DATA_TYPE_UINT32_ARRAY:
			tmpStrPtr = strtok(parm,AUDIO_TUNE_PARM_DELIMITERS);
			while (NULL != tmpStrPtr)
			{
				thisUInt32 = atoi((char*)tmpStrPtr);
				memcpy( thisPtr, (void *)&thisUInt32, sizeof(UInt32));
				thisPtr+=sizeof(UInt32);
				tmpStrPtr = strtok(NULL,AUDIO_TUNE_PARM_DELIMITERS);
			}
			break;

		default:
			return RESULT_ERROR;
			break;

	}

	ret =  AUDIO_SetAudioParam(audioParam, (void *)this_MAUDTUNE_TempBuffer);

#ifdef _AT_AUDTUNE_TST_
		AT_OutputStr(chan, (UInt8 *)this_MAUDTUNE_TempBuffer);
#endif

	return ret;
} //this_MAUDTUNE_SetParm()


//***********************************************************************************
//
// Local Function:	this_MAUDTUNE_GetParm
//
// Output:	*MAUDTUNE: [more|list of param]
//***********************************************************************************
static void* this_MAUDTUNE_GetParm(AudioParam_t audioParam, UInt8 chan)
{
	void *parm=NULL;
	AudioParmType_t type;
	Int16 cnt;

	UInt16 i;
	char itoaStr[15];	//max # digist of uin32/int32 with sign char is 11.

	parm = AUDIO_GetAudioParam( audioParam );

	if (NULL == parm)
	{
		return NULL;
	}

	type = this_MAUDTUNE_GetParmType( audioParam );
	cnt = this_MAUDTUNE_GetParmCnt( audioParam );

	//post-consturct
	this_MAUDTUNE_TempBuffer[0]=0;
	strcat(this_MAUDTUNE_TempBuffer, "*MAUDTUNE: [");

	switch (type)
	{
		case AUD_DATA_TYPE_INT8:
			sprintf(itoaStr, "%d", *(Int8 *)(parm));
			strcat(this_MAUDTUNE_TempBuffer, itoaStr);
			break;

		case AUD_DATA_TYPE_INT8_ARRAY:
			{
				Int8 *thisPtr = (Int8 *)parm;
				for ( i=0; i<cnt; i++)
				{
					sprintf(itoaStr, "%u", *thisPtr++);
					strcat(this_MAUDTUNE_TempBuffer, itoaStr);
					strcat(this_MAUDTUNE_TempBuffer, AUDIO_TUNE_PARM_DELIMITERS);
				}
			}
			break;

		case AUD_DATA_TYPE_UINT8:
			sprintf(itoaStr, "%u", *(UInt8 *)(parm));
			strcat(this_MAUDTUNE_TempBuffer, itoaStr);
			break;

		case AUD_DATA_TYPE_UINT8_ARRAY:
			{
				UInt8 *thisPtr = (UInt8 *)parm;
				for ( i=0; i<cnt; i++)
				{
					sprintf(itoaStr, "%u", *thisPtr++);
					strcat(this_MAUDTUNE_TempBuffer, itoaStr);
					strcat(this_MAUDTUNE_TempBuffer, AUDIO_TUNE_PARM_DELIMITERS);
				}
			}
			break;

		case AUD_DATA_TYPE_INT16:
			sprintf(itoaStr, "%d", *(Int16 *)(parm));
			strcat(this_MAUDTUNE_TempBuffer, itoaStr);
			break;

		case AUD_DATA_TYPE_INT16_ARRAY:
			{
				Int16 *thisPtr = (Int16 *)parm;
				for ( i=0; i<cnt; i++)
				{
					sprintf(itoaStr, "%u", *thisPtr++);
					strcat(this_MAUDTUNE_TempBuffer, itoaStr);
					strcat(this_MAUDTUNE_TempBuffer, AUDIO_TUNE_PARM_DELIMITERS);
				}
			}
			break;

		case AUD_DATA_TYPE_UINT16:
			sprintf(itoaStr, "%u", *(UInt16 *)(parm));
			strcat(this_MAUDTUNE_TempBuffer, itoaStr);
			break;

		case AUD_DATA_TYPE_UINT16_ARRAY:
			{
				UInt16 *thisPtr = (UInt16 *)parm;
				for ( i=0; i<cnt; i++)
				{
					sprintf(itoaStr, "%u", *thisPtr++);
					strcat(this_MAUDTUNE_TempBuffer, itoaStr);
					strcat(this_MAUDTUNE_TempBuffer, AUDIO_TUNE_PARM_DELIMITERS);
				}
			}
			break;

		case AUD_DATA_TYPE_INT32:
			sprintf(itoaStr, "%ld", *(Int32 *)(parm));
			strcat(this_MAUDTUNE_TempBuffer, itoaStr);
			break;

		case AUD_DATA_TYPE_INT32_ARRAY:
			{
				Int32 *thisPtr = (Int32 *)parm;
				for ( i=0; i<cnt; i++)
				{
					sprintf(itoaStr, "%ld", *thisPtr++);
					strcat(this_MAUDTUNE_TempBuffer, itoaStr);
					strcat(this_MAUDTUNE_TempBuffer, AUDIO_TUNE_PARM_DELIMITERS);
				}
			}
			break;

		case AUD_DATA_TYPE_UINT32:
			sprintf(itoaStr, "%ld", *(UInt32 *)(parm));
			strcat(this_MAUDTUNE_TempBuffer, itoaStr);
			break;

		case AUD_DATA_TYPE_UINT32_ARRAY:
			{
				UInt32 *thisPtr = (UInt32 *)parm;
				for ( i=0; i<cnt; i++)
				{
					sprintf(itoaStr, "%ld", *thisPtr++);
					strcat(this_MAUDTUNE_TempBuffer, itoaStr);
					strcat(this_MAUDTUNE_TempBuffer, AUDIO_TUNE_PARM_DELIMITERS);
				}
			}
			break;

		default:
			break;

	}
	strcat(this_MAUDTUNE_TempBuffer, "]");
	AT_OutputStr(chan, (UInt8 *) this_MAUDTUNE_TempBuffer);

#ifdef _AT_AUDTUNE_TST_
	{
		char temp_buffer[40];
		sprintf(temp_buffer, "output len = %d", strlen( (char *)this_MAUDTUNE_TempBuffer));
		AT_OutputStr(chan, (UInt8 *)temp_buffer);
	}
#endif //_AT_AUDTUNE_TST_

	return parm;


} //this_MAUDTUNE_GetParm()



//***********************************************************************************
//
// Local Function:	this_MAUDTUNE_GetParmType
//
// Get parameter type
//***********************************************************************************
static UInt8 this_MAUDTUNE_GetParmType(AudioParam_t audioParam)
{
	UInt16 i;

	for ( i=0; i<AUDIO_PARM_NUMBER; i++ )
	{
		if ( audioParam == AudioParmEntryTable[i].AudioParmID )
			return (UInt8)AudioParmEntryTable[i].parmType;
	}
} //this_MAUDTUNE_GetParmType()


//***********************************************************************************
//
// Local Function:	this_MAUDTUNE_GetParmCnt
//
// Get parameter count
//
// NOTE: Current desing: AT does not check/verify this field from the table
//       Neither does checking for variable number of parameters for one entry (-1) exists.
//***********************************************************************************
static Int16 this_MAUDTUNE_GetParmCnt(AudioParam_t audioParam)
{
	UInt16 i;

	for ( i=0; i<AUDIO_PARM_NUMBER; i++ )
	{
		if ( audioParam == AudioParmEntryTable[i].AudioParmID )
			return AudioParmEntryTable[i].parm_cnt;
	}
} //this_MAUDTUNE_GetParmCnt()


//***********************************************************************************
//
// Local Function:	this_MAUDTUNE_GetParmLrange
//
// Get parameter lower range
//
// NOTE: Current desing: AT does not check/verify this field from the table
//***********************************************************************************
static Int32 this_MAUDTUNE_GetParmLrange(AudioParam_t audioParam)
{
	UInt16 i;

	for ( i=0; i<AUDIO_PARM_NUMBER; i++ )
	{
		if ( audioParam == AudioParmEntryTable[i].AudioParmID )
			return AudioParmEntryTable[i].lower_range;
	}
} //this_MAUDTUNE_GetParmLrange()


//***********************************************************************************
//
// Local Function:	this_MAUDTUNE_GetParmLrange
//
// Get parameter upper range
//
// NOTE: Current desing: AT does not check/verify this field from the table
//***********************************************************************************
static Int32 this_MAUDTUNE_GetParmUrange(AudioParam_t audioParam)
{
	UInt16 i;

	for ( i=0; i<AUDIO_PARM_NUMBER; i++ )
	{
		if ( audioParam == AudioParmEntryTable[i].AudioParmID )
			return AudioParmEntryTable[i].upper_range;
	}
} //this_MAUDTUNE_GetParmUrange()



