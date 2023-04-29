"""This module contains dispatched cast() implementations for floating point
data.
"""
from pdcast import types
from pdcast.util import wrapper
from pdcast.util.round import snap_round, Tolerance

from .base import (
    cast, generic_to_boolean, generic_to_integer
)


@cast.overload("float", "bool")
def float_to_boolean(
    series: wrapper.SeriesWrapper,
    dtype: types.AtomicType,
    rounding: str,
    tol: Tolerance,
    errors: str,
    **unused
) -> wrapper.SeriesWrapper:
    """Convert floating point data to a boolean data type."""
    series = snap_round(
        series,
        tol=tol.real,
        rule=rounding,
        errors=errors
    )
    series, dtype = series.boundscheck(dtype, errors=errors)
    return generic_to_boolean(series, dtype, errors=errors)


@cast.overload("float", "int")
def float_to_integer(
    series: wrapper.SeriesWrapper,
    dtype: types.AtomicType,
    rounding: str,
    tol: Tolerance,
    downcast: types.CompositeType,
    errors: str,
    **unused
) -> wrapper.SeriesWrapper:
    """Convert floating point data to an integer data type."""
    series = snap_round(
        series,
        tol=tol.real,
        rule=rounding,
        errors=errors
    )
    series, dtype = series.boundscheck(dtype, errors=errors)
    return generic_to_integer(series, dtype, downcast=downcast, errors=errors)


try:  # float80 might not be defined on all systems

    @cast.overload("float80", "decimal")
    def longdouble_to_decimal(
        series: wrapper.SeriesWrapper,
        dtype: types.AtomicType,
        errors: str,
        **unused
    ) -> wrapper.SeriesWrapper:
        """A special case of float_to_decimal() that bypasses
        `TypeError: conversion from numpy.float128 to Decimal is not supported`.
        """
        # convert longdouble to integer ratio and then to decimal
        def call(element):
            numerator, denominator = element.as_integer_ratio()
            return dtype.type_def(numerator) / denominator

        return series.apply_with_errors(
            call=call,
            errors=errors,
            element_type=dtype
        )

except ValueError:
    pass
