
cmake_minimum_required(VERSION 3.4.1)

project("protolib")

#add_definitions("-DANDROID=1" "-D__ANDROID_API__=24" "-DANDROID_API_VERSION=24" "-DPUSH_NOTIFICATIONS=1" "-DANDROID" "-DLINUX" "-DUNIX" "-DHAVE_IPV6" "-DHAVE_DIRFD" "-DPROTO_DEBUG" "-DHAVE_ASSERT" "-DHAVE_GETLOGIN" "-DUSE_SELECT" "-D_FILE_OFFSET_BITS=64" "-DHAVE_OLD_SIGNALHANDLER" "-DHAVE_SCHED" "-DNO_SCM_RIGHTS" "-DAPP_VERSION=1.0.0" "-DAPP_VERSION_HEX=0x10000")

add_definitions("-DANDROID=1" "-DANDROID_API_VERSION=24" "-DPUSH_NOTIFICATIONS=1" "-DANDROID" "-DLINUX" "-DUNIX" "-DHAVE_IPV6" "-DHAVE_DIRFD" "-DPROTO_DEBUG" "-DHAVE_ASSERT" "-DHAVE_GETLOGIN" "-DUSE_SELECT" "-D_FILE_OFFSET_BITS=64" "-DHAVE_OLD_SIGNALHANDLER" "-DHAVE_SCHED" "-DNO_SCM_RIGHTS" "-DAPP_VERSION=1.0.0" "-DAPP_VERSION_HEX=0x10000")


include_directories( AFTER
    "../../OID_NDK}/sources/android/cpufeatures"
    "../../include"
)

enable_language(ASM)

IF(BUILD_CONFIGURATION MATCHES "DEBUG")
    SET(BINARY_NAME "protokit")
    add_definitions("-DDEBUG=1" "-D_DEBUG=1")
ELSEIF(BUILD_CONFIGURATION MATCHES "RELEASE")
    SET(BINARY_NAME "protokit")
    add_definitions("-DNDEBUG=1")
ELSE(BUILD_CONFIGURATION MATCHES "DEBUG")
    MESSAGE( FATAL_ERROR "No matching build-configuration found." )
ENDIF(BUILD_CONFIGURATION MATCHES "DEBUG")

add_library( ${BINARY_NAME}

    STATIC

    "../../src/manet/manetGraph.cpp"
    "../../src/manet/manetMsg.cpp"
    "../../src/common/protoAddress.cpp"
    "../../src/common/protoApp.cpp"
    "../../src/common/protoBase64.cpp"
    "../../src/common/protoBitmask.cpp"
    "../../src/common/protoCap.cpp"
    "../../src/common/protoChannel.cpp"
    "../../src/common/protoCheck.cpp"
    "../../src/common/protoDebug.cpp"
    "../../src/common/protoDispatcher.cpp"
    "../../src/common/protoEvent.cpp"
    "../../src/common/protoFile.cpp"
    "../../src/common/protoFlow.cpp"
    "../../src/common/protoGraph.cpp"
    "../../src/common/protoJson.cpp"
    "../../src/common/protoLFSR.cpp"
    "../../src/common/protoList.cpp"
    "../../src/common/protoNet.cpp"
    "../../src/common/protoPipe.cpp"
    "../../src/common/protoPkt.cpp"
    "../../src/common/protoPktARP.cpp"
    "../../src/common/protoPktETH.cpp"
    "../../src/common/protoPktIGMP.cpp"
    "../../src/common/protoPktIP.cpp"
    "../../src/common/protoPktRIP.cpp"
    "../../src/common/protoPktRTP.cpp"
    "../../src/common/protoPktTCP.cpp"
    "../../src/common/protoQueue.cpp"
    "../../src/common/protoRouteMgr.cpp"
    "../../src/common/protoRouteTable.cpp"
    "../../src/common/protoSocket.cpp"
    "../../src/common/protoSpace.cpp"
    "../../src/common/protoString.cpp"
    "../../src/common/protoTime.cpp"
    "../../src/common/protoTimer.cpp"
    "../../src/common/protoTree.cpp"
    "../../src/common/protoVif.cpp"
    "../../src/unix/unixNet.cpp"
    "../../src/linux/linuxNet.cpp"
    "../../src/unix/unixVif.cpp"
)

find_library(log "log")
find_library(android "android")
find_library(glesv2 "GLESv2")
find_library(egl "EGL")

target_link_libraries( ${BINARY_NAME}

    ${log}
    ${android}
    ${glesv2}
    ${egl}
)
