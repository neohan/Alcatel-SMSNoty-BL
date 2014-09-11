//#include "config.hpp"
#include "Daemon.hpp"
#include "config.hpp"
#include "db.hpp"
#include "sofia.hpp"
#include "callflowmgr.hpp"
#include "playnode.hpp"
#include <log4cxx/logger.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/basicconfigurator.h>
#include "ace/Thread_Manager.h"
#include <conio.h>
#include <comutil.h>
#include "ThreadSafeQueue.h"
#include <list>
#include <windows.h>


using namespace log4cxx;
LoggerPtr g_appLog;
LoggerPtr g_sipLog;
LoggerPtr g_dbLog;
LoggerPtr g_dbspLog;
LoggerPtr g_callflowLog;
LoggerPtr g_rtpLog;
LoggerPtr emipLog;

static volatile bool serviceStop = false;

HANDLE closingEvent = NULL;


void StopHandler()
{
	serviceStop = true;
}


#ifdef WIN32
long ExceptionFilter(struct _EXCEPTION_POINTERS *ptr)
{
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif



void MainThread()
{
	if ( !ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(SofiaUASingleton::instance()->threadfunc), (void *)0) )
		return;
	if ( !ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(Config::threadfunc), (void *)0) )
		return;
	if ( !ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(DB::threadfunc), (void *)0) )
		return;
	if ( !ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(CallFlowProcess::threadfunc), (void *)0) )
		return;
	if ( !ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(voiceThreadFunc), (void *)0) )
		return;
	if ( !ACE_Thread_Manager::instance()->spawn(ACE_THR_FUNC(getDTMFThreadFunc), (void *)0) )
		return;
	
	Sleep(1000);
	while ( !DaemonSingleton::instance()->IsStopping() )
	{
		Sleep(5000);
		DWORD ret = WaitForSingleObject(closingEvent, 1);
		if ( ret == WAIT_OBJECT_0 )
			{LOG4CXX_INFO(g_appLog,  "Closing event is set.MainThread");break;}
		continue;
	}
}


void SetCurrPath(void)
{
	char								buf[MAX_PATH];
	char								* pright;


	GetModuleFileName(NULL, buf, MAX_PATH);

	pright = strrchr(buf, '\\');

	if ( pright )
		*pright = '\0';

	SetCurrentDirectory(buf);
}


int main(int argc, char * argv[])
{
	closingEvent = CreateEvent(NULL, TRUE, FALSE, "");
	SetCurrPath();
	BasicConfigurator::configure();
	PropertyConfigurator::configure("log4cxx.properties");
	g_appLog  = Logger::getLogger("zgsvr");
	g_sipLog = Logger::getLogger("sip");
	g_dbLog = Logger::getLogger("db");
	g_dbspLog = Logger::getLogger("dbsp");
	g_callflowLog = Logger::getLogger("callflow");
	g_rtpLog = Logger::getLogger("rtp");
	emipLog = Logger::getLogger("emiplog");
	LOG4CXX_INFO(g_appLog, "zg starting...");


	std::string						serviceName = ("alcatelsmsnotyblsipivr");
	std::string						serviceDisplayName = ("SIP IVR");
	std::string						serviceDisplayInfo = ("Alcatel SMSNoty BL - SIP IVR¡£");
	DaemonSingleton::instance()->Initialize(serviceName, serviceDisplayName, serviceDisplayInfo, MainThread, StopHandler);	

	if ( argc > 1 )
	{
		std::string argument = argv[1];
		if ( argument == "debug" )
		{
			MainThread();printf("\r\nmainthread exit");
		}
		else if ( argument == "install" )
		{
			DaemonSingleton::instance()->Install();
		}
		else if ( argument == "remove" )
		{
			DaemonSingleton::instance()->Uninstall();
		}
		else
		{
#ifdef WIN32
			printf("Argument incorrect. Possibilies are:\n\tinstall:\t\tinstall NT service\n\tuninstall:\t\tuninstall NT service\n\n");
#else
			printf("Argument incorrect. Possibilies are:\n\tdebug:\trun attached to tty\n\n");
#endif
		}
	}
	else
	{
		// No arguments, launch the daemon
		printf("Starting service daemon ...\n");
		DaemonSingleton::instance()->Start();
	}
	return 0;
}