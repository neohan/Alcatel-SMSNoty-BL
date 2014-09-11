#ifndef DOUGHAN_ALCATELSMSNOTYBL_LOCK_20140813__HPP__
#define DOUGHAN_ALCATELSMSNOTYBL_LOCK_20140813__HPP__


#include "ace/Guard_T.h"
#include "ace/Thread_Mutex.h"


//=====================================================
// threading related stuff

typedef ACE_Guard<ACE_Thread_Mutex> SIPIVRLock;


#endif