//-----------------------------------------------------------------------------
//	atgen utility
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "at_gen.h"

//	at_gen infile srcdir incdir [symdef]

#define ARG_INFILE argv[1]
#define ARG_SRCDIR argv[2]
#define ARG_INCDIR argv[3]
#define ARG_SYMDEF argv[4]
#define ARG_MINARG 4
#define ARG_MAXARG 5

void main (
	int				argc,
	char**			argv )

{
	ATGEN_CmdTbl_t*		cmdTbl ;

	if( argc < ARG_MINARG || argc > ARG_MAXARG ) {
		printf("Usage: at_gen infile srcdir incdir [symdef] \n");
		exit(1);
	}

	cmdTbl = ATGEN_NewCmdTbl ( ) ;

	ATGEN_ParseInputFile ( ARG_INFILE, ARG_MAXARG==argc?ARG_SYMDEF:0, cmdTbl ) ;

	ATGEN_GenerateBasicIndices( cmdTbl ) ;

	ATGEN_SerializeCmdTbl( cmdTbl, ARG_SRCDIR ) ;

	ATGEN_GenerateCode( cmdTbl, ARG_SRCDIR, ARG_INCDIR ) ;

	printf ( "...done.\n" ) ;

	exit(0);
}

