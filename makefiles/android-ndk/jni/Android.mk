LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := protolib
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../../../include \
	$(LOCAL_PATH)/../../../include/unix
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_CFLAGS := \
	-DUNIX -frtti \
	-DHAVE_IPV6 \
	-DHAVE_DIRFD \
	-DPROTO_DEBUG \
	-DHAVE_ASSERT \
	-DHAVE_GETLOGIN \
	-DUSE_SELECT \
	-DLINUX -DANDROID\
	-D_FILE_OFFSET_BITS=64 \
	-DHAVE_OLD_SIGNALHANDLER \
	-DHAVE_SCHED \
	-DNO_SCM_RIGHTS

ifeq ($(APP_OPTIM),debug)
	LOCAL_CFLAGS += -DPROTO_DEBUG
endif
LOCAL_EXPORT_CFLAGS := $(LOCAL_CFLAGS)

LOCAL_EXPORT_LDLIBS := -llog

LOCAL_SRC_FILES := \
	../../../src/common/protoAddress.cpp \
	../../../src/common/protoApp.cpp \
	../../../src/common/protoBitmask.cpp \
	../../../src/common/protoCap.cpp \
	../../../src/common/protoChannel.cpp \
	../../../src/common/protoDebug.cpp \
	../../../src/common/protoDispatcher.cpp \
	../../../src/common/protoEvent.cpp \
	../../../src/common/protoGraph.cpp \
	../../../src/common/protoJson.cpp \
	../../../src/common/protoList.cpp \
	../../../src/common/protoNet.cpp \
	../../../src/common/protoPipe.cpp \
	../../../src/common/protoPkt.cpp \
	../../../src/common/protoPktETH.cpp \
	../../../src/common/protoPktIP.cpp \
	../../../src/common/protoPktRIP.cpp \
	../../../src/common/protoQueue.cpp \
	../../../src/common/protoRouteMgr.cpp \
	../../../src/common/protoRouteTable.cpp \
	../../../src/common/protoSocket.cpp \
    ../../../src/common/protoString.cpp \
	../../../src/common/protoTime.cpp \
	../../../src/common/protoTimer.cpp \
	../../../src/common/protoTree.cpp \
	../../../src/unix/unixNet.cpp \
	../../../src/linux/linuxNet.cpp \
	../../../src/linux/linuxCap.cpp \
	../../../src/linux/linuxRouteMgr.cpp \
	../../../src/unix/zebraRouteMgr.cpp \
# ../../../src/linux/androidDetour.cpp
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := ProtolibJni
LOCAL_STATIC_LIBRARIES := protolib
LOCAL_SRC_FILES := ../../../src/java/protoPipeJni.cpp
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := riposer
LOCAL_STATIC_LIBRARIES := protolib
LOCAL_SRC_FILES := \
	../../../examples/riposer.cpp
include $(BUILD_EXECUTABLE)
