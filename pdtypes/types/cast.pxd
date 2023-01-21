cimport numpy as np

cimport pdtypes.types.atomic as atomic


cdef tuple _apply_with_errors(
    np.ndarray[object] arr,
    object call,
    str errors
)


cdef class SeriesWrapper:
    cdef:
        object _hasinfs  # bint can't store None
        object _hasnans  # bint can't store None
        object _series

    cdef readonly:
        object fill_value
        object original_index
        unsigned int size

    cdef public:
        dict cache
