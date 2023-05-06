"""This module describes a ``SparseType`` object, which can be used to
dynamically wrap other types.
"""
from typing import Any

import numpy as np
import pandas as pd

from pdcast cimport resolve
from pdcast import resolve

from pdcast.decorators cimport wrapper
from pdcast.util.type_hints import type_specifier

from .base cimport AdapterType, CompositeType, ScalarType
from .base import register


# TODO: may need to create a special case for nullable integers, booleans
# to use their numpy counterparts.  This avoids converting to object, but
# forces the fill value to be pd.NA.
# -> this can be handled in a dispatched sparsify() implementation



@register
class SparseType(AdapterType):

    name = "sparse"
    aliases = {pd.SparseDtype, "sparse", "Sparse"}
    _priority = 10

    def __init__(self, wrapped: ScalarType = None, fill_value: Any = None):
        # do not re-wrap SparseTypes
        if isinstance(wrapped, SparseType):  # 1st order
            if fill_value is None:
                fill_value = wrapped.fill_value
            wrapped = wrapped.wrapped

        elif wrapped is not None:  # 2nd order
            for x in wrapped.adapters:
                if isinstance(x.wrapped, SparseType):
                    if fill_value is None:
                        fill_value = x.fill_value
                    wrapped = x.wrapped.wrapped
                    x.wrapped = self
                    break

        # call AdapterType.__init__()
        super().__init__(wrapped=wrapped, fill_value=fill_value)

    @property
    def fill_value(self) -> Any:
        """The value to mask from the array."""
        val = self.kwargs["fill_value"]
        if val is None:
            return getattr(self.wrapped, "na_value", None)
        return val

    ############################
    ####    CONSTRUCTORS    ####
    ############################

    @classmethod
    def from_dtype(
        cls,
        dtype: pd.api.extensions.ExtensionDtype
    ) -> AdapterType:
        """Convert a pandas SparseDtype into a
        :class:`SparseType <pdcast.SparseType>` object.
        """
        return cls(
            wrapped=resolve.resolve_type(dtype.subtype),
            fill_value=dtype.fill_value
        )

    @classmethod
    def resolve(
        cls,
        wrapped: str = None,
        fill_value: str = None
    ) -> AdapterType:
        """Resolve a sparse specifier in the
        :ref:`type specification mini langauge <resolve_type.mini_language>`.
        """
        from pdcast.convert import cast

        if wrapped is None:
            return cls()

        cdef ScalarType instance = resolve.resolve_type(wrapped)
        cdef object parsed = None

        # resolve fill_value
        if fill_value is not None:
            if fill_value in resolve.na_strings:
                parsed = resolve.na_strings[fill_value]
            else:
                parsed = cast(fill_value, instance)[0]

        # insert into sorted adapter stack according to priority
        for x in instance.adapters:
            if x._priority <= cls._priority:  # initial
                break
            if getattr(x.wrapped, "_priority", -np.inf) <= cls._priority:
                x.wrapped = cls(x.wrapped, fill_value=parsed)
                return instance

        # add to front of stack
        return cls(instance, fill_value=parsed)

    @classmethod
    def slugify(
        cls,
        wrapped: ScalarType = None,
        fill_value: Any = None
    ) -> str:
        """Create a unique string representation of a
        :class:`SparseType <pdcast.SparseType>`
        """
        if wrapped is None:
            return cls.name
        if fill_value is None:
            return f"{cls.name}[{str(wrapped)}]"
        return f"{cls.name}[{str(wrapped)}, {fill_value}]"

    ############################
    ####    TYPE METHODS    ####
    ############################

    def contains(
        self,
        other: type_specifier,
        include_subtypes: bool = True
    ) -> bool:
        """Check whether the given type is contained within this type's
        subtype hierarchy.
        """
        other = resolve.resolve_type(other)

        # if target is composite, test each element individually
        if isinstance(other, CompositeType):
            return all(
                self.contains(o, include_subtypes=include_subtypes)
                for o in other
            )

        # assert other is sparse
        if not isinstance(other, type(self)):
            return False

        # check for naked specifier
        if self.wrapped is None:
            return True
        if other.wrapped is None:
            return False

        # check for unequal fill values
        if self.kwargs["fill_value"] is not None:
            na_1 = self.is_na(self.fill_value)
            na_2 = other.is_na(other.fill_value)
            if (
                na_1 ^ na_2 or
                na_1 == na_2 == False and self.fill_value != other.fill_value
            ):
                return False

        # delegate to wrapped
        return self.wrapped.contains(
            other.wrapped,
            include_subtypes=include_subtypes
        )

    @property
    def dtype(self) -> pd.SparseDtype:
        """An equivalent SparseDtype to use for arrays of this type."""
        return pd.SparseDtype(self.wrapped.dtype, fill_value=self.fill_value)

    ##############################
    ####    SERIES METHODS    ####
    ##############################

    def inverse_transform(
        self,
        series: wrapper.SeriesWrapper
    ) -> wrapper.SeriesWrapper:
        """Convert a sparse series into a dense format"""
        # NOTE: this is a pending deprecation shim.  In a future version of
        # pandas, astype() from a sparse to non-sparse dtype will return a
        # non-sparse series.  Currently, it returns a sparse equivalent. When
        # this behavior changes, this method can be deleted.
        return wrapper.SeriesWrapper(
            series.sparse.to_dense(),
            hasnans=series._hasnans,
            element_type=self.wrapped
        )

    def transform(
        self,
        series: wrapper.SeriesWrapper
    ) -> wrapper.SeriesWrapper:
        """Convert a series into a sparse format with the given fill_value."""
        # apply custom logic for each AtomicType
        series = self.atomic_type.make_sparse(
            series,
            fill_value=self.fill_value
        )
        series.element_type = self
        return series
