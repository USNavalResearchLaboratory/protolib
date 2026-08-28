// Python binding code for ProtoSortedQueue class
//
// This wrapper provides a one-dimensional Python binding to ProtoSortedQueue
// with the sorting key type selected when the queue is constructed.
//
// Examples:
//
//     q = protokit.SortedQueue(float)
//     q.insert(obj, 12.5)
//
//     q = protokit.SortedQueue(int)                  // signed=True by default
//     q.insert(obj, -10)
//
//     q = protokit.SortedQueue(int, signed=False)    // uint64_t ordering
//     q.insert(obj, 10)
//
//     q = protokit.SortedQueue(str)
//     q.insert(obj, "hello")
//
//     q.remove(obj)
//     obj = q.find(key)              // first exact-key match, or None
//     obj = q.head()                 // minimum-key object, or None
//     obj = q.tail()                 // maximum-key object, or None
//     key = q.key(obj)               // original Python key object
//     n = q.size()
//     q.clear()
//
//     for obj in q.iterate():
//         ...                        // ascending key order
//
//     for obj in q.iterate(start):
//         ...                        // first key >= start, then ascending
//
//     for obj in q.iterate(start, True):
//         ...                        // first key <= start, then descending
//
// Constructor:
//
//     SortedQueue(key_type=float, signed=True, use_pool=False)
//
// "key_type" must be the Python built-in type float, int, or str.
//
// For int queues, Python integers are represented as fixed-width 64-bit C++
// values. signed=True uses int64_t ordering; signed=False uses uint64_t
// ordering. Values outside the corresponding 64-bit range raise OverflowError.
//
// The "signed" argument is only valid when key_type is int.
//
// Key sorting configuration:
//
//                         Endian       UseSignBit   UseComplement2
//     float               native          true          false
//     int signed          native          true           true
//     int unsigned        native         false          false
//     str                 big            false          false
//
// Float keys must actually be Python float objects; integer values are not
// silently converted. NaN is rejected. -0.0 is normalized to +0.0 in the
// C++ sorting representation as required by ProtoSortedTree.
//
// String keys must actually be Python str objects. They are converted to UTF-8
// and then encoded as:
//
//     0x01, byte0, 0x01, byte1, ... 0x00
//
// This provides a non-empty, prefix-free tree key while preserving normal
// UTF-8 lexical ordering (and thus Python Unicode code-point ordering).
//
// Multiple objects may have the same sorting key, as supported by
// ProtoSortedQueue. A given Python object may only be inserted once in a
// particular SortedQueue so that remove(obj) and key(obj) remain unambiguous.

#define PY_SSIZE_T_CLEAN
#include "protopy.h"
#include "protoQueue.h"

#include <stdint.h>
#include <limits.h>
#include <new>
#include <string>


enum PythonQueueKeyType
{
    QUEUE_KEY_FLOAT,
    QUEUE_KEY_INT,
    QUEUE_KEY_STRING
};


// Base item stored by ProtoSortedQueue.
//
// The Python sorting-key object is retained so key(obj) can return the
// original Python key. The Python data object itself is retained by the
// QueueItem object->QueueNode mapping below.
class QueueNode : public ProtoQueue::Item
{
    public:
        QueueNode(PyObject* pyObj, PyObject* pyKey)
          : py_object(pyObj), py_key(pyKey)
        {
            Py_INCREF(py_key);
        }

        virtual ~QueueNode()
        {
            Py_DECREF(py_key);
        }

        PyObject* GetObject() const
            {return py_object;}

        PyObject* GetPythonKey() const
        {
            Py_INCREF(py_key);
            return py_key;
        }

        virtual const char* GetKeyPtr() const = 0;
        virtual unsigned int GetKeysize() const = 0;

    private:
        PyObject* py_object;  // reference is owned by QueueItem
        PyObject* py_key;     // reference is owned by this QueueNode
};  // end class QueueNode


template <typename KEY_TYPE>
class NumericQueueNode : public QueueNode
{
    public:
        NumericQueueNode(PyObject* pyObj, PyObject* pyKey, KEY_TYPE theKey)
          : QueueNode(pyObj, pyKey), item_key(theKey) {}

        virtual ~NumericQueueNode()
        {
            // Do this before item_key storage disappears. ProtoQueue::Item
            // cleanup may indirectly call PythonSortedQueue::GetKey().
            Cleanup();
        }

        const char* GetKeyPtr() const
            {return reinterpret_cast<const char*>(&item_key);}

        unsigned int GetKeysize() const
            {return static_cast<unsigned int>(sizeof(KEY_TYPE) << 3);}

    private:
        KEY_TYPE item_key;
};  // end class NumericQueueNode


class StringQueueNode : public QueueNode
{
    public:
        StringQueueNode(PyObject* pyObj,
                        PyObject* pyKey,
                        const std::string& encodedKey)
          : QueueNode(pyObj, pyKey), item_key(encodedKey) {}

        virtual ~StringQueueNode()
        {
            // Do this before item_key storage disappears.
            Cleanup();
        }

        const char* GetKeyPtr() const
            {return item_key.data();}

        unsigned int GetKeysize() const
            {return static_cast<unsigned int>(item_key.size() << 3);}

    private:
        std::string item_key;
};  // end class StringQueueNode


// Concrete ProtoSortedQueue implementation whose sort interpretation is fixed
// at construction time.
class PythonSortedQueue : public ProtoSortedQueue
{
    public:
        PythonSortedQueue(PythonQueueKeyType theKeyType,
                          bool               signedInt,
                          bool               usePool = false)
          : ProtoSortedQueue(usePool),
            key_type(theKeyType),
            signed_int(signedInt),
            key_endian(ProtoTree::ENDIAN_BIG),
            use_sign_bit(false),
            use_complement2(false)
        {
            switch (key_type)
            {
                case QUEUE_KEY_FLOAT:
                    key_endian = ProtoTree::GetNativeEndian();
                    use_sign_bit = true;
                    use_complement2 = false;
                    break;

                case QUEUE_KEY_INT:
                    key_endian = ProtoTree::GetNativeEndian();
                    use_sign_bit = signed_int;
                    use_complement2 = signed_int;
                    break;

                case QUEUE_KEY_STRING:
                    key_endian = ProtoTree::ENDIAN_BIG;
                    use_sign_bit = false;
                    use_complement2 = false;
                    break;
            }
        }

        virtual ~PythonSortedQueue()
        {
            // ProtoSortedQueue::~ProtoSortedQueue() also calls Empty(), but
            // calling it here guarantees the most-derived virtual overrides
            // remain available while Containers are detached.
            Empty();
        }

        virtual const char* GetKey(const ProtoQueue::Item& item) const
        {
            return static_cast<const QueueNode&>(item).GetKeyPtr();
        }

        virtual unsigned int GetKeysize(const ProtoQueue::Item& item) const
        {
            return static_cast<const QueueNode&>(item).GetKeysize();
        }

        virtual ProtoTree::Endian GetEndian() const
            {return key_endian;}

        virtual bool UseComplement2() const
            {return use_complement2;}

        virtual bool UseSignBit() const
            {return use_sign_bit;}

        PythonQueueKeyType GetKeyType() const
            {return key_type;}

        bool IsSignedInt() const
            {return signed_int;}

    private:
        PythonQueueKeyType key_type;
        bool               signed_int;
        ProtoTree::Endian  key_endian;
        bool               use_sign_bit;
        bool               use_complement2;
};  // end class PythonSortedQueue


// Object-pointer -> QueueNode mapping used to make remove(obj) and key(obj)
// efficient without requiring the user's Python object to carry C++ state.
class QueueItem : public ProtoTree::Item
{
    public:
        QueueItem(PyObject* pyObj, QueueNode& queueNode)
          : py_object(pyObj), queue_node(queueNode)
        {
            Py_INCREF(py_object);
        }

        virtual ~QueueItem()
        {
            Py_DECREF(py_object);
        }

        QueueNode& GetNode()
            {return queue_node;}

        const QueueNode& GetNode() const
            {return queue_node;}

        const char* GetKey() const
            {return reinterpret_cast<const char*>(&py_object);}

        unsigned int GetKeysize() const
            {return static_cast<unsigned int>(sizeof(PyObject*) << 3);}

    private:
        PyObject*   py_object;
        QueueNode&  queue_node;
};  // end class QueueItem


class QueueItemTree : public ProtoTreeTemplate<QueueItem>
{
    public:
        QueueItem* FindItem(const PyObject* pyObj)
        {
            return Find(reinterpret_cast<const char*>(&pyObj),
                        static_cast<unsigned int>(sizeof(PyObject*) << 3));
        }

        const QueueItem* FindItem(const PyObject* pyObj) const
        {
            return Find(reinterpret_cast<const char*>(&pyObj),
                        static_cast<unsigned int>(sizeof(PyObject*) << 3));
        }
};  // end class QueueItemTree


// Temporary converted key used by find() and iterator construction and as the
// source for QueueNode creation during insert().
class QueueKeyBuffer
{
    public:
        QueueKeyBuffer()
          : float_key(0.0),
            signed_key(0),
            unsigned_key(0),
            key_ptr(NULL),
            keysize(0) {}

        double       float_key;
        int64_t      signed_key;
        uint64_t     unsigned_key;
        std::string  string_key;
        const char*  key_ptr;
        unsigned int keysize;
};


// Convert UTF-8 bytes to a non-empty prefix-free key.
//
// Every source byte is preceded by 0x01 and the entire key is terminated by
// 0x00. Thus:
//   ""    -> 00
//   "a"   -> 01 61 00
//   "aa"  -> 01 61 01 61 00
// and ordinary lexical ordering is preserved even for embedded U+0000.
static bool Queue_EncodeString(const char* utf8,
                               Py_ssize_t utf8Len,
                               std::string& encoded)
{
    if (utf8Len < 0)
    {
        PyErr_SetString(PyExc_ValueError, "invalid UTF-8 string length");
        return false;
    }

    // 2*n + 1 bytes, converted to key bits below.
    const size_t n = static_cast<size_t>(utf8Len);
    const size_t maxKeyBytes = static_cast<size_t>(UINT_MAX >> 3);
    if (n > ((maxKeyBytes - 1) >> 1))
    {
        PyErr_SetString(PyExc_OverflowError,
                        "SortedQueue string key is too long");
        return false;
    }

    try
    {
        encoded.resize(2 * n + 1);
    }
    catch (const std::bad_alloc&)
    {
        PyErr_NoMemory();
        return false;
    }

    for (size_t i = 0; i < n; ++i)
    {
        encoded[2 * i]     = '\x01';
        encoded[2 * i + 1] = utf8[i];
    }
    encoded[2 * n] = '\x00';
    return true;
}  // end Queue_EncodeString()


// Strictly validate and convert a Python key according to the queue's declared
// key type. This one routine is used by insert(), find(), and iterate(start)
// so they cannot disagree about key semantics.
static bool Queue_ConvertKey(const PythonSortedQueue& queue,
                             PyObject*                pyKey,
                             QueueKeyBuffer&          key)
{
    switch (queue.GetKeyType())
    {
        case QUEUE_KEY_FLOAT:
        {
            if (!PyFloat_Check(pyKey))
            {
                PyErr_SetString(PyExc_TypeError,
                                "SortedQueue(float) key must be a float");
                return false;
            }

            key.float_key = PyFloat_AsDouble(pyKey);
            if (PyErr_Occurred())
                return false;

            // Reject NaN. +/-inf are sortable and are allowed.
            if (key.float_key != key.float_key)
            {
                PyErr_SetString(PyExc_ValueError,
                                "SortedQueue float key may not be NaN");
                return false;
            }

            // ProtoSortedTree's documented double-key configuration requires
            // -0.0 to be normalized to +0.0.
            if (0.0 == key.float_key)
                key.float_key = 0.0;

            key.key_ptr = reinterpret_cast<const char*>(&key.float_key);
            key.keysize = static_cast<unsigned int>(sizeof(double) << 3);
            return true;
        }

        case QUEUE_KEY_INT:
        {
            if (!PyLong_Check(pyKey) || PyBool_Check(pyKey))
            {
                PyErr_SetString(PyExc_TypeError,
                                "SortedQueue(int) key must be an int (not bool)");
                return false;
            }

            if (queue.IsSignedInt())
            {
                long long value = PyLong_AsLongLong(pyKey);
                if ((-1 == value) && PyErr_Occurred())
                    return false;

                key.signed_key = static_cast<int64_t>(value);
                key.key_ptr = reinterpret_cast<const char*>(&key.signed_key);
                key.keysize = static_cast<unsigned int>(sizeof(int64_t) << 3);
            }
            else
            {
                unsigned long long value = PyLong_AsUnsignedLongLong(pyKey);
                if ((static_cast<unsigned long long>(-1) == value) &&
                    PyErr_Occurred())
                    return false;

                key.unsigned_key = static_cast<uint64_t>(value);
                key.key_ptr = reinterpret_cast<const char*>(&key.unsigned_key);
                key.keysize = static_cast<unsigned int>(sizeof(uint64_t) << 3);
            }
            return true;
        }

        case QUEUE_KEY_STRING:
        {
            if (!PyUnicode_Check(pyKey))
            {
                PyErr_SetString(PyExc_TypeError,
                                "SortedQueue(str) key must be a str");
                return false;
            }

            Py_ssize_t utf8Len = 0;
            const char* utf8 = PyUnicode_AsUTF8AndSize(pyKey, &utf8Len);
            if (NULL == utf8)
                return false;

            if (!Queue_EncodeString(utf8, utf8Len, key.string_key))
                return false;

            key.key_ptr = key.string_key.data();
            key.keysize =
                static_cast<unsigned int>(key.string_key.size() << 3);
            return true;
        }
    }

    PyErr_SetString(ProtoError, "invalid SortedQueue key type");
    return false;
}  // end Queue_ConvertKey()


static QueueNode* Queue_CreateNode(const PythonSortedQueue& queue,
                                   PyObject*                pyObj,
                                   PyObject*                pyKey,
                                   const QueueKeyBuffer&    key)
{
    QueueNode* node = NULL;

    try
    {
        switch (queue.GetKeyType())
        {
            case QUEUE_KEY_FLOAT:
                node = new (std::nothrow)
                    NumericQueueNode<double>(pyObj, pyKey, key.float_key);
                break;

            case QUEUE_KEY_INT:
                if (queue.IsSignedInt())
                {
                    node = new (std::nothrow)
                        NumericQueueNode<int64_t>(
                            pyObj, pyKey, key.signed_key);
                }
                else
                {
                    node = new (std::nothrow)
                        NumericQueueNode<uint64_t>(
                            pyObj, pyKey, key.unsigned_key);
                }
                break;

            case QUEUE_KEY_STRING:
                node = new (std::nothrow)
                    StringQueueNode(pyObj, pyKey, key.string_key);
                break;
        }
    }
    catch (const std::bad_alloc&)
    {
        PyErr_NoMemory();
        return NULL;
    }

    if (NULL == node)
        PyErr_NoMemory();

    return node;
}  // end Queue_CreateNode()


static bool Queue_ParseKeyType(PyObject* pyType, PythonQueueKeyType& keyType)
{
    if (pyType == reinterpret_cast<PyObject*>(&PyFloat_Type))
    {
        keyType = QUEUE_KEY_FLOAT;
        return true;
    }
    else if (pyType == reinterpret_cast<PyObject*>(&PyLong_Type))
    {
        keyType = QUEUE_KEY_INT;
        return true;
    }
    else if (pyType == reinterpret_cast<PyObject*>(&PyUnicode_Type))
    {
        keyType = QUEUE_KEY_STRING;
        return true;
    }

    PyErr_SetString(PyExc_TypeError,
                    "SortedQueue key_type must be float, int, or str");
    return false;
}  // end Queue_ParseKeyType()


extern "C" {

    typedef struct {
        PyObject_HEAD
        PythonSortedQueue* thisptr;
        QueueItemTree      item_tree;
        Py_ssize_t         item_count;
    } SortedQueue;

    typedef struct {
        PyObject_HEAD
        ProtoSortedQueue::Iterator* thisptr;
        PyObject*                   py_queue;
        bool                        reverse;
    } SortedQueueIterator;


    static void SortedQueue_dealloc(SortedQueue* self)
    {
        if (NULL != self->thisptr)
        {
            // Destroy() deletes all QueueNode objects. QueueItem entries
            // retain the Python data-object references until item_tree is
            // destroyed below.
            self->thisptr->Destroy();
            delete self->thisptr;
            self->thisptr = NULL;
        }

        self->item_tree.Destroy();
        self->item_count = 0;

        Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
    }  // end SortedQueue_dealloc()


    static PyObject* SortedQueue_new(PyTypeObject* type,
                                     PyObject* args,
                                     PyObject* kwargs)
    {
        SortedQueue* self =
            reinterpret_cast<SortedQueue*>(type->tp_alloc(type, 0));
        if (NULL == self)
            return NULL;

        self->thisptr = NULL;
        self->item_count = 0;
        return reinterpret_cast<PyObject*>(self);
    }  // end SortedQueue_new()


    static int SortedQueue_init(SortedQueue* self,
                                PyObject* args,
                                PyObject* kwargs)
    {
        PyObject* pyKeyType =
            reinterpret_cast<PyObject*>(&PyFloat_Type);
        int signedInt = 1;
        int usePool = 0;

        static char* kwlist[] = {
            const_cast<char*>("key_type"),
            const_cast<char*>("signed"),
            const_cast<char*>("use_pool"),
            NULL
        };

        // Detect whether "signed" was explicitly supplied so we can reject it
        // for non-int queues while still having signed=True as the default.
        bool signedProvided = (PyTuple_Size(args) >= 2);
        if ((NULL != kwargs) &&
            (NULL != PyDict_GetItemString(kwargs, "signed")))
        {
            signedProvided = true;
        }

        if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Opp:SortedQueue",
                                         kwlist,
                                         &pyKeyType,
                                         &signedInt,
                                         &usePool))
        {
            return -1;
        }

        PythonQueueKeyType keyType;
        if (!Queue_ParseKeyType(pyKeyType, keyType))
            return -1;

        if ((QUEUE_KEY_INT != keyType) && signedProvided)
        {
            PyErr_SetString(PyExc_TypeError,
                            "'signed' is only valid for SortedQueue(int)");
            return -1;
        }

        // Reinitialization is unusual but Python permits explicit __init__()
        // calls, so safely release any previous queue state.
        if (NULL != self->thisptr)
        {
            self->thisptr->Destroy();
            delete self->thisptr;
            self->thisptr = NULL;

            self->item_tree.Destroy();
            self->item_count = 0;
        }

        self->thisptr = new (std::nothrow)
            PythonSortedQueue(keyType, 0 != signedInt, 0 != usePool);

        if (NULL == self->thisptr)
        {
            PyErr_NoMemory();
            return -1;
        }

        return 0;
    }  // end SortedQueue_init()


    // SortedQueue.insert(object, key)
    static PyObject* SortedQueue_insert(SortedQueue* self, PyObject* args)
    {
        PyObject* pyObj = NULL;
        PyObject* pyKey = NULL;

        if (!PyArg_ParseTuple(args, "OO:insert", &pyObj, &pyKey))
            return NULL;

        QueueKeyBuffer key;
        if (!Queue_ConvertKey(*self->thisptr, pyKey, key))
            return NULL;

        // A ProtoQueue::Item cannot appear more than once in the same queue.
        // Each Python object maps to one QueueNode, so reject duplicates.
        if (NULL != self->item_tree.FindItem(pyObj))
        {
            PyErr_SetString(ProtoError,
                            "SortedQueue.insert() object is already in queue");
            return NULL;
        }

        QueueNode* queueNode =
            Queue_CreateNode(*self->thisptr, pyObj, pyKey, key);
        if (NULL == queueNode)
            return NULL;

        if (!self->thisptr->Insert(*queueNode))
        {
            delete queueNode;
            PyErr_SetString(ProtoError,
                            "ProtoSortedQueue::Insert() error");
            return NULL;
        }

        QueueItem* queueItem =
            new (std::nothrow) QueueItem(pyObj, *queueNode);

        if (NULL == queueItem)
        {
            self->thisptr->Remove(*queueNode);
            delete queueNode;
            PyErr_NoMemory();
            return NULL;
        }

        if (!self->item_tree.Insert(*queueItem))
        {
            self->thisptr->Remove(*queueNode);
            delete queueNode;
            delete queueItem;
            PyErr_SetString(ProtoError, "QueueItem insertion error");
            return NULL;
        }

        ++self->item_count;
        Py_RETURN_NONE;
    }  // end SortedQueue_insert()


    // SortedQueue.remove(object)
    static PyObject* SortedQueue_remove(SortedQueue* self, PyObject* args)
    {
        PyObject* pyObj = NULL;

        if (!PyArg_ParseTuple(args, "O:remove", &pyObj))
            return NULL;

        QueueItem* queueItem = self->item_tree.FindItem(pyObj);
        if (NULL == queueItem)
        {
            PyErr_SetString(ProtoError,
                            "SortedQueue.remove() object is not in queue");
            return NULL;
        }

        QueueNode& node = queueItem->GetNode();

        self->thisptr->Remove(node);
        self->item_tree.Remove(*queueItem);

        delete &node;
        delete queueItem;

        --self->item_count;
        Py_RETURN_NONE;
    }  // end SortedQueue_remove()


    // SortedQueue.find(key) -> first exact-key object or None
    static PyObject* SortedQueue_find(SortedQueue* self, PyObject* args)
    {
        PyObject* pyKey = NULL;

        if (!PyArg_ParseTuple(args, "O:find", &pyKey))
            return NULL;

        QueueKeyBuffer key;
        if (!Queue_ConvertKey(*self->thisptr, pyKey, key))
            return NULL;

        ProtoQueue::Item* item =
            self->thisptr->Find(key.key_ptr, key.keysize);

        if (NULL == item)
            Py_RETURN_NONE;

        PyObject* obj = static_cast<QueueNode*>(item)->GetObject();
        Py_INCREF(obj);
        return obj;
    }  // end SortedQueue_find()


    // SortedQueue.head() -> minimum-key object or None
    static PyObject* SortedQueue_head(SortedQueue* self, PyObject* args)
    {
        ProtoQueue::Item* item = self->thisptr->GetHead();

        if (NULL == item)
            Py_RETURN_NONE;

        PyObject* obj = static_cast<QueueNode*>(item)->GetObject();
        Py_INCREF(obj);
        return obj;
    }  // end SortedQueue_head()


    // SortedQueue.tail() -> maximum-key object or None
    static PyObject* SortedQueue_tail(SortedQueue* self, PyObject* args)
    {
        ProtoQueue::Item* item = self->thisptr->GetTail();

        if (NULL == item)
            Py_RETURN_NONE;

        PyObject* obj = static_cast<QueueNode*>(item)->GetObject();
        Py_INCREF(obj);
        return obj;
    }  // end SortedQueue_tail()


    // SortedQueue.key(object) -> original Python key object
    static PyObject* SortedQueue_key(SortedQueue* self, PyObject* args)
    {
        PyObject* pyObj = NULL;

        if (!PyArg_ParseTuple(args, "O:key", &pyObj))
            return NULL;

        QueueItem* queueItem = self->item_tree.FindItem(pyObj);
        if (NULL == queueItem)
        {
            PyErr_SetString(ProtoError,
                            "SortedQueue.key() object is not in queue");
            return NULL;
        }

        return queueItem->GetNode().GetPythonKey();
    }  // end SortedQueue_key()


    static PyObject* SortedQueue_size(SortedQueue* self, PyObject* args)
    {
        return PyLong_FromSsize_t(self->item_count);
    }  // end SortedQueue_size()


    static PyObject* SortedQueue_is_empty(SortedQueue* self, PyObject* args)
    {
        if (self->thisptr->IsEmpty())
            Py_RETURN_TRUE;

        Py_RETURN_FALSE;
    }  // end SortedQueue_is_empty()


    static PyObject* SortedQueue_clear(SortedQueue* self, PyObject* args)
    {
        // Delete QueueNodes first. QueueItem entries still retain the Python
        // data objects until the object index is destroyed immediately after.
        self->thisptr->Destroy();
        self->item_tree.Destroy();
        self->item_count = 0;

        Py_RETURN_NONE;
    }  // end SortedQueue_clear()


    static PyObject* SortedQueue_iterate(SortedQueue* self, PyObject* args);


    static PyMethodDef SortedQueue_methods[] =
    {
        {"insert", (PyCFunction)SortedQueue_insert, METH_VARARGS,
            "insert(object, key) -> None. Insert object using declared key type."},
        {"remove", (PyCFunction)SortedQueue_remove, METH_VARARGS,
            "remove(object) -> None. Remove object from queue."},
        {"find", (PyCFunction)SortedQueue_find, METH_VARARGS,
            "find(key) -> object or None. Return first exact-key match."},
        {"head", (PyCFunction)SortedQueue_head, METH_NOARGS,
            "head() -> object or None. Return minimum-key object."},
        {"tail", (PyCFunction)SortedQueue_tail, METH_NOARGS,
            "tail() -> object or None. Return maximum-key object."},
        {"key", (PyCFunction)SortedQueue_key, METH_VARARGS,
            "key(object) -> key. Return object's original Python key."},
        {"size", (PyCFunction)SortedQueue_size, METH_NOARGS,
            "size() -> int. Return number of objects in queue."},
        {"is_empty", (PyCFunction)SortedQueue_is_empty, METH_NOARGS,
            "is_empty() -> bool."},
        {"clear", (PyCFunction)SortedQueue_clear, METH_NOARGS,
            "clear() -> None. Remove all objects."},
        {"iterate", (PyCFunction)SortedQueue_iterate, METH_VARARGS,
            "iterate([start [, reverse]]) -> iterator."},
        {NULL}
    };


    static PyTypeObject SortedQueueType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "protokit.SortedQueue",              /* tp_name */
        sizeof(SortedQueue),                 /* tp_basicsize */
        0,                                   /* tp_itemsize */
        (destructor)SortedQueue_dealloc,     /* tp_dealloc */
        0,                                   /* tp_print / vectorcall */
        0,                                   /* tp_getattr */
        0,                                   /* tp_setattr */
        0,                                   /* tp_compare / reserved */
        0,                                   /* tp_repr */
        0,                                   /* tp_as_number */
        0,                                   /* tp_as_sequence */
        0,                                   /* tp_as_mapping */
        0,                                   /* tp_hash */
        0,                                   /* tp_call */
        0,                                   /* tp_str */
        0,                                   /* tp_getattro */
        0,                                   /* tp_setattro */
        0,                                   /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
        "ProtoSortedQueue wrapper for float, int, and str keys", /* tp_doc */
        0,                                   /* tp_traverse */
        0,                                   /* tp_clear */
        0,                                   /* tp_richcompare */
        0,                                   /* tp_weaklistoffset */
        0,                                   /* tp_iter */
        0,                                   /* tp_iternext */
        SortedQueue_methods,                 /* tp_methods */
        0,                                   /* tp_members */
        0,                                   /* tp_getset */
        0,                                   /* tp_base */
        0,                                   /* tp_dict */
        0,                                   /* tp_descr_get */
        0,                                   /* tp_descr_set */
        0,                                   /* tp_dictoffset */
        (initproc)SortedQueue_init,          /* tp_init */
        0,                                   /* tp_alloc */
        SortedQueue_new,                     /* tp_new */
    };


    // ---------------------------------------------------------------------
    // protokit.SortedQueue.Iterator
    // ---------------------------------------------------------------------

    static void SortedQueueIterator_dealloc(SortedQueueIterator* self)
    {
        if (NULL != self->thisptr)
        {
            delete self->thisptr;
            self->thisptr = NULL;
        }

        if (NULL != self->py_queue)
        {
            Py_DECREF(self->py_queue);
            self->py_queue = NULL;
        }

        Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
    }  // end SortedQueueIterator_dealloc()


    static PyObject* SortedQueueIterator_new(PyTypeObject* type,
                                             PyObject* args,
                                             PyObject* kwargs)
    {
        SortedQueueIterator* self =
            reinterpret_cast<SortedQueueIterator*>(
                type->tp_alloc(type, 0));

        if (NULL == self)
            return NULL;

        self->thisptr = NULL;
        self->py_queue = NULL;
        self->reverse = false;
        return reinterpret_cast<PyObject*>(self);
    }  // end SortedQueueIterator_new()


    // Iterator(queue [, start [, reverse]])
    static int SortedQueueIterator_init(SortedQueueIterator* self,
                                        PyObject* args,
                                        PyObject* kwargs)
    {
        PyObject* pyQueue = NULL;
        PyObject* pyStart = Py_None;
        int reverse = 0;

        static char* kwlist[] = {
            const_cast<char*>("queue"),
            const_cast<char*>("start"),
            const_cast<char*>("reverse"),
            NULL
        };

        if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|Op",
                                         kwlist,
                                         &SortedQueueType,
                                         &pyQueue,
                                         &pyStart,
                                         &reverse))
        {
            return -1;
        }

        SortedQueue* queue =
            reinterpret_cast<SortedQueue*>(pyQueue);

        QueueKeyBuffer startKey;
        const char* keyPtr = NULL;
        unsigned int keysize = 0;

        if (Py_None != pyStart)
        {
            if (!Queue_ConvertKey(*queue->thisptr, pyStart, startKey))
                return -1;

            keyPtr = startKey.key_ptr;
            keysize = startKey.keysize;
        }

        if (NULL != self->thisptr)
        {
            delete self->thisptr;
            self->thisptr = NULL;
        }

        if (NULL != self->py_queue)
        {
            Py_DECREF(self->py_queue);
            self->py_queue = NULL;
        }

        self->reverse = (0 != reverse);

        self->thisptr = new (std::nothrow)
            ProtoSortedQueue::Iterator(
                *queue->thisptr, self->reverse, keyPtr, keysize);

        if (NULL == self->thisptr)
        {
            PyErr_NoMemory();
            return -1;
        }

        // Keep the queue, all QueueNodes, and their Python references alive
        // for the lifetime of the iterator.
        Py_INCREF(pyQueue);
        self->py_queue = pyQueue;

        return 0;
    }  // end SortedQueueIterator_init()


    static PyObject* SortedQueueIterator_next(PyObject* self)
    {
        SortedQueueIterator* iterator =
            reinterpret_cast<SortedQueueIterator*>(self);

        ProtoQueue::Item* next =
            iterator->reverse ?
                iterator->thisptr->GetPrevItem() :
                iterator->thisptr->GetNextItem();

        if (NULL == next)
        {
            PyErr_SetNone(PyExc_StopIteration);
            return NULL;
        }

        PyObject* obj = static_cast<QueueNode*>(next)->GetObject();
        Py_INCREF(obj);
        return obj;
    }  // end SortedQueueIterator_next()


    static PyObject* SortedQueueIterator_iter(PyObject* self)
    {
        Py_INCREF(self);
        return self;
    }  // end SortedQueueIterator_iter()


    static PyMethodDef SortedQueueIterator_methods[] =
    {
        {NULL}
    };


    static PyTypeObject SortedQueueIteratorType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "protokit.SortedQueue.Iterator",         /* tp_name */
        sizeof(SortedQueueIterator),             /* tp_basicsize */
        0,                                       /* tp_itemsize */
        (destructor)SortedQueueIterator_dealloc, /* tp_dealloc */
        0,                                       /* tp_print / vectorcall */
        0,                                       /* tp_getattr */
        0,                                       /* tp_setattr */
        0,                                       /* tp_compare / reserved */
        0,                                       /* tp_repr */
        0,                                       /* tp_as_number */
        0,                                       /* tp_as_sequence */
        0,                                       /* tp_as_mapping */
        0,                                       /* tp_hash */
        0,                                       /* tp_call */
        0,                                       /* tp_str */
        0,                                       /* tp_getattro */
        0,                                       /* tp_setattro */
        0,                                       /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
        "ProtoSortedQueue::Iterator wrapper",    /* tp_doc */
        0,                                       /* tp_traverse */
        0,                                       /* tp_clear */
        0,                                       /* tp_richcompare */
        0,                                       /* tp_weaklistoffset */
        SortedQueueIterator_iter,                /* tp_iter */
        SortedQueueIterator_next,                /* tp_iternext */
        SortedQueueIterator_methods,             /* tp_methods */
        0,                                       /* tp_members */
        0,                                       /* tp_getset */
        0,                                       /* tp_base */
        0,                                       /* tp_dict */
        0,                                       /* tp_descr_get */
        0,                                       /* tp_descr_set */
        0,                                       /* tp_dictoffset */
        (initproc)SortedQueueIterator_init,      /* tp_init */
        0,                                       /* tp_alloc */
        SortedQueueIterator_new,                 /* tp_new */
    };


    static PyObject* SortedQueue_iterate(SortedQueue* self, PyObject* args)
    {
        PyObject* pyStart = Py_None;
        int reverse = 0;

        if (!PyArg_ParseTuple(args, "|Op:iterate",
                              &pyStart, &reverse))
        {
            return NULL;
        }

        QueueKeyBuffer startKey;
        const char* keyPtr = NULL;
        unsigned int keysize = 0;

        if (Py_None != pyStart)
        {
            if (!Queue_ConvertKey(*self->thisptr, pyStart, startKey))
                return NULL;

            keyPtr = startKey.key_ptr;
            keysize = startKey.keysize;
        }

        SortedQueueIterator* iterator =
            PyObject_New(SortedQueueIterator,
                         &SortedQueueIteratorType);

        if (NULL == iterator)
            return NULL;

        iterator->thisptr = NULL;
        iterator->py_queue = NULL;
        iterator->reverse = (0 != reverse);

        iterator->thisptr = new (std::nothrow)
            ProtoSortedQueue::Iterator(
                *self->thisptr, iterator->reverse, keyPtr, keysize);

        if (NULL == iterator->thisptr)
        {
            Py_TYPE(iterator)->tp_free(
                reinterpret_cast<PyObject*>(iterator));
            PyErr_NoMemory();
            return NULL;
        }

        iterator->py_queue =
            reinterpret_cast<PyObject*>(self);
        Py_INCREF(reinterpret_cast<PyObject*>(self));

        return reinterpret_cast<PyObject*>(iterator);
    }  // end SortedQueue_iterate()

}  // end extern "C"
