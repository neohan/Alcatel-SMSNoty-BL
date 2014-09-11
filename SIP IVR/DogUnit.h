#ifndef __PURLVOICE_DOGUNIT_H__
#define __PURLVOICE_DOGUNIT_H__


/*#define IDM_ABOUT                       100
#define IDM_DEMO                        101
#define IDD_DIALOG                      101
#define IDC_BUTTON_WRITE                1002
#define IDC_BUTTON_READ                 1003
#define IDC_BUTTON_SN                   1004
#define IDC_RADIO_INTEGER               1005
#define IDC_RADIO_LONG                  1006
#define IDC_RADIO_FLOAT                 1007
#define IDC_RADIO_STRING                1008
#define IDC_STATIC_OPERATION            1009
#define IDC_STATIC_RESULT               1010
#define IDC_BUTTON_CURNO                1011
#define IDC_BUTTON_CHECK                1012
#define IDC_BUTTON_CONVERT              1013
#define IDC_EDIT_PASSWORD               1014
#define IDC_BUTTON_ENABLE               1016
#define IDC_BUTTON_DISABLE              1017
#define IDC_BUTTON_SETDOGCASCADE        1018
#define IDC_BUTTON_SETPASSWORD          1019
#define IDC_EDIT_CASCADE                1020
#define IDC_EDIT_NEWCASCADE             1021
#define IDC_EDIT_NEWPASSWORD            1022

#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        102
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1015
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif*/

#include <windows.h>

#ifndef ULONG
#define ULONG unsigned long
#endif

#ifndef HUINT
#define HUINT unsigned short
#endif

#ifndef UCHAR
#define UCHAR unsigned char
#endif

#ifndef WORD
#define WORD unsigned short
#endif

#ifndef DWORD
#define DWORD unsigned long
#endif

#define MHSTATUS ULONG

typedef struct _MH_DLL_PARA
{
    WORD/*BYTE*/						Command;
    WORD/*BYTE*/						Cascade;
    WORD								DogAddr;
    WORD								DogBytes;
    DWORD								DogPassword;
    DWORD								DogResult;
  	DWORD/*BYTE*/						NewPassword;
    BYTE								DogData[200];
}MH_DLL_PARA,  far * PMH_DLL_PARA;

typedef unsigned long (PASCAL * GS_MHDog)(PMH_DLL_PARA pmdp);


class TDogUnit
{
public:
	int iRetValue;
	bool Init();
	bool DogCheck();
	bool ReadDogInfo(unsigned long PassWord, char * Info, int DogAddr, int Length);
	bool ReadDogInfo(unsigned long PassWord, int * Info, int DogAddr, int Length);
	bool ReadDogSN(long * Info);
	void CloseDog();
};

extern TDogUnit *DogUnit;


#endif