/**
*
*   @file   at_fax.c
*
*   @brief  This file contains all fax related functions.
*
****************************************************************************/

#include <stdlib.h>
#include "at_data.h"
#include "string.h"
#include "fax.h"
#include "at_call.h"
#include "at_api.h"


extern CallConfig_t	curCallCfg;


//******************************************************************************
//
// Function Name:	ATC_FAA_CB
//
// Description:		Transition on AT+FAA
//
// Notes:			Adaptive Answer
//
//******************************************************************************

AT_Status_t ATCmd_FAA_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{

	AT_Status_t atStatus = AT_STATUS_DONE;

	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.faa = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FAA_PARAM, curCallCfg.faxParam.faa);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->faa.current_value );
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FAP_CB
//
// Description:		Transition on AT+FAP
//
// Notes:			Address and Polling
//
//******************************************************************************

AT_Status_t ATCmd_FAP_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1,
								const		UInt8* _P2,
								const		UInt8* _P3)
{

	AT_Status_t atStatus = AT_STATUS_DONE;

	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.sub = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(SUB_PARAM, curCallCfg.faxParam.sub);
		curCallCfg.faxParam.sep = _P2 == NULL ? 0 : atoi((char*)_P2);
		FAX_UpdateSimpleFaxParam(SEP_PARAM, curCallCfg.faxParam.sep);
		curCallCfg.faxParam.pwd = _P3 == NULL ? 0 : atoi((char*)_P3);
		FAX_UpdateSimpleFaxParam(PWD_PARAM, curCallCfg.faxParam.pwd);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d,%d,%d",
						fax_param->sub.current_value,
						fax_param->sep.current_value,
						fax_param->pwd.current_value);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FBO_CB
//
// Description:		Transition on AT+FBO
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FBO_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fbo = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FBO_PARAM, curCallCfg.faxParam.fbo);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->fbo.current_value );
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FBU_CB
//
// Description:		Transition on AT+FBU
//
// Notes:			HDLC Frame Reporting
//
//******************************************************************************

AT_Status_t ATCmd_FBU_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fbu = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FBU_PARAM, curCallCfg.faxParam.fbu);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->fbu.current_value );
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FCQ_CB
//
// Description:		Transition on AT+FCQ
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FCQ_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1,
								const		UInt8* _P2)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.crq = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(CRQ_PARAM, curCallCfg.faxParam.crq);
		curCallCfg.faxParam.ctq = _P2 == NULL ? 0 : atoi((char*)_P2);
		FAX_UpdateSimpleFaxParam(CTQ_PARAM, curCallCfg.faxParam.ctq);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d,%d",
			fax_param->crq.current_value,
			fax_param->ctq.current_value );
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FCC_CB
//
// Description:		Transition on AT+FCC
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FCC_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1,
								const		UInt8* _P2,
								const		UInt8* _P3,
								const		UInt8* _P4,
								const		UInt8* _P5,
								const		UInt8* _P6,
								const		UInt8* _P7,
								const		UInt8* _P8)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.vr = _P1 == NULL ? 0 : atoi((char*)_P1);
		curCallCfg.faxParam.vr_current = curCallCfg.faxParam.vr;
		curCallCfg.faxParam.br = _P2 == NULL ? 3 : atoi((char*)_P2);
		curCallCfg.faxParam.br_current = curCallCfg.faxParam.br;
		curCallCfg.faxParam.wd = _P3 == NULL ? 0 : atoi((char*)_P3);
		curCallCfg.faxParam.wd_current = curCallCfg.faxParam.wd;
		curCallCfg.faxParam.ln = _P4 == NULL ? 0 : atoi((char*)_P4);
		curCallCfg.faxParam.ln_current = curCallCfg.faxParam.ln;
		curCallCfg.faxParam.df = _P5 == NULL ? 0 : atoi((char*)_P5);
		curCallCfg.faxParam.df_current = curCallCfg.faxParam.df;
		curCallCfg.faxParam.ec = _P6 == NULL ? 0 : atoi((char*)_P6);
		curCallCfg.faxParam.ec_current = curCallCfg.faxParam.ec;
		curCallCfg.faxParam.bf = _P7 == NULL ? 0 : atoi((char*)_P7);
		curCallCfg.faxParam.bf_current = curCallCfg.faxParam.bf;
		curCallCfg.faxParam.st = _P8 == NULL ? 0 : atoi((char*)_P8);
		curCallCfg.faxParam.st_current = curCallCfg.faxParam.st;
		FAX_UpdateFCCFaxParam(	curCallCfg.faxParam.vr,
								curCallCfg.faxParam.br,
								curCallCfg.faxParam.wd,
								curCallCfg.faxParam.ln,
								curCallCfg.faxParam.df,
								curCallCfg.faxParam.ec,
								curCallCfg.faxParam.bf,
								curCallCfg.faxParam.st);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d,%d,%d,%d,%d,%d,%d,%d",
				 fax_param->vr_current,
				 fax_param->br_current,
				 fax_param->wd_current,
				 fax_param->ln_current,
				 fax_param->df_current,
				 fax_param->ec_current,
				 fax_param->bf_current,
				 fax_param->st_current);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FCS_CB
//
// Description:		Transition on AT+FCS
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FCS_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType)
{
	UInt8 atRes[MAX_RESPONSE_LEN+1];

	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		AT_CmdRspError(chan, AT_ERROR_RSP );
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d,%d,%d,%d,%d,%d,%d,%d",
				 fax_param->vr_current,
				 fax_param->br_current,
				 fax_param->wd_current,
				 fax_param->ln_current,
				 fax_param->df_current,
				 fax_param->ec_current,
				 fax_param->bf_current,
				 fax_param->st_current);
		AT_OutputStr(chan, atRes );
		AT_CmdRspOK( chan );
	}

	return AT_STATUS_DONE;
}


//******************************************************************************
//
// Function Name:	ATC_FDR_CB
//
// Description:		Transition on AT+FDR
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FDR_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	FAX_Receive_FDR();

 	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FDT_CB
//
// Description:		Transition on AT+FDT
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FDT_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1,
								const		UInt8* _P2,
								const		UInt8* _P3,
								const		UInt8* _P4)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	FAX_Send_FDT();

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FEA_CB
//
// Description:		Transition on AT+FEA
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FEA_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fea = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FEA_PARAM, curCallCfg.faxParam.fea);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->fea.current_value );
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FET_CB
//
// Description:		Transition on AT+FET
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FET_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1,
								const		UInt8* _P2,
								const		UInt8* _P3,
								const		UInt8* _P4){

	AT_Status_t atStatus = AT_STATUS_DONE;
	FAX_Send_FET();

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FIE_CB
//
// Description:		Transition on AT+FIE
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FIE_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fie = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FIE_PARAM, curCallCfg.faxParam.fie);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->fie.current_value);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FFC_CB
//
// Description:		Transition on AT+FFC
//
// Notes:			Format Conversion
//
//******************************************************************************

AT_Status_t ATCmd_FFC_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1,
								const		UInt8* _P2,
								const		UInt8* _P3,
								const		UInt8* _P4)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.vrc = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(VRC_PARAM, curCallCfg.faxParam.vrc);
		curCallCfg.faxParam.dfc = _P2 == NULL ? 0 : atoi((char*)_P2);
		FAX_UpdateSimpleFaxParam(DFC_PARAM, curCallCfg.faxParam.dfc);
		curCallCfg.faxParam.lnc = _P3 == NULL ? 0 : atoi((char*)_P3);
		FAX_UpdateSimpleFaxParam(LNC_PARAM, curCallCfg.faxParam.lnc);
		curCallCfg.faxParam.wdc = _P4 == NULL ? 0 : atoi((char*)_P4);
		FAX_UpdateSimpleFaxParam(WDC_PARAM, curCallCfg.faxParam.wdc);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d,%d,%d,%d",
					fax_param->vrc.current_value,
					fax_param->dfc.current_value,
					fax_param->lnc.current_value,
					fax_param->wdc.current_value);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FIP_CB
//
// Description:		Transition on AT+FIP
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FIP_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fip = _P1 == NULL ? 0 : atoi((char*)_P1);
		CC_InitFaxConfig(&curCallCfg.faxParam);
		FAX_InitializeFaxParam();
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",curCallCfg.faxParam.fip );
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FIS_CB
//
// Description:		Transition on AT+FIS
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FIS_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1,
								const		UInt8* _P2,
								const		UInt8* _P3,
								const		UInt8* _P4,
								const		UInt8* _P5,
								const		UInt8* _P6,
								const		UInt8* _P7,
								const		UInt8* _P8)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.vr = _P1 == NULL ? 0 : atoi((char*)_P1);
		curCallCfg.faxParam.br = _P2 == NULL ? 3 : atoi((char*)_P2);
		curCallCfg.faxParam.wd = _P3 == NULL ? 0 : atoi((char*)_P3);
		curCallCfg.faxParam.ln = _P4 == NULL ? 0 : atoi((char*)_P4);
		curCallCfg.faxParam.df = _P5 == NULL ? 0 : atoi((char*)_P5);
		curCallCfg.faxParam.ec = _P6 == NULL ? 0 : atoi((char*)_P6);
		curCallCfg.faxParam.bf = _P7 == NULL ? 0 : atoi((char*)_P7);
		curCallCfg.faxParam.st = _P8 == NULL ? 0 : atoi((char*)_P8);
		FAX_UpdateFISFaxParam(	curCallCfg.faxParam.vr,
								curCallCfg.faxParam.br,
								curCallCfg.faxParam.wd,
								curCallCfg.faxParam.ln,
								curCallCfg.faxParam.df,
								curCallCfg.faxParam.ec,
								curCallCfg.faxParam.bf,
								curCallCfg.faxParam.st);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d,%d,%d,%d,%d,%d,%d,%d",
				 fax_param->vr.current_value,
				 fax_param->br.current_value,
				 fax_param->wd.current_value,
				 fax_param->ln.current_value,
				 fax_param->df.current_value,
				 fax_param->ec.current_value,
				 fax_param->bf.current_value,
				 fax_param->st.current_value);

		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}



//******************************************************************************
//
// Function Name:	ATC_FIT_CB
//
// Description:		Transition on AT+FIT
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FIT_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1,
								const		UInt8* _P2)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.it_time = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(IT_TIME_PARAM, curCallCfg.faxParam.it_time);
		curCallCfg.faxParam.it_action = _P2 == NULL ? 0 : atoi((char*)_P2);
		FAX_UpdateSimpleFaxParam(IT_ACTION_PARAM, curCallCfg.faxParam.it_action);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d,%d",
				 fax_param->it_time.current_value,
				 fax_param->it_action.current_value);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FKS_CB
//
// Description:		Transition on AT+FKS
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FKS_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{

	AT_Status_t atStatus = AT_STATUS_DONE;
	FAX_Send_FKS();
	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FLI_CB
//
// Description:		Transition on AT+FLI
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FLI_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
 	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		strcpy((char*)curCallCfg.faxParam.fli, (char*)_P1);
		FAX_UpdateFaxStringParam(FLI_PARAM, (UInt8*)curCallCfg.faxParam.fli);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%s",(char*)fax_param->fli);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FPI_CB
//
// Description:		Transition on AT+FPI
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FPI_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		strcpy((char*)curCallCfg.faxParam.fpi, (char*)_P1);
		FAX_UpdateFaxStringParam(FPI_PARAM, (UInt8*)curCallCfg.faxParam.fpi);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%s",(char*)fax_param->fpi);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FLP_CB
//
// Description:		Transition on AT+FLP
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FLP_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.flp = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FLP_PARAM, curCallCfg.faxParam.flp);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->flp.current_value);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FMS_CB
//
// Description:		Transition on AT+FMS
//
// Notes:			Minimum Phase C Speed
//
//******************************************************************************

AT_Status_t ATCmd_FMS_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fms = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FMS_PARAM, curCallCfg.faxParam.fms);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->fms.current_value);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FNR_CB
//
// Description:		Transition on AT+FNR
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FNR_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1,
								const		UInt8* _P2,
								const		UInt8* _P3,
								const		UInt8* _P4)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.rpr = _P1 == NULL ? 1 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(RPR_PARAM, curCallCfg.faxParam.rpr);
		curCallCfg.faxParam.tpr = _P2 == NULL ? 1 : atoi((char*)_P2);
		FAX_UpdateSimpleFaxParam(TPR_PARAM, curCallCfg.faxParam.tpr);
		curCallCfg.faxParam.idr = _P3 == NULL ? 0 : atoi((char*)_P3);
		FAX_UpdateSimpleFaxParam(IDR_PARAM, curCallCfg.faxParam.idr);
		curCallCfg.faxParam.nsr = _P4 == NULL ? 0 : atoi((char*)_P4);
		FAX_UpdateSimpleFaxParam(NSR_PARAM, curCallCfg.faxParam.nsr);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d,%d,%d,%d",
			fax_param->rpr.current_value,
			fax_param->tpr.current_value,
			fax_param->idr.current_value,
			fax_param->nsr.current_value);

		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FPP_CB
//
// Description:		Transition on AT+FPP
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FPP_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fpp = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FPP_PARAM, curCallCfg.faxParam.fpp);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->fpp.current_value );
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FPS_CB
//
// Description:		Transition on AT+FPS
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FPS_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fps = _P1 == NULL ? 1 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FPS_PARAM, curCallCfg.faxParam.fps);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->fps.current_value );
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}

//******************************************************************************
//
// Function Name:	ATC_FRQ_CB
//
// Description:		Transition on AT+FRQ
//
// Notes:			Receive Quality Thresholds
//
//******************************************************************************

AT_Status_t ATCmd_FRQ_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1,
								const		UInt8* _P2)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.pgl = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(PGL_PARAM, curCallCfg.faxParam.pgl);
		curCallCfg.faxParam.cbl = _P2 == NULL ? 0 : atoi((char*)_P2);
		FAX_UpdateSimpleFaxParam(CBL_PARAM, curCallCfg.faxParam.cbl);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d,%d",fax_param->pgl.current_value,
					fax_param->cbl.current_value);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FRY_CB
//
// Description:		Transition on AT+FRY
//
// Notes:			ECM Retry Count
//
//******************************************************************************

AT_Status_t ATCmd_FRY_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fry = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FRY_PARAM, curCallCfg.faxParam.fry);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->fry.current_value);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FSP_CB
//
// Description:		Transition on AT+FSP
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FSP_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fsp = _P1 == NULL ? 0 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FSP_PARAM, curCallCfg.faxParam.fsp);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->fsp.current_value);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}


//******************************************************************************
//
// Function Name:	ATC_FPO_CB
//
// Description:		Transition on AT+FPO
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FPO_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;

	AT_CmdRspOK( chan );

	return atStatus;
}

//******************************************************************************
//
// Function Name:	ATC_FCR_CB
//
// Description:		Transition on AT+FCR
//
// Notes:
//
//******************************************************************************

AT_Status_t ATCmd_FCR_Handler(	AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)
{
	AT_Status_t atStatus = AT_STATUS_DONE;
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Stack_Fax_Param_t*  fax_param = FAX_GetRefreshFaxParam();

	if ( accessType == AT_ACCESS_REGULAR )
	{
		curCallCfg.faxParam.fcr = _P1 == NULL ? 1 : atoi((char*)_P1);
		FAX_UpdateSimpleFaxParam(FCR_PARAM, curCallCfg.faxParam.fcr);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		sprintf( (char*)atRes, "%d",fax_param->fcr.current_value);
		AT_OutputStr(chan, atRes );
	}

	AT_CmdRspOK( chan );

	return atStatus;
}

//******************************************************************************
//
// Function Name:	ATC_Fclass_CB
//
// Description:		Transition on AT+FCLASS
//
// Notes:
//
//******************************************************************************
AT_Status_t ATCmd_FCLASS_Handler(AT_CmdId_t	cmdId,
								UInt8		chan,
								UInt8		accessType,
								const		UInt8* _P1)

{
	UInt8 atRes[MAX_RESPONSE_LEN+1];
	Result_t result = RESULT_OK ;

	if(accessType == AT_ACCESS_TEST)
	{

/* Fix for TR 1204 - GC8X card should not respond with fax class 2 and 2.0
** when queried by WinFax application. Setting FCLASS to 2/2.0 will continue to be
** possible since FTA TC 11.1.2 requires setting FCLASS to 2.
*/

#if defined(GCXX_PC_CARD)
		{
			sprintf( (char*)atRes, "%s","0");
		}
#else
		{
			sprintf( (char*)atRes, "%s","0,2,2.0");
		}
#endif
		{
			AT_OutputStr( chan, (UInt8*)atRes ) ;
			AT_CmdRspOK(chan);
			return AT_STATUS_DONE;
		}
	}else if ( accessType == AT_ACCESS_REGULAR )
	{
		if ( strcmp((char*)_P1, "0") == 0 ) {
			curCallCfg.faxParam.fclass = AT_FCLASSSERVICEMODE_DATA;
		} else if ( strcmp((char*)_P1, "2") == 0 ) {
			curCallCfg.faxParam.fclass = AT_FCLASSSSERVICEMODE_FAX_2;
		} else if ( strcmp((char*)_P1, "2.0") == 0 ) {
			curCallCfg.faxParam.fclass = AT_FCLASSSSERVICEMODE_FAX_2dot0;
		} else {
			result = RESULT_ERROR;
		}

		if ( curCallCfg.faxParam.fclass != 0){

			V24_SetOperationMode((DLC_t)chan,V24OPERMODE_FAX_CMD);
		} else {

			V24_SetOperationMode((DLC_t)chan,V24OPERMODE_AT_CMD);
		}

		FAX_UpdateSimpleFaxParam(FCLASS_PARAM, curCallCfg.faxParam.fclass);
	}
	else if ( accessType == AT_ACCESS_READ )
	{
		/* Note: ITU-T T.32 says we do not return "FCLASS: " in the response */
        switch (curCallCfg.faxParam.fclass)
		{
			case AT_FCLASSSERVICEMODE_DATA:
				AT_OutputStr(chan, (UInt8*)"0");
				break;
			case AT_FCLASSSSERVICEMODE_FAX_2:
				AT_OutputStr(chan, (UInt8*)"2");
				break;
			case AT_FCLASSSSERVICEMODE_FAX_2dot0:
				AT_OutputStr(chan, (UInt8*)"2.0");
				break;
			default:
				break;
		}
	}


	if ( result == RESULT_ERROR)
		AT_CmdRspError(chan,AT_ERROR_RSP);
	else
		AT_CmdRspOK(chan);

	return AT_STATUS_DONE;
}


