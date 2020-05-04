#ifndef _PROTOKIT
#define _PROTOKIT

#include "protokit/protoVersion.h"
#include "protokit/protoDefs.h"
#include "protokit/protoSocket.h"
#include "protokit/protoDebug.h"
#include "protokit/protoTimer.h"
#include "protokit/protoTree.h"
#include "protokit/protoRouteTable.h"
#include "protokit/protoRouteMgr.h"
#include "protokit/protoPipe.h"
#include "protokit/protoEvent.h"

#ifndef WIN32
#include "protokit/protoFile.h"
#endif

#ifdef SIMULATE
#ifdef NS2
#include "protokit/nsProtoSimAgent.h"
#endif // NS2
#else
#include "protokit/protoDispatcher.h"
#include "protokit/protoApp.h"
#endif // if/else SIMULATE

#endif // _PROTOKIT
