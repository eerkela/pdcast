import decimal

import numpy as np
cimport numpy as np
import pandas as pd
import pytz

from .base cimport AtomicType
from .base import generic

cimport pdtypes.types.cast as cast
import pdtypes.types.cast as cast
cimport pdtypes.types.resolve as resolve
import pdtypes.types.resolve as resolve

from pdtypes.util.time cimport Epoch


######################
####    MIXINS    ####
######################


class BooleanMixin:

    # is_boolean = True
    min = 0
    max = 1

    ############################
    ####    TYPE METHODS    ####
    ############################

    def force_nullable(self) -> AtomicType:
        """Create an equivalent boolean type that can accept missing values."""
        if self.is_nullable:
            return self
        return self.generic.instance(backend="pandas")

    @property
    def is_nullable(self) -> bool:
        """Check if a boolean type supports missing values."""
        if isinstance(self.dtype, np.dtype):
            return np.issubdtype(self.dtype, "O")
        return True

    def parse(self, input_str: str):
        if input_str in resolve.na_strings:
            return resolve.na_strings[input_str]
        if input_str not in ("True", "False"):
            raise TypeError(
                f"could not interpret boolean string: {input_str}"
            )
        return self.type_def(input_str == "True")

    ##############################
    ####    SERIES METHODS    ####
    ##############################

    def to_decimal(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert boolean data into an equivalent decimal representation."""
        series = series + dtype.type_def(0)  # ~2x faster than loop
        series.element_type = dtype
        return series

    def to_datetime(
        self,
        series: cast.SeriesWrapper,
        dtype: AtomicType,
        unit: str,
        step_size: int,
        rounding: str,
        epoch: Epoch,
        tz: pytz.BaseTzInfo,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert boolean data into an equivalent datetime representation."""
        # 2-step conversion: bool -> int, int -> datetime
        transfer_type = resolve.resolve_type(int)
        series = self.to_integer(
            series,
            dtype=transfer_type,
            downcast=None,
            errors="raise"
        )
        return transfer_type.to_datetime(
            series,
            dtype=dtype,
            unit=unit,
            step_size=step_size,
            rounding=rounding,
            epoch=epoch,
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
        epoch: Epoch,
        errors: str,
        **unused
    ) -> cast.SeriesWrapper:
        """Convert integer data to a timedelta data type."""
        transfer_type = resolve.resolve_type(int)
        series = self.to_integer(
            series,
            dtype=transfer_type,
            downcast=None,
            errors="raise"
        )
        return transfer_type.to_timedelta(
            series,
            dtype=dtype,
            unit=unit,
            step_size=step_size,
            rounding=rounding,
            epoch=epoch,
            errors=errors,
            **unused
        )


#######################
####    GENERIC    ####
#######################


@generic
class BooleanType(BooleanMixin, AtomicType):
    """Boolean supertype."""

    conversion_func = cast.to_boolean  # all subtypes/backends inherit this
    name = "bool"
    aliases = {bool, "bool", "boolean", "bool_", "bool8", "b1", "?"}
    dtype = np.dtype(np.bool_)
    itemsize = 1
    type_def = bool
    is_nullable = False


#####################
####    NUMPY    ####
#####################


@BooleanType.register_backend("numpy")
class NumpyBooleanType(BooleanMixin, AtomicType):

    aliases = {np.bool_, np.dtype(np.bool_)}
    dtype = np.dtype(np.bool_)
    itemsize = 1
    type_def = np.bool_
    is_nullable = False


######################
####    PANDAS    ####
######################


@BooleanType.register_backend("pandas")
class PandasBooleanType(BooleanMixin, AtomicType):

    aliases = {pd.BooleanDtype(), "Boolean"}
    dtype = pd.BooleanDtype()
    itemsize = 1
    type_def = np.bool_


######################
####    PYTHON    ####
######################


@BooleanType.register_backend("python")
class PythonBooleanType(BooleanMixin, AtomicType):

    aliases = set()
    dtype = np.dtype("O")
    itemsize = 1
    type_def = bool
