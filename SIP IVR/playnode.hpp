#ifndef DOUGHAN_ALCATELSMSNOTYBL_CHANNELRESOURCE_20140813__HPP__
#define DOUGHAN_ALCATELSMSNOTYBL_CHANNELRESOURCE_20140813__HPP__


#include "ace/Thread_Mutex.h"
#include <string>


class ChannelResourceManager
{
public:
	static bool addNode(const std::string p_sessionid, const std::string & p_callflowtype, float p_delaysecs);
	static bool addNewNode(const std::string p_sessionid, const std::string & p_callflowtype, float p_delaysecs);
	static bool updateNode(const std::string p_sessionid, const std::string p_ip, unsigned short p_port, unsigned short p_localport);
	static bool delNode(const std::string p_sessionid);
	static bool setWAVFile(const std::string & p_sessionid, const std::string & p_wavfilename);
	static bool startPlay(const std::string p_sessionid);
	static bool stopPlay(const std::string p_sessionid);
	static bool deleteNode(const std::string p_sessionid);
	static void getDTMF();

private:
	static ACE_Thread_Mutex				mutex_;
};

void voiceThreadFunc(void *);
void getDTMFThreadFunc(void *);


#endif