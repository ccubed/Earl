/*
 * Earl, the fanciest ETF Packer and Unpacker for Python.
 * Written as a C++ Extension for Maximum Effort and Speed.
 *
 * Copyright 2016 Charles Click under the MIT License.
 */

#define FLOAT_IEEE_EXT 70
#define BIT_BINARY_EXT 77
#define SMALL_INTEGER_EXT 97
#define INTEGER_EXT 98
#define FLOAT_EXT 99
#define SMALL_TUPLE_EXT 104
#define LARGE_TUPLE_EXT 105
#define NIL_EXT 106
#define STRING_EXT 107
#define LIST_EXT 108
#define BINARY_EXT 109
#define SMALL_BIG_EXT 110
#define LARGE_BIG_EXT 111
#define MAP_EXT 116

#define DEBUG 1

#include <Python.h>
#include <vector>
#include <string>
#include <iostream>

std::string etf_small_int(int value){

    return std::to_string(SMALL_INTEGER_EXT) + std::to_string(value);

}

std::string etf_big_int(unsigned long value){

    std::string buffer = std::to_string(INTEGER_EXT);
    buffer += (value >> 24) & 0xFF;
    buffer += (value >> 16) & 0xFF;
    buffer += (value >> 8) & 0xFF;
    buffer += value & 0xFF;

    return buffer;

}

std::string etf_string(PyObject *value){

    int len = PyUnicode_GET_LENGTH(value);
    int kind = PyUnicode_KIND(value);
    std::string buffer = "";

    if (!len or !kind){

        if ( !PyErr_Occurred() ){

            PyErr_SetString(PyExc_RuntimeError, "Failed to process the python string.");

        }

        return NULL;

    }

    for( int i={0}; i < len; i++){

        buffer += std::to_string(PyUnicode_READ(kind, PyUnicode_DATA(value), i));

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

        }else{

            if ( !PyErr_Occurred() ){

                PyErr_SetString(PyExc_TypeError, "Earl can't pack one of the types you gave it!");

            }
            return NULL;

        }

    }

    #if debug == 1
    std::cout << "Debug Post Expansion: " << package << std::endl;
    #endif

    return Py_BuildValue("s", package.c_str());

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
