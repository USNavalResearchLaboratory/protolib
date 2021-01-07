/**
 * Python ProtoKit Wrapper
 * By: Tom Wambold  and Brian Adamson
 */

 // IMPORTANT NOTE:  This code is not _yet_ compatible with Python3

#include "protopy.h"

// We include the other .cpp files directly
#include "protopipe.cpp"


extern "C" {

static PyMethodDef protokit_methods[] = {{NULL}};

#if  PY_MAJOR_VERSION >= 3
    static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "protokit",                     /* m_name */
        "Python wrapper for Protokit",  /* m_doc */
        -1,                             /* m_size */
        protokit_methods,               /* m_methods */
        NULL,                           /* m_reload */
        NULL,                           /* m_traverse */
        NULL,                           /* m_clear */
        NULL,                           /* m_free */
    };
#endif  // end PY_MAJOR_VERSION >= 3


static PyObject *
moduleinit(void)
{
    PyObject *m;

    if (PyType_Ready(&PipeType) < 0)
            return m;

#if PY_MAJOR_VERSION >= 3
    m = PyModule_Create(&moduledef);
#else
    m = Py_InitModule3("protokit", protokit_methods, "Python wrapper for Protokit");
#endif

    if (m == NULL)
        return NULL;

    ProtoError = PyErr_NewException((char*)"protokit.ProtoError", NULL, NULL);
    Py_INCREF(ProtoError);
    PyModule_AddObject(m, "ProtoError", ProtoError);

    Py_INCREF(&PipeType);
    PyModule_AddObject(m, "Pipe", (PyObject *)&PipeType);

  return m;
}

#if PY_MAJOR_VERSION < 3
    PyMODINIT_FUNC
    initprotokit(void)
    {
        moduleinit();
    }
#else
    PyMODINIT_FUNC
    PyInit_protokit(void)
    {
        return moduleinit();
    }
#endif


} // extern "C"
