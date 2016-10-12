/*
 * Earl, the fanciest ETF Packer and Unpacker for Python.
 * Written as a C++ Extension for Maximum Effort and Speed.
 *
 * Copyright 2016 Charles Click under the MIT License.
 */

#define FLOAT_IEEE_EXT "F"
#define BIT_BINARY_EXT "M"
#define SMALL_INTEGER_EXT "a"
#define INTEGER_EXT "b"
#define FLOAT_EXT "c"
#define SMALL_TUPLE_EXT "h"
#define LARGE_TUPLE_EXT "i"
#define NIL_EXT "j"
#define STRING_EXT "k"
#define LIST_EXT "l"
#define BINARY_EXT "m"
#define SMALL_BIG_EXT "n"
#define LARGE_BIG_EXT "o"
#define MAP_EXT "t"

#include <Python.h>
#include <vector>
#include <string>

std::string etf_small_int(int value){

    std::string buffer = SMALL_INTEGER_EXT;
    buffer += char(value);
    return buffer;

}

std::string etf_big_int(unsigned long value){

    std::string buffer = INTEGER_EXT;
    buffer += ((value >> 24) & 0xFF);
    buffer += ((value >> 16) & 0xFF);
    buffer += ((value >> 8) & 0xFF);
    buffer += (value & 0xFF);

    return buffer;

}

std::string etf_string(PyObject *value){

    int len = PyUnicode_GET_LENGTH(value);
    int kind = PyUnicode_KIND(value);
    
    std::string buffer = STRING_EXT;
    buffer += (len >> 8) & 0xFF;
    buffer += len & 0xFF;

    if (!len or !kind){

        if ( !PyErr_Occurred() ){

            PyErr_SetString(PyExc_RuntimeError, "Failed to process the python string.");

        }

        return NULL;

    }

    for( int i={0}; i < len; i++){

        buffer += char(PyUnicode_READ(kind, PyUnicode_DATA(value), i));

    }

    return buffer;

}

std::string etf_float(double value){
    
    std::string buffer = FLOAT_IEEE_EXT;
    
    unsigned char const * p = reinterpret_cast<unsigned char const *>(&value);
    
    for ( int i={0}; i < sizeof(p); i++ ){
        
        buffer += (int(p[7-i]) & 0xFF);
       
    }
    
    return buffer;
    
}

std::string etf_tuple(PyObject* tuple){
    
    Py_ssize_t len = PyTuple_Size(tuple);
    std::string buffer;
    
    if ( len > 256 ){
    
        buffer = LARGE_TUPLE_EXT;
        buffer += len & 0xFF;
        
        for( int ii={0}; ii < len; ii++ ){
        
            PyObject* temp = PyTuple_GetItem(tuple, ii);
            
            buffer += SMALL_INTEGER_EXT;
            buffer += char(PyLong_AsUnsignedLong(temp));
        
        }
    
    } else {
    
        buffer = SMALL_TUPLE_EXT;
        buffer += char(len);
        
        for( int ii={0}; ii < len; ii++ ){
            
            PyObject* temp = PyTuple_GetItem(tuple, ii);
            
            buffer += SMALL_INTEGER_EXT;
            buffer += char(PyLong_AsUnsignedLong(temp));
            
        }
    
    }
    
    return buffer;
    
}



static PyObject* earl_pack(PyObject* self, PyObject *args){

    std::string package;
    std::vector<PyObject*> objects;
    Py_ssize_t len = PyTuple_Size(args);

    if( !len ){
        
        if( !PyErr_Occurred() ){

            PyErr_SetString(PyExc_SyntaxError, "You must supply at least one argument.");

        }

        return NULL;

    }

    package = "\x83";

    for( int i={0}; i < len; i++ ){

        PyObject* temp = PyTuple_GetItem(args, i);

        if( temp == NULL ){

            if( !PyErr_Occurred() ){

                PyErr_SetString(PyExc_TypeError, "Earl wasn't able to unpack all your arguments for some reason.");

            }
            return NULL;

        }

        if( PyLong_Check(temp) ){

            unsigned long temp_int = PyLong_AsUnsignedLong(temp);
            temp_int < 256 ? package += etf_small_int(temp_int) : package += etf_big_int(temp_int);

        } else if( PyUnicode_Check(temp) ) {

            if( PyUnicode_READY(temp) != 0 ){

                if( !PyErr_Occurred() ){

                    PyErr_SetString(PyExc_RuntimeError, "Earl wasn't able to migrate the Python Unicode data to memory.");

                }
                return NULL;

            }else{

                package += etf_string(temp);

            }

        }else if( PyFloat_Check(temp) ){
        
            package += etf_float(PyFloat_AsDouble(temp));
    
        }else if( PyTuple_Check(temp) ){
        
            package += etf_tuple(temp);
        
        }else if( PySet_Check(temp) ){
        
            //package += etf_set(temp);
            break;
        
        }else if( PyList_Check(temp) ){
        
            //package += etf_listt(temp);
            break;
        
        }else if( PyDict_Check(temp) ){
        
            //package += etf_dict(temp);
            break;
        
        }else{

            if ( !PyErr_Occurred() ){

                PyErr_SetString(PyExc_TypeError, "Earl can't pack one of the types you gave it!");

            }
            return NULL;

        }

    }

    return Py_BuildValue("y#", package.c_str(), package.length());

}

static char earl_pack_docs[] = "pack(values): Pack a bunch of things into an External Term Format";

static PyMethodDef earlmethods[] = {

    {"pack", earl_pack, METH_VARARGS, earl_pack_docs},
    {NULL, NULL, 0, NULL}

};

static struct PyModuleDef earl = {

    PyModuleDef_HEAD_INIT,
    "earl",
    "Earl is the fanciest External Term Format library for Python.",
    -1,
    earlmethods

};

PyMODINIT_FUNC PyInit_earl(void){

    return PyModule_Create(&earl);

}
