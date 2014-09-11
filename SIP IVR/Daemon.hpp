#ifndef DOUGHAN_DAEMON_20110606__HPP__
#define DOUGHAN_DAEMON_20110606__HPP__




#include "ace/Singleton.h"
#include "ace/Thread_Mutex.h"
#include <string>


typedef void (*DaemonHandler)(void);


class Daemon
{
public:
	//void Initialize(std::string serviceName, void (*RunHandler)(void), void(*StopHandler)(void));
	void Initialize(std::string serviceName, std::string serviceDisplayName, std::string serviceDisplayInfo, DaemonHandler runHandler, DaemonHandler stopHandler);
	void Start();
	void Stop();
	void Install();
	void Uninstall();
	bool IsStopping();


private:
#ifdef WIN32
	static void WINAPI Run( DWORD /*argc*/, TCHAR* /*argv*/[] );
#else
	static void Run();
#endif
	void UpdateServiceDisplayInfo();


	DaemonHandler						m_runHandler;
	DaemonHandler						m_stopHandler;
	std::string							m_serviceName;
	std::string							m_serviceDisplayName;
	std::string							m_serviceDisplayInfo;
	bool								m_stopping;
};

typedef ACE_Singleton<Daemon, ACE_Thread_Mutex> DaemonSingleton;


#endif