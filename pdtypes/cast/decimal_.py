from __future__ import annotations
import decimal

import numpy as np
import pandas as pd

from pdtypes.check.check import (
    check_dtype, extension_type, get_dtype, is_dtype, resolve_dtype
)
from pdtypes.cython.decimal import quantize_decimal
from pdtypes.error import ConversionError, error_trace, shorten_list
from pdtypes.util.type_hints import dtype_like

from .helpers import (
    _validate_dtype, _validate_errors, _validate_rounding, integral_range,
    tolerance, DEFAULT_STRING_TYPE
)
from .float_ import FloatSeries


def apply_tolerance(
    val: decimal.Decimal | np.ndarray | pd.Series,
    tol: int | float | decimal.Decimal,
    copy: bool = True
) -> pd.Series:
    """test"""
    rounded = round_decimal(val, "half_even", decimals=0, copy=True)

    # numpy array, using np.where
    if isinstance(val, np.ndarray):
        if copy:
            return np.where(np.abs(val - rounded) > tol, val, rounded)
        val[:] = np.where(np.abs(val - rounded) > tol, val, rounded)
        return val

    # pandas series, using Series.where
    if isinstance(val, pd.Series):
        rounded.index = val.index  # match index
        if copy:
            return val.where(np.abs(val - rounded) > tol, rounded)
        val.where(np.abs(val - rounded) > tol, rounded, inplace=True)
        return val

    # scalar
    if np.abs(val - rounded) > tol:
        return val
    return rounded


def round_decimal(
    val: decimal.Decimal | np.ndarray | pd.Series,
    rule: str = "half_even",
    decimals: int = 0,
    copy: bool = True
) -> decimal.Decimal | np.ndarray | pd.Series:
    """test"""
    is_array_like = isinstance(val, (np.ndarray, pd.Series))

    # This function implements an optimization known as hidden mutability.
    # Internally, any statement that modifies `val` is done in-place, with
    # an explicit copy generated (optionally) beforehand.
    if not is_array_like:  # scalars are always copied
        val = val + 0
    elif copy:  # optionally copy if array-like
        val = val.copy()

    # optimization: only scale if `decimals != 0`
    if decimals:
        scale_factor = decimal.Decimal(10)**decimals
        val *= scale_factor

    switch = {  # C-style switch statement
        "floor": lambda x: x.quantize(1, decimal.ROUND_FLOOR),
        "ceiling": lambda x: x.quantize(1, decimal.ROUND_CEILING),
        "down": lambda x: x.quantize(1, decimal.ROUND_DOWN),
        "up": lambda x: x.quantize(1, decimal.ROUND_UP),
        "half_floor": (lambda x: x.quantize(1, decimal.ROUND_HALF_UP)
                                 if x < 0 else
                                 x.quantize(1, decimal.ROUND_HALF_DOWN)),
        "half_ceiling": (lambda x: x.quantize(1, decimal.ROUND_HALF_DOWN)
                                   if x < 0 else
                                   x.quantize(1, decimal.ROUND_HALF_UP)),
        "half_down": lambda x: x.quantize(1, decimal.ROUND_HALF_DOWN),
        "half_up": lambda x: x.quantize(1, decimal.ROUND_HALF_UP),
        "half_even": lambda x: x.quantize(1, decimal.ROUND_HALF_EVEN)
    }

    # do rounding in-place
    if is_array_like:
        if isinstance(val, pd.Series):  # pandas.Series
            index = val.index  # remember index
            val = quantize_decimal(val.to_numpy(), switch[rule], copy=False)
            val = pd.Series(val, copy=False)
            val.index = index  # replace index
        else:  # numpy.array
            val = quantize_decimal(val, switch[rule], copy=False)
    else:  # scalar
        val = switch[rule](val)

    # undo scaling
    if decimals:
        val /= scale_factor

    # return
    return val


class DecimalSeries:
    """test"""

    def __init__(
        self,
        series: pd.Series,
        validate: bool = True
    ) -> DecimalSeries:
        if validate and not check_dtype(series, decimal.Decimal):
            err_msg = (f"[{error_trace()}] `series` must contain decimal "
                       f"data, not {get_dtype(series)}")
            raise TypeError(err_msg)

        self.series = series
        self._infs = None
        self._hasinfs = None

    @property
    def infs(self) -> pd.Series:
        """test"""
        if self._infs is not None:  # infs is cached
            return self._infs

        # infs must be computed
        comp = (decimal.Decimal("-inf"), decimal.Decimal("inf"))
        self._infs = self.series.isin(comp)
        self._hasinfs = self._infs.any()
        return self._infs

    @property
    def hasinfs(self) -> bool:
        """test"""
        if self._hasinfs is not None:  # hasinfs is cached
            return self._hasinfs

        # hasinfs must be computed
        self._hasinfs = self.infs.any()
        return self._hasinfs

    def round(
        self,
        rule: str = "half_even",
        decimals: int = 0,
        copy: bool = True
    ) -> pd.Series:
        """test"""
        # TODO: this can be attached directly to pd.Series
        return round_decimal(self.series, rule=rule, decimals=decimals,
                             copy=copy)

    def to_boolean(
        self,
        dtype: dtype_like = bool,
        tol: int | float | decimal.Decimal = 0,
        rounding: None | str = None,
        errors: str = "raise"
    ) -> pd.Series:
        """test"""
        dtype = resolve_dtype(dtype)
        _validate_dtype(dtype, bool)
        tol, _ = tolerance(tol)
        _validate_rounding(rounding)
        _validate_errors(errors)
        if tol >= 0.5:
            rounding = "half_even"
            tol = 0

        # TODO: explicitly reject infinities?

        series = self.series.copy()

        # apply tolerance and rounding rules, if applicable
        nearest = ("half_floor", "half_ceiling", "half_down", "half_up",
                   "half_even")
        if tol and rounding not in nearest:
            series = apply_tolerance(series, tol=tol, copy=False)
        if rounding:
            series = round_decimal(series, rule=rounding, copy=False)

        # check for precision loss
        if ((series != 0) & (series != 1)).any():
            if errors != "coerce":
                bad_vals = series[(series != 0) & (series != 1)]
                err_msg = (f"non-boolean value encountered at index "
                           f"{shorten_list(bad_vals.index.values)}")
                raise ConversionError(err_msg, bad_vals)
            series = np.ceil(series.abs().clip(0, 1))  # coerce to [0, 1]

        # return
        return series.astype(dtype, copy=False)

    def to_integer(
        self,
        dtype: dtype_like = int,
        tol: int | float | decimal.Decimal = 0,
        rounding: None | str = None,
        downcast: bool = False,
        errors: str = "raise"
    ) -> pd.Series:
        """test

        tol=0.5 <==> rounding="half_even"
        errors="coerce" <==> rounding="down" + overflow to nan
        """
        dtype = resolve_dtype(dtype)
        tol, _ = tolerance(tol)
        _validate_rounding(rounding)
        _validate_errors(errors)
        if tol >= 0.5:
            rounding = "half_even"
            tol = 0

        series = self.series.copy()
        coerced = False  # NAs may be introduced by coercion

        # reject any series that contains infinity
        if self.hasinfs:
            if errors != "coerce":
                bad_vals = series[self.infs]
                err_msg = (f"no integer equivalent for infinity at index "
                           f"{shorten_list(bad_vals.index.values)}")
                raise ConversionError(err_msg, bad_vals)
            series[self.infs] = pd.NA  # coerce
            coerced = True  # remember to convert to extension type later

        # apply tolerance and rounding rules, if applicable
        nearest = ("half_floor", "half_ceiling", "half_down", "half_up",
                   "half_even")
        if tol and rounding not in nearest:
            series = apply_tolerance(series, tol=tol, copy=False)
        if rounding:
            series = round_decimal(series, rule=rounding, copy=False)

        # check for precision loss
        if not (rounding or series.equals(round_decimal(series))):
            if errors != "coerce":
                bad_vals = series[(series != round_decimal(series)) ^
                                  self.infs]
                err_msg = (f"precision loss detected at index "
                           f"{shorten_list(bad_vals.index.values)}")
                raise ConversionError(err_msg, bad_vals)
            round_decimal(series, "down", copy=False)  # coerce toward zero

        # get min/max to evaluate range
        min_val = series.min()
        max_val = series.max()

        # built-in integer special case - can be arbitrarily large
        if is_dtype(dtype, int, exact=True):
            if min_val < -2**63 or max_val > 2**63 - 1:  # >int64
                if min_val >= 0 and max_val <= 2**64 - 1:  # <uint64
                    dtype = pd.UInt64Dtype() if coerced else np.uint64
                    return series.astype(dtype, copy=False)
                # series is >int64 and >uint64, return as built-in python ints
                return np.frompyfunc(int, 1, 1)(series)  # as fast as cython
            # extended range isn't needed, demote to int64
            dtype = np.int64

        # check whether result fits within specified dtype
        min_poss, max_poss = integral_range(dtype)
        if min_val < min_poss or max_val > max_poss:
            if errors != "coerce":
                bad_vals = series[(series < min_poss) | (series > max_poss)]
                err_msg = (f"values exceed {dtype} range at index "
                           f"{shorten_list(bad_vals.index.values)}")
                raise ConversionError(err_msg, bad_vals)
            series[(series < min_poss) | (series > max_poss)] = pd.NA
            min_val = np.longdouble(series.min())
            max_val = np.longdouble(series.max())
            coerced = True  # remember to convert to extension type later

        # attempt to downcast if applicable
        if downcast:  # search for smaller dtypes that can represent series
            if is_dtype(dtype, "unsigned"):
                int_types = [np.uint8, np.uint16, np.uint32, np.uint64]
            else:
                int_types = [np.int8, np.int16, np.int32, np.int64]
            for downcast_type in int_types[:int_types.index(dtype)]:
                min_poss, max_poss = integral_range(downcast_type)
                if min_val >= min_poss and max_val <= max_poss:
                    dtype = downcast_type
                    break  # stop at smallest

        # convert and return
        if coerced:  # convert to extension type early
            dtype = extension_type(dtype)
        return series.astype(dtype, copy=False)

    def to_float(
        self,
        dtype: dtype_like = float,
        tol: int | float | complex | decimal.Decimal = 1e-6,
        downcast: bool = False,
        errors: str = "raise"
    ) -> pd.Series:
        """test"""
        dtype = resolve_dtype(dtype)
        _validate_dtype(dtype, float)
        tol, _ = tolerance(tol)
        _validate_errors(errors)
        if dtype == float:  # built-in `float` is identical to np.float64
            dtype = np.float64
        if tol == np.inf:  # infinite tolerance is equivalent to "coerce"
            errors = "coerce"

        # do naive conversion, then reverse to detect precision loss/overflow
        series = self.series.astype(dtype)  # naive conversion
        reverse = FloatSeries(series, validate=False)
        if errors == "coerce":
            series[reverse.infs ^ self.infs] = np.nan
        else:  # problem arises when subtracting infinities
            reverse = reverse.to_decimal()[~self.infs]
            outside_tol = np.abs(reverse - self.series[~self.infs]) > tol
            if outside_tol.any():
                bad_vals = series[outside_tol]
                err_msg = (f"precision loss detected at index "
                           f"{shorten_list(bad_vals.index.values)}")
                raise ConversionError(err_msg, bad_vals)

        # downcast if applicable
        if downcast:
            float_types = [np.float16, np.float32, np.float64, np.longdouble]
            for downcast_type in float_types[:float_types.index(dtype)]:
                attempt = series.astype(downcast_type, copy=False)
                if (attempt == series).all():
                    return attempt

        # return
        return series

    def to_complex(
        self,
        dtype: dtype_like = complex,
        tol: int | float | complex | decimal.Decimal = 1e-6,
        downcast: bool = False,
        errors: str = "raise"
    ) -> pd.Series:
        """test"""
        dtype = resolve_dtype(dtype)
        tol, _ = tolerance(tol)
        _validate_dtype(dtype, complex)
        _validate_errors(errors)
        if dtype == complex:  # built-in `complex` is identical to complex128
            dtype = np.complex128

        # 2 steps: decimal -> float, then float -> complex
        equiv_float = {
            np.complex64: np.float32,
            np.complex128: np.float64,
            np.clongdouble: np.longdouble
        }
        series = self.to_float(dtype=equiv_float[dtype], tol=tol, errors=errors)
        series = FloatSeries(series, validate=False)
        return series.to_complex(dtype=dtype, downcast=downcast, errors=errors)

    def to_decimal(self) -> pd.Series:
        """test"""
        # decimal.Decimal is the only recognized decimal implementation
        return self.series.copy()

    def to_string(self, dtype: dtype_like = str) -> pd.Series:
        """test"""
        resolve_dtype(dtype)  # ensures scalar, resolvable
        _validate_dtype(dtype, str)

        # force string extension type
        if not pd.api.types.is_extension_array_dtype(dtype):
            dtype = DEFAULT_STRING_TYPE

        # do conversion
        return self.series.astype(dtype, copy=True)
