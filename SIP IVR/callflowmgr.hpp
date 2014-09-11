#ifndef DOUGHAN_ALCATELSMSNOTYBL_CALLFLOW_20140825__HPP__
#define DOUGHAN_ALCATELSMSNOTYBL_CALLFLOW_20140825__HPP__


#include <sofia-sip/nua_tag.h>
#include "ace/Singleton.h"
#include "ace/Thread_Mutex.h"
#include <string>
#include <deque>
#include <map>


class EventObj
{
public:
	std::string							eventName_;
	std::string							callid_;
	nua_handle_t *						nh_;
	std::string							dnis_;
	std::string							ip_;
	unsigned short						port_;
	std::string							key_;
};

class TimeoutObj
{
public:
	DWORD								tickcount_;
	size_t								timeoutmsecs_;
};

class CallFlowProcess
{
public:
	void pushEventObj(EventObj * p_eventobj);
	static void threadfunc(void *);
	static void newTimeoutObj(const std::string & p_sessionid);
	static void removeTimeoutObj(const std::string & p_sessionid);
	static void delayTimeoutObj(const std::string & p_sessionid);

private:
	std::deque < EventObj * >			eventObjs_;
	static std::map< std::string, TimeoutObj * > timeoutObjs_;
	ACE_Thread_Mutex					mutex_;
};

typedef ACE_Singleton<CallFlowProcess, ACE_Thread_Mutex> CallFlowProcessSingleton;
#define CallFlowProcSingleton CallFlowProcessSingleton::instance()


#endif