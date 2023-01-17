import inspect
from typing import Any, Callable

import numpy as np
cimport numpy as np
import pandas as pd

from .base cimport AtomicType, CompositeType
from .base import lru_cache

from pdtypes.error import shorten_list
cimport pdtypes.types.cast as cast
import pdtypes.types.cast as cast
cimport pdtypes.types.resolve as resolve
import pdtypes.types.resolve as resolve


#######################
####    GENERIC    ####
#######################


@lru_cache(64)
class ObjectType(AtomicType):

    name = "object"
    aliases = {"object", "obj", "O", "pyobject", "object_", "object0"}

    def __init__(self, base: type = object):
        super().__init__(
            type_def=base,
            dtype=np.dtype("O"),
            na_value=pd.NA,
            itemsize=None,
            base=base
        )

    #############################
    ####    CLASS METHODS    ####
    #############################

    @classmethod
    def slugify(cls, base: type = object) -> str:
        slug = cls.name
        if base is not object:
            slug += f"[{base.__module__}.{base.__name__}]"
        return slug

    @classmethod
    def resolve(cls, base: str = None) -> AtomicType:
        if base is None:
            return cls.instance()
        return cls.instance(base=from_caller(base))

    #######################
    ####    METHODS    ####
    #######################

    def contains(self, other: Any) -> bool:
        other = resolve.resolve_type(other)

        # treat base=object as wildcard
        if self.base is object:
            if isinstance(other, CompositeType):
                return all(isinstance(o, type(self)) for o in other)
            return isinstance(other, type(self))

        return super().contains(other)

    def to_boolean(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        call: Callable = None,
        errors: str = "raise",
        **unused
    ) -> pd.Series:
        """Convert unstructured objects to a boolean data type."""
        # hook into __bool__() magic method
        if call is None:
            call = lambda x: dtype.type_def(bool(x))

        return two_step_conversion(
            series=series,
            dtype=dtype,
            call=call,
            errors=errors,
            conv_func=cast.to_boolean,
            **unused
        )

    def to_integer(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        call: Callable = None,
        errors: str = "raise",
        **unused
    ) -> pd.Series:
        """Convert unstructured objects to an integer data type."""
        # hook into __int__() magic method
        if call is None:
            call = lambda x: dtype.type_def(int(x))

        return two_step_conversion(
            series=series,
            dtype=dtype,
            call=call,
            errors=errors,
            conv_func=cast.to_integer,
            **unused
        )

    def to_float(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        call: Callable = None,
        errors: str = "raise",
        **unused
    ) -> pd.Series:
        """Convert unstructured objects to a float data type."""
        # hook into __float__() magic method
        if call is None:
            call = lambda x: dtype.type_def(float(x))

        return two_step_conversion(
            series=series,
            dtype=dtype,
            call=call,
            errors=errors,
            conv_func=cast.to_float,
            **unused
        )

    def to_complex(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        call: Callable = None,
        errors: str = "raise",
        **unused
    ) -> pd.Series:
        """Convert unstructured objects to a complex data type."""
        # hook into __complex__() magic method
        if call is None:
            call = lambda x: dtype.type_def(complex(x))

        return two_step_conversion(
            series=series,
            dtype=dtype,
            call=call,
            errors=errors,
            conv_func=cast.to_complex,
            **unused
        )

    def to_decimal(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        call: Callable = None,
        errors: str = "raise",
        **unused
    ) -> pd.Series:
        """Convert unstructured objects to a decimal data type."""
        return two_step_conversion(
            series=series,
            dtype=dtype,
            call=dtype.type_def if call is None else call,
            errors=errors,
            conv_func=cast.to_decimal,
            **unused
        )

    def to_datetime(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        call: Callable = None,
        errors: str = "raise",
        **unused
    ) -> pd.Series:
        """Convert unstructured objects to a datetime data type."""
        return two_step_conversion(
            series=series,
            dtype=dtype,
            call=dtype.type_def if call is None else call,
            errors=errors,
            conv_func=cast.to_datetime,
            **unused
        )

    def to_timedelta(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        call: Callable = None,
        errors: str = "raise",
        **unused
    ) -> pd.Series:
        """Convert unstructured objects to a timedelta data type."""
        return two_step_conversion(
            series=series,
            dtype=dtype,
            call=dtype.type_def if call is None else call,
            errors=errors,
            conv_func=cast.to_timedelta,
            **unused
        )

    def to_string(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        call: Callable = None,
        errors: str = "raise",
        **unused
    ) -> pd.Series:
        """Convert unstructured objects to a string data type."""
        # hook into __str__() magic method
        if call is None:
            call = lambda x: dtype.type_def(str(x))

        return two_step_conversion(
            series=series,
            dtype=dtype,
            call=call,
            errors=errors,
            conv_func=cast.to_string,
            **unused
        )

    def to_object(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        call: Callable = None,
        errors: str = "raise",
        **unused
    ) -> pd.Series:
        """Convert unstructured objects to an object data type."""
        if call is None:
            call = dtype.type_def

        def wrapped_call(object val):
            cdef object result = call(val)
            cdef type output_type = type(result)

            if output_type != dtype.type_def:
                raise ValueError(
                    f"`call` must return an object of type {dtype.type_def}"
                )
            return result

        return cast.apply_with_errors(
            series=series,
            call=wrapped_call,
            na_value=dtype.na_value,
            errors=errors
        )


#######################
####    PRIVATE    ####
#######################


class Test:

    def __init__(self, x: str = "foo"):
        self.x = x

    def as_boolean(self):
        print("Test.as_boolean called!")
        return bool(self)

    def __bool__(self) -> bool:
        return np.random.rand() > 0.5

    def __int__(self) -> int:
        return np.random.randint(10)

    def __float__(self) -> float:
        return np.random.rand()

    def __complex__(self) -> complex:
        return complex(np.random.rand(), np.random.rand())

    def __str__(self) -> str:
        return self.x


class Test2:

    def __init__(self, x: Test):
        self.x = f"{x} bar"

    def as_boolean(self):
        print("Test2.as_boolean called!")
        return bool(self)

    def __bool__(self) -> bool:
        return np.random.rand() > 0.5

    def __int__(self) -> int:
        return np.random.randint(10)

    def __float__(self) -> float:
        return np.random.rand()

    def __complex__(self) -> complex:
        return complex(np.random.rand(), np.random.rand())

    def __str__(self) -> str:
        return self.x


def two_step_conversion(
    series: cast.SeriesWrapper,
    dtype: AtomicType ,
    call: Callable,
    errors: str,
    conv_func: Callable,
    **unused
) -> pd.Series:
    """A conversion in two parts."""
    def wrapped_call(object val):
        cdef object result = call(val)
        cdef type output_type = type(result)

        if output_type != dtype.type_def:
            raise ValueError(
                f"`call` must return an object of type {dtype.type_def}"
            )
        return result

    return conv_func(
        cast.apply_and_wrap(
            series=series,
            call=wrapped_call,
            na_value=dtype.na_value,
            errors=errors
        ),
        dtype=dtype,
        errors=errors,
        **unused
    )


cdef object from_caller(str name, int stack_index = 0):
    """Get an arbitrary object from a parent calling context."""
    # split name into '.'-separated object path
    cdef list full_path = name.split(".")
    frame = inspect.stack()[stack_index].frame

    # get first component of full path from calling context
    cdef str first = full_path[0]
    if first in frame.f_locals:
        result = frame.f_locals[first]
    elif first in frame.f_globals:
        result = frame.f_globals[first]
    elif first in frame.f_builtins:
        result = frame.f_builtins[first]
    else:
        raise ValueError(
            f"could not find {name} in calling frame at position {stack_index}"
        )

    # walk through any remaining path components
    cdef str component
    for component in full_path[1:]:
        result = getattr(result, component)

    # return fully resolved object
    return result