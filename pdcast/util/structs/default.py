from __future__ import annotations
from functools import update_wrapper, wraps
import inspect
import threading
from types import MappingProxyType
from typing import Any, Callable


######################
####    PUBLIC    ####
######################


def extension_func(func: Callable) -> Callable:
    """A decorator that transforms a Python function into a thread-local
    :class:`ExtensionFunc <BaseExtensionFunc>` object.

    Parameters
    ----------
    func : Callable
        A function to decorate.  This must accept a ``**kwargs`` dict or
        similar variable-length keyword argument.

    Returns
    -------
    Callable
        A callable :class:`ExtensionFunc <BaseExtensionFunc>` object, which
        manages default values and argument validators for the decorated
        function.

    Raises
    ------
    TypeError
        If the decorated function does not accept variable-length keyword
        arguments.

    Examples
    --------
    By default, the decorated function behaves exactly like the original.

    .. doctest::

        >>> @pdcast.extension_func
        ... def foo(bar, baz=2, **kwargs):
        ...     return bar, baz

        >>> foo
        foo(bar, baz = 2, **kwargs)
        >>> foo(1)
        (1, 2)
        >>> foo(1, 3)
        (1, 3)
        >>> foo(1, 3, qux=4)
        (1, 3)
        >>> foo()
        Traceback (most recent call last):
            ...
        TypeError: foo() missing 1 required positional argument: 'bar'

    We can manage the values that are supplied to it by defining
    :meth:`validators <BaseExtensionFunc.register_arg>` for one or more of its
    arguments.

    .. doctest::

        >>> @foo.register_arg
        ... def bar(val: int, defaults: dict) -> int:
        ...     return int(val)

    This validator will be implicitly executed whenever ``bar`` is supplied to
    ``foo()``.

    .. doctest::

        >>> foo("a", 2)
        Traceback (most recent call last):
            ...
        ValueError: invalid literal for int() with base 10: 'a'

    :class:`ExtensionFunc <BaseExtensionFunc>` also allows us to
    programmatically assign/modify default values for our managed arguments.

    .. doctest::

        >>> foo.bar = 1
        >>> foo
        foo(bar = 1, baz = 2, **kwargs)
        >>> foo.bar
        1
        >>> foo()
        (1, 2)

    We can also do this by supplying a ``default`` argument to the
    :func:`@register_arg <BaseExtensionFunc.register_arg>` decorator.

    .. testsetup::

        foo.remove_arg("bar")

    .. doctest::

        >>> @foo.register_arg(default=1)
        ... def bar(val: int, defaults: dict) -> int:
        ...     return int(val)

        >>> foo.bar
        1

    Or by assigning a default value in the signature of ``foo()`` itself, as
    with ``baz``:

    .. doctest::

        >>> @foo.register_arg
        ... def baz(val: int, defaults: dict) -> int:
        ...     return int(val)

        >>> foo.baz
        2

    Defaults can be updated at run time to globally change the behavior of
    ``foo()``.

    .. doctest::

        >>> foo.bar = 24
        >>> foo.baz = -17
        >>> foo()
        (24, -17)

    These values are thread-local:

    .. doctest::

        >>> import random
        >>> random.seed(5)
        >>> values = list(np.arange(-50, 50))

        >>> def worker():
        ...     foo.bar, foo.baz = random.sample(values, 2)
        ...     print(foo())

        >>> import threading
        >>> threads = [threading.Thread(target=worker) for _ in range(3)]
        >>> for t in threads:
        ...     t.start()
        (79, 32)
        (94, 45)
        (88, 94)
        >>> foo()
        (24, -17)

    Additionally, each argument can be reset to its default value by simply
    deleting the attribute:

    .. doctest::

        >>> foo
        foo(bar = 24, baz = -17, **kwargs)
        >>> del foo.bar, foo.baz
        >>> foo
        foo(bar, baz = 2, **kwargs)
        >>> foo(1)
        (1, 2)

    .. note::

        Unless a default is assigned in
        :meth:`@register_arg <BaseExtensionFunc.register_arg>` or the signature
        of ``foo()`` itself, deleting the argument will make it required
        whenever ``foo()`` is invoked.

        .. doctest::

            >>> foo()
            Traceback (most recent call last):
                ...
            TypeError: foo() missing 1 required positional argument: 'bar'

        Note that this does not remove the underlying validator.

        .. doctest::

            >>> foo("a")
            Traceback (most recent call last):
                ...
            ValueError: invalid literal for int() with base 10: 'a'

        To purge a managed argument entirely, use
        :meth:`@remove_arg <BaseExtensionFunc.remove_arg>`

    :func:`@register_arg <BaseExtensionFunc.register_arg>` also allows us to
    dynamically add new arguments to ``foo()`` at run time, with the same
    validation logic as the others.

    .. doctest::

        >>> @foo.register_arg(default=3)
        ... def qux(val: int, defaults: dict) -> int:
        ...     return int(val)

        >>> foo
        foo(bar, baz = 2, qux = 3, **kwargs)
        >>> foo(1, qux="a")
        Traceback (most recent call last):
            ...
        ValueError: invalid literal for int() with base 10: 'a'
    """
    main_thread = []  # using a list bypasses UnboundLocalError

    class ExtensionFunc(BaseExtensionFunc):
        """A subclass of :class:`BaseExtensionFunc` that supports dynamic
        assignment of ``@properties`` without affecting other instances.
        """

        def __init__(self, _func: Callable):
            super().__init__(_func)
            update_wrapper(self, _func)

            # store attributes from main thread
            if threading.current_thread() == threading.main_thread():
                main_thread.append({
                    "class": type(self),
                    "_vals": self._vals,
                    "_defaults": self._defaults,
                    "_validators": self._validators
                })

            # copy attributes from main thread
            else:
                main = main_thread[0]
                for k in main["_validators"]:
                    setattr(type(self), k, getattr(main["class"], k))
                self._vals = main["_vals"].copy()
                self._defaults = main["_defaults"].copy()
                self._validators = main["_validators"].copy()

    return ExtensionFunc(func)


#######################
####    PRIVATE    ####
#######################


class NoDefault:
    """Signals that an argument does not have an associated default value."""

    def __repr__(self) -> str:
        return "<no default>"


no_default = NoDefault()


class BaseExtensionFunc(threading.local):
    """A callable object that can be dynamically extended with custom
    arguments.

    Parameters
    ----------
    _func : Callable
        A function to decorate.  This must accept a ``**kwargs`` dict or
        similar variable-length keyword argument.

    Raises
    ------
    TypeError
        If the decorated function does not accept variable-length keyword
        arguments.

    Notes
    -----
    Whenever an argument is :meth:`registered <BaseExtensionFunc.register_arg>`
    to this object, it is added as a managed ``@property``.  This automatically
    generates an appropriate getter, setter, and deleter for the attribute
    based on the decorated validation function.

    Additionally, this object inherits from ``threading.local``.  If the
    decorated function is referenced in a child thread, a new instance will be
    dynamically created with arguments and defaults from the main thread.  This
    instance can then be modified without affecting the behavior of any other
    threads.

    Examples
    --------
    See the docs for :func:`extension_func` for example usage.
    """

    def __init__(self, _func: Callable):
        super().__init__()
        self._signature = inspect.signature(_func)

        # assert function accepts **kwargs
        self._kwargs_name = None
        for p in self._signature.parameters.values():
            if p.kind == p.VAR_KEYWORD:
                self._kwargs_name = p.name
                break
        if self._kwargs_name is None:
            raise TypeError(f"func must accept **kwargs")

        update_wrapper(self, _func)
        self._func = _func
        self._vals = {}
        self._defaults = {}
        self._validators = {}

    ####################
    ####    BASE    ####
    ####################

    @property
    def default_values(self) -> MappingProxyType:
        """A mapping of all argument names to their associated default values
        for this :class:`ExtensionFunc <BaseExtensionFunc>`.

        Returns
        -------
        MappingProxyType
            A read-only dictionary suitable for use as the ``**kwargs`` input
            to the decorated function.

        Notes
        -----
        If no default value is associated with an argument, then it will be
        excluded from this dictionary.
        """
        return MappingProxyType({**self._defaults, **self._vals})

    @property
    def validators(self) -> MappingProxyType:
        """A mapping of argument names to their respective validators.

        Returns
        -------
        MappingProxyType
            A read-only dictionary mapping argument names to their associated
            validation functions.

        Examples
        --------
        .. doctest::

            >>> @pdcast.extension_func
            ... def foo(bar, baz, **kwargs):
            ...     return bar, baz

            >>> @foo.register_arg
            ... def bar(val: int, defaults: dict) -> int:
            ...     return int(val)

            >>> foo.validators   # doctest: +SKIP
            mappingproxy({'bar': <function bar at 0x7ff5ad9c6e60>})
        """
        return MappingProxyType(self._validators)

    def register_arg(
        self,
        _func: Callable = None,
        *,
        default: Any = no_default
    ) -> Callable:
        """A decorator that transforms a naked validation function into a
        managed argument for :class:`ExtensionFunc <BaseExtensionFunc>`.

        Parameters
        ----------
        default : Optional[Any]
            The default value to use for this argument.  This is implicitly
            passed to the validator itself, so any custom logic that is
            implemented there will also be applied to this value.  If this
            argument is omitted and the decorated function defines a default
            value in its call signature, then that value will be used instead.

        Returns
        -------
        Callable
            A decorated version of the validation function that automatically
            fills out its ``defaults`` argument.  This function will
            implicitly be called whenever the
            :class:`ExtensionFunc <BaseExtensionFunc>` is executed.

        Notes
        -----
        A validation function must have the following signature:

        .. code:: python

            def validator(val, defaults):
                ...

        Where ``val`` can be an arbitrary input to the argument and
        ``defaults`` is a dictionary containing the current parameter space for
        each argument.  If the validator interacts with other arguments (via
        mutual exclusivity, for instance), then they can be obtained from
        ``defaults``.

        .. note::

            Race conditions may be introduced when arguments access each other
            in their validators.  This can be mitigated by using ``dict.get()``
            with a default value rather than relying on direct access, as well
            as manually applying the same coercions as in the referenced
            argument's validation function.

        Examples
        --------
        See the docs for :func:`extension_func` for example usage.
        """

        def argument(validator: Callable) -> Callable:
            """Attach a validation function to the ExtensionFunc as a managed
            property.
            """
            # use name of validation function as argument name
            name = validator.__name__
            if name in self._validators:
                raise KeyError(f"default argument '{name}' already exists.")

            # wrap validator to accept default arguments
            @wraps(validator)
            def accept_default(val, defaults=self.default_values):
                return validator(val, defaults=defaults)

            # compute and validate default value
            if default is no_default:
                pars = self._signature.parameters
                if name in pars and pars[name].default is not inspect._empty:
                    self._defaults[name] = pars[name].default
            else:
                self._defaults[name] = accept_default(default)

            # generate getter, setter, and deleter attributes for @property
            def getter(self) -> Any:
                if name in self._vals:
                    return self._vals[name]
                if name in self._defaults:
                    return self._defaults[name]
                raise AttributeError(f"'{name}' has no default value")

            def setter(self, val: Any) -> None:
                self._vals[name] = accept_default(val)

            def deleter(self) -> None:
                if name in self._defaults:
                    accept_default(self._defaults[name])
                self._vals.pop(name, None)

            # attach @property to DefaultFunc
            prop = property(
                getter,
                setter,
                deleter,
                doc=validator.__doc__
            )
            setattr(type(self), name, prop)

            # make decorated validator available from CastDefaults and return
            self._validators[name] = accept_default
            return accept_default

        if _func is None:
            return argument
        return argument(_func)

    def remove_arg(self, *args: str) -> None:
        """Remove a registered argument from an
        :class:`ExtensionFunc <BaseExtensionFunc>` instance.

        Parameters
        ----------
        *args
            The names of one or more arguments that are being actively managed
            by this object.

        Raises
        ------
        AttributeError
            If any of the referenced arguments are not being actively managed
            by this :class:`ExtensionFunc <BaseExtensionFunc>`.

        Examples
        --------
        .. doctest::

            >>> @pdcast.extension_func
            ... def foo(bar, baz, **kwargs):
            ...     return bar, baz

            >>> @foo.register_arg(default=1)
            ... def bar(val: int, defaults: dict) -> int:
            ...     return int(val)

            >>> foo.bar
            1
            >>> foo.remove_arg("bar")
            >>> foo.bar
            Traceback (most recent call last):
                ...
            AttributeError: 'ExtensionFunc' object has no attribute 'bar'.
        """
        # ensure each argument is being actively managed
        for name in args:
            if name not in self:
                raise AttributeError(f"'{name}' is not a managed argument")

        # commit to deletion
        for name in args:
            delattr(type(self), name)
            self._vals.pop(name, None)
            self._defaults.pop(name, None)
            self._validators.pop(name)

    def _validate_args(self, *args, **kwargs: dict) -> dict:
        """Format the input to the decorated function and ensure it is valid
        according to the validators registered to this ExtensionFunc.
        """
        # bind *args, **kwargs
        result = self._signature.bind_partial(*args, **kwargs).arguments

        # flatten remaining **kwargs
        if self._kwargs_name in result:
            result.update(result[self._kwargs_name])
            del result[self._kwargs_name]

        # apply validators 
        for k, v in result.items():
            if k in self._validators:
                result[k] = self._validators[k](v, defaults=result)

        return result

    ###############################
    ####    SPECIAL METHODS    ####
    ###############################

    def __call__(self, *args, **kwargs):
        """Execute the decorated function with the default arguments stored in
        this ExtensionFunc.

        This also validates the input to each argument using the attached
        validators.
        """
        kwargs = self._validate_args(*args, **kwargs)
        kwargs = {**self.default_values, **kwargs}
        return self._func(**kwargs)

    def __contains__(self, item: str) -> bool:
        """Check if the named argument is being managed by this ExtensionFunc.
        """
        return item in self._validators

    def __iter__(self):
        """Iterate through the arguments that are being managed by this
        ExtensionFunc.
        """
        return iter(self._validators)

    def __len__(self) -> int:
        """Return the total number of arguments that are being managed by this
        ExtensionFunc
        """
        return len(self.default_values)

    def __repr__(self) -> str:
        """Return a string representation of the decorated function with a
        reconstructed signature incorporating the default values stored in this
        ExtensionFunc.
        """
        # get vals for default arguments
        keywords = self._signature.bind_partial(**self.default_values)
        keywords.apply_defaults()
        keywords = keywords.arguments
        if self._kwargs_name in keywords:  # flatten **kwargs
            keywords.update(keywords[self._kwargs_name])
            del keywords[self._kwargs_name]

        # reconstruct signature
        signature = []
        for p in self._signature.parameters.values():
            if p.name in keywords:
                signature.append(f"{p.name} = {keywords[p.name]}")
            elif p.name == self._kwargs_name:
                pars = self._signature.parameters
                kwargs = {k: v for k, v in keywords.items() if k not in pars}
                signature.extend(f"{k} = {v}" for k, v in kwargs.items())
                signature.append(f"**{self._kwargs_name}")
            else:
                signature.append(p.name)

        return f"{self._func.__qualname__}({', '.join(signature)})"