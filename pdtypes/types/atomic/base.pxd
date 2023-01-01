cimport numpy as np


##########################
####    PRIMITIVES    ####
##########################


cdef class AliasInfo:
    cdef readonly:
        type base
        dict defaults


cdef class CacheValue:
    cdef readonly:
        object value
        long long hash


cdef class BaseType:
    pass


###########################
####    ATOMIC TYPE    ####
###########################


cdef class AtomicTypeRegistry:
    cdef:
        list atomic_types
        long long int hash
        CacheValue _aliases
        CacheValue _regex
        CacheValue _resolvable

    cdef int validate_aliases(self, type subclass) except -1
    cdef int validate_kwargs(self, type subclass) except -1
    cdef int validate_name(self, type subclass) except -1
    cdef int validate_slugify(self, type subclass) except -1
    cdef void update_hash(self)


cdef class AtomicType(BaseType):
    cdef:
        CacheValue _subtypes
        CacheValue _supertype
        bint _is_frozen

    cdef readonly:
        type type_def
        object dtype
        object na_value
        object itemsize
        str slug
        long long hash


cdef class AdapterType(AtomicType):
    cdef readonly:
        AtomicType atomic_type


##############################
####    COMPOSITE TYPE    ####
##############################


cdef class CompositeType(BaseType):
    cdef readonly:
        set atomic_types
        np.ndarray index

    cdef void forget_index(self)