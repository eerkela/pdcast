from pdcast import types
from pdcast.util import wrapper
from pdcast.util.error import shorten_list
from pdcast.util.round import Tolerance

from .base import cast, generic_to_float


#######################
####    BOOLEAN    ####
#######################


@cast.overload("complex", "bool")
def complex_to_boolean(
    series: wrapper.SeriesWrapper,
    dtype: types.AtomicType,
    rounding: str,
    tol: Tolerance,
    errors: str,
    **unused
) -> wrapper.SeriesWrapper:
    """Convert complex data to a boolean data type."""
    # 2-step conversion: complex -> float, float -> bool
    series = cast(
        series,
        "float",
        tol=tol,
        downcast=None,
        errors=errors
    )
    return cast(
        series,
        dtype,
        rounding=rounding,
        tol=tol,
        errors=errors,
        **unused
    )


#######################
####    INTEGER    ####
#######################


@cast.overload("complex", "int")
def complex_to_integer(
    series: wrapper.SeriesWrapper,
    dtype: types.AtomicType,
    rounding: str,
    tol: Tolerance,
    downcast: types.CompositeType,
    errors: str,
    **unused
) -> wrapper.SeriesWrapper:
    """Convert complex data to an integer data type."""
    # 2-step conversion: complex -> float, float -> int
    series = cast(
        series,
        "float",
        tol=tol,
        downcast=None,
        errors=errors
    )
    return cast(
        series,
        dtype,
        rounding=rounding,
        tol=tol,
        downcast=downcast,
        errors=errors,
        **unused
    )


#####################
####    FLOAT    ####
#####################


@cast.overload("complex", "float")
def complex_to_float(
    series: wrapper.SeriesWrapper,
    dtype: types.AtomicType,
    tol: Tolerance,
    downcast: types.CompositeType,
    errors: str,
    **unused
) -> wrapper.SeriesWrapper:
    """Convert complex data to a float data type."""
    real = series.real

    # check for nonzero imag
    if errors != "coerce":  # ignore if coercing
        bad = ~series.imag.within_tol(0, tol=tol.imag)
        if bad.any():
            raise ValueError(
                f"imaginary component exceeds tolerance "
                f"{float(tol.imag):g} at index "
                f"{shorten_list(bad[bad].index.values)}"
            )

    return generic_to_float(
        real,
        dtype,
        tol=tol,
        downcast=downcast,
        errors=errors,
        **unused
    )


#######################
####    DECIMAL    ####
#######################


@cast.overload("complex", "decimal")
def complex_to_decimal(
    series: wrapper.SeriesWrapper,
    dtype: types.AtomicType,
    tol: Tolerance,
    errors: str,
    **unused
) -> wrapper.SeriesWrapper:
    """Convert complex data to a decimal data type."""
    # 2-step conversion: complex -> float, float -> decimal
    series = cast(
        series,
        "float",
        tol=tol,
        downcast=None,
        errors=errors
    )
    return cast(
        series,
        dtype,
        tol=tol,
        errors=errors,
        **unused
    )
