#include "LogManager.h"
#include "Utils.h"
#include "DogUnit.h"


MH_DLL_PARA								mhp;
PMH_DLL_PARA							pmhp = &mhp;
GS_MHDog								MHDog;
HANDLE									hInst;
HINSTANCE								mlib;
TDogUnit *								DogUnit;

typedef int (PASCAL * INSTDRIVER)(int iFlag);
typedef int (PASCAL * GETDOGDRIVERINFO)();
INSTDRIVER InstDriver;
GETDOGDRIVERINFO GetDogDriverInfo;


//-----The defination of  Command is as following ----------------------
/*
DogCheck       1
ReadDog        2
WriteDog       3
DogConvert     4
GetCurrentNo   5
EnableShare    6
DisableShare   7
SetDogCascade  8
SetNewPassword 9
*/


bool TDogUnit::Init()
{
	mlib = LoadLibrary("RCMicroDogSetup.dll");
	if ( mlib != NULL )
	{
		InstDriver = (INSTDRIVER)GetProcAddress(mlib, "InstDriver");
		GetDogDriverInfo = (GETDOGDRIVERINFO)GetProcAddress(mlib, "GetDogDriverInfo");
	}
	else
	{
		DWORD retCode = GetLastError();
		LOG4CXX_ERROR(LOG.rootLog, ("RCMicroDogSetup.dll not found." + IntToString(retCode)).c_str());
		FreeLibrary(mlib);
		return false;
	}


	int iStatus = GetDogDriverInfo();
	/*
	检测 驱动是否已经安装	0 没有安装驱动程序
							1 驱动版本相同(并口和usb)
							2 USB驱动版本相同
							3 并口驱动版本相同
							4 已安装旧版本驱动 (并口和usb)
							5 已安装旧版本USB驱动
							6 已安装旧版本并口驱动
							7 已安装新版本驱动 (并口和usb)
							8 已安装新版本USB驱动
							9 已安装新版本并口驱动
	*/
	if ( iStatus == 0 || iStatus == 4 )
	{
		LOG4CXX_ERROR(LOG.rootLog, "Dog driver have not been installed.");
		FreeLibrary(mlib);
		return false;
	}
	FreeLibrary(mlib);

	mlib = LoadLibrary("win32dll.dll");
	pmhp->Cascade = 0;
	if ( mlib != NULL )
	{
		MHDog =  (GS_MHDog)GetProcAddress(mlib, "GS_MHDog");
		if ( GetProcAddress(mlib, "GS_MHDog" ) == NULL )
		{
			LOG4CXX_ERROR(LOG.rootLog, "GS_MHDog(win32dll.dll) not found.");
			return false;
		}
	}
	else
	{
		LOG4CXX_ERROR(LOG.rootLog, "Win32dll.dll not found.");
		FreeLibrary(mlib);
		return false;
	}
	return true;
}

bool TDogUnit::DogCheck()
{
	pmhp->Command = 1;
	iRetValue = MHDog(pmhp);
	if ( iRetValue != 0 )
	{
		LOG4CXX_ERROR(LOG.rootLog, ("DogCheck function return code is " + IntToString(iRetValue)).c_str());
		return false;
	}
	return true;
}

bool TDogUnit::ReadDogInfo(unsigned long PassWord, char * Info, int DogAddr, int Length)
{
	pmhp->Command = 2;
	pmhp->DogPassword = PassWord;
	pmhp->DogAddr = DogAddr;
	pmhp->DogBytes = Length;

	char * sInfo = (char*)(pmhp->DogData);
	iRetValue = MHDog(pmhp);
   
	strcpy(Info, sInfo);

	if ( iRetValue != 0 )
		return false;
	else
		return true;
}

bool TDogUnit::ReadDogInfo(unsigned long PassWord, int * Info, int DogAddr, int Length)
{
	pmhp->Command = 2;
	pmhp->DogPassword = PassWord;
	pmhp->DogAddr = DogAddr;
	pmhp->DogBytes = Length;

	iRetValue = MHDog(pmhp);
	*Info = *(int *)pmhp->DogData;

	if ( iRetValue != 0 )
		return false;
	else
		return true;
}

bool TDogUnit::ReadDogSN(long * Info)
{
	long CurrentNo;
	pmhp->Command = 5;

	iRetValue = MHDog(pmhp);
	CurrentNo = *(long *)pmhp->DogData;

	if ( iRetValue != 0 )
	{
		LOG4CXX_ERROR(LOG.rootLog, ("GetCurrentNo function return code is " + IntToString(iRetValue)).c_str());
		return false;
	}
	else
	{
		*Info = CurrentNo;
		return true;
	}
}

void  TDogUnit::CloseDog()
{
	FreeLibrary(mlib);
}