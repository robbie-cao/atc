# Microsoft Developer Studio Project File - Name="at_win32" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=at_win32 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "at_win32.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "at_win32.mak" CFG="at_win32 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "at_win32 - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "at_win32 - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "at_win32"
# PROP Scc_LocalPath "..\.."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "at_win32 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "at_win32 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../inc" /I "../../src" /I "../inc" /I "../../../inc" /I "../../../baseband/inc" /I "../../../dev/inc" /I "../../../rtos/inc" /I "../../../rtos/nucleus/inc" /I "../../../sim/inc" /I "../../../util/inc" /I "../../../stack/$(PRJ_STACK)/inc" /I "../../../msc/inc" /I "../../../ecdc/inc" /I "../../../sms/inc" /I "../../../mpx/inc" /I "../../../phonebk/inc" /I "../../../pch/inc" /I "../../../api/inc" /I "../../../phy/inc" /I "../../../system/inc" /I "../../../stack/$(PRJ_STACK)/sdtc" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "ALLOW_PPP_LOOPBACK" /D "AT_SIMULATOR" /D "PCTEST" /D "_$(BASEBAND_CHIP)_" /D "SDTENV" /D "STACK_$(PRJ_STACK)" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "at_win32 - Win32 Release"
# Name "at_win32 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\src\at_api.c
# End Source File
# Begin Source File

SOURCE=..\..\src\at_cmdtbl.c
# End Source File
# Begin Source File

SOURCE=..\..\src\at_state.c
# End Source File
# Begin Source File

SOURCE=..\..\src\at_string.c
# End Source File
# Begin Source File

SOURCE=..\src\at_win32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\util\src\bcd.c
# End Source File
# Begin Source File

SOURCE=..\..\..\msc\src\callparser.c
# End Source File
# Begin Source File

SOURCE=..\..\..\util\src\dialstr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\util\src\parser.c
# End Source File
# Begin Source File

SOURCE=..\..\..\msc\src\ss_api.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\inc\at_api.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\at_cmdtbl.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\at_rspcode.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\at_state.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\at_string.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\at_types.h
# End Source File
# Begin Source File

SOURCE=..\inc\at_win32.h
# End Source File
# Begin Source File

SOURCE=..\..\..\util\inc\dialstr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\util\inc\parser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\msc\inc\ss_api.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=..\..\src\at_cmd.tbl
# End Source File
# Begin Source File

SOURCE=..\..\src\at_cmd_ext.tbl
# End Source File
# End Target
# End Project
