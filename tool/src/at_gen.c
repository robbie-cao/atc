//-----------------------------------------------------------------------------
//	atgen utility
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "consts.h"

#include "at_types.h"
#include "parser.h"
#include "at_gen.h"

//-----------------------------------------------------------------------------
//	                            File names
//-----------------------------------------------------------------------------

#define	FNAME_API_GEN_C		"at_api.gen.c"
#define	FNAME_API_GEN_H		"at_api.gen.h"
#define	FNAME_ARGLIST_C		"at_arglist.gen.c"
#define	FNAME_STUBS_C		"at_stubs.gen.c"
#define FNAME_SERIALIZED	"at_cmd.gen"

//-----------------------------------------------------------------------------
//	                           Symbol names
//-----------------------------------------------------------------------------

#define	SYMNAME_CMD_TBL		"atCmdTbl"
#define	SYMNAME_HASH_TBL	"atHashTbl"
#define	SYMNAME_RANGE_VAL	"atRangeVal"
#define	SYMNAME_RANGE_STR	"atRangeStr"
#define SYNNAME_HASH_REDIR	"atHashRedirect"
#define	SYMNAME_NUM_CMDS	"TOTAL_NUM_OF_AT_CMDS"
#define	SYMNAME_MAX_PARMS	"AT_MAX_NUM_PARMS"

//-----------------------------------------------------------------------------
//	                     Preprocessor symbol syntax
//-----------------------------------------------------------------------------
//	Using preprocessor symbols, AT commands can be conditionally selected from
//	the AT command table as in the following example:
//
//		#ifdef SYMBOL1
//			CMD1:	...
//			CMD2:	...
//			...
//		#endif
//
//		#ifndef SYMBOL2
//			CMD3:	...
//			CMD4:	...
//			...
//		#endif
//
//	In this example, CMD1 and CMD2 are included in the build only when
//	SYMBOL1 is defined, and CMD3 and CMD4 are excluded from the build
//	only when SYMBOL2 is defined.
//
//	The symbols are passed to the program from the command line.  Each
//	symbol is prefixed with "-D".. The "-D" prefix is selected to match
//	the compiler switches of the target system, so that effectively compiler
//	switches passed at run time to this program.  The "-D" prefix is defined
//	by PREPROC_SWITCH below, and can be changed.
//
//	For example, to define symbols SYMBOL1 and SYMBOL2, use
//
//		at_gen "-DSYMBOL1 -DSYMBOL2"
//
//	If more than one symbol definition is present, the entire list of
//	symbols must be enclosed in double quotes.
//
//	Note:  #ifdef and #ifndef blocks may be nested.
//
//-----------------------------------------------------------------------------

#define PREPROC_SWITCH		"-D"

//-----------------------------------------------------------------------------
//	                             Error handling
//-----------------------------------------------------------------------------

typedef struct {
	UInt32			val ;
	char*			str ;
}	Error_t ;

enum {
	E_INVALID_PP=1,
	E_NOCMD,
	E_GARBAGE,
	E_CONTINUE,
	E_KEYWORD,
	E_FLAG,
	E_OPTION
} ;

const Error_t errorTbl [] = {
	{ E_INVALID_PP,	"Missing or invalid #ifdef/#ifndef/#endif"		},
	{ E_NOCMD,		"Expected command definition"					},
	{ E_GARBAGE,	"Extra characters near end of line"				},
	{ E_CONTINUE,	"Expected continuation line"					},
	{ E_KEYWORD,	"Bad keyword"									},
	{ E_FLAG,		"Bad flag(s)"									},
	{ E_OPTION,		"Bad option(s)"									},
	{ 0,			0												}
} ;

void ExitOnCmdError ( char* cmdName, char* str )
{
	printf ( "Error: %s\n", str ) ;
	printf ( "Command: %s\n", cmdName ) ;
	exit(1) ;
}


//-----------------------------------------------------------------------------
//	                      Preprocessor line evaluation
//-----------------------------------------------------------------------------

Boolean EvalSymbol( const char* arg, char* val )
{
	ParserMatch_t	match ;
	ParserToken_t	token ;
	const char*		symDef ;
	char			matchStr[ 80 ] ;

	if( !arg ) {
		return FALSE ;
	}

	strcpy( matchStr, "" ) ;
	strcat( matchStr, "~s*" ) ;
	strcat( matchStr, PREPROC_SWITCH ) ;
	strcat( matchStr, "(~l/A-Za-z0-9_/+)" ) ;

	while( 0 != ( symDef = strstr( arg, PREPROC_SWITCH ) ) ) {
		ParserInitMatch( symDef, &match ) ;
		if( RESULT_OK == ParserMatch( matchStr, &match, &token ) ) {
			char str[80] ;
			ParserTknToStr( &token, str, 80 ) ;
			if( !strcmp( str, val ) ) {
				return TRUE ;
			}
			arg = symDef + token.tknLen ;
		}
		else {
			assert(0) ;
		}
	}
	return FALSE ;
}

//-----------------------------------------------------------------------------
//					          Input file reader
//-----------------------------------------------------------------------------

#define INP_BUF_LEN 8192		// nice big number should keep us out of trouble
#define MAX_PP_STACK_DEPTH 8

typedef struct {
	FILE*			file ;
	UInt32			lineNum ;
	UInt32			charNum ;
	char*			readBuf ;
	Boolean			cmdContinue ;
	Int16			ppStackDepth ;
	Boolean			ppInclude[ MAX_PP_STACK_DEPTH ] ;
	const char*		symTab ;
}	InpFile_t ;

//
//	Show an error message
//

void ExitOnInputError ( InpFile_t* inpFile, UInt32 errNum )
{
	const Error_t* error = errorTbl ;

	while ( error->str ) {
		if ( error->val == errNum ) {
			printf ( "Line %d: %s\n", inpFile->lineNum, inpFile->readBuf ) ;
			printf ( "%s\n", error->str ) ;
			exit (1) ;
		}
		++error ;
	}
	printf ( "UNKNOWN ERROR: %d\n", errNum ) ;
	exit (1) ;
}

//
//	Open the input file and init statistics
//

InpFile_t*	OpenInputFile ( char* fName, char* symTab )
{
	InpFile_t*	inpFile = calloc ( 1, sizeof ( InpFile_t ) ) ;

	if ( ! ( inpFile->file = fopen ( fName, "r" ) ) ) {
		free ( inpFile ) ;
		return 0 ;
	}

	inpFile->lineNum          = 0 ;
	inpFile->charNum          = 0 ;
	inpFile->readBuf          = calloc ( INP_BUF_LEN, sizeof ( char ) ) ;
	inpFile->cmdContinue      = FALSE ;
	inpFile->ppStackDepth     = 0 ;
	inpFile->ppInclude[0]     = TRUE ;
	inpFile->symTab = symTab ;

	return inpFile ;

}

//
//	Get a line -- remove comments and newlines, skip blank lines, handle
//	preprocessor lines
//

// #define DEBUG_GETLINE

Result_t GetLine ( InpFile_t* inpFile )
{
	ParserMatch_t	match ;
	ParserToken_t	token ;
	char*			c ;
	Boolean			evalResult ;

	for ( ;; ) {

		if ( ! fgets ( inpFile->readBuf, INP_BUF_LEN, inpFile->file ) ) {
			return RESULT_ERROR ;
		}

		//
		//	remove newline
		//

		++inpFile->lineNum ;

		if ( c = strchr ( inpFile->readBuf, '\n' ) ) {
			*c = 0 ;
		}

		//
		//	remove comments
		//

		if ( c = strstr ( inpFile->readBuf, "//" ) ) {
			*c = 0 ;
		}

		//
		//	blank line test
		//

		if ( RESULT_OK == ParserMatchPattern ( "~s*$", inpFile->readBuf, &match, 0 ) ) {
			continue ;
		}

		//
		//	handle preprocessor line
		//

		if  (RESULT_OK == ParserMatchPattern ( "~s*#ifdef~s+(~w+)~s*$",
			inpFile->readBuf, &match, &token ) ) {

			char symbol[ 80 ] ;

			if( RESULT_OK != ParserTknToStr( &token, symbol, sizeof(symbol ) ) ) {
				ExitOnInputError( inpFile, E_INVALID_PP ) ;
			}

#ifdef DEBUG_GETLINE
			printf ( "%s\n", inpFile->readBuf ) ;
#endif

			evalResult = EvalSymbol( inpFile->symTab, symbol )
				&& inpFile->ppInclude[inpFile->ppStackDepth] ;

			if( ++(inpFile->ppStackDepth) >= MAX_PP_STACK_DEPTH ) {
				ExitOnInputError( inpFile, E_INVALID_PP ) ;
			}

			inpFile->ppInclude[inpFile->ppStackDepth] = evalResult ;

			continue ;
		}

		if  (RESULT_OK == ParserMatchPattern ( "~s*#ifndef~s+(~w+)~s*$",
			inpFile->readBuf, &match, &token ) ) {

			char symbol[ 80 ] ;

			if( RESULT_OK != ParserTknToStr( &token, symbol, sizeof(symbol ) ) ) {
				ExitOnInputError( inpFile, E_INVALID_PP ) ;
			}

#ifdef DEBUG_GETLINE
			printf ( "%s\n", inpFile->readBuf ) ;
#endif

			evalResult = (!EvalSymbol( inpFile->symTab, symbol ) )
				&& inpFile->ppInclude[inpFile->ppStackDepth] ;

			if( ++(inpFile->ppStackDepth) >= MAX_PP_STACK_DEPTH ) {
				ExitOnInputError( inpFile, E_INVALID_PP ) ;
			}

			inpFile->ppInclude[inpFile->ppStackDepth] = evalResult ;

			continue ;
		}

		if  (RESULT_OK == ParserMatchPattern ( "~s*#endif", inpFile->readBuf, &match, 0 ) ) {
#ifdef DEBUG_GETLINE
			printf ("%s\n", inpFile->readBuf ) ;
#endif
			if ( --(inpFile->ppStackDepth) < 0 ) {
				ExitOnInputError ( inpFile, E_INVALID_PP ) ;
			}

			continue ;
		}

		if ( !inpFile->ppInclude[inpFile->ppStackDepth] ) {
#ifdef DEBUG_GETLINE
			printf ("DISCARD: %s\n", inpFile->readBuf ) ;
#endif
			continue ;
		}

		break ;
	}

	return RESULT_OK ;
}


//-----------------------------------------------------------------------------
//	                 Parsed command table management
//-----------------------------------------------------------------------------

//
//	Initialize the parsed command table
//
ATGEN_CmdTbl_t* ATGEN_NewCmdTbl ( void )
{
	ATGEN_CmdTbl_t* cmdTbl = (void*)calloc(1, sizeof( ATGEN_CmdTbl_t ) ) ;
	cmdTbl->nCmd = 0 ;
	cmdTbl->cmd  = 0 ;
	cmdTbl->maxParms = 0 ;
	cmdTbl->sortOrder = 0 ;

	return cmdTbl ;
}

//
//	Add command to parsed command table
//
ATGEN_Cmd_t* AddCmd ( ATGEN_CmdTbl_t* cmdTbl, char* cmdName )
{
	ATGEN_Cmd_t* cmd ;

	cmdTbl->cmd = realloc ( cmdTbl->cmd, ( cmdTbl->nCmd+1 ) * sizeof ( ATGEN_Cmd_t ) ) ;
	cmd = &cmdTbl->cmd[cmdTbl->nCmd++] ;

	cmd->cmdName		= strdup ( cmdName ) ;
	cmd->cmdSymbol		= 0 ;
	cmd->cmdType		= 0 ;
	cmd->handler		= 0 ;
	cmd->minParms		= 0 ;
	cmd->maxParms		= 0 ;
	cmd->validParms		= 0 ;
	cmd->validParmsIdx	= AT_RANGE_NOVALIDPARMS ;
	cmd->options		= 0 ;
	cmd->alias			= 0 ;
	cmd->cmdDescr		= 0 ;
	cmd->enumVal		= ~0 ;

	return cmd ;
}

//
//	Sort the command table alphabetically.  This is a bubble sort, which is
//	NOT an efficient way to sort things, but it's simple and it works!  Just
//	please don't adapt it to any real-time applications.
//
void SortCmdTbl ( const ATGEN_CmdTbl_t* cmdTbl )
{
	ATGEN_Cmd_t*  cmd ;
	UInt32	i, j, k ;

	assert ( cmdTbl->nCmd > 1 ) ;

	for ( i=0, cmd = cmdTbl->cmd; i<cmdTbl->nCmd; i++, cmd++ ) {
		cmdTbl->sortOrder[i] = i ;
	}

	for ( k=cmdTbl->nCmd-1; k>0; k-- ) {
		for ( i=0; i<k; i++ ) {
			char* str1 = cmdTbl->cmd[cmdTbl->sortOrder[i]].cmdName ;
			char* str2 = cmdTbl->cmd[cmdTbl->sortOrder[i+1]].cmdName ;
			if ( strcmp ( str1, str2 ) > 0 ) {
				j = cmdTbl->sortOrder[i] ;
				cmdTbl->sortOrder [i] = cmdTbl->sortOrder[i+1] ;
				cmdTbl->sortOrder [i+1] = j ;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	                        Hash table construction
//-----------------------------------------------------------------------------

typedef struct {
	UInt32	firstRow ;
	UInt32	lastRow ;
	void*	down ;
	void*	right ;
}	HashNode_t ;

HashNode_t* NewHashNode ( void )
{
	HashNode_t* node = (HashNode_t*) calloc ( 1, sizeof ( HashNode_t ) ) ;

	node->down     = 0 ;
	node->right    = 0 ;

	return node ;
}

void Hash ( char* list[], HashNode_t* parent, UInt32 col )
{
	HashNode_t*		thisNode ;
	HashNode_t*		topNode ;
	UInt32			i ;

	UInt32 startRow = parent->firstRow ;
	UInt32 endRow   = parent->lastRow ;

	char prevChar = list[startRow][col] ;
	char thisChar = prevChar ;

	topNode = NewHashNode ( ) ;
	parent->right = topNode ;

	thisNode = topNode ;

	thisNode->firstRow = startRow ;
	thisNode->lastRow  = startRow ;

	for ( i=startRow; i<=endRow; i++ ) {
		thisChar = list[i][col] ;
		if ( prevChar == thisChar ) {
			thisNode->lastRow = i ;
		}
		else {
			prevChar = thisChar ;
			thisNode->down = NewHashNode ( ) ;
			thisNode = (HashNode_t*) thisNode->down ;
			thisNode->firstRow = i ;
			thisNode->lastRow  = i ;
		}
	}

	thisNode = topNode ;

	if ( thisNode->firstRow < thisNode->lastRow ) {
		Hash ( list, thisNode, col+1 ) ;
	}

	while ( thisNode->down ) {
		thisNode = (HashNode_t*)thisNode->down ;
		if ( thisNode->firstRow < thisNode->lastRow ) {
			Hash ( list, thisNode, col+1 ) ;
		}
	}

}

void TraverseHashTblPrt (
	FILE*			fOut,
	HashNode_t*		node,
	UInt32			level,
	const ATGEN_CmdTbl_t* cmdTbl,
	UInt32*			cmdIdx )
{
	UInt32 i ;
	UInt32 j ;
	if ( !node ) return ;

	fprintf ( fOut, "//" ) ;
	for ( i=0; i<4*level; i++ ) fprintf ( fOut, " " ) ;

	fprintf ( fOut, "%d %d", node->firstRow, node->lastRow ) ;

	if ( !(HashNode_t*)node->right) {
		j = cmdTbl->sortOrder [ *cmdIdx] ;
		fprintf ( fOut, " %s", cmdTbl->cmd[j].cmdName ) ;
		*cmdIdx += 1 ;
	}

	fprintf ( fOut, "\n" ) ;

	if ( (HashNode_t*)node->right) {
		TraverseHashTblPrt ( fOut, (HashNode_t*)node->right, level+1, cmdTbl, cmdIdx ) ;
	}

	if ( (HashNode_t*)node->down ) {
		TraverseHashTblPrt ( fOut, (HashNode_t*)node->down, level, cmdTbl, cmdIdx ) ;
	}
}

UInt32 CountHashNodes ( HashNode_t* node )
{
	UInt32 count ;

	if ( !node ) return 0 ;
	count = 0 ;

	while ( node ) {
		count += CountHashNodes ( (HashNode_t*) node->right ) ;
		count += 1 ;
		node = (HashNode_t*)node->down ;
	}
	return count ;
}

void TraverseHashTbl (
	FILE*			fOut,
	HashNode_t*		node,
	const ATGEN_CmdTbl_t* cmdTbl,
	UInt32*			cmdIdx )
{
	UInt32	count ;
	UInt32	j ;

	if ( !node ) return ;

	count = CountHashNodes ( (HashNode_t*) node->right ) ;

	fprintf ( fOut, "\t{ %d, %d", node->firstRow, count ) ;
	if ( ! (HashNode_t*)node->down ) fprintf ( fOut, " | AT_ENDHASH" ) ;
	fprintf ( fOut, " } ," ) ;

	if ( !(HashNode_t*)node->right) {
		j = cmdTbl->sortOrder [ *cmdIdx] ;
		fprintf ( fOut, "\t\t//\t%s\n", cmdTbl->cmd[j].cmdName ) ;
		*cmdIdx += 1 ;
	}
	fprintf ( fOut, "\n" ) ;


	if ( (HashNode_t*)node->right) {
		TraverseHashTbl ( fOut, (HashNode_t*)node->right, cmdTbl, cmdIdx ) ;
	}

	if ( (HashNode_t*)node->down ) {
			TraverseHashTbl ( fOut, (HashNode_t*)node->down, cmdTbl, cmdIdx ) ;
	}
}


//----------------------------------------------------------------------------
//	                           Code generation
//-----------------------------------------------------------------------------

//
//	Write "header" comments to file.
//

void WriteFileHeader (
	FILE*	fOut )				//	file pointer
{
	fprintf ( fOut,
		"//---------------------------------------------------\n"
		"//\t\tDO NOT MODIFY THIS FILE!\n"
		"//\n"
		"//\t\tCreated %s %s by at_gen\n"
		"//---------------------------------------------------\n",
		__DATE__, __TIME__ ) ;
}

//
//	Open a file for write, and generate file header
//

void Label ( FILE* fOut, char* label )
{
	fprintf ( fOut, "//----------------------------------------\n" ) ;
	fprintf ( fOut, "//\t%s\n", label ) ;
	fprintf ( fOut, "//----------------------------------------\n" ) ;
}

FILE* InitFile (
	char* fName )
{
	FILE*	f ;

	f = fopen ( fName, "w" ) ;
	assert ( f ) ;

	WriteFileHeader ( f ) ;

	return f ;
}

//
//	Generate command-handler function prototypes.
//

void GenPrototypes (
	char*			fName,			//	filename
	const ATGEN_CmdTbl_t*	cmdTbl ) 		//	table of parsed AT commands
{
	FILE*		fOut ;
	UInt32		c ;
	UInt8		i ;

	fOut = InitFile ( fName ) ;
	assert ( fOut ) ;


	fprintf( fOut, "#ifndef __%s_HDR__\n", SYMNAME_CMD_TBL ) ;
	fprintf( fOut, "#define __%s_HDR__\n\n", SYMNAME_CMD_TBL ) ;

	for ( c=0; c<cmdTbl->nCmd; c++ ) {
		ATGEN_Cmd_t* cmd = &cmdTbl->cmd[cmdTbl->sortOrder[c]] ;

		fprintf ( fOut, "#define\t%s\t%d\n", cmd->cmdSymbol, cmd->enumVal ) ;

	}

	for ( c=0; c<cmdTbl->nCmd; c++ ) {
		ATGEN_Cmd_t* cmd = &cmdTbl->cmd[cmdTbl->sortOrder[c]] ;

		if( cmd->alias ) continue ;

		fprintf ( fOut, "extern AT_Status_t %s ( AT_CmdId_t cmdId, UInt8 ch, UInt8 accessType",
			cmd->handler
			) ;

		for ( i=0; i< cmd->maxParms; i++ ) {
			fprintf ( fOut, ", const UInt8* _P%d", i+1 ) ;
		}

		fprintf ( fOut, ") ;\n" ) ;
	}

	fprintf( fOut, "\n#endif // __%s_HDR__\n", SYMNAME_CMD_TBL ) ;

	fclose ( fOut ) ;
}

//
//	Generate command-handler stubs
//

void GenStubs (
	char*			fName,			//	filename
	const ATGEN_CmdTbl_t*	cmdTbl )		//	table of parsed AT commands
{
	FILE*		fOut ;
	UInt32		c ;
	UInt8		i ;

	fOut = InitFile ( fName ) ;
	assert ( fOut ) ;

	for ( c=0; c<cmdTbl->nCmd; c++ ) {
		ATGEN_Cmd_t* cmd = &cmdTbl->cmd[cmdTbl->sortOrder[c]] ;
		if( cmd->alias ) continue ;

		fprintf ( fOut, "#ifndef IMP_%s\n", cmd->handler ) ;

		fprintf ( fOut, "AT_Status_t %s ( AT_CmdId_t cmdId, UInt8 ch, UInt8 accessType", cmd->handler ) ;

		for ( i=0; i< cmd->maxParms; i++ ) {
			fprintf ( fOut, ", const UInt8* _P%d", i+1 ) ;
		}

		fprintf ( fOut, " )\n" ) ;
		fprintf ( fOut, "{\n" ) ;

		fprintf ( fOut, "\tprintf ( \"cmdId enum %%d %%d %%s\\n\", cmdId, %s, AT_GetCmdName(%s));\n", cmd->cmdSymbol, cmd->cmdSymbol ) ;
		fprintf ( fOut, "\tprintf ( \"\\n\t==>%s\\n\" ) ;\n" , cmd->handler ) ;
		fprintf ( fOut, "\tprintf ( \"\t   Name = %%s\\n\", AT_GetCmdName ( cmdId ) ) ;\n");
		fprintf ( fOut, "\tswitch ( accessType ) {\n" ) ;
		fprintf ( fOut, "\tcase AT_ACCESS_REGULAR:\n" ) ;
		fprintf ( fOut, "\t\tprintf ( \"\t   AccessType = AT_ACCESS_REGULAR\\n\") ;\n");
		fprintf ( fOut, "\t\tbreak;\n" ) ;
		fprintf ( fOut, "\tcase AT_ACCESS_READ:	\n" ) ;
		fprintf ( fOut, "\t\tprintf ( \"\t   AccessType = AT_ACCESS_READ\\n\") ;\n");
		fprintf ( fOut, "\t\tbreak;\n" ) ;
		fprintf ( fOut, "\tcase AT_ACCESS_TEST:	\n" ) ;
		fprintf ( fOut, "\t\tprintf ( \"\t   AccessType = AT_ACCESS_TEST\\n\") ;\n");
		fprintf ( fOut, "\t\tbreak;\n" ) ;
		fprintf ( fOut, "\tdefault:\n" ) ;
		fprintf ( fOut, "\t\tprintf ( \"\t   AccessType = UNKNOWN\\n\") ;\n");
		fprintf ( fOut, "\t\tbreak;\n" ) ;
		fprintf ( fOut, "\t}\n" ) ;

		for ( i=0; i<cmd->maxParms; i++ ) {
			fprintf ( fOut, "\tprintf ( \"\t   _P%d = %%s\\n\", _P%d );\n", i+1 ,i+1 ) ;
		}

		fprintf ( fOut, "\tAT_CmdRspOK(ch) ;\n" ) ;
		fprintf ( fOut, "\treturn AT_STATUS_DONE;\n" ) ;
		fprintf ( fOut, "}\n" ) ;

		fprintf ( fOut, "#endif\n" ) ;
	}

	fclose ( fOut ) ;
}

//
//	Generate run-time command parser table
//

void WriteHashTbl (
	FILE*			fOut,
	const ATGEN_CmdTbl_t*	cmdTbl )
{
	char**		list ;
	HashNode_t* rootNode ;
	UInt32		i ;
	UInt32		cmdIdx ;

	list = (char**) calloc ( cmdTbl->nCmd, sizeof ( char* ) ) ;

	for ( i=0; i<cmdTbl->nCmd; i++ ) {
		UInt32 j = cmdTbl->sortOrder [i] ;
		list [i] = strdup ( cmdTbl->cmd[j].cmdName ) ;
	}

	rootNode = NewHashNode ( ) ;

	rootNode->firstRow = 0 ;
	rootNode->lastRow  = cmdTbl->nCmd-1 ;

	Hash ( list, rootNode, 0 ) ;

	cmdIdx = 0 ;

	Label ( fOut, "Hash table information" ) ;
	TraverseHashTblPrt ( fOut, rootNode, 0, cmdTbl, &cmdIdx ) ;

	fprintf ( fOut, "static const AT_HashBlk_t %s [] = {\n", SYMNAME_HASH_TBL ) ;

	cmdIdx = 0 ;
	TraverseHashTbl       ( fOut, (HashNode_t*)rootNode->right, cmdTbl, &cmdIdx ) ;

	fprintf ( fOut, "} ;\n" ) ;
}

//-----------------------------------------------------------------------------
//	TknToNewStr
//-----------------------------------------------------------------------------
//	Converts a token to a string, allocating memory for the string.  This is
//	a PC-only function.
//-----------------------------------------------------------------------------

char* TknToNewStr ( ParserToken_t* token )
{
	char*	c ;
	char*	p ;
	UInt32  i ;

	if ( token->tknLen == 0 ) {
		return 0 ;
	}

	p = c = calloc ( token->tknLen+1,sizeof(char));

	for ( i=0; i<token->tknLen; i++ ) {
		*p++ = *token->tknPtr++ ;
	}

	return c ;
}

//-----------------------------------------------------------------------------
//	ParseRangeSpec
//-----------------------------------------------------------------------------
//	Parse a range-specification string.
//-----------------------------------------------------------------------------

typedef struct {
	UInt8		parmSpec [10000] ;
	UInt32		parmSpecLen ;
	char*		strTab [10000] ;	// todo tbd fix me allocate dynamically
	UInt32		strTabLen ;
}	RangeSpec_t ;

//
//	Pack a range-spec value into UInt8 array
//
void Pack ( UInt8 rangeType, UInt32 v1, RangeSpec_t* rangeSpec )
{
	UInt32	v = 0 ;
	UInt8	i, n ;
	UInt32	mask ;
	UInt8	shift ;

	switch ( rangeType ) {
	case AT_RANGE_SCALAR_UINT32:
	case AT_RANGE_MINMAX_UINT32:
	case AT_RANGE_STRING:
		n=4 ;
		shift=24;
		mask=0xff000000;
		break ;
	case AT_RANGE_SCALAR_UINT16:
	case AT_RANGE_MINMAX_UINT16:
		n=2 ;
		shift=8;
		mask=0xff00;
		break;
	case AT_RANGE_SCALAR_UINT8:
	case AT_RANGE_MINMAX_UINT8:
		n=1 ;
		shift=0;
		mask=0xff;
		break;
	}
	for ( i=0; i<n; i++ ) {
		rangeSpec->parmSpec[rangeSpec->parmSpecLen++] = (UInt8)((v1&mask)>>shift) ;
		mask >>= 8 ;
		shift -=8 ;
	}
}

Result_t ParseRangeSpec (
	ATGEN_Cmd_t*			cmd,
	RangeSpec_t*	rangeSpec )
{
	ParserMatch_t	match ;
	ParserToken_t	token [3] ;
	char**		parms ;
	UInt32		i ;
	UInt32		v1, v2 ;
	char*		lostPtr ;

	parms = calloc ( cmd->maxParms, sizeof ( char* ) ) ;

	ParserInitMatch( cmd->validParms, &match ) ;

	//
	//	separate into parameters, i.e., "(p1),(p2),(p3)" -> "p1", "p2", "p3"
	//
	for ( i=0; i<cmd->maxParms; i++ ) {

		if ( RESULT_OK == ParserMatch ( "~s*~c((~C)+)~c)~s*(~c,?)~s*", &match, token ) ) {
			parms[i] = TknToNewStr ( &token[0] ) ;
		}

		else {
			return RESULT_ERROR ;
		}
	}

	for ( i=0; i<cmd->maxParms; i++ ) {

		ParserInitMatch( parms[i], &match ) ;

		for ( ;; ) {

			if ( RESULT_OK == ParserMatch ( "(~d+)-(~d+)(~c,?)~s*", &match, token )) {

				ParserTknToUInt(&token[0], &v1 ) ;
				ParserTknToUInt(&token[1], &v2 ) ;

				if ( v1 <= 0xff && v2 <= 0xff ) {
					rangeSpec->parmSpec[rangeSpec->parmSpecLen++] = AT_RANGE_MINMAX_UINT8 ;
					Pack ( AT_RANGE_MINMAX_UINT8, v1, rangeSpec ) ;
					Pack ( AT_RANGE_MINMAX_UINT8, v2, rangeSpec ) ;
				}
				else if ( v1 <= 0xffff && v2 <= 0xffff ) {
					rangeSpec->parmSpec[rangeSpec->parmSpecLen++] = AT_RANGE_MINMAX_UINT16 ;
					Pack ( AT_RANGE_MINMAX_UINT16, v1, rangeSpec ) ;
					Pack ( AT_RANGE_MINMAX_UINT16, v2, rangeSpec ) ;
				}
				else {
					rangeSpec->parmSpec[rangeSpec->parmSpecLen++] = AT_RANGE_MINMAX_UINT32 ;
					Pack ( AT_RANGE_MINMAX_UINT32, v1, rangeSpec ) ;
					Pack ( AT_RANGE_MINMAX_UINT32, v2, rangeSpec ) ;
				}


				if ( token[2].tknLen > 0 ) continue ;

				if ( RESULT_OK != ParserMatch ( "~s*$", &match, 0 ) ) {
					return RESULT_ERROR ;
				}
				break ;
			}

			else if ( RESULT_OK == ParserMatch ( "(~d+)(~c,?)~s*", &match, token )) {

				ParserTknToUInt(&token[0], &v1 ) ;

				if ( v1 <= 0xff ) {
					rangeSpec->parmSpec[rangeSpec->parmSpecLen++] = AT_RANGE_SCALAR_UINT8 ;
					Pack ( AT_RANGE_MINMAX_UINT8, v1, rangeSpec ) ;
				}
				else if ( v1 <= 0xffff ) {
					rangeSpec->parmSpec[rangeSpec->parmSpecLen++] = AT_RANGE_SCALAR_UINT16 ;
					Pack ( AT_RANGE_MINMAX_UINT16, v1, rangeSpec ) ;
				}
				else {
					rangeSpec->parmSpec[rangeSpec->parmSpecLen++] = AT_RANGE_SCALAR_UINT32 ;
					Pack ( AT_RANGE_MINMAX_UINT32, v1, rangeSpec ) ;
				}

				if ( token[1].tknLen > 0 ) continue ;
				if ( RESULT_OK != ParserMatch ( "~s*$", &match, 0 ) ) {
					return RESULT_ERROR ;
				}
				break ;
			}

			else if ( RESULT_OK == ParserMatch ( "~s*?~s*", &match, 0 )) {

				rangeSpec->parmSpec[rangeSpec->parmSpecLen++] = AT_RANGE_NOCHECK ;

				break ;
			}

			else if ( RESULT_OK == ParserMatch ( "~c\"(~C\"+)~c\"(~c,?)~s*", &match, token )) {

				lostPtr = TknToNewStr ( &token[0] ) ;

				rangeSpec->parmSpec[rangeSpec->parmSpecLen++] = AT_RANGE_STRING ;
				v1 = rangeSpec->strTabLen ;
					Pack ( AT_RANGE_STRING, v1, rangeSpec ) ;
				rangeSpec->strTab[rangeSpec->strTabLen++] = strdup ( lostPtr ) ;

				if ( token[1].tknLen > 0 ) continue ;
				if ( RESULT_OK != ParserMatch ( "~s*$", &match, 0 ) ) {
					return RESULT_ERROR ;
				}
				break ;
			}

			else {
				return RESULT_ERROR ;
			}
		}

		rangeSpec->parmSpec[rangeSpec->parmSpecLen++] = AT_RANGE_ENDRANGE ;
	}

	for ( i=0; i<cmd->maxParms; i++ ) {
		free ( parms[i] ) ;
	}
	free ( parms ) ;

	return RESULT_OK ;
}

void GetParmRanges (
	const ATGEN_CmdTbl_t*	cmdTbl,
	RangeSpec_t*	rangeSpec
)
{
	UInt32		c ;

	rangeSpec->parmSpecLen  = 0 ;
	rangeSpec->strTabLen    = 0 ;

	for ( c=0; c<cmdTbl->nCmd; c++ ) {
		ATGEN_Cmd_t* cmd = &cmdTbl->cmd[cmdTbl->sortOrder[c]] ;
		if ( cmd->validParms ) {
			cmd->validParmsIdx = rangeSpec->parmSpecLen ;
			if ( RESULT_OK != ParseRangeSpec ( cmd, rangeSpec ) ) {
				ExitOnCmdError ( cmd->cmdName, "Invalid range spec" ) ;
			}
		}
		else {
			cmd->validParmsIdx = AT_RANGE_NOVALIDPARMS ;
		}
	}
}

void WriteParmRanges (
	FILE*			fOut,
	RangeSpec_t*	rangeSpec
)
{
	UInt32		i, j ;
	UInt32		nCmd = 0 ;

	fprintf ( fOut, "static const UInt8 %s[] = { \n", SYMNAME_RANGE_VAL ) ;

	if( rangeSpec->parmSpecLen == 0 ) {
		fprintf( fOut, " 0 " ) ;
	}

	for ( i=0; i<rangeSpec->parmSpecLen;  ) {
		switch ( rangeSpec->parmSpec[i++] ) {
		case AT_RANGE_MINMAX_UINT32:
			fprintf ( fOut, "/* %d */", i-1 ) ;
			fprintf ( fOut, "\tAT_RANGE_MINMAX_UINT32, ") ;
			for ( j=0; j<8; j++ ) {
				fprintf ( fOut, "0x%2.2x, ", rangeSpec->parmSpec[i++] ) ;
			}
			fprintf ( fOut, "\n" ) ;
			break ;
		case AT_RANGE_MINMAX_UINT16:
			fprintf ( fOut, "/* %d */", i-1 ) ;
			fprintf ( fOut, "\tAT_RANGE_MINMAX_UINT16, ") ;
			for ( j=0; j<4; j++ ) {
				fprintf ( fOut, "0x%2.2x, ", rangeSpec->parmSpec[i++] ) ;
			}
			fprintf ( fOut, "\n" ) ;
			break ;
		case AT_RANGE_MINMAX_UINT8:
			fprintf ( fOut, "/* %d */", i-1 ) ;
			fprintf ( fOut, "\tAT_RANGE_MINMAX_UINT8, ") ;
			for ( j=0; j<2; j++ ) {
				fprintf ( fOut, "0x%2.2x, ", rangeSpec->parmSpec[i++] ) ;
			}
			fprintf ( fOut, "\n" ) ;
			break ;
		case AT_RANGE_SCALAR_UINT32:
			fprintf ( fOut, "/* %d */", i-1 ) ;
			fprintf ( fOut, "\tAT_RANGE_SCALAR_UINT32, ") ;
			for ( j=0; j<4; j++ ) {
				fprintf ( fOut, "0x%2.2x, ", rangeSpec->parmSpec[i++] ) ;
			}
			fprintf ( fOut, "\n" ) ;
			break ;
		case AT_RANGE_SCALAR_UINT16:
			fprintf ( fOut, "/* %d */", i-1 ) ;
			fprintf ( fOut, "\tAT_RANGE_SCALAR_UINT16, ") ;
			for ( j=0; j<2; j++ ) {
				fprintf ( fOut, "0x%2.2x, ", rangeSpec->parmSpec[i++] ) ;
			}
			fprintf ( fOut, "\n" ) ;
			break ;
		case AT_RANGE_SCALAR_UINT8:
			fprintf ( fOut, "/* %d */", i-1 ) ;
			fprintf ( fOut, "\tAT_RANGE_SCALAR_UINT8, ") ;
			for ( j=0; j<1; j++ ) {
				fprintf ( fOut, "0x%2.2x, ", rangeSpec->parmSpec[i++] ) ;
			}
			fprintf ( fOut, "\n" ) ;
			break ;
		case AT_RANGE_STRING:
			fprintf ( fOut, "/* %d */", i-1 ) ;
			fprintf ( fOut, "\tAT_RANGE_STRING, " ) ;
			for ( j=0; j<4; j++ ) {
				fprintf ( fOut, "0x%2.2x, ", rangeSpec->parmSpec[i++] ) ;
			}
			fprintf ( fOut, "\n" ) ;
			break ;
		case AT_RANGE_NOCHECK:
			fprintf ( fOut, "/* %d */", i-1 ) ;
			fprintf ( fOut, "\tAT_RANGE_NOCHECK,\n" ) ;
			break ;
		case AT_RANGE_ENDRANGE:
			fprintf ( fOut, "/* %d */", i-1 ) ;
			fprintf ( fOut, "\tAT_RANGE_ENDRANGE,\n" ) ;
			break ;
		default:
			fclose ( fOut ) ;
			assert(0);
		}
	}

	fprintf ( fOut, "};\n" ) ;

	if ( rangeSpec->strTabLen > 0 ) {
		fprintf ( fOut, "static const UInt8* %s[] = {\n", SYMNAME_RANGE_STR ) ;
		for ( i=0; i<rangeSpec->strTabLen; i++ ) {
			fprintf ( fOut, "\t(UInt8*)\"%s\",\n", rangeSpec->strTab[i] ) ;
		}
		fprintf ( fOut, "} ; \n" ) ;
	}

	else {
		fprintf ( fOut, "const UInt8* %s[] = { 0 } ;\n", SYMNAME_RANGE_STR ) ;
	}
}

void GenCmdTbl (
	char*			fName,			//	filename
	const ATGEN_CmdTbl_t*	cmdTbl )		//	table of parsed AT commands
{
	FILE*			fOut ;
	UInt32			c ;
	UInt32			options ;
	RangeSpec_t		rangeSpec ;

	fOut = InitFile ( fName ) ;
	assert ( fOut ) ;

	fprintf ( fOut, "\n" ) ;

	fprintf ( fOut, "#define %s %d\n", SYMNAME_NUM_CMDS, cmdTbl->nCmd ) ;
	fprintf ( fOut, "#define %s %d\n\n", SYMNAME_MAX_PARMS, cmdTbl->maxParms ) ;

	GetParmRanges ( cmdTbl, &rangeSpec ) ;

	fprintf ( fOut, "static const AT_Cmd_t %s [] = {\n", SYMNAME_CMD_TBL ) ;

	for ( c=0; c<cmdTbl->nCmd; c++ ) {

		ATGEN_Cmd_t* cmd = &cmdTbl->cmd[c] ;

		if( cmd->cmdDescr ) {
			fprintf( fOut, "\n//\tAT%s:\t%s\n", cmd->cmdName, cmd->cmdDescr ) ;
		}

		fprintf ( fOut, "\t\t{ \t\"%s\",\n", cmd->cmdName ) ;

		if( cmd->alias ) {
			Boolean found = FALSE ;
			UInt32 c2 ;

			for( c2=0; c2<cmdTbl->nCmd; c2++ ) {
				if ( c2!=c ) {

					ATGEN_Cmd_t* cmd2 = &cmdTbl->cmd[c2] ;

					if ( !strcmp( cmd->alias, cmd2->cmdSymbol ) ) {
						cmd = cmd2 ;
						found = TRUE ;
						break ;
					}
				}
			}

			if( !found ) {
				ExitOnCmdError ( cmd->cmdName, "Alias not found" ) ;
			}
		}

		if ( cmd->validParms ) {
			char* s = cmd->validParms ;
			fprintf ( fOut, "\t\t\t\"" ) ;
			while ( *s ) {
				char t = *s++ ;
				if ( t != '\"' ) {
					fprintf ( fOut, "%c", t ) ;
				}
			}
			fprintf ( fOut, "\",\n" ) ;
		}
		else {
			fprintf ( fOut, "\t\t\t0,\n" ) ;
		}

		if( !cmd->cmdType )  ExitOnCmdError( cmd->cmdName, "cmdType not specified" ) ;
		if( !cmd->handler )  ExitOnCmdError( cmd->cmdName, "handler not specified" ) ;

		fprintf ( fOut, "\t\t\t%s,\n"
			"\t\t\t(AT_CmdHndlr_t)%s,\n"
			"\t\t\t%d,\n"
			"\t\t\t%d,\n"
			"\t\t\t%d,\n"
			"\t\t\t",
			cmd->cmdType,
			cmd->handler,
			cmd->validParmsIdx,
			cmd->minParms,
			cmd->maxParms
			) ;

		options = cmd->options ;

		if ( cmd->options == 0 ) {
			fprintf ( fOut, "0" ) ;
		}
		else {
			if ( options & AT_OPTION_NO_PIN ) {
				fprintf ( fOut, "AT_OPTION_NO_PIN" ) ;
				options &= ~AT_OPTION_NO_PIN ;
				if ( options ) {
					fprintf ( fOut, " | " ) ;
				}
			}
			if ( options & AT_OPTION_NO_PWR ) {
				fprintf ( fOut, "AT_OPTION_NO_PWR" ) ;
				options &= ~AT_OPTION_NO_PWR ;
				if ( options ) {
					fprintf ( fOut, " | " ) ;
				}
			}
			if ( options & AT_OPTION_USER_DEFINED_TEST_RSP ) {
				fprintf ( fOut, "AT_OPTION_USER_DEFINED_TEST_RSP" ) ;
				options &= ~AT_OPTION_USER_DEFINED_TEST_RSP ;
				if ( options ) {
					fprintf ( fOut, " | " ) ;
				}
			}
			if ( options & AT_OPTION_WAIT_PIN_INIT ) {
				fprintf ( fOut, "AT_OPTION_WAIT_PIN_INIT" ) ;
				options &= ~AT_OPTION_WAIT_PIN_INIT ;
				if ( options ) {
					fprintf ( fOut, " | " ) ;
				}
			}
			if ( options & AT_OPTION_HIDE_NAME ) {
				fprintf ( fOut, "AT_OPTION_HIDE_NAME" ) ;
				options &= ~AT_OPTION_HIDE_NAME ;
				if ( options ) {
					fprintf ( fOut, " | " ) ;
				}
			}
		}
		fprintf ( fOut, "\n\t\t},\n" ) ;
	}

	fprintf ( fOut, "\n//\n//\t Terminate list\n//\n" ) ;

	fprintf ( fOut, "\t\t{ 0, 0, 0, 0, 0 },\n" ) ;
	fprintf ( fOut, "} ;\n\n" ) ;

	WriteHashTbl    ( fOut, cmdTbl ) ;

	//
	//	hash redirect table
	//
	fprintf( fOut, "static const AT_CmdId_t %s [] = {\n ", SYNNAME_HASH_REDIR ) ;
	for( c=0; c< cmdTbl->nCmd; c++ ) {
		fprintf( fOut, "\t %4d, ", cmdTbl->sortOrder[c] ) ;
		if( c%10==9 ) fprintf( fOut, "\n" ) ;
	}
	fprintf( fOut, "\n} ;\n\n" ) ;

	WriteParmRanges ( fOut, &rangeSpec ) ;

	fclose ( fOut ) ;
}

//
//	Generate ExecCmd call arglist
//

void GenExecCmdCall (
	char*			fName,			//	filename
	const ATGEN_CmdTbl_t*	cmdTbl )		//	table of parsed AT commands
{
	FILE*		fOut ;
	UInt8		i ;

	fOut = InitFile ( fName ) ;
	assert ( fOut ) ;

	fprintf( fOut, "\treturn cmd->cmdHndlr( cmdId, ch, accType,\n" ) ;

	for( i=0; i<cmdTbl->maxParms; i++ ) {
		fprintf( fOut, "\t\t*(cmdParmPtr+%d)", i ) ;
		if( i<cmdTbl->maxParms-1 ) {
			fprintf( fOut, "," ) ;
		}
		fprintf( fOut, "\n" ) ;
	}

	fprintf( fOut, "\t); \n" ) ;

	fclose ( fOut ) ;
}

//-----------------------------------------------------------------------------
//	                     Parse the AT command table
//-----------------------------------------------------------------------------

//
//	Case-insensitive string compare - copied from at_api.c
//
static UInt8 strcmp_nocase( const char* s1, const char* s2 )
{
	if( strlen( s1 ) != strlen( s2 ) ) return 1 ;
	while( *s1 ) {
		if( toupper(*s1++) != toupper(*s2++) ) return 1 ;
	}
	return 0 ;
}

void ATGEN_ParseInputFile ( char * fileName, char * symTab, ATGEN_CmdTbl_t * cmdTbl )
{
	ParserMatch_t	match ;				//	pattern-matching results
	ParserToken_t	token [3] ;			//	pattern-matching tokens

	InpFile_t*		inpFile ;			//	input file

	char			bigBuf [1024] ;
	UInt32			tmpVal ;

	//
	//	enumerate keywords
	//

	enum {
		KWD_CMDID=1,
		KWD_CMDTYPE_BASIC,
		KWD_CMDTYPE_EXTENDED,
		KWD_CMDTYPE_SPECIAL,
		KWD_HANDLER_FUNC,
		KWD_MINPARMS,
		KWD_MAXPARMS,
		KWD_VALIDPARMS,
		KWD_OPTIONS,
		KWD_ALIAS,
		KWD_CMDDESCR
	} ;

	//
	//	define table of keywords
	//

	typedef struct {
		UInt32		val ;
		const char*	pattern ;
	}	Keyword_t ;

	//
	//	instantiate table of keywords and their associated patterns
	//

	const Keyword_t kwdList [] = {
		{	KWD_CMDID,				"~s*cmdId~s*=~s*(~w+)~s*"			},
		{	KWD_CMDTYPE_BASIC,		"~s*cmdType~s*=~s*basic~s*"			},
		{	KWD_CMDTYPE_EXTENDED,	"~s*cmdType~s*=~s*extended~s*"		},
		{	KWD_CMDTYPE_SPECIAL,	"~s*cmdType~s*=~s*special~s*"		},
		{	KWD_HANDLER_FUNC,		"~s*handler~s*=~s*(~w+)~s*"			},
		{	KWD_MINPARMS,			"~s*minParms~s*=~s*(~d+)~s*"		},
		{	KWD_MAXPARMS,			"~s*maxParms~s*=~s*(~d+)~s*"		},
		{	KWD_VALIDPARMS,			"~s*validParms~s*=~s*"				},
		{	KWD_OPTIONS,			"~s*options~s*=~s*"					},
		{	KWD_ALIAS,				"~s*alias~s*=~s*(~w+)~s*"			},
		{	KWD_CMDDESCR,			"~s*cmdDescr~s*=~s*"				},
		{   0,						0									}
	} ;

	ATGEN_Cmd_t* currCmd ;

	if ( ! ( inpFile = OpenInputFile ( fileName, symTab ) ) ) {
		printf ( "Error opening %s for input\n", fileName ) ;
		exit ( 1 ) ;
	}

	for ( ;; ) {

		//
		//	get input line
		//

		if ( RESULT_OK != GetLine ( inpFile ) ) {
			break ;
		}

		//
		//	check for format "cmd: ...";
		//

		if ( RESULT_OK != ParserMatchPattern (
			"(~l/A-Za-z0-9&*+_?/+)~s*:~s*",
			inpFile->readBuf,
			&match,
			token ) ) {
			ExitOnInputError ( inpFile, E_NOCMD ) ;
		}

		//
		//	add command to table
		//

		ParserTknToStr ( &token[0], bigBuf, sizeof(bigBuf) ) ;

		//	check duplicate name
		{
			UInt32 i ;
			for( i=0; i<cmdTbl->nCmd; i++ ) {
				if( !strcmp_nocase( cmdTbl->cmd[i].cmdName, bigBuf ) ) {
					ExitOnCmdError( cmdTbl->cmd[i].cmdName, "Duplicate command" ) ;
				}
			}
		}

		currCmd = AddCmd ( cmdTbl, bigBuf ) ;

		//
		//	scan for attributes
		//

		for ( ;; ) {

			const Keyword_t* kwd = kwdList ;

			Boolean found = FALSE ;

			while ( kwd->pattern ) {

				//
				//	check for format "keyword=value"
				//

				if ( RESULT_OK == ParserMatch ( kwd->pattern, &match, token ) ) {

					found = TRUE ;

					switch ( kwd->val ) {

					case KWD_CMDID:
						ParserTknToStr ( &token[0], bigBuf, sizeof(bigBuf) ) ;
						currCmd->cmdSymbol = strdup ( bigBuf ) ;
						break ;

					case KWD_CMDTYPE_BASIC:
						currCmd->cmdType = "AT_CMDTYPE_BASIC" ;
						break ;

					case KWD_CMDTYPE_EXTENDED:
						currCmd->cmdType = "AT_CMDTYPE_EXTENDED" ;
						break ;

					case KWD_CMDTYPE_SPECIAL:
						currCmd->cmdType = "AT_CMDTYPE_SPECIAL" ;
						break ;

					case KWD_HANDLER_FUNC:
						ParserTknToStr ( &token[0], bigBuf, sizeof(bigBuf) ) ;
						currCmd->handler = strdup ( bigBuf ) ;
						break ;

					case KWD_MINPARMS:
						ParserTknToStr ( &token[0], bigBuf, sizeof(bigBuf) ) ;
						ParserStrToInt ( bigBuf, &tmpVal ) ;
						currCmd->minParms = tmpVal ;
						if ( tmpVal > cmdTbl->maxParms ) cmdTbl->maxParms = tmpVal ;
						break ;

					case KWD_MAXPARMS:
						ParserTknToStr ( &token[0], bigBuf, sizeof(bigBuf) ) ;
						ParserStrToInt ( bigBuf, &tmpVal ) ;
						currCmd->maxParms = tmpVal ;
						if ( tmpVal > cmdTbl->maxParms ) cmdTbl->maxParms = tmpVal ;
						break ;

					case KWD_VALIDPARMS:

						if ( RESULT_OK == ParserMatch ( "~s*(~c(~C)+~c))", &match, token ) ) {
							ParserTknToStr ( &token[0], bigBuf, sizeof(bigBuf) ) ;
							currCmd->validParms = strdup (bigBuf);
						}

						for ( ;; ) {
							if ( RESULT_OK == ParserMatch ( "~s*,~s*(~c(~C)+~c))", &match, token ) ) {
								ParserTknToStr ( &token[0], bigBuf, sizeof(bigBuf) ) ;
								currCmd->validParms = realloc ( currCmd->validParms,
									2+strlen(bigBuf)+strlen(currCmd->validParms));
								strcat( currCmd->validParms, "," ) ;
								strcat ( currCmd->validParms, bigBuf ) ;
							}
							else {
								break ;
							}
						}

						break ;

					case KWD_OPTIONS:
						for ( ;; ) {
							if ( RESULT_OK == ParserMatch ( "~s*NO_PIN~s*", &match, 0 ) ) {
								if ( currCmd->options & AT_OPTION_NO_PIN ) {
									ExitOnInputError ( inpFile, E_OPTION ) ;
								}
								currCmd->options |= AT_OPTION_NO_PIN ;
							}

							else if ( RESULT_OK == ParserMatch ( "~s*NO_PWR~s*", &match, 0 ) ) {
								if ( currCmd->options & AT_OPTION_NO_PWR ) {
									ExitOnInputError ( inpFile, E_OPTION ) ;
								}
								currCmd->options |= AT_OPTION_NO_PWR ;
							}

							else if ( RESULT_OK == ParserMatch ( "~s*USER_DEFINED_TEST_RSP~s*", &match, 0 ) ) {
								if ( currCmd->options & AT_OPTION_USER_DEFINED_TEST_RSP ) {
									ExitOnInputError ( inpFile, E_OPTION ) ;
								}
								currCmd->options |= AT_OPTION_USER_DEFINED_TEST_RSP ;
							}

							else if ( RESULT_OK == ParserMatch ( "~s*WAIT_PIN_INIT~s*", &match, 0 ) ) {
								if ( currCmd->options & AT_OPTION_WAIT_PIN_INIT ) {
									ExitOnInputError ( inpFile, E_OPTION ) ;
								}
								currCmd->options |= AT_OPTION_WAIT_PIN_INIT ;
							}

							else if ( RESULT_OK == ParserMatch ( "~s*HIDE_NAME~s*", &match, 0 ) ) {
								if ( currCmd->options & AT_OPTION_HIDE_NAME ) {
									ExitOnInputError ( inpFile, E_OPTION ) ;
								}
								currCmd->options |= AT_OPTION_HIDE_NAME ;
							}
							else {
								ExitOnInputError ( inpFile, E_OPTION ) ;
							}

							if ( RESULT_OK != ParserMatch ( "~s*|~s*", &match, 0 ) ) {
								break ;
							}
						}

						break ;

					case KWD_ALIAS:
						ParserTknToStr ( &token[0], bigBuf, sizeof(bigBuf) ) ;
						currCmd->alias = strdup ( bigBuf ) ;
						break ;

					case KWD_CMDDESCR:
						if ( RESULT_OK == ParserMatch ( "~s*~c\"(~C\"+)~c\"~s*", &match, token ) ) {
							ParserTknToStr ( &token[0], bigBuf, sizeof(bigBuf) ) ;
							currCmd->cmdDescr = strdup( bigBuf ) ;
						}

						else {
							ExitOnInputError ( inpFile, E_OPTION ) ;
						}

					}

					break ;
				}

				++kwd ;
			}

			if ( !found ) {
				ExitOnInputError ( inpFile, E_KEYWORD ) ;
			}

			//
			//	if comma at end of line the line continues
			//

			inpFile->cmdContinue = FALSE ;

			if ( RESULT_OK == ParserMatch ( "~s*,~s*$", &match, 0 ) ) {

				inpFile->cmdContinue = TRUE ;

				if ( RESULT_OK != GetLine ( inpFile ) ) {
					ExitOnInputError ( inpFile, E_CONTINUE ) ;
				}

				//
				//	this will 'reset' the string pointer in 'match' to
				//	the start of the new line
				//
				ParserInitMatch( inpFile->readBuf, &match ) ;

				continue ;
			}

			//
			//	if comma present the attribute list continues
			//

			else if ( RESULT_OK == ParserMatch ( "~s*,~s*", &match, 0 ) ) {
				continue ;
			}

			//
			//	if anything else there is a syntax error
			//

			else if ( RESULT_OK != ParserMatch ( "~s*$", &match, 0 ) ) {
				ExitOnInputError ( inpFile, E_GARBAGE ) ;
				exit(1) ;
			}

			break ;
		}
	}

	if( inpFile->ppStackDepth != 0 ) {
		ExitOnInputError( inpFile, E_INVALID_PP ) ;
	}

	//
	//	sort the command table alphabetically
	//
	cmdTbl->sortOrder = calloc ( cmdTbl->nCmd, sizeof ( UInt32 ) ) ;

	SortCmdTbl ( cmdTbl ) ;

	fclose( inpFile->file ) ;
}

void SerializeStr( FILE * f, UInt8 * str )
{
	UInt16 len ;

	if( str ) {
		len = 1 + strlen( str ) ;
		fwrite( &len, 1, sizeof( len ) , f ) ;
		fwrite( str, len, 1, f ) ;
	}

	else {
		len = 0 ;
		fwrite( &len, 1, sizeof( len ) , f ) ;
	}
}

void ATGEN_SerializeCmdTbl( ATGEN_CmdTbl_t * cmdTbl, char * srcdir )
{
	UInt32 i ;
	ATGEN_Cmd_t * cmd ;
	char fname[80] ;
	FILE * f ;

	strcpy( fname, srcdir ) ;
	strcat( fname, "/" ) ;
	strcat( fname, FNAME_SERIALIZED ) ;

	f = fopen( fname, "wb" ) ;
	if( !f ) {
		printf( " Error creating serialization file\n" ) ;
		exit( 1 ) ;
	}

	fwrite( &cmdTbl->nCmd, 1, sizeof( cmdTbl->nCmd ) , f ) ;
	fwrite( &cmdTbl->maxParms, 1, sizeof( cmdTbl->maxParms ), f ) ;

	cmd = cmdTbl->cmd ;

	for( i=0; i<cmdTbl->nCmd; i++, cmd++ ) {
		SerializeStr( f, cmd->cmdName ) ;
		SerializeStr( f, cmd->cmdSymbol ) ;
		SerializeStr( f, cmd->cmdType ) ;
		SerializeStr( f, cmd->handler ) ;
		fwrite( &cmd->minParms, 1, sizeof(  cmd->minParms ), f ) ;
		fwrite( &cmd->maxParms, 1, sizeof(  cmd->maxParms ), f ) ;
		SerializeStr( f, cmd->validParms ) ;
		fwrite( &cmd->validParmsIdx, 1, sizeof(  cmd->validParmsIdx ), f ) ;
		fwrite( &cmd->options, 1, sizeof(  cmd->options ), f ) ;
		SerializeStr( f, cmd->alias ) ;
		SerializeStr( f, cmd->cmdDescr ) ;
		fwrite( &cmd->enumVal, 1, sizeof(  cmd->enumVal ), f ) ;
	}

	fclose( f ) ;

	printf ( "  serialized -> %s\n", fname ) ;
}

UInt8 * DeserializeStr( FILE * f )
{
	UInt16 len ;
	UInt8 * str ;

	fread( &len, 1, sizeof( len ) , f ) ;

	if( len ) {
		str = (UInt8*)calloc( len, 1 ) ;
		fread( str, len, 1, f ) ;
	}

	else {
		str = 0 ;
	}

	return str ;
}

ATGEN_CmdTbl_t * ATGEN_DeserializeCmdTbl( UInt32 * nCmd, char * srcdir )
{
	UInt32 i ;
	ATGEN_Cmd_t * cmd ;
	FILE * f ;
	char fname[80] ;

	ATGEN_CmdTbl_t * cmdTbl ;

	strcpy( fname, srcdir ) ;
	strcat( fname, "/" ) ;
	strcat( fname, FNAME_SERIALIZED ) ;

	f = fopen( fname, "rb" ) ;
	if( !f ) {
		printf( " Error opening serialization file\n" ) ;
		exit( 1 ) ;
	}

	cmdTbl = ATGEN_NewCmdTbl( ) ;

	fread( &cmdTbl->nCmd, 1, sizeof( cmdTbl->nCmd ) , f ) ;
	fread( &cmdTbl->maxParms, 1, sizeof( cmdTbl->maxParms ), f ) ;
	cmd = cmdTbl->cmd = (ATGEN_Cmd_t*) calloc( cmdTbl->nCmd, sizeof( ATGEN_Cmd_t ) ) ;

	for( i=0; i<cmdTbl->nCmd; i++, cmd++ ) {
		cmd->cmdName = DeserializeStr( f ) ;
		cmd->cmdSymbol = DeserializeStr( f ) ;
		cmd->cmdType = DeserializeStr( f ) ;
		cmd->handler = DeserializeStr( f ) ;
		fread( &cmd->minParms, 1, sizeof(  cmd->minParms ), f ) ;
		fread( &cmd->maxParms, 1, sizeof(  cmd->maxParms ), f ) ;
		cmd->validParms = DeserializeStr( f ) ;
		fread( &cmd->validParmsIdx, 1, sizeof(  cmd->validParmsIdx ), f ) ;
		fread( &cmd->options, 1, sizeof(  cmd->options ), f ) ;
		cmd->alias = DeserializeStr( f ) ;
		cmd->cmdDescr = DeserializeStr( f ) ;
		fread( &cmd->enumVal, 1, sizeof(  cmd->enumVal ), f ) ;
	}

	fclose( f ) ;


	printf ( "deserialized <- %s\n", fname ) ;

	*nCmd = cmdTbl->nCmd ;

	return cmdTbl ;
}

void ATGEN_GenerateCode( ATGEN_CmdTbl_t * cmdTbl, char * srcdir, char * incdir )
{
	char fname [80] ;

	strcpy( fname, srcdir ) ;
	strcat( fname, "/" ) ;
	strcat( fname, FNAME_API_GEN_C ) ;

	GenCmdTbl( fname, cmdTbl ) ;
	printf ( "        code -> %s\n", fname ) ;

	strcpy( fname, incdir ) ;
	strcat( fname, "/" ) ;
	strcat( fname, FNAME_API_GEN_H ) ;

	GenPrototypes( fname, cmdTbl ) ;
	printf ( "      header -> %s\n", fname ) ;

	strcpy( fname, srcdir ) ;
	strcat( fname, "/" ) ;
	strcat( fname, FNAME_ARGLIST_C ) ;

	GenExecCmdCall( fname, cmdTbl ) ;
	printf ( "        args -> %s\n", fname ) ;

	strcpy( fname, srcdir ) ;
	strcat( fname, "/" ) ;
	strcat( fname, FNAME_STUBS_C ) ;

	GenStubs( fname,  cmdTbl ) ;
	printf ( "       stubs -> %s\n", fname ) ;
}

void ATGEN_GenerateBasicIndices( ATGEN_CmdTbl_t * cmdTbl )
{
	UInt32 i ;

	for( i=0; i<cmdTbl->nCmd; i++ ) {
		UInt32 sortIdx = cmdTbl->sortOrder[i] ;
		ATGEN_Cmd_t * cmd = &cmdTbl->cmd[i] ;
		cmd->enumVal = i ;
	}
}

void ATGEN_GenExtendedIndices( ATGEN_CmdTbl_t * cmdTbl, UInt32 nextEnum )
{
	UInt32 i ;

	for( i=0; i<cmdTbl->nCmd; i++ ) {
		UInt32 sortIdx = cmdTbl->sortOrder[i] ;
		ATGEN_Cmd_t * cmd = &cmdTbl->cmd[i] ;
		if( cmd->enumVal == ~0 ) {
			cmd->enumVal = nextEnum++ ;
		}
	}
}
