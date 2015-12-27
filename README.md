# ATC

AT Command Interpreter:
Hash Search Algorithm

This document is a work in progress. The information presented may not be accurate or up to date.

Contents

* Overview	
* Hash Search Principles 
* Hash Table Structure 
* Hash Search Rules 
* Hash Search Example 

## Overview

This page describes the algorithm used by the AT Command Parser to identify AT commands.

Each time a command line is entered, the AT Command Parser is called to scan the line for AT commands and associated parameters, to validate parameter ranges and to call appropriate command-handler functions. The scanning operation isolates possible AT commands, based on command-line syntax rules. The isolated AT commands must be compared to a table of supported AT commands to determine whether there is a match; the table of supported AT commands is called the AT Command Table.

One way to handle search the AT Command Table is to loop over all supported commands, comparing each supported command to the isolated command. This method, while straightforward, has several disadvantages:

* It is inefficient. An 'average' search requires on the order of N*M/2 comparison operations, where N is the average command length (in characters) and M is the number of possible commands. Assuming N=4 and M=100 (representative of our current AT command set), approximately 200 comparisons are required for an average search.
* Commands can be "concatenated", in some instances without any special delimiter characters. For example the search engine must be able to identify two separate commands, "E" and "V", in the command line "ATEV". Therefore it is not possible to simply isolate a possible command.
* Commands can "aliased" by other commands. For example, the search engine must be able to distinguish "AT*" from "AT*XXX". Again, isolating a possible command is not a simple matter.

The AT Command Parser employs a hash algorithm to quickly search the table of allowed commands. The isolation of the command is integral to the hash algorithm.

The hash algorithm is efficient; the number of comparison operations is on the order of 2*N. For our representative command set, this reduces the number of comparisons to 8, a 25:1 reduction in processing time!

## Hash Search Principles

The following simple set of AT commands, comprising the AT Command Table, is useful for demonstrating the principles of the search algorithm:


    - ATA
    - ATAA
    - ATABCD
    - ATABEF
    - ATB

Notes:

1. The structure of the hash table requires that the command set be sorted alphabetically. However when defining commands for the AT Command Generator Tool, the commands may be entered in arbitrary order.
2. The AT is always stripped from the command prior to a search.
3. The search is not case sensitive; ATA is the same as ata and the same as aTa, and so on.

This AT Command Table can be described by the following topology:

```
    >--+--A--+----------------------->  "A"
       |     |                  
       |     +--A-------------------->  "AA"
       |     |                
       |     +--B--+--C--+--D-------->  "ABCD"
       |                 |
       |                 +--E--+--F-->  "ABEF"
       |
       +--B-------------------------->  "B"
```

A search begins at the left-hand side of the map, and with the first character of the isolated command. At each node in the map (shown by +) a single-character comparison is made. If a character match is found the search continues along the associated path. If no character match is found the search terminates with a 'not-found' result.

## Hash Table Structure

The hash table is implemented as an array of hash blocks:

```
    const AT_HashBlk_t atHashTbl [] = {
        hash-block-0,
        hash-block-1,
        hash-block-2,
        .
        .
        .
        hash-entry-n
    } ;
```
    
AT_HashBlk_t is a structure comprising two integers:

```
    typedef struct {
        UInt16 firstRow ;
        UInt16 nextBlock ; 
    }   AT_HashBlk_t ;
```
    
The firstRow element is an index to the alphabetically-sorted list of commands.

The nextBlock element is an index to the hash-table array itself. The high-order bit of nextBlock is reserved as a flag bit. This leaves the remaining 15 bits for indexing; therefore up to 2^15, or 32768 commands may be stored.

The symbol AT_ENDHASH is used to described bit 15 of nextBlock. Its use is described below.

In practice the number of AT commands will most likely never exceed the 15-bit limit. If more than 2^15 commands are required, firstRow and nextBlock can easily be increased to 32 bits.

Using the sample command set, the following code is generated:

```
    const AT_HashBlk_t atHashTbl [] = {
    	{ 0, 4 } ,
    	{ 0, 0 } ,				//	AA
    	{ 1, 2 | AT_ENDHASH } ,
    	{ 1, 0 } ,				//	ABB
    	{ 2, 0 | AT_ENDHASH } ,		//	ABC
    	{ 3, 3 | AT_ENDHASH } ,
    	{ 3, 2 | AT_ENDHASH } ,
    	{ 3, 0 } ,				//	XCC
    	{ 4, 0 | AT_ENDHASH } ,		//	XCD
    } ;
```

AT_ENDHASH is set to the high-order bit of nextBlock; its use will be desribed shortly.
