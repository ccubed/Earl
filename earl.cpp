/*
 * Earl, the fanciest ETF Packer and Unpacker for Python.
 * Written as a C++ Extension for Maximum Effort and Speed.
 *
 * Copyright 2016 Charles Click under the MIT License.
 */

 // Includes
 #include <Python.h>
 #include <string>
 #include <vector>

// External Term Format Defines
#define FLOAT_IEEE_EXT 'F'
#define BIT_BINARY_EXT 'M'
#define SMALL_INTEGER_EXT 'a'
#define INTEGER_EXT 'b'
#define FLOAT_EXT 'c'
#define SMALL_TUPLE_EXT 'h'
#define LARGE_TUPLE_EXT 'i'
#define NIL_EXT 'j'
#define STRING_EXT 'k'
#define LIST_EXT 'l'
#define BINARY_EXT 'm'
#define SMALL_BIG_EXT 'n'
#define LARGE_BIG_EXT 'o'
#define MAP_EXT 't'

#if _MSC_VER // MSVC doesn't support or
#define or ||
#endif

// Function Prototypes
// Packing Functions
std::string etfp_small_int(int value);
std::string etfp_big_int(unsigned long value);
std::string etfp_string(PyObject* value);
std::string etfp_float(double value);
std::string etfp_tuple(PyObject* tuple);
std::string etfp_set(PyObject* set);
std::string etfp_list(PyObject* list);
std::string etfp_dict(PyObject* dict);
std::string etf_pack_item(PyObject* object);
static earl_pack(PyObject* self, PyObject* args);
// Unpacking Functions
PyObject* etfup_bytes(PyObject* item);
PyObject* etfup_str(PyObject* item);
PyObject* etfup_small_int(int value);
PyObject* etfup_big_int(unsigned long value);
PyObject* etfup_string(std::string text);
PyObject* etfup_float(double value);
PyObject* etfup_tuple(std::vector tuple);
PyObject* etfup_set(std::vector set);
PyObject* etfup_list(std::vector list);
PyObject* etfup_dict(std::string etfbytes); // We have to parse this as the bytes by itself or we'll lose the depth.
static earl_unpack(PyObject* self, PyObject* args);
// End Function Prototypes

std::string etfp_small_int(int value){

  std::string buffer = SMALL_INTEGER_EXT;
  buffer += char(value);
  return buffer;

}

std::string etfp_big_int(unsigned long value){

  std::string buffer = INTEGER_EXT;
  buffer += ((value >> 24) & 0xFF);
  buffer += ((value >> 16) & 0xFF);
  buffer += ((value >> 8) & 0xFF);
  buffer += (value & 0xFF);

  return buffer;

}

std::string etfp_string(PyObject *value){

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

std::string etfp_float(double value){

  std::string buffer = FLOAT_IEEE_EXT;

  unsigned char const * p = reinterpret_cast<unsigned char const *>(&value);

  if ( sizeof(p) < 8 ){

    return NULL;

  }

  for ( int ii={0}; ii < 8; ii++ ){

      buffer += (int(p[7-ii]) & 0xFF);

  }

  return buffer;

}

std::string etfp_tuple(PyObject* tuple){

  Py_ssize_t len = PyTuple_Size(tuple);
  std::string buffer = (len > 256 ? LARGE_TUPLE_EXT : SMALL_TUPLE_EXT);

  if( len > 256 ){

    buffer += ((len >> 24) & 0xFF);
    buffer += ((len >> 16) & 0xFF);
    buffer += ((len >> 8) & 0xFF);
    buffer += (len & 0xFF);

  } else {

    buffer += (len & 0xFF);

  }

  for( int ii={0}; ii < len; ii++ ){

      PyObject* temp = PyTuple_GetItem(tuple, ii);

      buffer += etf_pack_item(temp);

  }

  return buffer;

}

std::string etfp_set(PyObject* set){

  Py_ssize_t len = PySet_Size(set);
  std::string buffer = LIST_EXT;

  buffer += ((len >> 24) & 0xFF);
  buffer += ((len >> 16) & 0xFF);
  buffer += ((len >> 8) & 0xFF);
  buffer += (len & 0xFF);

  for( int ii={0}; ii < len; ii++ ){

    PyObject* temp = PySet_Pop(set);
    buffer += etf_pack_item(temp);

  }

  buffer += NIL_EXT;
  return buffer;

}

std::string etfp_list(PyObject* list){

  Py_ssize_t len = PyList_Size(list);
  std::string buffer = LIST_EXT;

  buffer += ((len >> 24) & 0xFF);
  buffer += ((len >> 16) & 0xFF);
  buffer += ((len >> 8) & 0xFF);
  buffer += (len & 0xFF);

  for( int ii={0}; ii < len; ii++ ){

    PyObject* temp = PyList_GetItem(list, ii);
    buffer += etf_pack_item(temp);

  }

  buffer += NIL_EXT;
  return buffer;

}

std::string etfp_dict(PyObject* dict){

  Py_ssize_t len = PyDict_Size(dict);
  std::string buffer = MAP_EXT;

  buffer += ((len >> 24) & 0xFF);
  buffer += ((len >> 16) & 0xFF);
  buffer += ((len >> 8) & 0xFF);
  buffer += (len & 0xFF);

  len = 0;
  PyObject *k, *v;

  while( PyDict_Next(dict, &len, &k, &v) ){

    buffer += etf_pack_item(k);
    buffer += etf_pack_item(v);

  }

  return buffer;

}

std::string etf_pack_item(PyObject* temp){

  std::string buffer;

  if( PyLong_Check(temp) ){

    unsigned long temp_int = PyLong_AsUnsignedLong(temp);
    temp_int < 256 ? buffer += etfp_small_int(temp_int) : buffer += etfp_big_int(temp_int);

  } else if( PyUnicode_Check(temp) ) {

    if( PyUnicode_READY(temp) != 0 ){

      if( !PyErr_Occurred() ){

        PyErr_SetString(PyExc_RuntimeError, "Earl wasn't able to migrate the Python Unicode data to memory.");

      }
      return NULL;

    }else{

      buffer += etfp_string(temp);

    }

  }else if( PyFloat_Check(temp) ){

    buffer += etfp_float(PyFloat_AsDouble(temp));

  }else if( PyTuple_Check(temp) ){

    buffer += etfp_tuple(temp);

  }else if( PySet_Check(temp) ){

    buffer += etfp_set(temp);

  }else if( PyList_Check(temp) ){

    buffer += etfp_list(temp);

  }else if( PyDict_Check(temp) ){

    buffer += etfp_dict(temp);

  }else{

    if ( !PyErr_Occurred() ){

      PyErr_SetString(PyExc_TypeError, "Earl can't pack one of the types you gave it!");

    }
    return NULL;

  }

  return buffer;

}

PyObject* etfup_bytes(PyObject* item){
    
    std::string buffer(PyBytes_AsString(item));
    int pos = 0;
    
    for( pos; pos < buffer.length()-1; pos++ ){
        
        if( buffer[ii] == INTEGER_EXT or buffer[ii] == SMALL_INTEGER_EXT ){
            
          if( buffer[ii] == INTEGER_EXT ){
            
            long upd = 0;
            for( int nb = {1}; nb < 5; nb++ ){
              
              upd = (upd << 8) + buffer[ii+nb];
              
            }
            return PyLong_FromLong(upd);
            
          } else {
            
            long upd = 0;
            upd = (upd << 8) + buffer[ii+1];
            return PyLong_FromLong(upd);
            
          }
            
        } else if( buffer[ii] == FLOAT_IEEE_EXT or buffer[ii] == FLOAT_EXT ){
            
            
            
        } else if( buffer[ii] == LIST_EXT ){
            
            
            
        } else if( buffer[ii] == MAP_EXT ){
            
            
            
        } else if( buffer[ii] == STRING_EXT ){
            
            
            
        } else if( buffer[ii] == SMALL_TUPLE_EXT or buffer[ii] = LARGE_TUPLE_EXT ){
            
            
            
        }
        
    }
    
}

PyObject* etfup_str(PyObject* item);
PyObject* etfup_small_int(int value);
PyObject* etfup_big_int(unsigned long value);
PyObject* etfup_string(std::string text);
PyObject* etfup_float(double value);
PyObject* etfup_tuple(std::vector tuple);
PyObject* etfup_set(std::vector set);
PyObject* etfup_list(std::vector list);
PyObject* etfup_dict(std::string etfbytes);

static PyObject* earl_pack(PyObject* self, PyObject *args){

  std::string package;
  Py_ssize_t len = PyTuple_Size(args);

  if( !len ){

    if( !PyErr_Occurred() ){

      PyErr_SetString(PyExc_SyntaxError, "You must supply at least one argument.");

    }

    return NULL;

  }

  package = "\x83";

  if( len > 1 ){

    package += LIST_EXT;
    package += (len & 0xFF);

  }

  for( int i={0}; i < len; i++ ){

    PyObject* temp = PyTuple_GetItem(args, i);

    if( temp == NULL ){

      if( !PyErr_Occurred() ){

        PyErr_SetString(PyExc_TypeError, "Earl wasn't able to unpack all your arguments for some reason.");

      }
      return NULL;

    }

    package += etf_pack_item(temp);

  }

  if( len > 1 ){

    package += NIL_EXT;

  }

  return Py_BuildValue("y#", package.c_str(), package.length());

}

static PyObject* earl_unpack(PyObject* self, PyObject *args){
    
    Py_ssize_t len = PyTuple_Size(args);
    
    if ( len > 1 ){
        
        PyObject* return_value = PyList_New(len);
    
        for( int i={0}; i < len; i++){
        
            PyObject* temp = PyTuple_GetItem(args, i);
        
            if( PyBytes_Check(temp) ){
            
                if( PyList_SetItem(return_value, i, etfup_bytes(temp)) == -1 ){
                    
                    if( !PyErr_Occurred() ){
                        
                        PyErr_SetString(PyExc_RuntimeError, "Earl was unable to unpack data.");
                        return NULL;
                        
                    }
                    
                }
            
            } else if( PyUnicode_Check(temp) ){
            
                if( PyList_SetItem(return_value, i, etfup_str(temp)) == -1 ){
                    
                    if( !PyErr_Occurred() ){
                        
                        PyErr_SetString(PyExc_RuntimeError, "Earl was unable to unpack data.");
                        return NULL;
                        
                    }
                    
                }
            
            } else {
            
            
                if( !PyErr_Occurred() ){
                
                    PyErr_SetString(PyExc_TypeError, "Earl can only unpack Str and Bytes objects.");
                    return NULL;
                
                }
            
            }
        
        }
        
        return Py_BuildValue("O", return_value);
    
    } else {
        
        if( PyBytes_Check(temp) ){
            
            return Py_BuildValue("O", etfup_bytes(PyTuple_GetItem(tuple, 0)));
            
        } else if( PyUnicode_Check(temp) ){
            
            return Py_BuildValue("O", etfup_str(PyTuple_GetItem(tuple, 0)));
            
        } else {
            
            if( !PyErr_Occurred() ){
                
                PyErr_SetString(PyExc_TypeError, "Earl can only unpack Str and Bytes objects.");
                return NULL;
                
            }
            
        }
        
    }
    
    return NULL;
    
}

static char earl_pack_docs[] = "pack(values): Pack a bunch of things into an External Term Format. Multiple items or types of items are packed into a list format.";
static char earl_unpack_docs[] = "unpack(data): Unpack ETF data. Data is unpacked according to standards and supported types. If multiple items are given, return as a list.";

static PyMethodDef earlmethods[] = {

  {"pack", earl_pack, METH_VARARGS, earl_pack_docs},
  {"unpack", earl_unpack, METH_VARARGS, earl_unpack_docs},
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
