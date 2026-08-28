// Python binding code for ProtoSortedTree
//
// Direct Python binding to ProtoSortedTree for one-dimensional sorted keys.
//
// Examples:
//
//     tree = protokit.SortedTree(float)
//     tree.insert(obj, 12.5)
//
//     tree = protokit.SortedTree(int)                  // signed=True by default
//     tree.insert(obj, -10)
//
//     tree = protokit.SortedTree(int, signed=False)    // uint64_t ordering
//     tree.insert(obj, 10)
//
//     tree = protokit.SortedTree(str)
//     tree.insert(obj, "hello")
//
//     tree.remove(obj)
//     obj = tree.find(key)              // an exact-key match, or None
//     obj = tree.head()                 // minimum-key object, or None
//     obj = tree.tail()                 // maximum-key object, or None
//     key = tree.key(obj)               // original Python key object
//     n = tree.size()
//     tree.clear()
//
//     for obj in tree.iterate():
//         ...                            // ascending order
//
//     for obj in tree.iterate(10.0):
//         ...                            // first key >= 10.0
//
//     for obj in tree.iterate(10.0, True):
//         ...                            // first key <= 10.0, descending
//
// Constructor:
//
//     SortedTree(key_type=float, signed=True, unique=False)
//
// "key_type" must be the Python built-in type float, int, or str.
//
// For int trees, Python integers are represented as fixed-width 64-bit C++
// values. signed=True uses int64_t ordering; signed=False uses uint64_t
// ordering. Values outside the corresponding 64-bit range raise OverflowError.
//
// The "signed" argument is only meaningful for key_type == int.
//
// "unique=False" permits multiple Python objects to have the same sorting key.
// Duplicate-key handling is provided directly by ProtoSortedTree.  With
// unique=True, ProtoSortedTree rejects insertion of an item whose key is
// already present.  A given Python object may still be inserted only once in
// a particular SortedTree so remove(obj) and key(obj) remain unambiguous.
//
// Sorting configuration:
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
// This supplies a non-empty, prefix-free ProtoTree key while preserving UTF-8
// lexical order.
//
// NOTE: start-key iteration for signed floating point relies on the corrected
// ProtoSortedTree::Iterator::Reset() bound logic that honors UseSignBit() and
// UseComplement2() for non-exact start keys.

#define PY_SSIZE_T_CLEAN
#include "protopy.h"
#include "protoTree.h"

#include <stdint.h>
#include <limits.h>
#include <new>
#include <string>
#include <string.h>


enum PythonTreeKeyType
{
    TREE_KEY_FLOAT,
    TREE_KEY_INT,
    TREE_KEY_STRING
};


// ---------------------------------------------------------------------------
// Converted user-key storage
// ---------------------------------------------------------------------------

class TreeKeyBuffer
{
    public:
        TreeKeyBuffer()
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


// ---------------------------------------------------------------------------
// Concrete ProtoSortedTree configuration
//
// Each item exposes only its actual user sorting key.  ProtoSortedTree handles
// duplicate keys natively and enforces unique=True through its
// uniqueItemsOnly constructor argument.
// ---------------------------------------------------------------------------

class PythonSortedTree : public ProtoSortedTree
{
    public:
        PythonSortedTree(PythonTreeKeyType theKeyType,
                         bool              signedInt,
                         bool              uniqueItems)
          : ProtoSortedTree(uniqueItems),
            key_type(theKeyType),
            signed_int(signedInt),
            key_endian(ProtoTree::ENDIAN_BIG),
            use_sign_bit(false),
            use_complement2(false)
        {
            switch (key_type)
            {
                case TREE_KEY_FLOAT:
                    key_endian = ProtoTree::GetNativeEndian();
                    use_sign_bit = true;
                    use_complement2 = false;
                    break;

                case TREE_KEY_INT:
                    key_endian = ProtoTree::GetNativeEndian();
                    use_sign_bit = signed_int;
                    use_complement2 = signed_int;
                    break;

                case TREE_KEY_STRING:
                    key_endian = ProtoTree::ENDIAN_BIG;
                    use_sign_bit = false;
                    use_complement2 = false;
                    break;
            }
        }

        virtual ~PythonSortedTree()
        {
            Destroy();
        }

        PythonTreeKeyType GetKeyType() const
            {return key_type;}

        bool IsSignedInt() const
            {return signed_int;}

        ProtoTree::Endian GetKeyEndian() const
            {return key_endian;}

        bool GetUseSignBit() const
            {return use_sign_bit;}

        bool GetUseComplement2() const
            {return use_complement2;}

    private:
        PythonTreeKeyType key_type;
        bool              signed_int;
        ProtoTree::Endian key_endian;
        bool              use_sign_bit;
        bool              use_complement2;
};


// ---------------------------------------------------------------------------
// Key conversion
// ---------------------------------------------------------------------------

static bool Tree_EncodeString(const char* utf8,
                              Py_ssize_t utf8Len,
                              std::string& encoded)
{
    if (utf8Len < 0)
    {
        PyErr_SetString(PyExc_ValueError, "invalid UTF-8 string length");
        return false;
    }

    const size_t n = static_cast<size_t>(utf8Len);
    const size_t maxKeyBytes = static_cast<size_t>(UINT_MAX >> 3);

    if ((0 == maxKeyBytes) || (n > ((maxKeyBytes - 1) >> 1)))
    {
        PyErr_SetString(PyExc_OverflowError,
                        "SortedTree string key is too long");
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
        encoded[2 * i]     = '\\x01';
        encoded[2 * i + 1] = utf8[i];
    }

    encoded[2 * n] = '\\x00';
    return true;
}


static bool Tree_ConvertKey(const PythonSortedTree& tree,
                            PyObject*               pyKey,
                            TreeKeyBuffer&          key)
{
    switch (tree.GetKeyType())
    {
        case TREE_KEY_FLOAT:
        {
            if (!PyFloat_Check(pyKey))
            {
                PyErr_SetString(PyExc_TypeError,
                                "SortedTree(float) key must be a float");
                return false;
            }

            key.float_key = PyFloat_AsDouble(pyKey);
            if (PyErr_Occurred())
                return false;

            if (key.float_key != key.float_key)
            {
                PyErr_SetString(PyExc_ValueError,
                                "SortedTree float key may not be NaN");
                return false;
            }

            // ProtoSortedTree's floating-point ordering treats zero as one
            // value.  Normalize -0.0 to +0.0 so exact Find() is consistent.
            if (0.0 == key.float_key)
                key.float_key = 0.0;

            key.key_ptr = reinterpret_cast<const char*>(&key.float_key);
            key.keysize = static_cast<unsigned int>(sizeof(key.float_key) << 3);
            return true;
        }

        case TREE_KEY_INT:
        {
            if (!PyLong_Check(pyKey) || PyBool_Check(pyKey))
            {
                PyErr_SetString(PyExc_TypeError,
                                "SortedTree(int) key must be an int (not bool)");
                return false;
            }

            if (tree.IsSignedInt())
            {
                long long value = PyLong_AsLongLong(pyKey);
                if ((-1 == value) && PyErr_Occurred())
                    return false;

                key.signed_key = static_cast<int64_t>(value);
                key.key_ptr = reinterpret_cast<const char*>(&key.signed_key);
            }
            else
            {
                unsigned long long value = PyLong_AsUnsignedLongLong(pyKey);
                if ((static_cast<unsigned long long>(-1) == value) &&
                    PyErr_Occurred())
                {
                    return false;
                }

                key.unsigned_key = static_cast<uint64_t>(value);
                key.key_ptr = reinterpret_cast<const char*>(&key.unsigned_key);
            }

            key.keysize = static_cast<unsigned int>(sizeof(uint64_t) << 3);
            return true;
        }

        case TREE_KEY_STRING:
        {
            if (!PyUnicode_Check(pyKey))
            {
                PyErr_SetString(PyExc_TypeError,
                                "SortedTree(str) key must be a str");
                return false;
            }

            Py_ssize_t utf8Len = 0;
            const char* utf8 = PyUnicode_AsUTF8AndSize(pyKey, &utf8Len);
            if (NULL == utf8)
                return false;

            if (!Tree_EncodeString(utf8, utf8Len, key.string_key))
                return false;

            key.key_ptr = key.string_key.data();
            key.keysize = static_cast<unsigned int>(key.string_key.size() << 3);
            return true;
        }
    }

    PyErr_SetString(ProtoError, "invalid SortedTree key type");
    return false;
}


static bool Tree_ParseKeyType(PyObject* pyType, PythonTreeKeyType& keyType)
{
    if (pyType == reinterpret_cast<PyObject*>(&PyFloat_Type))
    {
        keyType = TREE_KEY_FLOAT;
        return true;
    }
    else if (pyType == reinterpret_cast<PyObject*>(&PyLong_Type))
    {
        keyType = TREE_KEY_INT;
        return true;
    }
    else if (pyType == reinterpret_cast<PyObject*>(&PyUnicode_Type))
    {
        keyType = TREE_KEY_STRING;
        return true;
    }

    PyErr_SetString(PyExc_TypeError,
                    "SortedTree key_type must be float, int, or str");
    return false;
}


// ---------------------------------------------------------------------------
// Item directly stored in ProtoSortedTree
// ---------------------------------------------------------------------------

class PythonSortedTreeNode : public ProtoSortedTree::Item
{
    public:
        PythonSortedTreeNode(PythonSortedTree& theTree,
                             PyObject*         pyObj,
                             PyObject*         pyKey)
          : tree(theTree),
            py_object(pyObj),
            py_key(pyKey)
        {
            Py_INCREF(py_object);
            Py_INCREF(py_key);
        }

        virtual ~PythonSortedTreeNode()
        {
            Py_DECREF(py_key);
            Py_DECREF(py_object);
        }

        PyObject* GetObject() const
            {return py_object;}

        PyObject* GetPythonKey() const
        {
            Py_INCREF(py_key);
            return py_key;
        }

        ProtoTree::Endian GetEndian() const
            {return tree.GetKeyEndian();}

        bool UseSignBit() const
            {return tree.GetUseSignBit();}

        bool UseComplement2() const
            {return tree.GetUseComplement2();}

    protected:
        PythonSortedTree& tree;

    private:
        PyObject* py_object;
        PyObject* py_key;
};


template <typename KEY_TYPE>
class NumericSortedTreeNode : public PythonSortedTreeNode
{
    public:
        NumericSortedTreeNode(PythonSortedTree& tree,
                              PyObject*         pyObj,
                              PyObject*         pyKey,
                              KEY_TYPE          value)
          : PythonSortedTreeNode(tree, pyObj, pyKey),
            item_key(value)
        {
        }

        virtual ~NumericSortedTreeNode() {}

        const char* GetKey() const
        {
            return reinterpret_cast<const char*>(&item_key);
        }

        unsigned int GetKeysize() const
        {
            return static_cast<unsigned int>(sizeof(KEY_TYPE) << 3);
        }

    private:
        KEY_TYPE item_key;
};


class StringSortedTreeNode : public PythonSortedTreeNode
{
    public:
        StringSortedTreeNode(PythonSortedTree&    tree,
                             PyObject*            pyObj,
                             PyObject*            pyKey,
                             const TreeKeyBuffer& key)
          : PythonSortedTreeNode(tree, pyObj, pyKey),
            item_key(key.string_key)
        {
        }

        virtual ~StringSortedTreeNode() {}

        const char* GetKey() const
            {return item_key.data();}

        unsigned int GetKeysize() const
        {
            return static_cast<unsigned int>(item_key.size() << 3);
        }

    private:
        std::string item_key;
};


static PythonSortedTreeNode* Tree_CreateNode(
    PythonSortedTree&    tree,
    PyObject*            pyObj,
    PyObject*            pyKey,
    const TreeKeyBuffer& key)
{
    PythonSortedTreeNode* node = NULL;

    try
    {
        switch (tree.GetKeyType())
        {
            case TREE_KEY_FLOAT:
                node = new (std::nothrow)
                    NumericSortedTreeNode<double>(
                        tree, pyObj, pyKey, key.float_key);
                break;

            case TREE_KEY_INT:
                if (tree.IsSignedInt())
                {
                    node = new (std::nothrow)
                        NumericSortedTreeNode<int64_t>(
                            tree, pyObj, pyKey, key.signed_key);
                }
                else
                {
                    node = new (std::nothrow)
                        NumericSortedTreeNode<uint64_t>(
                            tree, pyObj, pyKey, key.unsigned_key);
                }
                break;

            case TREE_KEY_STRING:
                node = new (std::nothrow)
                    StringSortedTreeNode(
                        tree, pyObj, pyKey, key);
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
}


// ---------------------------------------------------------------------------
// Python-object pointer -> PythonSortedTreeNode lookup index
//
// This preserves efficient remove(obj) / key(obj) without ProtoQueue's
// per-item queue-membership Container machinery.
// ---------------------------------------------------------------------------

class SortedTreeObjectItem : public ProtoTree::Item
{
    public:
        SortedTreeObjectItem(PyObject* pyObj, PythonSortedTreeNode& treeNode)
          : py_object(pyObj), tree_node(treeNode) {}

        virtual ~SortedTreeObjectItem() {}

        PythonSortedTreeNode& GetNode()
            {return tree_node;}

        const PythonSortedTreeNode& GetNode() const
            {return tree_node;}

        const char* GetKey() const
            {return reinterpret_cast<const char*>(&py_object);}

        unsigned int GetKeysize() const
            {return static_cast<unsigned int>(sizeof(PyObject*) << 3);}

    private:
        // The PythonSortedTreeNode owns the strong Python reference.
        PyObject*              py_object;
        PythonSortedTreeNode&  tree_node;
};


class SortedTreeObjectIndex : public ProtoTreeTemplate<SortedTreeObjectItem>
{
    public:
        SortedTreeObjectItem* FindItem(const PyObject* pyObj)
        {
            return Find(reinterpret_cast<const char*>(&pyObj),
                        static_cast<unsigned int>(sizeof(PyObject*) << 3));
        }

        const SortedTreeObjectItem* FindItem(const PyObject* pyObj) const
        {
            return Find(reinterpret_cast<const char*>(&pyObj),
                        static_cast<unsigned int>(sizeof(PyObject*) << 3));
        }
};


// ---------------------------------------------------------------------------
// Key conversion
// ---------------------------------------------------------------------------
// Python extension objects
// ---------------------------------------------------------------------------

extern "C" {

    typedef struct {
        PyObject_HEAD
        PythonSortedTree*      thisptr;
        SortedTreeObjectIndex* object_index;
        Py_ssize_t             item_count;
    } SortedTree;

    typedef struct {
        PyObject_HEAD
        ProtoSortedTree::Iterator* thisptr;
        PyObject*                  py_tree;
        bool                       reverse;
    } SortedTreeIterator;

    // -----------------------------------------------------------------------
    // SortedTree
    // -----------------------------------------------------------------------

    static void SortedTree_dealloc(SortedTree* self)
    {
        if (NULL != self->object_index)
        {
            self->object_index->Destroy();
            delete self->object_index;
            self->object_index = NULL;
        }

        if (NULL != self->thisptr)
        {
            self->thisptr->Destroy();
            delete self->thisptr;
            self->thisptr = NULL;
        }

        self->item_count = 0;
        Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
    }


    static PyObject* SortedTree_new(PyTypeObject* type,
                                    PyObject* args,
                                    PyObject* kwargs)
    {
        SortedTree* self =
            reinterpret_cast<SortedTree*>(type->tp_alloc(type, 0));

        if (NULL == self)
            return NULL;

        self->thisptr = NULL;
        self->object_index = NULL;
        self->item_count = 0;

        return reinterpret_cast<PyObject*>(self);
    }


    static int SortedTree_init(SortedTree* self,
                               PyObject* args,
                               PyObject* kwargs)
    {
        PyObject* pyKeyType =
            reinterpret_cast<PyObject*>(&PyFloat_Type);

        int signedInt = 1;
        int uniqueItems = 0;

        static char* kwlist[] = {
            const_cast<char*>("key_type"),
            const_cast<char*>("signed"),
            const_cast<char*>("unique"),
            NULL
        };

        bool signedProvided = (PyTuple_Size(args) >= 2);
        if ((NULL != kwargs) &&
            (NULL != PyDict_GetItemString(kwargs, "signed")))
        {
            signedProvided = true;
        }

        if (!PyArg_ParseTupleAndKeywords(
                args, kwargs, "|Opp:SortedTree",
                kwlist,
                &pyKeyType,
                &signedInt,
                &uniqueItems))
        {
            return -1;
        }

        PythonTreeKeyType keyType;
        if (!Tree_ParseKeyType(pyKeyType, keyType))
            return -1;

        if ((TREE_KEY_INT != keyType) && signedProvided)
        {
            PyErr_SetString(PyExc_TypeError,
                            "'signed' is only valid for SortedTree(int)");
            return -1;
        }

        // Handle explicit reinitialization safely.
        if (NULL != self->object_index)
        {
            self->object_index->Destroy();
            delete self->object_index;
            self->object_index = NULL;
        }

        if (NULL != self->thisptr)
        {
            self->thisptr->Destroy();
            delete self->thisptr;
            self->thisptr = NULL;
        }

        self->item_count = 0;

        self->thisptr = new (std::nothrow)
            PythonSortedTree(
                keyType,
                0 != signedInt,
                0 != uniqueItems);

        if (NULL == self->thisptr)
        {
            PyErr_NoMemory();
            return -1;
        }

        self->object_index = new (std::nothrow)
            SortedTreeObjectIndex();

        if (NULL == self->object_index)
        {
            delete self->thisptr;
            self->thisptr = NULL;
            PyErr_NoMemory();
            return -1;
        }

        return 0;
    }


    // SortedTree.insert(object, key)
    static PyObject* SortedTree_insert(
        SortedTree*     self,
        PyObject* const* args,
        Py_ssize_t       nargs)
    {
        if (2 != nargs)
        {
            PyErr_Format(
                PyExc_TypeError,
                "insert() takes exactly 2 arguments (%zd given)",
                nargs);
            return NULL;
        }

        PyObject* pyObj = args[0];
        PyObject* pyKey = args[1];

        TreeKeyBuffer key;
        if (!Tree_ConvertKey(*self->thisptr, pyKey, key))
            return NULL;

        PythonSortedTreeNode* node =
            Tree_CreateNode(
                *self->thisptr, pyObj, pyKey, key);

        if (NULL == node)
            return NULL;

        if (!self->thisptr->Insert(*node))
        {
            delete node;
            PyErr_SetString(
                ProtoError,
                "SortedTree.insert() key rejected by ProtoSortedTree");
            return NULL;
        }

        SortedTreeObjectItem* indexItem = new (std::nothrow)
            SortedTreeObjectItem(pyObj, *node);

        if (NULL == indexItem)
        {
            self->thisptr->Remove(*node);
            delete node;
            PyErr_NoMemory();
            return NULL;
        }

        if (!self->object_index->Insert(*indexItem))
        {
            // The object pointer is already indexed.  Roll back the tree
            // insertion so the same Python object cannot appear twice in
            // this SortedTree.
            self->thisptr->Remove(*node);
            delete node;
            delete indexItem;
            PyErr_SetString(
                ProtoError,
                "SortedTree.insert() object is already in tree");
            return NULL;
        }

        ++self->item_count;
        Py_RETURN_NONE;
    }


    // SortedTree.remove(object)
    static PyObject* SortedTree_remove(
        SortedTree* self,
        PyObject*   pyObj)
    {
        SortedTreeObjectItem* indexItem =
            self->object_index->FindItem(pyObj);

        if (NULL == indexItem)
        {
            PyErr_SetString(
                ProtoError,
                "SortedTree.remove() object is not in tree");
            return NULL;
        }

        PythonSortedTreeNode& node = indexItem->GetNode();

        self->object_index->Remove(*indexItem);
        delete indexItem;

        self->thisptr->Remove(node);
        delete &node;

        --self->item_count;
        Py_RETURN_NONE;
    }


    // SortedTree.find(key) -> one exact-key object or None
    static PyObject* SortedTree_find(
        SortedTree* self,
        PyObject*   pyKey)
    {
        TreeKeyBuffer key;
        if (!Tree_ConvertKey(*self->thisptr, pyKey, key))
            return NULL;

        ProtoSortedTree::Item* item =
            self->thisptr->Find(key.key_ptr, key.keysize);

        if (NULL == item)
            Py_RETURN_NONE;

        PyObject* obj =
            static_cast<PythonSortedTreeNode*>(item)->GetObject();

        Py_INCREF(obj);
        return obj;
    }


    static PyObject* SortedTree_head(SortedTree* self, PyObject* args)
    {
        ProtoSortedTree::Item* item = self->thisptr->GetHead();

        if (NULL == item)
            Py_RETURN_NONE;

        PyObject* obj =
            static_cast<PythonSortedTreeNode*>(item)->GetObject();

        Py_INCREF(obj);
        return obj;
    }


    static PyObject* SortedTree_tail(SortedTree* self, PyObject* args)
    {
        ProtoSortedTree::Item* item = self->thisptr->GetTail();

        if (NULL == item)
            Py_RETURN_NONE;

        PyObject* obj =
            static_cast<PythonSortedTreeNode*>(item)->GetObject();

        Py_INCREF(obj);
        return obj;
    }


    static PyObject* SortedTree_key(
        SortedTree* self,
        PyObject*   pyObj)
    {
        SortedTreeObjectItem* indexItem =
            self->object_index->FindItem(pyObj);

        if (NULL == indexItem)
        {
            PyErr_SetString(
                ProtoError,
                "SortedTree.key() object is not in tree");
            return NULL;
        }

        return indexItem->GetNode().GetPythonKey();
    }


    static PyObject* SortedTree_size(SortedTree* self, PyObject* args)
    {
        return PyLong_FromSsize_t(self->item_count);
    }


    static PyObject* SortedTree_is_empty(SortedTree* self, PyObject* args)
    {
        if (self->thisptr->IsEmpty())
            Py_RETURN_TRUE;

        Py_RETURN_FALSE;
    }


    static PyObject* SortedTree_clear(SortedTree* self, PyObject* args)
    {
        self->object_index->Destroy();
        self->thisptr->Destroy();
        self->item_count = 0;

        Py_RETURN_NONE;
    }


    static PyObject* SortedTree_iterate(SortedTree* self, PyObject* args);


    static PyMethodDef SortedTree_methods[] =
    {
        {"insert",
            (PyCFunction)(void(*)(void))SortedTree_insert,
            METH_FASTCALL,
            "insert(object, key) -> None."},

        {"remove", (PyCFunction)SortedTree_remove, METH_O,
            "remove(object) -> None."},

        {"find", (PyCFunction)SortedTree_find, METH_O,
            "find(key) -> object or None. Return an exact-key match."},

        {"head", (PyCFunction)SortedTree_head, METH_NOARGS,
            "head() -> object or None. Return minimum-key object."},

        {"tail", (PyCFunction)SortedTree_tail, METH_NOARGS,
            "tail() -> object or None. Return maximum-key object."},

        {"key", (PyCFunction)SortedTree_key, METH_O,
            "key(object) -> key. Return object's original Python key."},

        {"size", (PyCFunction)SortedTree_size, METH_NOARGS,
            "size() -> int. Return number of objects in tree."},

        {"is_empty", (PyCFunction)SortedTree_is_empty, METH_NOARGS,
            "is_empty() -> bool."},

        {"clear", (PyCFunction)SortedTree_clear, METH_NOARGS,
            "clear() -> None. Remove all objects."},

        {"iterate", (PyCFunction)SortedTree_iterate, METH_VARARGS,
            "iterate([start [, reverse]]) -> ordered iterator."},
        {NULL}
    };


    static PyTypeObject SortedTreeType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "protokit.SortedTree",                 /* tp_name */
        sizeof(SortedTree),                    /* tp_basicsize */
        0,                                     /* tp_itemsize */
        (destructor)SortedTree_dealloc,        /* tp_dealloc */
        0,                                     /* tp_print / vectorcall */
        0,                                     /* tp_getattr */
        0,                                     /* tp_setattr */
        0,                                     /* tp_compare / reserved */
        0,                                     /* tp_repr */
        0,                                     /* tp_as_number */
        0,                                     /* tp_as_sequence */
        0,                                     /* tp_as_mapping */
        0,                                     /* tp_hash */
        0,                                     /* tp_call */
        0,                                     /* tp_str */
        0,                                     /* tp_getattro */
        0,                                     /* tp_setattro */
        0,                                     /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
        "Direct ProtoSortedTree wrapper",      /* tp_doc */
        0,                                     /* tp_traverse */
        0,                                     /* tp_clear */
        0,                                     /* tp_richcompare */
        0,                                     /* tp_weaklistoffset */
        0,                                     /* tp_iter */
        0,                                     /* tp_iternext */
        SortedTree_methods,                    /* tp_methods */
        0,                                     /* tp_members */
        0,                                     /* tp_getset */
        0,                                     /* tp_base */
        0,                                     /* tp_dict */
        0,                                     /* tp_descr_get */
        0,                                     /* tp_descr_set */
        0,                                     /* tp_dictoffset */
        (initproc)SortedTree_init,             /* tp_init */
        0,                                     /* tp_alloc */
        SortedTree_new,                        /* tp_new */
    };


    // -----------------------------------------------------------------------
    // SortedTree.Iterator
    // -----------------------------------------------------------------------

    static void SortedTreeIterator_dealloc(SortedTreeIterator* self)
    {
        delete self->thisptr;
        self->thisptr = NULL;

        if (NULL != self->py_tree)
        {
            Py_DECREF(self->py_tree);
            self->py_tree = NULL;
        }

        Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
    }


    static PyObject* SortedTreeIterator_new(PyTypeObject* type,
                                            PyObject* args,
                                            PyObject* kwargs)
    {
        SortedTreeIterator* self =
            reinterpret_cast<SortedTreeIterator*>(
                type->tp_alloc(type, 0));

        if (NULL == self)
            return NULL;

        self->thisptr = NULL;
        self->py_tree = NULL;
        self->reverse = false;

        return reinterpret_cast<PyObject*>(self);
    }


    static int SortedTreeIterator_init(SortedTreeIterator* self,
                                       PyObject* args,
                                       PyObject* kwargs)
    {
        PyObject* pyTree = NULL;
        PyObject* pyStart = Py_None;
        int reverse = 0;

        static char* kwlist[] = {
            const_cast<char*>("tree"),
            const_cast<char*>("start"),
            const_cast<char*>("reverse"),
            NULL
        };

        if (!PyArg_ParseTupleAndKeywords(
                args, kwargs, "O!|Op",
                kwlist,
                &SortedTreeType,
                &pyTree,
                &pyStart,
                &reverse))
        {
            return -1;
        }

        SortedTree* tree =
            reinterpret_cast<SortedTree*>(pyTree);

        TreeKeyBuffer startKey;
        const char* keyPtr = NULL;
        unsigned int keysize = 0;

        if (Py_None != pyStart)
        {
            if (!Tree_ConvertKey(
                    *tree->thisptr, pyStart, startKey))
            {
                return -1;
            }

            keyPtr = startKey.key_ptr;
            keysize = startKey.keysize;
        }

        delete self->thisptr;
        self->thisptr = NULL;

        if (NULL != self->py_tree)
        {
            Py_DECREF(self->py_tree);
            self->py_tree = NULL;
        }

        self->reverse = (0 != reverse);

        self->thisptr = new (std::nothrow)
            ProtoSortedTree::Iterator(
                *tree->thisptr,
                self->reverse,
                keyPtr,
                keysize);

        if (NULL == self->thisptr)
        {
            PyErr_NoMemory();
            return -1;
        }

        Py_INCREF(pyTree);
        self->py_tree = pyTree;

        return 0;
    }


    static PyObject* SortedTreeIterator_next(PyObject* self)
    {
        SortedTreeIterator* iterator =
            reinterpret_cast<SortedTreeIterator*>(self);

        ProtoSortedTree::Item* item =
            iterator->reverse ?
                iterator->thisptr->GetPrevItem() :
                iterator->thisptr->GetNextItem();

        if (NULL == item)
        {
            PyErr_SetNone(PyExc_StopIteration);
            return NULL;
        }

        PyObject* obj =
            static_cast<PythonSortedTreeNode*>(item)->GetObject();

        Py_INCREF(obj);
        return obj;
    }


    static PyObject* SortedTreeIterator_iter(PyObject* self)
    {
        Py_INCREF(self);
        return self;
    }


    static PyMethodDef SortedTreeIterator_methods[] =
    {
        {NULL}
    };


    static PyTypeObject SortedTreeIteratorType = {
        PyVarObject_HEAD_INIT(NULL, 0)
        "protokit.SortedTree.Iterator",         /* tp_name */
        sizeof(SortedTreeIterator),             /* tp_basicsize */
        0,                                      /* tp_itemsize */
        (destructor)SortedTreeIterator_dealloc, /* tp_dealloc */
        0,                                      /* tp_print / vectorcall */
        0,                                      /* tp_getattr */
        0,                                      /* tp_setattr */
        0,                                      /* tp_compare / reserved */
        0,                                      /* tp_repr */
        0,                                      /* tp_as_number */
        0,                                      /* tp_as_sequence */
        0,                                      /* tp_as_mapping */
        0,                                      /* tp_hash */
        0,                                      /* tp_call */
        0,                                      /* tp_str */
        0,                                      /* tp_getattro */
        0,                                      /* tp_setattro */
        0,                                      /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
        "ProtoSortedTree::Iterator wrapper",    /* tp_doc */
        0,                                      /* tp_traverse */
        0,                                      /* tp_clear */
        0,                                      /* tp_richcompare */
        0,                                      /* tp_weaklistoffset */
        SortedTreeIterator_iter,                /* tp_iter */
        SortedTreeIterator_next,                /* tp_iternext */
        SortedTreeIterator_methods,             /* tp_methods */
        0,                                      /* tp_members */
        0,                                      /* tp_getset */
        0,                                      /* tp_base */
        0,                                      /* tp_dict */
        0,                                      /* tp_descr_get */
        0,                                      /* tp_descr_set */
        0,                                      /* tp_dictoffset */
        (initproc)SortedTreeIterator_init,      /* tp_init */
        0,                                      /* tp_alloc */
        SortedTreeIterator_new,                 /* tp_new */
    };


    static PyObject* SortedTree_iterate(SortedTree* self, PyObject* args)
    {
        PyObject* pyStart = Py_None;
        int reverse = 0;

        if (!PyArg_ParseTuple(
                args, "|Op:iterate", &pyStart, &reverse))
        {
            return NULL;
        }

        TreeKeyBuffer startKey;
        const char* keyPtr = NULL;
        unsigned int keysize = 0;

        if (Py_None != pyStart)
        {
            if (!Tree_ConvertKey(
                    *self->thisptr, pyStart, startKey))
            {
                return NULL;
            }

            keyPtr = startKey.key_ptr;
            keysize = startKey.keysize;
        }

        SortedTreeIterator* iterator =
            PyObject_New(
                SortedTreeIterator,
                &SortedTreeIteratorType);

        if (NULL == iterator)
            return NULL;

        iterator->thisptr = NULL;
        iterator->py_tree = NULL;
        iterator->reverse = (0 != reverse);

        iterator->thisptr = new (std::nothrow)
            ProtoSortedTree::Iterator(
                *self->thisptr,
                iterator->reverse,
                keyPtr,
                keysize);

        if (NULL == iterator->thisptr)
        {
            Py_TYPE(iterator)->tp_free(
                reinterpret_cast<PyObject*>(iterator));
            PyErr_NoMemory();
            return NULL;
        }

        iterator->py_tree =
            reinterpret_cast<PyObject*>(self);

        Py_INCREF(reinterpret_cast<PyObject*>(self));

        return reinterpret_cast<PyObject*>(iterator);
    }


}  // end extern "C"
