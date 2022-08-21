
// Python binding code for ProtoPipe class

#include "protopy.h"
#include "protoPipe.h"
#include <protoSocket.h>

extern "C" {


    typedef struct {
        PyObject_HEAD
        ProtoPipe *thisptr;
        bool isConnected;
    } Pipe;

    static void Pipe_dealloc(Pipe *self) {
        if (self->isConnected)
            self->thisptr->Close();

        delete self->thisptr;
        Py_TYPE(self)->tp_free((PyObject*)self);
    }

    static PyObject* Pipe_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
        Pipe *self = (Pipe*)type->tp_alloc(type, 0);

        if (self == NULL)
            return NULL;

        self->thisptr = NULL;
        self->isConnected = false;

        return (PyObject*)self;
    }

    static int Pipe_init(Pipe *self, PyObject *args, PyObject *kwargs) {
        const char *typestr = NULL;
        static char *kwlist[] = {(char*)"type", NULL};

        if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &typestr))
            return -1;

        if (strcmp(typestr, "MESSAGE") == 0)
            self->thisptr = new ProtoPipe(ProtoPipe::MESSAGE);
        else if (strcmp(typestr, "STREAM") == 0)
            self->thisptr = new ProtoPipe(ProtoPipe::STREAM);
        else
            return -2;

        return 0;
    }

    static PyObject* Pipe_GetType(Pipe *self) {
        ProtoPipe::Type type = self->thisptr->GetType();

        switch (type) {
            case ProtoPipe::MESSAGE:
                return Py_BuildValue("s", "MESSAGE");
                break;

            case ProtoPipe::STREAM:
                return Py_BuildValue("s", "STREAM");
                break;

            default:
                PyErr_SetString(ProtoError, "Invalid Pipe Type");
                return NULL;
                break;
        }
    }

    static PyObject* Pipe_GetName(Pipe *self) {

#if PY_MAJOR_VERSION >= 3
        PyObject *rv = PyUnicode_FromString(self->thisptr->GetName()); //Human readable?
#else
        PyObject *rv = PyString_FromString(self->thisptr->GetName()); //Human readable?
#endif

        if (rv == NULL)
            PyErr_SetString(ProtoError, "Could not get Pipe name.");

        return rv;
    }

    static PyObject* Pipe_Connect(Pipe *self, PyObject *args) {
        const char * name;

        if (!PyArg_ParseTuple(args, "s", &name))
            return NULL;

        if (!self->thisptr->Connect(name)) {
            PyErr_SetString(ProtoError, "Could not connect to server.");
            return NULL;
        }

        self->isConnected = true;
        Py_RETURN_NONE;
    }

    static PyObject* Pipe_Listen(Pipe *self, PyObject *args)
    {
        const char *name;
        if (!PyArg_ParseTuple(args, "s", &name))
            return NULL;
        if (!self->thisptr->Listen(name)) {
            PyErr_SetString(ProtoError, "Could not start listener.");
            return NULL;
        }
        self->isConnected = true;
        Py_RETURN_NONE;
    }

    // Thisc currently only allows a single connection to be accepted
    // TBD - provide option to accept multiple connections
    static PyObject* Pipe_Accept(Pipe *self, PyObject *args) {

        if (!self->thisptr->Accept())
        {
            PyErr_SetString(ProtoError, "ProtoPipe::Accept() error");
            return NULL;
        }
        //PyErr_SetString(PyExc_NotImplementedError, "");
        //return NULL;
        Py_RETURN_NONE;
    }

    static PyObject* Pipe_Close(Pipe *self) {
        self->thisptr->Close();
        self->isConnected = false;
        Py_RETURN_NONE;
    }

    static PyObject* Pipe_Send(Pipe *self, PyObject *args) {
        const char *buffer;
        unsigned int size;

        if (!PyArg_ParseTuple(args, "s#", &buffer, &size))
            return NULL;

        if (!self->thisptr->Send(buffer, size)) {
            PyErr_SetString(ProtoError, "Could not send buffer.");
            return NULL;
        }
        Py_RETURN_NONE;
    }

    static PyObject* Pipe_Recv(Pipe *self, PyObject *args) {
        char *buffer;
        unsigned int size;
        bool result = false;

        if (!PyArg_ParseTuple(args, "I", &size))
            return NULL;

        buffer = new char[size];

        // Release the GIL since this can block...
        Py_BEGIN_ALLOW_THREADS
        result = self->thisptr->Recv(buffer, size);
        Py_END_ALLOW_THREADS

        if (!result) {
            PyErr_SetString(ProtoError, "Could not recv.");
            return NULL;
        }

#if PY_MAJOR_VERSION >= 3
        PyObject *rv = PyBytes_FromStringAndSize(buffer, size); //Human readable?
#else
        PyObject *rv = PyString_FromStringAndSize(buffer, size); //Human readable?
#endif
        delete[] buffer;
        return rv;
    }

    static PyObject* Pipe_GetHandle(Pipe *self) {
        return Py_BuildValue("i", self->thisptr->GetHandle());
    }

    static PyMethodDef Pipe_methods[] = {
        {"GetType", (PyCFunction)Pipe_GetType, METH_NOARGS,
            "Returns the type of the pipe."},
        {"GetName", (PyCFunction)Pipe_GetName, METH_NOARGS,
            "Returns the name of the pipe."},
        {"Connect", (PyCFunction)Pipe_Connect, METH_VARARGS,
            "Connects the pipe."},
        {"Listen", (PyCFunction)Pipe_Listen, METH_VARARGS,
            "Starts a listener on the pipe."},
        {"Accept", (PyCFunction)Pipe_Accept, METH_VARARGS,
            "Accepts multiple incoming connections."},
        {"Close", (PyCFunction)Pipe_Close, METH_NOARGS,
            "Closes the pipe."},
        {"Send", (PyCFunction)Pipe_Send, METH_VARARGS,
            "Send data on the pipe."},
        {"Recv", (PyCFunction)Pipe_Recv, METH_VARARGS,
            "Receive data on the pipe."},
        {"GetHandle", (PyCFunction)Pipe_GetHandle, METH_NOARGS,
            "Gets the file descriptor for this socket."},
        {NULL}
    };

    static PyTypeObject PipeType = {
        PyVarObject_HEAD_INIT(NULL,0) /*ob_size*/
        "protokit.Pipe",           /*tp_name*/
        sizeof(Pipe),              /*tp_basicsize*/
        0,                         /*tp_itemsize*/
        (destructor)Pipe_dealloc,  /*tp_dealloc*/
        0,                         /*tp_print*/
        0,                         /*tp_getattr*/
        0,                         /*tp_setattr*/
        0,                         /*tp_compare*/
        0,                         /*tp_repr*/
        0,                         /*tp_as_number*/
        0,                         /*tp_as_sequence*/
        0,                         /*tp_as_mapping*/
        0,                         /*tp_hash */
        0,                         /*tp_call*/
        0,                         /*tp_str*/
        0,                         /*tp_getattro*/
        0,                         /*tp_setattro*/
        0,                         /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
        "ProtoPipe wrapper",       /* tp_doc */
        0,		                   /* tp_traverse */
        0,		                   /* tp_clear */
        0,		                   /* tp_richcompare */
        0,		                   /* tp_weaklistoffset */
        0,		                   /* tp_iter */
        0,		                   /* tp_iternext */
        Pipe_methods,              /* tp_methods */
        0,                         /* tp_members */
        0,                         /* tp_getset */
        0,                         /* tp_base */
        0,                         /* tp_dict */
        0,                         /* tp_descr_get */
        0,                         /* tp_descr_set */
        0,                         /* tp_dictoffset */
        (initproc)Pipe_init,       /* tp_init */
        0,                         /* tp_alloc */
        Pipe_new,                  /* tp_new */
    };
}  // end extern "C"
