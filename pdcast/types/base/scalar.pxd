from .registry cimport Type, AliasManager


cdef Exception READ_ONLY_ERROR


cdef class ScalarType(Type):
    cdef:
        dict _kwargs
        str _slug
        long long _hash
        bint _read_only

    cdef readonly:
        ArgumentEncoder encoder
        InstanceFactory instances

    @staticmethod
    cdef void set_encoder(type cls, ArgumentEncoder encoder)

    @staticmethod
    cdef void set_instances(type cls, InstanceFactory instances)

    cdef void init_base(self)
    cdef void init_parametrized(self)


cdef class ArgumentEncoder:
    cdef:
        str name
        tuple parameters
        dict defaults

    cdef void set_name(self, str name)
    cdef void set_kwargs(self, dict kwargs)


cdef class BackendEncoder(ArgumentEncoder):
    cdef:
        str backend


cdef class InstanceFactory:
    cdef:
        type base_class


cdef class FlyweightFactory(InstanceFactory):
    cdef:
        dict instances
        ArgumentEncoder encoder
