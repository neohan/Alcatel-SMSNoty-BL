#ifndef DOUGHAN_ALCATELSMSNOTYBL_CONFIG_2014081__HPP__
#define DOUGHAN_ALCATELSMSNOTYBL_CONFIG_2014081__HPP__


#include "ace/Singleton.h"
#include "ace/Thread_Mutex.h"
#include <list>
#include <map>
#include <string>


class LicenseConfig
{
public:
	LicenseConfig() : blacklist_(false), smnotify_(false), smnotifynum_(0) {}
	bool								blacklist_;
	bool								smnotify_;
	size_t								smnotifynum_;
	std::list< std::string >			licensestrslist_;
};

class LicenseUsage
{
public:
	LicenseUsage() : smnotifynum_(0) {}
	size_t								smnotifynum_;
};

class SIPExtension
{
public:
	bool								registered_;
	std::string							sipno;
	bool								blacklist_;
	bool								smnotify_;
};

class SIPInfoConfig
{
public:
	std::string							sipserverip_;
	unsigned short						sipbaseport_;
};

class Config
{
public:
	Config() : needupdate_(false) {}
	bool needUpdate();
	void commitUpdate();
	void setLicenseStrings(const std::list< std::string > & p_licstrings);
	bool haveBlacklistFeature();
	bool haveSMNotifyFeature();
	size_t getSMNotifyConcurrentNum();

	void updateSIPInfo(const std::string p_sipserverip, unsigned short p_sipbaseport);
	void getSIPInfo(std::string * p_sipserverip, unsigned short * p_sipbaseport);

	void addSIPExtension(const std::string & p_sipno, bool p_blacklist, bool p_smnotify);
	void removeSIPExtension(const std::string & p_sipno);
	bool setSIPExtensionRegisterFlag(const std::string & p_sipno, bool p_registerflag);
	std::list< std::string > getUnregisterSIPExtensions();
	bool sipExtensionHaveBlacklistFeature(const std::string & p_sipno);
	bool sipExtensionHaveSMNotifyFeature(const std::string & p_sipno);
	void addWaitToRemoveSIPExtension(const std::string & p_sipno);
	std::list< std::string > getWaitToRemoveSIPExtensions();

	void plusSMNotifyNum();
	void minusSMNotifyNum();
	bool exceedSMNotifyLicNum();

	static void threadfunc();

private:
	ACE_Thread_Mutex					mutex_;
	SIPInfoConfig						sipinfoConfig_;
	LicenseConfig						licenseConfig_;
	LicenseConfig						newlicenseConfig_;
	LicenseUsage						licenseUsage_;
	bool								needupdate_;
	std::map< std::string, SIPExtension * > sipExtMap_;
	std::list< std::string >			waittoremoveSipnoList_;
};

typedef ACE_Singleton<Config, ACE_Thread_Mutex> ConfigSingleton;
#define ConfSingleton ConfigSingleton::instance()


#endif