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

import platform

import waflib

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

    bind_opts = ctx.parser.add_option_group('Language Bindings', 'Use during configure step.')
    bind_opts.add_option('--build-python', action='store_true',
            help='Build Python bindings [default:false]')
    bind_opts.add_option('--build-java', action='store_true',
            help='Build Java bindings [default:false]')

    build_opts = ctx.parser.add_option_group('Compile Options', 'Use during build step.')
    build_opts.add_option('--debug', action='store_true',
            help='Build in debug mode [default:release]')
    build_opts.add_option('--enable-wx', action='store_true',
            help='Enable checking for wxWidgets.')

def configure(ctx):
    if system == 'windows':
        ctx.env.MSVC_TARGETS = ['x86']

    ctx.load('compiler_cxx')

    # Use this USE variable to add flags to protolib's compilation
    ctx.env.USE_BUILD_PROTOLIB += ['BUILD_PROTOLIB']

    if system in ('linux', 'darwin', 'freebsd'):
        ctx.env.DEFINES_BUILD_PROTOLIB += ['UNIX', 'HAVE_DIRFD', 'HAVE_IPV6',
                'HAVE_ASSERT', 'HAVE_GETLOGIN']

        ctx.check_cxx(lib='pthread')
        ctx.env.USE_BUILD_PROTOLIB += ['PTHREAD']

        if ctx.options.enable_wx:
            ctx.check_cfg(path='wx-config', args=['--cxxflags', '--libs'],
                    package='', uselib_store='WX', mandatory=False)

    if system == 'linux':
        ctx.env.DEFINES_BUILD_PROTOLIB += ['_FILE_OFFSET_BITS=64',
                'HAVE_LOCKF', 'HAVE_OLD_SIGNALHANDLER', 'HAVE_SCHED', 'LINUX',
                'HAVE_TIMERFD', 'NO_SCM_RIGHTS']

        ctx.check_cxx(lib='dl rt')
        ctx.env.USE_BUILD_PROTOLIB += ['DL', 'RT']

        ctx.env.HAVE_NETFILTER_QUEUE = ctx.check_cxx(lib='netfilter_queue',
                mandatory=False)

    if system == 'darwin':
        ctx.env.DEFINES_BUILD_PROTOLIB += ['MACOSX', 'HAVE_FLOCK',
                '_FILE_OFFSET_BITS=64', 'HAVE_DIRFD', 'HAVE_PSELECT']
        ctx.check_cxx(lib='resolv')
        ctx.env.USE_BUILD_PROTOLIB += ['RESOLV']

    if system == 'freebsd':
        ctx.env.DEFINES_BUILD_PROTOLIB += ['HAVE_FLOCK']

    if system == 'windows':
        ctx.env.DEFINES_BUILD_PROTOLIB += ['_CRT_SECURE_NO_WARNINGS',
                'HAVE_ASSERT', 'WIN32', 'HAVE_IPV6']
        #ctx.env.CXXFLAGS += ['/EHsc']
        ctx.check_libs_msvc(['ws2_32', 'iphlpapi', 'user32', 'gdi32', 'Advapi32'])
        ctx.env.USE_BUILD_PROTOLIB += ['WS2_32', 'IPHLPAPI', 'USER32', 'GDI32', 'ADVAPI32']

    if ctx.options.build_python:
        ctx.load('python')
        ctx.check_python_version((2,4))
        ctx.check_python_headers()
        if ctx.env.PYTHON_VERSION.split('.')[0] != '2':
            waflib.Logs.warn('Python bindings currently only support Python 2')
            ctx.env.BUILD_PYTHON = False
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
        ctx.env.DEFINES_BUILD_PROTOLIB += ['PROTO_DEBUG', 'DEBUG', '_DEBUG']
    else:
        ctx.env.DEFINES_BUILD_PROTOLIB += ['NDEBUG']

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
    protolib = ctx.stlib(
        target = 'protolib',
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
            'protoGraph',
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
            'protoTime',
            'protoTimer',
            'protoTree',
            'protoVif',
        ]]
    )

    if system in ('linux', 'darwin', 'freebsd'):
        protolib.source.extend(['src/unix/{0}.cpp'.format(x) for x in [
            'unixNet',
            'unixSerial',
            'unixVif',
        ]])
        protolib.source.extend(['src/manet/{0}.cpp'.format(x) for x in [
            'manetGraph',
            'manetMsg',
        ]])
        protolib.source.append('src/common/protoFile.cpp')

    if system == 'linux':
        protolib.source.extend(['src/linux/{0}.cpp'.format(x) for x in [
            'linuxCap',
            'linuxNet',
            'linuxRouteMgr',
        ]])
        if ctx.env.HAVE_NETFILTER_QUEUE:
            protolib.source.append('src/linux/linuxDetour.cpp')
            protolib.use.append('NETFILTER_QUEUE')

    if system in ('darwin', 'freebsd'):
        protolib.source.extend(['src/bsd/{0}.cpp'.format(x) for x in [
            'bsdDetour',
            'bsdNet',
            'bsdRouteMgr',
        ]])
        protolib.source.append('src/unix/bpfCap.cpp')

    if system == 'windows':
        protolib.source.extend(['src/win32/{0}.cpp'.format(x) for x in [
            'win32Net',
            'win32RouteMgr',
        ]])

    # Only add wxWidgets support if we have the libraries installed
    if ctx.env.HAVE_WX:
        protolib.source.append('src/wx/wxProtoApp.cpp')
        protolib.use.append('WX')

    # Language bindings
    if ctx.env.BUILD_PYTHON:
        ctx.shlib(
            features = 'pyext',
            target = 'protokit',
            name = 'pyprotokit',
            use = ['protolib'],
            source = ['src/python/protokit.cpp'],
        )

    if ctx.env.BUILD_JAVA:
        ctx.shlib(
            target = 'ProtolibJni',
            use = ['protolib', 'JAVA'],
            source = ['src/java/protoPipeJni.cpp'],
        )
        ctx(
            features = ['javac', 'jar'],
            srcdir = 'src/java/src',
            outdir = 'src/java/src',
            basedir = 'src/java/src',
            destfile = 'protolib-jni.jar',
        )

    # Example programs to build (not built by default, see below).
    for example in (
            'base64Example',
            'detourExample',
            'graphExample',
            'graphRider',
            'lfsrExample',
            'msg2MsgExample',
            'msgExample',
            'netExample',
            'pcapExample',
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
            'wxProtoExample',
            ):
        _make_simple_example(ctx, example)

    # Enable example targets specified on the command line
    ctx._parse_targets()

def _make_simple_example(ctx, name):
    '''Makes a task from a single source file in the examples directory.

    These tasks are not built by default.  Use the --targets flag.
    '''
    ctx.program(
        target = name,
        use = ['protolib'],
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
