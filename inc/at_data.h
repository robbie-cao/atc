#ifndef __AT_DATA_H__
#define __AT_DATA_H__


#include "types.h"
#include "mstypes.h"
#include "msconsts.h"

#include "assert.h"

#include "osqueue.h"

#include "mti_mn.h"
#include "mti_build.h"

#include "atc.h"
#include "at_util.h"

#include "v24.h"
#include "mpx.h"
#include "nvram.h"
#include "phonebk.h"

#include "callconfig.h"

#define ATDLCCOUNT		4








//******************************************************************************
//		Variables
//******************************************************************************

extern Queue_t       atc_queue;
extern UInt8         atc_ReleaseCause;
extern UInt8         atc_RxLev;
extern UInt8         atc_RxQual;
extern UInt8         atc_Trace;

#endif  /* __ATC_DATA_H__ */


