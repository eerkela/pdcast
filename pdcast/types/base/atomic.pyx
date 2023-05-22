"""This module describes an ``AtomicType`` object, which serves as the base
of the ``pdcast`` type system.
"""
import decimal
import inspect
from types import MappingProxyType
from typing import Any, Callable, Iterator

cimport numpy as np
import numpy as np
import pandas as pd
from pandas.api.extensions import ExtensionDtype, register_extension_dtype
import pytz

from pdcast cimport resolve
from pdcast import resolve
from pdcast.util.structs cimport LRUDict
from pdcast.util.type_hints import array_like, type_specifier

from . cimport scalar
from . cimport flyweights
from . cimport adapter
from . cimport composite
from pdcast.types.array import abstract


# TODO: __init_subclass__ returns a proxy, that is removed by @register.  This
# pushes all the updates to the registry at once, rather than requiring checks
# in decorators.



# TODO: add examples/raises for each method

# TODO: remove is_na() in favor of pd.isna() and convert make_nullable into
# a .nullable property.


# conversions
# +------------------------------------------------
# |           | b | i | f | c | d | d | t | s | o |
# +-----------+------------------------------------
# | bool      | x | x | x | x | x | x | x | x | x |
# +-----------+---+---+---+---+---+---+---+---+---+
# | int       | x | x | x | x | x | x | x | x | x |
# +-----------+---+---+---+---+---+---+---+---+---+
# | float     | x | x | x | x | x | x | x | x | x |
# +-----------+---+---+---+---+---+---+---+---+---+
# | complex   | x | x | x | x | x | x | x | x | x |
# +-----------+---+---+---+---+---+---+---+---+---+
# | decimal   | x | x | x | x | x | x | x | x | x |
# +-----------+---+---+---+---+---+---+---+---+---+
# | datetime  | x | x | x | x | x | x | x | x | x |
# +-----------+---+---+---+---+---+---+---+---+---+
# | timedelta | x | x | x | x | x | x | x | x | x |
# +-----------+---+---+---+---+---+---+---+---+---+
# | string    | x | x | x | x | x | x | x | x | x |
# +-----------+---+---+---+---+---+---+---+---+---+
# | object    | x | x | x | x | x | x | x | x | x |
# +-----------+---+---+---+---+---+---+---+---+---+


######################
####    ATOMIC    ####
######################

# Type
# ScalarType
# GenericType
# AtomicType
# DecoratorType
# CompositeType


cdef class AtomicType(scalar.ScalarType):
    """Abstract base class for all user-defined scalar types.

    :class:`AtomicTypes <AtomicType>` are the most fundamental unit of the
    ``pdcast`` type system.  They are used to describe scalar values of a
    particular type (i.e. :class:`int <python:int>`, :class:`numpy.float32`,
    etc.), and are responsible for defining all the necessary implementation
    logic for :doc:`dispatched </content/api/attach>` methods,
    :doc:`conversions </content/api/cast>`, and
    :ref:`type-related <AtomicType.required>` functionality at the scalar
    level.

    Parameters
    ----------
    **kwargs : dict
        Arbitrary keyword arguments describing metadata for this type.  If a
        subclass accepts arguments in its ``__init__`` method, they should
        always be passed here via ``super().__init__(**kwargs)``.  This is
        conceptually equivalent to the ``_metadata`` field of pandas
        :class:`ExtensionDtype <pandas.api.extensions.ExtensionDtype>` objects.

    Examples
    --------
    All in all, a typical :class:`AtomicType` definition could look something like
    this:

    .. code:: python

        @pdcast.register
        @pdcast.subtype(ParentType)
        @GenericType.register_backend("backend name")  # inherits .name
        class BackendType(pdcast.AtomicType, cache_size=128):

            aliases = {"foo", "bar", "baz", np.dtype(np.int64), int, ...}
            type_def = int
            dtype = np.dtype(np.int64)
            itemsize = 8
            na_value = pd.NA

            def __init__(self, x, y):
                # custom arg parsing goes here, along with any new attributes
                super().__init__(x=x, y=y)  # no new attributes after this point

            @classmethod
            def slugify(cls, x, y) -> str:
                return f"cls.name[{str(x)}, {str(y)}]"

            # additional customizations/dispatch methods as needed

    Where ``ParentType`` and ``GenericType`` reference other :class:`AtomicType`
    definitions that ``BackendType`` is linked to.
    """

    ###################################
    ####    REQUIRED ATTRIBUTES    ####
    ###################################

    @property
    def type_def(self) -> type | None:
        """The scalar class for objects of this type.

        Returns
        -------
        type | None
            A class object used to instantiate scalar examples of this type.
        """
        # TODO: raise NotImplementedError?
        return None

    @property
    def dtype(self) -> np.dtype | ExtensionDtype:
        """The numpy :class:`dtype <numpy.dtype>` or pandas
        :class:`ExtensionDtype <pandas.api.extensions.ExtensionDtype>` to use
        for arrays of this type.

        Returns
        -------
        numpy.dtype | pandas.api.extensions.ExtensionDtype
            The dtype to use for arrays of this type.
            :class:`ExtensionDtypes <pandas.api.extensions.ExtensionDtype>` are
            free to define their own storage backends for objects of this type.

        Notes
        -----
        By default, this will automatically create a new
        :class:`ExtensionDtype <pandas.api.extensions.ExtensionDtype>` to
        encapsulate data of this type, storing them internally as a
        ``dtype: object`` array, which may not be the most efficient.  If there
        is a more compact representation for a particular data type, users can
        :ref:`provide <pandas:extending>` their own
        :class:`ExtensionDtype <pandas.api.extensions.ExtensionDtype>`
        instead.
        """
        if not self._dtype:
            return abstract.construct_extension_dtype(
                self,
                is_boolean=self.is_subtype("bool"),
                is_numeric=self.is_numeric,
                add_comparison_ops=True,
                add_arithmetic_ops=True
            )
        return self._dtype

    @property
    def itemsize(self) -> int | None:
        """The size (in bytes) for scalars of this type.

        Returns
        -------
        int | None
            If not :data:`None`, a positive integer describing the size of each
            element in bytes.  If this would be hard to compute, use
            :func:`sys.getsizeof() <python:sys.getsizeof>` or give an
            approximate lower bound here.

        Notes
        -----
        :data:`None` is interpreted as being resizable/unlimited.
        """
        return None

    @property
    def na_value(self) -> Any:
        """The representation to use for missing values of this type.

        Returns
        -------
        Any
            An NA-like value for this data type.

        See Also
        --------
        AtomicType.is_na : for comparisons against this value.
        """
        return pd.NA

    @property
    def is_numeric(self) -> bool:
        """Used to auto-generate :class:`AbstractDtypes <pdcast.AbstractDtype>`
        from this type.
        """
        return False

    ############################
    ####    CONSTRUCTORS    ####
    ############################

    @classmethod
    def slugify(cls) -> str:
        """Generate a string representation of a type.

        This method must have the same arguments as a type's
        :class:`__init__() <AtomicType>` method, and its output determines how
        flyweights are identified.  If a type is not parameterized and does not
        implement a custom :class:`__init__() <AtomicType>` method, this can be
        safely omitted in subclasses.

        Returns
        -------
        str
            A string that fully specifies the type.  The string must be unique
            for every set of inputs, as it is used to look up flyweights.

        Notes
        -----
        This method is always called **before** initializing a new
        :class:`AtomicType`.  The uniqueness of its result determines whether a
        new flyweight will be generated for this type.
        """
        # NOTE: we explicitly check for _is_generic=False, which signals that
        # @register_backend has been explicitly called on this type.
        if cls._is_generic == False:
            return f"{cls.name}[{cls._backend}]"
        return cls.name

    @classmethod
    def resolve(cls, *args: str) -> AtomicType:
        """Construct a new :class:`AtomicType` in the :ref:`type specification
        mini-language <resolve_type.mini_language>`.

        Override this if a type implements custom parsing rules for any
        arguments that are supplied to it.

        Parameters
        ----------
        *args : str
            Positional arguments supplied to this type.  These will always be
            passed as strings, exactly as they appear in the :ref:`type
            specification mini-language <resolve_type.mini_language>`.

        See Also
        --------
        AdapterType.resolve : the adapter equivalent of this method.

        Returns
        -------
        AtomicType
            A flyweight for the specified type.  If this method is given the
            same inputs again in the future, then this will be a simple
            reference to the previous instance.
        """
        # NOTE: Most types don't accept any arguments at all
        return cls.instance(*args)

    @classmethod
    def detect(cls, example: Any) -> AtomicType:
        """Construct a new :class:`AtomicType` from scalar example data.

        Override this if a type has attributes that depend on the value of a
        corresponding scalar (e.g. datetime64 units, timezones, etc.)

        Parameters
        ----------
        example : Any
            A scalar example of this type (e.g. ``1``, ``42.0``, ``"foo"``,
            etc.).

        Returns
        -------
        AtomicType
            A flyweight for the specified type.  If this method is given the
            same input again in the future, then this will be a simple
            reference to the previous instance.

        Notes
        -----
        This method is called during :func:`detect_type` operations when there
        is no explicit ``.dtype`` field to interpret.  This might be the case
        for objects that are stored in a base Python list or ``dtype: object``
        array, for instance.

        In order for this method to be called, the output of
        :class:`type() <python:type>` on the example must be registered as one
        of this type's :attr:`aliases <AtomicType.aliases>`.

        If the input to :func:`detect_type` is vectorized, then this method
        will be called at each index.
        """
        # NOTE: most types disregard example data
        return cls.instance()

    @classmethod
    def from_dtype(cls, dtype: np.dtype | ExtensionDtype) -> AtomicType:
        """Construct an :class:`AtomicType` from a corresponding numpy/pandas
        :class:`dtype <numpy.dtype>`\ /\
        :class:`ExtensionDtype <pandas.api.extensions.ExtensionDtype>` object.

        Override this if a type must parse the attributes of an associated
        :class:`dtype <numpy.dtype>` or
        :class:`ExtensionDtype <pandas.api.extensions.ExtensionDtype>`.

        Parameters
        ----------
        dtype : np.dtype | ExtensionDtype
            The numpy :class:`dtype <numpy.dtype>` or pandas
            :class:`ExtensionDtype <pandas.api.extensions.ExtensionDtype>` to
            parse.

        Returns
        -------
        AtomicType
            A flyweight for the specified type.  If this method is given the
            same input again in the future, then this will be a simple
            reference to the previous instance.

        See Also
        --------
        AdapterType.from_dtype : the adapter equivalent of this method.

        Notes
        -----
        For numpy :class:`dtypes <numpy.dtype>`, the input to this function
        must be a member of the type's :attr:`aliases <AtomicType.aliases>`
        attribute.

        For pandas :class:`ExtensionDtypes <pandas.api.extensions.ExtensionDtype>`,
        the input's **type** must be a member of
        :attr:`aliases <AtomicType.aliases>`, not the dtype itself.  This
        asymmetry allows pandas dtypes to be arbitrarily parameterized when
        passed to this method.
        """
        return cls.instance()  # NOTE: most types disregard dtype metadata

    def replace(self, **kwargs) -> AtomicType:
        """Return a modified copy of a type with the values specified in
        ``**kwargs``.

        Parameters
        ----------
        **kwargs : dict
            keyword arguments corresponding to attributes of this type.  Any
            arguments that are not specified will be replaced with the current
            values for this type.

        Returns
        -------
        AtomicType
            A flyweight for the specified type.  If this method is given the
            same input again in the future, then this will be a simple
            reference to the previous instance.

        Notes
        -----
        This method respects the immutability of :class:`AtomicType` objects.
        It always returns a flyweight with the new values.
        """
        cdef dict merged = {**self.kwargs, **kwargs}
        return self.instance(**merged)

    ###################################
    ####    SUBTYPES/SUPERTYPES    ####
    ###################################

    @property
    def is_root(self) -> bool:
        """Indicates whether this type is the root of its
        :func:`@subtype <subtype>` hierarchy.

        Returns
        -------
        bool
            ``True`` if this type has no :attr:`supertype
            <AtomicType.supertype>`, ``False`` otherwise.
        """
        return self._parent is None

    @property
    def root(self) -> AtomicType:
        """The root node of this type's subtype hierarchy."""
        if self.is_root:
            return self
        return self.supertype.root

    def _generate_supertype(self, supertype: type) -> AtomicType:
        """Transform a (possibly null) supertype definition into its
        corresponding instance.

        Override this if your AtomicType implements custom logic to generate
        supertype instances (due to an interface mismatch or similar obstacle).
        
        Parameters
        ----------
        supertype : type
            A parent :class:`AtomicType` definition that has not yet been
            instantiated.  This can be ``None``, indicating that the type has
            not been decorated with :func:`@subtype <subtype>`.

        Returns
        -------
        AtomicType | None
            The transformed equivalent of the input type, converted to its
            corresponding instance.  This method is free to determine how this
            is done.

        Notes
        -----
        This method will only be called once, with the result being cached
        until the shared :class:`TypeRegistry` is next updated.
        """
        if supertype is None or supertype not in self.registry:
            return None
        return supertype.instance()

    @property
    def supertype(self) -> AtomicType:
        """An :class:`AtomicType` representing the supertype that this type is
        registered to, if one exists.

        Notes
        -----
        The result of this accessor is cached between :class:`TypeRegistry`
        updates.
        """
        if self.registry.needs_updating(self._supertype_cache):
            result = self._generate_supertype(self._parent)
            self._supertype_cache = self.registry.remember(result)

        return self._supertype_cache.value

    def _generate_subtypes(self, types: set) -> set:
        """Transform a set of subtype definitions into their corresponding
        instances.

        Override this if your AtomicType implements custom logic to generate
        subtype instances (such as wildcard behavior or similar functionality).

        Parameters
        ----------
        types : set
            A set containing uninstantiated subtype definitions.  This
            represents all the types that have been decorated with the
            :func:`@subtype <subtype>` decorator with this type as one of their
            parents.

        Returns
        -------
        set
            A set containing the transformed equivalents of the input types,
            converted into their corresponding instances.  This method is free
            to determine how this is done.

        Notes
        -----
        This method will only be called once, with the result being cached
        until the shared :class:`TypeRegistry` is next updated.
        """
        result = set()
        for t in types:  # skip invalid kwargs
            try:
                result.add(t.instance(**self.kwargs))
            except TypeError:
                continue

        return result

    @property
    def subtypes(self) -> composite.CompositeType:
        """A :class:`CompositeType` containing every subtype that is
        currently registered to this type.

        Notes
        -----
        The result of this accessor is cached between :class:`TypeRegistry`
        updates.
        """
        if self.registry.needs_updating(self._subtype_cache):
            result = composite.CompositeType(
                self._generate_subtypes(traverse_subtypes(type(self)))
            )
            self._subtype_cache = self.registry.remember(result)

        return self._subtype_cache.value

    def contains(
        self,
        other: type_specifier,
        include_subtypes: bool = True
    ) -> bool:
        """Test whether ``other`` is a member of this type's hierarchy tree.

        Override this to change the behavior of the `in` keyword and implement
        custom logic for membership tests for the given type.

        Parameters
        ----------
        other : type_specifier
            The type to check for membership.  This can be in any
            representation recognized by :func:`resolve_type`.
        include_subtypes : bool, default True
            Controls whether to include subtypes for this comparison.  If this
            is set to ``False``, then subtypes will be excluded.  Backends will
            still be considered, but only at the top level.

        Returns
        -------
        bool
            ``True`` if ``other`` is a member of this type's hierarchy.
            ``False`` otherwise.

        Notes
        -----
        This method also controls the behavior of the ``in`` keyword for
        :class:`AtomicTypes <AtomicType>`.
        """
        other = resolve.resolve_type(other)
        if isinstance(other, composite.CompositeType):
            return all(
                self.contains(o, include_subtypes=include_subtypes)
                for o in other
            )

        # self.backends includes self
        for backend in self.backends.values():
            if other == backend:
                return True
            if include_subtypes:
                subtypes = set(backend.subtypes) - {self}
                if any(s.contains(other) for s in subtypes):
                    return True

        return False

    def is_subtype(
        self,
        other: type_specifier,
        include_subtypes: bool = True
    ) -> bool:
        """Reverse of :meth:`AtomicType.contains`.

        Parameters
        ----------
        other : type specifier
            The type to check for membership.  This can be in any
            representation recognized by :func:`resolve_type`.
        include_subtypes : bool, default True
            Controls whether to include subtypes for this comparison.  If this
            is set to ``False``, then subtypes will be excluded.  Backends will
            still be considered, but only at the top level.

        Returns
        -------
        bool
            ``True`` if ``self`` is a member of ``other``\'s hierarchy.
            ``False`` otherwise.

        Notes
        -----
        This method performs the same check as :meth:`AtomicType.contains`,
        except in reverse.  It is functionally equivalent to
        ``other.contains(self)``.
        """
        other = resolve.resolve_type(other)
        return other.contains(self, include_subtypes=include_subtypes)

    #######################
    ####    GENERIC    ####
    #######################

    @property
    def is_generic(self) -> bool:
        """Indicates whether this type is decorated with
        :func:`@generic <generic>`.
        """
        return self._is_generic

    @property
    def backend(self) -> str:
        """The backend string used to refer to this type in the
        :ref:`type specification mini-language <resolve_type.mini_language>`.
        """
        return self._backend

    @property
    def backends(self) -> MappingProxyType:
        """A dictionary mapping backend specifiers to their corresponding
        implementation types.

        This dictionary always contains the key/value pair ``{None: self}``.
        """
        if self.registry.needs_updating(self._backend_cache):
            result = {None: self}
            result |= {
                k: v.instance(**self.kwargs) for k, v in self._backends.items()
            }
            result = MappingProxyType(result)
            self._backend_cache = self.registry.remember(result)

        return self._backend_cache.value

    @property
    def generic(self) -> AtomicType:
        """The generic equivalent of this type, if one exists."""
        if self.registry.needs_updating(self._generic_cache):
            if self.is_generic:
                result = self
            elif self._generic is None:
                result = None
            else:
                result = self._generic.instance()
            self._generic_cache = self.registry.remember(result)
        return self._generic_cache.value

    @classmethod
    def register_backend(cls, backend: str):
        """A decorator that allows individual backends to be added to generic
        types.

        Parameters
        ----------
        backend : str
            The string to use as this type's backend specifier.

        Notes
        -----
        The implementation for this method can be found within the
        :func:`@generic <generic>` decorator itself.  Users do not need to
        implement it themselves.
        """
        raise NotImplementedError(f"'{cls.__name__}' is not generic")

    ########################
    ####    ADAPTERS    ####
    ########################

    def make_categorical(
        self,
        series: pd.Series,
        levels: list = None
    ) -> pd.Series:
        """Transform a series of the associated type into a categorical format
        with the given levels.

        This method is invoked whenever a categorical conversion is performed
        that targets this type.

        Parameters
        ----------
        series : pd.Series
            The series to be transformed.
        levels : list
            The categories to use for the transformation.  If this is ``None``
            (the default), then levels will be automatically discovered when
            this method is called.

        Returns
        -------
        pd.Series
            The transformed series.

        Notes
        -----
        If a type implements custom logic when performing a categorical
        conversion, it should be implemented here.
        """
        if levels is None:
            categorical_type = pd.CategoricalDtype()
        else:
            categorical_type = pd.CategoricalDtype(
                pd.Index(levels, dtype=self.dtype)
            )

        return series.astype(categorical_type)

    def make_sparse(
        self,
        series: pd.Series,
        fill_value: Any = None
    ) -> pd.Series:
        """Transform a series of the associated type into a sparse format with
        the given fill value.

        This method is invoked whenever a sparse conversion is performed that
        targets this type.

        Parameters
        ----------
        series : pd.Series
            The series to be transformed.
        fill_value : Any
            The fill value to use for the transformation.  If this is ``None``
            (the default), then this type's ``na_value`` will be used instead.

        Returns
        -------
        pd.Series
            The transformed series.

        Notes
        -----
        If a type implements custom logic when performing a sparse conversion,
        it should be implemented here.
        """
        if fill_value is None:
            fill_value = self.na_value
        sparse_type = pd.SparseDtype(series.dtype, fill_value)
        return series.astype(sparse_type)

    ###############################
    ####    UPCAST/DOWNCAST    ####
    ###############################

    @property
    def larger(self) -> list:
        """A list of types that this type can be
        :meth:`upcasted <AtomicType.upcast>` to in the event of overflow.

        Override this to change the behavior of a bounded type (with
        appropriate `.min`/`.max` fields) when an ``OverflowError`` is
        detected.

        Notes
        -----
        Candidate types will always be tested in order.
        """
        return []  # NOTE: most types cannot be upcasted

    @property
    def smaller(self) -> list:
        """A list of types that this type can be
        :meth:`downcasted <AtomicType.downcast>` to if directed.

        Override this to change the behavior of a type when the ``downcast``
        argument is supplied to a conversion function.

        Notes
        -----
        Candidate types will always be tested in order.
        """
        return []

    ##############################
    ####    MISSING VALUES    ####
    ##############################

    @property
    def is_nullable(self) -> bool:
        """Indicates whether a type supports missing values.

        Set this ``False`` where necessary to invoke :meth:`make_nullable
        <AtomicType.make_nullable>`.  This allows automatic conversion to a
        nullable alternative when missing values are detected/coerced.
        """
        return True

    def is_na(self, val: Any) -> bool | array_like:
        """Check if one or more values are considered missing in this
        representation.

        Parameters
        ----------
        val : Any
            A scalar or 1D vector of values to check for NA equality.

        Returns
        -------
        bool | array-like
            ``True`` where ``val`` is equal to this type's ``na_value``,
            ``False`` otherwise.  If the input is vectorized, then the output
            will be as well.

        Notes
        -----
        Comparison with missing values is often tricky.  Most NA values are not
        equal to themselves, so some other algorithm must be used to test for
        them.  This method allows users to define this logic on a per-type
        basis.

        If you override this method, you should always call its base equivalent
        via ``super().is_na()`` before returning a custom result.
        """
        return pd.isna(val)

    def make_nullable(self) -> AtomicType:
        """Convert a non-nullable :class:`AtomicType` into one that can accept
        missing values.

        Override this to control how this type is coerced when missing values
        are detected during a :func:`cast` operation. 

        Returns
        -------
        AtomicType
            A nullable version of this data type to be used when missing or
            coerced values are detected during a conversion.
        """
        if self.is_nullable:
            return self

        raise NotImplementedError(
            f"'{type(self).__name__}' objects have no nullable alternative."
        )

    ###############################
    ####    SPECIAL METHODS    ####
    ###############################

    @classmethod
    def __init_subclass__(cls, cache_size: int = None, **kwargs):
        """Metaclass initializer.

        This method is responsible for
        `initializing subclasses <https://peps.python.org/pep-0487/>`_ of
        :class:`AtomicType`.
        """
        # allow cooperative inheritance
        super(AtomicType, cls).__init_subclass__(**kwargs)

        # limit to 1st order
        valid = AtomicType.__subclasses__()
        if cls not in valid:
            raise TypeError(
                f"{cls.__name__} cannot inherit from another AtomicType "
                f"definition"
            )

        cls.instance = flyweights.FlyweightFactory(
            cls,
            cache_size=0 if cache_size is None else cache_size
        )

        # init fields for @subtype
        cls._children = set()
        cls._parent = None

        # init fields for @generic
        cls._is_generic = None
        cls._generic = None
        cls._backend = None
        cls._backends = {}


#######################
####    GENERIC    ####
#######################


def generic(cls: type) -> GenericType:
    """Class decorator to mark generic type definitions.

    Generic types are backend-agnostic and act as wildcard containers for
    more specialized subtypes.  For instance, the generic "int" can contain
    the backend-specific "int[numpy]", "int[pandas]", and "int[python]"
    subtypes, which can be resolved as shown. 
    """
    return GenericType(cls)


cdef class GenericType(AtomicType):
    """A hierarchical type that can contain other types.
    """

    def __init__(self, cls):
        self._validate_class(cls)

        self.__wrapped__ = cls
        print(cls)
        self._default = cls.instance()
        self.name = cls.name
        self.aliases = cls.aliases
        self._subtypes = composite.CompositeType()
        self._backends = {None: self._default}

    def _validate_class(self, cls) -> None:
        """Ensure that the decorated class is valid."""
        if not issubclass(cls, AtomicType):
            raise TypeError("@generic types must inherit from AtomicType")

        # NOTE: cython __init__ is not introspectable.
        if (
            cls.__init__ != AtomicType.__init__ and
            inspect.signature(cls).parameters
        ):
            raise TypeError("@generic types cannot be parametrized")

    ############################
    ####    CONSTRUCTORS    ####
    ############################

    def instance(
        self,
        backend: str | None = None,
        *args,
        **kwargs
    ) -> AtomicType:
        """Forward constructor arguments to the appropriate implementation."""
        return self._backends[backend].instance(*args, **kwargs)

    def resolve(
        self,
        backend: str | None = None,
        *args
    ) -> AtomicType:
        """Forward constructor arguments to the appropriate implementation."""
        return self._backends[backend].resolve(*args)    
    
    ##########################
    ####    DECORATORS    ####
    ##########################

    def subtype(self, cls: type | None = None, *, **kwargs):
        """A class decorator that adds a type as a subtype of this GenericType.
        """
        def decorator(cls: type) -> type:
            """Link the decorated type to this GenericType."""
            if not issubclass(cls, AtomicType):
                raise TypeError("@generic types can only contain AtomicTypes")

            if cls._parent:
                raise TypeError(
                    f"AtomicTypes can only be registered to one @generic type "
                    f"at a time: '{cls.__qualname__}' is currently registered "
                    f"to '{cls._parent.__qualname__}'"
                )

            curr = cls
            while curr is not None:
                if curr is self:
                    raise TypeError("@generic type cannot contain itself")
                curr = curr._parent

            cls._parent = self
            self._subtypes |= cls.instance(**kwargs)
            self.registry.flush()
            return cls

        if cls is None:
            return decorator
        return decorator(cls)

    def implementation(self, backend: str = None, **kwargs):
        """A class decorator that adds a type as an implementation of this
        type.
        """
        def decorator(cls: type) -> type:
            """Link the decorated type to this GenericType."""
            if not issubclass(cls, AtomicType):
                raise TypeError("@generic types can only contain AtomicTypes")

            if not isinstance(backend, str):
                raise TypeError(
                    f"backend specifier must be a string, not {type(backend)}"
                )

            if backend in self._backends:
                raise TypeError(
                    f"backend specifier must be unique: {repr(backend)} is "
                    f"already registered to {str(self._backends[backend])}"
                )

            # ensure backend is self-consistent
            if cls._backend is None:
                cls._backend = backend
            elif backend != cls._backend:
                raise TypeError(
                    f"backend specifiers must match ({repr(backend)} != "
                    f"{repr(cls._backend)})"
                )

            cls._is_generic = False
            cls._generic = self
            cls.name = self.name
            self._backends[backend] = cls.instance(**kwargs)
            self.registry.flush()
            return cls

        return decorator

    #########################
    ####    HIERARCHY    ####
    #########################

    @property
    def is_generic(self) -> bool:
        return True

    @property
    def generic(self) -> AtomicType:
        return self

    @property
    def subtypes(self) -> composite.CompositeType:
        return self._subtypes.copy()

    @property
    def backends(self) -> dict:
        return MappingProxyType(self._backends)

    @property
    def children(self) -> composite.CompositeType:
        return self._subtypes | composite.CompositeType(self._backends.values())

    #################################
    ####    COMPOSITE PATTERN    ####
    #################################

    @property
    def default(self) -> AtomicType:
        """The concrete type that this generic type defaults to.

        This will be used whenever the generic type is specified without an
        explicit backend.
        """
        return self._default

    @default.setter
    def default(self, val: type_specifier) -> None:
        if val is None:
            del self.default
        else:
            val = resolve.resolve_type(val)
            if isinstance(val, composite.CompositeType):
                raise ValueError
            if val not in self.children:
                raise KeyError

            self._default = val

    @default.deleter
    def default(self) -> None:
        self._default = self._backends[None]

    def __getattr__(self, name: str) -> Any:
        return getattr(self.default, name)

    def __getitem__(self, key: str) -> AtomicType:
        # this should be part of the AtomicType interface
        return self._backends[key[:1]].instance(*key[1:])




#######################
####    PRIVATE    ####
#######################


cdef void _traverse_subtypes(type atomic_type, set result):
    """Recursive helper for traverse_subtypes()"""
    result.add(atomic_type)
    for subtype in atomic_type._children:
        if subtype in atomic_type.registry:
            _traverse_subtypes(subtype, result=result)


cdef set traverse_subtypes(type atomic_type):
    """Traverse through an AtomicType's subtype tree, recursively gathering
    every subtype definition that is contained within it or any of its
    children.
    """
    cdef set result = set()
    _traverse_subtypes(atomic_type, result)  # in-place
    return result
