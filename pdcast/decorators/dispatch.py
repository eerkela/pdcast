"""EXPERIMENTAL - NOT CURRENTLY FUNCTIONAL

This module describes an ``@dispatch`` decorator that transforms an
ordinary Python function into one that dispatches to a method attached to the
inferred type of its first argument.
"""
from __future__ import annotations
from collections import OrderedDict
import inspect
import itertools
import threading
from types import MappingProxyType
from typing import Any, Callable, Iterable, Iterator
import warnings

import numpy as np
import pandas as pd

from pdcast.detect import detect_type
from pdcast.resolve import resolve_type
from pdcast import types
from pdcast.util.structs import LRUDict
from pdcast.util.type_hints import type_specifier

from .base import FunctionDecorator, no_default


# TODO: emit a warning whenever an implementation is replaced.
# -> do this.

# TODO: None wildcard value?
# -> use registry wildcards instead

# TODO: result is None -> fill with NA?
# -> probably not.  Just return an empty series if filtering.


# TODO: appears to be an index mismatch in composite add() example from
# README.features

# TODO: DispatchStrategies should reference the DispatchDict, not the
# DispatchFunc.  This is a more direct code path.

# TODO: DispatchDict -> DispatchTable


# TODO: DispatchSignature takes over DispatchDict's role as a container for
# signatures + implementations.
# It creates DispatchArguments, which are referenced in every DispatchStrategy.



# TODO: pdcast.cast("today", "datetime") uses python datetimes rather than
# pandas


######################
####    PUBLIC    ####
######################


def dispatch(
    *args,
    drop_na: bool = True,
    cache_size: int = 64,
    convert_mixed: bool = False
) -> Callable:
    """A decorator that allows a Python function to dispatch to multiple
    implementations based on the type of one or more of its arguments.

    Parameters
    ----------
    *args : str
        Argument names to dispatch on.  Each of these must be reflected in the
        signature of the decorated function, and will be required in each of
        its overloaded implementations.
    drop_na : bool, default True
        Indicates whether to drop missing values from input vectors before
        forwarding to a dispatched implementation.
    cache_size : int, default 64
        The maximum number of signatures to store in cache.
    convert_mixed_output : bool, default False
        Controls whether to attempt standardization of mixed-type results
        during composite dispatch.  This is only applied if the output type for
        each group belongs to the same family (i.e. multiple variations of int,
        float, datetime, etc.).  This argument is primarily used to allow
        dynamic upcasting for each group during data conversions.

    Returns
    -------
    DispatchFunc
        A cooperative :class:`DispatchFunc` decorator, which manages dispatched
        implementations for the decorated callable.

    Raises
    ------
    TypeError
        If the decorated function does not accept the named arguments, or if no
        arguments are given.

    Notes
    -----
    :meth:`overloaded <pdcast.dispatch.overload>` implementations are searched
    from most specific to least specific, with ties broken from left to right.
    If no specific implementation can be found for the observed input, then the
    decorated function itself will be called as a generic implementation,
    similar to :func:`@functools.singledispatch <functools.singledispatch>`.
    """
    def decorator(func: Callable):
        """Convert a callable into a DispatchFunc object."""
        return DispatchFunc(
            func,
            args=args,
            drop_na=drop_na,
            cache_size=cache_size,
            convert_mixed=convert_mixed
        )

    return decorator


#######################
####    PRIVATE    ####
#######################


class DispatchDict(OrderedDict):
    """An :class:`OrderedDict <python:collections.OrderedDict>` that stores
    types and their dispatched implementations for :class:`DispatchFunc`
    operations.
    """

    def __init__(
        self,
        mapping: dict | None = None,
        cache_size: int = 64
    ):
        super().__init__()
        if mapping:
            for key, val in mapping.items():
                self[key] = val

        self._cache = LRUDict(maxsize=cache_size)
        self._ordered = False

    @classmethod
    def fromkeys(cls, iterable: Iterable, value: Any = None) -> DispatchDict:
        """Implement :meth:`dict.fromkeys` for DispatchDict objects."""
        return DispatchDict(super().fromkeys(iterable, value))

    def get(self, key: type_specifier, default: Any = None) -> Any:
        """Implement :meth:`dict.get` for DispatchDict objects."""
        try:
            return self[key]
        except KeyError:
            return default

    def pop(self, key: type_specifier, default: Any = no_default) -> Any:
        """Implement :meth:`dict.pop` for DispatchDict objects."""
        key = resolve_key(key)
        try:
            result = self[key]
            del self[key]
            return result
        except KeyError as err:
            if default is no_default:
                raise err
            return default

    def setdefault(self, key: type_specifier, default: Callable = None) -> Any:
        """Implement :meth:`dict.setdefault` for DispatchDict objects."""
        key = resolve_key(key)
        try:
            return self[key]
        except KeyError:
            self[key] = default
            return default

    def update(self, other) -> None:
        """Implement :meth:`dict.update` for DispatchDict objects."""
        super().update(DispatchDict(other))
        self._ordered = False

    def _validate_implementation(self, call: Callable) -> None:
        """Ensure that an overloaded implementation is valid."""
        if not callable(call):
            raise TypeError(
                f"overloaded implementation must be callable: {repr(call)}"
            )

    def sort(self) -> None:
        """Sort the dictionary into topological order, with most specific
        keys first.
        """
        sigs = list(self)
        edges = {}

        # group edges by first node
        for _edge in [(a, b) for a in sigs for b in sigs if edge(a, b)]:
            edges.setdefault(_edge[0], []).append(_edge[1])

        # add signatures not contained in edges
        for sig in sigs:
            if sig not in edges:
                edges[sig] = []

        # sort according to edges
        for sig in topological_sort(edges):
            super().move_to_end(sig)

        self._ordered = True

    def __or__(self, other) -> DispatchDict:
        result = self.copy()
        result.update(other)
        return result

    def __ior__(self, other) -> DispatchDict:
        self.update(other)
        return self

    def __getitem__(self, key: type_specifier | tuple[type_specifier]) -> Any:
        key = resolve_key(key)

        # trivial case: key has exact match
        if super().__contains__(key):
            return super().__getitem__(key)

        # sort dict
        if not self._ordered:
            self._cache.clear()
            self.sort()

        # check for cached result
        if key in self._cache:
            return self._cache[key]

        # search for first (sorted) match that fully contains key
        for sig, val in self.items():
            if all(x.contains(y) for x, y in zip(sig, key)):
                self._cache[key] = val
                return val

        # no match found
        raise KeyError(tuple(str(x) for x in key))

    def __setitem__(self, key: type_specifier, value: Callable) -> None:
        key = resolve_key(key)
        self._validate_implementation(value)
        super().__setitem__(key, value)
        self._ordered = False

    def __delitem__(self, key: type_specifier) -> None:
        key = resolve_key(key)

        # require exact match
        if super().__contains__(key):
            return super().__delitem__(key)

        # no match found
        raise KeyError(tuple(str(x) for x in key))

    def __contains__(self, key: type_specifier):
        try:
            self.__getitem__(key)
            return True
        except KeyError:
            return False


class DispatchFunc(FunctionDecorator):
    """A wrapper for the decorated callable that manages its dispatched
    implementations.

    Parameters
    ----------
    func : Callable
        The decorated function or other callable.

    Examples
    --------
    See the docs for :func:`@dispatch <pdcast.dispatch>` for example usage.
    """

    _reserved = (
        FunctionDecorator._reserved |
        {"_args", "_dispatched", "_drop_na", "_flags", "_signature"}
    )

    def __init__(
        self,
        func: Callable,
        args: list[str],
        drop_na: bool,
        cache_size: int,
        convert_mixed: bool
    ):
        super().__init__(func=func)

        if not args:
            raise TypeError(
                f"'{func.__qualname__}' must dispatch on at least one argument"
            )
        self._signature = inspect.signature(func)
        bad = [a for a in args if a not in self._signature.parameters]
        if bad:
            raise TypeError(f"argument not recognized: {bad}")

        self._args = args
        self._drop_na = drop_na
        self._dispatched = DispatchDict(cache_size=cache_size)
        self._convert_mixed = convert_mixed
        self._flags = threading.local()

    @property
    def dispatched(self) -> MappingProxyType:
        """A mapping from :doc:`types </content/types/types>` to their
        dispatched implementations.

        Returns
        -------
        MappingProxyType
            A read-only dictionary mapping types to their associated dispatch
            functions.

        Notes
        -----
        The mapping that is returned by this method is sorted according to the
        order in which implementations are searched when dispatching is
        performed.  If no match is found for any of the 

        Examples
        --------
        .. doctest::

            >>> @dispatch
            ... def foo(bar):
            ...     print("generic implementation")
            ...     return bar

            >>> @foo.overload("int")
            ... def integer_foo(bar):
            ...     print("integer implementation")
            ...     return bar

            >>> foo.dispatched

        .. TODO: check
        """
        return MappingProxyType(self._dispatched)

    def overload(self, *args, **kwargs) -> Callable:
        """A decorator that transforms a naked function into a dispatched
        implementation for this :class:`DispatchFunc`.

        TODO: update this

        Parameters
        ----------
        types : type_specifier | Iterable[type_specifier] | None, default None
            The type(s) to dispatch to this implementation.  See notes for
            handling of :data:`None <python:None>`.

        Returns
        -------
        Callable
            The original undecorated function.

        Raises
        ------
        TypeError
            If the decorated function is not callable with at least one
            argument.

        Notes
        -----
        This decorator works just like the :meth:`register` method of
        :func:`singledispatch <python:functools.singledispatch>` objects,
        except that it does not interact with type annotations in any way.
        Instead, if a type is not provided as an argument to this method, it
        will be disregarded during dispatch lookups, unless the decorated
        callable is a method of an :class:`ScalarType <pdcast.ScalarType>` or
        :class:`DecoratorType <pdcast.DecoratorType>` subclass.  In that case, the
        attached type will be automatically bound to the dispatched
        implementation during
        :meth:`__init_subclass__() <ScalarType.__init_subclass__>`.

        Examples
        --------
        See the :func:`dispatch` :ref:`API docs <dispatch.dispatched>` for
        example usage.
        """

        def implementation(call: Callable) -> Callable:
            """Attach a dispatched implementation to the DispatchFunc with the
            associated types.
            """
            # validate func accepts dispatched arguments
            sig = inspect.signature(call)
            missing = [a for a in self._args if a not in sig.parameters]
            if missing:
                call_name = f"'{call.__module__}.{call.__qualname__}()'"
                raise TypeError(
                    f"{call_name} must accept dispatched arguments: {missing}"
                )

            # bind signature
            try:
                bound = sig.bind_partial(*args, **kwargs).arguments
            except TypeError as err:
                call_name = f"'{call.__module__}.{call.__qualname__}()'"
                reconstructed = [
                    ", ".join(repr(v) for v in args),
                    ", ".join(f"{k}={repr(v)}" for k, v in kwargs.items())
                ]
                reconstructed = ", ".join(s for s in reconstructed if s)
                err_msg = (
                    f"invalid signature for {call_name}: ({reconstructed})"
                )
                raise TypeError(err_msg) from err

            # parse overloaded signature
            paths = []
            missing = []
            for name in self._args:
                if name in bound:
                    paths.append(bound.pop(name))
                else:
                    missing.append(name)
            if missing:
                raise TypeError(f"no signature given for argument: {missing}")
            if bound:
                raise TypeError(
                    f"signature contains non-dispatched arguments: "
                    f"{list(bound)}"
                )

            # generate dispatch keys
            paths = [resolve_type([spec]) for spec in paths]
            paths = list(itertools.product(*paths))

            # merge with dispatch table
            existing = dict(self._dispatched)
            for path in paths:
                previous = existing.get(path, None)
                # if previous:
                #     warn_msg = (
                #         f"Replacing '{previous.__qualname__}()' with "
                #         f"'{call.__qualname__}()' for signature "
                #         f"{tuple(str(x) for x in path)}"
                #     )
                #     warnings.warn(warn_msg, UserWarning, stacklevel=2)
                self._dispatched[path] = call

            return call

        return implementation

    def fallback(self, *args, **kwargs) -> Any:
        """A reference to the default implementation of the decorated function.
        """
        return self.__wrapped__(*args, **kwargs)

    def _build_strategy(self, dispatch_args: dict[str, Any]) -> DispatchStrategy:
        """Normalize vectorized inputs to this :class:`DispatchFunc`.

        This method converts input vectors into pandas Series objects with
        homogenous indices, missing values, and element types.  Composite
        vectors will be grouped along with their homogenous counterparts and
        processed as independent frames.

        Notes
        -----
        Normalization is skipped if the dispatched function is called from a
        recursive context.
        """
        # extract vectors
        vectors = {}
        names = {}
        for arg, value in dispatch_args.items():
            if isinstance(value, pd.Series):
                vectors[arg] = value
                names[arg] = value.name
            elif isinstance(value, np.ndarray):
                vectors[arg] = value
            else:
                value = np.asarray(value, dtype=object)
                if value.shape:
                    vectors[arg] = value

        # bind vectors into DataFrame
        frame = pd.DataFrame(vectors)

        # normalize indices
        original_index = frame.index
        if not isinstance(original_index, pd.RangeIndex):
            frame.index = pd.RangeIndex(0, frame.shape[0])
        if any(v.shape[0] != original_index.shape[0] for v in vectors.values()):
            warn_msg = f"index mismatch in {self.__qualname__}()"
            warnings.warn(warn_msg, UserWarning, stacklevel=2)

        # drop missing values
        hasnans = None
        if self._drop_na:
            frame = frame.dropna(how="any")
            hasnans = frame.shape[0] < original_index.shape[0]

        # detect type of each column
        detected = detect_type(frame)

        # composite case
        if any(isinstance(v, types.CompositeType) for v in detected.values()):
            return CompositeDispatch(
                func=self,
                dispatch_args=dispatch_args,
                frame=frame,
                detected=detected,
                names=names,
                hasnans=hasnans,
                original_index=original_index,
                convert_mixed=self._convert_mixed
            )

        # homogenous case
        return HomogenousDispatch(
            func=self,
            dispatch_args=dispatch_args,
            frame=frame,
            detected=detected,
            names=names,
            hasnans=hasnans,
            original_index=original_index
        )

    def __call__(self, *args, **kwargs) -> Any:
        """Execute the decorated function, dispatching to an overloaded
        implementation if one exists.

        Notes
        -----
        This automatically detects aggregations, transformations, and
        filtrations based on the return value.

            *   :data:`None <python:None>` signifies a filtration.  The passed
                values will be excluded from the resulting series.
            *   A pandas :class:`Series <pandas.Series>` or
                :class:`DataFrame <pandas.DataFrame>`signifies a
                transformation.  Any missing indices will be replaced with NA.
            *   Anything else signifies an aggreggation.  Its result will be
                returned as-is if data is homogenous.  If mixed data is given,
                This will be a DataFrame with rows for each group.

        """
        # bind signature
        bound = self._signature.bind(*args, **kwargs)
        bound.apply_defaults()

        # extract dispatched arguments
        dispatch_args = {arg: bound.arguments[arg] for arg in self._args}

        # call detect_type() on each argument
        

        # fastpath for recursive calls
        if getattr(self._flags, "recursive", False):
            strategy = DirectDispatch(func=self, dispatch_args=dispatch_args)
            return strategy(bound)

        # outermost call - extract/normalize vectors and finalize results
        self._flags.recursive = True
        try:
            strategy = self._build_strategy(dispatch_args)
            return strategy(bound)
        finally:
            self._flags.recursive = False

    def __getitem__(self, key: type_specifier) -> Callable:
        """Get the dispatched implementation for objects of a given type.

        This method searches the implementation space being managed by this
        :class:`DispatchFunc`.  It always returns the same implementation that
        is used when the function is invoked.
        """
        try:
            return self._dispatched[key]
        except KeyError:
            return self.__wrapped__

    def __delitem__(self, key: type_specifier) -> None:
        """Remove an implementation from the pool.
        """
        self._dispatched.__delitem__(key)


#######################
####    PRIVATE    ####
#######################

# DispatchArguments are passed to every DispatchStrategy, along with a reference
# to the parent DispatchFunc.  DirectDispatch, uses its arguments to index the
# DispatchFunc using DispatchArguments.key, and then calls the appropriate
# implementation with *DispatchArguments.args, **DispatchArguments.kwargs.

# DispatchArguments have a .normalize method, which extracts vectors and
# normalizes them.  This is only called if the recursive flag is false.

# CompositeDispatch constructs its own DispatchArguments for each group.


# TODO: DispatchSignature is merged with DispatchDict


class DispatchSignature:
    """Factory object for creating `Signature` objects.

    These are used to extract dispatchable context from the arguments supplied
    to `DispatchFunc.__call__()`.
    """

    def __init__(self, func: Callable, args: tuple[str]):
        self.signature = inspect.signature(func)
        self.args = args

    def __call__(self, *args, **kwargs) -> DispatchArguments:
        """Bind this signature to create a `DispatchArguments` object."""
        # bind *args, **kwargs
        bound = self.signature.bind(*args, **kwargs)
        bound.apply_defaults()

        # extract dispatched arguments
        dispatched = {arg: bound.arguments[arg] for arg in self.args}

        # call detect_type() on each argument
        detected = {arg: detect_type(val) for arg, val in dispatched.items()}

        # build signature
        return DispatchArguments(
            bound=bound,
            dispatched=dispatched,
            detected=detected
        )


class DispatchArguments:
    """A simple wrapper for an `inspect.BoundArguments` object with extra
    context for dispatched arguments and their detected types.
    """

    def __init__(
        self,
        bound: inspect.BoundArguments,
        dispatched: dict[str, Any],
        detected: dict[str, types.Type]
    ):
        self.bound = bound
        self.dispatched = dispatched
        self.detected = detected

    @property
    def arguments(self) -> dict[str, Any]:
        """Get all arguments and their valuues from the bound signature."""
        return self.bound.arguments

    @property
    def args(self) -> tuple[Any, ...]:
        """Get an *args tuple from the bound signature."""
        return self.bound.args

    @property
    def kwargs(self) -> dict[str, Any]:
        """Get a **kwargs dict from the bound signature."""
        return self.bound.kwargs

    @property
    def key(self) -> tuple[types.Type, ...]:
        """Form a key for indexing a DispatchFunc."""
        return tuple(self.detected.values())

    def normalize(self) -> DispatchArguments:
        """Extract vectors from dispatched arguments and normalize them."""
        raise NotImplementedError()


# TODO: before doing anything crazy, just update DispatchFunc/DispatchStrategy
# to use Signatures



# TODO: Signature needs a .vectors property that returns a dict of pd.Series
# objects.

# Maybe Signature.vectors creates a VectorContext object that has no logic
# in __init__.  Instead, it has a .normalize() method that binds the vectors
# into a DataFrame with a normalized index and no missing values, and then
# separates them back out into a dict of pd.Series objects.



# TODO: add a class for normalizing vectorized arguments.

# class VectorContext:
#   __init__(signature: Signature)
#   frame -> pd.DataFrame
#   vectors -> dict[str, pd.Series]
#   groups() -> dict[tuple[VectorType], pd.DataFrame]
#   normalize(result)  # -> replace NAs, reindex, rename

# __init__ gets the Signature from above, extracts the vectors, and binds them
# into a DataFrame with a normalized index and no missing values.  It stores
# the original index and names of each vector for later use.




class DispatchStrategy:
    """Base class for Strategy-Pattern dispatch pipelines."""

    def execute(self, bound: inspect.BoundArguments) -> Any:
        """Abstract method for executing a dispatched strategy."""
        raise NotImplementedError(
            f"strategy does not implement an `execute()` method: "
            f"{self.__qualname__}"
        )

    def finalize(self, result: Any) -> Any:
        """Abstract method to post-process the result of a dispatched strategy.
        """
        raise NotImplementedError(
            f"strategy does not implement a `finalize()` method: "
            f"{self.__qualname__}"
        )

    def __call__(self, bound: inspect.BoundArguments) -> Any:
        """A macro for invoking a strategy's `execute` and `finalize` methods
        in sequence.
        """
        return self.finalize(self.execute(bound))


class DirectDispatch(DispatchStrategy):
    """Dispatch raw inputs to the appropriate implementation."""

    def __init__(
        self,
        func: DispatchFunc,
        dispatch_args: dict[str, Any]
    ):
        self.func = func
        self.dispatch_args = dispatch_args
        self.key = tuple(detect_type(v) for v in dispatch_args.values())

    def execute(self, bound: inspect.BoundArguments) -> Any:
        """Call the dispatched function with the bound arguments."""
        bound.arguments = {**bound.arguments, **self.dispatch_args}
        return self.func[self.key](*bound.args, **bound.kwargs)

    def finalize(self, result: Any) -> Any:
        """Process the result returned by this strategy's `execute` method."""
        return result  # do nothing


class HomogenousDispatch(DispatchStrategy):
    """Dispatch homogenous inputs to the appropriate implementation."""

    def __init__(
        self,
        func: DispatchFunc,
        dispatch_args: dict[str, Any],
        frame: pd.DataFrame,
        detected: dict[str, types.VectorType],
        names: dict[str, str],
        hasnans: bool,
        original_index: pd.Index
    ):
        self.index = frame.index
        self.original_index = original_index
        self.hasnans = hasnans

        # split frame into normalized columns
        for col, series in frame.items():
            series = series.rename(names.get(col, None))
            dispatch_args[col] = series.astype(detected[col].dtype, copy=False)

        # construct DispatchDirect wrapper
        self.direct = DirectDispatch(
            func=func,
            dispatch_args=dispatch_args
        )

    def execute(self, bound: inspect.BoundArguments) -> Any:
        """Call the dispatched implementation with the bound arguments."""
        return self.direct(bound)

    def finalize(self, result: Any) -> Any:
        """Infer mode of operation (filter/transform/aggregate) from return
        type and adjust result accordingly.
        """
        # transform
        if isinstance(result, (pd.Series, pd.DataFrame)):
            # warn if final index is not a subset of original
            if not result.index.difference(self.index).empty:
                warn_msg = (
                    f"index mismatch in {self.__qualname__}() with signature"
                    f"{tuple(str(x) for x in self.direct.key)}: dispatched "
                    f"implementation did not return a like-indexed Series/"
                    f"DataFrame"
                )
                warnings.warn(warn_msg, UserWarning, stacklevel=4)
                self.hasnans = True

            # replace missing values, aligning on index
            if self.hasnans or result.shape[0] < self.index.shape[0]:
                output_type = detect_type(result)
                output_index = pd.RangeIndex(0, self.original_index.shape[0])
                if isinstance(result, pd.Series):
                    nullable = output_type.make_nullable()
                    result = replace_na(
                        result,
                        dtype=nullable.dtype,
                        index=output_index,
                        na_value=nullable.na_value
                    )
                else:
                    with_na = {}
                    for col, series in result.items():
                        nullable = output_type[col].make_nullable()
                        with_na[col] = replace_na(
                            series,
                            dtype=nullable.dtype,
                            index=output_index,
                            na_value=nullable.na_value
                        )
                    result = pd.DataFrame(with_na)

            # replace original index
            result.index = self.original_index

        # aggregate
        return result


class CompositeDispatch(DispatchStrategy):
    """Dispatch composite inputs to the appropriate implementations."""

    def __init__(
        self,
        func: DispatchFunc,
        dispatch_args: dict[str, Any],
        frame: pd.DataFrame,
        detected: dict[str, types.VectorType | types.CompositeType],
        names: dict[str, str],
        hasnans: bool,
        original_index: pd.Index,
        convert_mixed: bool
    ):
        self.func = func
        self.dispatch_args = dispatch_args
        self.frame = frame
        self.names = names
        self.hasnans = hasnans
        self.original_index = original_index
        self.convert_mixed = convert_mixed

        # generate type frame
        type_frame = pd.DataFrame({
            k: getattr(v, "index", v) for k, v in detected.items()
        })

        # group by type
        grouped = type_frame.groupby(list(detected), sort=False)
        self.groups = grouped.groups

    def __iter__(self):
        """Sequentially extract each group from the parent frame"""
        for key, indices in self.groups.items():
            # extract group
            group = self.frame.iloc[indices]

            # bind names to key
            if not isinstance(key, tuple):
                key = (key,)
            detected = dict(zip(group.columns, key))

            # yield to __call__()
            yield detected, group

    def execute(self, bound: inspect.BoundArguments) -> list:
        """For each group in the input, call the dispatched implementation
        with the bound arguments.
        """
        results = []

        # process each group independently
        for detected, group in self:
            strategy = HomogenousDispatch(
                func=self.func,
                dispatch_args=self.dispatch_args,
                frame=group,
                detected=detected,
                names=self.names,
                hasnans=self.hasnans,
                original_index=self.original_index
            )
            result = (detected, strategy.execute(bound))
            results.append(result)

        return results

    def finalize(self, result: list) -> Any:
        computed = [group_result for _, group_result in result]

        # transform
        as_series = all(isinstance(comp, pd.Series) for comp in computed)
        as_dataframe = all(isinstance(comp, pd.DataFrame) for comp in computed)
        if as_series or as_dataframe:
            self._check_indices(series.index for series in computed)

            # return as series
            if as_series:
                return self._combine_series(computed)

            # return as dataframe
            columns = {tuple(df.columns) for df in computed}
            if len(columns) > 1:
                raise ValueError(f"column mismatch: {columns}")
            return pd.DataFrame({
                col: self._combine_series([df[col] for df in computed])
                for col in columns.pop()
            })

        # aggregate
        computed = [
            arg_types | {f"{self.func.__name__}()": pd.Series([comp])}
            for arg_types, comp in result
        ]
        return pd.concat(
            [pd.DataFrame(row, index=[0]) for row in computed],
            ignore_index=True
        )

    def _check_indices(self, indices: Iterator[pd.Index]) -> None:
        """Validate and merge a collection of transformed Series/DataFrame
        indices.
        """
        # check that indices do not overlap with each other
        final_index = next(indices)
        final_size = final_index.shape[0]
        for index in indices:
            final_index = final_index.union(index)
            if len(final_index) < final_size + index.shape[0]:
                warn_msg = (
                    f"index collision in composite {self.func.__name__}()"
                )
                warnings.warn(warn_msg, UserWarning)
            final_size = final_index.shape[0]

        # check that indices are subset of starting index
        if not final_index.difference(self.frame.index).empty:
            # TODO: this results in a KeyError during slice assignment, so
            # just raise it as an error here and in HomogenousDispatch
            warn_msg = "final index is not a subset of original"
            warnings.warn(warn_msg, UserWarning)
            self.hasnans = True

    def _combine_series(self, computed: list) -> pd.Series:
        """Merge the computed series results by index."""
        observed = [detect_type(series) for series in computed]
        unique = set(observed)

        # if results are composite but in same family, attempt to standardize
        if self.convert_mixed and len(unique) > 1:
            roots = {typ.generic.root for typ in unique}
            if any(all(t2.contains(t1) for t1 in unique) for t2 in roots):
                computed, unique = self._standardize_same_family(
                    computed,
                    unique
                )

        # results are homogenous
        if len(unique) == 1:
            final = pd.concat(computed)
            final.sort_index()
            if self.hasnans or final.shape[0] < self.frame.shape[0]:
                nullable = unique.pop().make_nullable()
                final = replace_na(
                    final,
                    dtype=nullable.dtype,
                    index=pd.RangeIndex(0, self.original_index.shape[0]),
                    na_value=nullable.na_value
                )
            final.index = self.original_index
            return final

        # results are composite

        # NOTE: we can't use pd.concat() because it tends to coerce mixed-type
        # results in undesirable ways.  Instead, we manually fold them into a
        # `dtype: object` series to preserve the actual values.

        final = pd.Series(
            np.full(self.original_index.shape[0], pd.NA, dtype=object),
            index=pd.RangeIndex(0, self.original_index.shape[0])
        )
        for series in computed:
            final[series.index] = series
        final.index = self.original_index
        return final

    def _standardize_same_family(
        self,
        computed: list[pd.Series],
        observed: set[types.VectorType]
    ) -> tuple[list[pd.Series], set[types.VectorType]]:
        """If the dispatched implementations return series objects of different
        types within the same family, then attempt to standardize their
        results.

        This method attempts to standardize on the observed type with the
        largest possible range (max - min).  If a tie is encountered, the types
        will be sorted based on the absolute value of their mean range,
        preferring types that are centered toward 0.
        """
        from pdcast.convert import cast

        max_range = max(typ.max - typ.min for typ in observed)
        candidates = [typ for typ in observed if typ.max - typ.min == max_range]
        candidates = sorted(candidates, key=lambda typ: abs(typ.min + typ.max))
        for typ in candidates:
            try:
                computed = [
                    cast(x, typ, downcast=False, errors="raise")
                    for x in computed
                ]
                observed = {typ}
                return computed, observed
            except:
                continue

        return computed, observed


def resolve_key(key: type_specifier | tuple[type_specifier]) -> tuple:
    """Convert arbitrary type specifiers into a valid key for DispatchDict
    lookups.
    """
    if not isinstance(key, tuple):
        key = (key,)

    key_type = []
    for spec in key:
        spec_type = resolve_type(spec)
        if isinstance(spec_type, types.CompositeType):
            raise TypeError(f"key must not be composite: {repr(spec)}")
        key_type.append(spec_type)

    return tuple(key_type)


def supercedes(sig1: tuple, sig2: tuple) -> bool:
    """Check if sig1 is consistent with and strictly more specific than sig2.
    """
    return all(x.contains(y) for x, y in zip(sig2, sig1))


def edge(sig1: tuple, sig2: tuple) -> bool:
    """If ``True``, check sig1 before sig2.

    Ties are broken by recursively backing off the last element of both
    signatures.  As a result, whichever one is more specific in its earlier
    elements will always be preferred.
    """
    # pylint: disable=arguments-out-of-order
    if not sig1 or not sig2:
        return False

    return (
        supercedes(sig1, sig2) and
        not supercedes(sig2, sig1) or edge(sig1[:-1], sig2[:-1])
    )


def topological_sort(edges: dict) -> list:
    """Topological sort algorithm by Kahn (1962).

    Parameters
    ----------
    edges : dict
        A dict of the form `{A: {B, C}}` where `B` and `C` depend on `A`.

    Returns
    -------
    list
        An ordered list of nodes that satisfy the dependencies of `edges`.

    Examples
    --------
    .. doctest::

        >>> topological_sort({1: (2, 3), 2: (3, )})
        [1, 2, 3]

    References
    ----------    
    Kahn, Arthur B. (1962), "Topological sorting of large networks",
    Communications of the ACM
    """
    # edge_count is used to detect cycles
    edge_count = sum(len(dependencies) for dependencies in edges.values())

    # invert edges: {A: {B, C}} -> {B: {A}, C: {A}}
    inverted = {}
    for node, dependencies in edges.items():
        for dependent in dependencies:
            inverted.setdefault(dependent, set()).add(node)

    # Proceed with Kahn topological sort algorithm
    no_incoming = [node for node in edges if node not in inverted]
    result = []
    while no_incoming:

        # pop a node with no incoming edges (order doesn't matter)
        node = no_incoming.pop()
        result.append(node)

        # for each edge from node -> dependent:
        for dependent in edges.get(node, ()):

            # remove edge from inverted map
            inverted[dependent].remove(node)

            # decrement edge count
            edge_count -= 1

            # if dependent has no more incoming edges, add it to the queue
            if not inverted[dependent]:
                no_incoming.append(dependent)

    # if there are any edges left, then there must be a cycle
    if edge_count:
        cycles = [node for node in edges if inverted.get(node, None)]
        raise ValueError(f"edges are cyclic: {cycles}")

    return result


def replace_na(
    series: pd.Series,
    dtype: np.dtype | pd.api.extensions.ExtensionDtype,
    index: pd.Index,
    na_value: Any
) -> pd.Series:
    """Replace any index that is not present in ``result`` with a missing
    value.
    """
    result = pd.Series(
        np.full(index.shape[0], na_value, dtype=object),
        index=index,
        name=series.name,
        dtype=object
    )
    result[series.index] = series
    return result.astype(dtype, copy=False)
