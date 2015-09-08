/**
*
*   @file   at_sim.c
*
*   @brief  This file contains all SIM related call functions, including
*			callback by SIM_MI, callback by ATC parser and callback for
*			ATK event.
*
****************************************************************************/

//*****************************************************************************
//                                  include block
//*****************************************************************************
#include "stdio.h"
#include "stdlib.h"

#include "mti_trace.h"
#include "v24.h"
#include "nvram.h"
#include "sysparm.h"
#include "atc.h"
#include "at_data.h"
#include "at_util.h"
#include "at_sim.h"
#include "sim_api.h"
#include "bcd.h"
#include "stk.h"
#include "ms_database.h"
#include "ss_api.h"
#include "util.h"

#include "pchdef.h"
#include "osheap.h"
#include "kpd_drv.h"


//******************************************************************************
//		Definition
//******************************************************************************
#define GOTO_ERROR goto ERROR_LABEL


extern	UInt8	atcClientID;

//******************************************************************************
//		Typedefs
//******************************************************************************


// FAC_ID_PS to FAC_ID_PC should be at beginning for the matching of encrypt_id
typedef enum
{
	FAC_ID_PS,		// lock phone to SIM
	FAC_ID_PN,		// Network personalization
	FAC_ID_PU,		// Network subset personalization
	FAC_ID_PP,		// Service provider personalization
	FAC_ID_PC,		// Corporate personalization
	FAC_ID_AO,									// Barring all outgoing calls
	FAC_ID_OI,									// Barring all outgoing international calls
	FAC_ID_AI,									// Barring all incoming calls
	FAC_ID_IR,									// Barring incoming calls while roaming outside home area
	FAC_ID_OX,									// Barring outgoing international except to home country
	FAC_ID_NM,									// Barring incoming calls from number not stored to ME memory
	FAC_ID_NS,									// Barring incoming calls from numbers not stored to SIM memory
	FAC_ID_NA,									// Barring incoming calls from numbers not stored to any memory
	FAC_ID_AB,									// All barring services
	FAC_ID_AG,									// All outgoing barring services
	FAC_ID_AC,									// All incoming barring services
	FAC_ID_SC,									// SIM PIN enable/disable
	FAC_ID_FD,									// Fixed dialing
	FAC_ID_PIN2,
	TOTAL_FAC
}ATCFacID_t;

typedef struct
{
   ATCFacID_t		 id;
   UInt8			*name;
   SS_CallBarType_t	 callBarSvcCode ;		// service code if call bar request
}ATCFacTab_t;


//******************************************************************************
//		Prototypes
//******************************************************************************


//******************************************************************************
//		Variables
//******************************************************************************
static const ATCFacTab_t fac_tab[] =
{
   {FAC_ID_PS, (UInt8 *)"PS",	0	}, 					   // lock phone to SIM
   {FAC_ID_PN, (UInt8 *)"PN",	0	},					   // Network personalization
   {FAC_ID_PU, (UInt8 *)"PU",	0	},					   // Network subset personalization
   {FAC_ID_PP, (UInt8 *)"PP",	0	},					   // Service provider personalization
   {FAC_ID_PC, (UInt8 *)"PC",	0	},					   // Corporate personalization
   {FAC_ID_AO, (UInt8 *)"AO",	SS_CALLBAR_TYPE_OUT_ALL					},	// Barring all outgoing calls
   {FAC_ID_OI, (UInt8 *)"OI",	SS_CALLBAR_TYPE_OUT_INTL				}, 	// Barring all outgoing international calls
   {FAC_ID_AI, (UInt8 *)"AI",	SS_CALLBAR_TYPE_INC_ALL					},	// Barring all incoming calls
   {FAC_ID_IR, (UInt8 *)"IR",	SS_CALLBAR_TYPE_INC_ROAM_OUTSIDE_HPLMN	}, 	// Barring incoming calls while roaming outside home area
   {FAC_ID_OX, (UInt8 *)"OX",	SS_CALLBAR_TYPE_OUT_INTL_EXCL_HPLMN		},	// Barring outgoing international except to home country
   {FAC_ID_NM, (UInt8 *)"NM",	0	}, 					   // Barring incoming calls from number not stored to ME memory
   {FAC_ID_NS, (UInt8 *)"NS",	0	},					   // Barring incoming calls from numbers not stored to SIM memory
   {FAC_ID_NA, (UInt8 *)"NA",	0	}, 					   // Barring incoming calls from numbers not stored to any memory
   {FAC_ID_AB, (UInt8 *)"AB",	SS_CALLBAR_TYPE_ALL						},		// All barring services
   {FAC_ID_AG, (UInt8 *)"AG",	SS_CALLBAR_TYPE_OUTG					},		// All outgoing barring services
   {FAC_ID_AC, (UInt8 *)"AC",	SS_CALLBAR_TYPE_INC						},		// All incoming barring services
   {FAC_ID_SC, (UInt8 *)"SC",	0	},					   // SIM PIN enable/disable
   {FAC_ID_FD, (UInt8 *)"FD",	0	},					   // Fixed dialing
   {FAC_ID_PIN2, (UInt8 *)"P2", 0	}					   // Corporate personalization
};


static void atcsim_SendPinChangeReq(UInt8 chan, Boolean is_pin1, char *old_pin, char *new_pin);

static Boolean Convert_PPUStr (UInt8* eppu_str, EPPU_t *eppu_ptr);

static UInt16 getAtPukStatus(SIMLockType_t simlock_type);

static const char* getPinCodeStr(SIM_PIN_Status_t pin_status);


//******************************************************************************
//
// Function Name:	atcsim_SendPinChangeReq
//
// Description:		This function sends the request to change the PIN1/PIN2 password
//					to the SIM task.
//
//******************************************************************************
static void atcsim_SendPinChangeReq(UInt8 chan, Boolean is_pin1, char *old_pin, char *new_pin)
{
	CHVString_t old_pin_str, new_pin_str;

	strncpy((char *) &old_pin_str, old_pin, sizeof(old_pin_str) - 1);
	((char *) &old_pin_str)[sizeof(old_pin_str) - 1] = '\0';

	strncpy((char *) &new_pin_str, new_pin, sizeof(new_pin_str) - 1);
	((char *) &new_pin_str)[sizeof(new_pin_str) - 1] = '\0';

	SIM_SendChangeChvReq(
			AT_GetV24ClientIDFromMpxChan(chan), // MPX channel
			is_pin1 ? CHV1 : CHV2,		// CHV selected
			old_pin_str,				// Attempted current CHV (null-term.)
			new_pin_str,				// Attempted new CHV (null-term.)
			PostMsgToATCTask			// Call back function
		);
}


//******************************************************************************
//
// Function Name:	Convert_PPUStr
//
// Description:		Convert eppu string into eppu structure. Return TRUE if
//                  conversion successful, otherwise return FALSE.
//
// Notes:   The passed eppu string can be modified in this function!!!
//
//******************************************************************************
static Boolean Convert_PPUStr (UInt8* eppu_str, EPPU_t *eppu_ptr)
{
	UInt32	temp;
	UInt32	temp1;
	UInt8	*temp_ptr = NULL;
	UInt8	i;
	UInt8   j;

	/* Get rid of useless leading zero's */
	while( (eppu_str[0] == '0') && (eppu_str[1] != '\0') && (eppu_str[1] != '.') )
	{
		eppu_str++;
	}

	/* Scan the string for invalid characters and look for the '.' position */
	i = 0;
	while(eppu_str[i] != '\0')
	{
		if(eppu_str[i] == '.')
		{
			if( (temp_ptr != NULL) || (eppu_str[i + 1] == '\0') || (i == 0) )
			{
				/* More than one dots found or dot in the last or first position: wrong */
				return FALSE;
			}
			else
			{
				/* Point to the dot position */
				temp_ptr = &eppu_str[i];
				*temp_ptr = '\0';
			}
		}
		else if( (eppu_str[i] < '0') || (eppu_str[i] > '9') )
		{
			/* Non-digit character found: wrong */
			return FALSE;
		}

		i++;
	}

	/* GSM 11.11 specifies 12 bits are available to store EPPU value and 3
	 * bits for the exponent value in SIM:
	 * thus max EPPU value is 0xFFF = 4095 in decimal (4 digits).
	 * max exponent value is 7.
	 */
	j = strlen((char *) eppu_str); /* Get the number of digits of the integer part */

	if( j > (4 + 7) )
	{
		/* Number is too large */
		return FALSE;
	}
	else if( j >= 4 )
	{
		/* Limit to the first 4 digits: leading 0 has been eliminated above */
		eppu_str[4] = '\0';

		if( (temp = atoi((char *) eppu_str)) > 0xFFF )
		{
			if( j == (4 + 7) )
			{
				/* Number is too large */
				return FALSE;
			}
			else
			{
				eppu_ptr->mant = temp / 10;
				eppu_ptr->exp = j - 4 + 1;
			}
		}
		else
		{
			eppu_ptr->mant = temp;
			eppu_ptr->exp = j - 4;
		}

		/* We are done, exponent is in range 0-7, no fraction part */
		return TRUE;
	}

	/* If we get here, we know that the integer part of the string contains 3 or
	 * fewer digits.
	 */
	temp = atoi((char *) eppu_str);

	if(temp_ptr == NULL)
	{
		/* No dot found in decimal string */
		eppu_ptr->mant = temp;
		eppu_ptr->exp = 0;
	}
	else
	{
		j = i - (temp_ptr - eppu_str + 1); /* j holds number of digits after dot */
		temp_ptr++; /* Point to the first digit after dot */

		/* Try to store as many fraction digits as possible, up to 4 digits */
		for(i = 0; i < j; i++)
		{
			/* GSM 11.11 reserves only 3 bits for exponent: max value is 7 */
			if(i >= 7)
			{
				break;
			}

			temp1 = temp * 10 + (temp_ptr[i] - '0');

			if(temp1 > 0x00000FFF)
			{
				break;
			}
			else
			{
				temp = temp1;
			}
		}

		eppu_ptr->mant = temp;
		eppu_ptr->exp = 0 - i;
	}

	return TRUE;
}


/************************************************************************************
*
*	Function Name:	getAtPukStatus
*
*	Description:	Return the corresponding AT PUK status for the passed SIMLOCK type.
*
*************************************************************************************/
static UInt16 getAtPukStatus(SIMLockType_t simlock_type)
{
	UInt16 at_puk_status;

	switch(simlock_type)
	{
	case SIMLOCK_NETWORK_LOCK:
		at_puk_status = AT_CME_ERR_NETWORK_LOCK_PUK_REQD;
		break;

	case SIMLOCK_NET_SUBSET_LOCK:
		at_puk_status = AT_CME_ERR_SUBNET_LOCK_PUK_REQD;
		break;

	case SIMLOCK_PROVIDER_LOCK:
		at_puk_status = AT_CME_ERR_SP_LOCK_PUK_REQD;
		break;

	case SIMLOCK_CORP_LOCK:
		at_puk_status = AT_CME_ERR_CORP_LOCK_PUK_REQD;
		break;

	case SIMLOCK_PHONE_LOCK:
	default:
		/* Phone lock does not have PUK status */
		at_puk_status = AT_CME_ERR_PH_SIM_PIN_REQD;
		break;
	}

	return at_puk_status;
}

/************************************************************************************
*
*	Function Name:	getPinCodeStr
*
*	Description:	This function returns the string corresponding to PIN status.
*
************************************************************************************/
static const char* getPinCodeStr(SIM_PIN_Status_t pin_status)
{
	const struct PIN_STR
	{
		SIM_PIN_Status_t	pin_status;
		const char			*pin_str;

	} Pin_Str_Table[] = {	{PIN_READY_STATUS,		"READY"},
							{SIM_PIN_STATUS,		"SIM PIN"},
							{SIM_PUK_STATUS,		"SIM PUK"},
							{PH_SIM_PIN_STATUS,		"PH-SIM PIN"},
							{PH_SIM_PUK_STATUS,		"PH-SIM PUK"},
							{SIM_PIN2_STATUS,		"SIM PIN2"},
							{SIM_PUK2_STATUS,		"SIM PUK2"},
							{PH_NET_PIN_STATUS,		"PH-NET PIN"},
							{PH_NET_PUK_STATUS,		"PH-NET PUK"},
							{PH_NETSUB_PIN_STATUS,	"PH-NETSUB PIN"},
							{PH_NETSUB_PUK_STATUS,	"PH-NETSUB PUK"},
							{PH_SP_PIN_STATUS,		"PH-SP PIN"},
							{PH_SP_PUK_STATUS,		"PH-SP PUK"},
							{PH_CORP_PIN_STATUS,	"PH-CORP PIN"},
							{PH_CORP_PUK_STATUS,	"PH-CORP PUK"}	};

	UInt8 i;

	for(i = 0; i < (sizeof(Pin_Str_Table) / sizeof(struct PIN_STR)); i++)
	{
		if( Pin_Str_Table[i].pin_status == pin_status )
		{
			return Pin_Str_Table[i].pin_str;
		}
	}

	return ((const char *) "");
}


/************************************************************************************
 *							Global Funtions
 ***********************************************************************************/

/************************************************************************************
*
*	Function Name:	ATCmd_CIMI_Handler
*
*	Description:	This function handles the AT+CIMI command (read IMSI).
*
*	Notes:
*
*************************************************************************************/
AT_Status_t ATCmd_CIMI_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	if(accessType == AT_ACCESS_REGULAR)
	{
		AT_OutputStr( chan, (UInt8*)SIM_GetIMSI() ) ;
	}

	AT_CmdRspOK(chan) ;
	return AT_STATUS_DONE ;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CKPD_Handler
*
*	Description:	This function handles the AT+CKPD command (keypad command).
*
*   Notes:
*************************************************************************************/
AT_Status_t ATCmd_CKPD_Handler ( AT_CmdId_t cmdId, UInt8 chan,
								 UInt8 accessType,
								 const UInt8* _P1,
								 const UInt8* _P2,
								 const UInt8* _P3)
{
	SIM_PIN_Status_t curr_PIN_status = SIM_GetPinStatus();
	char temp_buffer[30];

	//Based on the 27.22.7.5.1, any keypad press should trigger the user activity event download
	//no matter what is the command result.

	if( (accessType != AT_ACCESS_READ) && (accessType != AT_ACCESS_TEST) )
	{
		SATK_SendUserActivityEvent();
	}

	if( (accessType == AT_ACCESS_READ) || (accessType == AT_ACCESS_TEST) )
	{
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE ;
	}
	else if( strcmp((char *) _P1, "*#06#") == 0 )
	{
		UTIL_ExtractImei( (UInt8*)temp_buffer );
		AT_OutputStr( chan, (UInt8*)temp_buffer ) ;
		AT_CmdRspOK(chan);

		return AT_STATUS_DONE ;
	}
	else if( (strncmp((char *) _P1, "**04*", strlen("**04*")) == 0) ||	/* Change PIN1 ? */
		     (strncmp((char *) _P1, "**042*", strlen("**042*")) == 0) )	/* Change PIN2 ? */
	{
		/* The string should look like (GSM 02.30):
		 * PIN:		**04*OLD_PIN*NEW_PIN*NEW_PIN#
		 * PIN2:	**042*OLD_PIN2*NEW_PIN2*NEW_PIN2#
		 */
		char *ptr;
		char *ptr0;
		char *ptr1;
		char *ptr2;

		/* Only allow changing PIN1/PIN2 when PIN is ready and if changing PIN1,
		 * PIN1 must be enabled (see GSM 11.11)
		 */
		if( (curr_PIN_status != PIN_READY_STATUS) || ((_P1[4] == '*') && (!SIM_IsPINRequired())) )
		{
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			return AT_STATUS_DONE;
		}

		/* Point to the beginning of OLD_PIN or OLD_PIN2 */
		ptr = (char *) ((_P1[4] == '2') ? &_P1[6] : &_P1[5]);

		/* Point to the '*' denoting beginning of the first NEW_PIN or NEW_PIN2 */
		ptr0 = strchr( ptr, '*' );

		if(ptr0 != NULL)
		{
			/* Point to the '*' denoting beginning of the second NEW_PIN or NEW_PIN2 */
			ptr1 = strchr( ptr0 + 1, '*' );
		}
		else
		{
			ptr1 = NULL;
		}

		if(ptr1 != NULL)
		{
			/* Point to the '#' denoting the end of the second NEW_PIN or NEW_PIN2 */
			ptr2 = strchr( ptr1 + 1, '#' );
		}

		/* Perform string error checking */
		if( (ptr0 == NULL) || (ptr1 == NULL) || (ptr2 == NULL) ||
			( (ptr0 - ptr) < CHV_MIN_LENGTH) ||
			( (ptr0 - ptr) > CHV_MAX_LENGTH) ||
			( (ptr1 - ptr0 - 1) < CHV_MIN_LENGTH) ||
			( (ptr1 - ptr0 - 1) > CHV_MAX_LENGTH) ||
			( (ptr1 - ptr0) != (ptr2 - ptr1) ) ||
			(memcmp(ptr0, ptr1, ptr1 - ptr0) != 0) )

		{
			AT_CmdRspError(chan, AT_CME_ERR_SIM_PASSWD_WRONG);
			return AT_STATUS_DONE;
		}

		/* Terminate OLD_PIN (or OLD_PIN2) and NEW_PIN (or NEW_PIN2) */
		*ptr0 = '\0';
		*ptr1 = '\0';

		atcsim_SendPinChangeReq(chan, (_P1[4] == '*'), ptr, (ptr0 + 1));


		return AT_STATUS_PENDING;
	}
	else if( (strncmp((char *) _P1, "**05*", strlen("**05*")) == 0) ||	/* Unblock PIN1 ? */
		     (strncmp((char *) _P1, "**052*", strlen("**052*")) == 0) )	/* Unblock PIN2 ? */
	{
		/* The string should look like (GSM 02.30):
		 * PIN:		**05*PIN_UNBLOCKING_KEY*NEW_PIN*NEW_PIN#
		 * PIN2:	**052*PIN2_UNBLOCKING_KEY*NEW_PIN2*NEW_PIN2#
		 */
		char *ptr;
		char *ptr0;
		char *ptr1;
		char *ptr2;

		/* PIN status checking */
		switch(curr_PIN_status)
		{
			case PIN_READY_STATUS:
			case SIM_PIN2_STATUS:
			case SIM_PUK2_STATUS:
				break;

			case SIM_PIN_STATUS:
			case SIM_PUK_STATUS:
				if(_P1[4] == '2')
				{
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}
				break;

			default:
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
		}

		/* Point to the beginning of PIN_UNBLOCKING_KEY or PIN2_UNBLOCKING_KEY */
		ptr = (char *) ((_P1[4] == '2') ? &_P1[6] : &_P1[5]);

		/* Point to the '*' denoting beginning of the first NEW_PIN or NEW_PIN2 */
		ptr0 = strchr( ptr, '*' );

		if(ptr0 != NULL)
		{
			/* Point to the '*' denoting beginning of the second NEW_PIN or NEW_PIN2 */
			ptr1 = strchr( ptr0 + 1, '*' );
		}
		else
		{
			ptr1 = NULL;
		}

		if(ptr1 != NULL)
		{
			/* Point to the '#' denoting the end of the second NEW_PIN or NEW_PIN2 */
			ptr2 = strchr( ptr1 + 1, '#' );
		}

		/* Perform string checking */
		if( (ptr0 == NULL) || (ptr1 == NULL) || (ptr2 == NULL) ||
			( (ptr0 - ptr) < PUK_MIN_LENGTH) ||
			( (ptr0 - ptr) > PUK_MAX_LENGTH) ||
			( (ptr1 - ptr0 - 1) < CHV_MIN_LENGTH) ||
			( (ptr1 - ptr0 - 1) > CHV_MAX_LENGTH) ||
			( (ptr1 - ptr0) != (ptr2 - ptr1) ) ||
			(memcmp(ptr0, ptr1, ptr1 - ptr0) != 0) )

		{
			AT_CmdRspError(chan, AT_CME_ERR_SIM_PASSWD_WRONG);
			return AT_STATUS_DONE;
		}

		*ptr0 = '\0';
		*ptr1 = '\0';

		SIM_SendUnblockChvReq( AT_GetV24ClientIDFromMpxChan(chan), (_P1[4] == '*') ? CHV1 : CHV2,
					(UInt8 *) ptr, (UInt8 *) (ptr0 + 1), PostMsgToATCTask );


		return AT_STATUS_PENDING;
	}
	else if( ( accessType == AT_ACCESS_REGULAR ) && ( _P1 != NULL) )
	{
		char c = 0x00;
		UInt8 k = 0;
		UInt8 time  = _P2 == NULL ? 10 : atoi( (char*)_P2 ); // 100 ms default
		UInt8 pause = _P3 == NULL ? 10 : atoi( (char*)_P3 ); // 100 ms default
		time = time/10 ;
		pause =  pause/10  ;

		while ( c = _P1[k])
		{

			switch( c )
			{
				case '#':// key pound;
					KPD_DRV_Inject(KEY_POUND, KEY_ACTION_PRESS );
					OSTASK_Sleep( time* (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_POUND, KEY_ACTION_RELEASE );
					break;

				case '%':// key percent; **FixMe Invalid for now**
					KPD_DRV_Inject(KEY_INVALID, KEY_ACTION_PRESS );
					OSTASK_Sleep( time* (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_INVALID, KEY_ACTION_RELEASE );
					break;

				case '*'://key star;
					KPD_DRV_Inject(KEY_STAR, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_STAR, KEY_ACTION_RELEASE );
					break;

				case '<'://key left;
					KPD_DRV_Inject(KEY_LEFT, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_LEFT, KEY_ACTION_RELEASE );
					break;

				case '>'://key right;
					KPD_DRV_Inject(KEY_RIGHT, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_RIGHT, KEY_ACTION_RELEASE );
					break;

				case 'c':// key clear;
				case 'C':
					KPD_DRV_Inject(KEY_CLEAR, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_CLEAR, KEY_ACTION_RELEASE );
					break;

				case'e':// key end.
				case'E':
					KPD_DRV_Inject(KEY_END, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_END, KEY_ACTION_RELEASE );
					break;

				case'f':// key select:
				case'F':
					KPD_DRV_Inject(KEY_SELECT, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_SELECT, KEY_ACTION_RELEASE );
					break;

				case'p':// key camera **FixMe.. Invalid for now**.
				case'P':
					KPD_DRV_Inject(KEY_INVALID, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_INVALID, KEY_ACTION_RELEASE );
					break;

				case 's'://key send.
				case 'S':
					KPD_DRV_Inject(KEY_SEND, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_SEND, KEY_ACTION_RELEASE );
					break;

				case'u':// key volume up;
				case'U':
					KPD_DRV_Inject(KEY_VOL_UP, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_VOL_UP, KEY_ACTION_RELEASE );
					break;

				case'd':// volume down
				case'D':
					KPD_DRV_Inject(KEY_VOL_DOWN, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_VOL_DOWN, KEY_ACTION_RELEASE );
					break;

				case'^':// key up;
					KPD_DRV_Inject(KEY_UP, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_UP, KEY_ACTION_RELEASE );
					break;

				case'v':// key down;
				case'V':
					KPD_DRV_Inject(KEY_DOWN, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_DOWN, KEY_ACTION_RELEASE );
					break;

				case'w':// key WAP **FixMe.. Invalid for now ;
				case'W':
					KPD_DRV_Inject(KEY_INVALID, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_INVALID, KEY_ACTION_RELEASE );
					break;

				case'x':// key EXTANSWER **FixMe.. Invalid for now;
				case'X':
					KPD_DRV_Inject(KEY_INVALID, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_INVALID, KEY_ACTION_RELEASE );
					break;

				case'y':// key back **FixMe.. Invalid for now;
				case'Y':
					KPD_DRV_Inject(KEY_PB, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_PB, KEY_ACTION_RELEASE );
					break;

				case'[':// key soft left;
					KPD_DRV_Inject(KEY_SKL, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_SKL, KEY_ACTION_RELEASE );
					break;

				case']':// key soft right;
					KPD_DRV_Inject(KEY_SKR, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(KEY_SKR, KEY_ACTION_RELEASE );
					break;

				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					KPD_DRV_Inject(c - 0x30, KEY_ACTION_PRESS );
					OSTASK_Sleep( time * (TICKS_ONE_SECOND/10) );
					KPD_DRV_Inject(c - 0x30, KEY_ACTION_RELEASE );
					break;
				default:
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
					break;

			}
			k++;
			OSTASK_Sleep( pause * (TICKS_ONE_SECOND/10));
		}
		k = 0;
		AT_CmdRspOK( chan );
		return AT_STATUS_DONE;
	}
	else
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
		return AT_STATUS_DONE;
	}
}


/************************************************************************************
*
*	Function Name:	AtHandleSimAccessStatusResp
*
*	Description:	This function processes the received SIM Access status response in
*					the ATC task.
*
*	Notes:
*
*************************************************************************************/
void AtHandleSimAccessStatusResp(MsgType_t msg_type, UInt8 chan, SIMAccess_t sim_access)
{
	UInt16 error_code;

	switch(sim_access)
	{
		case SIMACCESS_SUCCESS:
			switch( AT_GetCmdIdByMpxChan(chan) )
			{
			case AT_CMD_CACM:
				if(msg_type == MSG_SIM_VERIFY_CHV_RSP)
				{
					/* We have just verified PIN2 in AT+CACM=<PIN2> command. Next
					 * we need to reset the Accumulated Call Meter value.
					 */
					SIM_SendWriteAcmReq(AT_GetV24ClientIDFromMpxChan(chan), 0, PostMsgToATCTask);
				}
				else
				{
					AT_CmdRspOK(chan);
				}

				break;

			case AT_CMD_CAMM:
				if(msg_type == MSG_SIM_VERIFY_CHV_RSP)
				{
					/* We have just verified PIN2 in AT+CAMM=<MAX_ACM>,<PIN2> command. Next
					 * we need to set the Maximum Accumulated Call Meter value, which
					 * is stored in _P1, obtained by calling "AT_GetParm(chan, 0)".
					 */
					UInt32 max_acm;

					sscanf( (char *) AT_GetParm(chan, 0), "%x", &max_acm );

					SIM_SendWriteAcmMaxReq(AT_GetV24ClientIDFromMpxChan(chan), max_acm, PostMsgToATCTask);
				}
				else
				{
					AT_CmdRspOK(chan);
				}

				break;

			case AT_CMD_CPUC:
				if(msg_type == MSG_SIM_VERIFY_CHV_RSP)
				{
					/* We have just verified PIN2 in AT+CPUC=<CURRENCY>,<PRICE_PER_UNIT>,<PIN2>
					 * command. Next we need to set Currency and Price Per Unit, which
					 * is stored in _P1 and _P2, obtained by calling "AT_GetParm(chan, 0)" and
					 * "AT_GetParm(chan, 0)".
					 */
					EPPU_t eppu;

					if( Convert_PPUStr((UInt8 *) AT_GetParm(chan, 1), &eppu) )
					{
						SIM_SendWritePuctReq( AT_GetV24ClientIDFromMpxChan(chan), (UInt8 *) AT_GetParm(chan, 0),
									   &eppu, PostMsgToATCTask );
					}
					else
					{
						/* Price Per Unit is not correct, send ERROR back */
						AT_CmdRspError(chan, AT_CME_ERR_INVALID_CHAR_INSTR);
					}
				}
				else
				{
					AT_CmdRspOK(chan);
				}

				break;

			default:
				AT_CmdRspOK(chan);
				break;
			}
			return;

		case SIMACCESS_INCORRECT_CHV:
			error_code = AT_CME_ERR_SIM_PASSWD_WRONG;
			break;

		case SIMACCESS_BLOCKED_CHV:
		case SIMACCESS_INCORRECT_PUK:
			error_code = (SIM_GetPinStatus() == SIM_PUK2_STATUS) ?
					AT_CME_ERR_SIM_PUK2_REQD : AT_CME_ERR_SIM_PUK_REQD;
			break;

		case SIMACCESS_NEED_CHV1:
			error_code = AT_CME_ERR_SIM_PIN_REQD;
			break;

		case SIMACCESS_NEED_CHV2:
			error_code = AT_CME_ERR_SIM_PIN2_REQD;
			break;

		case SIMACCESS_NO_SIM:
			error_code = AT_CME_ERR_PH_SIM_NOT_INSERTED;
			break;

		case SIMACCESS_OUT_OF_RANGE:
			error_code = AT_CME_ERR_INVALID_INDEX;
			break;

		case SIMACCESS_MEMORY_ERR:
		case SIMACCESS_INVALID:
		case SIMACCESS_BLOCKED_PUK:
			error_code = AT_CME_ERR_SIM_FAILURE;
			break;

		case SIMACCESS_NO_ACCESS:
		case SIMACCESS_NOT_FOUND:
		case SIMACCESS_NOT_NEED_CHV1:
		case SIMACCESS_NOT_NEED_CHV2:
		case SIMACCESS_MAX_VALUE:
		default:
			error_code = AT_CME_ERR_OP_NOT_ALLOWED;
			break;
	}

   	AT_CmdRspError(chan, error_code);
}



/************************************************************************************
*
*	Function Name:	AtHandleSimGenericAccessResp
*
*	Description:	This function processes the received Generic SIM Access
*					response in the ATC task (e.g. response for AT+CSIM command)
*
*************************************************************************************/
void AtHandleSimGenericAccessResp(InterTaskMsg_t* inMsg)
{
	SIM_GENERIC_ACCESS_DATA_t *access_data = (SIM_GENERIC_ACCESS_DATA_t *) inMsg->dataBuf;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	UInt16 data_len = (access_data->result == SIMACCESS_SUCCESS) ? access_data->data_len : 0;

	/* Allocate memory: we will change PDU data (2 digits in one byte) to Hex characters.
	 * Also allocate extra 30 bytes for string formatting.
	 */
	char *temp_buffer = OSHEAP_Alloc( (data_len << 1) + 30 );

	sprintf( temp_buffer, "%s: %d,\"", (char *) AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num)),
								data_len << 1 );


	if(data_len != 0)
	{
		UTIL_HexDataToHexStr( ((UInt8 *) temp_buffer + strlen(temp_buffer)), access_data->data,
							access_data->data_len );
	}

	strcat(temp_buffer, "\"");

	AT_OutputStr(mpx_chan_num, (UInt8 *) temp_buffer);
	AT_CmdRspOK(mpx_chan_num);

	OSHEAP_Delete(temp_buffer);
}


/************************************************************************************
*
*	Function Name:	AtHandleSimRestrictedAccessResp
*
*	Description:	This function processes the received Restricted SIM Access
*					response in the ATC task.
*
*	Notes:
*
*************************************************************************************/
void AtHandleSimRestrictedAccessResp(InterTaskMsg_t* inMsg)
{
	SIM_RESTRICTED_ACCESS_DATA_t *access_data = (SIM_RESTRICTED_ACCESS_DATA_t *) inMsg->dataBuf;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	UInt16 data_len = (access_data->result == SIMACCESS_SUCCESS) ? access_data->data_len : 0;

	/* Allocate memory: we will change PDU data (2 digits in one byte) to Hex characters.
	 * Also allocate extra 30 bytes for string formatting.
	 */
	char *temp_buffer = OSHEAP_Alloc( (data_len << 1) + 30 );

	sprintf( temp_buffer, "%s: %d,%d", (char *) AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num)),
								access_data->sw1, access_data->sw2 );

	if(data_len != 0)
	{
		strcat(temp_buffer, ",\"");

		UTIL_HexDataToHexStr( ((UInt8 *) temp_buffer + strlen(temp_buffer)), access_data->data,
							access_data->data_len );

		strcat(temp_buffer, "\"");
	}

	AT_OutputStr( mpx_chan_num, (UInt8*)temp_buffer ) ;
	AT_CmdRspOK(mpx_chan_num);

	OSHEAP_Delete(temp_buffer);
}


/************************************************************************************
*
*	Function Name:	ATCmd_CPIN_Handler
*
*	Description:	This function handles the AT+CPIN command (unlock/unblock PIN).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CPIN_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
								 const UInt8* _P1, const UInt8* _P2 )
{
	SIM_PIN_Status_t curr_PIN_status = SIM_GetPinStatus();
	UInt16 i;
	SIMLockType_t simlock_type;
	SIMLock_Status_t simlock_status;
	char temp_buffer[30];

	if(accessType == AT_ACCESS_TEST)
	{
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}
	else if(accessType == AT_ACCESS_READ)
   	{
		Boolean send_error = TRUE;

		switch(curr_PIN_status)
		{
		case NO_SIM_PIN_STATUS:
			i = AT_CME_ERR_PHONE_FAILURE;
			break;

		case SIM_ABSENT_STATUS:
			curr_PIN_status = SIM_GetCurrLockedSimlockType();

			if (curr_PIN_status != PIN_READY_STATUS)
			{
				/* Send the current SIMLOCK type that is locked instead of SIM Not Inserted status. This is
				 * to allow the host to unlock the SIMLOCK without SIM inserted.
				 */
				send_error = FALSE;
			}
			else
			{
				i = AT_CME_ERR_PH_SIM_NOT_INSERTED;
			}
			break;

		case SIM_ERROR_STATUS:
			i = AT_CME_ERR_SIM_WRONG;
			break;

		case SIM_PUK_BLOCK_STATUS:
			i = AT_CME_ERR_SIM_FAILURE;
			break;

		default:
			send_error = FALSE;
			break;
		}

		if(send_error)
		{
			AT_CmdRspError(chan, i);
		}
		else
		{
			sprintf(temp_buffer,"%s: %s", (char *) AT_GetCmdName(cmdId), getPinCodeStr(curr_PIN_status));
			AT_OutputStr(chan, (UInt8 *) temp_buffer) ;
			AT_CmdRspOK(chan);
		}

		return AT_STATUS_DONE;
   	}
	else /* Must be AT_ACCESS_REGULAR */
	{
		// Get the PIN 1 and check current PIN status
		i = strlen((char *) _P1);

		if( (i > MAX_CONTROL_KEY_LENGTH &&
			 i > (CHV_MAX_LENGTH > PUK_MAX_LENGTH ? CHV_MAX_LENGTH : PUK_MAX_LENGTH)) ||
			i < (CHV_MIN_LENGTH < PUK_MIN_LENGTH ? CHV_MIN_LENGTH : PUK_MIN_LENGTH) )
		{
			AT_CmdRspError(chan, AT_CME_ERR_SIM_PASSWD_WRONG);
			return AT_STATUS_DONE;
		}

		switch(curr_PIN_status)
		{
			case PIN_READY_STATUS:
			case SIM_PIN_STATUS:
			case SIM_PIN2_STATUS:
				SIM_SendVerifyChvReq( AT_GetV24ClientIDFromMpxChan(chan),
						(curr_PIN_status == SIM_PIN_STATUS ? CHV1 : CHV2), (UInt8 *) _P1, PostMsgToATCTask );

				return AT_STATUS_PENDING;

			case SIM_PUK_STATUS:
			case SIM_PUK2_STATUS:
				if( (_P2 == NULL) || (strlen((char *) _P2) == 0) )
				{
					AT_CmdRspError( chan,
						(curr_PIN_status == SIM_PUK_STATUS ? AT_CME_ERR_SIM_PUK_REQD : AT_CME_ERR_SIM_PUK2_REQD) );

					return AT_STATUS_DONE;
				}

				SIM_SendUnblockChvReq( AT_GetV24ClientIDFromMpxChan(chan), (curr_PIN_status == SIM_PUK_STATUS) ? CHV1 : CHV2,
							(UInt8 *) _P1, (UInt8 *) _P2, PostMsgToATCTask );

				return AT_STATUS_PENDING;

			case SIM_ABSENT_STATUS:
				/* SIM is not inserted, but we allow the host to unlock SIMLOCK if any lock is closed */
				switch (SIM_GetCurrLockedSimlockType())
				{
				case PH_NET_PIN_STATUS:
					simlock_type = SIMLOCK_NETWORK_LOCK;
					goto SIMLOCK_UNLOCK;

				case PH_NETSUB_PIN_STATUS:
					simlock_type = SIMLOCK_NET_SUBSET_LOCK;
					goto SIMLOCK_UNLOCK;

				case PH_SP_PIN_STATUS:
					simlock_type = SIMLOCK_PROVIDER_LOCK;
					goto SIMLOCK_UNLOCK;

				case PH_CORP_PIN_STATUS:
					simlock_type = SIMLOCK_CORP_LOCK;
					goto SIMLOCK_UNLOCK;

				case PH_SIM_PIN_STATUS: /* PH-SIM lock: lock to a particular SIM card */
					simlock_type = SIMLOCK_PHONE_LOCK;
					goto SIMLOCK_UNLOCK;

				default:
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}

				break;

			case PH_NET_PIN_STATUS:
				simlock_type = SIMLOCK_NETWORK_LOCK;
				goto SIMLOCK_UNLOCK;
			case PH_NETSUB_PIN_STATUS:
				simlock_type = SIMLOCK_NET_SUBSET_LOCK;
				goto SIMLOCK_UNLOCK;
			case PH_SP_PIN_STATUS:
				simlock_type = SIMLOCK_PROVIDER_LOCK;
				goto SIMLOCK_UNLOCK;
			case PH_CORP_PIN_STATUS:
				simlock_type = SIMLOCK_CORP_LOCK;
				goto SIMLOCK_UNLOCK;
			case PH_SIM_PIN_STATUS: /* PH-SIM lock: lock to a particular SIM card */
				simlock_type = SIMLOCK_PHONE_LOCK;

SIMLOCK_UNLOCK:
				if(simlock_type == SIMLOCK_PHONE_LOCK)
				{
					/* PH-SIM lock is a bit different from the other locks:
					 * after the password is verified to be correct, the PH-SIM lock
					 * is unlocked, but the PH-SIM lock setting is still on and now
					 * it is locked to the new SIM card. So we can not use SIMLockUnlockSIM
					 * function to unlock PH-SIM lock. We need to first obtain the IMSI
					 * of the new SIM card before trying to unlock the PH-SIM lock.
					 */
					Boolean sim_full_lock_on;

					(void) SIMLockIsLockOn(SIMLOCK_PHONE_LOCK, &sim_full_lock_on);

					simlock_status = SIMLockSetLock( ATC_LOCK_MODE, sim_full_lock_on, SIMLOCK_PHONE_LOCK,
											(UInt8 *) _P1,
											(curr_PIN_status == SIM_ABSENT_STATUS) ? NULL : (UInt8 *) SIM_GetIMSI(),
											(curr_PIN_status == SIM_ABSENT_STATUS) ? NULL : (UInt8 *) SIM_GetGID1(),
											(curr_PIN_status == SIM_ABSENT_STATUS) ? NULL : (UInt8 *) SIM_GetGID2() );
				}
				else
				{
					simlock_status = SIMLockUnlockSIM(simlock_type, (UInt8 *) _P1);
				}

				switch(simlock_status)
				{
				case SIMLOCK_SUCCESS:
					/* If all simlock types are opened, we need to attach to network */
					if( SIM_IsPinOK() && (SYS_GetSystemState() == SYSTEM_STATE_ON) )
					{
						MS_SendCombinedAttachReq(atcClientID, TRUE, SIM_GetSIMType(), REG_BOTH, getSelectedPlmn());
					}

					AT_CmdRspOK(chan);
					return AT_STATUS_DONE;

				case SIMLOCK_WRONG_KEY:
				case SIMLOCK_PERMANENTLY_LOCKED:
					AT_CmdRspError(chan, AT_CME_ERR_SIM_PASSWD_WRONG);
					return AT_STATUS_DONE;

				default:
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
				}

				break;

			case PH_NET_PUK_STATUS:
			case PH_NETSUB_PUK_STATUS:
			case PH_SP_PUK_STATUS:
			case PH_CORP_PUK_STATUS:
			default:
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
		}
	}
}


/************************************************************************************
*
*	Function Name:	ATCmd_CLCK_Handler
*
*	Description:	This function handles the AT+CLCK command (set facility on/off).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CLCK_Handler( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
				const UInt8* _P1, const UInt8* _P2, const UInt8* _P3, const UInt8* _P4 )
{
	UInt8 fac_index, pwd_len;
	UInt8 mode;
	ATCFacID_t fac_id;
	Boolean simlock_on;
	Boolean ps_full_lock_on;
	SIMLockType_t simlock_type;
	char temp_buffer[40];
	const char test_rsp[] = "+CLCK: (\"PS\",\"SC\",\"AO\",\"OI\",\"AI\",\"IR\",\"OX\",\"AB\",\"AG\",\"AC\",\"FD\",\"PN\",\"PU\",\"PP\",\"PC\")";

	UInt8				clientID;
	SS_Mode_t			ssMode;
	SS_CallBarType_t	ssCallBarType;
	SS_SvcCls_t			ssSvcCls;
	UInt8*				password = NULL;

	if(accessType == AT_ACCESS_TEST)
	{
		AT_OutputStr( chan, (UInt8*)test_rsp ) ;

		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}
	else if(accessType == AT_ACCESS_READ)
	{
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	} /* Else: Must be AT_ACCESS_REGULAR */

	/* Check if passed facility name is valid */
	for(fac_index = 0; fac_index < (TOTAL_FAC - 1); fac_index++ )	// not include FAC_ID_PIN2
	{
		if ( !strcmp((char *) _P1, (char *) fac_tab[fac_index].name) )
		{
			break;
		}
	}

    if ( fac_index >= (TOTAL_FAC - 1) )
    {
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		return AT_STATUS_DONE;
	}

	fac_id = fac_tab[fac_index].id;
	mode = atoi((char *) _P2);

	/* Check the validity of mode.
	 * When mode == 10: full lock (always ask for pwd after power on,
	 * only applicable for PH-SIM lock ("PS").
	 */
	if( !( (mode == ATC_UNLOCK_MODE) || (mode == ATC_LOCK_MODE) ||
		   (mode == ATC_QUERY_MODE) ||
		   ((mode == ATC_PS_FULL_LOCK) && (fac_id == FAC_ID_PS))
		 ) )
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		return AT_STATUS_DONE;
	}

	pwd_len = (_P3 == NULL) ? 0 : strlen((char *) _P3);

	switch(fac_id)
	{
		// Call barring network related services
		case FAC_ID_AO:
		case FAC_ID_OI:
		case FAC_ID_AI:
		case FAC_ID_IR:
		case FAC_ID_OX:
		case FAC_ID_AB:
		case FAC_ID_AG:
		case FAC_ID_AC:
			clientID	  = AT_GetV24ClientIDFromMpxChan( chan );
			ssMode        = SS_MODE_NOTSPECIFIED ;
			ssCallBarType = SS_CALLBAR_TYPE_NOTSPECIFIED ;
			ssSvcCls      = SS_SVCCLS_NOTSPECIFIED ;

			// For service activation or deactivation, must have password.
			if( (pwd_len == 0) && (mode != ATC_QUERY_MODE) )
			{
				AT_CmdRspError(chan, AT_CME_ERR_SIM_PASSWD_WRONG);
				return AT_STATUS_DONE;
			}
			else if( SYS_GetGSMRegistrationStatus() != REGISTERSTATUS_NORMAL )
			{
				/* We are to perform network operation, but we are not registered */
				AT_CmdRspError(chan, AT_CME_ERR_NO_NETWORK_SVC);
				return AT_STATUS_DONE;
			}

   			switch( mode )
   			{
				case ATC_UNLOCK_MODE:
					ssMode = SS_MODE_DISABLE;
					break;

   			   	case ATC_LOCK_MODE:
   			   		if(fac_id == FAC_ID_AB || fac_id == FAC_ID_AG || fac_id == FAC_ID_AC)
   			   		{
						AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
						return AT_STATUS_DONE;
   			   		}
					ssMode = SS_MODE_ENABLE;
   			   		break;

   			   	case ATC_QUERY_MODE:
   			   		if(fac_id == FAC_ID_AB || fac_id == FAC_ID_AG || fac_id == FAC_ID_AC)
   			   		{
						AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
   			   			return AT_STATUS_DONE;
   			   		}
					ssMode = SS_MODE_INTERROGATE;
   			   		break;

				/* Default case has been checked above in checking mode value */
   			}

			ssCallBarType = fac_tab[fac_index].callBarSvcCode ;

			if (_P3)
			{
				password = (UInt8*)_P3;
			}

			if ( _P4 != NULL )
			{
   				switch( atoi( (char *) _P4 ) )
   				{
   				case   1 :
					ssSvcCls = (SIM_GetAlsDefaultLine() == 0) ? SS_SVCCLS_SPEECH : SS_SVCCLS_ALT_SPEECH;
					break;

				case   2 :
					ssSvcCls = SS_SVCCLS_DATA;
					break;

				case   4 :
					ssSvcCls = SS_SVCCLS_FAX;
					break;

				case   5 :
					ssSvcCls = SS_SVCCLS_ALL_TELE_SERVICES;
					break;

				case   7 :
					ssSvcCls = SS_SVCCLS_NOTSPECIFIED;
					break;

				case   8 :
					ssSvcCls = SS_SVCCLS_SMS;
					break;

				case  10 :
					ssSvcCls = SS_SVCCLS_ALL_SYNC_SERVICES;
					break;

				case  11 :
					ssSvcCls = SS_SVCCLS_ALL_BEARER_SERVICES;
					break;

				case  16 :
					ssSvcCls = SS_SVCCLS_DATA_CIRCUIT_SYNC;
					break;

				case  32 :
					ssSvcCls = SS_SVCCLS_DATA_CIRCUIT_ASYNC;
					break;

				case  64 :
					ssSvcCls = SS_SVCCLS_DEDICATED_PACKET;
					break;

				case  80 :
					ssSvcCls = SS_SVCCLS_ALL_SYNC_SERVICES;
					break;

				case 128 :
					ssSvcCls = SS_SVCCLS_DEDICATED_PAD;
					break;

				case 135 :
					ssSvcCls = SS_SVCCLS_ALL_TELESERVICE_EXCEPT_SMS;
					break;

   				default  :
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
 			   		return AT_STATUS_DONE;
   				}
			}

			switch (SS_SendCallBarringReq(	clientID,
											ssMode,
											ssCallBarType,
											ssSvcCls,
											password))
			{
				case RESULT_OK:
					return AT_STATUS_PENDING;

				case SS_OPERATION_IN_PROGRESS:
					AT_CmdRspError(chan, AT_CME_ERR_AT_BUSY);
					return AT_STATUS_DONE;

				default:
					AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
					return AT_STATUS_DONE;
			}

		// SIM PIN enable/disable
		case FAC_ID_SC:
			switch(mode)
			{
			case ATC_UNLOCK_MODE:
			case ATC_LOCK_MODE:
				if( (pwd_len > CHV_MAX_LENGTH) || (pwd_len < CHV_MIN_LENGTH) )
				{
					AT_CmdRspError(chan, AT_CME_ERR_SIM_PASSWD_WRONG);
   			   		return AT_STATUS_DONE;
				}

				if( (SIM_IsPINRequired() && mode == ATC_LOCK_MODE) ||
					(!SIM_IsPINRequired() && mode == ATC_UNLOCK_MODE) )
				{
					/* Either of the following cases:
					 * 1. We are asked to enable SIM lock, and it is originally locked.
					 * 2. We are asked to disable SIM lock, and it is originally unlocked.
					 * We just return OK.
					 */
					AT_CmdRspOK(chan);
					return AT_STATUS_DONE;
				}

				SIM_SendSetChv1OnOffReq( AT_GetV24ClientIDFromMpxChan(chan), (UInt8 *) _P3,
							   mode == ATC_LOCK_MODE, PostMsgToATCTask );

				return AT_STATUS_PENDING;

			case ATC_QUERY_MODE:
				sprintf(temp_buffer, "%s: %d", (char *) AT_GetCmdName(cmdId),
							(SIM_IsPINRequired() ? ATC_LOCK_MODE : ATC_UNLOCK_MODE) );
				AT_OutputStr( chan, (UInt8*)temp_buffer ) ;

				AT_CmdRspOK(chan);
				return AT_STATUS_DONE;
			}
			break;

		// fixed dialing enable/disable
		case FAC_ID_FD:
			switch(mode)
			{
				case ATC_UNLOCK_MODE:
				case ATC_LOCK_MODE:
					// If current setting is the same as the required setting, just return OK.
					if( (SIM_IsOperationRestricted() && mode == ATC_LOCK_MODE) ||
						(!SIM_IsOperationRestricted() && mode == ATC_UNLOCK_MODE) )
					{
	   				 	AT_CmdRspOK(chan);
						return AT_STATUS_DONE;
					}

					SIM_SendSetOperStateReq( AT_GetV24ClientIDFromMpxChan(chan),
						(mode == ATC_LOCK_MODE) ? SIMOPERSTATE_RESTRICTED_OPERATION : SIMOPERSTATE_UNRESTRICTED_OPERATION,
						(UInt8 *) _P3, PostMsgToATCTask );

					return AT_STATUS_PENDING;

				case ATC_QUERY_MODE:
					// check PIN enable status

					sprintf( temp_buffer, "%s: %d", (char *) AT_GetCmdName(cmdId),
							 SIM_IsOperationRestricted() ? ATC_LOCK_MODE : ATC_UNLOCK_MODE );

					AT_OutputStr( chan, (UInt8*)temp_buffer ) ;

					AT_CmdRspOK(chan);
					return AT_STATUS_DONE;
			}
			break;

		// SIM or network personalization
		case FAC_ID_PS: /* Lock phone to SIM card */
			simlock_type = SIMLOCK_PHONE_LOCK;
			goto SIMLOCK_OPERATION;
		case FAC_ID_PN: /* Network Personalization */
			simlock_type = SIMLOCK_NETWORK_LOCK;
			goto SIMLOCK_OPERATION;
		case FAC_ID_PU: /* Network Subset Personalization */
			simlock_type = SIMLOCK_NET_SUBSET_LOCK;
			goto SIMLOCK_OPERATION;
		case FAC_ID_PP: /* Service Provider Personalization */
			simlock_type = SIMLOCK_PROVIDER_LOCK;
			goto SIMLOCK_OPERATION;
		case FAC_ID_PC: /* Corporate Personalizatin */
			simlock_type = SIMLOCK_CORP_LOCK;

SIMLOCK_OPERATION:
			simlock_on = SIMLockIsLockOn(simlock_type, &ps_full_lock_on);

			/* PH-SIM full lock stutus is only for PH-SIM lock */
			if(fac_id != FAC_ID_PS)
			{
				ps_full_lock_on = FALSE;
			}

			switch(mode)
			{
			/* ATC_PS_FULL_LOCK: always ask user for phone lock password when power up,
			 *							  even though it is the same SIM card as last time.
			 */
			case ATC_UNLOCK_MODE:
			case ATC_LOCK_MODE:
			case ATC_PS_FULL_LOCK:
				if( (simlock_on && (!ps_full_lock_on) && (mode == ATC_LOCK_MODE)) ||
					(simlock_on && ps_full_lock_on && (mode == ATC_PS_FULL_LOCK)) ||
					((!simlock_on) && (mode == ATC_UNLOCK_MODE)) )
				{
					/* The mode we need to set is the same as the current mode.
					 * Just return OK.
					 */
					AT_CmdRspOK(chan);
					return AT_STATUS_DONE;
				}
				else
				{
					switch( SIMLockSetLock( ((mode == ATC_UNLOCK_MODE) ? ATC_UNLOCK_MODE : ATC_LOCK_MODE),
											(mode == ATC_PS_FULL_LOCK), simlock_type,
											(UInt8 *) _P3, (UInt8 *) SIM_GetIMSI(),
											(UInt8 *) SIM_GetGID1(), (UInt8 *) SIM_GetGID2()) )
					{
					case SIMLOCK_SUCCESS:
						AT_CmdRspOK(chan);
						return AT_STATUS_DONE;

					case SIMLOCK_WRONG_KEY:
						AT_CmdRspError(chan, AT_CME_ERR_SIM_PASSWD_WRONG);
						return AT_STATUS_DONE;

					case SIMLOCK_PERMANENTLY_LOCKED:
						AT_CmdRspError(chan, getAtPukStatus(simlock_type));
						return AT_STATUS_DONE;

					default:
						AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
						return AT_STATUS_DONE;
					}
				}
				break;

			case ATC_QUERY_MODE:
				if(simlock_on)
				{
					if(ps_full_lock_on)
					{
						mode = ATC_PS_FULL_LOCK;
					}
					else
					{
						mode = ATC_LOCK_MODE;
					}
				}
				else
				{
					mode = ATC_UNLOCK_MODE;
				}

				sprintf( temp_buffer, "%s: %d", (char *) AT_GetCmdName(cmdId), mode );
				AT_OutputStr( chan, (UInt8*)temp_buffer ) ;
				AT_CmdRspOK(chan);

				return AT_STATUS_DONE;
			}

			break;

		// call barring memory related services
		case FAC_ID_NM:
		case FAC_ID_NS:
		case FAC_ID_NA:
		default:
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_SUPPORTED);
			return AT_STATUS_DONE;
	}

	return AT_STATUS_DONE;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CPWD_Handler
*
*	Description:	This function handles the AT+CPWD command (change facility password).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CPWD_Handler ( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
				const UInt8* _P1, const UInt8* _P2, const UInt8* _P3, const UInt8* _P4 )
{
	UInt8 fac_index;
	ATCFacID_t fac_id;
 	UInt8 old_pwd_len, new_pwd_len, rep_new_pwd_len;
	const char test_rsp[] = "+CPWD: (\"P2\",8),(\"FD\",8),(\"PS\",8),(\"SC\",8),(\"AO\",4),(\"OI\",4),(\"AI\",4),(\"IR\",4),(\"OX\",4),(\"AB\",4),(\"AG\",4),(\"AC\",4)";

	if(accessType == AT_ACCESS_TEST)
	{
		AT_OutputStr( chan, (UInt8*)test_rsp ) ;
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}
	else if(accessType == AT_ACCESS_READ)
	{
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}

	if(_P1 == NULL || _P2 == NULL || _P3 == NULL)
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		return AT_STATUS_DONE;
	}

    for ( fac_index = 0; fac_index < TOTAL_FAC; fac_index++ )
	{
		if ( !strcmp( (char *) _P1, (char *) fac_tab[fac_index].name ) )
		{
			break;
		}
    }

    if ( fac_index >= TOTAL_FAC )
    {
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		return AT_STATUS_DONE;
	}

	fac_id = fac_tab[fac_index].id;

   	old_pwd_len = strlen((char *) _P2);
   	new_pwd_len = strlen((char *) _P3);

   	if( (old_pwd_len < CHV_MIN_LENGTH) || (old_pwd_len > CHV_MAX_LENGTH) ||
  		(new_pwd_len < CHV_MIN_LENGTH) || (new_pwd_len > CHV_MAX_LENGTH) )
   	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
   		return AT_STATUS_DONE;
   	}

	switch(fac_id)
	{
		// call barring network related services
		case FAC_ID_AO:
		case FAC_ID_OI:
		case FAC_ID_AI:
		case FAC_ID_IR:
		case FAC_ID_OX:
		case FAC_ID_AB:
		case FAC_ID_AG:
		case FAC_ID_AC:
			if( SYS_GetGSMRegistrationStatus() != REGISTERSTATUS_NORMAL )
			{
				/* We are to perform network operation, but we are not registered */
				AT_CmdRspError(chan, AT_CME_ERR_NO_NETWORK_SVC);
				return AT_STATUS_DONE;
			}

			if (_P4)
			{
				rep_new_pwd_len = strlen((char *)_P4);
			}

			if ((old_pwd_len != CHV_MIN_LENGTH) ||
				(new_pwd_len != CHV_MIN_LENGTH) ||
				((_P4) && (rep_new_pwd_len != CHV_MIN_LENGTH)))
			{
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}

			SS_SendCallBarringPWDChangeReq(	AT_GetV24ClientIDFromMpxChan(chan),
											fac_tab[fac_index].callBarSvcCode,
											(UInt8*)_P2,
											(UInt8*)_P3,
											_P4 ? (UInt8*)_P4 : (UInt8*)_P3);
			return AT_STATUS_PENDING ;

		case FAC_ID_SC:
			if( !SIM_IsPINRequired() )
			{
				/* Can not change PIN1 password when PIN1 is not enabled (see GSM 11.11) */
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				return AT_STATUS_DONE;
			}
			/* Fall through !!!!!!!!!!!!! */

		case FAC_ID_FD:
		case FAC_ID_PIN2:
			atcsim_SendPinChangeReq(chan, (fac_id == FAC_ID_SC), (char *) _P2, (char *) _P3);
			return AT_STATUS_PENDING;

		/* Password for PH-SIM lock (lock to a particular SIM card) */
		case FAC_ID_PS:
			switch( SIMLockChangePasswordPHSIM((UInt8 *) _P2, (UInt8 *) _P3) )
			{
			case SIMLOCK_SUCCESS:
				AT_CmdRspOK(chan);
				break;

			case SIMLOCK_WRONG_KEY:
				AT_CmdRspError(chan, AT_CME_ERR_SIM_PASSWD_WRONG);
				break;

			case SIMLOCK_FAILURE:
			default:
				AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
				break;
			}

			return AT_STATUS_DONE;

			break;

		/* Change password for Network, Network Subset, Service Provider or
		 * Corporate locks or memory related locks: it is not allowed!!!
		 */
		case FAC_ID_PN:
		case FAC_ID_PU:
		case FAC_ID_PP:
		case FAC_ID_PC:
		case FAC_ID_NM:
		case FAC_ID_NS:
		case FAC_ID_NA:
		default:
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			return AT_STATUS_DONE;
	}
}


/************************************************************************************
*
*	Function Name:	ATCmd_CMAR_Handler
*
*	Description:	This function handles the AT+CMAR command (master reset).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CMAR_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	if( (accessType == AT_ACCESS_TEST) || (accessType == AT_ACCESS_READ) )
	{
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}

	if( SIMLockCheckPasswordPHSIM((UInt8 *) _P1) )
	{
		/* Phone lock password verification is ok, go ahead to reset the user data
		 * in NVRAM. If phone lock is on, since we are resetting NVRAM, it
		 * will be set to off and password set back to default value of "0000".
		 * Resetting the phone lock data is a requirement of Master Reset as
		 * defined in GSM 07.07.
		 */
		NVRAM_Change2Default();

		/* Wait one second before returning OK to minimize the chance that
		 * the user powers us down when the NVRAM sync is going on.
		 */
		OSTASK_Sleep(TICKS_ONE_SECOND);

		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}
	else
	{
		/* Wrong phone lock password received */
		AT_CmdRspError(chan, AT_CME_ERR_SIM_PASSWD_WRONG);
		return AT_STATUS_DONE;
	}
}


/************************************************************************************
*
*	Function Name:	ATCmd_CSIM_Handler
*
*	Description:	This function handles the AT+CSIM command (Generic SIM access).
*
*************************************************************************************/
AT_Status_t ATCmd_CSIM_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1, const UInt8* _P2)
{
	UInt16 length = atoi((char *) _P1);
	UInt16 str_len = (_P2 == NULL) ? 0 : strlen((char *) _P2);
	AT_Status_t status = AT_STATUS_DONE;
	SIM_PIN_Status_t pin_status = SIM_GetPinStatus();

	if( (accessType == AT_ACCESS_TEST) || (accessType == AT_ACCESS_READ) )
	{
		AT_CmdRspOK(chan);
	}
	else if( (length != str_len) || ((length & 0x0001) != 0) || (str_len == 0) )
	{
		AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
	}
	else if(pin_status == SIM_ABSENT_STATUS)
	{
		AT_CmdRspError(chan, AT_CME_ERR_PH_SIM_NOT_INSERTED);
	}
	else if(pin_status == NO_SIM_PIN_STATUS)
	{
		AT_CmdRspError(chan, AT_CME_ERR_SIM_BUSY);
	}
	else
	{
		/* Allocate one more byte for NULL character in order for
		 * UTIL_HexStrToHexData to terminate the string.
		 */
		UInt8 *temp_buffer = OSHEAP_Alloc((length >> 1) + 1);

		if( UTIL_HexStrToHexData(temp_buffer, (UInt8 *) _P2, length) > 0 )
		{
			/* Error when converting Hex string to raw Hex number */
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
		}
		else
		{
			SIM_SendGenericAccessReq( AT_GetV24ClientIDFromMpxChan(chan),
						(length >> 1), temp_buffer, PostMsgToATCTask );

			status = AT_STATUS_PENDING;
		}

		OSHEAP_Delete(temp_buffer);
	}

	return status;
}


/************************************************************************************
*
*	Function Name:	ATCmd_MECSIM_Handler
*
*	Description:	This function handles the AT*MECSIM command (end CSIM command session).
*
*************************************************************************************/
AT_Status_t ATCmd_MECSIM_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	SIM_SendGenericAccessEndReq();
	AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CRSM_Handler
*
*	Description:	This function handles the AT+CRSM command (restricted SIM access).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CRSM_Handler( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1,
	const UInt8* _P2, const UInt8* _P3, const UInt8* _P4, const UInt8* _P5, const UInt8* _P6, const UInt8* _P7 )
{
	UInt8 command;
	UInt8 p1, p2, p3;				// P1, P2, P3 for SIM
	UInt8 path_len;
	UInt8 i;
	UInt16 data_len;
	UInt16 efile_id;
	UInt16 dfile_id;
	const UInt16* select_path_ptr;
	UInt8 *temp_buffer = NULL;
	UInt16 select_path[MAX_SIM_FILE_PATH_LEN];

	if( (accessType == AT_ACCESS_TEST) || (accessType == AT_ACCESS_READ) )
	{
		AT_CmdRspOK(chan);
		return AT_STATUS_DONE;
	}

	// Check the existance of the passed parameters.
	if( (_P1 == NULL) || (_P1[0] == '\0') || (_P2 == NULL) || (_P2[0] == '\0') ||
		(_P3 == NULL) || (_P3[0] == '\0') || (_P4 == NULL) || (_P4[0] == '\0') ||
		(_P5 == NULL) || (_P5[0] == '\0') )
	{
		GOTO_ERROR;
	}
	else
	{
		command = atoi((char *) _P1);
		efile_id = atoi((char *) _P2);
		p1 = atoi((char *) _P3);
		p2 = atoi((char *) _P4);
		p3 = atoi((char *) _P5);

		/* Perform further checking according to Section 9.2 of GSM 11.11 */
		switch(command)
		{
		case APDUCMD_READ_BINARY:
			if( (p1 != 0) || (p3 == 0) )
			{
				GOTO_ERROR;
			}
			break;

		case APDUCMD_UPDATE_BINARY:
			if( (p1 != 0) || (p3 == 0) || (_P6 == NULL) || (_P6[0] == '\0') )
			{
				GOTO_ERROR;
			}
			break;

		case APDUCMD_READ_RECORD:
			if( ((p2 != EFILE_RECORD_NEXT_MODE) && (p2 != EFILE_RECORD_PREVIOUS_MODE) &&
				 (p2 != EFILE_RECORD_ABSOLUTE_MODE)) || (p3 == 0) )
			{
				GOTO_ERROR;
			}
			break;

		case APDUCMD_UPDATE_RECORD:
			if( ((p2 != EFILE_RECORD_NEXT_MODE) && (p2 != EFILE_RECORD_PREVIOUS_MODE) &&
				 (p2 != EFILE_RECORD_ABSOLUTE_MODE)) ||
				(p3 == 0) || (_P6 == NULL) || (_P6[0] == '\0') )
			{
				GOTO_ERROR;
			}
			break;

		case APDUCMD_STATUS:
			if( (p1 != 0) || (p2 != 0) || (p3 != 2) )
			{
				GOTO_ERROR;
			}
			break;

		default:
			GOTO_ERROR;
			break;
		}
	}

	if( (_P6 != NULL) && (_P6[0] != '\0') )
	{
		data_len = strlen((char *) _P6);

		if( (data_len & 0x0001) != 0 )
		{
			/* Hex strings must contain even number of bytes */
			GOTO_ERROR;
		}
		else
		{
			temp_buffer = OSHEAP_Alloc((data_len >> 1) + 1);

			if( UTIL_HexStrToHexData(temp_buffer, (UInt8 *) _P6, data_len) != 0 )
			{
				/* Error when converting Hex string to raw Hex number */
				GOTO_ERROR;
			}
		}
	}

	if (_P7 == NULL)
	{
		/* Old GSM 07.07 format: AT+CRSM does not have <pathid> paramter */
		dfile_id = simmi_GetMasterFileId(efile_id);

		if (dfile_id == APDUFILEID_MF)
		{
			path_len = 0;
			select_path_ptr = NULL;
		}
		else
		{
			path_len = SIM_GetMfPathLen();
			select_path_ptr = SIM_GetMfPath();
		}
	}
	else if (_P7[0] == '\0')
	{
		/* New 3GPP 27.007 format: AT+CRSM has <pathid> as NULL string. This means the EF's parent is MF */
		dfile_id = APDUFILEID_MF;
		path_len = 0;
		select_path_ptr = NULL;
	}
	else
	{
		/* New 3GPP 27.007 format: AT+CRSM has <pathid> as the parent DF's select path (excluding MF) */
		UInt8 file_id_buf[(MAX_SIM_FILE_PATH_LEN * 2) + 1];

		data_len = strlen((char *) _P7);

		if ( ((data_len & 0x0003) != 0) || (data_len > (MAX_SIM_FILE_PATH_LEN * 4)) )
		{
			/* The number of Hex characters must be multiples of 4 (4 characters for one two-byte file ID) */
			GOTO_ERROR;
		}

		if ( UTIL_HexStrToHexData(file_id_buf, (UInt8 *) _P7, data_len) != 0 )
		{
			GOTO_ERROR;
		}

		/* Add MF to the select path */
		select_path[0] = APDUFILEID_MF;

		for (i = 0; (i < MAX_SIM_FILE_PATH_LEN) && (file_id_buf[i << 1] != '\0') ; i++)
		{
			select_path[i + 1] = (file_id_buf[i << 1] << 8) | file_id_buf[(i << 1) + 1];
		}

		dfile_id = select_path[i];
		path_len = i;
		select_path_ptr = select_path;
	}

	/* If we get here, we pass all checking, send command to SIM
	 * using generic command function.
	 */
	SIM_SendRestrictedAccessReq(
		AT_GetV24ClientIDFromMpxChan(chan),	// V24 Client ID
		command,							// SIM command
		efile_id,						// SIM EF file ID
		dfile_id,						// Parent DF file ID
		p1,								// instruction param 1
	 	p2,								// instruction param 2
	 	p3,								// instruction param 3,
		path_len,						// Number of file ID's in select path
		select_path_ptr,				// Select path file ID array
	 	temp_buffer,					// data
		PostMsgToATCTask				// Callback to return result of SIM Restricted Access
		);

	if(temp_buffer != NULL)
	{
		OSHEAP_Delete(temp_buffer);
	}

	return AT_STATUS_PENDING;

ERROR_LABEL:
	if(temp_buffer != NULL)
	{
		OSHEAP_Delete(temp_buffer);
	}

	AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
	return AT_STATUS_DONE;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CACM_Handler
*
*	Description:	This function handles the AT+CACM command (Accumulated Call Meter).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CACM_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType, const UInt8* _P1)
{
	AT_Status_t status;

	if(accessType == AT_ACCESS_REGULAR)
	{
		SIM_SendVerifyChvReq(AT_GetV24ClientIDFromMpxChan(chan), CHV2, (UInt8 *) _P1, PostMsgToATCTask);
		status = AT_STATUS_PENDING;
	}
	else if(accessType == AT_ACCESS_READ)
	{
		SIM_SendReadAcmReq(AT_GetV24ClientIDFromMpxChan(chan), PostMsgToATCTask);
		status = AT_STATUS_PENDING;
	}
	else
	{
		AT_CmdRspOK(chan);
		status = AT_STATUS_DONE;
	}

	return status;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CAMM_Handler
*
*	Description:	This function handles the AT+CAMM command (Maximum Accumulated Call Meter).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CAMM_Handler( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
							    const UInt8* _P1, const UInt8* _P2 )
{
	AT_Status_t status;

	if(accessType == AT_ACCESS_REGULAR)
	{
	    if( (_P1 == NULL) || (_P1[0] == '\0') )
		{
			/* _P1 stores MAX ACM value, must be present */
			AT_CmdRspError(chan, AT_CME_ERR_INVALID_CHAR_INSTR);
			status = AT_STATUS_DONE;
		}
		else
		{
			/* First verify PIN2: we will access _P1 again after we verify PIN2 is correct */
			SIM_SendVerifyChvReq(AT_GetV24ClientIDFromMpxChan(chan), CHV2, (UInt8 *) _P2, PostMsgToATCTask);
			status = AT_STATUS_PENDING;
		}
	}
	else if(accessType == AT_ACCESS_READ)
	{
		SIM_SendReadAcmMaxReq( AT_GetV24ClientIDFromMpxChan(chan), PostMsgToATCTask );
		status = AT_STATUS_PENDING;
	}
	else
	{
		AT_CmdRspOK(chan);
		status = AT_STATUS_DONE;
	}

	return status;
}


/************************************************************************************
*
*	Function Name:	ATCmd_CPUC_Handler
*
*	Description:	This function handles the AT+CPUC command (Price Per Unit).
*
*
*************************************************************************************/
AT_Status_t ATCmd_CPUC_Handler( AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType,
							    const UInt8* _P1, const UInt8* _P2, const UInt8* _P3 )
{
	AT_Status_t status;

	if(accessType == AT_ACCESS_REGULAR)
	{
		if( (_P1 == NULL) || (_P2 == NULL) || (_P2[0] == '\0') )
		{
			/* Currency and PPU must exist */
			AT_CmdRspError(chan, AT_CME_ERR_OP_NOT_ALLOWED);
			status = AT_STATUS_DONE;
		}
		else if(strlen((char *) _P1) >= sizeof(CurrencyName_t))
		{
			/* Currency string too long */
			AT_CmdRspError(chan, AT_CME_ERR_TXTSTR_TOO_LONG);
			status = AT_STATUS_DONE;
		}
		else
		{
			SIM_SendVerifyChvReq(AT_GetV24ClientIDFromMpxChan(chan), CHV2, (UInt8 *) _P3, PostMsgToATCTask);
			status = AT_STATUS_PENDING;
		}
	}
	else if(accessType == AT_ACCESS_READ )
	{
		SIM_SendReadPuctReq(AT_GetV24ClientIDFromMpxChan(chan), PostMsgToATCTask);
		status = AT_STATUS_PENDING;
	}
	else
	{
		AT_CmdRspOK(chan);
		status = AT_STATUS_DONE;
	}

	return status;
}


/************************************************************************************
*
*	Function Name:	AtHandleAcmValueResp
*
*	Description:	This function processes the received MSG_SIM_ACM_VALUE_RSP
*					response in the ATC task.
*
*	Notes:
*
*************************************************************************************/
void AtHandleAcmValueResp(InterTaskMsg_t* inMsg)
{
	SIM_ACM_VALUE_t *acm_ptr = (SIM_ACM_VALUE_t *) inMsg->dataBuf;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	char temp_buffer[20];

	if(acm_ptr->result == SIMACCESS_SUCCESS)
	{
		sprintf( temp_buffer, "%s: \"%06x\"",
			(char *) AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num)),
									acm_ptr->acm);

		AT_OutputStr( mpx_chan_num, (UInt8*)temp_buffer ) ;
		AT_CmdRspOK(mpx_chan_num);
	}
	else
	{
		AtHandleSimAccessStatusResp(inMsg->msgType, mpx_chan_num, acm_ptr->result);
	}
}


/************************************************************************************
*
*	Function Name:	AtHandleMaxAcmValueResp
*
*	Description:	This function processes the received MSG_SIM_MAX_ACM_RSP
*					response in the ATC task.
*
*	Notes:
*
*************************************************************************************/
void AtHandleMaxAcmValueResp(InterTaskMsg_t* inMsg)
{
	SIM_MAX_ACM_t *acm_ptr = (SIM_MAX_ACM_t *) inMsg->dataBuf;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	char temp_buffer[20];

	if(acm_ptr->result == SIMACCESS_SUCCESS)
	{
		sprintf( temp_buffer, "%s: \"%02x%02x%02x\"",
			(char *) AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num)),
									(UInt8)acm_ptr->max_acm >> 16,
									(UInt8)acm_ptr->max_acm >> 8,
									(UInt8)acm_ptr->max_acm);

		AT_OutputStr( mpx_chan_num, (UInt8*)temp_buffer ) ;
		AT_CmdRspOK(mpx_chan_num);
	}
	else
	{
		AtHandleSimAccessStatusResp(inMsg->msgType, mpx_chan_num, acm_ptr->result);
	}
}


/************************************************************************************
*
*	Function Name:	AtHandlePuctDataResp
*
*	Description:	This function processes the received MSG_SIM_PUCT_DATA_RSP
*					response in the ATC task.
*
*	Notes:
*
*************************************************************************************/
void AtHandlePuctDataResp(InterTaskMsg_t* inMsg)
{
	UInt8 i;
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	char temp_buffer[70];
	UInt16 temp;
	char *temp_ptr;
	char *cmd_ptr = (char *) AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num));
	SIM_PUCT_DATA_t *puct_ptr = (SIM_PUCT_DATA_t *) inMsg->dataBuf;

	if (puct_ptr->result == SIMACCESS_SUCCESS)
	{
		if(puct_ptr->eppu.exp == 0)
		{
			sprintf(temp_buffer, "%s: \"%s\",\"%d\"", cmd_ptr, puct_ptr->currency, puct_ptr->eppu.mant);
		}
		else if(puct_ptr->eppu.exp < 0)
		{
			/* Exponent is negative, we need to insert dot into the value.
			 * Use the latter part of buffer for processing.
			 */
			temp_ptr = temp_buffer + 30;

			sprintf(temp_ptr, "%d", puct_ptr->eppu.mant);
			temp = strlen( temp_ptr );

			if( temp <= (0 - puct_ptr->eppu.exp) )
			{
				/* Decimal string less than 1 */
				strcpy( temp_ptr, "0." );

				for(i = 0; i < ((0 - puct_ptr->eppu.exp) - temp); i++)
				{
					strcat(temp_ptr, "0");
				}

				sprintf(temp_ptr + strlen(temp_ptr), "%d", puct_ptr->eppu.mant);
			}
			else
			{
				/* Copy the digits that need to be put after dot to a temporary place */
				strcpy(temp_buffer, temp_ptr + (temp - (0 - puct_ptr->eppu.exp)));

				strcpy(temp_ptr + (temp - (0 - puct_ptr->eppu.exp)), ".");

				strcat(temp_ptr, temp_buffer);
			}

			sprintf(temp_buffer, "%s: \"%s\",\"%s\"", cmd_ptr, puct_ptr->currency, temp_ptr);
		}
		else /* Case for "eppu.exp > 0" */
		{
			/* Point to the latter part of buffer for processing */
			temp_ptr = temp_buffer + 30;

			sprintf(temp_ptr, "%d", puct_ptr->eppu.mant);

			for(i = 0; i < puct_ptr->eppu.exp; i++)
			{
				strcat(temp_ptr, "0");
			}

			sprintf(temp_buffer, "%s: \"%s\",\"%s\"", cmd_ptr, puct_ptr->currency, temp_ptr);
		}

		AT_OutputStr( mpx_chan_num, (UInt8*)temp_buffer ) ;
		AT_CmdRspOK(mpx_chan_num);
	}
	else
	{
		AtHandleSimAccessStatusResp(inMsg->msgType, mpx_chan_num, puct_ptr->result);
	}
}


/************************************************************************************
*
*	Function Name:	ATCmd_EPRO_Handler
*
*	Description:	This function handles the AT*EPRO command
*					(read Service Provider Name from SIM).
*
*	Notes:
*
*************************************************************************************/
AT_Status_t ATCmd_EPRO_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
	if(accessType == AT_ACCESS_TEST)
	{
		AT_CmdRspOK(chan) ;
		return AT_STATUS_DONE ;
	}
	else
	{
		SIM_SendReadSvcProvNameReq(AT_GetV24ClientIDFromMpxChan(chan), PostMsgToATCTask);
		return AT_STATUS_PENDING;
	}
}


/************************************************************************************
*
*	Function Name:	AtHandleEPROResp
*
*	Description:	This function processes the received MSG_SIM_SVC_PROV_NAME_RSP
*					response in the ATC task.
*
*	Notes:
*
*************************************************************************************/
void AtHandleEPROResp(InterTaskMsg_t *inMsg)
{
	UInt8 mpx_chan_num = AT_GetMpxChanFromV24ClientID(inMsg->clientID);
	char temp_buffer[50];

	SIM_SVC_PROV_NAME_t *svc_prov = (SIM_SVC_PROV_NAME_t *) inMsg->dataBuf;

	sprintf( temp_buffer, "%s: \"%s\",%d",
		(char *) AT_GetCmdName(AT_GetCmdIdByMpxChan(mpx_chan_num)),
		((svc_prov->result == SIMACCESS_SUCCESS) ? (char *) &svc_prov->name : "\0"), svc_prov->display_flag );

	AT_OutputStr( mpx_chan_num, (UInt8*)temp_buffer ) ;

	AT_CmdRspOK(mpx_chan_num);
}


AT_Status_t ATCmd_MQSSIG_Handler(AT_CmdId_t cmdId, UInt8 chan, UInt8 accessType)
{
        UInt8			temp_buf[MAX_SIMLOCK_SIGNATURE_LEN+20];
        UInt8           signStr[MAX_SIMLOCK_SIGNATURE_LEN+1];

#ifdef SEMC_BOOTLOADER

		AT_CmdRspSyntaxError(chan) ;

#else

        SIMLockGetSignature(signStr, sizeof(signStr));

        // build up the response string
        sprintf((char *)temp_buf,"*MQSSIG: \"%s\"", (char *)signStr);

        AT_OutputStr( chan, (UInt8*)temp_buf ) ;
        AT_CmdRspOK(chan);

#endif

		return AT_STATUS_DONE ;
}


