/**
*
*   @file   at_rspcode.h
*
*   @brief  This file defines AT response codes.
*
****************************************************************************/

#ifndef _AT_RSPCODE_H_
#define _AT_RSPCODE_H_

// ---- ATC Error Code ------

// ATC Errors (0 - 255) for 07.07 reserved by ETSI
#define  AT_CME_ERR_PHONE_FAILURE				0
#define  AT_CME_ERR_NO_CONNECTION				1
#define  AT_CME_ERR_PH_ADAPTOR_RESERVED			2
#define  AT_CME_ERR_OP_NOT_ALLOWED				3
#define  AT_CME_ERR_OP_NOT_SUPPORTED			4
#define  AT_CME_ERR_PH_SIM_PIN_REQD				5
#define  AT_CME_ERR_PH_FSIM_PIN_REQD			6
#define  AT_CME_ERR_PH_FSIM_PUK_REQD			7
#define  AT_CME_ERR_PH_SIM_NOT_INSERTED			10
#define  AT_CME_ERR_SIM_PIN_REQD				11
#define  AT_CME_ERR_SIM_PUK_REQD				12
#define  AT_CME_ERR_SIM_FAILURE					13
#define  AT_CME_ERR_SIM_BUSY					14
#define  AT_CME_ERR_SIM_WRONG					15
#define  AT_CME_ERR_SIM_PASSWD_WRONG			16
#define  AT_CME_ERR_SIM_PIN2_REQD				17
#define  AT_CME_ERR_SIM_PUK2_REQD				18
#define  AT_CME_ERR_MEMORY_FULL					20
#define  AT_CME_ERR_INVALID_INDEX				21
#define  AT_CME_ERR_NOT_FOUND					22
#define  AT_CME_ERR_MEMORY_FAILURE				23
#define  AT_CME_ERR_TXTSTR_TOO_LONG				24
#define  AT_CME_ERR_INVALID_CHAR_INSTR			25
#define  AT_CME_ERR_DIAL_STR_TOO_LONG			26
#define  AT_CME_ERR_INVALID_DIALSTR_CHAR		27
#define  AT_CME_ERR_NO_NETWORK_SVC				30
#define  AT_CME_ERR_NETWORK_TIMEOUT				31
#define  AT_CME_ERR_NETWORK_NOT_ALLOWED			32
#define  AT_CME_ERR_NETWORK_LOCK_PIN_REQD		40
#define  AT_CME_ERR_NETWORK_LOCK_PUK_REQD		41
#define  AT_CME_ERR_SUBNET_LOCK_PIN_REQD		42
#define  AT_CME_ERR_SUBNET_LOCK_PUK_REQD		43
#define  AT_CME_ERR_SP_LOCK_PIN_REQD			44
#define  AT_CME_ERR_SP_LOCK_PUK_REQD			45
#define  AT_CME_ERR_CORP_LOCK_PIN_REQD			46
#define  AT_CME_ERR_CORP_LOCK_PUK_REQD			47
#define  AT_CME_ERR_UNKNOWN						100

#define  AT_CME_ERR_ILLEGAL_MS					103		// GPRS related errors
#define  AT_CME_ERR_ILLEGAL_ME					106
#define  AT_CME_ERR_GPRS_SVC_NOT_ALLOWED		107
#define  AT_CME_ERR_PLMN_NOT_ALLOWED			111
#define  AT_CME_ERR_LA_NOT_ALLOWED				112
#define  AT_CME_ERR_ROAM_NOT_ALLOWED			113

#define  AT_CME_ERR_INSUF_RESOURCES				126
#define  AT_CME_ERR_MISSING_OR_UNKNOWN_APN		127
#define  AT_CME_ERR_UNKNOWN_PDP_ADDRESS			128
#define  AT_CME_ERR_ACTIVATION_REJECTED_BY_GGSN	130
#define  AT_CME_ERR_ACTIVATION_REJECTED			131
#define  AT_CME_ERR_SERV_OPTION_NOT_SUPPORTED	132
#define  AT_CME_ERR_SERV_OPTION_NOT_SUSCRIBED	133
#define  AT_CME_ERR_SERV_OPTION_OUT_OF_ORDER	134
#define  AT_CME_ERR_NSAPI_ALREADY_USED			135
#define  AT_CME_ERR_REGULAR_DEACT				136
#define  AT_CME_ERR_QOS_NOT_ACCEPTED			137
#define  AT_CME_ERR_PDP_NETWORK_FAILURE			138
#define  AT_CME_ERR_REACTIVATION_REQUIRED		139
#define  AT_CME_ERR_PDP_AUTHENTICATE_FAIL		149


//---------------------------------------------------------------------
// Note: per GSM 07.05 §3.2.5. CMS error below 512 are reserved by ETSI
//       per GSM 07.07 §9.2.1: CME error below 256 are reserved by ETSI
//----------------------------------------------------------------------
// Customized ATC Errors
#define  AT_CME_ERR_NO_NETWORK					256

// CMS ERROR based on Diana IWD 24.2
#define  AT_CMS_ERR_ME_FAILURE					300
#define  AT_CMS_ERR_SMS_ME_SERV_RESERVED		301
#define  AT_CMS_ERR_OP_NOT_ALLOWED				302
#define  AT_CMS_ERR_OP_NOT_SUPPORTED			303
#define  AT_CMS_ERR_INVALID_PUD_MODE_PARM		304
#define  AT_CMS_ERR_INVALID_TEXT_MODE_PARM		305
#define  AT_CMS_ERR_SIM_NOT_INSERTED			310
#define  AT_CMS_ERR_SIM_PIN_REQUIRED			311
#define  AT_CMS_ERR_PH_SIM_PIN_REQUIRED			312
#define  AT_CMS_ERR_SIM_FAILURE					313
#define  AT_CMS_ERR_SIM_BUSY					314
#define  AT_CMS_ERR_SIM_WRONG					315
#define  AT_CMS_ERR_SIM_PUK_REQUIRED			316
#define  AT_CMS_ERR_SIM_PIN2_REQUIRED			317
#define  AT_CMS_ERR_SIM_PUK2_REQUIRED			318
#define  AT_CMS_ERR_MEMORY_FAILURE				320
#define  AT_CMS_ERR_INVALID_MEMORY_INDEX		321
#define  AT_CMS_ERR_MEMORY_FULL					322
#define  AT_CMS_ERR_SMSC_ADDR_UNKNOWN			330
#define  AT_CMS_ERR_NO_NETWORK_SERVICE			331
#define  AT_CMS_ERR_NETWORK_TIMEOUT				332
#define  AT_CMS_ERR_NO_CNMA_EXPECTED			340
#define  AT_CMS_ERR_UNKNOWN_ERROR				500

#define  AT_CMS_MM_ESTABLISHMENT_FAILURE		512
#define  AT_CME_ERR_AT_BUSY						513		// current AT command not yet done
#define  AT_CME_ERR_AT_COMMAND_TIMEOUT			514		// current AT command timeout
#define  AT_CMS_MO_SMS_IN_PROGRESS				515

#define	 AT_CMS_SATK_CALL_CONTROL_BARRING		517
#define	 AT_CMS_FDN_NOT_ALLOWED					520

#define	 AT_AUD_SETMODE_INVALID					600
#define  AT_AUD_PARMID_MISSING					601
#define  AT_AUD_PARMID_INVALID					602
#define  AT_AUD_GETPARM_DATA_UNAVAILABLE		603
#define  AT_AUD_SETPARM_ERROR					604
#define  AT_AUD_STARTTUNE_ERROR					605
#define  AT_AUD_STOPTUNE_SAVE_MISSING			606
#define  AT_AUD_STOPTUNE_ERROR					607
#define  AT_AUD_COMMAND_ERROR					608

#define  AT_NO_CARRIER_RSP						1001
#define  AT_ERROR_RSP							1002
#define  AT_NO_DIALTONE_RSP						1003
#define  AT_BUSY_RSP							1004
#define  AT_NO_ANSWER_RSP						1005
#define  AT_IDLE_RSP							1006
#define  AT_CONNECT_300_RSP						1007
#define  AT_CONNECT_1200_RSP					1008
#define  AT_CONNECT_1200_75_RSP					1009
#define  AT_CONNECT_2400_RSP					1010
#define  AT_CONNECT_4800_RSP					1011
#define  AT_CONNECT_9600_RSP					1012
#define  AT_CME_ERR_RSP							1013
#define  AT_CMS_ERR_RSP							1014
#define  AT_RING_VOICE_RSP						1015
#define  AT_RING_FAX_RSP						1016
#define  AT_RING_ASYNC_RSP						1017
#define  AT_RING_RELASYNC_RSP					1018
#define  AT_RING_SYNC_RSP						1019
#define  AT_RING_RELSYNC_RSP					1020
#define  AT_OK_RSP								1021
#define  AT_CONNECT_RSP							1022
#define  AT_RING_RSP							1023

#endif // _AT_RSPCODE_H_

