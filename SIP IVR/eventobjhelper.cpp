#include "eventobjhelper.hpp"
#include "callflowmgr.hpp"


void EventObjHelper::newEventObj(const std::string & p_sessionid, const std::string & p_eventname)
{
	EventObj * eventobj = new EventObj;
	eventobj->callid_ = p_sessionid;
	eventobj->eventName_ = p_eventname;
	CallFlowProcessSingleton::instance()->pushEventObj(eventobj);
}

void EventObjHelper::newKeyPressedEventObj(const std::string & p_sessionid, const std::string & p_key)
{
	EventObj * eventobj = new EventObj;
	eventobj->callid_ = p_sessionid;
	eventobj->eventName_ = "call.dtmfypressed";
	eventobj->key_ = p_key;
	CallFlowProcessSingleton::instance()->pushEventObj(eventobj);
}