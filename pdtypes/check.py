from __future__ import annotations
import datetime
import decimal
from typing import Union

import numpy as np
import pandas as pd

from pdtypes.error import error_trace
from pdtypes.util.array import array_like, object_types, vectorize


dtype_like = Union[type, str, np.dtype, pd.api.extensions.ExtensionDtype]
_extension_types = {
    bool: pd.BooleanDtype(),
    np.int8: pd.Int8Dtype(),
    np.int16: pd.Int16Dtype(),
    np.int32: pd.Int32Dtype(),
    np.int64: pd.Int64Dtype(),
    np.uint8: pd.UInt8Dtype(),
    np.uint16: pd.UInt16Dtype(),
    np.uint32: pd.UInt32Dtype(),
    np.uint64: pd.UInt64Dtype(),
    str: pd.StringDtype(),
    np.dtype(bool): pd.BooleanDtype(),
    np.dtype(np.int8): pd.Int8Dtype(),
    np.dtype(np.int16): pd.Int16Dtype(),
    np.dtype(np.int32): pd.Int32Dtype(),
    np.dtype(np.int64): pd.Int64Dtype(),
    np.dtype(np.uint8): pd.UInt8Dtype(),
    np.dtype(np.uint16): pd.UInt16Dtype(),
    np.dtype(np.uint32): pd.UInt32Dtype(),
    np.dtype(np.uint64): pd.UInt64Dtype()
}


_dtype_to_atomic_type = {
    np.dtype(bool): bool,
    pd.BooleanDtype(): bool,
    np.dtype(np.int8): np.int8,
    pd.Int8Dtype(): np.int8,
    np.dtype(np.int16): np.int16,
    pd.Int16Dtype(): np.int16,
    np.dtype(np.int32): np.int32,
    pd.Int32Dtype(): np.int32,
    np.dtype(np.int64): np.int64,
    pd.Int64Dtype(): np.int64,
    np.dtype(np.uint8): np.uint8,
    pd.UInt8Dtype(): np.uint8,
    np.dtype(np.uint16): np.uint16,
    pd.UInt16Dtype(): np.uint16,
    np.dtype(np.uint32): np.uint32,
    pd.UInt32Dtype(): np.uint32,
    np.dtype(np.uint64): np.uint64,
    pd.UInt64Dtype(): np.uint64,
    np.dtype(np.float16): np.float16,
    np.dtype(np.float32): np.float32,
    np.dtype(np.float64): np.float64,
    np.dtype(np.longdouble): np.longdouble,
    np.dtype(np.complex64): np.complex64,
    np.dtype(np.complex128): np.complex128,
    np.dtype(np.clongdouble): np.clongdouble,
    np.dtype("M8[ns]"): pd.Timestamp,
    np.dtype("<M8[ns]"): pd.Timestamp,
    np.dtype(">M8[ns]"): pd.Timestamp,
    np.dtype("m8[ns]"): pd.Timedelta,
    np.dtype(">m8[ns]"): pd.Timedelta,
    np.dtype("<m8[ns]"): pd.Timedelta,
    pd.StringDtype(): str
}


def is_integer_series(series: int | list | np.ndarray | pd.Series) -> bool:
    """Check if a series contains integer data in any form."""
    # vectorize
    series = pd.Series(series)

    # option 1: series is properly formatted -> check dtype directly
    if pd.api.types.is_integer_dtype(series):
        return True

    # series has object dtype -> attempt to infer
    if pd.api.types.is_object_dtype(series):
        return pd.api.types.infer_dtype(series) == "integer"

    # series has some other dtype -> not integer
    return False


def get_dtype(series: int | list | np.ndarray | pd.Series) -> np.ndarray:
    """Get the types of elements present in the given series."""
    # TODO: fix type annotations
    # vectorize
    series = pd.Series(series).infer_objects()

    # check if series has object dtype
    if pd.api.types.is_object_dtype(series):
        type_ufunc = np.frompyfunc(type, 1, 1)
        if series.hasnans:  # do not count missing values
            series = series[series.notna()]
        return type_ufunc(series).unique()

    # series does not have object dtype
    atomic_type = _dtype_to_atomic_type.get(series.dtype, series.dtype)
    return np.array([atomic_type])






# TODO: allow is_x to accept dtype_like args as well
# TODO: allow multiple comparison



_type_aliases = {
    # booleans
    np.dtype(bool): bool,
    pd.BooleanDtype(): pd.BooleanDtype(),

    # integers
    int: int,
    "int": int,
    "integer": int,
    "i": int,
    np.dtype("i1"): np.int8,
    np.dtype("i2"): np.int16,
    np.dtype("i4"): np.int32,
    np.dtype("i8"): np.int64,
    np.dtype("u1"): np.uint8,
    np.dtype("u2"): np.uint16,
    np.dtype("u4"): np.uint32,
    np.dtype("u8"): np.uint64,
    pd.Int8Dtype(): pd.Int8Dtype(),
    pd.Int16Dtype(): pd.Int16Dtype(),
    pd.Int32Dtype(): pd.Int32Dtype(),
    pd.Int64Dtype(): pd.Int64Dtype(),
    pd.UInt8Dtype(): pd.UInt8Dtype(),
    pd.UInt16Dtype(): pd.UInt16Dtype(),
    pd.UInt32Dtype(): pd.UInt32Dtype(),
    pd.UInt64Dtype(): pd.UInt64Dtype(),

    # floats
    float: float,
    "float": float,
    "floating": float,
    "f": float,
    np.dtype("f2"): np.float16,
    np.dtype("f4"): np.float32,
    np.dtype("f8"): np.float64,
    np.dtype(np.longdouble): np.longdouble,  # platform-specific

    # complex numbers
    complex: complex,
    "complex": complex,
    "c": complex,
    np.dtype("c8"): np.complex64,
    np.dtype("c16"): np.complex128,
    np.dtype(np.clongdouble): np.clongdouble,  # platform-specific

    # decimals
    decimal.Decimal: decimal.Decimal,
    "decimal": decimal.Decimal,
    "decimal.decimal": decimal.Decimal,
    "d": decimal.Decimal,

    # datetimes
    "datetime": "datetime",
    pd.Timestamp: pd.Timestamp,
    "pd.timestamp": pd.Timestamp,
    "pandas.timestamp": pd.Timestamp,
    datetime.datetime: datetime.datetime,
    "pydatetime": datetime.datetime,
    "datetime.datetime": datetime.datetime,
    np.datetime64: np.datetime64,
    np.dtype(np.datetime64): np.datetime64,
    "np.datetime64": np.datetime64,
    "numpy.datetime64": np.datetime64,

    # timedeltas
    "timedelta": "timedelta",
    pd.Timedelta: pd.Timedelta,
    "pd.timedelta": pd.Timedelta,
    "pandas.timedelta": pd.Timedelta,
    datetime.timedelta: datetime.timedelta,
    "pytimedelta": datetime.timedelta,
    "datetime.timedelta": datetime.timedelta,
    np.timedelta64: np.timedelta64,
    np.dtype(np.timedelta64): np.timedelta64,
    "np.timedelta64": np.timedelta64,
    "numpy.timedelta64": np.timedelta64,

    # objects
    np.dtype(object): object,
    "obj": object,

    # strings
    np.dtype(str): str,
    pd.StringDtype(): pd.StringDtype(),

    # categorical
    pd.Categorical: pd.Categorical,
    pd.CategoricalDtype(): pd.Categorical,
    "categorical": pd.Categorical,
    "cat": pd.Categorical
}


def resolve_dtype(
    dtype: dtype_like,
    allow_extensions: bool = True
) -> type | str | pd.api.extensions.ExtensionDtype:
    """Collapse abstract dtype aliases into their corresponding atomic type."""
    # TODO: rename to resolve_dtype
    if isinstance(dtype, str) and dtype.lower() in _type_aliases:
        return _type_aliases[dtype.lower()]

    # get preliminary result
    try_lookup = lambda: _type_aliases[pd.api.types.pandas_dtype(dtype)]
    dtype = _type_aliases.get(dtype, try_lookup())

    # if result is an extension type, apply collapse rule
    if not allow_extensions and pd.api.types.is_extension_array_dtype(dtype):
        return _type_aliases[dtype.numpy_dtype]
    return dtype


#######################
####   Booleans    ####
#######################


def is_boolean(
    array: bool | array_like,
    dtype: dtype_like = bool,
    exact: bool = False
) -> bool:
    """Efficiently check whether a given scalar/sequence/array/series contains
    boolean values.

    If `exact=True`, this will return `True` if and only if the array would be
    best represented by the given dtype, taking into account the presence of
    missing values.  If missing values are detected in the array, this will
    force a standard `bool` dtype comparison to return `False`, since standard
    booleans are not nullable by default.  In this case, one should use the
    `pandas.BooleanDtype()` extension dtype instead, which can fully
    encapsulate the array.  Conversely, if the array contains no missing
    values, the inverse is true, causing a `bool` comparison to return `True`,
    and a `pandas.BooleanDtype()` comparison to return `False`.
    """
    # vectorize scalars and sequences and note missing values
    array = vectorize(array)
    nans = pd.isna(array)

    # collapse dtype aliases and note whether given dtype is extension type
    dtype = resolve_dtype(dtype)
    is_extension = pd.api.types.is_extension_array_dtype(dtype)

    # if exact, check that is_extension matches hasnans
    if exact and is_extension ^ nans.any():
        return False

    # collapse to atomic type and disregard missing values
    if is_extension:
        dtype = resolve_dtype(dtype.numpy_dtype)
    array = array[~nans]

    # option 1: array is properly initialized boolean dtype
    if pd.api.types.is_bool_dtype(array):
        return True

    # option 2: array has dtype="O", but contains boolean objects
    if pd.api.types.is_object_dtype(array):
        types = pd.unique(object_types(array))
        return len(types) == 1 and issubclass(types[0], dtype)

    return False


########################
####    Integers    ####
########################


def is_integer(
    array: int | array_like,
    dtype: dtype_like = int,
    exact: bool = False
) -> bool:
    """Efficiently check whether a given scalar, sequence, array, or series
    contains integers of the specified dtype.

    If `exact=True`, this will return `True` if and only if the array would be
    best represented by the given dtype, taking into account the presence of
    missing values.  If missing values are detected in the array, this will
    force a numpy dtype comparison to return `False`, since numpy integers are
    not nullable by default.  In this case, one should use a pandas extension
    dtype instead, which can fully encapsulate the array.  Conversely, if an
    array contains no missing values, the inverse is true, causing a numpy
    dtype comparison to return `True`, and a pandas extension dtype comparison
    to return `False`.

    When `exact=True`, every integer dtype is considered to be orthogonal to
    every other integer dtype, meaning that every possible input array has
    exactly one unique solution to this function, provided that all of its
    elements are integers, and are identically typed.

    `Exact = False`:
           +------+------+------+------+------+------+------+------+------+
           | int  | 'i1' | 'i2' | 'i4' | 'i8' | 'u1' | 'u2' | 'u4' | 'u8' |
    +------+------+------+------+------+------+------+------+------+------+
    | int  |  T   |  T*  |  T*  |  T*  |  T*  |  T*  |  T*  |  T*  |  T*  |
    |------+------+------+------+------+------+------+------+------+------+
    | 'i1' |  -   |  T** |  -   |  -   |  -   |  -   |  -   |  -   |  -   |
    |------+------+------+------+------+------+------+------+------+------+
    | 'i2' |  -   |  -   |  T** |  -   |  -   |  -   |  -   |  -   |  -   |
    |------+------+------+------+------+------+------+------+------+------+
    | 'i4' |  -   |  -   |  -   |  T** |  -   |  -   |  -   |  -   |  -   |
    |------+------+------+------+------+------+------+------+------+------+
    | 'i8' |  -   |  -   |  -   |  -   |  T** |  -   |  -   |  -   |  -   |
    |------+------+------+------+------+------+------+------+------+------+
    | 'u1' |  -   |  -   |  -   |  -   |  -   |  T** |  -   |  -   |  -   |
    |------+------+------+------+------+------+------+------+------+------+
    | 'u2' |  -   |  -   |  -   |  -   |  -   |  -   |  T** |  -   |  -   |
    |------+------+------+------+------+------+------+------+------+------+
    | 'u4' |  -   |  -   |  -   |  -   |  -   |  -   |  -   |  T** |  -   |
    |------+------+------+------+------+------+------+------+------+------+
    | 'u8' |  -   |  -   |  -   |  -   |  -   |  -   |  -   |  -   |  T** |
    +------+------+------+------+------+------+------+------+------+------+
    (rows: test type, columns: underlying element type)

    * indicates values that become `False` when `exact=True`.
    ** indicates values that become `False` when both `exact=True` and missing
    values are present in the array.  Use the equivalent extension dtypes
    for these values.
    """
    # vectorize scalars and sequences and note missing values
    array = vectorize(array)
    nans = pd.isna(array)

    # collapse dtype aliases and note whether given dtype is extension type
    dtype = resolve_dtype(dtype)
    is_extension = pd.api.types.is_extension_array_dtype(dtype)

    # if exact, check that is_extension matches hasnans
    if exact and dtype != int and is_extension ^ nans.any():
        return False

    # collapse to atomic type and disregard missing values
    if is_extension:
        dtype = resolve_dtype(dtype.numpy_dtype)
    array = array[~nans]

    # case 1: array is properly initialized, with integer dtype
    if pd.api.types.is_integer_dtype(array):
        # option 1: dtype is integer supertype
        if dtype == int:
            return not exact  # array cannot contain exactly python ints

        # option 2: array is extension dtype
        if pd.api.types.is_extension_array_dtype(array):
            return resolve_dtype(array.dtype.numpy_dtype) == dtype

        # option 3: array has ordinary numpy dtype
        return np.issubdtype(array.dtype, dtype)

    # case 2: array has dtype="O", but contains numpy integers
    if pd.api.types.is_object_dtype(array):
        types = pd.unique(object_types(array))

        # option 1: dtype is integer supertype
        if dtype == int and not exact:
            # test array contains integers of any kind and nothing else
            return all(np.issubdtype(t, np.integer) for t in types)

        # option 2: dtype is integer subtype
        return len(types) == 1 and issubclass(types[0], dtype)

    return False


######################
####    Floats    ####
######################


def is_float(
    array: float | array_like,
    dtype: dtype_like = float,
    exact: bool = False
) -> bool:
    """Efficiently check whether a given scalar/sequence/array/series contains
    floating point numbers of the specified dtype.

    TODO: expand documentaion to match is_integer
    """
    # vectorize scalars and sequences
    array = vectorize(array)

    # collapse dtype aliases
    dtype = resolve_dtype(dtype)

    # case 1: array is properly initialized, with float dtype
    if pd.api.types.is_float_dtype(array):
        # option 1: dtype is float supertype
        if dtype == float:
            return not exact  # array cannot contain exactly python floats

        # option 2: dtype is float subtype
        return np.issubdtype(array.dtype, dtype)

    # case 2: array has dtype="O", but contains floats
    if pd.api.types.is_object_dtype(array):
        array = array[pd.notna(array)]
        types = pd.unique(object_types(array))

        # option 1: dtype is float supertype
        if dtype == float and not exact:
            # test array contains floats of any kind and nothing else
            return all(np.issubdtype(t, np.floating) for t in types)

        # option 2: dtype is float subtype
        return len(types) == 1 and issubclass(types[0], dtype)

    return False


#########################
####    Datetimes    ####
#########################


def is_pandas_timestamp(array: pd.Timestamp | array_like) -> bool:
    """Efficiently check whether a given scalar/sequence/array/series contains
    `pandas.Timestamp` objects.
    """
    # vectorize scalars and sequences
    array = vectorize(array)

    # option 1: array is properly initialized pd.Timestamp series
    if (isinstance(array, pd.Series) and
        pd.api.types.is_datetime64_ns_dtype(array)):
        return True

    # option 2: array has dtype="O", but contains pd.Timestamp objects
    if pd.api.types.is_object_dtype(array):
        array = array[pd.notna(array)]
        types = pd.unique(object_types(array))
        return len(types) == 1 and issubclass(types[0], pd.Timestamp)

    return False


def is_pydatetime(array: datetime.datetime | array_like) -> bool:
    """Efficiently check whether a given scalar/sequence/array/series contains
    `datetime.datetime` objects.
    """
    # vectorize scalars and sequences
    array = vectorize(array)

    # array has dtype="O" and contains datetime.datetime objects
    if pd.api.types.is_object_dtype(array):
        array = array[pd.notna(array)]
        types = pd.unique(object_types(array))
        return len(types) == 1 and issubclass(types[0], datetime.datetime)

    return False


def is_numpy_datetime64(array: np.datetime64 | array_like) -> bool:
    """Efficiently check whether a given scalar/sequence/array/series contains
    `numpy.datetime64` objects.
    """
    # vectorize scalars and sequences
    array = vectorize(array)

    # option 1: array is properly intitialized M8 array
    if isinstance(array, np.ndarray) and np.issubdtype(array.dtype, "M8"):
        return True

    # option 2: array has dtype="O", but contains M8 objects
    if pd.api.types.is_object_dtype(array):
        array = array[pd.notna(array)]
        types = pd.unique(object_types(array))
        return len(types) == 1 and issubclass(types[0], np.datetime64)

    return False


def is_datetime(
    array: pd.Timestamp | datetime.datetime | np.datetime64 | array_like,
    dtype: dtype_like = "datetime",
    exact: bool = False
) -> bool:
    """Efficiently check whether a given scalar/sequence/array/series contains
    datetime objects of any type.
    """
    # vectorize scalars and sequences
    array = vectorize(array)

    # collapse dtype aliases
    dtype = resolve_dtype(dtype)

    # TODO: treat like is_integer, is_float, etc

    # option 1: array/series is properly initialized and has datetime64 dtype
    if pd.api.types.is_datetime64_any_dtype(array):
        return True

    # option 2: array/series has dtype="O", but contains datetime objects
    if pd.api.types.is_object_dtype(array):
        array = array[pd.notna(array)]
        types = pd.unique(object_types(array))
        valid = (pd.Timestamp, datetime.datetime, np.datetime64)
        return np.isin(types, valid).all()

    return False


##########################
####    Timedeltas    ####
##########################


def is_pandas_timedelta(array: pd.Timedelta | array_like) -> bool:
    """Efficiently check whether a given scalar/sequence/array/series contains
    `pandas.Timedelta` objects.
    """
    # vectorize scalars and sequences
    array = vectorize(array)

    # option 1: array is properly initialized pd.Timedelta series
    if (isinstance(array, pd.Series) and
        pd.api.types.is_timedelta64_ns_dtype(array)):
        return True

    # option 2: array has dtype="O", but contains pd.Timedelta objects
    if pd.api.types.is_object_dtype(array):
        array = array[pd.notna(array)]
        types = pd.unique(object_types(array))
        return len(types) == 1 and issubclass(types[0], pd.Timedelta)

    return False


def is_pytimedelta(array: datetime.timedelta | array_like) -> bool:
    """Efficiently check whether a given scalar/sequence/array/series contains
    `datetime.timedelta` objects.
    """
    # vectorize scalars and sequences
    array = vectorize(array)

    # array has dtype="O" and contains datetime.datetime objects
    if pd.api.types.is_object_dtype(array):
        array = array[pd.notna(array)]
        types = pd.unique(object_types(array))
        return len(types) == 1 and issubclass(types[0], datetime.timedelta)

    return False


def is_numpy_timedelta64(array: np.timedelta64 | array_like) -> bool:
    """Efficiently check whether a given scalar/sequence/array/series contains
    `numpy.datetime64` objects.
    """
    # vectorize scalars and sequences
    array = vectorize(array)

    # option 1: array is properly intitialized m8 array
    if isinstance(array, np.ndarray) and np.issubdtype(array.dtype, "m8"):
        return True

    # option 2: array has dtype="O", but contains m8 objects
    if pd.api.types.is_object_dtype(array):
        array = array[pd.notna(array)]
        types = pd.unique(object_types(array))
        return len(types) == 1 and issubclass(types[0], np.timedelta64)

    return False


def is_timedelta(
    array: pd.Timestamp | datetime.datetime | np.datetime64 | array_like
) -> bool:
    """Efficiently check whether a given scalar/sequence/array/series contains
    datetime objects of any type.
    """
    # vectorize scalars and sequences
    array = vectorize(array)

    # option 1: array/series is properly initialized and has timedelta64 dtype
    if pd.api.types.is_timedelta64_dtype(array):
        return True

    # option 2: array/series has dtype="O", but contains datetime objects
    if pd.api.types.is_object_dtype(array):
        array = array[pd.notna(array)]
        types = pd.unique(object_types(array))
        valid = (pd.Timedelta, datetime.timedelta, np.timedelta64)
        return np.isin(types, valid).all()

    return False
