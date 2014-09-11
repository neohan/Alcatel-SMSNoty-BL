#ifndef DOUGHAN_ALCATELSMSNOTYBL_SESSION_20140815__HPP__
#define DOUGHAN_ALCATELSMSNOTYBL_SESSION_20140815__HPP__


#include "playnode.hpp"
#include "ace/Singleton.h"
#include "ace/Thread_Mutex.h"
#include <string>
#include <map>
#include <list>


class Session
{
public:
	Session(const std::string & p_sessionid, const std::string & p_flowtype)
		: callid_(p_sessionid), flowstatus_("idle"), flowtype_(p_flowtype) { play1times_ = 0; play2times_ = 0; play3times_ = 0;}
	void reset() { callid_ = ""; flowstatus_ = "idle"; }
	void reset(const std::string & p_sessionid, const std::string & p_flowtype)
	{ callid_ = p_sessionid; flowstatus_ = "idle"; flowtype_ = p_flowtype; }
	void resetFlowStatus() { flowstatus_ = "idle"; }
	void setFlowStatus(const std::string & p_flowstatus) { flowstatus_ = p_flowstatus; }
	std::string getFlowStatus() { return flowstatus_; }
	void setFlowType(const std::string & p_flowtype) { flowtype_ = p_flowtype; }
	std::string getFlowType() { return flowtype_; }
	void setRTPInfo(const std::string & p_ip, const unsigned short p_port) { remoteRTPIP_ = p_ip; remoteRTPPort_ = p_port; }
	void getRTPInfo(std::string * p_ip, unsigned short * p_port)
	{
		*p_ip = remoteRTPIP_;
		*p_port = remoteRTPPort_;
	}
	void clearKeypressed() { keypressed_ = ""; }
	void addToKeypressed(const std::string p_key) { keypressed_ += p_key; }
	std::string getKeypressed() const { return keypressed_; }
	void plusPlay1Times() { play1times_ += 1; }
	size_t getPlay1Times() { return play1times_; }
	void resetPlay1Times() { play1times_ = 0; }
	void plusPlay2Times() { play2times_ += 1; }
	size_t getPlay2Times() { return play2times_; }
	void resetPlay2Times() { play2times_ = 0; }
	void plusPlay3Times() { play3times_ += 1; }
	size_t getPlay3Times() { return play3times_; }
	void resetPlay3Times() { play3times_ = 0; }

private:
	std::string							callid_;
	std::string							flowstatus_;
	std::string							flowtype_;
	std::string							remoteRTPIP_;
	unsigned short						remoteRTPPort_;
	std::string							keypressed_;
	size_t								play1times_;
	size_t								play2times_;
	size_t								play3times_;
};

class SessionManager
{
public:
	void recycle(const std::string & p_sessionid);
	void recycle(Session * p_session);

	void addPort(const std::string & p_sessionid, unsigned short * p_localport);
	bool removePort(const std::string & p_sessionid);
	unsigned short getPort(const std::string & p_sessionid);
	/*
		return true. continue to process.
		return false. respond BUSY sip message.
	 */
	bool newSession(const std::string & p_sessionid, const std::string & p_flowtype);
	bool setSessionFlowStatus(const std::string & p_sessionid, const std::string & p_flowstatus);
	std::string getSessionFlowType(const std::string & p_sessionid);
	std::string getSessionFlowStatus(const std::string & p_sessionid);
	bool setSessionRTPInfo(const std::string & p_sessionid, float p_delaysecs);
	bool setSessionRTPInfo(const std::string & p_sessionid, float p_delaysecs, const std::string & p_ip, const unsigned short p_port);
	bool getSessionRTPInfo(const std::string & p_sessionid, std::string * p_ip, unsigned short * p_port);
	bool setWAVFile(const std::string & p_sessionid, const std::string & p_wavfilename);
	bool setNewWAVFile(const std::string & p_sessionid, float p_delaysecs, const std::string & p_wavfilename);
	void startPlay(const std::string & p_sessionid);
	void stopPlay(const std::string & p_sessionid);
	void deletePlayNode(const std::string & p_sessionid);
	bool addToKeypressed(const std::string & p_sessionid, const std::string & p_key);
	bool getKeypressed(const std::string & p_sessionid, std::string * p_keypressed);
	bool clearKeypressed(const std::string & p_sessionid);
	bool plusPlay1Times(const std::string & p_sessionid);
	int getPlay1Times(const std::string & p_sessionid);
	bool resetPlay1Times(const std::string & p_sessionid);
	bool plusPlay2Times(const std::string & p_sessionid);
	int getPlay2Times(const std::string & p_sessionid);
	bool resetPlay2Times(const std::string & p_sessionid);
	bool plusPlay3Times(const std::string & p_sessionid);
	int getPlay3Times(const std::string & p_sessionid);
	bool resetPlay3Times(const std::string & p_sessionid);

private:
	ACE_Thread_Mutex					mutex_;
	std::map< std::string, Session * >	sessionMap_;
	std::list< Session * >				idlesessions_;
	unsigned short						localPort_;
	std::map< std::string, unsigned short >	sessionIDPortMap_;
};

typedef ACE_Singleton<SessionManager, ACE_Thread_Mutex> SessionManagerSingleton;
#define SessMgrSingleton SessionManagerSingleton::instance()


#endif