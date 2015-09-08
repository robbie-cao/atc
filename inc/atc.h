/**
*
*   @file   atc.h
*
*   @brief  This file contains ATC task definitions.
*
****************************************************************************/

#ifndef _ATC_H_
#define _ATC_H_

#include "types.h"
#include "consts.h"
#include "mstypes.h"
#include "pchtypes.h"

#include "at_utsm.h"
#include "mpx.h"
#include "mti_build.h"
#include "taskmsgs.h"
#include "serialapi.h"
#include "at_api.h"

//******************************************************************************
//		Definition
//******************************************************************************

// Supplementary Service Mode in 07.07
#define ATC_UNLOCK_MODE 0	/* Deactivate service */
#define ATC_LOCK_MODE 1		/* Activate service */
#define ATC_QUERY_MODE 2	/* Query service status */


// Specific facility mode for PH-SIM lock */
#define ATC_PS_FULL_LOCK 10


/* Max GID1 and GID2 length we assume */
#define MAX_GID_LENGTH 20

//====================================================================
// Project specific default values enter here
//====================================================================
// The default value should be modified based on project requirements
// generic AT default setting
#define		DEF_CRC		0
#define		DEF_CREG	0
#define		DEF_CGREG	0
#define		DEF_S7		50
//#define		DEF_COLP	0
//#define		DEF_CLIP	0
//#define		DEF_CCWA	0
#define		DEF_ECAM	0
#define		DEF_MVMIND	0
//====================================================================

#define		DEF_E		1


typedef enum
{
	MODULE_NULL						= 0,
	MODULE_GSM_GPRS_READY			= 1,
	MODULE_112_CALL_READY			= 2,
	MODULE_ALL_AT_READY_READY		= 3,
	MODULE_SIM_INSERT_IND			= 4,
	MODULE_SIM_REMOVE_IND			= 5,
	MODULE_NO_SERVICE				= 6,
	MODULE_LIMITED_NOT_RECOVERABLE	= 7,
	MODULE_INVALID_IND				= 0xFF
} MODULE_READY_STATUS_t;

#define		MS_CLASS_GPRS_TYPE	1
#define		MS_CLASS_EGPRS_TYPE	2
#define		MS_CLASS_HSCSD_TYPE	3

//******************************************************************************
// Define callback functions
//******************************************************************************

typedef void (*CB_ReportPower_t)(		// Callback to report power status
	UInt8 func,				// function type
	Boolean	On				// TRUE if power on
	);

void ATC_Init( void );

void ATC_Run( void );

void ATC_Register( void );

void ATC_ReportMrdy( UInt8 status );

void ATC_ReportV24SplitInd(		// Multiple AT cmds split
	UInt8  chan,			// DLC in question
	UInt8 *p_cmd			// Ptr to NULL terminated AT cmd string
					// in case sensitive command string
	);

void  ATC_SetContextDeactivation(UInt8  chan);

void ATC_V24UsrDataInd (               // User data managment (eg SMS input).
        UInt8  chan,                     // DLC in question
        UInt8 *p_Data                  // Probably length is missing...
        );

typedef enum
{
 ATC_CLOSE_TRANS,
 ATC_OPEN_TRANS,
 ATC_GET_STATE
} atc_TransactionReq_t;

typedef enum
{
 ATC_CLOSED_TRANS,
 ATC_OPENED_TRANS
} atc_TransactionState_t;

void atc_PurgeURspBuffer (
        void
        );

UInt8 atc_SetNewURspInBuffer (
        void *Rsp,
        UInt16 Size,
        atc_BufferedURsp_t Type
        );

void *atc_GetOldestURspFromBuffer (
         atc_BufferedURsp_t  *Type
         );


void ATC_ManageBufferedURsp ( void );


#define RP_ACK_MSK 0xF0


//******************************************************************************
// Call back available for SMS storage manager.
//******************************************************************************

// Storage and change status purpose -> ATK_SMS_STORED_EVT
//-----------------------------------------------------------------------------

//void atc_SmsStoredCb ( SIMAccess_t result,  // Access result
//                       UInt16      rec_no); // Record number found, range 0 to n-1

// Call back function for overwriting an existing SMS message with new message
// reference and status in an ATC+CMSS command.
//-----------------------------------------------------------------------------
//void atc_CmssSmsStoredCb( SIMAccess_t result,  // Access result
//						  UInt16      rec_no); // Record number overwritten, range 0 to n-1

// Assynchronous storage (Incoming sms)  -> ATK_INC_SMS_STORED_EVT
//-----------------------------------------------------------------------------

//void atc_IncSmsStoredCb ( SIMAccess_t result,  // Access result
//                          UInt16      rec_no); // Record number found, range 0 to n-1

// Deletion purpose -> ATK_SMS_STORED_EVT
//-----------------------------------------------------------------------------

//void atc_SmsDeletedCb ( SIMAccess_t result,  // Access result
//                        UInt16      rec_no); // Record number found, range 0 to n-1

// Sms read result -> ATK_SMS_READ_EVT
//-----------------------------------------------------------------------------

//void atc_ReadSmsCb (SIMAccess_t  result,    // SIM access result
//                    UInt16       rec_no,    // Record number found, 0 to n-1
//                    SIMSMSMesg_t *sms_mesg); // pointer to SMS Message

//void atc_ReadSmsForSendCb (SIMAccess_t  result,    // SIM access result
//                    UInt16       rec_no,    // Record number found, 0 to n-1
//                    SIMSMSMesg_t *sms_mesg); // pointer to SMS Message


//******************************************************************************
// Misc API.
//******************************************************************************
Boolean  IsPowerDownInProgress(void);			// TRUE: at+cfun=0 power down is in progress
void stopPowerDownDetachTimer(void);


#define RP_ACK_TIMER (Ticks_t)4000


//-----------------------------------------------
// New ATC Function Prototype (tempoary kept here)
//------------------------------------------------
//--- General

//--- MPX Channel Number and API Client ID Conversion ----
UInt8 AT_GetV24ClientIDFromMpxChan(UInt8 mpxChanNum);
UInt8 AT_GetMpxChanFromV24ClientID(UInt8 clientID);


//--- ATC Long Cmd Processor ------------------
void		ATProcessCOPSCmd(InterTaskMsg_t* inMsg);
void		ATProcessCmeError(InterTaskMsg_t *inMsg);

void		AtHandleSimAccessStatusResp(MsgType_t msg_type, UInt8 chan, SIMAccess_t sim_access);
void		AtHandleSimRestrictedAccessResp(InterTaskMsg_t* inMsg);
void		AtHandleSimEfileInfoResp(InterTaskMsg_t* inMsg);
void		AtHandleSimEfileDataResp(InterTaskMsg_t* inMsg);
void		AtHandleAcmValueResp(InterTaskMsg_t* inMsg);
void		AtHandleMaxAcmValueResp(InterTaskMsg_t* inMsg);
void		AtHandlePuctDataResp(InterTaskMsg_t* inMsg);
void		AtHandlePbkInfoResp(InterTaskMsg_t* inMsg);
void		AtHandlePbkEntryDataResp(InterTaskMsg_t* inMsg);
void		AtHandleWritePbkResp(InterTaskMsg_t* inMsg);
void		AtHandlePbkReadyInd(void);
void		AtHandleMsReadyInd(InterTaskMsg_t* inMsg);
void		AtHandleRxSignalChgInd(InterTaskMsg_t* inMsg);
void		AtHandleSimPinAttemptRsp(InterTaskMsg_t* inMsg);
void		AtHandleRSSIInd(InterTaskMsg_t* inMsg);
void        AtHandleGPRSModifyInd(InterTaskMsg_t *InterMsg);
void		AtHandleGPRSReActInd(InterTaskMsg_t *InterMsg);
void		AtHandleTimeZoneInd(InterTaskMsg_t* inMsg);
void		AtHandleNetworkNameInd(InterTaskMsg_t* inMsg);
void		AtHandlePlmnSelectCnf(InterTaskMsg_t *inMsg);

void		AtHandleGPRSCallMonitorStatus( InterTaskMsg_t *inMsg );

// vmind
void AtSendVMWaitingInd(void);


// ATC Open indication response callback
void ATC_ReportPPPOpen(DLC_t	dlc);		// Data Link Connection

// ATC Put command string callback
void ATC_PutPPPCmdString(
	UInt32	str_idx,							// AT Command string index
	int     code,								// Error code, if any
	DLC_t   dlc
	);

// ATC Close request callback
void ATC_ReportPPPCloseReq(DLC_t	dlc);	// Data Link Connection


//--- ATC State Related Functions (at_state.c)
extern void		SetLongATCmdRspPendingState(AT_CmdId_t cmdId, Boolean true_false);
extern Boolean	GetLongATCmdRspPendingState(AT_CmdId_t cmdId);


//--- Call Control Related Functions


//--- Network Related Functions (at_netwk.c)
extern Result_t AtHandleCipherIndication(InterTaskMsg_t* msg);
extern Result_t	AtHandlePlmnListInd(InterTaskMsg_t* msg);

extern	void AtHandleAttachCnf(InterTaskMsg_t *InterMsg);

//---- SIM Related Functions to process response from SIM
void		AtHandleEPROResp(InterTaskMsg_t* inMsg);

void		SetATUnsolicitedEvent(Boolean true_false);
Boolean		IsATUnsolicitedEventEnabled(void);


void SysProcessDownloadReq( void );
void SysProcessCalibrateReq( void );

//----- Serial device Handle for ATC
extern SerialHandle_t serialHandleforAT;

Boolean ATC_IsCharacterSetUcs2(void);

#endif // _ATC_H_

