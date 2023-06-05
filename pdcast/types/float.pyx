"""This module contains all the prepackaged float types for the ``pdcast``
type system.
"""
import decimal
import sys

import numpy as np
cimport numpy as np

from pdcast.util.type_hints import numeric

from .base cimport ScalarType, AbstractType, CompositeType
from .base import register
import pdcast.types.complex as complex_types


# pdcast.Float32Type < pdcast.Float64Type -> decimal.InvalidOperation


##################################
####    MIXINS & CONSTANTS    ####
##################################


# NOTE: x86 extended precision floating point (long double) is
# platform-specific and may not be exposed depending on hardware configuration.
cdef bint has_longdouble = (np.dtype(np.longdouble).itemsize > 8)


class FloatMixin:

    is_numeric = True

    ############################
    ####    TYPE METHODS    ####
    ############################

    @property
    def equiv_complex(self) -> ScalarType:
        c_root = complex_types.ComplexType()
        candidates = [x for y in c_root.backends.values() for x in y.subtypes]
        for x in candidates:
            if type(x).__name__ == self._equiv_complex:
                return x
        raise TypeError(f"{repr(self)} has no equivalent complex type")


##########################
####    ROOT FLOAT    ####
##########################


@register
class FloatType(AbstractType):
    """Generic float supertype"""

    name = "float"
    aliases = {"float", "floating", "f"}
    _equiv_complex = "ComplexType"


@register
@FloatType.implementation("numpy")
class NumpyFloatType(AbstractType):

    aliases = {np.floating}
    _equiv_complex = "NumpyComplexType"


#######################
####    FLOAT16    ####
#######################


@register
@FloatType.subtype
class Float16Type(AbstractType):

    name = "float16"
    aliases = {"float16", "half", "f2", "e"}
    _equiv_complex = "Complex64Type"


@register
@NumpyFloatType.subtype
@Float16Type.default
@Float16Type.implementation("numpy")
class NumpyFloat16Type(FloatMixin, ScalarType):

    aliases = {np.float16, np.dtype(np.float16)}
    dtype = np.dtype(np.float16)
    itemsize = 2
    na_value = np.nan
    type_def = np.float16
    max = 2**11
    min = -2**11
    _equiv_complex = "NumpyComplex64Type"


#######################
####    FLOAT32    ####
#######################


@register
@FloatType.subtype
class Float32Type(AbstractType):

    name = "float32"
    aliases = {"float32", "single", "f4"}
    _equiv_complex = "Complex64Type"


@register
@NumpyFloatType.subtype
@Float32Type.default
@Float32Type.implementation("numpy")
class NumpyFloat32Type(FloatMixin, ScalarType):

    aliases = {np.float32, np.dtype(np.float32)}
    dtype = np.dtype(np.float32)
    itemsize = 4
    na_value = np.nan
    type_def = np.float32
    max = 2**24
    min = -2**24
    _equiv_complex = "NumpyComplex64Type"


#######################
####    FLOAT64    ####
#######################


@register
@FloatType.default
@FloatType.subtype
class Float64Type(FloatMixin, AbstractType):

    name = "float64"
    aliases = {"float64", "double", "float_", "f8", "d"}
    _equiv_complex = "Complex128Type"


@register
@NumpyFloatType.default
@NumpyFloatType.subtype
@Float64Type.default
@Float64Type.implementation("numpy")
class NumpyFloat64Type(FloatMixin, ScalarType):

    aliases = {np.float64, np.dtype(np.float64)}
    dtype = np.dtype(np.float64)
    itemsize = 8
    na_value = np.nan
    type_def = np.float64
    max = 2**53
    min = -2**53
    _equiv_complex = "NumpyComplex128Type"


@register
@FloatType.implementation("python")
@Float64Type.implementation("python")
class PythonFloatType(FloatMixin, ScalarType):

    aliases = {float}
    itemsize = sys.getsizeof(0.0)
    na_value = np.nan
    type_def = float
    max = 2**53
    min = -2**53
    _equiv_complex = "PythonComplexType"


#################################
####    LONG DOUBLE (x86)    ####
#################################


@register(cond=has_longdouble)
@FloatType.subtype
class Float80Type(FloatMixin, AbstractType):

    name = "float80"
    aliases = {
        "float80", "longdouble", "longfloat", "long double", "long float",
        "f10", "g"
    }
    _equiv_complex = "Complex160Type"


@register(cond=has_longdouble)
@NumpyFloatType.subtype
@Float80Type.default
@Float80Type.implementation("numpy")
class NumpyFloat80Type(FloatMixin, ScalarType):

    aliases = {np.longdouble, np.dtype(np.longdouble)}
    dtype = np.dtype(np.longdouble)
    itemsize = np.dtype(np.longdouble).itemsize
    na_value = np.nan
    type_def = np.longdouble
    max = 2**64
    min = -2**64
    _equiv_complex = "NumpyComplex160Type"
