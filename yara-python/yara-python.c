/*
Copyright (c) 2007. Victor M. Alvarez [plusvic@gmail.com].

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/* headers */

#include <Python.h>
#include "structmember.h"

#if PY_VERSION_HEX >= 0x02060000
#include "bytesobject.h"
#elif PY_VERSION_HEX < 0x02060000
#define PyBytes_AsString PyString_AsString
#define PyBytes_Check PyString_Check
#endif

#include <yara.h>

#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

#ifndef PyVarObject_HEAD_INIT
#define PyVarObject_HEAD_INIT(type, size) PyObject_HEAD_INIT(type) size,
#endif

#if PY_MAJOR_VERSION >= 3
#define PY_STRING(x) PyUnicode_FromString(x)
#define PY_STRING_TO_C(x) PyBytes_AsString(PyUnicode_AsEncodedString(x, "utf-8", "Error"))
#define PY_STRING_CHECK(x) PyUnicode_Check(x)
#else
#define PY_STRING(x) PyString_FromString(x)
#define PY_STRING_TO_C(x) PyString_AsString(x)
#define PY_STRING_CHECK(x) PyString_Check(x)
#endif

/* Module globals */

static PyObject *YaraError = NULL;
static PyObject *YaraSyntaxError = NULL;


#define YARA_DOC "\
This module allows you to apply YARA rules to files or strings.\n\
\n\
For complete documentation please visit:\n\
http://code.google.com/p/yara-project/\n"



//////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    
    PyObject_HEAD
    PyObject* rule;
    PyObject* ns;
    PyObject* tags;
    PyObject* meta;
    PyObject* strings;

} Match;

static PyObject * Match_repr(PyObject *self);
static PyObject * Match_getattro(PyObject *self, PyObject *name);
static PyObject * Match_richcompare(PyObject *self, PyObject *other, int op);
static long Match_hash(PyObject *self);
static void Match_dealloc(PyObject *self);


//TODO: Change strings member to be a dictionary of offsets and objects of my own String class. This class should hold information about the matching string.

static PyMemberDef Match_members[] = {
    {"rule", T_OBJECT_EX, offsetof(Match, rule), READONLY, "Name of the matching rule"},
    {"namespace", T_OBJECT_EX, offsetof(Match, ns), READONLY, "Namespace of the matching rule"},
    {"tags", T_OBJECT_EX, offsetof(Match, tags), READONLY, "List of tags associated to the rule"},
    {"meta", T_OBJECT_EX, offsetof(Match, meta), READONLY, "Dictionary with metadata associated to the rule"},
    {"strings", T_OBJECT_EX, offsetof(Match, strings), READONLY, "Dictionary with offsets and strings that matched the file"},
    {NULL}  /* Sentinel */
};

static PyMethodDef Match_methods[] = 
{
    {NULL},
};




static PyTypeObject Match_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "yara.Match",               /*tp_name*/
    sizeof(Match),              /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor)Match_dealloc,  /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    Match_repr,                 /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    Match_hash,                 /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    Match_getattro,             /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Match class",              /* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    Match_richcompare,          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    Match_methods,              /* tp_methods */
    Match_members,              /* tp_members */
    0,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0,                          /* tp_init */
    0,                          /* tp_alloc */
    0,                          /* tp_new */
};


static PyObject * Match_NEW(const char* rule, const char* ns, PyObject* tags, PyObject* meta, PyObject* strings)
{ 
    Match* object;
    
    object = PyObject_NEW(Match, &Match_Type);
    
    if (object != NULL)
    {
        object->rule = PY_STRING(rule);
        object->ns = PY_STRING(ns);
        object->tags = tags;
        object->meta = meta;
        object->strings = strings;
        
        Py_INCREF(tags);
        Py_INCREF(meta);
        Py_INCREF(strings);
    } 
      
    return (PyObject *)object;
}

static void Match_dealloc(PyObject *self)
{    
    Match *object = (Match *) self;
     
    Py_DECREF(object->rule); 
    Py_DECREF(object->ns);
    Py_DECREF(object->tags);
    Py_DECREF(object->meta);  
    Py_DECREF(object->strings);

    PyObject_Del(self);
}

static PyObject * Match_repr(PyObject *self)
{ 
    Match *object = (Match *) self;
    Py_INCREF(object->rule);
    return object->rule;
}

static PyObject * Match_getattro(PyObject *self, PyObject *name)
{
    return PyObject_GenericGetAttr(self, name); 
}


static PyObject * Match_richcompare(PyObject *self, PyObject *other, int op)
{
    PyObject* result = NULL;
    
    Match *a = (Match *) self;
    Match *b = (Match *) other;

    if(PyObject_TypeCheck(other, &Match_Type))
    {
        switch(op)
        {
        case Py_EQ:

            if (PyObject_RichCompareBool(a->rule, b->rule, Py_EQ) && PyObject_RichCompareBool(a->ns, b->ns, Py_EQ))
                result = Py_True;
            else
                result = Py_False;
                
            Py_INCREF(result);
            break;

        case Py_NE:

            if (PyObject_RichCompareBool(a->rule, b->rule, Py_NE) || PyObject_RichCompareBool(a->ns, b->ns, Py_NE))
                result = Py_True;
            else
                result = Py_False;
                
            Py_INCREF(result);
            break;

        case Py_LT:
        case Py_LE:
        case Py_GT:
        case Py_GE:

            if (PyObject_RichCompareBool(a->rule, b->rule, Py_EQ)) 
                result = PyObject_RichCompare(a->ns, b->ns, op);
            else
                result = PyObject_RichCompare(a->rule, b->rule, op);
                
            break;
        }
    }
    else 
    {
        result = PyErr_Format(PyExc_TypeError, "'Match' objects must be compared with objects of the same class");
    }
    
    return result;

}


static long Match_hash(PyObject *self)
{
    Match *match = (Match *) self;
    return PyObject_Hash(match->rule) + PyObject_Hash(match->ns);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    
    PyObject_HEAD
    YARA_CONTEXT* context;

} Rules;


static PyObject * Rules_match(PyObject *self, PyObject *args, PyObject *keywords);
static PyObject * Rules_weight(PyObject *self);
static PyObject * Rules_getattro(PyObject *self, PyObject *name);
static void Rules_dealloc(PyObject *self);


static PyMethodDef Rules_methods[] = 
{
    {"match", (PyCFunction) Rules_match, METH_VARARGS | METH_KEYWORDS},
    {"weight", (PyCFunction) Rules_weight, METH_NOARGS},
    {NULL, NULL}
};

static PyTypeObject Rules_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "yara.Rules",               /*tp_name*/
    sizeof(Rules),              /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor)Rules_dealloc,  /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    Rules_getattro,             /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Rules class",              /* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    Rules_methods,              /* tp_methods */
    0,                          /* tp_members */
    0,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0,                          /* tp_init */
    0,                          /* tp_alloc */
    0,                          /* tp_new */
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////

int process_externals(PyObject* externals, YARA_CONTEXT* context)
{
    PyObject *key, *value;
    Py_ssize_t pos = 0;
  
    char* identifier = NULL;

    while (PyDict_Next(externals, &pos, &key, &value)) 
    {
        identifier = PY_STRING_TO_C(key);
         
#if PY_MAJOR_VERSION >= 3
        if (PyLong_Check(value))
#else
        if (PyLong_Check(value) || PyInt_Check(value))
#endif
        {  
            yr_define_integer_variable(context, identifier, PyLong_AsLong(value));
        } 
        else if (PyBool_Check(value))
        {
            yr_define_boolean_variable(context, identifier, PyObject_IsTrue(value));
        }
        else if (PY_STRING_CHECK(value))
        {
            yr_define_string_variable(context, identifier, PY_STRING_TO_C(value));
        }
        else
        {
            return FALSE;
        }       
    }

    return TRUE;
}


static PyObject * Rules_new_from_file(FILE* file, const char* filepath, PyObject* rules, YARA_CONTEXT* context)
{ 
    Rules* result;
    
    int  errors;
    int  error_line;
    char error_message[256];

    if (file == NULL)
    {
        return PyErr_SetFromErrno(PyExc_IOError);
    }
  
    if (filepath != NULL)
    {
          yr_push_file_name(context, filepath);
    }
             
    errors = yr_compile_file(file, context);
       
    if (errors)   /* errors during compilation */
    {
        error_line = context->last_error_line;
        yr_get_error_message(context, error_message, sizeof(error_message));
        
        return PyErr_Format(YaraSyntaxError, "line %d: %s", error_line, error_message);
    }

    if (rules == NULL)
    {
        result = PyObject_NEW(Rules, &Rules_Type);

        if (result != NULL)
            result->context = context;
    }
    else
    {
        result = (Rules*) rules;
    }
          
    return (PyObject *) result;
}


static PyObject * Rules_new_from_string(const char* string, PyObject* rules, YARA_CONTEXT* context)
{ 
    Rules* result;

    int  errors;
    int  error_line;
    char error_message[256];
      
    errors = yr_compile_string(string, context);
       
    if (errors)   /* errors during compilation */
    {
        error_line = context->last_error_line;
        yr_get_error_message(context, error_message, sizeof(error_message));
              
        return PyErr_Format(YaraSyntaxError, "line %d: %s", error_line, error_message); 
    }

    if (rules == NULL)
    {
        result = PyObject_NEW(Rules, &Rules_Type);

        if (result != NULL)
              result->context = context;
    }
    else
    {
        result = (Rules*) rules;
    }
          
    return (PyObject*) result;
}

static void Rules_dealloc(PyObject *self)
{     
    yr_destroy_context(((Rules*) self)->context);
    PyObject_Del(self);
}


typedef struct _CALLBACK_DATA {

    PyObject *matches;
    PyObject *callback;
    
} CALLBACK_DATA;


int yara_callback(RULE* rule, void* data)
{
    TAG* tag;
    STRING* string;
    MATCH* m;
    META* meta;
    
    PyObject* tag_list = NULL;
    PyObject* string_list = NULL;
    PyObject* meta_list = NULL;
    PyObject* match;
    PyObject* callback_dict;
    PyObject* object;
    PyObject* tuple;
    PyObject* matches = ((CALLBACK_DATA*) data)->matches;
    PyObject* callback = ((CALLBACK_DATA*) data)->callback;
    PyObject* callback_result;
    
    int result = CALLBACK_CONTINUE;
    
    if (!(rule->flags & RULE_FLAGS_MATCH) && callback == NULL)
        return CALLBACK_CONTINUE;
    
    tag_list = PyList_New(0);
    string_list = PyList_New(0);
    meta_list = PyDict_New();
    
    if (tag_list == NULL || string_list == NULL || meta_list == NULL)
    {
        Py_XDECREF(tag_list);
        Py_XDECREF(string_list);
        Py_XDECREF(meta_list);
        
        return CALLBACK_ERROR;
    }           
    
    tag = rule->tag_list_head;
    
    while(tag != NULL)
    {
        object = PY_STRING(tag->identifier);
        PyList_Append(tag_list, object);
        Py_DECREF(object);    
                   
        tag = tag->next;
    }      
    
    meta = rule->meta_list_head;
    
    while(meta != NULL)
    {
        if (meta->type == META_TYPE_INTEGER)
        {
            object = Py_BuildValue("I", meta->integer);
        }
        else if (meta->type == META_TYPE_BOOLEAN)
        {
            object = PyBool_FromLong(meta->boolean);
        }
        else
        {
            object = PY_STRING(meta->string);
        }
        
        PyDict_SetItemString(meta_list, meta->identifier, object);  
        Py_DECREF(object);
        
        meta = meta->next;
    } 
    
    string = rule->string_list_head;

    while (string != NULL)
    {
        if (string->flags & STRING_FLAGS_FOUND)
        {
            m = string->matches_head;

            while (m != NULL)
            {
                object = PyBytes_FromStringAndSize((char*) m->data, m->length);
                tuple = Py_BuildValue("(i,s,O)", m->offset, string->identifier, object);
                
                PyList_Append(string_list, tuple);
                
                Py_DECREF(object);
                Py_DECREF(tuple);
                   
                m = m->next;
            }
        }

        string = string->next;
    }
    
    PyList_Sort(string_list);
    
    if (rule->flags & RULE_FLAGS_MATCH)
    {
        match = Match_NEW(rule->identifier, rule->ns->name, tag_list, meta_list, string_list);

        if (match != NULL)
        {       
            PyList_Append(matches, match);
            Py_DECREF(match);
        }
        else
        {
            Py_DECREF(tag_list);
            Py_DECREF(string_list);
            Py_DECREF(meta_list);
    
            return CALLBACK_ERROR;
        }
    }
    
    if (callback != NULL)
    {
        Py_INCREF(callback); 
      
        callback_dict = PyDict_New();
      
        object = PyBool_FromLong(rule->flags & RULE_FLAGS_MATCH);
        PyDict_SetItemString(callback_dict, "matches", object);
        Py_DECREF(object);
      
        object = PY_STRING(rule->identifier);        
        PyDict_SetItemString(callback_dict, "rule", object);
        Py_DECREF(object);
        
        object = PY_STRING(rule->ns->name);
        PyDict_SetItemString(callback_dict, "namespace", object);
        Py_DECREF(object);
        
        PyDict_SetItemString(callback_dict, "tags", tag_list);
        PyDict_SetItemString(callback_dict, "meta", meta_list);
        PyDict_SetItemString(callback_dict, "strings", string_list);

        callback_result = PyObject_CallFunctionObjArgs(callback, callback_dict, NULL);

        if (callback_result != NULL)
        {
            #if PY_MAJOR_VERSION >= 3
            if (PyLong_Check(callback_result))
            #else
            if (PyLong_Check(callback_result) || PyInt_Check(callback_result))
            #endif
            {
                result = (int) PyLong_AsLong(callback_result);
            }
        
            Py_DECREF(callback_result);
        }
        else
        {
            result = CALLBACK_ERROR;
        }
      
        Py_DECREF(callback_dict);
        Py_DECREF(callback); 
    }
    
  Py_DECREF(tag_list);
  Py_DECREF(string_list);
  Py_DECREF(meta_list);  
    
  return result;

}

PyObject * Rules_match(PyObject *self, PyObject *args, PyObject *keywords)
{
    static char *kwlist[] = {"filepath", "pid", "data", "externals", "callback", NULL};
    
    char* filepath = NULL;
    char* data = NULL;

    int pid = 0;
    int length;
    int result;
    
    PyObject *error;
    PyObject *externals = NULL;
    Rules* object = (Rules*) self;

    CALLBACK_DATA callback_data;
    
    callback_data.matches = NULL;
    callback_data.callback = NULL;
    
    if (PyArg_ParseTupleAndKeywords(args, keywords, "|sis#OO", kwlist, &filepath, &pid, &data, &length, &externals, &callback_data.callback))
    {
        if (externals != NULL)
        {
            if (PyDict_Check(externals))
            {
                if (!process_externals(externals, object->context))
                {
                    return PyErr_Format(PyExc_TypeError, "external values must be of type integer, boolean or string");
                }       
            }
            else
            {
                return PyErr_Format(PyExc_TypeError, "'externals' must be a dictionary");
            }
        }
             
        if (callback_data.callback != NULL)
        {
            if (!PyCallable_Check(callback_data.callback)) 
            {
                return PyErr_Format(YaraError, "callback must be callable");
            }
        }
             
        if (filepath != NULL)
        {    
            callback_data.matches = PyList_New(0);
        
            result = yr_scan_file(filepath, object->context, yara_callback, &callback_data);

            if (result != ERROR_SUCCESS)
            {
                Py_DECREF(callback_data.matches);

                switch(result)
                {
                    case ERROR_COULD_NOT_OPEN_FILE:
                        return PyErr_Format(YaraError, "could not open file \"%s\"", filepath);
                    case ERROR_COULD_NOT_MAP_FILE:
                        return PyErr_Format(YaraError, "could not map file \"%s\" into memory", filepath);
                    case ERROR_ZERO_LENGTH_FILE:
                        return PyErr_Format(YaraError, "zero length file \"%s\"", filepath);
                    default:
                        return PyErr_Format(YaraError, "unknown error while scanning file \"%s\"", filepath);
                }
            }
        }
        else if (data != NULL)
        {
            callback_data.matches = PyList_New(0);
        
            result = yr_scan_mem((unsigned char*) data, (unsigned int) length, object->context, yara_callback, &callback_data);

            if (result != ERROR_SUCCESS)
            {
                Py_DECREF(callback_data.matches);
               
                error = PyErr_Occurred();
               
                if (error != NULL)
                    return NULL;
                else
                    return PyErr_Format(PyExc_Exception, "internal error"); 
            }
        }
        else if (pid != 0)
        {
            callback_data.matches = PyList_New(0);
            
            result = yr_scan_proc(pid, object->context, yara_callback, &callback_data);
            
            if (result != ERROR_SUCCESS)
            {
               Py_DECREF(callback_data.matches);
               
               switch(result)
               {
                   case ERROR_COULD_NOT_ATTACH_TO_PROCESS:
                       return PyErr_Format(YaraError, "access denied");
                   case ERROR_INSUFICIENT_MEMORY:
                       return PyErr_Format(YaraError, "not enough memory"); 
                   default:
                       return PyErr_Format(YaraError, "unknown error while scanning file \"%s\"", filepath);
               }
            }
        }
        else
        {
            return PyErr_Format(PyExc_TypeError, "match() takes 1 argument");
        }
    }
    
    return callback_data.matches;
}

static PyObject * Rules_getattro(PyObject *self, PyObject *name)
{
    return PyObject_GenericGetAttr(self, name); 
}

static PyObject * Rules_weight(PyObject *self)
{
    Rules* object = (Rules*) self;
    
    return PyLong_FromLong(yr_calculate_rules_weight(object->context));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

static PyObject * yara_compile(PyObject *self, PyObject *args, PyObject *keywords)
{ 
    static char *kwlist[] = {"filepath", "source", "file", "filepaths", "sources", "includes", "externals", NULL};
    
    YARA_CONTEXT* context;
    FILE* fh;
    int fd;
    
    PyObject *result = NULL;
    PyObject *file = NULL;
  
    PyObject *sources_dict = NULL;
    PyObject *filepaths_dict = NULL;
    PyObject *includes = NULL;
    PyObject *externals = NULL;
  
    PyObject *key, *value;
  
    Py_ssize_t pos = 0;

    char* filepath = NULL;
    char* source = NULL;
    char* ns = NULL;
      
    if (PyArg_ParseTupleAndKeywords(args, keywords, "|ssOOOOO", kwlist, &filepath, &source, &file, &filepaths_dict, &sources_dict, &includes, &externals))
    {      
        context = yr_create_context();
   
      if (context == NULL)
          return PyErr_NoMemory();
                    
        if (includes != NULL)
        {
            if (PyBool_Check(includes))
            {
                context->allow_includes = (PyObject_IsTrue(includes) == 1);  // PyObject_IsTrue can return -1 in case of error
            }
            else
            {
                yr_destroy_context(context); 
                return PyErr_Format(PyExc_TypeError, "'includes' param must be of boolean type");
            }
        }
            
        if (externals != NULL)
        {
            if (PyDict_Check(externals))
            {
                if (!process_externals(externals, context))
                {
                    yr_destroy_context(context); 
                    return PyErr_Format(PyExc_TypeError, "external values must be of type integer, boolean or string");
                }       
            }
            else
            {
                yr_destroy_context(context); 
                return PyErr_Format(PyExc_TypeError, "'externals' must be a dictionary");
            }
        }
     
        if (filepath != NULL)
        {            
            fh = fopen(filepath, "r");
            
            if (fh != NULL)
            {
                result = Rules_new_from_file(fh, filepath, NULL, context);
                fclose(fh);
            }
            else
            {
                result = PyErr_SetFromErrno(YaraError);
            }
        }
        else if (source != NULL)
        {
            result = Rules_new_from_string(source, NULL, context);
        }
        else if (file != NULL)
        {
            fd = dup(PyObject_AsFileDescriptor(file));
            fh = fdopen(fd, "r");
            result = Rules_new_from_file(fh, NULL, NULL, context);
            fclose(fh);
        }
        else if (sources_dict != NULL)
        {
            if (PyDict_Check(sources_dict))
            {
                while (PyDict_Next(sources_dict, &pos, &key, &value)) 
                {
                    source = PY_STRING_TO_C(value);
                    ns = PY_STRING_TO_C(key);
          
                    if (source != NULL && ns != NULL)
                    {
                        context->current_namespace = yr_create_namespace(context, ns);
                        result = Rules_new_from_string(source, result, context);
                    }
                    else
                    {
                        result = PyErr_Format(PyExc_TypeError, "keys and values of the 'sources' dictionary must be of string type");
                        break;
                    }
                }
            }
            else
            {
                result = PyErr_Format(PyExc_TypeError, "'sources' must be a dictionary");
            }
        }
        else if (filepaths_dict != NULL)
        {
            if (PyDict_Check(filepaths_dict))
            {
                while (PyDict_Next(filepaths_dict, &pos, &key, &value)) 
                {
                    filepath = PY_STRING_TO_C(value);
                    ns = PY_STRING_TO_C(key);
      
                    if (filepath != NULL && ns != NULL)
                    {
                        fh = fopen(filepath, "r");
        
                        if (fh != NULL)
                        {
                            context->current_namespace = yr_create_namespace(context, ns);
              
                            result = Rules_new_from_file(fh, filepath, result, context);
                            fclose(fh);
                        }
                        else
                        { 
                            result = PyErr_SetFromErrno(YaraError);
                        }
                    }
                    else
                    {
                        result = PyErr_Format(PyExc_TypeError, "keys and values of the filepaths dictionary must be of string type");
                        break;
                    }
                }
            }
            else
            {
                result = PyErr_Format(PyExc_TypeError, "filepaths must be a dictionary");
            }
        }
        else
        {
            result = PyErr_Format(PyExc_TypeError, "compile() takes 1 argument");
        }
    } 
      
    return result;
}



/* Module functions */

static PyMethodDef yara_methods[] = {
    {"compile", (PyCFunction) yara_compile, METH_VARARGS | METH_KEYWORDS, "Compiles a YARA rules file and returns an instance of class Rules"},
    {NULL, NULL}
};

#if PY_MAJOR_VERSION >= 3
#define MOD_ERROR_VAL NULL
#define MOD_SUCCESS_VAL(val) val
#define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
#define MOD_DEF(ob, name, doc, methods) \
      static struct PyModuleDef moduledef = { \
        PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
      ob = PyModule_Create(&moduledef);
#else
#define MOD_ERROR_VAL
#define MOD_SUCCESS_VAL(val)
#define MOD_INIT(name) void init##name(void)
#define MOD_DEF(ob, name, doc, methods) \
      ob = Py_InitModule3(name, methods, doc);
#endif


MOD_INIT(yara)
{ 
    PyObject *m;
    
    MOD_DEF(m, "yara", YARA_DOC, yara_methods)
    
    if (m == NULL)
        return MOD_ERROR_VAL;
    
    /* initialize module variables/constants */
    
    PyModule_AddIntConstant(m, "CALLBACK_CONTINUE", 0);
    PyModule_AddIntConstant(m, "CALLBACK_ABORT", 1);

#if PYTHON_API_VERSION >= 1007
    YaraError = PyErr_NewException("yara.Error", PyExc_Exception, NULL);
    YaraSyntaxError = PyErr_NewException("yara.SyntaxError", YaraError, NULL);
#else
    YaraError = Py_BuildValue("s", "yara.Error");
    YaraSyntaxError = Py_BuildValue("s", "yara.SyntaxError");
#endif

    if (PyType_Ready(&Rules_Type) < 0)
        return MOD_ERROR_VAL;
        
    if (PyType_Ready(&Match_Type) < 0)
        return MOD_ERROR_VAL;

    PyModule_AddObject(m, "Error", YaraError);
    PyModule_AddObject(m, "SyntaxError", YaraSyntaxError);
    
    yr_init();
    
    return MOD_SUCCESS_VAL(m);
}
