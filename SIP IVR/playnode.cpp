/*
放音资源。本质上是个线程。向网络上固定地址端口发送RTP包。
一旦发现某个session需要放音，取出session的sdp信息赋给playnode，再启动这个线程。放音的目的是收号。
因此一旦checkdtmf线程检测到某个session有按键事件，停止放音。checkdtmf只能获取到对端和本端的ip和port。
是否可以使用放音资源还取决于许可数量。
*/
#include "playnode.hpp"
#include "lock.h"
#include "eventobjhelper.hpp"
#include <mipcomponentchain.h>
#include <mipcomponent.h>
#include <miptime.h>
#include <mipaveragetimer.h>
#include <mipwavinput.h>
#include <mipsamplingrateconverter.h>
#include <mipsampleencoder.h>
#include <mipulawencoder.h>
#include <mipalawencoder.h>
#include <miprtpulawencoder.h>
#include <miprtpalawencoder.h>
#include <miprtpcomponent.h>
#include <miprtpdecoder.h>
#include <miprtpulawdecoder.h>
#include <mipulawdecoder.h>
#include <mipaudiomixer.h>
#include <miprawaudiomessage.h>
#include <jrtplib3\rtpsession.h>
#include <jrtplib3\rtpsessionparams.h>
#include <jrtplib3\rtpipv4address.h>
#include <jrtplib3\rtpudpv4transmitter.h>
#include <jrtplib3\rtperrors.h>
#include <jrtplib3\rtppacket.h>
#include <log4cxx/logger.h>
#include <map>
#include <deque>
#include <strstream>
#include <iostream>

using namespace jrtplib;
using namespace log4cxx;


extern LoggerPtr g_rtpLog;


void checkError(bool returnValue, const MIPComponent &component)
{
	if (returnValue == true)
		return;

	std::cerr << "An error occured in component: " << component.getComponentName() << std::endl;
	std::cerr << "Error description: " << component.getErrorString() << std::endl;
}

void checkError(bool returnValue, const MIPComponentChain &chain)
{
	if (returnValue == true)
		return;

	std::cerr << "An error occured in chain: " << chain.getName() << std::endl;
	std::cerr << "Error description: " << chain.getErrorString() << std::endl;
}

void checkError(int status)
{
	if (status >= 0)
		return;
	
	std::cerr << "An error occured in the RTP component: " << std::endl;
	std::cerr << "Error description: " << RTPGetErrorString(status) << std::endl;
}

class VoiceEventObj
{
public:
	std::string							eventName_;
	std::string							callid_;
};

std::deque < VoiceEventObj * >			voiceEventObjs_;
ACE_Thread_Mutex						mutex_;

void pushVoiceEventObj(const std::string & p_eventName, const std::string & p_callid)
{
	VoiceEventObj * voiceeventobj = new VoiceEventObj;
	voiceeventobj->callid_ = p_callid;
	voiceeventobj->eventName_ = p_eventName;
	SIPIVRLock sipivrLock(mutex_);
	voiceEventObjs_.push_back(voiceeventobj);
}

void voiceThreadFunc(void *)
{
	std::stringstream					ssvocthd;
	std::deque < VoiceEventObj * >::iterator ite;
	VoiceEventObj *						voiceeventobj;
	while ( 1 )
	{
		SIPIVRLock sipivrLock(mutex_);
		if ( voiceEventObjs_.size() == 0 )
		{
			sipivrLock.release();
			Sleep(50);
			continue;
		}
		ite = voiceEventObjs_.begin();
		voiceeventobj = *ite;
		voiceEventObjs_.pop_front();
		sipivrLock.release();

		ssvocthd.str("");
		ssvocthd << "event name:" << voiceeventobj->eventName_ << "    session id:" << voiceeventobj->callid_ << "\r\n";
		if ( voiceeventobj->eventName_ == "play.stop" )
		{
			ssvocthd << "ChannelResourceManager::stopPlay" << "\r\n";
			ChannelResourceManager::stopPlay(voiceeventobj->callid_);
		}
		
		LOG4CXX_INFO(g_rtpLog,  ssvocthd.str().c_str());
		delete voiceeventobj;
		Sleep(50);
	}
}

class PlayWAVChain : public MIPComponentChain
{
public:
	PlayWAVChain(const std::string &chainName) : MIPComponentChain(chainName) {}
	void setSessionID(const std::string & p_sessionid) { sessionid_ = p_sessionid; }
	std::string getSessionID() const { return sessionid_; }
private:
	std::string							sessionid_;
	void onThreadExit(bool error, const std::string &errorComponent, const std::string &errorDescription)
	{
		std::stringstream					sschain;
		if ( error )
			sschain << "MIPComponentChain (" << getName() << ") Thread Exit    err occurs    component:" << errorComponent << "    desc:" << errorDescription << "\r\n";
		else
			sschain << "MIPComponentChain (" << getName() << ") Thread Exit\r\n";
		LOG4CXX_INFO(g_rtpLog,  sschain.str().c_str());
	}
};

class SIPIVRWAVInput : public MIPWAVInput
{
public:
	SIPIVRWAVInput() : alreadyLastFrame_(false) {}
	std::string							sessionid_;
	bool								alreadyLastFrame_;
private:
	void onLastInputFrame()
	{
		if ( alreadyLastFrame_ == true )
			return;
		alreadyLastFrame_ = true;
		pushVoiceEventObj("play.stop", sessionid_);
		LOG4CXX_INFO(g_rtpLog,  "onLastInputFrame");
		EventObjHelper::newEventObj(sessionid_, "play.done");
	}
};

struct RFC2833Payload
{
	uint8_t event : 8;
	bool ebit : 1;
	bool rbit : 1;
	uint8_t vol : 6;
	uint16_t duration : 16;
};

class MIPRTPTelephonyEventDecoder : public MIPRTPPacketDecoder
{
public:
	MIPRTPTelephonyEventDecoder(){}
	~MIPRTPTelephonyEventDecoder(){}
	std::string							sessionID_;
private:
	bool validatePacket(const jrtplib::RTPPacket *pRTPPack, real_t &timestampUnit, real_t timestampUnitEstimate)
	{
		size_t								length;
		length = (size_t)pRTPPack->GetPayloadLength();
		if ( length < 1 )
			return false;
		return true;
		/*timestampUnit = 1.0/8000.0;
		RFC2833Payload * payload = (RFC2833Payload *)pRTPPack->GetPayloadData();
		printf("event:%d    ebit:%d    rbit:%d    vol:%d    duration:%d", payload->event, payload->ebit, payload->rbit, payload->vol, payload->duration);
		return true;*/
	}
	void createNewMessages(const jrtplib::RTPPacket *pRTPPack, std::list<MIPMediaMessage *> &messages, std::list<uint32_t> &timestamps){}
};


class PlayNode
{
public:
	PlayNode(float);
	~PlayNode();
	void reset();
	void setRTPInfo(const std::string & p_ip, unsigned short p_port, unsigned short p_localport);
	void setWavFileName(const std::string & p_wavfilename);
	void setSessionID(const std::string & p_sessionid);
	void play();
	void stopPlay();
	PlayWAVChain *						chain_;
	MIPAverageTimer *					timer_;
	SIPIVRWAVInput						sndFileInput_;
	MIPSamplingRateConverter			sampConv_;
	MIPSampleEncoder					sampEnc_;
	MIPULawEncoder						uLawEnc_;
	MIPALawEncoder						aLawEnc_;
	MIPRTPULawEncoder					rtpEnc_;
	MIPRTPALawEncoder					rtpEncA_;
	MIPRTPComponent						rtpComp_;
	jrtplib::RTPSession					rtpSession_;
	MIPRTPDecoder						rtpDec_;
	MIPRTPTelephonyEventDecoder			rfc2833Dec_;
	std::string							wavfilename_;
	bool								play_;
	bool								checked_;
	uint32_t							timestamp_;
};

PlayNode::PlayNode(float p_delaysecs)
{
	play_ = false;
	checked_ = false;
	MIPTime interval(p_delaysecs);
	chain_ = new PlayWAVChain("df");
	timer_ = new MIPAverageTimer(interval);
}

PlayNode::~PlayNode()
{
	if ( chain_ )
		delete chain_;
	if ( timer_ )
		delete timer_;
}

void PlayNode::reset()
{
	play_ = false;
	checked_ = false;
	sndFileInput_.sessionid_ = "";
	rfc2833Dec_.sessionID_ = "";
}

void PlayNode::setSessionID(const std::string & p_sessionid)
{
	sndFileInput_.sessionid_ = p_sessionid;
	rfc2833Dec_.sessionID_ = p_sessionid;
}

void PlayNode::setRTPInfo(const std::string & p_ip, unsigned short p_port, unsigned short p_localport)
{
	bool returnValue;
	int samplingRate = 8000;
	int numChannels = 1;

	returnValue = sampConv_.init(samplingRate, numChannels);
	checkError(returnValue, sampConv_);
	returnValue = sampEnc_.init(MIPRAWAUDIOMESSAGE_TYPE_S16);
	checkError(returnValue, sampEnc_);
	returnValue = uLawEnc_.init();
	checkError(returnValue, uLawEnc_);
	returnValue = rtpEnc_.init();
	checkError(returnValue, rtpEnc_);
	RTPUDPv4TransmissionParams transmissionParams;
	RTPSessionParams sessionParams;
	transmissionParams.SetPortbase(p_localport);
	sessionParams.SetOwnTimestampUnit(1.0/((double)samplingRate));
	sessionParams.SetMaximumPacketSize(64000);
	sessionParams.SetAcceptOwnPackets(true);

	returnValue = rtpSession_.Create(sessionParams,&transmissionParams);
	checkError(returnValue);
	returnValue = rtpSession_.AddDestination(RTPIPv4Address(ntohl(inet_addr(p_ip.c_str())),p_port));
	checkError(returnValue);
	returnValue = rtpComp_.init(&rtpSession_);

	checkError(returnValue, rtpComp_);
	returnValue = rtpDec_.init(true, 0, &rtpSession_);
	checkError(returnValue, rtpDec_);

	returnValue = rtpDec_.setPacketDecoder(0, &rfc2833Dec_);
	checkError(returnValue, rtpDec_);
	returnValue = chain_->clearChain();
	checkError(returnValue, *chain_);
	returnValue = chain_->setChainStart(timer_);
	checkError(returnValue, *chain_);
	returnValue = chain_->addConnection(timer_, &sndFileInput_);
	checkError(returnValue, *chain_);
	returnValue = chain_->addConnection(&sndFileInput_, &sampConv_);
	checkError(returnValue, *chain_);
	returnValue = chain_->addConnection(&sampConv_, &sampEnc_);
	checkError(returnValue, *chain_);
	returnValue = chain_->addConnection(&sampEnc_, &uLawEnc_);
	checkError(returnValue, *chain_);
	returnValue = chain_->addConnection(&uLawEnc_, &rtpEnc_);
	checkError(returnValue, *chain_);
	returnValue = chain_->addConnection(&rtpEnc_, &rtpComp_);
	checkError(returnValue, *chain_);
	returnValue = chain_->addConnection(&rtpComp_, &rtpDec_);
	checkError(returnValue, *chain_);
}

void PlayNode::setWavFileName(const std::string & p_wavfilename)
{
	wavfilename_ = p_wavfilename;
	MIPTime interval(0.020);
	sndFileInput_.alreadyLastFrame_ = false;
	sndFileInput_.close();
	sndFileInput_.open(wavfilename_, interval, false, false);
}

void PlayNode::play()
{
	chain_->start();
	play_ = true;
}

void PlayNode::stopPlay()
{
	LOG4CXX_INFO(g_rtpLog,  "stopping play");
	sndFileInput_.setStopFlag(false);
	play_ = false;
}


std::map < std::string, PlayNode * > g_playNodeMap;

ACE_Thread_Mutex					ChannelResourceManager::mutex_;

bool ChannelResourceManager::addNode(const std::string p_sessionid, const std::string & p_callflowtype, float p_delaysecs)
{
	PlayNode *							playnode = new PlayNode(p_delaysecs);
	playnode->setSessionID(p_sessionid);
	playnode->chain_->setSessionID(p_sessionid);
	std::map < std::string, PlayNode * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = g_playNodeMap.find(p_sessionid);
	if ( ite != g_playNodeMap.end() )
		return false;
	g_playNodeMap.insert(std::make_pair< std::string, PlayNode * >(p_sessionid, playnode));
	return true;
}

bool ChannelResourceManager::addNewNode(const std::string p_sessionid, const std::string & p_callflowtype, float p_delaysecs)
{
	PlayNode *							playnode = new PlayNode(p_delaysecs);
	playnode->setSessionID(p_sessionid);
	playnode->chain_->setSessionID(p_sessionid);
	std::map < std::string, PlayNode * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	g_playNodeMap.erase(p_sessionid);
	g_playNodeMap.insert(std::make_pair< std::string, PlayNode * >(p_sessionid, playnode));
	return true;
}

bool ChannelResourceManager::updateNode(const std::string p_sessionid, const std::string p_ip, unsigned short p_port, unsigned short p_localport)
{
	PlayNode *							playnode;
	std::map < std::string, PlayNode * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = g_playNodeMap.find(p_sessionid);
	if ( ite == g_playNodeMap.end() )
		return false;
	playnode = ite->second;
	playnode->setRTPInfo(p_ip, p_port, p_localport);
	return true;
}

bool ChannelResourceManager::delNode(const std::string p_sessionid)
{
	PlayNode *							playnode;
	std::map < std::string, PlayNode * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = g_playNodeMap.find(p_sessionid);
	if ( ite != g_playNodeMap.end() )
	{
		playnode = ite->second;
		playnode->play();
		return true;
	}
	return false;
}

bool ChannelResourceManager::setWAVFile(const std::string & p_sessionid, const std::string & p_wavfilename)
{
	PlayNode *							playnode;
	std::map < std::string, PlayNode * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = g_playNodeMap.find(p_sessionid);
	if ( ite != g_playNodeMap.end() )
	{
		playnode = ite->second;
		playnode->setWavFileName(p_wavfilename);
		return true;
	}
	return false;
}

bool ChannelResourceManager::startPlay(const std::string p_sessionid)
{
	PlayNode *							playnode;
	std::map < std::string, PlayNode * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = g_playNodeMap.find(p_sessionid);
	if ( ite != g_playNodeMap.end() )
	{
		playnode = ite->second;
		playnode->play();
		return true;
	}
	return false;
}

bool ChannelResourceManager::stopPlay(const std::string p_sessionid)
{
	PlayNode *							playnode;
	std::map < std::string, PlayNode * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = g_playNodeMap.find(p_sessionid);
	if ( ite != g_playNodeMap.end() )
	{
		playnode = ite->second;
		playnode->stopPlay();
		return true;
	}
	return false;
}

bool ChannelResourceManager::deleteNode(const std::string p_sessionid)
{
	PlayNode *							playnode;
	std::map < std::string, PlayNode * >::iterator ite;
	SIPIVRLock sipivrLock(mutex_);
	ite = g_playNodeMap.find(p_sessionid);
	if ( ite != g_playNodeMap.end() )
	{
		playnode = ite->second;
		delete playnode;
		g_playNodeMap.erase(ite);
		return true;
	}
	return false;
}

void ChannelResourceManager::getDTMF()
{
	std::map < std::string, PlayNode * >::iterator ite;
	PlayNode *							playnode;
	bool								processed = false;
	std::stringstream					sskey;
	char								key[32];
	while ( 1 )
	{
		processed = false;
		SIPIVRLock sipivrLock(mutex_);
		for ( ite = g_playNodeMap.begin(); ite != g_playNodeMap.end(); ++ite )
		{
			playnode = ite->second;
			if ( playnode->checked_ == false )
			{
				do
				{
					RTPPacket *packet;
					while ( (packet = playnode->rtpSession_.GetNextPacket()) != 0 )
					{
						playnode->checked_ = true;
						processed = true;
						if ( packet->GetPayloadType() == 101 )
						{
							RFC2833Payload * payload = (RFC2833Payload *)packet->GetPayloadData();
							
							sprintf(key, "%d", payload->event);
							printf("timestamp:%d  event:%d    ebit:%d    rbit:%d    vol:%d    duration:%d\r\n", packet->GetTimestamp(), payload->event, payload->ebit, payload->rbit, payload->vol, payload->duration);
							if ( playnode->timestamp_ != packet->GetTimestamp() )
							{
								playnode->timestamp_ = packet->GetTimestamp();
								EventObjHelper::newKeyPressedEventObj(playnode->rfc2833Dec_.sessionID_, key);
							}
						}
						playnode->rtpSession_.DeletePacket(packet);
					}
				} while ( playnode->rtpSession_.GotoFirstSourceWithData());
			}
			if ( processed == true )//只要处理过一个playnode，就可以退出循环，这样可以让出锁以便其他线程可以访问map
				break;
		}
		if ( ite == g_playNodeMap.end() )
		{//某次处理时如果到了map的最后一个，那么将所有playnode的check_字段重置，以便可以再次处理
			for ( ite = g_playNodeMap.begin(); ite != g_playNodeMap.end(); ++ite )
			{
				playnode = ite->second;
				playnode->checked_ = false;
			}
		}
		sipivrLock.release();
		Sleep(50);
	}
}

void getDTMFThreadFunc(void *)
{
	while ( 1 )
	{
		ChannelResourceManager::getDTMF();
	}
}