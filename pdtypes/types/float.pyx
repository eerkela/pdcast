import numpy as np
cimport numpy as np
import pandas as pd

from .base cimport CompositeType, compute_hash, ElementType, shared_registry
from .complex cimport (
    ComplexType, Complex64Type, Complex128Type, CLongDoubleType
)


##########################
####    SUPERTYPES    ####
##########################


cdef class FloatType(ElementType):
    """Float supertype"""

    def __init__(
        self,
        bint sparse = False,
        bint categorical = False
    ):
        super(FloatType, self).__init__(
            sparse=sparse,
            categorical=categorical,
            nullable=True,
            atomic_type=float,
            numpy_type=np.dtype(np.float64),
            pandas_type=None,
            slug="float",
            supertype=None,
            subtypes=None  # lazy-loaded
        )

        # hash
        self.hash = compute_hash(
            sparse=sparse,
            categorical=categorical,
            nullable=True,
            base=self.__class__
        )

        # min/max representable integer (determined by size of significand)
        self.min = -2**53
        self.max = 2**53

    @property
    def equiv_complex(self) -> ComplexType:
        """Add an imaginary component to this ElementType."""
        return ComplexType.instance(
            sparse=self.sparse,
            categorical=self.categorical
        )

    @property
    def subtypes(self) -> CompositeType:
        # cached
        if self._subtypes is not None:
            return self._subtypes

        # uncached
        subtypes = {self} | {
            t.instance(sparse=self.sparse, categorical=self.categorical)
            for t in (Float16Type, Float32Type, Float64Type, LongDoubleType)
        }
        self._subtypes = CompositeType(subtypes, immutable=True)
        return self._subtypes


########################
####    SUBTYPES    ####
########################


cdef class Float16Type(FloatType):
    """16-bit float subtype"""

    def __init__(
        self,
        bint sparse = False,
        bint categorical = False
    ):
        super(FloatType, self).__init__(
            sparse=sparse,
            categorical=categorical,
            nullable=True,
            atomic_type=np.float16,
            numpy_type=np.dtype(np.float16),
            pandas_type=None,
            slug="float16",
            supertype=None,  # lazy-loaded
            subtypes=CompositeType({self}, immutable=True)
        )

        # hash
        self.hash = compute_hash(
            sparse=sparse,
            categorical=categorical,
            nullable=True,
            base=self.__class__
        )

        # min/max representable integer (determined by size of significand)
        self.min = -2**11
        self.max = 2**11

    @property
    def equiv_complex(self) -> Complex64Type:
        """Add an imaginary component to this ElementType."""
        return Complex64Type.instance(
            sparse=self.sparse,
            categorical=self.categorical
        )

    @property
    def supertype(self) -> FloatType:
        # cached
        if self._supertype is not None:
            return self._supertype

        self._supertype = FloatType.instance(
            sparse=self.sparse,
            categorical=self.categorical
        )
        return self._supertype


cdef class Float32Type(FloatType):
    """32-bit float subtype"""

    def __init__(
        self,
        bint sparse = False,
        bint categorical = False
    ):
        super(FloatType, self).__init__(
            sparse=sparse,
            categorical=categorical,
            nullable=True,
            atomic_type=np.float32,
            numpy_type=np.dtype(np.float32),
            pandas_type=None,
            slug="float32",
            supertype=None,  # lazy-loaded
            subtypes=CompositeType({self}, immutable=True)
        )

        # hash
        self.hash = compute_hash(
            sparse=sparse,
            categorical=categorical,
            nullable=True,
            base=self.__class__
        )

        # min/max representable integer (determined by size of significand)
        self.min = -2**24
        self.max = 2**24

    @property
    def equiv_complex(self) -> Complex64Type:
        """Add an imaginary component to this ElementType."""
        return Complex64Type.instance(
            sparse=self.sparse,
            categorical=self.categorical
        )

    @property
    def supertype(self) -> FloatType:
        # cached
        if self._supertype is not None:
            return self._supertype

        self._supertype = FloatType.instance(
            sparse=self.sparse,
            categorical=self.categorical
        )
        return self._supertype


cdef class Float64Type(FloatType):
    """64-bit float subtype"""

    def __init__(
        self,
        bint sparse = False,
        bint categorical = False
    ):
        super(FloatType, self).__init__(
            sparse=sparse,
            categorical=categorical,
            nullable=True,
            atomic_type=np.float64,
            numpy_type=np.dtype(np.float64),
            pandas_type=None,
            slug="float64",
            supertype=None,  # lazy-loaded
            subtypes=CompositeType({self}, immutable=True)
        )

        # hash
        self.hash = compute_hash(
            sparse=sparse,
            categorical=categorical,
            nullable=True,
            base=self.__class__
        )

        # min/max representable integer (determined by size of significand)
        self.min = -2**53
        self.max = 2**53

    @property
    def equiv_complex(self) -> Complex128Type:
        """Add an imaginary component to this ElementType."""
        return Complex128Type.instance(
            sparse=self.sparse,
            categorical=self.categorical
        )

    @property
    def supertype(self) -> FloatType:
        # cached
        if self._supertype is not None:
            return self._supertype

        self._supertype = FloatType.instance(
            sparse=self.sparse,
            categorical=self.categorical
        )
        return self._supertype


cdef class LongDoubleType(FloatType):
    """Long double float subtype (platform-dependent)"""

    def __init__(
        self,
        bint sparse = False,
        bint categorical = False
    ):
        super(FloatType, self).__init__(
            sparse=sparse,
            categorical=categorical,
            nullable=True,
            atomic_type=np.longdouble,
            numpy_type=np.dtype(np.longdouble),
            pandas_type=None,
            slug="longdouble",
            supertype=None,  # lazy-loaded
            subtypes=CompositeType({self}, immutable=True)
        )

        # hash
        self.hash = compute_hash(
            sparse=sparse,
            categorical=categorical,
            nullable=True,
            base=self.__class__
        )

        # min/max representable integer (determined by size of significand)
        self.min = -2**64
        self.max = 2**64

    @property
    def equiv_complex(self) -> CLongDoubleType:
        """Add an imaginary component to this ElementType."""
        return CLongDoubleType.instance(
            sparse=self.sparse,
            categorical=self.categorical
        )

    @property
    def supertype(self) -> FloatType:
        # cached
        if self._supertype is not None:
            return self._supertype

        self._supertype = FloatType.instance(
            sparse=self.sparse,
            categorical=self.categorical
        )
        return self._supertype
