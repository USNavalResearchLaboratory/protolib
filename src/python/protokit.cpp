/**
 * Python ProtoKit Wrapper
 * By: Tom Wambold  and Brian Adamson
 */

#define PY_SSIZE_T_CLEAN
#include "protopy.h"

// We include the other .cpp files directly
#include "../common/protoSpace.cpp"
#include "protopipe.cpp"
#include "prototree.cpp"
#include "protospace.cpp"
#include "protoqueue.cpp"

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

static PyObject* moduleinit(void)
{
    PyObject* m = NULL;

    if (PyType_Ready(&PipeType) < 0)
        return m;

    if (PyType_Ready(&SpaceType) < 0)
        return m;

    if (PyType_Ready(&SpaceIteratorType) < 0)
        return m;

    if (PyType_Ready(&SortedQueueType) < 0)
        return m;

    if (PyType_Ready(&SortedQueueIteratorType) < 0)
        return m;

    if (PyType_Ready(&SortedTreeType) < 0)
        return m;

    if (PyType_Ready(&SortedTreeIteratorType) < 0)
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
    PyModule_AddObject(m, "Pipe", (PyObject*)&PipeType);

    Py_INCREF(&SpaceType);
    PyModule_AddObject(m, "Space", (PyObject*)&SpaceType);

    Py_INCREF(&SpaceIteratorType);
    PyModule_AddObject(m, "Space.Iterator", (PyObject*)&SpaceIteratorType);

    // This sets an Space.Iterator attribute so that Space.Iterator is nested class of Space
    PyDict_SetItemString(SpaceType.tp_dict, "Iterator",  (PyObject*)&SpaceIteratorType);

    Py_INCREF(&SortedQueueType);
    PyModule_AddObject(m, "SortedQueue", (PyObject*)&SortedQueueType);

    Py_INCREF(&SortedQueueIteratorType);
    PyModule_AddObject(m, "SortedQueue.Iterator", (PyObject*)&SortedQueueIteratorType);

    // This sets an SortedQueue.Iterator attribute so that Space.Iterator is nested class of Space
    PyDict_SetItemString(SortedQueueType.tp_dict, "Iterator",  (PyObject*)&SortedQueueIteratorType);

    Py_INCREF(&SortedTreeType);
    PyModule_AddObject(m, "SortedTree", (PyObject*)&SortedTreeType);

    Py_INCREF(&SortedTreeIteratorType);
    PyModule_AddObject(m, "SortedTree.Iterator", (PyObject*)&SortedTreeIteratorType);

    // This sets an SortedTree.Iterator attribute so that Space.Iterator is nested class of Space
    PyDict_SetItemString(SortedTreeType.tp_dict, "Iterator",  (PyObject*)&SortedTreeIteratorType);

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
