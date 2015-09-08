//********************************************************************
//
// Description:  This file defines some utility functions for supporting
// call related handlers.
//
// at_callutil.c
//
//********************************************************************

#include <stdio.h>
#include "stdlib.h"
#include "string.h"
#include "mstruct.h"
#include "types.h"
#include "callconfig.h"
#include "at_api.h"

//util functions for formating data call AT report
void ErrorCorrectionStr(UInt8 ecVal,UInt8* ecStr){

  ecStr[0] = '\0';

  switch(ecVal)
  {
          case 0 :
          case 1 :
          case 2 :
                  sprintf((char*) ecStr,"+ER: NONE");
                  break;
          case 3 :
          case 4 :
                  sprintf((char*) ecStr,"+ER: ALT");
                  break;
          case 5 :
                  sprintf((char*) ecStr,"+ER: LAPM");
                  break;
          default :
                  break;
  }

  return;

}

void DataCompressionStr(UInt8 dcVal,UInt8* dcStr){

  dcStr[0] = '\0';

  switch(dcVal){

	   case 0 :
               sprintf((char*) dcStr, "+DR: NONE");
               break;
       case 1 :
               sprintf((char*) dcStr, "+DR: V42TD");
               break;
       case 2 :
               sprintf((char*) dcStr, "+DR: V42RD");
               break;
       case 3 :
               sprintf((char*) dcStr, "+DR: V42B");
               break;
       default :
               break;

  }
	   return;
}


void CallTransparentStr(UInt8 trVal,UInt8* trStr){

	trStr[0] = '\0';

	if( trVal == 1 )
		sprintf((char*) trStr, "+CR: REL ASYNC" );
	else
		sprintf((char*) trStr, "+CR: ASYNC" );

}

void LocalRateStr(UInt8 lrVal,UInt8* lrStr){

	lrStr[0] = '\0';

	switch ( lrVal )
    {
		case 0 :
            sprintf((char*) lrStr, "2400 bps");
            break;
		case 1 :
            sprintf((char*) lrStr, "4800 bps");
            break;
		case 2 :
            sprintf((char*) lrStr, "9600 bps");
            break;
		case 3 :
            sprintf((char*) lrStr, "19200 bps");
            break;
		case 4 :
            sprintf((char*) lrStr, "38400 bps");
            break;
		case 5 :
            sprintf((char*) lrStr, "57600 bps");
            break;
		case 6 :
            sprintf((char*) lrStr, "115200 bps");
            break;
		default :
            break;
    }

	return;
}

void DataRateStr(UInt8 coding, UInt8 drVal,UInt8* aiur,UInt8* drStr){


	drStr[0] = '\0';
	// coding is for 9600bps slots
	if (coding == 4){
		switch ( drVal ){

			case 1 :	sprintf((char*)drStr, (char*)GetATString(AT_CONNECT_9600_STR) );
						*aiur = 1;
						break;
			case 2 :    sprintf((char*)drStr, (char*)GetATString(AT_CONNECT_19200_STR));
						*aiur = 3;
						break;
			case 3 :    sprintf((char*)drStr, (char*)GetATString(AT_CONNECT_28800_STR));
						*aiur = 4;
						break;
			case 4:		sprintf((char*)drStr, (char*)GetATString(AT_CONNECT_38400_STR));
						*aiur = 5;
						break;
			case 6:		sprintf((char*)drStr, (char*)GetATString(AT_CONNECT_57600_STR));
						*aiur = 7;
						break;
			default :
						break;
		}
	} else
		if( coding == 8 ){

		// coding is for 14400bps slots
			switch ( drVal ){
				case 1 :sprintf((char*)drStr, (char*)GetATString(AT_CONNECT_14400_STR));
						*aiur = 2;
						break;
				case 2 :sprintf((char*)drStr, (char*)GetATString(AT_CONNECT_28800_STR));
						*aiur = 4;
						break;
				case 3 :sprintf((char*)drStr, (char*)GetATString(AT_CONNECT_43200_STR));
						*aiur = 6;
						break;
				case 4 :sprintf((char*)drStr, (char*)GetATString(AT_CONNECT_57600_STR));
						*aiur = 7;
						break;
				default :
						break;

				}

		} else { // default

				sprintf((char*)drStr, (char*)GetATString(AT_CONNECT_9600_STR));

				}

	return;
}


void UserDataRateStr(UInt8 aiur,UInt8* aiurStr){


	aiurStr[0] = '\0';

	switch( aiur ){

		case 1:

			sprintf((char*)aiurStr, (char*)GetATString(AT_CONNECT_9600_STR));
			break;

		case 2:

			sprintf((char*)aiurStr, (char*)GetATString(AT_CONNECT_14400_STR));
			break;

		case 3:

			sprintf((char*)aiurStr, (char*)GetATString(AT_CONNECT_19200_STR));
			break;

		case 4:

			sprintf((char*)aiurStr, (char*)GetATString(AT_CONNECT_28800_STR));
			break;

		case 5:

			sprintf((char*)aiurStr, (char*)GetATString(AT_CONNECT_38400_STR));
			break;

		case 6:

			sprintf((char*)aiurStr, (char*)GetATString(AT_CONNECT_43200_STR));
			break;

		case 7:

			sprintf((char*)aiurStr, (char*)GetATString(AT_CONNECT_57600_STR));
			break;

		default:

			sprintf((char*)aiurStr, (char*)GetATString(AT_CONNECT_9600_STR));
			break;
	}
}




