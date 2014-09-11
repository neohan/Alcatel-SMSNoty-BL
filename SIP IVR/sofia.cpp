#include "sofia.hpp"
#include "constvals.hpp"
#include "config.hpp"
#include "lock.h"
#include "callflowmgr.hpp"
#include <log4cxx/logger.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/msg.h>
#include <sofia-sip/msg_header.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/tport_tag.h>
#include <strstream>


using namespace log4cxx;
extern LoggerPtr g_sipLog;

std::map < nua_handle_t *, std::string > nuahandleSIPCallIDMap;


struct SessionInfo_s
{
char									callid_[256];
};

void event_callback(nua_event_t   event,
					int           status,
					char const   *phrase,
					nua_t        *nua,
					nua_magic_t  *magic,
					nua_handle_t *nh,
					nua_hmagic_t *hmagic,
					sip_t const  *sip,
					tagi_t        tags[]);

void SofiaUA::init()
{
	su_init();
	su_home_init(home_);
	root_ = su_root_create(NULL);
	std::string urlstr = "sip:0.0.0.0:5060";
	nua_ = nua_create(root_, event_callback, NULL, NUTAG_URL(urlstr.c_str()), TAG_END());
	nua_set_params(nua_, TPTAG_LOG(1), TAG_END());
	nua_set_params(nua_, NUTAG_ALLOW("INFO"), TAG_END());
}

void SofiaUA::uninit()
{
	nua_destroy(nua_);
	su_root_destroy(root_);
	su_deinit();  
}

std::stringstream ssg;
/* This callback will be called by SIP stack to process incoming events.
	此函数在此模块线程内被调用。
*/
void event_callback(nua_event_t   event,
					int           status,
					char const   *phrase,
					nua_t        *nua,
					nua_magic_t  *magic,
					nua_handle_t *nh,
					nua_hmagic_t *hmagic,
					sip_t const  *sip,
					tagi_t        tags[])
{
	SessionInfo_s *						sessioninfo;
	ssg.str("");
	ssg <<"received an event " << nua_event_name(event) << ", status " << status << ", " << phrase << SUFIX_PER_LOG_LINE;
	if ( sip )
		ssg << PREFIX_PER_LOG_LINE << "call id :" << sip->sip_call_id->i_id << SUFIX_PER_LOG_LINE;
	if ( hmagic )
	{
		sessioninfo = (SessionInfo_s *)hmagic;
		ssg << PREFIX_PER_LOG_LINE << "call id(hmagic) :" << sessioninfo->callid_ << SUFIX_PER_LOG_LINE;
	}
	switch ( event )
	{
		case nua_i_info:
			break;

		case nua_r_register:
			if ( status == 401 )
			{
				if ( sip && sip->sip_www_authenticate && sip->sip_www_authenticate->au_params )
				{
					std::string no = SofiaUASingleton::instance()->getRegisterRequestSIPNo(nh);
					if ( no.length() > 0 )
					{
						sip_www_authenticate_t const* www_auth = sip->sip_www_authenticate;
						char const* scheme = www_auth->au_scheme;
						const char* realm = msg_params_find(www_auth->au_params, "realm=");
						char auth_str[8192] = { 0 };
						printf ("scheme:%s  au_params:%s\n", sip->sip_www_authenticate->au_scheme, sip->sip_www_authenticate->au_params);
						snprintf(auth_str, 8192, "%s:%s:1001:1234", scheme, realm);
						nua_authenticate(nh, NUTAG_AUTH(auth_str), TAG_END());
						ssg << PREFIX_PER_LOG_LINE << "sip:" << no << SUFIX_PER_LOG_LINE;
					}
					else
						ssg << PREFIX_PER_LOG_LINE << "cannot find sip no by handle:" << nh << SUFIX_PER_LOG_LINE;
				}
			}
			else if ( status == 200 )
			{
				std::string no = SofiaUASingleton::instance()->getAndRemoveRegisterRequestSIPNo(nh);
				ssg << PREFIX_PER_LOG_LINE << "sip:" << no << "    remove sipno-handle map" << SUFIX_PER_LOG_LINE;
				ConfigSingleton::instance()->setSIPExtensionRegisterFlag(no, true);
			}
			break;

		case nua_r_unregister:
			if ( status == 401 )
			{
				if ( sip && sip->sip_www_authenticate && sip->sip_www_authenticate->au_params )
				{
					std::string no = SofiaUASingleton::instance()->getUnregisterRequestSIPNo(nh);
					if ( no.length() > 0 )
					{
						sip_www_authenticate_t const* www_auth = sip->sip_www_authenticate;
						char const* scheme = www_auth->au_scheme;
						const char* realm = msg_params_find(www_auth->au_params, "realm=");
						char auth_str[8192] = { 0 };
						printf ("scheme:%s  au_params:%s\n", sip->sip_www_authenticate->au_scheme, sip->sip_www_authenticate->au_params);
						snprintf(auth_str, 8192, "%s:%s:%s:1234", scheme, realm, no.c_str());
						nua_authenticate(nh, NUTAG_AUTH(auth_str), TAG_END());
						ssg << PREFIX_PER_LOG_LINE << "sip:" << no << "\r\n";
					}
					else
						ssg << PREFIX_PER_LOG_LINE << "cannot find sip no by handle:" << nh << "\r\n";
				}
			}
			else if ( status == 200 )
			{
				std::string no = SofiaUASingleton::instance()->getAndRemoveUnregisterRequestSIPNo(nh);
				ssg << PREFIX_PER_LOG_LINE << "sip:" << no << "    remove sipno-handle map\r\n";
				nua_handle_destroy(nh);
				ConfigSingleton::instance()->setSIPExtensionRegisterFlag(no, false);
			}
			break;

		case nua_i_invite:
			{
			if ( hmagic == NULL )
			{
				SessionInfo_s * sessioninfo = new SessionInfo_s;
				if ( sip && sip->sip_call_id && sip->sip_call_id->i_id )
				{
					if ( strlen(sip->sip_call_id->i_id) < 256 )
						strcpy(sessioninfo->callid_, sip->sip_call_id->i_id);
					else
						strncpy(sessioninfo->callid_, sip->sip_call_id->i_id, 255);
				}
				nua_handle_bind(nh, sessioninfo);
				ssg << PREFIX_PER_LOG_LINE <<"  bind SessionInfo_s(" << sessioninfo << ") to nua_handle(" << nh << ")" << SUFIX_PER_LOG_LINE;
			}
			if ( sip && sip->sip_from )
			{
				ssg << PREFIX_PER_LOG_LINE << "FROM Header";
				if ( sip->sip_from->a_display )
					ssg << "  display:" << sip->sip_from->a_display;
				if ( sip->sip_from->a_url->url_type == url_sip )
					ssg << "  url-type:" << sip->sip_from->a_url->url_scheme;
				if ( sip->sip_from->a_url->url_user )
					ssg << "  url-user:" << sip->sip_from->a_url->url_user;
				if ( sip->sip_from->a_comment )
					ssg << "  comment:" << sip->sip_from->a_comment;
				if ( sip->sip_from->a_tag )
					ssg << "  tag:" << sip->sip_from->a_tag;
				ssg << SUFIX_PER_LOG_LINE;
			}
			if ( sip && sip->sip_to )
			{
				ssg << PREFIX_PER_LOG_LINE << "TO Header";
				if ( sip->sip_to->a_display )
					ssg << " display:" << sip->sip_to->a_display;
				if ( sip->sip_to->a_url->url_type == url_sip )
					ssg << "  url-type:" << sip->sip_to->a_url->url_scheme;
				if ( sip->sip_to->a_comment )
					ssg << "  comment:" << sip->sip_to->a_comment;
				if ( sip->sip_to->a_tag )
					ssg << "  tag:" << sip->sip_to->a_tag;
				ssg << SUFIX_PER_LOG_LINE;
			}
			if ( sip && sip->sip_call_id && sip->sip_call_id->i_id )
			{
				EventObj * eventobj = new EventObj;
				eventobj->eventName_ = "call.incomming";
				eventobj->callid_ = sip->sip_call_id->i_id;
				eventobj->nh_ = nh;
				if ( sip->sip_to && sip->sip_to->a_url && sip->sip_to->a_url->url_user )
					eventobj->dnis_ = sip->sip_to->a_url->url_user;
				CallFlowProcessSingleton::instance()->pushEventObj(eventobj);
				ssg << PREFIX_PER_LOG_LINE << "create call.incomming EventObj" << SUFIX_PER_LOG_LINE;
			}
			}
			break;

		case nua_i_state:
			if ( status == 180 )
			{
				if ( sessioninfo )
				{
					EventObj * eventobj = new EventObj;
					eventobj->callid_ = sessioninfo->callid_;
					eventobj->eventName_ = "call.ringing";
					eventobj->nh_ = nh;
					CallFlowProcessSingleton::instance()->pushEventObj(eventobj);
					ssg << PREFIX_PER_LOG_LINE << "create call.ringring EventObj" << SUFIX_PER_LOG_LINE;
				}
			}
			else if ( status == 100 )
			{
				const char * p1stPos;
				const char * p2ndPos;
				char findStr[512], ipAddress[32];
				char const *r_sdp = NULL;
				char const *call_id = NULL;
				unsigned short port;

				tl_gets(tags, SIPTAG_CALL_ID_STR_REF(call_id), TAG_END());
				tl_gets(tags, SOATAG_REMOTE_SDP_STR_REF(r_sdp), TAG_END());

				p1stPos = strstr(r_sdp, "c=IN IP4 ");
				p2ndPos = strstr(p1stPos, "\n");
				if ( p1stPos && p2ndPos )
				{
					memset(ipAddress, 0, 32);
					strncpy(ipAddress, p1stPos + 9, p2ndPos - p1stPos - 9);
				}

				p1stPos = strstr(r_sdp, "m=audio ");
				p2ndPos = strstr(p1stPos + 8, " ");
				if ( p1stPos && p2ndPos )
				{
					memset(findStr, 0, 512);
					strncpy(findStr, p1stPos + 8, p2ndPos - p1stPos - 8);
					port = atoi(findStr);
				}

				if ( sessioninfo )
				{
					EventObj * eventobj = new EventObj;
					eventobj->eventName_ = "call.ringing.sdp";
					eventobj->callid_ = sessioninfo->callid_;
					eventobj->nh_ = nh;
					eventobj->ip_ = ipAddress;
					eventobj->port_ = port;
					CallFlowProcessSingleton::instance()->pushEventObj(eventobj);
					ssg << PREFIX_PER_LOG_LINE << "create call.ringring.sdp EventObj.  ip:" << ipAddress << ".  port:" << port << SUFIX_PER_LOG_LINE;
				}
			}
			break;

		case nua_i_active:
			if ( status == 200 )
			{
				if ( sessioninfo )
				{
					EventObj * eventobj = new EventObj;
					eventobj->callid_ = sessioninfo->callid_;
					eventobj->eventName_ = "call.connect";
					eventobj->nh_ = nh;
					CallFlowProcessSingleton::instance()->pushEventObj(eventobj);
					ssg << PREFIX_PER_LOG_LINE << "create call.connect EventObj" << SUFIX_PER_LOG_LINE;
				}
				else
					ssg << PREFIX_PER_LOG_LINE << "no hmagic, no SessionInfo_s" << SUFIX_PER_LOG_LINE;
			}
			break;

		case nua_i_terminated:
			if ( status == 200 )
			{
				if ( sessioninfo )
				{
					EventObj * eventobj = new EventObj;
					eventobj->callid_ = sessioninfo->callid_;
					eventobj->eventName_ = "call.disconnect";
					eventobj->nh_ = nh;
					CallFlowProcessSingleton::instance()->pushEventObj(eventobj);
					ssg << PREFIX_PER_LOG_LINE << "create call.disconnect EventObj" << SUFIX_PER_LOG_LINE;
				}
				else
					ssg << PREFIX_PER_LOG_LINE << "no hmagic, no SessionInfo_s" << SUFIX_PER_LOG_LINE;
			}
			break;
	}
	LOG4CXX_INFO(g_sipLog,  ssg.str().c_str());
}

void SofiaUA::createUA(const std::string & p_no, const std::string & p_serverip)
{
	std::stringstream ss;
	std::string fromstr = "sip:" + p_no + "@" + p_serverip;

	sip_to_t *							to;
	to = sip_to_create(home_, (const url_string_t *)(fromstr.c_str()));
	if ( url_sanitize(to->a_url) < 0 )
	{
		ss << fromstr << " invalid address\n";
		LOG4CXX_INFO(g_sipLog,  ss.str().c_str());
		return;
	}
	nua_handle_t * nuahandle = nua_handle(nua_, NULL, SIPTAG_TO(to), TAG_END());
	su_free(home_, to);
	ss << "create ua success. from:" << fromstr << "\ncreate sipno(" << p_no << ")-handle(" << nuahandle << ") map\r\n";
	if ( addRegisterRequest(nuahandle, (std::string)p_no) == false )
		ss << "addRegisterRequest return false\r\n";
	nua_register(nuahandle, NUTAG_M_FEATURES("expires=180"), TAG_NULL());
	LOG4CXX_INFO(g_sipLog,  ss.str().c_str());
}

bool SofiaUA::addRegisterRequest(nua_handle_t * p_handle, std::string & p_sipno)
{
	SIPIVRLock sipivrLock(mutex_);
	std::map< nua_handle_t *, std::string >::iterator ite = registerReqMap_.find(p_handle);
	if ( ite != registerReqMap_.end() )
		return false;
	registerReqMap_.insert(std::make_pair< nua_handle_t *, std::string >(p_handle, p_sipno));
	return true;
}

std::string SofiaUA::getRegisterRequestSIPNo(nua_handle_t * p_handle)
{
	SIPIVRLock sipivrLock(mutex_);
	std::map< nua_handle_t *, std::string >::iterator ite = registerReqMap_.find(p_handle);
	if ( ite == registerReqMap_.end() )
		return "";
	std::string sipno = ite->second;
	return sipno;
}

std::string SofiaUA::getAndRemoveRegisterRequestSIPNo(nua_handle_t * p_handle)
{
	SIPIVRLock sipivrLock(mutex_);
	std::map< nua_handle_t *, std::string >::iterator ite = registerReqMap_.find(p_handle);
	if ( ite == registerReqMap_.end() )
		return "";
	std::string sipno = ite->second;
	registerReqMap_.erase(ite);
	return sipno;
}

bool SofiaUA::addUnregisterRequest(nua_handle_t * p_handle, std::string & p_sipno)
{
	SIPIVRLock sipivrLock(mutex_);
	std::map< nua_handle_t *, std::string >::iterator ite = unregisterReqMap_.find(p_handle);
	if ( ite != unregisterReqMap_.end() )
		return false;
	unregisterReqMap_.insert(std::make_pair< nua_handle_t *, std::string >(p_handle, p_sipno));
	return true;
}

std::string SofiaUA::getUnregisterRequestSIPNo(nua_handle_t * p_handle)
{
	SIPIVRLock sipivrLock(mutex_);
	std::map< nua_handle_t *, std::string >::iterator ite = unregisterReqMap_.find(p_handle);
	if ( ite == unregisterReqMap_.end() )
		return "";
	std::string sipno = ite->second;
	return sipno;
}

std::string SofiaUA::getAndRemoveUnregisterRequestSIPNo(nua_handle_t * p_handle)
{
	SIPIVRLock sipivrLock(mutex_);
	std::map< nua_handle_t *, std::string >::iterator ite = unregisterReqMap_.find(p_handle);
	if ( ite == unregisterReqMap_.end() )
		return "";
	std::string sipno = ite->second;
	unregisterReqMap_.erase(ite);
	return sipno;
}

void SofiaUA::removeUA(const std::string & p_no, const std::string & p_serverip)
{
	std::string fromstr = "sip:" + p_no + "@" + p_serverip;
	nua_handle_t * nuahandle = nua_handle(nua_, NULL, SIPTAG_FROM_STR(fromstr.c_str()), TAG_END());
	nua_unregister(nuahandle, TAG_NULL());
	std::stringstream ss;
	ss << "remove ua success. from:" << fromstr << "\ncreate sipno(" << p_no << ")-handle(" << nuahandle << ") map\r\n";
	if ( addUnregisterRequest(nuahandle, (std::string)p_no) == false )
		ss << "addUnregisterRequest return false\r\n";
	LOG4CXX_INFO(g_sipLog,  ss.str().c_str());
}

void SofiaUA::threadfunc()
{
	SofiaUASingleton::instance()->init();
	su_root_run(SofiaUASingleton::instance()->root_);
	SofiaUASingleton::instance()->uninit();
}