//=============================================================================
// File:			at_string.c
//
// Description:		AT string concentration module.
//
// Note:
//
//==============================================================================
#include "at_api.h"

//-------------------------------------------------
// Import Functions and Variables
//-------------------------------------------------


//----------------------------------------------------------------------
// GetATString(): return AT constant string.
//-----------------------------------------------------------------------
const UInt8* GetATString(AT_ConstString_t stringName)
{
	const UInt8*	str = 0;

	//-------------------------------------------------------------------
	//	If you add a string to this table please ensure that it appears
	//	in the same order as the enumerations in at_string.h.  This is
	//	not mandatory for code operation but makes keeping track
	//	of the strings easier.
	//-------------------------------------------------------------------
	switch(stringName){

	case AT_OK_STR:
		str = (const UInt8*)"OK";
		break;

	case AT_ERROR_STR:
		str = (const UInt8*)"ERROR";
		break;

	case AT_CONNECT_STR:
		str = (const UInt8*)"CONNECT";
		break;

	case AT_RING_STR:
		str = (const UInt8*)"RING";
		break;

	case AT_NO_CARRIER_STR:
		str = (const UInt8*)"NO CARRIER";
		break;

	case AT_EMPTY_STR:
		str = (const UInt8*)"";
		break;

	case AT_NO_DIALTONE_STR:
		str = (const UInt8*)"NO DIALTONE";
		break;

	case AT_BUSY_STR:
		str = (const UInt8*)"BUSY";
		break;

	case AT_NO_ANSWER_STR:
		str = (const UInt8*)"NO ANSWER";
		break;

	case AT_CONNECT_300_STR:
		str = (const UInt8*)"CONNECT 300";
		break;

	case AT_CONNECT_1200_STR:
		str = (const UInt8*)"CONNECT 1200";
		break;

	case AT_CONNECT_1200_75_STR:
		str = (const UInt8*)"CONNECT 1200/75";
		break;

	case AT_CONNECT_2400_STR:
		str = (const UInt8*)"CONNECT 2400";
		break;

	case AT_CONNECT_4800_STR:
		str = (const UInt8*)"CONNECT 4800";
		break;

	case AT_CONNECT_9600_STR:
		str = (const UInt8*)"CONNECT 9600";
		break;

	case AT_CONNECT_14400_STR:
		str = (const UInt8*)"CONNECT 14400";
		break;

	case AT_CONNECT_19200_STR:
		str = (const UInt8*)"CONNECT 19200";
		break;

	case AT_CONNECT_28800_STR:
		str = (const UInt8*)"CONNECT 28800";
		break ;

	case AT_CONNECT_38400_STR:
		str = (const UInt8*)"CONNECT 38400";
		break;

	case AT_CONNECT_43200_STR:
		str = (const UInt8*)"CONNECT 43200";
		break;

	case AT_CONNECT_56000_STR:
		str = (const UInt8*)"CONNECT 56000";
		break;

	case AT_CONNECT_57600_STR:
		str = (const UInt8*)"CONNECT 57600";
		break;

	case AT_CME_ERROR_STR:
		str = (const UInt8*)"+CME ERROR: ";
		break;

	case AT_CMS_ERROR_STR:
		str = (const UInt8*)"+CMS ERROR: ";
		break;

	case AT_CPIN_READY_STR:
		str = (const UInt8*)"READY";
		break;

	case AT_CPIN_SIM_PIN_STR:
		str = (const UInt8*)"SIM PIN";
		break;

	case AT_CPIN_SIM_PUK_STR:
		str = (const UInt8*)"SIM PUK";
		break;

	case AT_CPIN_SIM_PIN2_STR:
		str = (const UInt8*)"SIM PIN2";
		break;

	case AT_CPIN_SIM_PUK2_STR:
		str = (const UInt8*)"SIM PUK2";
		break;

	case AT_CPIN_PH_SIM_PIN_STR:
		str = (const UInt8*)"PH-SIM PIN";
		break;

	case AT_CPIN_PH_NET_PIN_STR:
		str = (const UInt8*)"PH-NET PIN";
		break;

	case AT_CPIN_PH_NETSUB_PIN_STR:
		str = (const UInt8*)"PH-NETSUB PIN";
		break;

	case AT_CRING_VOICE_STR:
		str = (const UInt8*)"+CRING: VOICE";
		break;

	case AT_CRING_FAX_STR:
		str = (const UInt8*)"+CRING: FAX";
		break;

	case AT_CRING_ASYNC_STR:
		str = (const UInt8*)"+CRING: ASYNC";
		break;

	case AT_CRING_REL_ASYNC_STR:
		str = (const UInt8*)"+CRING: REL ASYNC";
		break;

	case AT_CRING_SYNC_STR:
		str = (const UInt8*)"+CRING: SYNC";
		break;

	case AT_CRING_REL_SYNC_STR:
		str = (const UInt8*)"+CRING: REL SYNC";
		break;

	case AT_CR_ASYNC_STR:
		str = (const UInt8*)"+CR: ASYNC";
		break;

	case AT_CR_REL_ASYNC_STR:
		str = (const UInt8*)"+CR: REL ASYNC";
		break;

	case AT_CR_SYNC_STR:
		str = (const UInt8*)"+CR: SYNC";
		break;

	case AT_CR_REL_SYNC_STR:
		str = (const UInt8*)"+CR: REL SYNC";
		break;

	case AT_CME_ERR_PHONE_FAILURE_STR:
		str = (const UInt8*)"phone failure";
		break;

	case AT_CME_ERR_NO_CONNECTION_STR:
		str = (const UInt8*)"No connection to phone";
		break;

	case AT_CME_ERR_PH_ADAPTOR_RESERVED_STR:
		str = (const UInt8*)"Phone-adaptor link reserved";
		break;

	case AT_CME_ERR_OP_NOT_ALLOWED_STR:
		str = (const UInt8*)"Operation not allowed";
		break;

	case AT_CME_ERR_OP_NOT_SUPPORTED_STR:
		str = (const UInt8*)"Operation not supported";
		break;

	case AT_CME_ERR_PH_SIM_PIN_REQD_STR:
		str = (const UInt8*)"PH-SIM PIN required";
		break;

	case AT_CME_ERR_PH_FSIM_PIN_REQD_STR:
		str = (const UInt8*)"PH-FSIM PIN required";
		break;

	case AT_CME_ERR_PH_FSIM_PUK_REQD_STR:
		str = (const UInt8*)"PH-FSIM PUK required";
		break;

	case AT_CME_ERR_PH_SIM_NOT_INSERTED_STR:
		str = (const UInt8*)"SIM not inserted";
		break;

	case AT_CME_ERR_SIM_PIN_REQD_STR:
		str = (const UInt8*)"SIM PIN required";
		break;

	case AT_CME_ERR_SIM_PUK_REQD_STR:
		str = (const UInt8*)"SIM PUK required";
		break;

	case AT_CME_ERR_SIM_FAILURE_STR:
		str = (const UInt8*)"SIM failure";
		break;

	case AT_CME_ERR_SIM_BUSY_STR:
		str = (const UInt8*)"SIM busy";
		break;

	case AT_CME_ERR_SIM_WRONG_STR:
		str = (const UInt8*)"SIM wrong";
		break;

	case AT_CME_ERR_SIM_PASSWD_WRONG_STR:
		str = (const UInt8*)"Incorrect password";
		break;

	case AT_CME_ERR_SIM_PIN2_REQD_STR:
		str = (const UInt8*)"SIM PIN2 required";
		break;

	case AT_CME_ERR_SIM_PUK2_REQD_STR:
		str = (const UInt8*)"SIM PUK2 required";
		break;

	case AT_CME_ERR_MEMORY_FULL_STR:
		str = (const UInt8*)"Memory full";
		break;

	case AT_CME_ERR_INVALID_INDEX_STR:
		str = (const UInt8*)"Invalid index";
		break;

	case AT_CME_ERR_NOT_FOUND_STR:
		str = (const UInt8*)"Not found";
		break;

	case AT_CME_ERR_MEMORY_FAILURE_STR:
		str = (const UInt8*)"Memory failure";
		break;

	case AT_CME_ERR_TXTSTR_TOO_LONG_STR:
		str = (const UInt8*)"Text string too long";
		break;

	case AT_CME_ERR_INVALID_CHAR_INSTR_STR:
		str = (const UInt8*)"Invalid characters in text string";
		break;

	case AT_CME_ERR_DIAL_STR_TOO_LONG_STR:
		str = (const UInt8*)"Dial string too long";
		break;

	case AT_CME_ERR_INVALID_DIALSTR_CHAR_STR:
		str = (const UInt8*)"Invalid characters in dial string";
		break;

	case AT_CME_ERR_NO_NETWORK_SVC_STR:
		str = (const UInt8*)"No network service";
		break;

	case AT_CME_ERR_NETWORK_TIMEOUT_STR:
		str = (const UInt8*)"Network timeout";
		break;

	case AT_CME_ERR_NETWORK_NOT_ALLOWED_STR:
		str = (const UInt8*)"Network not allowed - emergency calls only";
		break;

	case AT_CME_ERR_NETWORK_LOCK_PIN_REQD_STR:
		str = (const UInt8*)"Network personalisation PIN required";
		break;

	case AT_CME_ERR_NETWORK_LOCK_PUK_REQD_STR:
		str = (const UInt8*)"Network personalisation PUK required";
		break;

	case AT_CME_ERR_SUBNET_LOCK_PIN_REQD_STR:
		str = (const UInt8*)"Network subset personalisation PIN required";
		break;

	case AT_CME_ERR_SUBNET_LOCK_PUK_REQD_STR:
		str = (const UInt8*)"Network subset personalisation PUK required";
		break;

	case AT_CME_ERR_SP_LOCK_PIN_REQD_STR:
		str = (const UInt8*)"Service provider personalisation PIN required";
		break;

	case AT_CME_ERR_SP_LOCK_PUK_REQD_STR:
		str = (const UInt8*)"Service provider personalisation PUK required";
		break;

	case AT_CME_ERR_CORP_LOCK_PIN_REQD_STR:
		str = (const UInt8*)"Corporate personalisation PIN required";
		break;

	case AT_CME_ERR_CORP_LOCK_PUK_REQD_STR:
		str = (const UInt8*)"Corporate personalisation PUK required";
		break;

	case AT_CME_ERR_UNKNOWN_STR:
		str = (const UInt8*)"Unknown";
		break;

	case AT_CME_ERR_ILLEGAL_MS_STR:
		str = (const UInt8*)"Illegal MS";
		break;

	case AT_CME_ERR_ILLEGAL_ME_STR:
		str = (const UInt8*)"Illegal ME";
		break;

	case AT_CME_ERR_GPRS_SVC_NOT_ALLOWED_STR:
		str = (const UInt8*)"GPRS services not allowed";
		break;

	case AT_CME_ERR_PLMN_NOT_ALLOWED_STR:
		str = (const UInt8*)"PLMN not allowed";
		break;

	case AT_CME_ERR_LA_NOT_ALLOWED_STR:
		str = (const UInt8*)"Location area not allowed";
		break;

	case AT_CME_ERR_ROAM_NOT_ALLOWED_STR:
		str = (const UInt8*)"Roaming not allowed in this location area";
		break;

	case AT_CME_ERR_INSUFFICIENT_RESOURCES_STR:
		str = (const UInt8*)"Insufficient resources";
		break;

	case AT_CME_ERR_MISSING_OR_UNKNOWN_APN_STR:
		str = (const UInt8*)"Missing or unknown APN";
		break;

	case AT_CME_ERR_UNKNOWN_PDP_ADDRESS_STR:
		str = (const UInt8*)"Unknown PDP address";
		break;

	case AT_CME_ERR_ACTIVATION_REJECTED_BY_GGSN_STR:
		str = (const UInt8*)"Activation rejected by GGSN";
		break;

	case AT_CME_ERR_ACTIVATION_REJECTED_UNSPECIFIED_STR:
		str = (const UInt8*)"Activation rejected, unspecified";
		break;

	case AT_CME_ERR_SERV_OPTION_NOT_SUPPORTED_STR:
		str = (const UInt8*)"Service option not supported";
		break;

	case AT_CME_ERR_SERV_OPTION_NOT_SUSCRIBED_STR:
		str = (const UInt8*)"Requested service option not subscribed";
		break;

	case AT_CME_ERR_SERV_OPTION_OUT_OF_ORDER_STR:
		str = (const UInt8*)"Service option temporarily out of order";
		break;

	case AT_CME_ERR_NSAPI_ALREADY_USED_STR:
		str = (const UInt8*)"NSAPI already used";
		break;

	case AT_CME_ERR_REGULAR_DEACTIVATION_STR:
		str = (const UInt8*)"Regular deactivation";
		break;

	case AT_CME_ERR_QOS_NOT_ACCEPTED_STR:
		str = (const UInt8*)"QOS not accepted";
		break;

	case AT_CME_ERR_PCH_NETWORK_FAILURE_STR:
		str = (const UInt8*)"PDP network failure";
		break;

	case AT_CME_ERR_REACTIVATION_REQUIRED_STR:
		str = (const UInt8*)"Reactivation required";
		break;

	case AT_CME_ERR_PDP_AUTHENTICATE_FAIL_STR:
		str = (const UInt8*)"PDP authentication failure";
		break;

	case AT_CME_ERR_NO_NETWORK_STR:
		str = (const UInt8*)"NO NETWORK";
		break;

	case AT_CMS_ERR_ME_FAILURE_STR:
		str = (const UInt8*)"ME failure";
		break;

	case AT_CMS_ERR_SMS_ME_SERV_RESERVED_STR:
		str = (const UInt8*)"SMS service of ME reserved";
		break;

	case AT_CMS_ERR_OP_NOT_ALLOWED_STR:
		str = (const UInt8*)"Operation not allowed";
		break;

	case AT_CMS_ERR_OP_NOT_SUPPORTED_STR:
		str = (const UInt8*)"Operation not supported";
		break;

	case AT_CMS_ERR_INVALID_PUD_MODE_PARM_STR:
		str = (const UInt8*)"Invalid PDU mode parameter";
		break;

	case AT_CMS_ERR_INVALID_TEXT_MODE_PARM_STR:
		str = (const UInt8*)"Invalid text mode parameter";
		break;

	case AT_CMS_ERR_SIM_NOT_INSERTED_STR:
		str = (const UInt8*)"SIM not inserted";
		break;

	case AT_CMS_ERR_SIM_PIN_REQUIRED_STR:
		str = (const UInt8*)"PIN required";
		break;

	case AT_CMS_ERR_PH_SIM_PIN_REQUIRED_STR:
		str = (const UInt8*)"PH-SIM PIN required";
		break;

	case AT_CMS_ERR_SIM_FAILURE_STR:
		str = (const UInt8*)"SIM failure";
		break;

	case AT_CMS_ERR_SIM_BUSY_STR:
		str = (const UInt8*)"SIM busy";
		break;

	case AT_CMS_ERR_SIM_WRONG_STR:
		str = (const UInt8*)"SIM wrong";
		break;

	case AT_CMS_ERR_SIM_PUK_REQUIRED_STR:
		str = (const UInt8*)"SIM PUK required";
		break;

	case AT_CMS_ERR_SIM_PIN2_REQUIRED_STR:
		str = (const UInt8*)"SIM PIN2 required";
		break;

	case AT_CMS_ERR_SIM_PUK2_REQUIRED_STR:
		str = (const UInt8*)"SIM PUK2 required";
		break;

	case AT_CMS_ERR_MEMORY_FAILURE_STR:
		str = (const UInt8*)"Memory failure";
		break;

	case AT_CMS_ERR_INVALID_MEMORY_INDEX_STR:
		str = (const UInt8*)"Invalid memory index";
		break;

	case AT_CMS_ERR_MEMORY_FULL_STR:
		str = (const UInt8*)"Memory full";
		break;

	case AT_CMS_ERR_SMSC_ADDR_UNKNOWN_STR:
		str = (const UInt8*)"SMSC address unknown";
		break;

	case AT_CMS_ERR_NO_NETWORK_SERVICE_STR:
		str = (const UInt8*)"No network service";
		break;

	case AT_CMS_ERR_NETWORK_TIMEOUT_STR:
		str = (const UInt8*)"Network timeout";
		break;

	case AT_CMS_ERR_NO_CNMA_EXPECTED_STR:
		str = (const UInt8*)"No +CNMA acknowledgement expected";
		break;

	case AT_CMS_ERR_UNKNOWN_ERROR_STR:
		str = (const UInt8*)"Unknown error";
		break;

	case ATC_CME_ERR_ATC_BUSY_STR:
		str = (const UInt8*)"ATC still in processing other AT command";
		break;

	case AT_CME_ERR_AT_COMMAND_TIMEOUT_STR:
		str = (const UInt8*)"Current AT command timed out";
		break;

	case AT_CMS_MO_SMS_IN_PROGRESS_STR:
		str = (const UInt8*)"MO SMS in progress";
		break;

	case AT_CMS_SATK_CALL_CONTROL_BARRING_STR:
		str = (const UInt8*)"Call or MO SMS to this number not allowed";
		break;

	case AT_CMS_FDN_NOT_ALLOWED_STR:
		str = (const UInt8*)"MO CALL/SS/SMS to this number not allowed (FDN)";
		break;

	case AT_AUD_SETMODE_INVALID_STR:
		str = (const UInt8*)"Audio tune: set invalid mode";
		break;

	case AT_AUD_PARMID_MISSING_STR:
		str = (const UInt8*)"Audio tune: parm id missing";
		break;

	case AT_AUD_PARMID_INVALID_STR:
		str = (const UInt8*)"Audio tune: parm id invalid";
		break;

	case AT_AUD_GETPARM_DATA_UNAVAILABLE_STR:
		str = (const UInt8*)"Audio tune: getparm data unavaliable";
		break;

	case AT_AUD_SETPARM_ERROR_STR:
		str = (const UInt8*)"Audio tune: setparm return error";
		break;

	case AT_AUD_STARTTUNE_ERROR_STR:
		str = (const UInt8*)"Audio tune: start tune return error";
		break;

	case AT_AUD_STOPTUNE_SAVE_MISSING_STR:
		str = (const UInt8*)"Audio tune: stop tune <save> param missing";
		break;

	case AT_AUD_STOPTUNE_ERROR_STR:
		str = (const UInt8*)"Audio tune: stop tune return error";
		break;

	case AT_AUD_COMMAND_ERROR_STR:
		str = (const UInt8*)"Audio tune: command error";
		break;


	case AT_INVALID_STR:
	default:
		str = (const UInt8*)"phone failure";
		break;
	}

	return(str);
}


