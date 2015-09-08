//-----------------------------------------------------------------------------
//	at_task.win32.c:
//
//		Implements the AT asynchronous command handler task.
//
//	Note:
//
//		This is a Windows simulation -- it will evolve into target code.
//-----------------------------------------------------------------------------

#define ENABLE_LOGGING

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "at_api.h"
#include "ms_database.h"
#include "at_cfg.h"
#include "mti_trace.h"
#include "dialstr.h"
#include "ss_api.h"

DECLARE_LOG_FILE ;

SystemState_t 	systemState = SYSTEM_STATE_ON ;

//-----------------------------------------------------------------------------
//	Abdi's ssapi struct dispatcher --- it will eventually 'live' somewhere in
//	CC.
//-----------------------------------------------------------------------------

//******************************************************************************
// Function Name: DispatchSsApiReq
//
//******************************************************************************
static Result_t DispatchSsApiReq(SS_SsApiReq_t *theSsApiReq)
{
	int inClientId = 0 ;

 switch (theSsApiReq->ssReq)
 {
  case SS_API_CALL_FORWARDING_REQUEST:
   return (SS_SendCallForwardReq( inClientId,
           theSsApiReq->mode,
           theSsApiReq->reason,
           theSsApiReq->svcCls,
           theSsApiReq->waitToFwdSec,
           theSsApiReq->number));

  case SS_API_QUERY_CALL_FORWARDING_REQUEST:
   return (SS_QueryCallForwardStatus( inClientId,
            theSsApiReq->reason,
            theSsApiReq->svcCls));

  case SS_API_CALL_BARRING_REQUEST:
   return (SS_SendCallBarringReq( inClientId,
           theSsApiReq->mode,
           theSsApiReq->callBarType,
           theSsApiReq->svcCls,
           theSsApiReq->oldPwd));

  case SS_API_QUERY_CALL_BARRING_REQUEST:
   return (SS_QueryCallBarringStatus( inClientId,
            theSsApiReq->callBarType,
            theSsApiReq->svcCls));

  case SS_API_CALL_BAR_PW_CHANGE_REQUEST:
   return 0;


  case SS_API_CALL_WAITING_REQUEST:
   return (SS_SendCallWaitingReq( inClientId,
           theSsApiReq->mode,
           theSsApiReq->svcCls));


  case SS_API_QUERY_CALL_WAITING_REQUEST:
   return (SS_QueryCallWaitingStatus( inClientId,
            theSsApiReq->svcCls));


  case SS_API_QUERY_CLIP_STATUS_REQUEST:
   return (SS_QueryCallingLineIDStatus(inClientId));

  case SS_API_QUERY_COLP_STATUS_REQUEST:
   return (SS_QueryConnectedLineIDStatus(inClientId));

  case SS_API_QUERY_CLIR_STATUS_REQUEST:
   return (SS_QueryCallingLineRestrictionStatus(inClientId));

  case SS_API_QUERY_COLR_STATUS_REQUEST:
   return (SS_QueryConnectedLineRestrictionStatus(inClientId));

  case SS_API_QUERY_CNAP_STATUS_REQUEST:
   return (SS_QueryCallingNAmePresentStatus(inClientId));

  default:
   return (RESULT_ERROR);
 }
}

//-----------------------------------------------------------------------------
//	RTOS emulation
//-----------------------------------------------------------------------------
#if 0
void* OSHEAP_Alloc ( UInt32 size )
{
	return calloc ( size, 1 ) ;
}

void OSHEAP_Delete ( void* ptr )
{
	free ( ptr ) ;
}
#endif
void *dbHEAP_Alloc( UInt32 size, char* file, int line )
{
	return calloc( size, 1 ) ;
}

void dbHEAP_Delete( void *ptr, char* file, int line )
{
	free( ptr ) ;
}

Semaphore_t OSSEMAPHORE_Create (
	SCount_t	count,
	OSSuspend_t mode )
{
	return CreateSemaphore (
		0,				//	security attributes
		0,				//	initial count
		count,			//	maximum count
		0 ) ;			//	name
}

OSStatus_t OSSEMAPHORE_Obtain (
	Semaphore_t s,
	Ticks_t		timeout	)
{
	WaitForSingleObject ( s, INFINITE ) ;	//	don't support 'timeout' arg
	return OSSTATUS_SUCCESS ;
}

OSStatus_t OSSEMAPHORE_Release (
	Semaphore_t s )
{
	ReleaseSemaphore ( s, 1, 0 ) ;
	return OSSTATUS_SUCCESS ;
}

void WakeupHost( void )
{
}

void OSTASK_Sleep ( UInt32 s )
{
	Sleep( s ) ;
}


//-----------------------------------------------------------------------------
//	MPX emulation
//-----------------------------------------------------------------------------
MuxMode_t MPX_ObtainMuxMode( void )
{
	return NONMUX_MODE ;
}

SystemState_t		SYS_GetSystemState(void)
{
	return SYSTEM_STATE_ON ;
}

//-----------------------------------------------------------------------------
//	V24 emulation
//-----------------------------------------------------------------------------
typedef struct {
	UInt8 cmdAvail ;
	UInt8 cmdBuf [1024];
}	V24_TaskStatus_t ;

static V24_TaskStatus_t v24TaskStatus ;

void V24_Init ( void )
{
	v24TaskStatus.cmdAvail = 0 ;
}

void V24_Exec ( char* cmdBuf )
{
	strcpy ( v24TaskStatus.cmdBuf, cmdBuf ) ;
	v24TaskStatus.cmdAvail = 1 ;
}

DWORD WINAPI V24_Task ( LPVOID lpParam )
{
	for ( ;; ) {

		Sleep ( 10 ) ;

		if ( v24TaskStatus.cmdAvail ) {
			v24TaskStatus.cmdAvail = 0 ;
			AT_ProcessCmd ( 0, v24TaskStatus.cmdBuf ) ;
		}

	}
	return 0 ;
}

Int16 V24_PutTxBlock( DLC_t dlc, BufferDatum_t* buf, Int16 len )
{
	printf( "%s\n", buf ) ;
	return len ;
}

V24OperMode_t V24_GetOperationMode( DLC_t dlc )
{
	return 0 ;
}

//-----------------------------------------------------------------------------
//	SIM emulation
//-----------------------------------------------------------------------------

SIM_PIN_Status_t SIM_GetPinStatus( void )
{
	return PIN_READY_STATUS ;
}

//-----------------------------------------------------------------------------
//	ATC emulation
//-----------------------------------------------------------------------------

typedef struct {
	MsgType_t	msgType ;
	Semaphore_t	protSem ;
	Semaphore_t	pendSem ;
	UInt8*		msgBuf ;
}	ATC_TaskStatus_t ;

static ATC_TaskStatus_t atcTaskStatus ;

void ATC_Init ( void )
{
	atcTaskStatus.protSem = OSSEMAPHORE_Create ( 1, OSSUSPEND_FIFO ) ;
	atcTaskStatus.pendSem = OSSEMAPHORE_Create ( 1, OSSUSPEND_FIFO ) ;

	OSSEMAPHORE_Release ( atcTaskStatus.protSem ) ;
}

void RecvATCMsg ( MsgType_t* msgType, UInt8** cmdBuf )
{
	OSSEMAPHORE_Obtain ( atcTaskStatus.pendSem, INFINITE ) ;

	OSSEMAPHORE_Obtain ( atcTaskStatus.protSem, INFINITE ) ;
	*msgType = atcTaskStatus.msgType ;
	*cmdBuf  = atcTaskStatus.msgBuf ;
	OSSEMAPHORE_Release ( atcTaskStatus.protSem ) ;

}

void SendATCMsg ( MsgType_t msgType, UInt8* cmdBuf )
{
	OSSEMAPHORE_Obtain ( atcTaskStatus.protSem, INFINITE ) ;
	atcTaskStatus.msgType = msgType ;
	atcTaskStatus.msgBuf = cmdBuf ;
	OSSEMAPHORE_Release ( atcTaskStatus.protSem ) ;
	OSSEMAPHORE_Release ( atcTaskStatus.pendSem ) ;
}

typedef enum {
	MSG_ATC_LONG_CMD	=	0x2000,	//	send a long command
	MSG_ATC_LONG_CMD_ERROR			//	send long command and generate error
}	MsgType_t ;

DWORD WINAPI ATC_Task ( LPVOID lpParam )
{
	MsgType_t	msgType ;
	UInt8*		cmdBuf ;

	for ( ;; ) {

		RecvATCMsg ( &msgType, &cmdBuf ) ;

		switch ( msgType ) {
		case MSG_ATC_LONG_CMD:
			Sleep ( 5000 ) ;
			printf(" %s\n", AT_GetParm( 0, 0 ) ) ;
			AT_CmdRspOK(0) ;
			break;

		case MSG_ATC_LONG_CMD_ERROR:
			Sleep ( 1000 ) ;
			AT_CmdRspError(0, 1234) ;
			break;

		default:
			assert(0);
		}

	}

	return 0 ;
}

static AT_ChannelCfg_t cfg ;
AT_ChannelCfg_t* AT_GetChannelCfg(UInt8 ch)
{
	return &cfg ;
}

void SetChannelConfig( int Q, int V, int S3, int S4, int CMEE, int CRC )
{
	cfg.Q = Q ;
	cfg.V = V ;
	cfg.S3 = S3 ;
	cfg.S4 = S4 ;
	cfg.CMEE = CMEE ;
	cfg.CRC = CRC ;
}

//-----------------------------------------------------------------------------
//	Command handlers
//-----------------------------------------------------------------------------
//
//	implement command handlers
//
//	Note; for PC simulation a stub is ALWAYS implemented for each handler.  To
//	implement a custom handler, #define IMP_handlerName where handlerName is the name
//	of the command handler as defined in at_cmd.tbl.  This will exclude the
//	stub from compilation.
//

#define IMP_ATCmd_AND_C_Handler
#define IMP_ATCmd_AND_D_Handler
#define IMP_ATCmd_D_Handler
#define IMP_ATCmd_SI_Handler
#define IMP_ATCmd_LI_Handler
#define IMP_ATCmd_LN_Handler

AT_Status_t ATCmd_SI_Handler ( AT_CmdId_t cmdId, UInt8 ch, UInt8 accessType )
{
	printf("ATCmd_SI_Handler\n");
	return AT_STATUS_DONE;
}

DWORD WINAPI LI_Task(  LPVOID lpParam )
{
	OSTASK_Sleep( 5000 ) ;
	printf("LI_Task\n");
	return 0 ;
}

DWORD WINAPI LN_Task(  LPVOID lpParam )
{
	OSTASK_Sleep( 5000 ) ;
	printf("LN_Task\n");
	AT_CmdRspOK( 0 ) ;
//	AT_CmdRspOK( 0 ) ;
	return 0 ;
}

AT_Status_t ATCmd_LI_Handler ( AT_CmdId_t cmdId, UInt8 ch, UInt8 accessType )
{
	printf("ATCmd_LI_Handler\n");
	CreateThread (
		0,
		1024,
  		LI_Task,
  		0,
  		0,
  		0 ) ;

	return AT_STATUS_PENDING ;
}

AT_Status_t ATCmd_LN_Handler ( AT_CmdId_t cmdId, UInt8 ch, UInt8 accessType )
{
	printf("ATCmd_LN_Handler\n");
	CreateThread (
		0,
		1024,
  		LN_Task,
  		0,
  		0,
  		0 ) ;

	return AT_STATUS_PENDING ;
}


AT_Status_t ATCmd_AND_C_Handler ( AT_CmdId_t cmdId, UInt8 ch, UInt8 accessType, const UInt8* _P1 )
{
	printf ( "\n	==>ATCmd_AND_C_Handler (custom implementation)\n" ) ;

	switch ( accessType ) {

	case AT_ACCESS_REGULAR:
		printf ( "	   AccessType = AT_ACCESS_REGULAR\n") ;
//		printf ("RELEASE PEND\n" ) ;
		SendATCMsg ( MSG_ATC_LONG_CMD, 0 )  ;
		return AT_STATUS_PENDING ;

	case AT_ACCESS_READ:
		printf ( "	   AccessType = AT_ACCESS_READ\n") ;
		break;

	case AT_ACCESS_TEST:
		printf ( "	   AccessType = AT_ACCESS_TEST\n") ;
		break;

	default:
		printf ( "	   AccessType = UNKNOWN\n") ;
		break;
	}

	AT_CmdRspOK(ch) ;

	return AT_STATUS_DONE;
}

AT_Status_t ATCmd_AND_D_Handler ( AT_CmdId_t cmdId, UInt8 ch, UInt8 accessType, const UInt8* _P1 )
{
	printf ( "\n	==>ATCmd_AND_D_Handler (custom implementation)\n" ) ;

	switch ( accessType ) {

	case AT_ACCESS_REGULAR:
		printf ( "	   AccessType = AT_ACCESS_REGULAR\n") ;
//		printf ("RELEASE PEND\n" ) ;
		SendATCMsg ( MSG_ATC_LONG_CMD_ERROR, 0 )  ;
		return AT_STATUS_PENDING ;

	case AT_ACCESS_READ:
		printf ( "	   AccessType = AT_ACCESS_READ\n") ;
		break;

	case AT_ACCESS_TEST:
		printf ( "	   AccessType = AT_ACCESS_TEST\n") ;
		break;

	default:
		printf ( "	   AccessType = UNKNOWN\n") ;
		break;
	}

	AT_CmdRspOK(ch) ;

	return AT_STATUS_DONE;
}

AT_Status_t ATCmd_D_Handler ( AT_CmdId_t cmdId, UInt8 ch, UInt8 accessType, const UInt8* _P1 )
{
	CallParams_t callParams ;
	Boolean		 voiceSuffix ;

	if( RESULT_OK == AT_DialStrParse( _P1, &callParams, &voiceSuffix ) ) {

		switch( callParams.callType ) {

		case CALLTYPE_SPEECH:
			printf( "CALLTYPE_SPEECH\n" ) ;
			if( voiceSuffix ) {
				printf( "Voice suffix present\n" ) ;
			}
			else {
				printf( "Voice suffix not present\n" ) ;
			}
			printf( "Number: %s\n", callParams.params.cc.number ) ;

			break ;

		case CALLTYPE_SUPPSVC:
			printf( "CALLTYPE_SUPPSVC\n" ) ;
			printf( "Number: %s\n", callParams.params.ss.number ) ;
//			DispatchSsApiReq( &callParams.params.ss.ssApiReq ) ;
			{
				UInt8 testDialStr[ 100 ] ;
				DIALSTR_CvtCallParamsToDialStr( &callParams, testDialStr ) ;
				printf( "Dial string = %s\n" , testDialStr ) ;
			}

			break ;

		case CALLTYPE_MOUSSDSUPPSVC:
			printf( "CALLTYPE_MOUSSDSUPPSVC\n" ) ;
			printf( "Number: %s\n", callParams.params.ussd.number ) ;
			break ;

		case CALLTYPE_GPRS:
			printf( "CALLTYPE_GPRS\n" ) ;
			printf( "Context ID: %d\n", callParams.params.gprs.context_id ) ;
			break ;

		case CALLTYPE_PPP_LOOPBACK:
			printf( "CALLTYPE_PPP_LOOPBACK\n" ) ;
			break ;

		case CALLTYPE_UNKNOWN:
			printf( "CALLTYPE_UNKNOWN\n" ) ;
			break ;

		default:
			printf( "INTERNAL ERROR!\n" ) ;
			assert( 0 ) ;
		}

		AT_CmdRspOK(ch) ;
		return AT_STATUS_DONE;
	}
	else {
		AT_CmdRspSyntaxError(ch);
		return AT_STATUS_DONE ;
	}
}

#include "at_stubs.gen.c"

//-----------------------------------------------------------------------------
//	main
//-----------------------------------------------------------------------------
char	cmdBuf[1000];

void main ( void )
{
	INIT_LOG( "loggit.dat" ) ;
	AT_InitState ( ) ;

	V24_Init ( ) ;
	ATC_Init ( ) ;

	CreateThread (
		0,
		1024,
  		V24_Task,
  		0,
  		0,
  		0 ) ;

  	CreateThread (
  		0,
  		1024,
		ATC_Task,
		0,
		0,
		0 ) ;

	SetChannelConfig( 0, 1, 10, 13, 1, 1 ) ;

	while (gets ( cmdBuf ) != NULL) {
		if ( cmdBuf[0] != '#' ) {
			if ( cmdBuf[0] != '?' ) {
				printf ("cmd buf = %s\n", cmdBuf ) ;
				strcat( cmdBuf, "\r\n" ) ;
				V24_Exec ( cmdBuf ) ;
			}
		}
	}
}

Ticks_t TIMER_GetValue( void )
{
	return 0 ;
}

//-----------------------------------------------------------------
// IsATUnsolicitedEventEnabled():
//------------------------------------------------------------------
Boolean IsATUnsolicitedEventEnabled()
{
	return TRUE;
}

void STK_PostMsg( InterTaskMsg_t* msg )
{
	assert( 0 ) ;
}

InterTaskMsg_t*	AllocInterTaskMsgFromHeap(MsgType_t msgType, UInt16 dataLength)
{
	assert( 0 ) ;
	return  0 ;
}

void STK_HandleCall_ControlRsp( InterTaskMsg_t* pMsg,Boolean forSimOriginalSS)
{
	assert( 0 ) ;
}

void MN_SendBasicCallReq(			// Request MN to Send a Basic Call
	UInt8 *number,					// Phone number (ascii null-term. string)
	CLIRMode_t clir_mode,			// CLIR mode
	CUGInfo_t *cug_info				// CUG info, NULL= no CUG info
	)
{
	assert( 0 ) ;
}

void STK_HandleAlphaID(UInt8 clientID, StkCallControlResult_t res,
						StkCallControl_t oldTy,StkCallControl_t newTy,
						Boolean validId,UInt8 len,UInt8* alphaId)
{
	assert( 0 ) ;
}

void SATK_HandleStopCallSetupRetryTimerReq(SATK_ResultCode_t resultCode)
{
	assert( 0 ) ;
}

Boolean IsMNCC_InIdle( void )	//	not prototypes in header file
{
	printf( "Stub function IsMNCC_InIdle returns TRUE\n" ) ;
	return TRUE ;
}

void SYS_SendMsgToClient(UInt8 inClientID, InterTaskMsg_t* inMsg)
{
	assert( 0 ) ;
}

void MN_AbortSSProcedure(
	CallIndex_t index
	)
{
	assert( 0 ) ;
}

void MN_SendUSSDResponse(
	CallIndex_t	ci,
	Unicode_t	dcs,
	UInt8		len,
	UInt8		*response				// USSD string for facility
	)
{
	assert( 0 ) ;
}

void SATK_SendCcUssdReq( UInt8 clientID, UInt8 ussd_len,
						UInt8 *ussd_data, Boolean simtk_orig )
{
	assert( 0 ) ;
}

Boolean SIM_IsStkCallCtrEnabled(void)
{
	assert( 0 ) ;
	return 0 ;
}

extern void PostMsgToATCTask(InterTaskMsg_t* inMsg)
{
	assert( 0 ) ;
}

extern void SYS_BroadcastMsg(InterTaskMsg_t* inMsg)
{
	assert( 0 ) ;
}

void MN_SendUSSDRegistration(UInt8 *ussdStr)
{
	assert( 0 ) ;
}

void RRMPRIM_TestParamReq( void ) { assert( 0 ) ; }
void MN_ReportRxTestParam( void ) { assert( 0 ) ; }
void MN_ProcessSsCallReq( void ) { assert( 0 ) ; }
void RRMPRIM_SetTestParamInterval( void ) { assert( 0 ) ; }
void SATK_GetProCallState( void ) { assert( 0 ) ; }
void STK_ResetSimOriginateSSFlag( void ) { assert( 0 ) ; }
Boolean STK_IsSimOriginateSS( void ) { assert( 0 ) ; }

#define NULLFUNCTION( name, type ) type name( void ) { assert( 0 ) ; }

//MSClass_t PDP_GetMSClass( void ) { assert( 0 ) ; }
//void STK_DS_GetDataServiceMode( void ) { assert( 0 ) ; }

NULLFUNCTION( PDP_GetMSClass, MSClass_t )
NULLFUNCTION( STK_DS_GetDataServiceMode, void )
