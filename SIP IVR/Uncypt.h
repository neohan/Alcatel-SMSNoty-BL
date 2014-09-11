#ifndef __PURLVOICE_UNCYPT_H__
#define __PURLVOICE_UNCYPT_H__


class TUncyptUnit
{
public:
	int iRetValue;
	bool Initi();
	bool SetAddKey(long sAddKey);
	bool SetMultKey(long sMultKey);
	bool EncryptCTINum(char * sSourceStr, char * PassWord, char * sSecretStr);
	bool DecryptCTINum(char * sSecretStr, char * PassWord, char * sSourceStr);
	bool DestroyYPT();
};

extern TUncyptUnit * UncyptUnit;


#endif