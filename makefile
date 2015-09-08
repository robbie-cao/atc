#*******************************************************************************
#
# Description:  This file contains the "makefile" template.  It is used by
#				Source Integrity to generate the subordinate "makefile"s.
#
# File:	makefile
#
#*******************************************************************************

### Importing environmental variables
.IMPORT					:	PRJ_NAME					# import project name
.INCLUDE				:	../env/$(PRJ_NAME).env

### take care some problems with different evironments for running make
DIRSEPSTR				:=	/
ROOTDIR					:= $(ROOTDIR:s~\~$(DIRSEPSTR)~)
ARMDIR					:= $(ARMDIR:s~\~$(DIRSEPSTR)~)

### remove any '/' suffix
ROOTDIR					:= $(ROOTDIR:$(DIRSEPSTR)=)


### Shell definition - MKS korn shell
BIN						:=	$(ROOTDIR)/mksnt
SHELL					:=	$(BIN)/sh
SHELLMETAS				:=	;*?"<>|()&][$$\#`'
SHELLFLAGS				:=	-c
GROUPSUFFIX				:=	.ksh
GROUPFLAGS				:=	$(NULL)

### Macros - suffix definitions
C						:=	.c						# "C" source files
S						:=	.s						# Assembly source files
O						:=	.o						# object files
A						:=	.aof					# incremental linker files
P						:=	.pl						# Perl files
L						:=	.lst					# "C" listing files
M						:=	.mk
B						:=	.lib					# library files

.SUFFIXES				:-	$C $S $O $A $L $E $P $B	# defining new suffixes

### Macros - standard directories
ARMBIN					:=	$(ARMDIR)/bin			# define the ARM tools directory
PROJDIR					:=	$(PWD)
UPPERDIR				:=	$(PROJDIR:d)
CURRDIR					:=	$(PROJDIR:b)
SRCDIR					:=	src
OBJDIR					:=	obj
LSTDIR					:=	lst
TOOLDIR					:=	tool
INCDIR					:=	inc
DOCDIR					:=	doc
TMPDIR					:=	tmp

.EXPORT					:	TMPDIR

## Macros - targets
TARGET					=	$(CURRDIR:+"$B")
DEPENDS					=	depends.mk

### Macros - command names
CC						:=	$(BIN)/cc				# ARM Toolkit C compiler
AS						:=	$(BIN)/cc -a			# ARM Toolkit assembler
LB						:=	$(BIN)/cc -b			# ARM Toolkit librarian
LD						:=	$(BIN)/cc 				# ARM Toolkit linker
RM						:=	$(BIN)/rm				# UNIX-style rm, provided with MKS Toolkit
PERL					:=	perl				# Perl, provided with MKS Toolkit
SED						:=	$(BIN)/sed				# Sed, provided with MKS Toolkit
ARMCC					:=	$(ARMBIN)/armcc
PJ						:=	$(BIN)/pj
MKDIR					:=	$(BIN)/mkdir
MAKE					=	$(MAKECMD) $(MFLAGS)



### Macros - Command flags and default args
PLOPTIONS				=							# Perl flags -- 
RMOPTIONS				=	-f						# rm flag -- 

### Inference - meta-rules
%$O						:	%$C
	$(CC) $< -c -DMSP -DSDT_V3 -DSIG_USHORT $(GLOBAL_DEFINE) $(INCPATH:^"-I") -o $@

%$O						:	%$S
							$(AS) -32 $< $(INCPATH:^"-I") -o $@

# Intermediate target file removal
.REMOVE					:
							$(RM) $(RMFLAGS) $?

### Macros - local targets

SRCS					:=	atc.c 			\
							at_call.c		\
							at_callutil.c	\
							at_fax.c		\
							at_pbk.c		\
							at_cfg.c		\
							at_pch.c		\
							at_plmn.c		\
							at_sim.c 		\
							at_sms.c		\
							at_utsm.c		\
							at_util.c		\
							at_v24.c		\
							at_stk.c		\
							at_api.c		\
							at_cmdhndlr.c	\
							at_netwk.c		\
							at_ss.c			\
							at_string.c		\
							at_state.c		\
							at_phone.c      \
							at_tst.c		\
                            at_irda.c		\
							at_cmdtbl.c

.IF $(BUILD_TYPE)==PACKAGE_RELEASE_BUILD
SRCS					:=	at_cmdtbl.c
.END

OTHER_SRCS	:=	baseband baseband/arm baseband/arm/$(BASEBAND_CHIP) dev dev/$(PRJ_NAME) rtos rtos/$(PRJ_OS) sim util stack/$(PRJ_STACK) msc ecdc sms mpx phonebk pch $(DLINK)/ppp $(DLINK)/ip api phy system $(TEST) DSP HAL stubs

STACK_INC	:=      ../stack/$(PRJ_STACK)/inc \
			../stack/$(PRJ_STACK)/sdtc

### Macros - derived names
#PROJFILE				:=	$(UPPERDIR)/$(PRJ_NAME).pj
PROJFILE				:=	$(CURRDIR).pj
NAMES					:=	$(SRCS:b)
OBJS					:=	$(NAMES:+"$O")
MEMBERS					:=	$(SRCS:^"$(SRCDIR)/")
INCPATH					:=	$(INCDIR) $(OTHER_SRCS:^"../":+"/$(INCDIR)") $(UPPERDIR:+"/$(INCDIR)") $(STACK_INC)

STDDIRS					:=	$(OBJDIR) $(LSTDIR) $(SRCDIR) $(DOCDIR) $(INCDIR) $(TOOLDIR) $(TMPDIR)
DQ						=	"
DEPENDS_TMP				:=	$(DEPENDS).tmp

### Special Target Directives -- local search paths
.SOURCE$C				:-	$(SRCDIR)
.SOURCE$S				:-	$(SRCDIR)
.SOURCE$O				:-	$(OBJDIR)

# Turn on warnings
.SILENT					:=	$(__SILENT)


AT_CMD_TBL				:= src/at_cmd.tbl
AT_CMD_EXT_TBL			:= src/at_cmd_ext.tbl

GENERATED_FILES			:= src/at_api.gen.c \
						   inc/at_api.gen.h \
						   src/at_arglist.gen.c \
						   src/at_stubs.gen.c \
						   src/at_cmd.gen

### Targets

.IF $(BUILD_TYPE)==PACKAGE_RELEASE_BUILD
default					:	package_release_build
.ELSE
default					:	standard_build
.END


standard_build			:	$(GENERATED_FILES) $(STDDIRS) $(DEPENDS) $(TARGET)
							@$(MAKE) CHECK_WSDIR
						
package_release_build	:	$(GENERATED_FILES) $(STDDIRS) $(DEPENDS) $(OBJS)
							@$(MAKE) CHECK_WSDIR
							cp -f $(TARGET).pkg $(TARGET)
							armar -r $(TARGET) $(OBJS:^"obj/")
						
AT_GEN					:=	./tool/at_gen.exe
AT_EXT					:=	./tool/at_ext.exe

$(AT_GEN):				tool/src/at_genmain.c tool/src/at_gen.c ../util/src/parser.c
						cl $^ -DSDTENV $(GLOBAL_DEFINE) $(INCPATH:^"/I ") /I"tool/inc" -o $(AT_GEN)

$(AT_EXT):				tool/src/at_extmain.c tool/src/at_gen.c ../util/src/parser.c
						cl $^ -DSDTENV $(GLOBAL_DEFINE) $(INCPATH:^"/I ") /I"tool/inc" -o $(AT_EXT)

.IF $(BUILD_TYPE)==PACKAGE_RELEASE_BUILD
$(GENERATED_FILES) :	$(AT_CMD_EXT_TBL)
.ELSE
$(GENERATED_FILES) :	$(AT_GEN) $(AT_EXT) $(AT_CMD_TBL) $(AT_CMD_EXT_TBL)
.END
.IF $(BUILD_TYPE)!=PACKAGE_RELEASE_BUILD
						$(WSDIR)/tool/pputil "-D" "$(GLOBAL_DEFINE)" "//" < $(AT_CMD_TBL) > $(AT_CMD_TBL:+".tmp")
						$(AT_GEN) $(AT_CMD_TBL:+".tmp") src/ inc/ "$(GLOBAL_DEFINE)"	
						$(RM) $(AT_CMD_TBL:+".tmp")
.END						
						$(WSDIR)/tool/pputil "-D" "$(GLOBAL_DEFINE)" "//" < $(AT_CMD_EXT_TBL) > $(AT_CMD_EXT_TBL:+".tmp")
						$(AT_EXT) $(AT_CMD_EXT_TBL:+".tmp") src/ inc/ "$(GLOBAL_DEFINE)"
						$(RM) $(AT_CMD_EXT_TBL:+".tmp")

$(TARGET)				:	$(OBJS)
							@$(MAKE) CHECK_WSDIR
							$(LB) $@ $^
							cp -f $(TARGET) $(TARGET).pkg

$(STDDIRS:^"$(DQ)":+"$(DQ)")	:
							@$(MAKE) CHECK_WSDIR
							$(MKDIR) $@

all						:
							@$(MAKE) CHECK_WSDIR
							$(MAKE) clean_all
							$(MAKE) depends
							$(MAKE)

clean_all				:	clean clean_depends
							@$(MAKE) CHECK_WSDIR

clean					:
							@$(MAKE) CHECK_WSDIR
							$(RM) $(RMOPTIONS) $(OBJDIR:+"/*$O")
							$(RM) $(RMOPTIONS) $(LSTDIR:+"/*$L")
.IF $(BUILD_TYPE)!=PACKAGE_RELEASE_BUILD
							$(RM) $(RMOPTIONS) $(TARGET)
							$(RM) $(RMOPTIONS) $(AT_GEN) $(AT_EXT)
.END
							$(RM) $(RMOPTIONS) $(GENERATED_FILES)

clean_depends			:
							@$(MAKE) CHECK_WSDIR
							$(RM) $(RMOPTIONS) $(DEPENDS)

clean_generated_files	:
							$(RM) -f $(GENERATED_FILES)

dir_only				:	$(STDDIRS)
							@$(MAKE) CHECK_WSDIR

"$(DEPENDS)" depends	:			$(MEMBERS)
							@$(MAKE) CHECK_WSDIR
							$(MAKE) force_depends

# definition of force_depends see "../makefile_depend" file
.INCLUDE				:   ../makefile_depend

.INCLUDE .IGNORE		:	$(DEPENDS)

##########################################################
#
#	Check for valid WSDIR 
#
CHECK_WSDIR:
	@if ! test -f $(WSDIR)/tool/check_wsdir.sh ; \
		then echo "ERROR - WSDIR does not define a valid path"; fi
	@test -f $(WSDIR)/tool/check_wsdir.sh ; 
	@sh $(WSDIR)/tool/check_wsdir.sh ; 
#
##########################################################

