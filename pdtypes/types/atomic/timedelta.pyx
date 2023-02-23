import datetime
from typing import Any

import numpy as np
cimport numpy as np
import pandas as pd
import regex as re  # using alternate python regex engine
import pytz

from .base cimport AtomicType, CompositeType
from .base import generic, register

cimport pdtypes.types.cast as cast
import pdtypes.types.cast as cast
cimport pdtypes.types.resolve as resolve
import pdtypes.types.resolve as resolve

from pdtypes.util.round cimport Tolerance
from pdtypes.util.round import round_div
from pdtypes.util.time cimport Epoch
from pdtypes.util.time import (
    as_ns, convert_unit, pytimedelta_to_ns, valid_units
)


# NOTE: timedelta -> float does not retain longdouble precision.  This is due
# to the / operator in convert_unit() defaulting to float64 precision, which is
# probably unfixable.


######################
####    MIXINS    ####
######################


class TimedeltaMixin:

    ##############################
    ####    SERIES METHODS    ####
    ##############################

    def to_boolean(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        tol: Tolerance,
        rounding: str,
        unit: str,
        step_size: int,
        since: Epoch,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert timedelta data to a boolean data type."""
        # 2-step conversion: timedelta -> decimal, decimal -> bool
        transfer_type = resolve.resolve_type("decimal")
        series = self.to_decimal(
            series,
            dtype=transfer_type,
            tol=tol,
            rounding=rounding,
            unit=unit,
            step_size=step_size,
            since=since,
            errors=errors
        )
        return transfer_type.to_boolean(
            series,
            dtype=dtype,
            tol=tol,
            rounding=rounding,
            unit=unit,
            step_size=step_size,
            since=since,
            errors=errors,
            **unused
        )

    def to_float(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        unit: str,
        step_size: int,
        since: Epoch,
        tol: Tolerance,
        rounding: str,
        downcast: CompositeType,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert timedelta data to a floating point data type."""
        # convert to nanoseconds, then from nanoseconds to final unit
        transfer_type = resolve.resolve_type(int)
        series = self.to_integer(
            series,
            dtype=transfer_type,
            unit="ns",
            step_size=1,
            since=since,
            rounding=None,
            downcast=None,
            errors=errors
        )
        if unit != "ns" or step_size != 1:
            series.series = convert_unit(
                series.series,
                "ns",
                unit,
                rounding=rounding,
                since=since
            )
            if step_size != 1:
                series.series /= step_size
            transfer_type = resolve.resolve_type(float)

        return transfer_type.to_float(
            series,
            dtype=dtype,
            unit=unit,
            step_size=step_size,
            since=since,
            tol=tol,
            rounding=rounding,
            downcast=downcast,
            errors=errors,
            **unused
        )

    def to_complex(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        unit: str,
        step_size: int,
        since: Epoch,
        tol: Tolerance,
        rounding: str,
        downcast: CompositeType,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert timedelta data to a complex data type."""
        # 2-step conversion: timedelta -> float, float -> complex
        transfer_type = dtype.equiv_float
        series = self.to_float(
            series,
            dtype=transfer_type,
            unit=unit,
            step_size=step_size,
            since=since,
            rounding=rounding,
            downcast=None,
            errors=errors
        )
        return transfer_type.to_complex(
            series,
            dtype=dtype,
            unit=unit,
            step_size=step_size,
            since=since,
            tol=tol,
            rounding=rounding,
            downcast=downcast,
            errors=errors,
            **unused
        )

    def to_decimal(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        unit: str,
        step_size: int,
        since: Epoch,
        tol: Tolerance,
        rounding: str,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert timedelta data to a decimal data type."""
        # 2-step conversion: timedelta -> ns, ns -> decimal
        transfer_type = resolve.resolve_type(int)
        series = self.to_integer(
            series,
            dtype=transfer_type,
            unit="ns",
            step_size=1,
            since=since,
            rounding=None,
            downcast=None,
            errors=errors
        )
        series = transfer_type.to_decimal(
            series,
            dtype=dtype,
            unit=unit,
            step_size=step_size,
            since=since,
            tol=tol,
            rounding=rounding,
            errors=errors,
            **unused
        )
        if unit != "ns" or step_size != 1:
            series.series = convert_unit(
                series.series,
                "ns",
                unit,
                rounding=rounding,
                since=since
            )
            if step_size != 1:
                series.series /= step_size

        return series

    def to_datetime(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        unit: str,
        step_size: int,
        rounding: str,
        since: Epoch,
        tz: pytz.BaseTzInfo,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert datetime data to another datetime representation."""
        # 2-step conversion: datetime -> ns, ns -> datetime
        transfer_type = resolve.resolve_type(int)
        series = self.to_integer(
            series,
            dtype=transfer_type,
            unit="ns",
            step_size=1,
            rounding=rounding,
            since=since,
            downcast=None,
            errors=errors
        )
        return transfer_type.to_datetime(
            series,
            dtype=dtype,
            unit="ns",
            step_size=1,
            rounding=rounding,
            since=since,
            tz=tz,
            errors=errors,
            **unused
        )

    def to_timedelta(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        unit: str,
        step_size: int,
        rounding: str,
        since: Epoch,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert timedelta data to a timedelta representation."""
        # trivial case: no conversion necessary
        if dtype == series.element_type:
            return series.rectify()

        # 2-step conversion: datetime -> ns, ns -> timedelta
        transfer_type = resolve.resolve_type(int)
        series = self.to_integer(
            series,
            dtype=transfer_type,
            unit="ns",
            step_size=1,
            rounding=rounding,
            since=since,
            downcast=None,
            errors=errors
        )
        return transfer_type.to_timedelta(
            series,
            dtype=dtype,
            unit="ns",
            step_size=1,
            rounding=rounding,
            since=since,
            errors=errors,
            **unused
        )


#######################
####    GENERIC    ####
#######################


@register
@generic
class TimedeltaType(TimedeltaMixin, AtomicType):

    conversion_func = cast.to_timedelta  # all subtypes/backends inherit this
    name = "timedelta"
    aliases = {"timedelta"}
    na_value = pd.NaT
    max = 0
    min = 1  # these values always trip overflow/upcast check

    ############################
    ####    TYPE METHODS    ####
    ############################

    @property
    def larger(self) -> list:
        """Get a list of types that this type can be upcasted to."""
        # start with bounded subtypes that have range wider than self
        candidates = set(self.subtypes) - {self}
        result = [
            x for x in candidates if (
                x.min <= x.max and (x.min < self.min or x.max > self.max)
            )
        ]
        result.sort(key=lambda x: x.max - x.min)

        # add subtypes that are themselves upcast-only
        others = [x for x in candidates if x.min > x.max]
        result.extend(sorted(others, key=lambda x: x.min - x.max))
        return result

    ##############################
    ####    SERIES METHODS    ####
    ##############################

    # NOTE: because this type has no associated scalars, it will never be given
    # as the result of a detect_type() operation.  It can only be specified
    # manually, as the target of a resolve_type() call.


#####################
####    NUMPY    ####
#####################


@register
@TimedeltaType.register_backend("numpy")
class NumpyTimedelta64Type(TimedeltaMixin, AtomicType, cache_size=64):

    aliases = {
        np.timedelta64,
        # np.dtype("m8") handled in resolve_typespec_dtype special case
        "m8",
        "timedelta64",
        "numpy.timedelta64",
        "np.timedelta64",
    }
    itemsize = 8
    na_value = np.timedelta64("NaT")
    type_def = np.timedelta64

    def __init__(self, unit: str = None, step_size: int = 1):
        if unit is None:
            self.dtype = np.dtype("m8")
            # NOTE: these min/max values always trigger upcast check.
            self.min = 1  # increase this to take precedence when upcasting
            self.max = 0
        else:
            self.dtype = np.dtype(f"m8[{step_size}{unit}]")
            # NOTE: these epochs are chosen to minimize range in the event of
            # irregular units ('Y'/'M'), so that conversions work regardless of
            # leap days and irregular month lengths.
            self.max = convert_unit(
                2**63 - 1,
                unit,
                "ns",
                since=Epoch(pd.Timestamp("2001-02-01"))
            )
            self.min = convert_unit(
                -2**63 + 1,  # NOTE: -2**63 reserved for NaT
                unit,
                "ns",
                since=Epoch(pd.Timestamp("2000-02-01"))
            )

        super().__init__(unit=unit, step_size=step_size)

    ############################
    ####    TYPE METHODS    ####
    ############################

    @classmethod
    def slugify(cls, unit: str = None, step_size: int = 1) -> str:
        if unit is None:
            return f"{cls.name}[{cls.backend}]"
        if step_size == 1:
            return f"{cls.name}[{cls.backend}, {unit}]"
        return f"{cls.name}[{cls.backend}, {step_size}{unit}]"

    def contains(self, other: Any) -> bool:
        other = resolve.resolve_type(other)
        if isinstance(other, CompositeType):
            return all(self.contains(o) for o in other)

        # treat unit=None as wildcard
        if self.unit is None:
            return isinstance(other, type(self))
        return super().contains(other)

    @classmethod
    def detect(cls, example: np.datetime64, **defaults) -> AtomicType:
        unit, step_size = np.datetime_data(example)
        return cls.instance(unit=unit, step_size=step_size, **defaults)

    @property
    def larger(self) -> list:
        """Get a list of types that this type can be upcasted to."""
        if self.unit is None:
            return [self.instance(unit=u) for u in valid_units]
        return []

    @classmethod
    def resolve(cls, context: str = None) -> AtomicType:
        if context is not None:
            match = m8_pattern.match(context)
            if not match:
                raise ValueError(f"invalid unit: {repr(context)}")
            unit = match.group("unit")
            step_size = int(match.group("step_size") or 1)
            return cls.instance(unit=unit, step_size=step_size)
        return cls.instance()

    ##############################
    ####    SERIES METHODS    ####
    ##############################

    def from_ns(
        self,
        series: cast.SeriesWrapper,
        rounding: str,
        since: Epoch,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert nanosecond offsets from the given epoch into numpy
        timedelta64s with this type's unit and step size.
        """
        # convert from ns to final unit
        series.series = convert_unit(
            series.series,
            "ns",
            self.unit,
            rounding=rounding or "down",
            since=since
        )
        if self.step_size != 1:
            series.series = round_div(
                series.series,
                self.step_size,
                rule=rounding or "down"
            )
        m8_str = f"m8[{self.step_size}{self.unit}]"
        return cast.SeriesWrapper(
            pd.Series(
                list(series.series.to_numpy(m8_str)),
                index=series.series.index,
                dtype="O"
            ),
            hasnans=series.hasnans,
            element_type=self
        )

    def to_integer(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        unit: str,
        step_size: int,
        since: Epoch,
        rounding: str,
        downcast: CompositeType,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert numpy timedelta64s into an integer data type."""
        # NOTE: using numpy m8 array is ~2x faster than looping through series
        m8_str = f"m8[{self.step_size}{self.unit}]"
        arr = series.series.to_numpy(m8_str).view(np.int64).astype("O")
        arr *= self.step_size
        arr = convert_unit(
            arr,
            self.unit,
            unit,
            rounding=rounding or "down",
            since=since
        )
        series = cast.SeriesWrapper(
            pd.Series(arr, index=series.series.index),
            hasnans=series.hasnans,
            element_type=resolve.resolve_type(int)
        )

        series, dtype = series.boundscheck(dtype, errors=errors)
        return super().to_integer(
            series,
            dtype,
            downcast=downcast,
            errors=errors
        )


######################
####    PANDAS    ####
######################


@register
@TimedeltaType.register_backend("pandas")
class PandasTimedeltaType(TimedeltaMixin, AtomicType):

    aliases = {pd.Timedelta, "Timedelta", "pandas.Timedelta", "pd.Timedelta"}
    dtype = np.dtype("m8[ns]")
    itemsize = 8
    na_value = pd.NaT
    type_def = pd.Timedelta
    max = pd.Timedelta.max.value
    min = pd.Timedelta.min.value

    ##############################
    ####    SERIES METHODS    ####
    ##############################

    def from_ns(
        self,
        series: cast.SeriesWrapper,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert nanosecond offsets into pandas Timedeltas."""
        # convert using pd.to_timedelta()
        return cast.SeriesWrapper(
            pd.to_timedelta(series.series, unit="ns"),
            hasnans=series.hasnans,
            element_type=self
        )

    def to_integer(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        unit: str,
        step_size: int,
        since: Epoch,
        rounding: str,
        downcast: CompositeType,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert pandas Timedeltas to an integer data type."""
        series = series.rectify().astype(np.int64)
        if unit != "ns" or step_size != 1:
            convert_ns_to_unit(
                series,
                unit=unit,
                step_size=step_size,
                rounding=rounding,
                since=since
            )

        series, dtype = series.boundscheck(dtype, errors=errors)
        return super().to_integer(
            series,
            dtype,
            downcast=downcast,
            errors=errors
        )


######################
####    PYTHON    ####
######################


@register
@TimedeltaType.register_backend("python")
class PythonTimedeltaType(TimedeltaMixin, AtomicType):

    aliases = {datetime.timedelta, "pytimedelta", "datetime.timedelta"}
    na_value = pd.NaT
    type_def = datetime.timedelta
    max = pytimedelta_to_ns(datetime.timedelta.max)
    min = pytimedelta_to_ns(datetime.timedelta.min)

    ##############################
    ####    SERIES METHODS    ####
    ##############################

    def from_ns(
        self,
        series: cast.SeriesWrapper,
        rounding: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert nanosecond offsets into python timedeltas."""
        # convert to us
        result = round_div(series.series, as_ns["us"], rule=rounding or "down")

        # NOTE: m8[us].astype("O") implicitly converts to datetime.timedelta
        return cast.SeriesWrapper(
            pd.Series(
                result.to_numpy("m8[us]").astype("O"),
                index=series.series.index,
                dtype="O"
            ),
            hasnans=series.hasnans,
            element_type=self
        )

    def to_integer(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        unit: str,
        step_size: int,
        since: Epoch,
        rounding: str,
        downcast: CompositeType,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert python timedeltas to an integer data type."""
        series = series.apply_with_errors(pytimedelta_to_ns)
        series.element_type = int

        if unit != "ns" or step_size != 1:
            convert_ns_to_unit(
                series,
                unit=unit,
                step_size=step_size,
                rounding=rounding,
                since=since
            )

        series, dtype = series.boundscheck(dtype, errors=errors)
        return super().to_integer(
            series,
            dtype,
            downcast=downcast,
            errors=errors
        )


#######################
####    PRIVATE    ####
#######################


cdef object m8_pattern = re.compile(
    r"(?P<step_size>[0-9]+)?(?P<unit>ns|us|ms|s|m|h|D|W|M|Y)"
)


def convert_ns_to_unit(
    series: cast.SeriesWrapper,
    unit: str,
    step_size: int,
    rounding: str,
    since: Epoch
) -> None:
    """Helper for converting between integer time units."""
    series.series = convert_unit(
        series.series,
        "ns",
        unit,
        since=since,
        rounding=rounding or "down",
    )
    if step_size != 1:
        series.series = round_div(
            series.series,
            step_size,
            rule=rounding or "down"
        )
