#ifndef DOUGHAN_ALCATELSMSNOTYBL_EVENTOBJHELPER_20140829__HPP__
#define DOUGHAN_ALCATELSMSNOTYBL_EVENTOBJHELPER_20140829__HPP__


#include <string>


class EventObjHelper
{
public:
	static void newEventObj(const std::string & p_sessionid, const std::string & p_eventname);
	static void newKeyPressedEventObj(const std::string & p_sessionid, const std::string & p_key);
};


#endif