#ifndef DOUGHAN_ALCATELSMSNOTYBL_SOFIA_20140813__HPP__
#define DOUGHAN_ALCATELSMSNOTYBL_SOFIA_20140813__HPP__


#include "ace/Singleton.h"
#include "ace/Thread_Mutex.h"
#include <sofia-sip/nua.h>
//#include <sofia-sip/nta.h>


#include <string>
#include <list>
#include <map>


class SofiaUA
{
public:
	SofiaUA(){idlePort = 6000;}
	~SofiaUA(){}
	void init();
	void uninit();
	void createUA(const std::string & p_no, const std::string & p_serverip);
	bool addRegisterRequest(nua_handle_t * p_handle, std::string & p_sipno);
	std::string getRegisterRequestSIPNo(nua_handle_t * p_handle);
	std::string getAndRemoveRegisterRequestSIPNo(nua_handle_t * p_handle);

	bool addUnregisterRequest(nua_handle_t * p_handle, std::string & p_sipno);
	std::string getUnregisterRequestSIPNo(nua_handle_t * p_handle);
	std::string getAndRemoveUnregisterRequestSIPNo(nua_handle_t * p_handle);

	void removeUA(const std::string & p_no, const std::string & p_serverip);
	static void threadfunc();

	unsigned short						idlePort;

public:
	su_home_t							home_[1];
	su_root_t *							root_;
	nua_t *								nua_;
	std::list<nua_t *>					nuas_;
	ACE_Thread_Mutex					mutex_;
	std::map< nua_handle_t *, std::string > registerReqMap_;
	std::map< nua_handle_t *, std::string > unregisterReqMap_;
};

typedef ACE_Singleton<SofiaUA, ACE_Thread_Mutex> SofiaUASingleton;
#define SofiaSingleton SofiaUASingleton::instance()


#endif