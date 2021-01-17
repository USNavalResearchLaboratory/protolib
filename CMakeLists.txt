cmake_minimum_required(VERSION 3.1)
cmake_policy(SET CMP0077 NEW)

# set the project name
project(protokit)

set(COMMON src/common)

option(PROTOKIT_BUILD_EXAMPLES "Enables building of the examples in /examples." OFF)
option(PROTOKIT_BUILD_PYTHON "Enables building of python bindings **NOT IMPLEMENTED**." OFF)
option(PROTOKIT_BUILD_JAVA "Enables building of jave bindings **NOT IMPLEMENTED**." OFF)
option(PROTOKIT_ENABLE_DEBUG "Include debugging messages/logging **NOT IMPLEMENTED**." OFF)
option(PROTOKIT_ENABLE_WX "Enables building with WX Widgets" OFF)
option(PROTOKIT_ENABLE_INSTALL "Enables install target" OFF)

# Availability checks
include(CheckCXXSymbolExists)
include(CheckStructHasMember)
include(CheckTypeSize)
include(CheckIncludeFiles)
include(CheckLibraryExists)

check_include_files( pcap.h HAVE_PCAP_H)
if(HAVE_PCAP_H)
	#list(APPEND PLATFORM_SOURCE_FILES ${COMMON}/pcapCap.cpp)
endif()

check_include_files( sys/timerfd.h HAVE_TIMERFD_H)
if(HAVE_TIMERFD_H)
	list(APPEND PLATFORM_DEFINITIONS USE_TIMERFD)
endif()

check_include_files( sys/select.h HAVE_SELECT_H)
check_include_files( sys/epoll.h HAVE_EPOLL_H)
if(HAVE_EPOLL_H)
	list(APPEND PLATFORM_DEFINITIONS USE_EPOLL)
	message(STATUS "Using epoll")
elseif(HAVE_SELECT_H)
	list(APPEND PLATFORM_DEFINITIONS USE_SELECT)
	message(STATUS "Using select")
else()
	message(STATUS "Using neither epoll or select")
endif()

check_cxx_symbol_exists( sched_setscheduler "sched.h" HAVE_SCHED_SETSCHEDULER )
if(HAVE_SCHED_SETSCHEDULER)
	list(APPEND PLATFORM_DEFINITIONS HAVE_SCHED)
endif()

check_cxx_symbol_exists(assert "assert.h" HAVE_ASSERT)
if(HAVE_ASSERT)
	list(APPEND PLATFORM_DEFINITIONS HAVE_ASSERT)
endif()

check_cxx_symbol_exists(lockf "unistd.h" HAVE_LOCKF)
if(HAVE_LOCKF)
	list(APPEND PLATFORM_DEFINITIONS HAVE_LOCKF)
endif()

check_cxx_symbol_exists(flock "sys/file.h" HAVE_FLOCK)
if(HAVE_FLOCK)
	list(APPEND PLATFORM_DEFINITIONS HAVE_FLOCK)
endif()

check_cxx_symbol_exists(eventfd "sys/eventfd.h" HAVE_EVENTFD) #USE
if(HAVE_EVENTFD)
	list(APPEND PLATFORM_DEFINITIONS USE_EVENTFD)
endif()

check_cxx_symbol_exists(pselect "sys/select.h" HAVE_PSELECT)
if(HAVE_PSELECT)
	list(APPEND PLATFORM_DEFINITIONS HAVE_PSELECT)
endif()

check_struct_has_member(sockaddr_un sun_len "sys/un.h" HAVE_SUN_LEN)
if(NOT HAVE_SUN_LEN)
	list(APPEND PLATFORM_DEFINITIONS NO_SCM_RIGHTS)
endif()

check_cxx_symbol_exists(in6addr_any "netinet/in.h" HAVE_IPV6)
if(HAVE_IPV6)
    list(APPEND PLATFORM_DEFINITIONS HAVE_IPV6)
endif()

# Check for libraries
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(LibXml2)
if(LibXml2_FOUND)
	include_directories(${LIBXML2_INCLUDE_DIR})
	list(APPEND PLATFORM_LIBS ${LIBXML2_LIBRARIES})
	list(APPEND PUBLIC_HEADER_FILES include/protoXml.h)
	list(APPEND COMMON_SOURCE_FILES ${COMMON}/protoXml.cpp)
endif()

if(PROTOKIT_ENABLE_WX)
        find_package(wxWidgets REQUIRED COMPONENTS core base)
	if(wxWidgets_FOUND)
		include(${wxWidgets_USE_FILE})
		list(APPEND PLATFORM_LIBS ${wxWidgets_LIBRARIES})
		list(APPEND PLATFORM_SOURCE_FILES src/wx/wxProtoApp.cpp)
	endif()
endif()

# List header files
list(APPEND PUBLIC_HEADER_FILES
include/manetGraph.h        
include/manetGraphML.h      
include/manetMsg.h          
include/ns3ProtoSimAgent.h  
include/protoAddress.h      
include/protoApp.h          
include/protoAverage.h      
include/protoBase64.h       
include/protoBitmask.h      
include/protoCap.h          
include/protoChannel.h     
include/protoCheck.h     
include/protoDebug.h       
include/protoDefs.h        
include/protoDetour.h      
include/protoDispatcher.h  
include/protoEvent.h       
include/protoFile.h        
include/protoGraph.h       
include/protoJson.h        
include/protoLFSR.h        
include/protoList.h     
include/protoNet.h      
include/protoNotify.h   
include/protoPipe.h     
include/protoPkt.h      
include/protoPktARP.h   
include/protoPktETH.h   
include/protoPktIGMP.h  
include/protoPktIP.h    
include/protoPktRIP.h   
include/protoPktRTP.h   
include/protoPktTCP.h      
include/protoQueue.h       
include/protoRouteMgr.h    
include/protoRouteTable.h  
include/protoSerial.h      
include/protoSimAgent.h    
include/protoSocket.h      
include/protoSpace.h       
include/protoString.h
include/protoTime.h
include/protoTimer.h
include/protoTree.h
include/protoVersion
include/protoVif.h
include/protokit.h
include/testFuncs.h
include/wxProtoApp.h
include/unix/zebraRouteMgr.h
)

# List platform-independent source files
list(APPEND COMMON_SOURCE_FILES 
          ${COMMON}/protoAddress.cpp  
		  ${COMMON}/protoApp.cpp 
		  ${COMMON}/protoBase64.cpp 
          ${COMMON}/protoBitmask.cpp 
		  ${COMMON}/protoCap.cpp 
		  ${COMMON}/protoChannel.cpp 
		  ${COMMON}/protoDebug.cpp 
		  ${COMMON}/protoDispatcher.cpp 
		  ${COMMON}/protoEvent.cpp 
          ${COMMON}/protoFile.cpp 
		  ${COMMON}/protoGraph.cpp 
		  ${COMMON}/protoJson.cpp 
		  ${COMMON}/protoLFSR.cpp 
		  ${COMMON}/protoList.cpp 
          ${COMMON}/protoNet.cpp 
          ${COMMON}/protoPipe.cpp 
          ${COMMON}/protoPkt.cpp 
		  ${COMMON}/protoPktARP.cpp 
		  ${COMMON}/protoPktETH.cpp 
		  ${COMMON}/protoPktIGMP.cpp 
		  ${COMMON}/protoPktIP.cpp 
		  ${COMMON}/protoPktRTP.cpp 
		  ${COMMON}/protoQueue.cpp 
          ${COMMON}/protoRouteMgr.cpp 
		  ${COMMON}/protoRouteTable.cpp 
          ${COMMON}/protoSerial.cpp 
		  ${COMMON}/protoSocket.cpp 
          ${COMMON}/protoSpace.cpp 
		  ${COMMON}/protoString.cpp
          ${COMMON}/protoTime.cpp 
		  ${COMMON}/protoTimer.cpp 
          ${COMMON}/protoTree.cpp 
          ${COMMON}/protoVif.cpp )

# Setup platform independent include directory
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include )

# Setup platform dependent libraries, defines, source file and compiler flags
if(MSVC)
	list(APPEND PLATFORM_LIBS ws2_32 iphlpapi user32 gdi32 Advapi32 ntdll)
	list(APPEND PLATFORM_DEFINITIONS _CRT_SECURE_NO_WARNINGS)
	list(APPEND PLATFORM_SOURCE_FILES src/win32/win32Net.cpp src/win32/win32RouteMgr.cpp)
	list(APPEND PLATFORM_FLAGS /EHsc)
elseif(UNIX)
	list(APPEND PLATFORM_DEFINITIONS UNIX _FILE_OFFSET_BITS=64)
	list(APPEND PLATFORM_LIBS pthread)
	list(APPEND PLATFORM_SOURCE_FILES 	src/unix/unixNet.cpp 
										src/unix/unixSerial.cpp 
										src/unix/unixVif.cpp
										src/manet/manetGraph.cpp
										src/manet/manetMsg.cpp)
	if(${CMAKE_SYSTEM_NAME} STREQUAL Linux)
		list(APPEND PLATFORM_DEFINITIONS LINUX )
		list(APPEND PLATFORM_LIBS dl rt)
		list(APPEND PLATFORM_SOURCE_FILES 	src/linux/linuxCap.cpp 
										    src/linux/linuxNet.cpp 
										    src/linux/linuxRouteMgr.cpp
										    src/unix/zebraRouteMgr.cpp)
		
		check_library_exists(netfilter_queue pktb_alloc "" HAVE_NETFILTER_QUEUE)
		if(HAVE_NETFILTER_QUEUE)
			list(APPEND PLATFORM_LIBS NETFILTER_QUEUE)
			list(APPEND PLATFORM_SOURCE_FILES src/linux/linuxDetour.cpp)
			list(APPEND examples detourExample)
		endif()
	elseif(${CMAKE_SYSTEM_NAME} STREQUAL Darwin )
		list(APPEND PLATFORM_DEFINITIONS MACOSX )
		list(APPEND PLATFORM_SOURCE_FILES 	src/bsd/bsdDetour.cpp
											src/bsd/bsdRouteMgr.cpp
											src/bsd/bsdNet.cpp
											src/unix/bpfCap.cpp)
            SET(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
            SET(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
            SET(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
            SET(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
	endif()
endif()

include(GNUInstallDirs)

# Setup target

add_library(protokit STATIC ${PLATFORM_SOURCE_FILES} ${COMMON_SOURCE_FILES} ${PUBLIC_HEADER_FILES})
add_library(protokit::protokit ALIAS protokit)
target_link_libraries(protokit PRIVATE ${PLATFORM_LIBS})
target_compile_definitions(protokit PUBLIC ${PLATFORM_DEFINITIONS})
target_compile_options(protokit PUBLIC ${PLATFORM_FLAGS})
target_include_directories(protokit PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(protokit PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

# Install target
if(${PROTOKIT_ENABLE_INSTALL})
	install( TARGETS protokit EXPORT protokitTargets
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} )

	install( EXPORT protokitTargets
		FILE protokitConfig.cmake
		NAMESPACE protokit::
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/protokit
	)

	install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

	export(EXPORT protokitTargets
		FILE ${CMAKE_CURRENT_BINARY_DIR}/protokitTargets.cmake
		NAMESPACE protokit::)
		
	export(PACKAGE protokit)
endif()

if(PROTOKIT_BUILD_EXAMPLES)
	# Setup examples
	list(APPEND examples 
	base64Example
	# detourExample This depends on netfilterqueue so doesn't work as a "simple example"
	eventExample
	fileTest
	graphExample
	#'graphRider', (this depends on manetGraphML.cpp so doesn't work as a "simple example"
	lfsrExample
	msg2MsgExample
	#'msgExample',  (this depends on examples/testFuncs.cpp so doesn't work as a "simple example"
	netExample
	pipe2SockExample
	pipeExample
	protoCapExample
	protoExample
	protoFileExample
	queueExample
	serialExample
	simpleTcpExample
	sock2PipeExample
	threadExample
	timerTest
	vifExample
	vifLan
	#'wxProtoExample', (this depends on wxWidgets (could use wx-config to test) so doesn't work as a "simple example"
	udptest)

	foreach(example ${examples})
		add_executable(${example} examples/${example}.cpp)
		target_link_libraries(${example} PRIVATE protokit)
		include_directories(${CMAKE_SOURCE_DIR}/include)
	endforeach()
endif()


