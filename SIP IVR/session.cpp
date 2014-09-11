/*
	����һ��SIP�������ͬʱ�������ͨ�����������������ʱ�޷����������������̬����SIP����
	����������������ʱ��̬����Session����Session�����봫ͳ��Channel�������ƣ�������ƽʱ��
	�����ڣ�ֻ��ͨ������ʱ����Ч��������ͨ���Ź�����ֻ��SIP��CallID�����������ض��ĺ��й�����

	���б�������Session����Ҫ�����������������Sessionʱ����һ����������

	����ĳЩ����ԭ���޷��÷���������ΪSession����ĳ�Ա������Session��������������һ��һ
	��ϵ��CallID��ΪŦ����

	�κ�һ���¼�����Ӧ����Ե���ĳһ�������Session�����������CallID��Ϊ��ʶ�����磬��ͨ��
	�¼�ID��connect����Я��CallID�������������¼�ID�Ƿ�����������Я��CallID���յ��������¼�
	ID�ǰ�������Я��CallID�������ĺô��ǣ����յ���ͨ�¼�����������CallID�ҵ�Session�������
	��Ҫ��������һ���������󣬲��ṩ����CallID��Ȼ��ʼ���������յ���������ʱ���ӷ����߳���
	����һ���¼�����ID�Ƿ����������ٽ�CallID�����¼�����
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