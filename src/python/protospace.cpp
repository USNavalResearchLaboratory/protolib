
// Python binding code for ProtoSpace class

#define PY_SSIZE_T_CLEAN
#include "protopy.h"
#include "protoSpace.h"

// This subclass of ProtoSpace::Node is used store reference 
// to a Python object (and its ordinates within the space)
class SpaceNode : public ProtoSpace::Node
{
    public:
        SpaceNode(PyObject* pNode)
          : py_node(pNode), num_dimensions(0), ordinate_list(NULL) {}
        virtual ~SpaceNode()
        {
            if (NULL != ordinate_list) delete[] ordinate_list;
            ordinate_list = NULL;
            num_dimensions = 0;
        }
        
        PyObject* GetObject() {return py_node;}
        
        bool Init(unsigned int numDimensions)
        {   
            if (NULL != ordinate_list) delete[] ordinate_list;
            if (NULL == (ordinate_list = new double[numDimensions]))
            {
                num_dimensions = 0;
                return false;
            }
            num_dimensions = numDimensions;
            return true;
        }
        
        void SetOrdinate(unsigned int dim, double value)
            {ordinate_list[dim] = value;}

        unsigned int GetDimensions() const
            {return num_dimensions;}
        double GetOrdinate(unsigned int dim) const
            {return dim < num_dimensions ? ordinate_list[dim] : 0.0;}
    
    private:
        PyObject*    py_node;
        unsigned int num_dimensions;
        double*      ordinate_list;
        
};  // end class SpaceNode

// Use ProtoTree to main map of Python object to SpaceNode entries
// (enables removal of objects/nodes from space

class SpaceItem : public ProtoTree::Item
{
    public:
        SpaceItem(PyObject* pyObj, SpaceNode& spaceNode) 
          : py_object(pyObj), space_node(spaceNode)
          {
             Py_INCREF(pyObj);
          }
        virtual ~SpaceItem() {Py_DECREF(py_object);}
        
        SpaceNode& GetNode()
            {return space_node;}
        
        const char* GetKey() const
            {return (const char*)&py_object;}
        unsigned int GetKeysize() const
            {return (unsigned int)sizeof(PyObject*) << 3;}
            
    private:
        PyObject*   py_object;
        SpaceNode&  space_node;
        
};  // end class SpaceItem

class SpaceItemTree : public ProtoTreeTemplate<SpaceItem>
{
    public:
        SpaceItem* FindItem(const PyObject* pyObj)
            {return Find((const char*)&pyObj, sizeof(PyObject*) << 3);}
};  // end class SpaceItemTree

extern "C" {
    // protokit.Space and protokoit.SpaceIterator declarations
    typedef struct {
        PyObject_HEAD
        ProtoSpace*     thisptr;
        SpaceItemTree   item_tree;
    } Space;
    
    typedef struct {
        PyObject_HEAD
        ProtoSpace::Iterator* thisptr;
        PyObject*             py_space;
    } SpaceIterator;

    static void Space_dealloc(Space *self) 
    {
        self->thisptr->Destroy();  // deletes all SpaceNodes held
        self->item_tree.Destroy();
        Py_TYPE(self)->tp_free((PyObject*)self);
    }  // end Space_dealloc()

    static PyObject* Space_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) 
    {
        Space *self = (Space*)type->tp_alloc(type, 0);
        if (self == NULL)
            return NULL;
        self->thisptr = NULL;
        return (PyObject*)self;
    }  // end Space_new()
    
    static int Space_init(Space *self, PyObject *args, PyObject *kwargs) 
    {
        if (NULL == (self->thisptr = new ProtoSpace()))
        {
            PyErr_SetString(ProtoError, "new ProtoSpace error");
            return -1;
        }
        return 0;
    }  // end Space_init()

    // Space_insert args (object, ordinate tuple
    static PyObject* Space_insert(Space* self, PyObject* args) 
    {
        PyObject* pNode;
        PyObject* pList;

        // TBD - support providing ordinates as tuple, too???
        if (!PyArg_ParseTuple(args, "O|O!", &pNode, &PyList_Type, &pList)) {
            PyErr_SetString(ProtoError, "Space ordinates must be provided as a list.");
            return NULL;
        }
        
        // Create SpaceNode that references Python object being inserted
        SpaceNode* spaceNode = new SpaceNode(pNode);
        if (NULL == spaceNode)
        {
            // TBD - do I need to dereference pNode and plist here?
            PyErr_SetString(ProtoError, "new SpaceNode error");
            return NULL;
        }
        // Init spaceNode with number of dimensions inferrred from list length
        unsigned int numDimensions = PyList_Size(pList);    
        if (!spaceNode->Init(numDimensions))
        {
            // TBD - do I need to dereference pNode and plist here?
            PyErr_SetString(ProtoError, "SpaceNode.Init() error");
            delete spaceNode;
            return NULL;
        }
        
        // Iterate through list of provided ordinates
        PyObject* pItem;
        for (unsigned int i=0; i<numDimensions; i++) 
        {
            pItem = PyList_GetItem(pList, i);
            double value;
            if (PyLong_Check(pItem))
            {
                value = (double)PyLong_AsLong(pItem);
            }
            else if (PyFloat_Check(pItem))
            {
                value = PyFloat_AsDouble(pItem);
            }
            else
            {
                PyErr_SetString(ProtoError, "Space ordinates must be integers or doubles");
                return NULL;
            }
            spaceNode->SetOrdinate(i, value);
        }
        if (!self->thisptr->InsertNode(*spaceNode))
        {
            PyErr_SetString(ProtoError, "ProtoSpace::Insert() error (dimensionality mismatch?)");
            return NULL;
        }
        // Create SpaceItem entry for PyObject -> SpaceNode lookup
        SpaceItem* spaceItem = new SpaceItem(pNode, *spaceNode);
        if (NULL == spaceItem)
        {
            self->thisptr->RemoveNode(*spaceNode);
            delete spaceNode;
            PyErr_SetString(ProtoError, "new SpaceItem error");
            return NULL;
        }
        if (!self->item_tree.Insert(*spaceItem))
        {
            PyErr_SetString(ProtoError, "SpaceItem insertion error");
            return NULL;
        }
        Py_RETURN_NONE;
    }  // end Space_insert()

    static PyObject* Space_remove(Space* self, PyObject* args) 
    {
        PyObject* pNode;
        if (!PyArg_ParseTuple(args, "O", &pNode)) 
        {
            PyErr_SetString(ProtoError, "invalid argument");
            return NULL;
        }
        // Find SpaceItem indexed by PyObject* pointer to
        // determine which SpaceNode to remove from space
        SpaceItem* spaceItem = self->item_tree.FindItem(pNode);
        if (NULL == spaceItem)
        {
            //PyObject* nameObj = PyObject_GetAttrString(pNode, "name");
            //const char* name = PyUnicode_AsUTF8(nameObj);
            //TRACE("Space_remove() Invalid object %p named %s\n", pNode, name);
            PyErr_Format(ProtoError, "Space_remove() invalid space item");
            return NULL;
        }
        // Remove/delete the SpaceNode
        self->thisptr->RemoveNode(spaceItem->GetNode());
        delete &spaceItem->GetNode();
        // Remove/delete the SpaceItem (TBD - should we maintain an item_pool?)
        self->item_tree.Remove(*spaceItem);
        delete spaceItem;
        Py_RETURN_NONE;
    }  // end Space_remove()
    
    static PyObject* Space_iterate(Space* self, PyObject* args);

    static PyMethodDef Space_methods[] = 
    {
        {"insert", (PyCFunction)Space_insert, METH_VARARGS,
            "Insert Python object into space with given ordinate list."},
        {"remove", (PyCFunction)Space_remove, METH_VARARGS,
            "Remove Python object from space"},
        {"iterate", (PyCFunction)Space_iterate, METH_VARARGS,
            "Remove Python object from space"},
        {NULL}
    };
    
    static PyTypeObject SpaceType = {
        PyVarObject_HEAD_INIT(NULL,0) /*ob_size*/
        "protokit.Space",             /*tp_name*/
        sizeof(Space),                /*tp_basicsize*/
        0,                            /*tp_itemsize*/
        (destructor)Space_dealloc,    /*tp_dealloc*/
        0,                            /*tp_print*/
        0,                            /*tp_getattr*/
        0,                            /*tp_setattr*/
        0,                            /*tp_compare*/
        0,                            /*tp_repr*/
        0,                            /*tp_as_number*/
        0,                            /*tp_as_sequence*/
        0,                            /*tp_as_mapping*/
        0,                            /*tp_hash */
        0,                            /*tp_call*/
        0,                            /*tp_str*/
        0,                            /*tp_getattro*/
        0,                            /*tp_setattro*/
        0,                            /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
        "ProtoSpace wrapper",         /* tp_doc */
        0,                            /* tp_traverse */
        0,                            /* tp_clear */
        0,                            /* tp_richcompare */
        0,                            /* tp_weaklistoffset */
        0,                            /* tp_iter */
        0,                            /* tp_iternext */
        Space_methods,                /* tp_methods */
        0,                            /* tp_members */
        0,                            /* tp_getset */
        0,                            /* tp_base */
        0,                            /* tp_dict */
        0,                            /* tp_descr_get */
        0,                            /* tp_descr_set */
        0,                            /* tp_dictoffset */
        (initproc)Space_init,         /* tp_init */
        0,                            /* tp_alloc */
        Space_new,                    /* tp_new */
    };
    
    
    // This is the protokit.Space.Iterator class  implementation

    static void SpaceIterator_dealloc(SpaceIterator *self) 
    {
        if (NULL != self->py_space)
        {
            Py_DECREF(self->py_space);
            self->py_space = NULL;
        }
        self->thisptr->Destroy();
        // Drop reference to Space on destruction
        delete self->thisptr;
        Py_TYPE(self)->tp_free((PyObject*)self);
    }  // end SpaceIterator_dealloc()

    static PyObject* SpaceIterator_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) 
    {
        SpaceIterator *self = (SpaceIterator*)type->tp_alloc(type, 0);
        if (self == NULL)
            return NULL;
        self->thisptr = NULL;
        self->py_space = NULL;
        return (PyObject*)self;
    }  // end SpaceIterator_new()

    static int SpaceIterator_init(SpaceIterator *self, PyObject *args) 
    {
        PyObject* pSpace;
        PyObject* pList;   // for iterator origin ordinate list
        if (!PyArg_ParseTuple(args, "O!|O!", &SpaceType, &pSpace, &PyList_Type, &pList)) 
        {
            PyErr_SetString(ProtoError, "invalid argument");
            return -1;
        }
        if (NULL == (self->thisptr = new ProtoSpace::Iterator(*(((Space*)pSpace)->thisptr))))
        {
            PyErr_SetString(ProtoError, "new ProtoSpace::Iterator error");
            return -1;
        }
        
        // Init space with number of dimensions inferrred from list length
        unsigned int numDimensions = PyList_Size(pList);    
        
        double* originOrdinates = (double*)malloc(numDimensions);
        if (NULL == originOrdinates)
        {
            PyErr_SetString(ProtoError, "new origin ordinates error");
            return -1;
        }
          
        // Iterate through list of provided ordinates
        PyObject* pItem;
        for (unsigned int i=0; i<numDimensions; i++) 
        {
            pItem = PyList_GetItem(pList, i);
            if (PyLong_Check(pItem))
            {
                originOrdinates[i] = (double)PyLong_AsLong(pItem);
            }
            else if (PyFloat_Check(pItem))
            {
                originOrdinates[i] = PyFloat_AsDouble(pItem);
            }
            else
            {
                free(originOrdinates);
                PyErr_SetString(ProtoError, "Space ordinates must be integers or doubles");
                return -1;
            }
        }
        if (!self->thisptr->Init(originOrdinates))
        {
            free(originOrdinates);
            PyErr_SetString(ProtoError, "ProtoSpace::Iterator::Init() error");
            return -1;
        }
        free(originOrdinates);
        // SpaceIterator maintains reference to Space until destroyed
        Py_INCREF(pSpace);
        self->py_space = pSpace;
        return 0;
    }  // end SpaceIterator_init()
    
    static PyObject* SpaceIterator_next(PyObject* self) 
    {
        SpaceNode* next = (SpaceNode*)(   ((SpaceIterator*)self)->thisptr->GetNextNode());
        if (NULL == next)
        {
            PyErr_SetNone(PyExc_StopIteration);
            return NULL;
        }
        PyObject* obj = next->GetObject();
        Py_INCREF(obj);
        return obj;
    }  // end SpaceIterator_next()
    
    static PyObject* SpaceIterator_iter(PyObject* self) 
    {
        Py_INCREF(self);
        return self;
    }  // end SpaceIterator_iter()
    
    
    static PyMethodDef SpaceIterator_methods[] = 
    {
       {NULL}
    };
    
    static PyTypeObject SpaceIteratorType = {
        PyVarObject_HEAD_INIT(NULL,0)       /*ob_size*/
        "protokit.Space.Iterator",           /*tp_name*/
        sizeof(SpaceIterator),              /*tp_basicsize*/
        0,                                  /*tp_itemsize*/
        (destructor)SpaceIterator_dealloc,  /*tp_dealloc*/
        0,                                  /*tp_print*/
        0,                                  /*tp_getattr*/
        0,                                  /*tp_setattr*/
        0,                                  /*tp_compare*/
        0,                                  /*tp_repr*/
        0,                                  /*tp_as_number*/
        0,                                  /*tp_as_sequence*/
        0,                                  /*tp_as_mapping*/
        0,                                  /*tp_hash */
        0,                                  /*tp_call*/
        0,                                  /*tp_str*/
        0,                                  /*tp_getattro*/
        0,                                  /*tp_setattro*/
        0,                                  /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
        "ProtoSpace::Iterator wrapper",     /* tp_doc */
        0,                                  /* tp_traverse */
        0,                                  /* tp_clear */
        0,                                  /* tp_richcompare */
        0,                                  /* tp_weaklistoffset */
        SpaceIterator_iter,                 /* tp_iter */
        SpaceIterator_next,                 /* tp_iternext */
        SpaceIterator_methods,              /* tp_methods */
        0,                                  /* tp_members */
        0,                                  /* tp_getset */
        0,                                  /* tp_base */
        0,                                  /* tp_dict */
        0,                                  /* tp_descr_get */
        0,                                  /* tp_descr_set */
        0,                                  /* tp_dictoffset */
        (initproc)SpaceIterator_init,       /* tp_init */
        0,                                  /* tp_alloc */
        SpaceIterator_new,                  /* tp_new */
    };
    
    static PyObject* Space_iterate(Space* self, PyObject* args)
    {
        // Allocate a new iterator
        SpaceIterator* iterator = PyObject_New(SpaceIterator, &SpaceIteratorType);
        if (iterator == NULL)
            return NULL;
            
        // Initialize it (using optional origin ordinates list, if provided)
        iterator->thisptr = new ProtoSpace::Iterator(*self->thisptr);
        if (NULL == iterator->thisptr)
        {
            Py_TYPE(iterator)->tp_free((PyObject*)iterator);
            PyErr_SetString(ProtoError, "new ProtoSpace::Iterator error");
            return NULL;
        }
        // SpaceIterator maintains reference to Space until destroyed
        iterator->py_space = (PyObject*)self;
        Py_INCREF((PyObject*)self);
        
        // Parse origin ordinate list, if provided
        PyObject* pList = NULL;
        if (!PyArg_ParseTuple(args, "|O!", &PyList_Type, &pList)) 
        {
            delete iterator->thisptr;
            Py_TYPE(iterator)->tp_free((PyObject*)iterator);
            PyErr_SetString(ProtoError, "Space.Iterator origin ordinates must be provided as a list.");
            return NULL;
        }
        unsigned int numDimensions = (NULL != pList) ? PyList_Size(pList) : 0; 
        double* originOrdinates = numDimensions ? new double[numDimensions] : NULL;
        if ((0 != numDimensions) && (NULL == originOrdinates))
        {
            delete iterator->thisptr;
            Py_TYPE(iterator)->tp_free((PyObject*)iterator);
            PyErr_SetString(ProtoError, "new origin ordinates error");
            return NULL;
        }
          
        // Iterate through list of provided ordinates
        PyObject* pItem;
        for (unsigned int i=0; i<numDimensions; i++) 
        {
            pItem = PyList_GetItem(pList, i);
            if (PyLong_Check(pItem))
            {
                originOrdinates[i] = (double)PyLong_AsLong(pItem);
            }
            else if (PyFloat_Check(pItem))
            {
                originOrdinates[i] = PyFloat_AsDouble(pItem);
            }
            else
            {
                delete[] originOrdinates;
                delete iterator->thisptr;
                Py_TYPE(iterator)->tp_free((PyObject*)iterator);
                PyErr_SetString(ProtoError, "Space ordinates must be integers or doubles");
                return NULL;
            }
        }
        if (!iterator->thisptr->Init(originOrdinates))
        {
            delete[] originOrdinates;
            delete iterator->thisptr;
            Py_TYPE(iterator)->tp_free((PyObject*)iterator);
            PyErr_SetString(ProtoError, "ProtoSpace::Iterator::Init() error");
            return NULL;
        }
        if (NULL != originOrdinates) delete[] originOrdinates;
        return (PyObject*)iterator;
    }  // end Space_iterate()
    
}  // end extern "C"
