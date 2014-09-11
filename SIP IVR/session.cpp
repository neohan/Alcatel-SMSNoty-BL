/*
	由于一个SIP号码可以同时接听多个通话，因此在许可数变更时无法做到依据许可数动态创建SIP对象。
	但可以在许可数变更时动态创建Session对象。Session对象与传统的Channel对象类似，但它在平时并
	不存在，只在通话生成时才有效。它不与通道号关联，只与SIP的CallID关联，即与特定的呼叫关联。

	所有被创建的Session都会要求放音，所以在生成Session时给他一个放音对象。

	由于某些特殊原因，无法让放音对象作为Session对象的成员变量。Session对象与放音对象的一对一
	关系以CallID作为纽带。

	任何一个事件对象都应该针对的是某一个具体的Session，这个对象以CallID作为标识。例如，接通，
	事件ID是connect，并携带CallID。放音结束，事件ID是放音结束，并携带CallID。收到按键，事件
	ID是按键，并携带CallID。这样的好处是，当收到接通事件，可以依据CallID找到Session对象，如果
	需要放音申请一个放音对象，并提供给他CallID，然后开始放音；当收到放音结束时，从放音线程内
	生成一个事件对象，ID是放音结束，再将CallID赋给事件对象。
 */
#include "session.hpp"
#include "lock.h"
#include "playnode.hpp"


const unsigned short PORT_LOW_VALUE = 19980;
const unsigned short PORT_HIGH_VALUE = 39980;
const unsigned short PORT_STEP_VALUE = 2;


void SessionManager::recycle(const std::string & p_sessionid)
{
	std::list< Session * >::iterator	ite;
	std::map< std::string, Session * >::iterator itemap;
	Session *							session;
	SIPIVRLock sipivrLock(mutex_);
	itemap = sessionMap_.find(p_sessionid);
	if ( itemap != sessionMap_.end() )
	{
		session = itemap->second;
		sessionMap_.erase(itemap);
	}
	ChannelResourceManager::delNode(p_sessionid);
}

void SessionManager::recycle(Session * p_session)
{
	SIPIVRLock sipivrLock(mutex_);
	idlesessions_.push_front(p_session);
}

void SessionManager::addPort(const std::string & p_sessionid, unsigned short * p_localport)
{
	std::map< std::string, unsigned short >::iterator	ite;
	ite = sessionIDPortMap_.find(p_sessionid);
	if ( ite != sessionIDPortMap_.end() )
		sessionIDPortMap_.erase(ite);
	if ( localPort_ < PORT_LOW_VALUE || localPort_ >= PORT_HIGH_VALUE )
		localPort_ = PORT_LOW_VALUE;
	else
		localPort_ += PORT_STEP_VALUE;
	*p_localport = localPort_;
	sessionIDPortMap_.insert(std::make_pair< std::string, unsigned short >(p_sessionid, localPort_));
}

bool SessionManager::removePort(const std::string & p_sessionid)
{
	std::map< std::string, unsigned short >::iterator	ite;
	ite = sessionIDPortMap_.find(p_sessionid);
	if ( ite != sessionIDPortMap_.end() )
	{
		sessionIDPortMap_.erase(ite);
		return true;
	}
	return false;
}

unsigned short SessionManager::getPort(const std::string & p_sessionid)
{
	std::map< std::string, unsigned short >::iterator	ite;
	ite = sessionIDPortMap_.find(p_sessionid);
	if ( ite != sessionIDPortMap_.end() )
		return ite->second;
	return 0;
}

bool SessionManager::newSession(const std::string & p_sessionid, const std::string & p_flowtype)
{
	Session *							newsession;
	std::list< Session * >::iterator	ite;
	std::map< std::string, Session * >::iterator itemap;
	SIPIVRLock sipivrLock(mutex_);
	/*if ( ChannelResourceManager::addNode(p_sessionid, p_flowtype) == false )
		return false;*/
	itemap = sessionMap_.find(p_sessionid);
	if ( itemap != sessionMap_.end() )
	{
		newsession = itemap->second;
		newsession->resetFlowStatus();
		newsession->setFlowType(p_flowtype);
		return true;
	}

	if ( idlesessions_.size() != 0 )
	{
		ite = idlesessions_.begin();
		idlesessions_.pop_front();
		newsession = *ite;
		newsession->reset(p_sessionid, p_flowtype);
	}
	else
		newsession = new Session(p_sessionid, p_flowtype);
	sessionMap_.insert(std::make_pair<std::string, Session * >(p_sessionid, newsession));
	return true;
}

bool SessionManager::setSessionFlowStatus(const std::string & p_sessionid, const std::string & p_flowstatus)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		findSession->setFlowStatus(p_flowstatus);
		return true;
	}
	return false;
}

std::string SessionManager::getSessionFlowType(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		return findSession->getFlowType();
	}
	return "none";
}

std::string SessionManager::getSessionFlowStatus(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		return findSession->getFlowStatus();
	}
	return "none";
}

bool SessionManager::setSessionRTPInfo(const std::string & p_sessionid, float p_delaysecs)
{
	std::string							ip;
	unsigned short						port;
	unsigned short						localport;
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		if ( ( localport = getPort(p_sessionid) ) == 0 )
			return false;
		findSession = ite->second;
		findSession->getRTPInfo(&ip, &port);
		findSession = ite->second;
		std::string flowtype = findSession->getFlowType();
		if ( ChannelResourceManager::addNewNode(p_sessionid, flowtype, p_delaysecs) == false )
			return false;
		if ( ChannelResourceManager::updateNode(p_sessionid, ip, port, localport) == false )
			return false;
		return true;
	}
	return false;
}

bool SessionManager::setSessionRTPInfo(const std::string & p_sessionid, float p_delaysecs, const std::string & p_ip, const unsigned short p_port)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	unsigned short						localport;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		addPort(p_sessionid, &localport);
		findSession = ite->second;
		findSession->setRTPInfo(p_ip, p_port);
		if ( ChannelResourceManager::addNewNode(p_sessionid, "", p_delaysecs) == false )
			return false;
		if ( ChannelResourceManager::updateNode(p_sessionid, p_ip, p_port, localport) == false )
			return false;
	}
	return false;
}

bool SessionManager::getSessionRTPInfo(const std::string & p_sessionid, std::string * p_ip, unsigned short * p_port)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		findSession->getRTPInfo(p_ip, p_port);
		return true;
	}
	return false;
}

bool SessionManager::setWAVFile(const std::string & p_sessionid, const std::string & p_wavfilename)
{
	return ChannelResourceManager::setWAVFile(p_sessionid, p_wavfilename);
}

bool SessionManager::setNewWAVFile(const std::string & p_sessionid, float p_delaysecs, const std::string & p_wavfilename)
{
	std::string							flowtype;
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		flowtype = findSession->getFlowType();
		if ( ChannelResourceManager::addNewNode(p_sessionid, flowtype, p_delaysecs) == false )
			return false;
	}
	return false;
}

void SessionManager::deletePlayNode(const std::string & p_sessionid)
{
	SIPIVRLock sipivrLock(mutex_);
	ChannelResourceManager::deleteNode(p_sessionid);
}

void SessionManager::startPlay(const std::string & p_sessionid)
{
	ChannelResourceManager::startPlay(p_sessionid);
}

void SessionManager::stopPlay(const std::string & p_sessionid)
{
	ChannelResourceManager::stopPlay(p_sessionid);
}

bool SessionManager::addToKeypressed(const std::string & p_sessionid, const std::string & p_key)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		findSession->addToKeypressed(p_key);
		return true;
	}
	return false;
}

bool SessionManager::getKeypressed(const std::string & p_sessionid, std::string * p_keypressed)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		*p_keypressed = findSession->getKeypressed();
		return true;
	}
	return false;
}

bool SessionManager::clearKeypressed(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		findSession->clearKeypressed();
		return true;
	}
	return false;
}

bool SessionManager::plusPlay1Times(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		findSession->plusPlay1Times();
		return true;
	}
	return false;
}

int SessionManager::getPlay1Times(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		return findSession->getPlay1Times();
	}
	return -1;
}

bool SessionManager::resetPlay1Times(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		findSession->resetPlay1Times();
		return true;
	}
	return false;
}

bool SessionManager::plusPlay2Times(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		findSession->plusPlay2Times();
		return true;
	}
	return false;
}

int SessionManager::getPlay2Times(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		return findSession->getPlay2Times();
	}
	return -1;
}

bool SessionManager::resetPlay2Times(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		findSession->resetPlay2Times();
		return true;
	}
	return false;
}

bool SessionManager::plusPlay3Times(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		findSession->plusPlay3Times();
		return true;
	}
	return false;
}

int SessionManager::getPlay3Times(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		return findSession->getPlay3Times();
	}
	return -1;
}

bool SessionManager::resetPlay3Times(const std::string & p_sessionid)
{
	Session *							findSession;
	std::map< std::string, Session * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = sessionMap_.find(p_sessionid);
	if ( ite != sessionMap_.end() )
	{
		findSession = ite->second;
		findSession->resetPlay3Times();
		return true;
	}
	return false;
}