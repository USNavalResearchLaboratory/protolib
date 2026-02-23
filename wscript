#!/usr/bin/env python
'''
wscript - Waf build script for Protolib
See http://waf.googlecode.com/ for more information.

In order to use different build directories (for example, a release and a debug
build), use the -o (--out) flag when configuring.  For example:

    ./waf configure -o build-debug --debug
    ./waf

    ./waf configure -o build-release
    ./waf

'''

import subprocess
import os
import platform
import waflib

# Fetch VERSION from include/protoVersion.h file
# TBD - do thi differently (using ctx.path.bldpath() in config?)
try:
    vfile = open('include/protoVersion.h', 'r')
except:
    vfile = open('protolib/include/protoVersion.h', 'r')
for line in vfile.readlines():
    line = line.split()
    if len(line) != 3:
        continue
    if "#define" == line[0] and "PROTOLIB_VERSION" == line[1]:
        VERSION = line[2].strip('"')
if VERSION is None:
    print ("Warning: Protolib version not found!?")
    

# So you don't need to do ./waf configure if you are just using the defaults
waflib.Configure.autoconfig = True

# Top-level project directory
top = '.'
# Directory where build files are placed
out = 'build'

# System waf is running on: linux, darwin (Mac OSX), freebsd, windows, etc.
system = platform.system().lower()

def options(ctx):
    ctx.load('compiler_cxx')
    ctx.load('python')
    ctx.load('java')

    bind_opts = ctx.parser.add_argument_group('Language Bindings', 'Use during configure step.')
    bind_opts.add_argument('--build-python', action='store_true',
            help='Build Python bindings [default:false]')
    bind_opts.add_argument('--build-java', action='store_true',
            help='Build Java bindings [default:false]')

    build_opts = ctx.parser.add_argument_group('Compile Options', 'Use during build step.')
    build_opts.add_argument('--debug', action='store_true',
            help='Build in debug mode [default:release]')
    build_opts.add_argument('--enable-wx', action='store_true',
            help='Enable checking for wxWidgets.')
    build_opts.add_argument('--enable-static-library', action='store_true',
            help='Enable installing static library. [default:false]')

def configure(ctx):
    if system == 'windows':
        ctx.env.MSVC_TARGETS = ['x86']
    elif system == 'darwin':
        ctx.env.ARCH = ['arm64', 'x86_64']

    ctx.load('compiler_cxx')

    # Use this USE variable to add flags to protolib's compilation
    ctx.env.USE_BUILD_PROTOLIB += ['BUILD_PROTOLIB']

    if system in ('linux', 'darwin', 'freebsd', 'gnu', 'gnu/kfreebsd'):
        ctx.env.DEFINES_BUILD_PROTOLIB += ['UNIX', 'HAVE_DIRFD', 'HAVE_IPV6',
                'HAVE_ASSERT', 'HAVE_GETLOGIN']

        ctx.check_cxx(lib='pthread')
        ctx.env.USE_BUILD_PROTOLIB += ['PTHREAD']

        if ctx.options.enable_wx:
            ctx.check_cfg(path='wx-config', args=['--cxxflags', '--libs'],
                    package='', uselib_store='WX', mandatory=False)

    if system == 'linux':
        ctx.env.DEFINES_BUILD_PROTOLIB += ['LINUX', 
                'HAVE_LOCKF', '_FILE_OFFSET_BITS=64', 'HAVE_OLD_SIGNALHANDLER', 
                'NO_SCM_RIGHTS', 'HAVE_SCHED',  
                'USE_TIMERFD', 'USE_EVENTFD', 'HAVE_PSELECT', 'USE_SELECT']
        ctx.check_cxx(lib='dl rt')
        ctx.env.USE_BUILD_PROTOLIB += ['DL', 'RT']

        ctx.env.HAVE_NETFILTER_QUEUE = ctx.check_cxx(lib='netfilter_queue',
                mandatory=False)

    if system == 'darwin':
        ctx.env.DEFINES_BUILD_PROTOLIB += ['MACOSX',
                'HAVE_FLOCK', '_FILE_OFFSET_BITS=64', 'HAVE_PSELECT', 'USE_SELECT'] ;#, "UNICODE"]
        ctx.check_cxx(lib='resolv')
        ctx.env.USE_BUILD_PROTOLIB += ['RESOLV']

    if system in ('freebsd'):
        ctx.env.DEFINES_BUILD_PROTOLIB += ['HAVE_FLOCK', '_FILE_OFFSET_BITS=64', 
                                           'HAVE_PSELECT', 'USE_SELECT']

    if system == 'windows':
        ctx.env.DEFINES_BUILD_PROTOLIB += ['_CRT_SECURE_NO_WARNINGS',
                'HAVE_ASSERT', 'WIN32', 'HAVE_IPV6']
        ctx.env.CXXFLAGS += ['/EHsc']
        ctx.check_libs_msvc(['ws2_32', 'iphlpapi', 'user32', 'gdi32', 'Advapi32', 'ntdll'])
        ctx.env.USE_BUILD_PROTOLIB += ['WS2_32', 'IPHLPAPI', 'USER32', 'GDI32', 'ADVAPI32', 'ntdll.lib']
        
    if system == "gnu":
        ctx.check_cxx(lib='pcap')
        ctx.env.LDFLAGS += ['-lpcap']
        
    # libxml2 is needed for a subset of Protolib classes (i.e., not always be required)
    # This looks for the libxml2 include path using the "xml2-config" command that should be 
    # available if libxml2-dev is installed.
    try:
        libxml2Include = subprocess.check_output(['xml2-config', '--cflags']).strip().split('I', 1)[1]
        ctx.env.append_value('INCLUDES', [libxml2Include])
        print ("Added '%s' to INCLUDES" % libxml2Include)
    except:
        print ("WARNING: libxml2 not found! Some Protolib code may not build. Install 'libxml2-dev' package.")

    if ctx.options.build_python:
        ctx.load('python')
        if 'darwin' == system:
            print ("(Note MacOSX requires 'gettext' installation)")
            ctx.env.LINKFLAGS += ['-L/opt/local/lib']  ;# MacPorts library install location
        ctx.check_python_version((2,4))
        ctx.check_python_headers()
        if ctx.env.PYTHON_VERSION.split('.')[0] != '2':
            waflib.Logs.warn('Python bindings currently only support Python 2')
            #ctx.env.BUILD_PYTHON = False
        else:
            ctx.env.BUILD_PYTHON = True

    if ctx.options.build_java:
        ctx.load('java')
        ctx.check_jni_headers()
        for i in ctx.env.DEFINES:
            if i == 'HAVE_JNI_H=1':
                ctx.env.BUILD_JAVA = True

    # Compiler-specific flags
    if ctx.options.debug:
        #ctx.env.DEFINES_BUILD_PROTOLIB += ['PROTO_DEBUG', 'DEBUG', '_DEBUG']
        ctx.env.DEFINES_BUILD_PROTOLIB += ['PROTO_DEBUG', 'DEBUG']
    else:
        ctx.env.DEFINES_BUILD_PROTOLIB += ['NDEBUG', "PROTO_DEBUG"]

    if ctx.env.COMPILER_CXX == 'g++' or ctx.env.COMPILER_CXX == 'clang++':
        ctx.env.CFLAGS += ['-fPIC']
        ctx.env.CXXFLAGS += ['-fPIC']
        if ctx.options.debug:
            ctx.env.CFLAGS += ['-O0', '-g']
            ctx.env.CXXFLAGS += ['-O0', '-g']
        else:
            ctx.env.CFLAGS += ['-O3']
            ctx.env.CXXFLAGS += ['-O3']

    if ctx.env.COMPILER_CXX == 'msvc':
        if ctx.options.debug:
            ctx.env.CFLAGS += ['/Od', '/RTC1', '/ZI']
        else:
            ctx.env.CXXFLAGS += ['/Ox', '/DNDEBUG']
            #ctx.env.CXXFLAGS += ['/Ox', '/DNDEBUG', '/DWINVER=0x0501']
        ctx.env.CFLAGS

def build(ctx):
    obj = ctx.objects(
        target = 'protoObjs',
        includes = ['include', 'include/unix'],
        export_includes = ['include', 'include/unix'],
        use = ctx.env.USE_BUILD_PROTOLIB,
        source = ['src/common/{0}.cpp'.format(x) for x in [
            'protoAddress',
            'protoApp',
            'protoBase64',
            'protoBitmask',
            'protoCap',
            'protoChannel',
            'protoDebug',
            'protoDispatcher',
            'protoEvent',
            'protoFile',
            'protoFlow',
            'protoGraph',
            'protoJson',
            'protoLFSR',
            'protoList',
            'protoNet',
            'protoPipe',
            'protoPkt',
            'protoPktARP',
            'protoPktETH',
            'protoPktIP',
            'protoPktRTP',
            'protoQueue',
            'protoRouteMgr',
            'protoRouteTable',
            'protoSerial',
            'protoSocket',
            'protoSpace',
            'protoString',
            'protoTime',
            'protoTimer',
            'protoTree',
            'protoVif',
        ]],
    )
    if system in ('linux', 'darwin', 'freebsd', 'gnu', 'gnu/kfreebsd'):
        obj.source.extend(['src/unix/{0}.cpp'.format(x) for x in [
            'unixNet',
            'unixSerial',
            'unixVif',
        ]])
        obj.source.extend(['src/manet/{0}.cpp'.format(x) for x in [
            'manetGraph',
            'manetMsg',
        ]])
    if system == 'linux':
        obj.source.extend(['src/linux/{0}.cpp'.format(x) for x in [
            'linuxCap',
            'linuxNet',
            'linuxRouteMgr',
        ]])
        obj.source.append('src/unix/zebraRouteMgr.cpp')
        if ctx.env.HAVE_NETFILTER_QUEUE:
            obj.source.append('src/linux/linuxDetour.cpp')
            obj.use.append('NETFILTER_QUEUE')

    if system in ('darwin', 'freebsd', 'gnu/kfreebsd'):
        obj.source.extend(['src/bsd/{0}.cpp'.format(x) for x in [
            'bsdDetour',
            'bsdRouteMgr',
        ]])
        if system != 'gnu/kfreebsd':
            obj.source.append('src/bsd/bsdNet.cpp')
        obj.source.append('src/unix/bpfCap.cpp')

    if system == 'windows':
        obj.source.extend(['src/win32/{0}.cpp'.format(x) for x in [
            'win32Net',
            'win32RouteMgr',
        ]])
        
    if system == 'gnu':
        obj.source.append('src/common/pcapCap.cpp')
    
    # Static library build
    protolib_st = ctx.stlib(
        target = 'protokit',
        name = 'protolib_st',
        vnum = VERSION,
        use = ['protoObjs'],
        source = [],
        features = 'cxx cxxstlib',
        install_path = '${LIBDIR}',
    )
    
    # Only add wxWidgets support if we have the libraries installed
    if ctx.env.HAVE_WX:
        protolib_st.source.append('src/wx/wxProtoApp.cpp')
        protolib_st.use.append('WX')

    # Language bindings
    if ctx.env.BUILD_PYTHON:
        # Hack to force clang to link to static library
        if ctx.env.COMPILER_CXX == 'clang++':
            use = ['protoObjs']
        else:
            use = ['protolib_st']
        ctx.shlib(
            features = 'pyext',
            target = 'protokit',
            name = 'pyprotokit',
            use = use,
            source = ['src/python/protokit.cpp'],
        )

    if ctx.env.BUILD_JAVA:
        # Hack to force clang to link to static library
        if ctx.env.COMPILER_CXX == 'clang++':
            use = ['protoObjs', 'JAVA']
        else:
            use = ['protolib_st', 'JAVA']
        ctx.shlib(
            target = 'ProtolibJni',
            includes = ['include'],
            use = use,
            source = ['src/java/protoPipeJni.cpp'],
        )
        ctx(
            features = ['javac', 'jar'],
            srcdir = 'src/java/src',
            outdir = 'src/java/src',
            basedir = 'src/java/src',
            destfile = 'protolib-jni.jar',
        )
    
    # Shared library build
    protolib_sh = ctx.shlib(
        target = 'protokit',
        name = 'protolib_sh',
        vnum = VERSION,
        use = ['protoObjs'],
        source = [],
        features = 'cxx cxxshlib',
        install_path = '${LIBDIR}',
    )
    
    # Example programs to build (not built by default, see below).
    for example in (
            'base64Example',
            'detourExample',
            'eventExample',
            'fileTest',
            'graphExample',
            #'graphRider', (this depends on manetGraphML.cpp so doesn't work as a "simple example"
            'lfsrExample',
            'msg2MsgExample',
            #'msgExample',  (this depends on examples/testFuncs.cpp so doesn't work as a "simple example"
            'netExample',
            'pipe2SockExample',
            'pipeExample',
            'protoCapExample',
            'protoExample',
            'protoFileExample',
            'queueExample',
            'serialExample',
            'simpleTcpExample',
            'sock2PipeExample',
            'threadExample',
            'timerTest',
            'vifExample',
            'vifLan',
            #'wxProtoExample', (this depends on wxWidgets (could use wx-config to test) so doesn't work as a "simple example"
            'udptest'
            ):
        _make_simple_example(ctx, example)

    # Enable example targets specified on the command line
    ctx._parse_targets()

def _make_simple_example(ctx, name):
    '''Makes a task from a single source file in the examples directory.
       These tasks are not built by default.  
       Use the waf build --targets flag.
    '''
    # Hack to force clang to link to static library
    if ctx.env.COMPILER_CXX == 'clang++':
        use = ['protoObjs']
    else:
        use = ['protolib_st']

    ctx.program(
        target = name,
        use = use,
        includes = ['include', 'include/unix'],
        source = ['examples/{0}.cpp'.format(name)],
        # Don't build examples by default
        posted = True,
        # Don't install examples
        install_path = False,
    )

# Attach this method to the build context
@waflib.Configure.conf
def _parse_targets(ctx):
    '''Explicitly enable any targets (examples) set on the command line.

    See the "waf list" command and "waf --targets=target1,target2"
    Use --targets=* to build all examples
    '''
    for task in ctx.get_group(None):
        if ctx.targets == '*' or task.target in ctx.targets.split(','):
            task.posted = False
