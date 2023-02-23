cimport numpy as np
cimport pdtypes.types.atomic as atomic


# constants
cdef dict na_strings
cdef object call
cdef object sequence
cdef object literal
cdef object token


# helpers
cdef str nested(str opener, str closer, str name)
cdef list tokenize(str input_str)
cdef atomic.BaseType resolve_typespec_string(str input_str)
cdef atomic.ScalarType resolve_typespec_dtype(object input_dtype)
cdef atomic.AtomicType resolve_typespec_type(type input_type)
