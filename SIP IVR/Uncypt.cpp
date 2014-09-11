//---------------------------------------------------------------------------
#pragma hdrstop


#include "Uncypt.h"
#include <windows.h>

typedef LPTSTR (_stdcall *EncryptNO)(LPTSTR ,LPTSTR );
typedef LPTSTR (_stdcall *DecryptNO)(LPTSTR,LPTSTR);
typedef void (_stdcall *Init)(void);
typedef void (_stdcall *Destroy)(void);
typedef void (_stdcall *WriteAddKey)(long);
typedef void (_stdcall *WriteMultiKey)(long);
Init Initsys;
Destroy Destroydll;
EncryptNO EncryptNum;
DecryptNO DecryptNum;
WriteAddKey InputAddKey;
WriteMultiKey InputMultKey;
HINSTANCE hLib;
TUncyptUnit *UncyptUnit;
//---------------------------------------------------------------------------
bool  TUncyptUnit::Initi()
{
   hLib = LoadLibrary( "cypt.dll" );
   if(hLib!=NULL)
   {
      Initsys = (Init)GetProcAddress(hLib,"Init");
      if(Initsys!=NULL)
        Initsys();
      EncryptNum = (EncryptNO)GetProcAddress(hLib,"EncryptNO");
      DecryptNum = (DecryptNO)GetProcAddress(hLib,"DecryptNO");
      Destroydll    = (Destroy)GetProcAddress(hLib,"Destroy");
      InputAddKey   = (WriteAddKey)GetProcAddress(hLib,"WriteAddKey");
      InputMultKey  = (WriteMultiKey)GetProcAddress(hLib,"WriteMultiKey");
   }
   else
   {
     MessageBox(NULL,"cypt.DLL not found!","Error",MB_OK);
     FreeLibrary(hLib);
     return false;
   }
  return true;
}
bool  TUncyptUnit::SetAddKey(long sAddKey)
{
  InputAddKey(sAddKey);
  return true;
}
bool  TUncyptUnit::SetMultKey(long sMultKey)
{
  InputMultKey(sMultKey);
  return true;
}
bool  TUncyptUnit::EncryptCTINum(char* sSourceStr,char* PassWord,char * sSecretStr)
{
  strcpy(sSecretStr,EncryptNum(sSourceStr,PassWord));
  return true;
}
bool  TUncyptUnit::DecryptCTINum(char* sSecretStr,char* PassWord,char * sSourceStr)
{
  //strcpy(sSourceStr,DecryptNum( sSecretStr , PassWord));
	try{

	char * pTemp = DecryptNum( sSecretStr , PassWord);

	strcpy(sSourceStr,pTemp);
	}catch(...)
	{
		return false;
	}
  return true;
}
bool  TUncyptUnit::DestroyYPT()
{

  Destroydll();
  FreeLibrary(hLib);
  return true;
}

