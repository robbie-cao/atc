#ifndef _AT_CFG_H_
#define _AT_CFG_H_

#include "types.h"
#include "phonebk.h"	//	for PBK_Id_t
#include "dlc.h"		//	for DLC_t
#include "at_data.h"	//  for ATDLCCOUN
#include "at_utsm.h"	//	for atc_SmsProfile_t
#include "serialapi.h"	//	for serial device

typedef struct
{
  /* One byte elements or element arrays, including enum type */
  UInt8  AmpC;     // 0 -> DCE always present ON on circuit 109
  UInt8  AmpD;     // Circuit 108/2 behaviour
  UInt8  CR;       // 0 -> no intermed results, 1 -> intermed results
  UInt8  CRC;      // 0 -> disable extended code on ring, 1 -> enable
  UInt8  CREG;     // 0 -> disable unsolicited +CREG, 1 -> enable
  UInt8  CGREG;    // 0 -> disable GPRS unsolicited +CGREG, 1 -> enable
  UInt8  MTREPORT; // 1:Enable 0:Disable - Measurement Report
  UInt8  E ;       // 0 -> echo off, 1 -> echo characters

  UInt8  O ;       // Online cmd mode to data mode
  UInt8  X ;       // see spec.
  UInt8  P ;       // Dummy for GSM, set to 0
  UInt8  Q ;       // 0 -> results, 1 -> quiet
  UInt8  S0;       // Nb rings before auto answer 0 -> disable
  UInt8  S3;       // Character used for <cr>
  UInt8  S4;       // Character used for <lf>
  UInt8  S5;       // Character used for <bs>
  UInt8  S6;       // Pause duration before blind dialling
  UInt8  S7;       // Wait time for call completion
  UInt8  S8;       // Pause duration in secs when dialling (when , is detected)
  UInt8  S10;      // Automatic Disconnect Delay Control ( Dummy)
  UInt8  V ;       // 0 -> text cr lf or code cr 1 -> cr lf text cr lf
  UInt8  CMEE;     // 0 disable +CME ERROR, 1 enable numeric, 2 enable txt
  UInt8  CSTA;     // 145 if + used in dialling, 129 otherwise
  UInt8  ILRR;     // 0 disable rate report 1 enable
  UInt8  IFC[2];   // DCE and DTE flow control
  UInt8  ICF[2];   // Character Framing
  UInt8	 RLP_Ver;  // The RLP version number currently is used
  UInt8  CVHU;     // DTR and ATH behaviour
  UInt8  CMGF;     // 0 -> PDU, 1 -> text mode
  UInt8  CPMS[3];  // SMS storages

  UInt8  CPOL;	   // format for CPOL

  UInt8  ER;	   // Intermediate result code pref. for ES
  UInt8  DR;	   // Intermediate result code pref. for DS
  UInt8  ETBM[3];  // Buffer management params
  UInt8  CMER[5];  // Mobile Equipment event reporting
// Moved to \system  UInt8  CGEREP[2];// GPRS Event Reporting
  UInt8  CHSR;	    // HSCSD parameters report

  UInt8  WS46;
  UInt8  ECIPC ;

  PBK_Id_t	CPBS;     // Phone book storage selected
  Boolean	CUSD;

  Boolean	AllowUnsolOutput; //Allow unsolicated report in this channel

} AT_ChannelCfg_t;


typedef struct
{
	Boolean logOn;
	Boolean presetAllowTrace;

	Boolean Unsoliciated_CCMReport;	// Activate unsoliciated CCM reporting
	Boolean CallMeterWarning; 		// enable or disable call meter warning event

	/* Two byte Elements or arrays of UInt16, Int16 type */
	Boolean CSMS;     // Selected message service : 0 or 1.
	Boolean CSDH;     // 0 -> do not show header, 1 -> show
	Boolean MVMIND;

	/* Bit Field elements */
	BitField ESIMC: 1;		// 1: Send SIM Detection message; 0: Don't send.

	/* Four byte elements */
	UInt32 IPR ;		// 4800,9600,19200,38400,57600,115200

	Boolean MCSQ;		// enable/disable unsolicited rssi, ber reporting

	//+CSSN Supplementary Service Notification (07.07, section 7.16)
	UInt8  CSSI;	// 1, show +CSSI result code status in the TA, 0 - disable
	UInt8  CSSU;	// 1, show +CSSU result code status in the TA, 0 - disable

	UInt8  VTD;			// DTMF duration used by +VTS
	Boolean	MRINFO;				//Radio Access Information

} AT_CommonCfg_t;

#define SET_MVMIND(x)			(AT_GetCommonCfg()->MVMIND = x)
#define GET_MVMIND				(AT_GetCommonCfg()->MVMIND)

#define SET_CSDH(x)				(AT_GetCommonCfg()->CSDH = x)
#define GET_CSDH				(AT_GetCommonCfg()->CSDH)

#define SET_CSMS(x)				(AT_GetCommonCfg()->CSMS = (x & 1))
#define GET_CSMS				(AT_GetCommonCfg()->CSMS & 1)

#define SET_CMGF(ch,x)			(AT_GetChannelCfg((ch))->CMGF = (x & 1))
#define GET_CMGF(ch)			(AT_GetChannelCfg((ch))->CMGF)


extern AT_ChannelCfg_t* AT_GetChannelCfg( UInt8 ch ) ;	// todo tbd fixme gdheinz - dlci or chan?
extern AT_CommonCfg_t*  AT_GetCommonCfg( void ) ;
extern void AT_SetDefaultParms( Boolean init_flag ) ;

extern Result_t ATC_ConfigSerialDevForATC(SerialDeviceID_t  inDevID);
extern PortId_t ATC_GetCurrentSIOPort(void);


#endif // _AT_CFG_H_

