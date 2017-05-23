# Earl
Earl is the fanciest C++ extension for Python that allows External Term Format packing and unpacking.
This library can support packing and unpacking the External Term Format made popular by Erlang in Python. Written in C++ using the Python C API, it should be
marginally usable across the various Python implementations, but for now I can guarantee CPython because that's what it was built against.

I have tested that the ETF this produces works with binary_to_term in Erlang.

# Installation
```
pip install Earl-ETF
```

# Usage
```Python
import earl
a = earl.pack([1,2,3])
earl.unpack(a)
```
## Functions
There are only two. Pack and Unpack. Give pack anything you want to convert to External Term Format. Give Unpack a bytes object that represents data in External Term Format to get back Python Objects as per below.

# Features
Currently Earl supports these features. Earl is written for the latest version of External Term Format as of Erlang 8.2.

## Packing
* SMALL_INTEGER_EXT
* INTEGER_EXT
* SMALL_TUPLE_EXT
* LARGE_TUPLE_EXT
* LIST_EXT
* STRING_EXT
* MAP_EXT
* ATOM_UTF8_EXT
* NIL_EXT

### Python Types to Pack Types
This is a list of Python types and the corresponding ETF type they are converted to.

* Integers < 256: SMALL_INTEGER_EXT
* Integers >= 256: INTEGER_EXT
* String/Unicode: ATOM_UTF8
* Bytes: STRING_EXT (If more than 65535, LIST_EXT)
* Dictionary: MAP_EXT
* Tuple: SMALL_TUPLE_EXT/LARGE_TUPLE_EXT (Depending on Size)
* Lists/Sets: LIST_EXT
* None: An Atom with the name nil
* True/False: Atoms with the same name
* Empty List ([]): In Erlang, Nil() is a special value representing the empty list. In ETF it is represented by NIL_EXT. If you pass an empty list Earl will return NIL_EXT.

## Unpacking
* SMALL_INTEGER_EXT
* INTEGER_EXT
* SMALL_TUPLE_EXT
* LARGE_TUPLE_EXT
* LIST_EXT
* STRING_EXT
* MAP_EXT
* ATOM_UTF8_EXT
* SMALL_ATOM_UTF8_EXT
* ATOM_EXT
* BINARY_EXT

### Some notes about unpacking
* You can only provide unpack one bytes object. It does not unpack many bytes objects.
* They are converted to python types according to the list above under packing.
* Maps are slow to parse because they are parsed independently in order to retain depth. I could make this faster by ignoring depth, but that defeats the purpose of a map.

# So how fast is this
This code is C++ accessing individual bytes, which means it is fairly fast. Overall, the speed depends on how many items you ask it to unpack or pack and how complex each item is. This is to be expected. Depending on that, the speeds can range from nanoseconds to microseconds.

## Actual Benchmarks
Benchmarks updated as of 1.7.

# Packing
```
click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl" "earl.pack(120)"
10000000 loops, best of 3: 0.179 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl" "earl.pack(12000)"
10000000 loops, best of 3: 0.183 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl" "earl.pack((1,2,3))"
1000000 loops, best of 3: 0.32 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl" "earl.pack([1,2,3])"
1000000 loops, best of 3: 0.371 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl" "earl.pack({1,2,3})"
1000000 loops, best of 3: 0.422 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl" "earl.pack({1:2, 3:4})"
1000000 loops, best of 3: 0.532 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl" "a=earl.pack({1:{'d':[1,2,3], '_trace': 12000}})"
1000000 loops, best of 3: 1.31 usec per loop
```

# Unpacking
```
click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl;a=earl.pack(120)" "earl.unpack(a)"
10000000 loops, best of 3: 0.185 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl;a=earl.pack(12000)" "earl.unpack(a)"
10000000 loops, best of 3: 0.201 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl;a=earl.pack((1,2,3))" "earl.unpack(a)"
1000000 loops, best of 3: 0.247 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl;a=earl.pack([1,2,3])" "earl.unpack(a)"
1000000 loops, best of 3: 0.262 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl;a=earl.pack({1,2,3})" "earl.unpack(a)"
1000000 loops, best of 3: 0.27 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl;a=earl.pack({1:2, 3:4})" "earl.unpack(a)"
1000000 loops, best of 3: 0.614 usec per loop

click@DESKTOP-QL2J54T C:\Users\click\Documents\GitHub\Earl
> python -m timeit --setup="import earl;a=earl.pack({1:{'d':[1,2,3], '_trace': 12000}})" "earl.unpack(a)"
1000000 loops, best of 3: 1.24 usec per loop
```
