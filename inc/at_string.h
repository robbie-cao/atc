#ifndef _AT_STRING_H_
#define _AT_STRING_H_


//-------------------------------------------------
// Constant Definitions
//-------------------------------------------------


//---------------------------------------------------------------
// enum
//---------------------------------------------------------------

//-------------------------------------------------------------------
//	If you add a string to this table please ensure that it is
//	handled in GetATString().
//-------------------------------------------------------------------
typedef enum {
	AT_OK_STR,
	AT_ERROR_STR,

	AT_CONNECT_STR,
	AT_RING_STR,
    AT_NO_CARRIER_STR,
	AT_EMPTY_STR,
	AT_NO_DIALTONE_STR,
	AT_BUSY_STR,
	AT_NO_ANSWER_STR,


    AT_CONNECT_300_STR,
    AT_CONNECT_1200_STR,
	AT_CONNECT_1200_75_STR,
    AT_CONNECT_2400_STR,
    AT_CONNECT_4800_STR,
    AT_CONNECT_9600_STR,
    AT_CONNECT_14400_STR,
    AT_CONNECT_19200_STR,
	AT_CONNECT_28800_STR,
    AT_CONNECT_38400_STR,
    AT_CONNECT_43200_STR,
    AT_CONNECT_56000_STR,
    AT_CONNECT_57600_STR,

	AT_CME_ERROR_STR,
	AT_CMS_ERROR_STR,
	AT_CPIN_READY_STR,
	AT_CPIN_SIM_PIN_STR,
	AT_CPIN_SIM_PUK_STR,
	AT_CPIN_SIM_PIN2_STR,
	AT_CPIN_SIM_PUK2_STR,
	AT_CPIN_PH_SIM_PIN_STR,
	AT_CPIN_PH_NET_PIN_STR,
	AT_CPIN_PH_NETSUB_PIN_STR,

	AT_CRING_VOICE_STR,
	AT_CRING_FAX_STR,
	AT_CRING_ASYNC_STR,
	AT_CRING_REL_ASYNC_STR,
	AT_CRING_SYNC_STR,
	AT_CRING_REL_SYNC_STR,

	AT_CR_ASYNC_STR,
	AT_CR_REL_ASYNC_STR,
	AT_CR_SYNC_STR,
	AT_CR_REL_SYNC_STR,

	// ERROR strings
	AT_CME_ERR_PHONE_FAILURE_STR,
	AT_CME_ERR_NO_CONNECTION_STR,
	AT_CME_ERR_PH_ADAPTOR_RESERVED_STR,
	AT_CME_ERR_OP_NOT_ALLOWED_STR,
	AT_CME_ERR_OP_NOT_SUPPORTED_STR,
	AT_CME_ERR_PH_SIM_PIN_REQD_STR,
	AT_CME_ERR_PH_FSIM_PIN_REQD_STR,
	AT_CME_ERR_PH_FSIM_PUK_REQD_STR,
	AT_CME_ERR_PH_SIM_NOT_INSERTED_STR,
	AT_CME_ERR_SIM_PIN_REQD_STR,
	AT_CME_ERR_SIM_PUK_REQD_STR,
	AT_CME_ERR_SIM_FAILURE_STR,
	AT_CME_ERR_SIM_BUSY_STR,
	AT_CME_ERR_SIM_WRONG_STR,
	AT_CME_ERR_SIM_PASSWD_WRONG_STR,
	AT_CME_ERR_SIM_PIN2_REQD_STR,
	AT_CME_ERR_SIM_PUK2_REQD_STR,
	AT_CME_ERR_MEMORY_FULL_STR,
	AT_CME_ERR_INVALID_INDEX_STR,
	AT_CME_ERR_NOT_FOUND_STR,
	AT_CME_ERR_MEMORY_FAILURE_STR,
	AT_CME_ERR_TXTSTR_TOO_LONG_STR,
	AT_CME_ERR_INVALID_CHAR_INSTR_STR,
	AT_CME_ERR_DIAL_STR_TOO_LONG_STR,
	AT_CME_ERR_INVALID_DIALSTR_CHAR_STR,
	AT_CME_ERR_NO_NETWORK_SVC_STR,
	AT_CME_ERR_NETWORK_TIMEOUT_STR,
	AT_CME_ERR_NETWORK_NOT_ALLOWED_STR,
	AT_CME_ERR_NETWORK_LOCK_PIN_REQD_STR,
	AT_CME_ERR_NETWORK_LOCK_PUK_REQD_STR,
	AT_CME_ERR_SUBNET_LOCK_PIN_REQD_STR,
	AT_CME_ERR_SUBNET_LOCK_PUK_REQD_STR,
	AT_CME_ERR_SP_LOCK_PIN_REQD_STR,
	AT_CME_ERR_SP_LOCK_PUK_REQD_STR,
	AT_CME_ERR_CORP_LOCK_PIN_REQD_STR,
	AT_CME_ERR_CORP_LOCK_PUK_REQD_STR,
	AT_CME_ERR_UNKNOWN_STR,

	AT_CME_ERR_ILLEGAL_MS_STR,
	AT_CME_ERR_ILLEGAL_ME_STR,
	AT_CME_ERR_GPRS_SVC_NOT_ALLOWED_STR,
	AT_CME_ERR_PLMN_NOT_ALLOWED_STR,
	AT_CME_ERR_LA_NOT_ALLOWED_STR,
	AT_CME_ERR_ROAM_NOT_ALLOWED_STR,
	AT_CME_ERR_INSUFFICIENT_RESOURCES_STR,
	AT_CME_ERR_MISSING_OR_UNKNOWN_APN_STR,
	AT_CME_ERR_UNKNOWN_PDP_ADDRESS_STR,
	AT_CME_ERR_USER_AUTH_FAILED_STR,
	AT_CME_ERR_ACTIVATION_REJECTED_BY_GGSN_STR,
	AT_CME_ERR_ACTIVATION_REJECTED_UNSPECIFIED_STR,
	AT_CME_ERR_SERV_OPTION_NOT_SUPPORTED_STR,
	AT_CME_ERR_SERV_OPTION_NOT_SUSCRIBED_STR,
	AT_CME_ERR_SERV_OPTION_OUT_OF_ORDER_STR,
	AT_CME_ERR_NSAPI_ALREADY_USED_STR,
	AT_CME_ERR_REGULAR_DEACTIVATION_STR,
	AT_CME_ERR_QOS_NOT_ACCEPTED_STR,
	AT_CME_ERR_PCH_NETWORK_FAILURE_STR,
	AT_CME_ERR_REACTIVATION_REQUIRED_STR,
	AT_CME_ERR_PDP_AUTHENTICATE_FAIL_STR,

	AT_CME_ERR_NO_NETWORK_STR,
	AT_CMS_ERR_ME_FAILURE_STR,
	AT_CMS_ERR_SMS_ME_SERV_RESERVED_STR,
	AT_CMS_ERR_OP_NOT_ALLOWED_STR,
	AT_CMS_ERR_OP_NOT_SUPPORTED_STR,
	AT_CMS_ERR_INVALID_PUD_MODE_PARM_STR,
	AT_CMS_ERR_INVALID_TEXT_MODE_PARM_STR,
	AT_CMS_ERR_SIM_NOT_INSERTED_STR,
	AT_CMS_ERR_SIM_PIN_REQUIRED_STR,
	AT_CMS_ERR_PH_SIM_PIN_REQUIRED_STR,
	AT_CMS_ERR_SIM_FAILURE_STR,
	AT_CMS_ERR_SIM_BUSY_STR,
	AT_CMS_ERR_SIM_WRONG_STR,
	AT_CMS_ERR_SIM_PUK_REQUIRED_STR,
	AT_CMS_ERR_SIM_PIN2_REQUIRED_STR,
	AT_CMS_ERR_SIM_PUK2_REQUIRED_STR,
	AT_CMS_ERR_MEMORY_FAILURE_STR,
	AT_CMS_ERR_INVALID_MEMORY_INDEX_STR,
	AT_CMS_ERR_MEMORY_FULL_STR,
	AT_CMS_ERR_SMSC_ADDR_UNKNOWN_STR,
	AT_CMS_ERR_NO_NETWORK_SERVICE_STR,
	AT_CMS_ERR_NETWORK_TIMEOUT_STR,
	AT_CMS_ERR_NO_CNMA_EXPECTED_STR,
	AT_CMS_ERR_UNKNOWN_ERROR_STR,

	ATC_CME_ERR_ATC_BUSY_STR,
	AT_CME_ERR_AT_COMMAND_TIMEOUT_STR,

	AT_CMS_MO_SMS_IN_PROGRESS_STR,
	AT_CMS_SATK_CALL_CONTROL_BARRING_STR,
	AT_CMS_FDN_NOT_ALLOWED_STR,

	AT_AUD_SETMODE_INVALID_STR,
	AT_AUD_PARMID_MISSING_STR,
	AT_AUD_PARMID_INVALID_STR,
	AT_AUD_GETPARM_DATA_UNAVAILABLE_STR,
	AT_AUD_SETPARM_ERROR_STR,
	AT_AUD_STARTTUNE_ERROR_STR,
	AT_AUD_STOPTUNE_SAVE_MISSING_STR,
	AT_AUD_STOPTUNE_ERROR_STR,
	AT_AUD_COMMAND_ERROR_STR,


	AT_INVALID_STR

} AT_ConstString_t;


//-------------------------------------------------
// Data Structure
//-------------------------------------------------


//-------------------------------------------------
// Function Prototype
//-------------------------------------------------
extern const UInt8*	GetATString( AT_ConstString_t stringName ) ;


#endif  // _AT_STRING_H_

