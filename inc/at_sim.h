/**
*
*   @file   at_sim.h
*
*   @brief  This file defines the interface for at_sim operation.
*
****************************************************************************/

#ifndef _AT_SIM_H_
#define _AT_SIM_H_


//******************************************************************************
//							include
//******************************************************************************
#include	"types.h"
#include	"consts.h"

//******************************************************************************
//							definition
//******************************************************************************
typedef enum {
	ENABLE_CHV,			// enable/disable CHV
	CHANGE_CHV1,		// change CHV1
	CHANGE_CHV2,		// change CHV2
	VERIFY_CHV2,		// verify CHV2
	UNBLOCK_CHV1,		// unblock CHV1
	UNBLOCK_CHV2,		// unblock CHV2
	ENABLE_FDN,			// enable/disable FDN
	READ_AOC,			// read aoc related items
	WRITE_AOC,			// write aoc related items

	/* The following states are used when we need to change PIN1 when it is
	 * disabled (certain PC SW needs this functionality). The SIM card does not allow
	 * us to change PIN1 when it is disabled (GSM 11.11, section 8.10).
	 * So we first enable PIN1, then change PIN1 and finally we disable it
	 * in order to accomplish what the PC SW needs.
	 */
	ENABLE_CHV1_TO_CHANGE,
	CHANGE_CHV1_TO_CHANGE,
	DISABLE_CHV1_TO_CHANGE,

	NULL_ID
}ATCSIMProcessID_t;

typedef struct
{
	CHVString_t	old_pin_str;
	CHVString_t	new_pin_str;
	SIMAccess_t	change_chv1_result;
} CHANGE_CHV1_Data_t;


#endif	// _AT_SIM_H_

