MSP_HOME=..

include $(MSP_HOME)/env/platform.gmk
include $(MSP_HOME)/env/stddefs.gmk

TARGET_LIB		=	libatc.a

AT_CMD_TBL		=	src/at_cmd.tbl
AT_CMD_EXT_TBL	=	src/at_cmd_ext.tbl

GENERATED_FILES = 	src/at_api.gen.c \
			     inc/at_api.gen.h \
			     src/at_arglist.gen.c \
			     src/at_stubs.gen.c
 
ifneq ($(PACKAGE_RELEASE),-DPACKAGE_RELEASE)
GENERATED_FILES += src/at_cmd.gen
endif

ifeq ($(OS),Windows_NT)
AT_GEN			=	./tool/at_gen.exe
AT_EXT			=	./tool/at_ext.exe
else
AT_GEN			=	./tool/at_gen.out
AT_EXT			=	./tool/at_ext.out
endif

vpath %.c src
vpath %.s src

INCPATH		=	inc $(STD_INCPATH)
CCFLAGS		=	$(STD_CCFLAGS) $(GLOBAL_DEFINE)

ifneq ($(PACKAGE_RELEASE),-DPACKAGE_RELEASE)
	SRCS =			atc.c 			\
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
					at_irda.c       \
					at_tst.c        \
					at_cmdtbl.c      \
					at_bluetooth.c
else
	SRCS =			at_cmdtbl.c
endif

all			:	
				@$(MAKE) $(AT_EXT)
				@$(MAKE) $(AT_GEN)
				@$(VERIFY_PLATFORM) || exit 1
				@$(MAKE) PLATFORM=$(GET_PLATFORM) all_r

clean		:	
				@$(VERIFY_PLATFORM) || exit 1
				@$(MAKE) PLATFORM=$(GET_PLATFORM) clean_r


ifneq ($(PACKAGE_RELEASE),-DPACKAGE_RELEASE)
all_r			:	$(STD_MKDIRS) 
				$(MAKE) $(AT_GEN) 
				$(MAKE) $(AT_EXT) 
else
all_r			:	$(STD_MKDIRS) 
endif
				#$(MAKE) $(GENERATED_FILES)
				@for i in $(GENERATED_FILES); do $(MAKE) $$i || exit 1;done
				$(MAKE) $(TARGET_LIB)

$(TARGET_LIB)	:		$(STD_OBJS)	
ifneq ($(PACKAGE_RELEASE),-DPACKAGE_RELEASE)
				$(LC) $@ $^
				$(CP) $@ $@.pkg
else
				$(CP) $@.pkg $@
				armar -r $@ $(STD_OBJS)
endif

$(STD_MKDIRS)	:
				$(MKDIR) $@

clean_r		:
				$(RM) $(STD_CLEANFILES)
				$(RM) $(TARGET_LIB)
				$(RM) $(GENERATED_FILES)
				$(RM) $(AT_GEN) $(AT_EXT)

$(AT_GEN): tool/src/at_genmain.c tool/src/at_gen.c $(MSP_HOME)/util/src/parser.c
	gcc $^ $(GLOBAL_DEFINE) $(addprefix -I, $(INCPATH)) -I"tool/inc" -o $(AT_GEN)

$(AT_EXT):	tool/src/at_extmain.c tool/src/at_gen.c $(MSP_HOME)/util/src/parser.c
	gcc $^ $(GLOBAL_DEFINE) $(addprefix -I, $(INCPATH)) -I"tool/inc" -o $(AT_EXT)

ifneq ($(PACKAGE_RELEASE),-DPACKAGE_RELEASE)

$(GENERATED_FILES): $(AT_CMD_TBL) $(AT_CMD_EXT_TBL) $(AT_GEN) $(AT_EXT)

	$(PPUTIL) "-D" "$(GLOBAL_DEFINE)" "//" < $(AT_CMD_TBL) > $(AT_CMD_TBL).tmp &&\
	$(AT_GEN) $(AT_CMD_TBL).tmp src/ inc/ "$(GLOBAL_DEFINE)" &&\
	$(PPUTIL) "-D" "$(GLOBAL_DEFINE)" "//" < $(AT_CMD_EXT_TBL) > $(AT_CMD_EXT_TBL).tmp &&\
	$(AT_EXT) $(AT_CMD_EXT_TBL).tmp src/ inc/ "$(GLOBAL_DEFINE)" 

else

$(GENERATED_FILES): $(AT_CMD_EXT_TBL) $(AT_EXT)

	$(PPUTIL) "-D" "$(GLOBAL_DEFINE)" "//" < $(AT_CMD_EXT_TBL) > $(AT_CMD_EXT_TBL).tmp &&\
	$(AT_EXT) $(AT_CMD_EXT_TBL).tmp src/ inc/ "$(GLOBAL_DEFINE)" 

endif

-include $(STD_DEPS)
