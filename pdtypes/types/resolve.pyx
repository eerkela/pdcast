"""Dynamic resolution of AtomicType/CompositeType specifiers.

Exports only a single function `resolve_type()`, which can parse type
specifiers of any of the following types:
    * numpy/pandas dtype objects.
    * raw type definitions (e.g. `int`, `float`, etc.).
    * strings that conform to the type resolution mini-language.
    * other AtomicType/CompositeType objects.

Custom aliases can be added or removed from the pool recognized by this
function through the `register_alias()`, `remove_alias()`, and
`clear_aliases()` classmethods attached to each respective AtomicType
definition.
"""
import regex as re  # alternate python regex engine
from typing import Iterable, Iterator, Union

cimport numpy as np
import numpy as np
import pandas as pd

from pdtypes.type_hints import type_specifier

import pdtypes.types.atomic as atomic
cimport pdtypes.types.atomic as atomic


# TODO: resolve_type currently can't interpret strings of the form:
# "sparse[sparse[bool[numpy]]]"

# TODO: check for fixed-length string type specifier?  "U32", "U64", etc.


#####################
####   PUBLIC    ####
#####################


def resolve_type(
    typespec: type_specifier | Iterable[type_specifier]
) -> atomic.BaseType:
    """Interpret a type specifier, returning a corresponding type object.

    .. note: the instances returned by this function are *flyweights*, meaning
    that repeated input will simply yield a reference to the first object
    created by this function, rather than distinct copies.  Practically,
    this means that only one instance of each type will ever exist at one time,
    and any changes made to that object will propagate wherever it is
    referenced.  Such objects are designed to be immutable for this reason,
    but the behavior should be noted nonetheless.
    """
    if isinstance(typespec, atomic.BaseType):
        result = typespec
    elif isinstance(typespec, str):
        result = resolve_typespec_string(typespec)
    elif isinstance(typespec, type):
        result = resolve_typespec_type(typespec)
    elif isinstance(typespec, (np.dtype, pd.api.extensions.ExtensionDtype)):
        result = resolve_typespec_dtype(typespec)
    elif hasattr(typespec, "__iter__"):
        result = atomic.CompositeType(resolve_type(x) for x in typespec)
    else:
        raise ValueError(
            f"could not resolve specifier of type {type(typespec)}"
        )
    return result


#######################
####    PRIVATE    ####
#######################


# strings associated with every possible missing value (for scalar parsing)
cdef dict na_strings = {
    str(pd.NA): pd.NA,
    str(pd.NaT): pd.NaT,
    str(np.nan): np.nan,
}


cdef str nested(str opener, str closer, str name):
    """Produce a regex pattern to match nested sequences with the specified
    opening and closing characters.  Relies on recursive expressions, which 
    are enabled by the alternate python `regex` package in the same syntax as
    the PCRE engine used in many C applications.
    """
    opener = re.escape(opener)
    closer = re.escape(closer)
    return (
        rf"(?P<{name}>{opener}"
        rf"(?P<content>([^{opener}{closer}]|(?&{name}))*)"
        rf"{closer})"
    )


cdef str parens = nested("(", ")", "parens")
cdef str brackets = nested("[", "]", "brackets")
cdef str curlies = nested("{", "}", "curlies")  # TODO: broken
cdef str call = rf"(?P<name>[^,]*)({parens}|{brackets}|{curlies})"
cdef object tokenize_regex = re.compile(rf"{call}|[^,]+")


cdef list tokenize(str input_str):
    """Split a comma-separated input string into individual tokens, respecting
    nested sequences and callable invocations.
    """
    return [x.group().strip() for x in tokenize_regex.finditer(input_str)]


cdef atomic.BaseType resolve_typespec_string(str input_str):
    """Resolve a string-based type specifier, returning a corresponding
    AtomicType.
    """
    # strip leading, trailing whitespace from input
    input_str = input_str.strip()

    # retrieve alias/regex registry
    registry = atomic.AtomicType.registry

    # ensure input consists only of resolvable type specifiers
    valid = registry.resolvable.fullmatch(input_str)
    if not valid:
        raise ValueError(
            f"could not interpret type specifier: {repr(input_str)}"
        )

    # parse every type specifier contained in the body of the input string
    result = set()
    input_str = valid.group("body")
    matches = [x.groupdict() for x in registry.regex.finditer(input_str)]
    for m in matches:
        base = registry.aliases[m["type"]]

        # if no args are provided, use the default kwargs
        if not m["args"]:  # empty string or None
            instance = base.instance()

        # tokenize args and pass to base.resolve()
        else:
            instance = base.resolve(*tokenize(m["args"]))

        # add to result set
        result.add(instance)

    # return either as single AtomicType or as CompositeType with 2+ types
    if len(result) == 1:
        return result.pop()
    return atomic.CompositeType(result)


cdef atomic.AtomicType resolve_typespec_dtype(object input_dtype):
    """Resolve a numpy/pandas dtype object, returning a corresponding
    AtomicType.
    """
    cdef atomic.TypeRegistry registry = atomic.AtomicType.registry
    cdef dict sparse = None
    cdef dict categorical = None
    cdef str unit
    cdef int step_size
    cdef atomic.ScalarType result

    # pandas special cases (sparse/categorical/DatetimeTZ)
    if isinstance(input_dtype, pd.api.extensions.ExtensionDtype):
        if isinstance(input_dtype, pd.SparseDtype):
            sparse = {"fill_value": input_dtype.fill_value}
            input_dtype = input_dtype.subtype
        if isinstance(input_dtype, pd.CategoricalDtype):
            categorical = {"levels": frozenset(input_dtype.categories)}
            input_dtype = input_dtype.categories.dtype
        if isinstance(input_dtype, pd.DatetimeTZDtype):
            result = atomic.PandasTimestampType.instance(tz=input_dtype.tz)

    # numpy special cases (M8/m8/U)
    if result is None and isinstance(input_dtype, np.dtype):
        if np.issubdtype(input_dtype, "M8"):
            unit, step_size = np.datetime_data(input_dtype)
            result = atomic.NumpyDatetime64Type.instance(
                unit=None if unit == "generic" else unit,
                step_size=step_size
            )
        elif np.issubdtype(input_dtype, "m8"):
            unit, step_size = np.datetime_data(input_dtype)
            result = atomic.NumpyTimedelta64Type.instance(
                unit=None if unit == "generic" else unit,
                step_size=step_size
            )
        elif np.issubdtype(input_dtype, "U"):
            result = atomic.StringType.instance()

    # general case
    if result is None:
        result = registry.aliases[input_dtype].instance()

    # re-wrap with sparse/categorical
    if categorical:
        result = atomic.CategoricalType(result, **categorical)
    if sparse:
        result = atomic.SparseType(result, **sparse)
    return result


cdef atomic.AtomicType resolve_typespec_type(type input_type):
    """Resolve a runtime type definition, returning a corresponding AtomicType.
    """
    cdef dict aliases = atomic.AtomicType.registry.aliases

    if input_type in aliases:
        return aliases[input_type].instance()

    return atomic.ObjectType.instance(base=input_type)
