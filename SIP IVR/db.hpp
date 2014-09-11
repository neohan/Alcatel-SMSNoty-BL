#ifndef DOUGHAN_ALCATELSMSNOTYBL_DB_20140818__HPP__
#define DOUGHAN_ALCATELSMSNOTYBL_DB_20140818__HPP__


#include "ace/Singleton.h"
#include "ace/Thread_Mutex.h"
#include "sqlite3.h"


class DB
{
private:


public:
	sqlite3 *							configdb_;
	sqlite3 *							realtimedb_;
	sqlite3 *							historydb_;
	static int sipinfocallback(void * p_notused, int argc, char **argv, char **azColName);
	static int sipextscallback(void * p_notused, int argc, char **argv, char **azColName);
	static int removesipextscallback(void * p_notused, int argc, char **argv, char **azColName);
	static void threadfunc();
};

typedef ACE_Singleton<DB, ACE_Thread_Mutex> DBSingleton;


#endif