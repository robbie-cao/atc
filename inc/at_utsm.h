//=============================================================================
//
// Description:		AT SMS types
//
//==============================================================================


#ifndef __AT_UTSM_H__
#define __AT_UTSM_H__

#ifdef __AT_UTSM_C__
#define defextern
#else
#define defextern extern
#endif


#include "types.h"
#include "ms.h"
#include "smsapi.h"
#include "at_api.h"
#include "ostimer.h"

//***************************
// Utilities
//***************************
#define MASKL1  0x01      /*          1  */
#define MASKL2  0x03      /*         11  */
#define MASKL3  0x07      /*        111  */
#define MASKL4  0x0f      /*       1111  */
#define MASKL5  0x1f      /*      11111  */
#define MASKL6  0x3f      /*     111111  */
#define MASKL7  0x7f      /*    1111111  */
#define MASKL8  0xff      /*   11111111  */

#define ATC_GET8_BITS(src,pos,mask)   (((src)>>pos) & (mask))
#define ATC_SET8_BITS(src,pos,mask)   (((src)&mask) << pos)

#define ATC_GET_RDIGIT(byte)      (byte & 0x0F)
#define ATC_GET_LDIGIT(byte)      ((byte >> 4) & 0x0F)

#define ATC_SET_RDIGIT(byte)      (byte & 0x0F)
#define ATC_SET_LDIGIT(byte)      ((byte & 0x0F) << 4)

#define ATC_SWAP(byte)      ( ATC_SET_LDIGIT(byte) | ATC_SET_RDIGIT(byte>>4) )

#ifndef _WIN32
#ifndef max
#define max(x,y)   ((x) > (y) ? (x) : (y))
#endif
#endif

//*************************
// Access to AI bit fields
//*************************
//----------
// Addresses
//----------
#define ATC_GET_TON(src)    ATC_GET8_BITS(src,4,MASKL3) // see REC. 4.08
#define ATC_SET_TON(src)    ATC_SET8_BITS(src,4,MASKL3) // see REC. 4.08
#define ATC_GET_NPI(src)    ATC_GET8_BITS(src,0,MASKL4) // see REC. 4.08
#define ATC_SET_NPI(src)    ATC_SET8_BITS(src,0,MASKL4) // see REC. 4.08

//-----
// <fo>
//-----
#define ATC_GET_MTI(fo)  ATC_GET8_BITS(fo,0,MASKL2) //Message type ind  BI-DIR
#define ATC_GET_RD(fo)   ATC_GET8_BITS(fo,2,MASKL1) //Reject duplicate  MO
#define ATC_GET_VPF(fo)  ATC_GET8_BITS(fo,3,MASKL2) //VP format         MO
#define ATC_GET_SRR(fo)  ATC_GET8_BITS(fo,5,MASKL1) //Status report req MO
#define ATC_GET_SRQ(fo)  ATC_GET8_BITS(fo,5,MASKL1) //SRQ
#define ATC_GET_UDHI(fo) ATC_GET8_BITS(fo,6,MASKL1) //User data hdr ind BI-DIR
#define ATC_GET_RP(fo)   ATC_GET8_BITS(fo,7,MASKL1) //Reply path        BI-DIR

#define ATC_SET_MTI(val)    ATC_SET8_BITS(val,0,MASKL2) //Message type ind
#define ATC_SET_RD(val)     ATC_SET8_BITS(val,2,MASKL1) //Reject duplicate
#define ATC_SET_MMS(val)    ATC_SET8_BITS(val,2,MASKL1) //More msg to send
#define ATC_SET_VPF(val)    ATC_SET8_BITS(val,3,MASKL2) //VP format
#define ATC_SET_SRR(val)    ATC_SET8_BITS(val,5,MASKL1) //SRR
#define ATC_SET_SRQ(val)    ATC_SET8_BITS(val,5,MASKL1) //SRQ
#define ATC_SET_SRI(val)    ATC_SET8_BITS(val,5,MASKL1) //SRI
#define ATC_SET_UDHI(val)   ATC_SET8_BITS(val,6,MASKL1) //User data hdr ind
#define ATC_SET_RP(val)     ATC_SET8_BITS(val,7,MASKL1) //Reply path


//------
// <dcs>
//------
#define ATC_GET_CODING_GRP(dcs) ATC_GET8_BITS(dcs, 4, MASKL4) // coding group

//------
// <pi>
//------
#define ATC_PI_PID   1
#define ATC_PI_DCS   2
#define ATC_PI_UDL   4

#define NB_SMS_PROF 1


//********************************************
// ATC 3.40 <-> TE decoding/encoding utilities
//********************************************
/*
** MTI supported values
*/
// 3.40 values
#define DELIVER_340        0
#define SUBMIT_REPORT_340  1
#define STATUS_REPORT_340  2
#define DELIVER_REPORT_340 0
#define SUBMIT_340         1
#define COMMAND_340        2

// ATC internal values
typedef enum{
   ATC_SMS_DELIVER,
   ATC_SMS_SUBMIT_REPORT,
   ATC_SMS_STATUS_REPORT,
   ATC_SMS_DELIVER_REPORT,
   ATC_SMS_COMMAND,
   ATC_SMS_SUBMIT
} atc_SmsMti_t;

/*
** VPF supported values
*/
typedef enum {
   ATC_VPF_NO_VP    ,
   ATC_VPF_ENHANCED ,
   ATC_VPF_RELATIVE ,
   ATC_VPF_ABSOLUTE
} atc_Vpf_t ;


/*
** Address formats
*/
#define ATC_UNKNOWN_TOA 0

typedef struct {
   UInt8 Len;
   UInt8 Toa;
   UInt8 Val[SMS_MAX_DIGITS / 2];
} atc_411Addr_t ;


/*
** Dcs
*/

typedef enum{ // Beware, must equal msg classes as specified in 3.38
              // for coding groups 00xxB and 1111B
  MSG_CLASS0,
  MSG_CLASS1,
  MSG_CLASS2,
  MSG_CLASS3,
  MSG_NO_CLASS = 0xFF
} atc_SmsMsgClass_t;

typedef enum {
   ATC_ALPHABET_DEFAULT,
   ATC_ALPHABET_8BIT,
   ATC_ALPHABET_UCS2,
   ATC_ALPHABET_RESERVED
} atc_SmsAlphabet_t;   // Beware, enum must follow 3.38 values

typedef enum
{
 ATC_LANG_GERMAN = 0,
 ATC_LANG_ENGLISH,
 ATC_LANG_ITALIAN,
 ATC_LANG_FRENCH,
 ATC_LANG_SPANISH,
 ATC_LANG_DUTCH,
 ATC_LANG_SWEDISH,
 ATC_LANG_DANISH,
 ATC_LANG_PORTUGUESE,
 ATC_LANG_FINNISH,
 ATC_LANG_NORWEGIAN,
 ATC_LANG_GREEK,
 ATC_LANG_TURKISH,
 ATC_LANG_HUNGARIAN,
 ATC_LANG_POLISH,
 ATC_LANG_UNSPECIFIED,
 ATC_LANG_CZECH,                           // Phase 2+
 ATC_LANG_EUROP,
 ATC_LANG_CODED
} atc_SmsLanguage_t;  // Beware, enum must follow 3.38 values

// ActionRpAck_t is combined with atc_BufferedURsp_t.
typedef enum
{
	WAIT_CNMA     = 0x80,
	SEND_RP_ACK   = 0x90,
	SEND_RP_ERROR = 0xA0
} ActionRpAck_t;

typedef enum
{
	ATC_SMSPP_IND_TYPE,
	ATC_SMSCB_IND_TYPE,
	ATC_SR_IND_TYPE,
	ATC_GENERICAL_TYPE
} atc_BufferedURsp_t;

//*********************************************************
// SMS task <-> ATC decoding/encoding utilities
// Built from what we guess the SMS task interface provides
//*********************************************************


//==================================================

//**************
// Const tables
//**************
#define ATC_NB_PCCP  1 // Only PCCP437 is supported by ME
typedef enum {
 PCCP437_IDX,
 ISO1_IDX,
 ISO2_IDX,
 ISO3_IDX,
 ISO4_IDX,
 ISO5_IDX,
 ISO6_IDX,
 NO_CONV_IDX = 0xff
} atc_PccpIdx_t;

#define IRA_IDX PCCP437_IDX

/*
** <state> pdu mode values
*/
#define    STAT_REC_UNREAD 0
#define    STAT_REC_READ   1
#define    STAT_STO_UNSENT 2
#define    STAT_STO_SENT   3
#define    STAT_ALL        4

typedef struct
{
	UInt8	num1;
	UInt8	num2;
	UInt8	num3;
	UInt8	num4;
} msgNumInBox_t;


#ifdef __AT_UTSM_C__

/*
** Tables used to convert internal SIMSMSMesgStatus_t SMS status
** to AT 07.05 strings
** Be carefull, indexed by SIMSMSMesgStatus_t values
*/
const char *atc_SmsStateText [9] =
{
 "",
 "REC READ",
 "",
 "REC UNREAD",
 "",
 "STO SENT",
 "",
 "STO UNSENT",
 "ALL"
}; // text mode

const UInt8 atc_SmsStatePdu  [9] =
{
 0xFF,
 STAT_REC_READ,
 0xFF,
 STAT_REC_UNREAD,
 0xFF,
 STAT_STO_SENT,
 0xFF,
 STAT_STO_UNSENT,
 STAT_ALL
}; // pdu mode
#else
extern const char  *atc_SmsStateText [9];
extern const UInt8 atc_SmsStatePdu   [9];
#endif


//***********
// Prototypes
//***********
UInt8  atc_8To7bit          (UInt8 *Str, UInt8 StrLen);
void   atc_7To8bit          (UInt8 *Dst, UInt8 *Src, UInt8 SrcLen);
void   AT_FormatUdForTe     (UInt8 *Out, UInt8* ud, UInt8 udl, SmsDcs_t* dcs, Boolean udhi, UInt8 udh1);

UInt16 AT_OutMsg2Te		  (UInt8* Out, SmsSimMsg_t *In, AT_CmdId_t cmdId);

void atc_UpdateSmsSlotStatus(SmsStorage_t storage, UInt16 RecNum, SIMSMSMesgStatus_t SmsState);
SIMSMSMesgStatus_t atc_GetSmsSlotStatus(SmsStorage_t storage, UInt16 RecNum);

UInt8  *atc_RmvBackSpace (UInt8 *Str);
void   atc_PduToHex (UInt8 *HexOut, UInt8 *PduIn, UInt16 Size);
UInt16 atc_HexToPdu (UInt8 *PduOut, UInt8 *HexIn, UInt16 Size);


/**************************** SMS CB utilities *******************************/
typedef struct {
   UInt8              DcsRaw;         // raw value
   smsCodingGrp_t	  CodingGrp;
   Boolean            Compression;
   atc_SmsLanguage_t  Language;
   atc_SmsMsgClass_t  MsgClass;
   atc_SmsAlphabet_t  MsgAlphabet;
}
atc_SmsCbDcs_t;

typedef struct{
   atc_SmsCbDcs_t     Dcs;
   UInt8              DataLen;     // Ud length in bytes (UDL byte not included)
   UInt8              UdNbChar;    // Ud number of char
   UInt8             *Ud;          // points to SMS User Data in 3.40 format
} atc_SmsCbPage_t;

typedef struct
{
 UInt16 Serial;
 UInt16 MsgId;
 UInt8  Dcs;
 UInt8  NbPages;
 UInt8  NoPage;
 UInt8  Msg [CB_DATA_PER_PAGE_SZ];
} StoredSmsCb_t;


UInt16  atc_DecCbDcs        (atc_SmsCbDcs_t *Dcs) ;
UInt8  *atc_SmsCbGetPage    (UInt8 Page, T_SMS_CB_MSG *CbMsg);
UInt16 atc_DecCbPage        (atc_SmsCbPage_t * Out, StoredSmsCb_t * In);
void   atc_FormatCbPageForTe(UInt8 * Out, atc_SmsCbPage_t * In);
//void   atc_ReadCbPageCb     (SIMAccess_t  result,
//                             UInt16       rec_no,
//                             SIMSMSMesg_t *sms_mesg);

UInt16 atc_DisplaySmsCbPage (UInt8 chan, char * CurAtCmd, UInt8 * Out,
                             StoredSmsCb_t * In, SIMSMSMesgStatus_t Status) ;

extern UInt8 atc_SmsWaitedCnma;
extern UInt8 atc_NextCmdIsCnma;
extern UInt8 atc_SendRpAck;

void atc_ResetCnmaDataBase (void);
void atc_CnmaTimerEvent ( TimerID_t Id );
void atc_SetCnmaTimer (UInt8 TpMti);
Boolean atc_GetCnmaTimer (void);
void atc_ResetCnmaTimerFiFo (void);

void	atc_SetCnmaTpMti (UInt8 TpMti);
UInt8	atc_GetCnmaTpMti (void);
void	atc_ResetCnmaTpMtiFiFo (void);


// New function protype
void AtHandleSmsDeliverInd(InterTaskMsg_t* inMsg);
void AtHandleIncomingSmsStoredRes(InterTaskMsg_t* inMsg);
void AtHandleWriteSmsStoredRes(InterTaskMsg_t* inMsg);
void AtHandleSmsStatusRptStoredRes(InterTaskMsg_t* inMsg);
void AtHandleSmsSubmitRsp(InterTaskMsg_t* inMsg);
void AtHandleSmsUsrDataInd(InterTaskMsg_t* inMsg);
void AtHandleSmsReportInd(InterTaskMsg_t* inMsg);
void AtHandleSmsStatusReportInd(InterTaskMsg_t* inMsg);
void AtHandleCBStartStopRsp(InterTaskMsg_t* inMsg);
void AtHandleCBDataInd(InterTaskMsg_t* inMsg);
void AtHandleSMSCBStoredRsp(InterTaskMsg_t* inMsg);
void AtHandleSmsDeleteSimRsp(InterTaskMsg_t* inMsg);
void AtHandleSmsReadResp(InterTaskMsg_t *inMsg);  // temp for CMGR
void AtHandleVMWaitingInd(InterTaskMsg_t *inMsg);
void AtHandleSmsCnmaTimerEvent(InterTaskMsg_t* inMsg);
void AtHandleVmscUpdateRsp(InterTaskMsg_t* inMsg);

void ATHandleComposeCmd(InterTaskMsg_t *inMsg);

Boolean GetNumMsgsForEachBox(SmsStorage_t storageType, msgNumInBox_t* msgNumInBox);

#endif  /* __AT_UTSM_H__ */

