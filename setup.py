# This Python script can be used to build and install a Python "protokit" module that
# provides a wrapper to select portions of the "Protlib" code base.
# This currently includes just the "protopipe" interprocess communication class.
# Example scripts are provided in the "examples" subdirectory.

import platform
from distutils.core import setup, Extension

# This setup.py script assumes that Protolib (libprotokit.a) has already
# been built and located in "protolib/lib with respect to this script
# You can 'cd makefiles' or 'cd protolib/makefiles' to build Protolib for
# your system before attempting to install this Python module.

PYTHON = "src/python/"
    
srcFiles = [PYTHON + 'protokit.cpp']

# Determine system-specific macro definitions, etc
# (For now we support only "linux", "darwin" (MacOS), "freebsd", and "win32")

system = platform.system().lower()

sys_macros = [('HAVE_ASSERT',None), ('HAVE_IPV6',None), ('PROTO_DEBUG', None)]
sys_libs = ['protokit']
extra_link_args = []

if system in ('linux', 'darwin', 'freebsd'):
    sys_macros.append(('UNIX',None))
    if(system in 'darwin'):
      extra_link_args.append("-mmacosx-version-min=10.9")
      extra_link_args.append("-stdlib=libc++")
elif system in ('windows'):
    sys_macros.append(('WIN32',None))
else:
     raise Exception("setup.py: unsupported operating system \"%s\"" % system)
 
if system == 'darwin':
    sys_libs.append('resolv') 
        
setup(name='protokit', 
      version = '1.0',
      ext_modules = [Extension('protokit', 
                               srcFiles, 
                               include_dirs = ['./include'],
                               define_macros = sys_macros,
                               library_dirs = ['./lib', './build'], 
                               libraries = sys_libs,
                               extra_link_args = extra_link_args)])
