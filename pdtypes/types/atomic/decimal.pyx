import decimal
from typing import Union, Sequence

import numpy as np
cimport numpy as np
import pandas as pd

from pdtypes.error import shorten_list
from pdtypes.type_hints import numeric
cimport pdtypes.types.cast as cast
import pdtypes.types.cast as cast
cimport pdtypes.types.resolve as resolve
import pdtypes.types.resolve as resolve
from pdtypes.util.round import round_decimal, Tolerance

from .base cimport AtomicType
from .base import generic


######################
####    MIXINS    ####
######################


class DecimalMixin:

    ##############################
    ####    SERIES METHODS    ####
    ##############################

    def round(
        self,
        series: cast.SeriesWrapper,
        rule: str = "half_even",
        decimals: int = 0
    ) -> cast.SeriesWrapper:
        """Round a decimal series to the given number of decimal places using
        the specified rounding rule.
        """
        return cast.SeriesWrapper(
            round_decimal(series.series, rule=rule, decimals=decimals),
            hasnans=series.hasnans,
            element_type=series.element_type
        )

    def to_boolean(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        rounding: str = None,
        tol: numeric = 1e-6,
        errors: str = "raise",
        **unused
    ) -> cast.SeriesWrapper:
        """Convert decimal data to a boolean data type."""
        # apply tolerance + rounding, rejecting any non-integer results
        series = cast.snap_round(series, Tolerance(tol).real, rounding, errors)

        # check for overflow
        dtype = cast.check_for_overflow(series, dtype, errors)

        # pass to AtomicType.to_boolean()
        return super().to_boolean(
            series=series,
            dtype=dtype,
            rounding=rounding,
            tol=tol,
            errors=errors,
            **unused
        )

    def to_integer(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        rounding: str = None,
        tol: numeric = 1e-6,
        errors: str = "raise",
        **unused
    ) -> cast.SeriesWrapper:
        """Convert decimal data to an integer data type."""
        # apply tolerance + rounding, rejecting any non-integer results
        series = cast.snap_round(series, Tolerance(tol).real, rounding, errors)

        # check for overflow
        dtype = cast.check_for_overflow(series, dtype, errors)

        # pass to AtomicType.to_integer()
        return super().to_integer(
            series=series,
            dtype=dtype,
            rounding=rounding,
            tol=tol,
            errors=errors,
            **unused
        )

    def to_float(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        tol: numeric = 1e-6,
        downcast: bool = False,
        errors: str = "raise",
        **unused
    ) -> cast.SeriesWrapper:
        """Convert decimal data to a floating point data type."""
        # parse tolerance
        tol = Tolerance(tol)

        # do naive conversion
        result = series.astype(dtype)

        # check for overflow
        if int(series.min()) < dtype.min or int(series.max()) > dtype.max:
            infs = result.isinf() ^ series.isinf()
            if infs.any():
                if errors == "coerce":
                    result.series = result[~infs]
                    result.hasnans = True
                    series.series = series[~infs]  # mirror on original
                else:
                    raise OverflowError(
                        f"values exceed {dtype} range at index "
                        f"{shorten_list(series[infs].index.values)}"
                    )

        # backtrack to check for precision loss
        if errors != "coerce":  # coercion ignores precision loss
            # NOTE: we can bypass overflow/precision loss checks by
            # delegating straight to AtomicType
            reverse = super().to_decimal(result, dtype=self)
            bad = ~cast.within_tolerance(series, reverse, tol=tol.real)
            if bad.any():
                raise ValueError(
                    f"precision loss exceeds tolerance {tol.real:.2e} at "
                    f"index {shorten_list(bad[bad].index.values)}"
                )

        if downcast:
            return dtype.downcast(result, tol=tol.real)
        return result

    def to_complex(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        tol: numeric = 1e-6,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert decimal data to a complex data type."""
        result = series.to_float(dtype=dtype.equiv_float, tol=tol, **unused)
        return result.to_complex(dtype=dtype, tol=tol, **unused)


#######################
####    GENERIC    ####
#######################


@generic
class DecimalType(DecimalMixin, AtomicType):

    conversion_func = cast.to_boolean  # all subtypes/backends inherit this
    name = "decimal"
    aliases = {"decimal"}

    def __init__(self):
        super().__init__(
            type_def=decimal.Decimal,
            dtype=np.dtype(np.object_),
            na_value=pd.NA,
            itemsize=None
        )


##############################
####    PYTHON DECIMAL    ####
##############################


@DecimalType.register_backend("python")
class PythonDecimalType(DecimalMixin, AtomicType):

    aliases = {decimal.Decimal}

    def __init__(self):
        super().__init__(
            type_def=decimal.Decimal,
            dtype=np.dtype(np.object_),
            na_value=pd.NA,
            itemsize=None
        )
