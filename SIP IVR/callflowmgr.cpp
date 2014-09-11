#include "callflowmgr.hpp"
#include "session.hpp"
#include "config.hpp"
#include "sofia.hpp"
#include "lock.h"
#include "constvals.hpp"
#include <sofia-sip/sip_status.h>
#include <log4cxx/logger.h>
#include <sstream>

using namespace log4cxx;
extern LoggerPtr g_callflowLog;


std::map< std::string, TimeoutObj * > CallFlowProcess::timeoutObjs_;


void CallFlowProcess::pushEventObj(EventObj * p_eventobj)
{
	SIPIVRLock sipivrLock(mutex_);
	eventObjs_.push_back(p_eventobj);
}

void CallFlowProcess::threadfunc(void *)
{
	std::deque < EventObj * >::iterator ite;
	std::map< std::string, TimeoutObj * >::iterator itetimeout;
	EventObj *							eventobj;
	TimeoutObj *						timeoutobj;
	std::stringstream					sscallflow, sssdp;
	std::string							status, flowtype, callid;
	unsigned short						localport;
	DWORD								nowtickcount;
	while ( 1 )
	{
		SIPIVRLock sipivrLock(CallFlowProcessSingleton::instance()->mutex_);
		if ( CallFlowProcSingleton->eventObjs_.size() == 0 )
		{
			sipivrLock.release();
			callid = "";
			for ( itetimeout = timeoutObjs_.begin(); itetimeout != timeoutObjs_.end(); ++itetimeout )
			{
				timeoutobj = itetimeout->second;
				nowtickcount = GetTickCount();
				if ( ( nowtickcount - timeoutobj->tickcount_ ) > timeoutobj->timeoutmsecs_ )
				{
					callid = itetimeout->first;
					delete timeoutobj;
					timeoutObjs_.erase(itetimeout);
					break;
				}
			}
			sscallflow.str("");
			if ( callid.length() > 0 )
			{
				flowtype = SessMgrSingleton->getSessionFlowType(callid);
				status = SessMgrSingleton->getSessionFlowStatus(callid);
				if ( flowtype == "directsm" )
				{
					if ( status == "getkey1" )
					{
						SessMgrSingleton->deletePlayNode(callid);
						SessMgrSingleton->plusPlay1Times(callid);
						int play1times = SessMgrSingleton->getPlay1Times(callid);
						if ( play1times == -1 )
							sscallflow << "timeout occurs, status is getkey1, cannt find session by callid:" << callid << SUFIX_PER_LOG_LINE;
						else
						{
							std::string filename, status;
							sscallflow << "timeout occurs, call id:" << callid << SUFIX_PER_LOG_LINE;
							SessMgrSingleton->deletePlayNode(callid);
							if ( play1times > 3 )
							{
								status = "playnoinputbye";
								filename = "NoInputBye.wav";
								sscallflow << "play1 times exceed limit" << SUFIX_PER_LOG_LINE;
							}
							else
							{
								status = "play1";
								filename = "InputNotifyedPhoneNo.wav";
								sscallflow << SUFIX_PER_LOG_LINE;
							}
							SessMgrSingleton->setSessionRTPInfo(callid, 0.02);
							SessMgrSingleton->setSessionFlowStatus(callid, status);
							SessMgrSingleton->setWAVFile(callid, filename);
							SessMgrSingleton->startPlay(callid);
							sscallflow << PREFIX_PER_LOG_LINE << "status change to " << status << ", play " << filename << SUFIX_PER_LOG_LINE;
						}
					}
				}
			}
			LOG4CXX_INFO(g_callflowLog,  sscallflow.str().c_str());
			Sleep(50);
			continue;
		}
		else
		{
			ite = CallFlowProcSingleton->eventObjs_.begin();
			eventobj = *ite;
			CallFlowProcSingleton->eventObjs_.pop_front();
			sscallflow.str("");
			sscallflow << "event name:" << eventobj->eventName_ << "  call id:" << eventobj->callid_ << SUFIX_PER_LOG_LINE;
			flowtype = SessMgrSingleton->getSessionFlowType(eventobj->callid_);
			status = SessMgrSingleton->getSessionFlowStatus(eventobj->callid_);
			sscallflow << PREFIX_PER_LOG_LINE << "flow type:" << flowtype << "  status:" << status << SUFIX_PER_LOG_LINE;
			if ( eventobj->eventName_ == "call.incomming" )
			{
				if ( ConfSingleton->haveBlacklistFeature() &&
						ConfSingleton->sipExtensionHaveBlacklistFeature(eventobj->dnis_) )
				{
					sscallflow << PREFIX_PER_LOG_LINE << "dnis:" << eventobj->dnis_ << " has black list feature." << SUFIX_PER_LOG_LINE
								<< PREFIX_PER_LOG_LINE << "  plan to answer it." << SUFIX_PER_LOG_LINE;
					SessMgrSingleton->newSession(eventobj->callid_, "blacklist");
					nua_respond(eventobj->nh_, SIP_180_RINGING, TAG_END());
				}
				else if ( true || (ConfSingleton->haveSMNotifyFeature() &&
							ConfSingleton->sipExtensionHaveSMNotifyFeature(eventobj->dnis_)) )
				{
					sscallflow << PREFIX_PER_LOG_LINE << "dnis:" << eventobj->dnis_ << " has smnotify feature." << SUFIX_PER_LOG_LINE
								<< PREFIX_PER_LOG_LINE << "  plan to answer it." << SUFIX_PER_LOG_LINE;
					SessMgrSingleton->newSession(eventobj->callid_, "directsm");
					nua_respond(eventobj->nh_, SIP_180_RINGING, TAG_END());
				}
			}
			else if ( eventobj->eventName_ == "call.ringing.sdp" )
			{
				SessMgrSingleton->setSessionRTPInfo(eventobj->callid_, 0.02, eventobj->ip_, eventobj->port_);
				sscallflow << PREFIX_PER_LOG_LINE << "set remote rtp info for    ip:" << eventobj->ip_
							<< " port:" << eventobj->port_ << SUFIX_PER_LOG_LINE;
			}
			else if ( eventobj->eventName_ == "call.ringing" )
			{
				localport = SessMgrSingleton->getPort(eventobj->callid_);
				if ( localport == 0 )
				{
					sscallflow << PREFIX_PER_LOG_LINE << "get port error, cannot find session by callid. cannt answer the call." << SUFIX_PER_LOG_LINE;
				}
				else
				{
					sssdp.str("");
					sssdp << "s=Orange 1 release 1.0.0  stamp 71236\nc=IN IP4 192.168.77.168\nm=audio " << localport <<" RTP/AVP 0\na=rtpmap:0 PCMU/8000\na=fmtp:101 0-15\na=sendrecv\n";
					nua_respond(eventobj->nh_, SIP_200_OK, SOATAG_AUDIO_AUX("telephone-event"),
								SOATAG_USER_SDP_STR(sssdp.str().c_str()), TAG_END());
					sscallflow << PREFIX_PER_LOG_LINE << "answer the call" << SUFIX_PER_LOG_LINE
								<< PREFIX_PER_LOG_LINE << "sdp which send to server:" << sssdp.str() << SUFIX_PER_LOG_LINE;
				}
			}
			else if ( eventobj->eventName_ == "call.connect" )
			{
				if ( flowtype == "blacklist" )
				{
					SessMgrSingleton->setWAVFile(eventobj->callid_, "NoPrivilege.wav");
					SessMgrSingleton->startPlay(eventobj->callid_);
					sscallflow << PREFIX_PER_LOG_LINE << "play NoPrivilege.wav" << SUFIX_PER_LOG_LINE;
				}
				else if ( flowtype == "directsm" )
				{
					SessMgrSingleton->resetPlay1Times(eventobj->callid_);
					SessMgrSingleton->plusPlay1Times(eventobj->callid_);
					SessMgrSingleton->setSessionFlowStatus(eventobj->callid_, "play1");
					SessMgrSingleton->setWAVFile(eventobj->callid_, "InputNotifyedPhoneNo.wav");
					SessMgrSingleton->startPlay(eventobj->callid_);
					sscallflow << PREFIX_PER_LOG_LINE << "play InputNotifyedPhoneNo.wav" << SUFIX_PER_LOG_LINE;
				}
			}
			else if ( eventobj->eventName_ == "play.done" )
			{
				if ( flowtype == "blacklist" )
				{
					SessMgrSingleton->deletePlayNode(eventobj->callid_);
					nua_handle_t * nuahandle = nua_handle_by_call_id(SofiaSingleton->nua_, eventobj->callid_.c_str());
					if ( nuahandle )
					{
						nua_bye(nuahandle, TAG_END());
						sscallflow << PREFIX_PER_LOG_LINE << "send BYE sip message" << SUFIX_PER_LOG_LINE;
					}
					else
						sscallflow << PREFIX_PER_LOG_LINE
									<< "cannt send BYE sip message, because cannt find nua_handle_t by callid"
									<< SUFIX_PER_LOG_LINE;
				}
				else if ( flowtype == "directsm" )
				{
					SessMgrSingleton->deletePlayNode(eventobj->callid_);
					if ( status == "play1" )
					{
						SessMgrSingleton->stopPlay(eventobj->callid_);
						SessMgrSingleton->setSessionFlowStatus(eventobj->callid_, "getkey1");
						newTimeoutObj(eventobj->callid_);
						sscallflow << PREFIX_PER_LOG_LINE << "status change to getkey1, new timeout for getkey1" << SUFIX_PER_LOG_LINE;
					}
					else if ( status == "playnoinputbye" )
					{
						nua_handle_t * nuahandle = nua_handle_by_call_id(SofiaSingleton->nua_, eventobj->callid_.c_str());
						if ( nuahandle )
						{
							nua_bye(nuahandle, TAG_END());
							sscallflow << PREFIX_PER_LOG_LINE << "send BYE sip message" << SUFIX_PER_LOG_LINE;
						}
						else
							sscallflow << PREFIX_PER_LOG_LINE
										<< "cannt send BYE sip message, because cannt find nua_handle_t by callid"
										<< SUFIX_PER_LOG_LINE;
					}
					else
						sscallflow << PREFIX_PER_LOG_LINE << "no more action" << SUFIX_PER_LOG_LINE;
				}
			}
			else if ( eventobj->eventName_ == "call.dtmfypressed" )
			{
				sscallflow << PREFIX_PER_LOG_LINE << "key:" << eventobj->key_ << SUFIX_PER_LOG_LINE;
				if ( flowtype == "directsm" )
				{
					if ( status == "play1" )
					{
						SessMgrSingleton->stopPlay(eventobj->callid_);
						SessMgrSingleton->clearKeypressed(eventobj->callid_);
						if ( eventobj->key_ == "11" )
						{

						}
						else
						{
							SessMgrSingleton->setSessionFlowStatus(eventobj->callid_, "getkey1");
							newTimeoutObj(eventobj->callid_);
							sscallflow << PREFIX_PER_LOG_LINE << "status change to getkey1, new timeout for getkey1" << SUFIX_PER_LOG_LINE;
						}
					}
					else if ( status == "getkey1" )
					{
							std::string keypressed;
							SessMgrSingleton->getKeypressed(eventobj->callid_, &keypressed);
						nua_handle_t * nuahandle = nua_handle_by_call_id(SofiaSingleton->nua_, eventobj->callid_.c_str());
						if ( nuahandle )
						{
							nua_bye(nuahandle, TAG_END());
							sscallflow << PREFIX_PER_LOG_LINE << "send BYE sip message" << SUFIX_PER_LOG_LINE;
						}
						else
							sscallflow << PREFIX_PER_LOG_LINE
										<< "cannt send BYE sip message, because cannt find nua_handle_t by callid"
										<< SUFIX_PER_LOG_LINE;
					}
					else
						sscallflow << PREFIX_PER_LOG_LINE << "no more action" << SUFIX_PER_LOG_LINE;
				}
			}
			else
				sscallflow << PREFIX_PER_LOG_LINE << "no more action for the event" << SUFIX_PER_LOG_LINE;

			sipivrLock.release();
			delete eventobj;
			LOG4CXX_INFO(g_callflowLog,  sscallflow.str().c_str());
		}
	}
}

void CallFlowProcess::newTimeoutObj(const std::string & p_sessionid)
{
	TimeoutObj * timeoutobj = new TimeoutObj;
	timeoutobj->tickcount_ = GetTickCount();
	timeoutobj->timeoutmsecs_ = 3000;
	timeoutObjs_.insert(std::make_pair< std::string, TimeoutObj * >(p_sessionid, timeoutobj));
}

void CallFlowProcess::removeTimeoutObj(const std::string & p_sessionid)
{
	TimeoutObj * timeoutobj;
	std::map< std::string, TimeoutObj * >::iterator ite;
	ite = timeoutObjs_.find(p_sessionid);
	if ( ite != timeoutObjs_.end() )
	{
		timeoutobj = ite->second;
		delete timeoutobj;
		timeoutObjs_.erase(ite);
	}
}

void CallFlowProcess::delayTimeoutObj(const std::string & p_sessionid)
{
	TimeoutObj * timeoutobj;
	std::map< std::string, TimeoutObj * >::iterator ite;
	ite = timeoutObjs_.find(p_sessionid);
	if ( ite != timeoutObjs_.end() )
	{
		timeoutobj = ite->second;
		timeoutobj->tickcount_ = GetTickCount();
	}
}