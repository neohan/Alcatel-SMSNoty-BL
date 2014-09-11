#include "config.hpp"
#include "sofia.hpp"
#include "lock.h"


bool Config::needUpdate()
{
	SIPIVRLock sipivrLock(mutex_);
	return needupdate_;
}

bool Config::haveBlacklistFeature()
{
	SIPIVRLock sipivrLock(mutex_);
	return licenseConfig_.blacklist_;
}

bool Config::haveSMNotifyFeature()
{
	SIPIVRLock sipivrLock(mutex_);
	return licenseConfig_.smnotify_;
}

size_t Config::getSMNotifyConcurrentNum()
{
	SIPIVRLock sipivrLock(mutex_);
	return licenseConfig_.smnotifynum_;
}

void Config::setLicenseStrings(const std::list< std::string > & p_licstrings)
{
	SIPIVRLock sipivrLock(mutex_);
	newlicenseConfig_.licensestrslist_ = p_licstrings;
	//解析许可字串 newlicenseConfig_与licenseConfig_比较如有变化,设置needupdate_为true。
}

void Config::commitUpdate()
{
	SIPIVRLock sipivrLock(mutex_);
	if ( needupdate_ == false )
		return;
	//newlicenseConfig_数据复制给licenseConfig_
	needupdate_ = false;
}

void Config::updateSIPInfo(const std::string p_sipserverip, unsigned short p_sipbaseport)
{
	SIPIVRLock sipivrLock(mutex_);
	sipinfoConfig_.sipserverip_ = p_sipserverip;
	sipinfoConfig_.sipbaseport_ = p_sipbaseport;
}

void Config::getSIPInfo(std::string * p_sipserverip, unsigned short * p_sipbaseport)
{
	SIPIVRLock sipivrLock(mutex_);
	*p_sipserverip = sipinfoConfig_.sipserverip_;
	*p_sipbaseport = sipinfoConfig_.sipbaseport_;
}

void Config::addSIPExtension(const std::string & p_sipno, bool p_blacklist, bool p_smnotify)
{
	SIPExtension * sipext;
	SIPIVRLock sipivrLock(mutex_);
	std::map< std::string, SIPExtension * >::iterator ite = sipExtMap_.find(p_sipno);
	if ( ite != sipExtMap_.end() )
	{
		sipext = ite->second;
		sipext->blacklist_ = p_blacklist;
		sipext->smnotify_ = p_smnotify;
		return;
	}
	sipext = new SIPExtension();
	sipext->sipno = p_sipno;
	sipext->blacklist_ = p_blacklist;
	sipext->smnotify_ = p_smnotify;
	sipext->registered_ = false;
	sipExtMap_.insert(std::make_pair< std::string, SIPExtension * >(p_sipno, sipext));
}

void Config::removeSIPExtension(const std::string & p_sipno)
{
	SIPIVRLock sipivrLock(mutex_);
	std::map< std::string, SIPExtension * >::iterator ite = sipExtMap_.find(p_sipno);
	if ( ite == sipExtMap_.end() )
		return;
	delete ite->second;
	sipExtMap_.erase(ite);
}

std::list< std::string > Config::getUnregisterSIPExtensions()
{
	std::list < std::string > unregisterlist;
	SIPIVRLock sipivrLock(mutex_);
	std::map< std::string, SIPExtension * >::iterator ite;
	for ( ite = sipExtMap_.begin(); ite != sipExtMap_.end(); ++ite )
	{
		if ( ite->second->registered_ == false )
			unregisterlist.push_back(ite->first);
	}
	return unregisterlist;
}

bool Config::setSIPExtensionRegisterFlag(const std::string & p_sipno, bool p_registerflag)
{
	SIPIVRLock sipivrLock(mutex_);
	std::map< std::string, SIPExtension * >::iterator ite = sipExtMap_.find(p_sipno);
	if ( ite == sipExtMap_.end() )
		return false;
	ite->second->registered_ = p_registerflag;
	return true;
}

bool Config::sipExtensionHaveBlacklistFeature(const std::string & p_sipno)
{
	SIPIVRLock sipivrLock(mutex_);
	std::map< std::string, SIPExtension * >::const_iterator ite = sipExtMap_.find(p_sipno);
	if ( ite == sipExtMap_.end() )
		return false;
	return ite->second->blacklist_;
}

bool Config::sipExtensionHaveSMNotifyFeature(const std::string & p_sipno)
{
	SIPIVRLock sipivrLock(mutex_);
	std::map< std::string, SIPExtension * >::const_iterator ite = sipExtMap_.find(p_sipno);
	if ( ite == sipExtMap_.end() )
		return false;
	return ite->second->smnotify_;
}

void Config::addWaitToRemoveSIPExtension(const std::string & p_sipno)
{
	SIPIVRLock sipivrLock(mutex_);
	waittoremoveSipnoList_.push_back(p_sipno);
}

std::list< std::string > Config::getWaitToRemoveSIPExtensions()
{
	std::list< std::string > removelist;
	SIPIVRLock sipivrLock(mutex_);
	removelist = waittoremoveSipnoList_;
	waittoremoveSipnoList_.clear();
	return removelist;
}

void Config::plusSMNotifyNum()
{
	SIPIVRLock sipivrLock(mutex_);
	++(licenseUsage_.smnotifynum_);
}

void Config::minusSMNotifyNum()
{
	SIPIVRLock sipivrLock(mutex_);
	--(licenseUsage_.smnotifynum_);
}

bool Config::exceedSMNotifyLicNum()
{
	SIPIVRLock sipivrLock(mutex_);
	if ( licenseUsage_.smnotifynum_ >= licenseConfig_.smnotifynum_ )
		return false;
	return true;
}

void Config::threadfunc()
{
	Sleep(5000);
	std::list< std::string > unregistersipextlst;
	std::list< std::string > waittoremovesipextlst;
	std::list< std::string >::iterator	unregisterite, toremoveite;
	std::string							unregistersipext, toremovesipext;
	std::string							sipserverip;
	unsigned short						sipbaseport;
	while ( true )
	{
		unregistersipextlst.clear();
		unregistersipextlst = ConfigSingleton::instance()->getUnregisterSIPExtensions();
		ConfigSingleton::instance()->getSIPInfo(&sipserverip, &sipbaseport);
		SofiaUASingleton::instance()->idlePort = sipbaseport;
		for ( unregisterite = unregistersipextlst.begin(); unregisterite != unregistersipextlst.end(); ++unregisterite )
		{
			unregistersipext = *unregisterite;
			SofiaUASingleton::instance()->createUA(unregistersipext, sipserverip);
		}
		waittoremovesipextlst.clear();
		waittoremovesipextlst = ConfigSingleton::instance()->getWaitToRemoveSIPExtensions();
		for ( toremoveite = waittoremovesipextlst.begin(); toremoveite != waittoremovesipextlst.end(); ++toremoveite )
		{
			toremovesipext = *toremoveite;
			SofiaUASingleton::instance()->removeUA(toremovesipext, sipserverip);
		}
		Sleep(10000);
	}
}