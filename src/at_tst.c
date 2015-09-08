//******************************************************************************
// Description: This file ATC test related function implementation.
//
// $File: at_tst.c$
// $Revision:$
// $DateTime:$
//
//******************************************************************************
#define	ENABLE_LOGGING

#include <stdio.h>
#include "stdlib.h"
#include "at_cfg.h"
#include "at_tst.h"
#include "at_util.h"
#include "at_state.h"
#include "mstruct.h"
#include "diagnost.h"

#define MAX_RESPONSE_LEN 100
//******************************************************************************
// Function Name:	ATCmd_MeasReport_Handler
//
// Description:	Called by AT parser for measurement reporting.
//
// Argument:	cmdId;	Command Identifier
//				chan;	The channel
//				accessType; Type of access
//				_P1;		Report type
//								0: stop report
//								1: start periodic report to both logging and AT ports
//								2: start periodic report to logging port only
//				_P2;		Time interval in seconds (if report type is automatic)
//******************************************************************************
AT_Status_t ATCmd_MeasReport_Handler(AT_CmdId_t		cmdId,
   									UInt8			chan,
   									UInt8			accessType,
   									const UInt8* 	_P1,
   									const UInt8* 	_P2 )
{
   	static int	theTimeInterval = 10; //10-second periodic updating by default
	UInt8		theReportFlag;

   	if ((accessType == AT_ACCESS_REGULAR) && _P1)
   	{
   		theReportFlag = atoi((char*)_P1);

		if (theReportFlag == 1)
		{
			AT_GetChannelCfg(chan)->MTREPORT = TRUE;
		}
		else
		{
			AT_GetChannelCfg(chan)->MTREPORT = FALSE;
		}

   		if (_P2)
   		{
   			theTimeInterval = atoi((char*)_P2);
   		}

   	   	//In order to change the time interval stop and then start with new interval
   		DIAG_ApiMeasurmentReportReq(FALSE, (UInt32)theTimeInterval);//Stop
   		if (theReportFlag)
   		{
   			DIAG_ApiMeasurmentReportReq(TRUE, (UInt32)theTimeInterval);//Start
   		}

   		AT_CmdRspOK(chan);

   		return (AT_STATUS_DONE);
   	}
	AT_CmdRspSyntaxError(chan);
   	return (AT_STATUS_DONE);
}

//******************************************************************************
// Function Name:	AT_SendMtReport
//
// Description:		This routin sends the string to AT port based on the chans.
//******************************************************************************
static void AT_SendMtReport(const UInt8* inStrPtr)
{
	UInt8 theChanIndex;

	for (theChanIndex = 0; theChanIndex < ATDLCCOUNT; theChanIndex++)
	{
		if (AT_GetChannelCfg(theChanIndex)->MTREPORT)
		{
			AT_OutputStr(theChanIndex, inStrPtr);
		}
	}
}

//******************************************************************************
// Function Name:	At_HandleMeasReportParamInd
//
// Description: This routin prints out all the test parameters to AT port.
//
// Note:		This is only used for the testing purpose.
//******************************************************************************
void At_HandleMeasReportParamInd(InterTaskMsg_t *inMsgPtr)
{
#ifdef  STACK_wedge
   	int i;
   	UInt8 output[MAX_RESPONSE_LEN+1];
	MS_MMParam_t*	mmParamPtr = &((MS_RxTestParam_t*)inMsgPtr->dataBuf)->mm_param;
	MS_GSMParam_t*	gsmParamPtr = &((MS_RxTestParam_t*)inMsgPtr->dataBuf)->gsm_param;
	MS_UMTSParam_t*	umtsParamPtr = &((MS_RxTestParam_t*)inMsgPtr->dataBuf)->umts_param;

  	sprintf((char*)output, "Xxxxxxxxxxxxxxxxxxxxxx Field Test Report xxxxxxxxxxxxxxxxxxxxxX");

   	AT_SendMtReport((UInt8*)output);

	if (gsmParamPtr->valid)
	{
		sprintf((char*)output, "***GSM  Parameters*** Valid:%d", gsmParamPtr->valid);
		AT_SendMtReport((UInt8*)output);

   		if (gsmParamPtr->ma.rf_chan_cnt <= 64)
   		{
   			for (i = 0; i < gsmParamPtr->ma.rf_chan_cnt; i++)
   			{
  				sprintf((char*)output, "MaRfChanNo: %x, ", gsmParamPtr->ma.rf_chan_array[i]);
   				AT_SendMtReport((UInt8*)output);
   			}
		}

  		sprintf((char*)output,
  				"***ADJ CELL INFO***	No. NCells: %d	MCC: %04x	MNC: %02x	LAC: %04x",
  				gsmParamPtr->no_ncells,
   				gsmParamPtr->ncell_param.A[0].mcc,
   				gsmParamPtr->ncell_param.A[0].mnc,
   				gsmParamPtr->ncell_param.A[0].lac);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output, "No ARFCN       RxLev  CI     BSIC   C1        C2        C31       C32");
   		AT_SendMtReport((UInt8*)output);

   		for (i = 0; i < 6; i++)
   		{
   			sprintf((char*)output,
  			"%d  %04x (%03d)  %02x     %04x   %02x     %08x  %08x  %08x  %08x",
   			i+1,
   			gsmParamPtr->ncell_param.A[i].arfcn,
   			gsmParamPtr->ncell_param.A[i].arfcn,
  			gsmParamPtr->ncell_param.A[i].rxlev,
   			gsmParamPtr->ncell_param.A[i].ci,
   			gsmParamPtr->ncell_param.A[i].bsic,
   			gsmParamPtr->ncell_param.A[i].c1,
   			gsmParamPtr->ncell_param.A[i].c2,
   			gsmParamPtr->ncell_param.A[i].c31,
   			gsmParamPtr->ncell_param.A[i].c32);
   			AT_SendMtReport((UInt8*)output);
   		}

  		sprintf((char*)output,
  				"***GSM INFO***	ARFCN:%04x(%d)     C1:%02x     C2:%02x	CBA:%02x  CBQ:%02x",
  				gsmParamPtr->arfcn,
  				gsmParamPtr->arfcn,
  				gsmParamPtr->c1,
  				gsmParamPtr->c2,
  				gsmParamPtr->cba,
  				gsmParamPtr->cbq);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"LAC:%04x  ACC:%04x    CrOffset:%02x   BSIC:%02x   BsPaMfrms:%02x    RxAccMin:%04x",
  				gsmParamPtr->lac,
  				gsmParamPtr->acc,
  				gsmParamPtr->cr_offset,
  				gsmParamPtr->bsic,
  				gsmParamPtr->bs_pa_mfrms,
  				gsmParamPtr->rx_acc_min);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"MCC:%04x  Band:%04x   BcchCumbin:%d  RxLev:%02x  BsAgBlksRes:%02x  T3212[s]:%d",
  				gsmParamPtr->mcc,
  				gsmParamPtr->band,
  				gsmParamPtr->bcch_combined,
  				gsmParamPtr->rxlev,
  				gsmParamPtr->bs_ag_blks_res,
  				gsmParamPtr->t3212);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"MNC:%04x  TxPWR:%04x  Penalty:%04x  CI:%04x   TmpOffset:%04x  C2Valid:%02d",
  				gsmParamPtr->mnc,
				gsmParamPtr->ms_txpwr,
  				gsmParamPtr->penalty_t,
  				gsmParamPtr->ci,
  				gsmParamPtr->tmp_offset,
  				gsmParamPtr->c2_valid);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"***GPRS/EGPRS INFO***  EDGEvalid:%d  EDGEpresent:%d   C31:%08x   C32:%08x",
  				gsmParamPtr->edge_param.valid,
  				gsmParamPtr->edge_param.edge_present,
  				gsmParamPtr->c31,
  				gsmParamPtr->c32);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"GPRSpresent:%d   PSI1rep:%x   AccBrstTyp:%04x   SpPgCy:%d  EMO:%08x",
  				gsmParamPtr->gprs_present,
  				gsmParamPtr->psi1_rep,
  				(int)gsmParamPtr->access_burst_type,
				gsmParamPtr->sp_pg_cy,
				(int)gsmParamPtr->emo);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"T3192[s]:%d  DRxMax:%02x  PSIcountHr:%x  RAC:%02x  PriAccThr:%02x  GrrState:%x",
  				gsmParamPtr->t3192,
  				gsmParamPtr->drx_max,
  				gsmParamPtr->psi_count_hr,
  				gsmParamPtr->rac,
  				gsmParamPtr->priority_access_thr,
  				gsmParamPtr->grr_state);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"PSIcountLr:%x   PBCCHpresent:%d   NOM:%04x    NCO:%08x",
  				gsmParamPtr->psi_count_lr,
  				gsmParamPtr->pbcch_present,
				gsmParamPtr->nom,
  				(int)gsmParamPtr->nco);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"***DEDICATED MODE*** RadioLnkTmout:%02x  ARFCN:%04x(%d)  TmSlotAss:%02x  chnRelCause:%02x",
  				gsmParamPtr->radio_link_timeout,
  				gsmParamPtr->arfcn_ded,
  				gsmParamPtr->arfcn_ded,
  				gsmParamPtr->timeslot_assigned,
				gsmParamPtr->chn_rel_cause);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"ChType:%02x  RxQlSub:%02x  RxLevSub:%02x  TxPwr:%02x  TSC:%02x  DTxOn:%02x  MaRfChCnt:%02x",
  				gsmParamPtr->chan_type,
  				gsmParamPtr->rxqualsub,
  				gsmParamPtr->rxlevsub,
  				gsmParamPtr->txpwr,
  				gsmParamPtr->tsc,
  				gsmParamPtr->dtx_used,
  				gsmParamPtr->ma.rf_chan_cnt);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"CiphOn:%02d  RxQlFul:%02x  RxLevFul:%02x  MAIO:%02x   HSN:%02x  TA:%02x  ChMode:%02x",
  				gsmParamPtr->cipher_on,
  				gsmParamPtr->rxqualfull,
  				gsmParamPtr->rxlevfull,
  				gsmParamPtr->maio,
  				gsmParamPtr->hsn,
  				gsmParamPtr->timing_advance,
  				gsmParamPtr->chan_mode);
   		AT_SendMtReport((UInt8*)output);

		if (gsmParamPtr->gprs_packet_param.valid)
		{
  			sprintf((char*)output,
  					"***Packet Mode GPRS***	GPRSpktVa:%d TA:%02x DlTsAs:%02x UlTsAs:%02x C_Val:%02x ALPHA:%02x",
  					gsmParamPtr->gprs_packet_param.valid,
  					gsmParamPtr->gprs_packet_param.timing_advance,
  					gsmParamPtr->gprs_packet_param.dl_timeslot_assigned,
  					gsmParamPtr->gprs_packet_param.ul_timeslot_assigned,
  					gsmParamPtr->gprs_packet_param.c_value,
  					gsmParamPtr->gprs_packet_param.alpha);
   			AT_SendMtReport((UInt8*)output);

			sprintf((char*)output, "Slot       DlCsModePerTs       UlCsModePerTs       GammaPerTs");
			AT_SendMtReport((UInt8*)output);

			for (i = 0; i < 8; i++)
			{
				sprintf((char*)output, "%d             %d                  %d                %d",
						i,
						gsmParamPtr->gprs_packet_param.dl_cs_mode_per_ts[i],
						gsmParamPtr->gprs_packet_param.ul_cs_mode_per_ts[i],
						gsmParamPtr->gprs_packet_param.gamma[i]);
				AT_SendMtReport((UInt8*)output);
			}
		}
		else
		{
  			sprintf((char*)output,
					"***Packet Mode GPRS***	GPRSpktVa:%d (Not Valid)",
  					gsmParamPtr->gprs_packet_param.valid);
   			AT_SendMtReport((UInt8*)output);
		}

		//FIX ME: Investigate valid verserses edge_present????
		if (gsmParamPtr->edge_param.valid &&
			gsmParamPtr->edge_param.edge_present &&
			gsmParamPtr->edge_param.edge_packet_param.valid)
		{
			if (gsmParamPtr->edge_param.edge_packet_param.gprs_packet_param.valid)
			{
  				sprintf((char*)output,
  						"***Pkt Mode EDGE*** EDGEpkVa:%d GPRSpkVa:%d TA:%02x DlTsAs:%02x UlTsAs:%02x C_Val:%02x ALPHA:%02x",
  						gsmParamPtr->edge_param.edge_packet_param.valid,
  						gsmParamPtr->edge_param.edge_packet_param.gprs_packet_param.valid,
  						gsmParamPtr->edge_param.edge_packet_param.gprs_packet_param.timing_advance,
  						gsmParamPtr->edge_param.edge_packet_param.gprs_packet_param.dl_timeslot_assigned,
  						gsmParamPtr->edge_param.edge_packet_param.gprs_packet_param.ul_timeslot_assigned,
  						gsmParamPtr->edge_param.edge_packet_param.gprs_packet_param.c_value,
  						gsmParamPtr->edge_param.edge_packet_param.gprs_packet_param.alpha);
   				AT_SendMtReport((UInt8*)output);

				sprintf((char*)output, "Slot   DlCsModePerTs      UlCsModePerTs		GammaPerTs");
					AT_SendMtReport((UInt8*)output);

				for (i = 0; i < 8; i++)
				{
					sprintf((char*)output, "%d          %d                 %d               %d",
							i,
							gsmParamPtr->edge_param.edge_packet_param.gprs_packet_param.dl_cs_mode_per_ts[i],
							gsmParamPtr->edge_param.edge_packet_param.gprs_packet_param.ul_cs_mode_per_ts[i],
							gsmParamPtr->edge_param.edge_packet_param.gprs_packet_param.gamma[i]);
					AT_SendMtReport((UInt8*)output);
				}
			}
			else
			{
  				sprintf((char*)output,
  						"***Pkt Mode EDGE*** EDGEpkVa:%d",
  						gsmParamPtr->edge_param.edge_packet_param.valid);
   				AT_SendMtReport((UInt8*)output);
			}

  			sprintf((char*)output,
  					"MeanBepGmsk:%02x  MeanBep8psk:%02x  CvBepGmsk:%02x  CvBep8psk:%02x",
  					gsmParamPtr->edge_param.edge_packet_param.mean_bep_gmsk,
  					gsmParamPtr->edge_param.edge_packet_param.mean_bep_8psk,
  					gsmParamPtr->edge_param.edge_packet_param.cv_bep_gmsk,
  					gsmParamPtr->edge_param.edge_packet_param.cv_bep_8psk);
   			AT_SendMtReport((UInt8*)output);
		}
		else
		{
  			sprintf((char*)output, "***Pkt Mode EDGE*** EDGEpk Valid: not Valid.");
   			AT_SendMtReport((UInt8*)output);
		}

		if (gsmParamPtr->amr_param.valid)
		{
  			sprintf((char*)output,
  					"***AMR Info***	AMRValid:%d    CMIP:%02x	CMI:%02x	CMR:%02x",
  					gsmParamPtr->amr_param.valid,
  					gsmParamPtr->amr_param.cmip,
  					gsmParamPtr->amr_param.cmi,
  					gsmParamPtr->amr_param.cmr);
   			AT_SendMtReport((UInt8*)output);

  			sprintf((char*)output,
  					"DlCi:%02x  ACS:%02x  SpeechRate:%02x	Subchan:%02x",
  					gsmParamPtr->amr_param.dl_ci,
  					gsmParamPtr->amr_param.acs,
  					gsmParamPtr->amr_param.speech_rate,
  					gsmParamPtr->amr_param.subchan);
   			AT_SendMtReport((UInt8*)output);

			sprintf((char*)output, "No   Threshold   Hysteresis");
			AT_SendMtReport((UInt8*)output);

			for (i = 0; i < 3; i++)
			{
				sprintf((char*)output, "%d    0x%02x        0x%02x",
						i+1,
						gsmParamPtr->amr_param.threshold[i],
						gsmParamPtr->amr_param.hysteresis[i]);
				AT_SendMtReport((UInt8*)output);
			}
		}
		else
		{
  			sprintf((char*)output, "***AMR Info***	AMRValid: Not valid.");
   			AT_SendMtReport((UInt8*)output);
		}
	}
	else
	{
  		sprintf((char*)output, "***NO GSM PARAMETERS***");
		AT_SendMtReport((UInt8*)output);
	}

	//Prints out the MS_MMParam_t

	if (mmParamPtr->valid)
	{
  		sprintf((char*)output,
  				"***MM Param***	Valid:%d  MmcState:%02x  MmeState:%02x  GmmState:%02x  rat:%02x  nom:%02x",
  				mmParamPtr->valid,
  				mmParamPtr->mmc_state,
  				mmParamPtr->mme_state,
  				mmParamPtr->gmm_state,
  				mmParamPtr->rat,
  				mmParamPtr->nom);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"searchMode:%02x  MmUpdatStatus:%02x  MmRejCause:%02x  MmRetransCnt:%02x  MmTimStatus:%08x",
  				mmParamPtr->search_mode,
  				mmParamPtr->mm_update_status,
  				mmParamPtr->mm_reject_cause,
  				mmParamPtr->mm_retrans_cnt,
  				mmParamPtr->mm_timer_status);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"GmmUpdatStatus:%02x  GmmRejCause:%02x  GmmRetransCnt:%02x  GmmTimStatus:%08x",
  				mmParamPtr->mm_update_status,
  				mmParamPtr->mm_reject_cause,
  				mmParamPtr->mm_retrans_cnt,
  				mmParamPtr->mm_timer_status);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output,
  				"GmmRetryCnt:%02x  GmmAttachType:%02x  GmmRauType:%02x  GmmDetachType:%02x  GmmDetachDir:%02x",
  				mmParamPtr->gmm_retry_cnt,
  				mmParamPtr->gmm_attach_type,
  				mmParamPtr->gmm_rau_type,
  				mmParamPtr->gmm_detach_type,
				mmParamPtr->gmm_detach_dir);
   		AT_SendMtReport((UInt8*)output);
	}
	else
	{
  		sprintf((char*)output, "***NO MM Parameters***");
		AT_SendMtReport((UInt8*)output);
	}

	//Prints out the MS_UMTSParam_t
	if (umtsParamPtr->valid)
	{
  		sprintf((char*)output, "***UMTS  Param: rrcState:%d rrcdcState:%d rrcbpState:%d",
  				umtsParamPtr->rrc_state,
  				umtsParamPtr->rrcdc_state,
				umtsParamPtr->rrcbp_state);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output, "rrcmState:%d  pSc:%04x  lac:%04x  rac:%d  rssi:%d",
  				umtsParamPtr->rrcm_state,
  				umtsParamPtr->p_sc,
  				umtsParamPtr->lac,
  				umtsParamPtr->rac,
				umtsParamPtr->rssi);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output, "cell_id:%08x  ulUarfcn:%d  dlUarfcn:%d  hcsUsed:%d  highMobil:%d",
  				umtsParamPtr->cell_id,
  				umtsParamPtr->ul_uarfcn,
  				umtsParamPtr->dl_uarfcn,
  				umtsParamPtr->hcs_used,
  				umtsParamPtr->high_mobility);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output, "cpichRscp:%d  cpichEcn0:%d  cipherOn:%d  rankValue:%d  txPwr:%d",
  				umtsParamPtr->cpich_rscp,
  				umtsParamPtr->cpich_ecn0,
  				umtsParamPtr->cipher_on,
  				umtsParamPtr->ranking_value,
  				umtsParamPtr->tx_pwr);
   		AT_SendMtReport((UInt8*)output);

  		sprintf((char*)output, "chnRelCause:%d  noActCells:%d  noVirtActCells:%d  noUmtsNcells:%d  noGsmNcells:%d",
  				umtsParamPtr->chn_rel_cause,
  				umtsParamPtr->no_active_set_cells,
  				umtsParamPtr->no_virtual_active_set_cells,
  				umtsParamPtr->no_umts_ncells,
				umtsParamPtr->no_gsm_ncells);
   		AT_SendMtReport((UInt8*)output);

		for (i = 0; ((i < umtsParamPtr->no_umts_ncells) && (i <= MAX_NUMBER_OF_UMTS_NCELLS)); i++)
		{
  			sprintf((char*)output, "%d	cellType:%d  dlUarfcn:%d  cpichSc:%d  cpichRscp:%d",
					i+1,
  					umtsParamPtr->umts_ncell.A[i].cell_type,
					umtsParamPtr->umts_ncell.A[i].dl_uarfcn,
					umtsParamPtr->umts_ncell.A[i].cpich_sc,
					umtsParamPtr->umts_ncell.A[i].cpich_rscp);

			AT_SendMtReport((UInt8*)output);

  			sprintf((char*)output, "%d	cpichEcn0:%d  rankPos:%d  rankingValue:%d  rankingStatus:%d",
					i+1,
  					umtsParamPtr->umts_ncell.A[i].cpich_ecn0,
					umtsParamPtr->umts_ncell.A[i].rank_pos,
					umtsParamPtr->umts_ncell.A[i].ranking_value,
					umtsParamPtr->umts_ncell.A[i].ranking_status);

			AT_SendMtReport((UInt8*)output);
		}

		if (umtsParamPtr->no_gsm_ncells)
		{

   			sprintf((char*)output, "# MCC   MNC  LAC   RankPos  RankVal  RankStatus");
   			AT_SendMtReport((UInt8*)output);

  			for (i = 0; ((i < umtsParamPtr->no_gsm_ncells) && (i <= MAX_NUMBER_OF_GSM_NCELLS)); i++)
   			{
  				sprintf((char*)output, "%d %04x   %02x   %04x  %02x       %04x       %02x",
						i+1,
   						umtsParamPtr->gsm_ncell.A[i].mcc,
   						umtsParamPtr->gsm_ncell.A[i].mnc,
   						umtsParamPtr->gsm_ncell.A[i].lac,
   						umtsParamPtr->gsm_ncell.A[i].rank_pos,
   						umtsParamPtr->gsm_ncell.A[i].ranking_value,
   						umtsParamPtr->gsm_ncell.A[i].ranking_status);
   				AT_SendMtReport((UInt8*)output);
			}

  			sprintf((char*)output, "No ARFCN       RxLev  CI     BSIC   C1        C2        C31       C32");
   			AT_SendMtReport((UInt8*)output);

   			for (i = 0; ((i < umtsParamPtr->no_gsm_ncells) && (i <= MAX_NUMBER_OF_GSM_NCELLS)); i++)
   			{
   				sprintf((char*)output,
  				"%d  %04x (%03d)  %02x     %04x   %02x     %08x  %08x  %08x  %08x",
   				i+1,
   				umtsParamPtr->gsm_ncell.A[i].arfcn,
   				umtsParamPtr->gsm_ncell.A[i].arfcn,
  				umtsParamPtr->gsm_ncell.A[i].rxlev,
   				umtsParamPtr->gsm_ncell.A[i].ci,
   				umtsParamPtr->gsm_ncell.A[i].bsic,
   				umtsParamPtr->gsm_ncell.A[i].c1,
   				umtsParamPtr->gsm_ncell.A[i].c2,
   				umtsParamPtr->gsm_ncell.A[i].c31,
   				umtsParamPtr->gsm_ncell.A[i].c32);
   				AT_SendMtReport((UInt8*)output);
   			}
		}

  		sprintf((char*)output, "*Meas Report ** GenP1:%02x  GenP2:%02x",
  				umtsParamPtr->meas_report.gen_param.p1,
  				umtsParamPtr->meas_report.gen_param.p2);

   		AT_SendMtReport((UInt8*)output);

		for (i = 0; i < MAX_PARAM_PER_MEAS; i++)
   		{
   			sprintf((char*)output, "#:%d measId:%02x  eventId:%02x  dataElem[0-5]:%02x %02x %02x %02x %02x %02x",
    		i+1,
  			umtsParamPtr->meas_report.param_per_meas[i].meas_id,
  			umtsParamPtr->meas_report.param_per_meas[i].event_id,
  			umtsParamPtr->meas_report.param_per_meas[i].data_elements[0],
			umtsParamPtr->meas_report.param_per_meas[i].data_elements[1],
			umtsParamPtr->meas_report.param_per_meas[i].data_elements[2],
			umtsParamPtr->meas_report.param_per_meas[i].data_elements[3],
			umtsParamPtr->meas_report.param_per_meas[i].data_elements[4],
			umtsParamPtr->meas_report.param_per_meas[i].data_elements[5]);
   			AT_SendMtReport((UInt8*)output);
   		}

  		sprintf((char*)output, "*DCH Report ** meas_bler:%d  target_sir:%d  meas_sir:%d",
  				umtsParamPtr->dch_report.meas_bler,
  				umtsParamPtr->dch_report.target_sir,
  				umtsParamPtr->dch_report.meas_sir);

   		AT_SendMtReport((UInt8*)output);
	}
	else
	{
  		sprintf((char*)output, "***NO UMTS Parameters***");
		AT_SendMtReport((UInt8*)output);
	}
#else  //STACK_wedge
   	int i;
   	UInt8 output[MAX_RESPONSE_LEN+1];
   	MS_RxTestParam_t* tstParamPtr = (MS_RxTestParam_t*)inMsgPtr->dataBuf;

  	sprintf((char*)output, "Xxxxxxxxxxxxxxxxxxxxxx Field Test Report xxxxxxxxxxxxxxxxxxxxxX");

   	AT_SendMtReport((UInt8*)output);

   	if (tstParamPtr->ma.rf_chan_cnt <= 64)
   	{
   		for (i = 0; i < tstParamPtr->ma.rf_chan_cnt; i++)
   		{
  			sprintf((char*)output, "MaRfChanNo: %x, ", tstParamPtr->ma.rf_chan_array[i]);
   			AT_SendMtReport((UInt8*)output);
   		}
   	}

  	sprintf((char*)output,
  			"***ADJ CELL INFO***	No. NCells: %d	MCC: %04x	MNC: %02x	LAC: %04x",
  			tstParamPtr->no_ncells,
   			tstParamPtr->ncell_param.A[0].mcc,
   			tstParamPtr->ncell_param.A[0].mnc,
   			tstParamPtr->ncell_param.A[0].lac);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output, "No ARFCN       RxLev  CI     BSIC   C1        C2        C31       C32");
   	AT_SendMtReport((UInt8*)output);

   	for (i = 0; i < 6; i++)
   	{
   		sprintf((char*)output,
  		"%d  %04x (%03d)  %02x     %04x   %02x     %08x  %08x  %08x  %08x",
   		i+1,
   		tstParamPtr->ncell_param.A[i].arfcn,
   		tstParamPtr->ncell_param.A[i].arfcn,
  		tstParamPtr->ncell_param.A[i].rxlev,
   		tstParamPtr->ncell_param.A[i].ci,
   		tstParamPtr->ncell_param.A[i].bsic,
   		tstParamPtr->ncell_param.A[i].c1,
   		tstParamPtr->ncell_param.A[i].c2,
   		tstParamPtr->ncell_param.A[i].c31,
   		tstParamPtr->ncell_param.A[i].c32);
   		AT_SendMtReport((UInt8*)output);

   	}

  	sprintf((char*)output,
  			"***GSM INFO***	ARFCN:%04x(%d)     C1:%02x     C2:%02x	CBA:%02x  CBQ:%02x",
  			tstParamPtr->arfcn,
  			tstParamPtr->arfcn,
  			tstParamPtr->c1,
  			tstParamPtr->c2,
  			tstParamPtr->cba,
  			tstParamPtr->cbq);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"LAC:%04x  ACC:%04x    CrOffset:%02x   BSIC:%02x   BsPaMfrms:%02x    RxAccMin:%04x",
  			tstParamPtr->lac,
  			tstParamPtr->acc,
  			tstParamPtr->cr_offset,
  			tstParamPtr->bsic,
  			tstParamPtr->bs_pa_mfrms,
  			tstParamPtr->rx_acc_min);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"MCC:%04x  Band:%04x   BcchCumbin:%d  RxLev:%02x  BsAgBlksRes:%02x  T3212[s]:%d",
  			tstParamPtr->mcc,
  			tstParamPtr->band,
  			tstParamPtr->bcch_combined,
  			tstParamPtr->rxlev,
  			tstParamPtr->bs_ag_blks_res,
  			tstParamPtr->t3212);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"MNC:%04x  TxPWR:%04x  Penalty:%04x  CI:%04x   TmpOffset:%04x  C2Valid:%02d",
  			tstParamPtr->mnc,
			tstParamPtr->ms_txpwr,
  			tstParamPtr->penalty_t,
  			tstParamPtr->ci,
  			tstParamPtr->tmp_offset,
  			tstParamPtr->c2_valid);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"***GPRS/EGPRS INFO***  EDGEvalid:%d  EDGEpresent:%d   C31:%08x   C32:%08x",
  			tstParamPtr->edge_param.valid,
  			tstParamPtr->edge_param.edge_present,
  			tstParamPtr->c31,
  			tstParamPtr->c32);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"GPRSpresent:%d   PSI1rep:%x   AccBrstTyp:%04x   SpPgCy:%d  EMO:%08x",
  			tstParamPtr->gprs_present,
  			tstParamPtr->psi1_rep,
  			(int)tstParamPtr->access_burst_type,
			tstParamPtr->sp_pg_cy,
			(int)tstParamPtr->emo);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"T3192[s]:%d  DRxMax:%02x    PSIcountHr:%x     RAC:%02x    PriAccThr:%02x",
  			tstParamPtr->t3192,
  			tstParamPtr->drx_max,
  			tstParamPtr->psi_count_hr,
  			tstParamPtr->rac,
  			tstParamPtr->priority_access_thr);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"PSIcountLr:%x   PBCCHpresent:%d   NOM:%04x    NCO:%08x",
  			tstParamPtr->psi_count_lr,
  			tstParamPtr->pbcch_present,
			tstParamPtr->nom,
  			(int)tstParamPtr->nco);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"***DEDICATED MODE*** RadioLnkTmout:%02x  ARFCN:%04x(%d)  TmSlotAss:%02x",
  			tstParamPtr->radio_link_timeout,
  			tstParamPtr->arfcn_ded,
  			tstParamPtr->arfcn_ded,
  			tstParamPtr->timeslot_assigned);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"ChType:%02x  RxQlSub:%02x  RxLevSub:%02x  TxPwr:%02x  TSC:%02x  DTxOn:%02x  MaRfChCnt:%02x",
  			tstParamPtr->chan_type,
  			tstParamPtr->rxqualsub,
  			tstParamPtr->rxlevsub,
  			tstParamPtr->txpwr,
  			tstParamPtr->tsc,
  			tstParamPtr->dtx_used,
  			tstParamPtr->ma.rf_chan_cnt);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"CiphOn:%02d  RxQlFul:%02x  RxLevFul:%02x  MAIO:%02x   HSN:%02x  TA:%02x  ChMode:%02x",
  			tstParamPtr->cipher_on,
  			tstParamPtr->rxqualfull,
  			tstParamPtr->rxlevfull,
  			tstParamPtr->maio,
  			tstParamPtr->hsn,
  			tstParamPtr->timing_advance,
  			tstParamPtr->chan_mode);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"***Packet Mode GPRS***	GPRSpktVa:%d TA:%02x DlTsAs:%02x UlTsAs:%02x C_Val:%02x ALPHA:%02x",
  			tstParamPtr->gprs_packet_param.valid,
  			tstParamPtr->gprs_packet_param.timing_advance,
  			tstParamPtr->gprs_packet_param.dl_timeslot_assigned,
  			tstParamPtr->gprs_packet_param.ul_timeslot_assigned,
  			tstParamPtr->gprs_packet_param.c_value,
  			tstParamPtr->gprs_packet_param.alpha);
   	AT_SendMtReport((UInt8*)output);

	sprintf((char*)output, "Slot       DlCsModePerTs       UlCsModePerTs       GammaPerTs");
	AT_SendMtReport((UInt8*)output);

	for (i = 0; i < 8; i++)
	{
		sprintf((char*)output, "%d             %d                  %d                %d",
				i,
				tstParamPtr->gprs_packet_param.dl_cs_mode_per_ts[i],
				tstParamPtr->gprs_packet_param.ul_cs_mode_per_ts[i],
				tstParamPtr->gprs_packet_param.gamma[i]);
		AT_SendMtReport((UInt8*)output);
	}

  	sprintf((char*)output,
  			"***Pkt Mode EDGE*** EDGEpkVa:%d TA:%02x DlTsAs:%02x UlTsAs:%02x C_Val:%02x ALPHA:%02x",
  			tstParamPtr->edge_param.edge_packet_param.valid,
  			tstParamPtr->edge_param.edge_packet_param.gprs_packet_param.timing_advance,
  			tstParamPtr->edge_param.edge_packet_param.gprs_packet_param.dl_timeslot_assigned,
  			tstParamPtr->edge_param.edge_packet_param.gprs_packet_param.ul_timeslot_assigned,
  			tstParamPtr->edge_param.edge_packet_param.gprs_packet_param.c_value,
  			tstParamPtr->edge_param.edge_packet_param.gprs_packet_param.alpha);
   	AT_SendMtReport((UInt8*)output);

	sprintf((char*)output, "Slot   DlCsModePerTs      UlCsModePerTs		GammaPerTs");
		AT_SendMtReport((UInt8*)output);

	for (i = 0; i < 8; i++)
	{
		sprintf((char*)output, "%d          %d                 %d               %d",
				i,
				tstParamPtr->edge_param.edge_packet_param.gprs_packet_param.dl_cs_mode_per_ts[i],
				tstParamPtr->edge_param.edge_packet_param.gprs_packet_param.ul_cs_mode_per_ts[i],
				tstParamPtr->edge_param.edge_packet_param.gprs_packet_param.gamma[i]);
		AT_SendMtReport((UInt8*)output);
	}

  	sprintf((char*)output,
  			"MeanBepGmsk:%02x  MeanBep8psk:%02x  CvBepGmsk:%02x  CvBep8psk:%02x",
  			tstParamPtr->edge_param.edge_packet_param.mean_bep_gmsk,
  			tstParamPtr->edge_param.edge_packet_param.mean_bep_8psk,
  			tstParamPtr->edge_param.edge_packet_param.cv_bep_gmsk,
  			tstParamPtr->edge_param.edge_packet_param.cv_bep_8psk);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"***AMR Info***	AMRValid:%d    CMIP:%02x	CMI:%02x	CMR:%02x",
  			tstParamPtr->amr_param.valid,
  			tstParamPtr->amr_param.cmip,
  			tstParamPtr->amr_param.cmi,
  			tstParamPtr->amr_param.cmr);
   	AT_SendMtReport((UInt8*)output);

  	sprintf((char*)output,
  			"DlCi:%02x  ACS:%02x  SpeechRate:%02x	Subchan:%02x",
  			tstParamPtr->amr_param.dl_ci,
  			tstParamPtr->amr_param.acs,
  			tstParamPtr->amr_param.speech_rate,
  			tstParamPtr->amr_param.subchan);
   	AT_SendMtReport((UInt8*)output);

	sprintf((char*)output, "No   Threshold   Hysteresis");
	AT_SendMtReport((UInt8*)output);

	for (i = 0; i < 3; i++)
	{
		sprintf((char*)output, "%d    0x%02x        0x%02x",
				i+1,
				tstParamPtr->amr_param.threshold[i],
				tstParamPtr->amr_param.hysteresis[i]);
		AT_SendMtReport((UInt8*)output);
	}
#endif  //STACK_wedge
}
