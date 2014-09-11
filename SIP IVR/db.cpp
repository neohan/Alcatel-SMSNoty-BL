#include "db.hpp"
#include "config.hpp"
#include <log4cxx/logger.h>
#include <strstream>


using namespace log4cxx;
extern LoggerPtr g_dbLog;


int DB::sipinfocallback(void * p_notused, int argc, char **argv, char **azColName)
{
	std::stringstream					ssdbcb;
	std::string							sipserverip, sipbaseport;
	unsigned short						nport;
	int i;
	for ( i = 0; i < argc; i++ )
	{
		ssdbcb << azColName[i] << " = ";
		if ( argv[i] )
			ssdbcb << (char *)&(argv[i][0]);
		else
			ssdbcb << "NULL";
		ssdbcb << "  ";
		if ( strcmp(azColName[i], "sipserverip") == 0 )
			sipserverip = (char *)&(argv[i][0]);
		else if ( strcmp(azColName[i], "sipbaseport") == 0 )
			sipbaseport = (char *)&(argv[i][0]);
	}
	try{nport=atoi(sipbaseport.c_str());}catch(...){nport=6000;}
	ConfigSingleton::instance()->updateSIPInfo(sipserverip, nport);
	LOG4CXX_INFO(g_dbLog,  ssdbcb.str().c_str());
	return 0;
}

int DB::sipextscallback(void * p_notused, int argc, char **argv, char **azColName)
{
	std::stringstream					ssdbcb;
	std::string							sipno, blflow, smflow;
	bool								bblflow, bsmflow;
	int i;
	for ( i = 0; i < argc; i++ )
	{
		ssdbcb << azColName[i] << " = ";
		if ( argv[i] )
			ssdbcb << (char *)&(argv[i][0]);
		else
			ssdbcb << "NULL";
		ssdbcb << "  ";
		if ( strcmp(azColName[i], "sipno") == 0 )
			sipno = (char *)&(argv[i][0]);
		else if ( strcmp(azColName[i], "blacklistflow") == 0 )
			blflow = (char *)&(argv[i][0]);
		else if ( strcmp(azColName[i], "smnotifyflow") == 0 )
			smflow = (char *)&(argv[i][0]);
	}
	if ( blflow == "1" )
		bblflow = true;
	else
		bblflow = false;
	if ( smflow == "1" )
		bsmflow = true;
	else
		bsmflow = false;
	ConfigSingleton::instance()->addSIPExtension(sipno, bblflow, bsmflow);
	LOG4CXX_INFO(g_dbLog,  ssdbcb.str().c_str());
	return 0;
}

int DB::removesipextscallback(void * p_notused, int argc, char **argv, char **azColName)
{
	std::stringstream					ssdbcb;
	std::string							sipno;
	int i;
	for ( i = 0; i < argc; i++ )
	{
		ssdbcb << azColName[i] << " = ";
		if ( argv[i] )
			ssdbcb << (char *)&(argv[i][0]);
		else
			ssdbcb << "NULL";
		ssdbcb << "  ";
		if ( strcmp(azColName[i], "sipno") == 0 )
			sipno = (char *)&(argv[i][0]);
	}
	ConfigSingleton::instance()->addWaitToRemoveSIPExtension(sipno);
	LOG4CXX_INFO(g_dbLog,  ssdbcb.str().c_str());
	return 0;
}

void DB::threadfunc()
{
	std::stringstream ssdb;
	int rc;
	char *								zErrMsg = 0;
	bool								bConfigDBOpened = false;
	while ( true )
	{
		if ( bConfigDBOpened == false )
		{
			rc = sqlite3_open("c:\\sqlitedbs\\config.db", &DBSingleton::instance()->configdb_);
			if( rc )
			{
				ssdb.str("");
				ssdb << "Can't open database: %s\n" << sqlite3_errmsg(DBSingleton::instance()->configdb_);
				LOG4CXX_INFO(g_dbLog,  ssdb.str().c_str());
				sqlite3_close(DBSingleton::instance()->configdb_);
				continue;
			}
			bConfigDBOpened = true;
		}

		if ( bConfigDBOpened )
		{
			LOG4CXX_INFO(g_dbLog,  "check config.db again...\n");
			rc = sqlite3_exec(DBSingleton::instance()->configdb_, "select * from sipinfo", sipinfocallback, 0, &zErrMsg);
			if ( rc != SQLITE_OK )
			{
				ssdb.str(""); ssdb << "sipinfo table  SQL error: " << zErrMsg << "\n";
				LOG4CXX_INFO(g_dbLog,  ssdb.str().c_str());
				sqlite3_free(zErrMsg);
			}

			rc = sqlite3_exec(DBSingleton::instance()->configdb_, "select * from sipextensions", sipextscallback, 0, &zErrMsg);
			if ( rc != SQLITE_OK )
			{
				ssdb.str(""); ssdb << "sipextensions table  SQL error: " << zErrMsg << "\n";
				LOG4CXX_INFO(g_dbLog,  ssdb.str().c_str());
				sqlite3_free(zErrMsg);
			}

			rc = sqlite3_exec(DBSingleton::instance()->configdb_, "select * from waittoremovesipexts", removesipextscallback, 0, &zErrMsg);
			if ( rc != SQLITE_OK )
			{
				ssdb.str(""); ssdb << "waittoremovesipexts table  SQL error: " << zErrMsg << "\n";
				LOG4CXX_INFO(g_dbLog,  ssdb.str().c_str());
				sqlite3_free(zErrMsg);
			}
			rc = sqlite3_exec(DBSingleton::instance()->configdb_, "delete from waittoremovesipexts", 0, 0, &zErrMsg);
			if ( rc != SQLITE_OK )
			{
				ssdb.str(""); ssdb << "clear waittoremovesipexts table  SQL error: " << zErrMsg << "\n";
				LOG4CXX_INFO(g_dbLog,  ssdb.str().c_str());
				sqlite3_free(zErrMsg);
			}
			LOG4CXX_INFO(g_dbLog,  "check config.db finished.\n");
		}
		Sleep(10000000);
	}
}