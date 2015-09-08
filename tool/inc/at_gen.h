//-----------------------------------------------------------------------------
//	atgen utility
//-----------------------------------------------------------------------------

#ifndef __ATGEN_H__
#define __ATGEN_H__

#include "types.h"
#include "consts.h"

typedef struct {
	char*			cmdName ;
	char*			cmdSymbol ;
	char*			cmdType ;
	char*			handler ;
	UInt32			minParms ;
	UInt32			maxParms ;
	char*			validParms ;
	Int32			validParmsIdx ;
	UInt32			options ;
	char*			alias ;
	char*			cmdDescr ;
	UInt32			enumVal ;
}	ATGEN_Cmd_t ;

typedef struct {
	UInt32			nCmd ;
	ATGEN_Cmd_t*	cmd ;
	UInt32*			sortOrder ;
	UInt32			maxParms ;
}	ATGEN_CmdTbl_t ;

ATGEN_CmdTbl_t* ATGEN_NewCmdTbl ( void ) ;
void ATGEN_ParseInputFile ( char * fileName, char * symTab, ATGEN_CmdTbl_t * cmdTbl ) ;
void ATGEN_GenerateBasicIndices( ATGEN_CmdTbl_t * cmdTbl ) ;
void ATGEN_GenExtendedIndices( ATGEN_CmdTbl_t * cmdTbl, UInt32 nextEnum ) ;
void ATGEN_SerializeCmdTbl( ATGEN_CmdTbl_t * cmdTbl, char * srcdir ) ;
ATGEN_CmdTbl_t * ATGEN_DeserializeCmdTbl( UInt32 * nCmd, char * srcdir ) ;
void ATGEN_GenerateCode( ATGEN_CmdTbl_t * cmdTbl, char * srcdir, char * incdir ) ;



#endif

