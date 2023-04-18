"""This module describes an ``@extension_func`` decorator that transforms an
ordinary Python function into one that can accept dynamic, managed arguments
with custom validators and default values.
"""
from __future__ import annotations
from functools import partial, update_wrapper, wraps
import inspect
import threading
from types import MappingProxyType
from typing import Any, Callable

from .base import BaseDecorator


######################
####    PUBLIC    ####
######################


def extension_func(func: Callable) -> Callable:
    """A decorator that transforms a Python function into a thread-local
    :class:`ExtensionFunc <pdcast.ExtensionFunc>` object.

    Parameters
    ----------
    func : Callable
        A function to decorate.  This must accept a ``**kwargs`` dict or
        similar variable-length keyword argument.

    Returns
    -------
    Callable
        A callable :class:`ExtensionFunc <pdcast.ExtensionFunc>` object, which
        manages default values and argument validators for the decorated
        function.

    Raises
    ------
    TypeError
        If the decorated function does not accept variable-length keyword
        arguments.
    """
    main_thread = []  # using a list bypasses UnboundLocalError

    class _ExtensionFunc(ExtensionFunc):
        """A subclass of :class:`ExtensionFunc <pdcast.ExtensionFunc>` that
        supports dynamic assignment of ``@properties`` without affecting other
        instances.
        """

        def __init__(self, _func: Callable):
            super().__init__(_func)
            # update_wrapper(self, _func)

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

    # print(func)
    return _ExtensionFunc(func)


#######################
####    PRIVATE    ####
#######################


class NoDefault:
    """Signals that an argument does not have an associated default value."""

    def __repr__(self) -> str:
        return "<no default>"


no_default = NoDefault()


class ExtensionFunc(BaseDecorator, threading.local):
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
    Whenever an argument is :meth:`registered <ExtensionFunc.register_arg>` to
    this object, it is added as a managed :class:`property <python:property>`.
    This automatically generates an appropriate getter, setter, and deleter for
    the attribute based on the decorated validation function.

    Additionally, this object inherits from :class:`threading.local`.  If the
    decorated function is referenced in a child thread, a new instance will be
    dynamically created with arguments and defaults from the main thread.  This
    instance can then be modified without affecting the behavior of any other
    threads.

    Examples
    --------
    See the docs for :func:`@extension_func <extension_func>` for example
    usage.
    """

    _reserved = BaseDecorator._reserved | {"_signature", "_vals", "_defaults", "_validators"}

    def __init__(self, func: Callable):
        super().__init__(func=func)

        # assert function accepts **kwargs
        self._signature = inspect.signature(func)
        self._kwargs_name = None
        for par in self._signature.parameters.values():
            if par.kind == par.VAR_KEYWORD:
                self._kwargs_name = par.name
                break
        if self._kwargs_name is None:
            raise TypeError("func must accept **kwargs")

        self._vals = {}
        self._defaults = {}
        self._validators = {}

    ####################
    ####    BASE    ####
    ####################

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

            >>> @extension_func
            ... def foo(bar, baz, **kwargs):
            ...     return bar, baz

            >>> @foo.register_arg
            ... def bar(val: int, defaults: dict) -> int:
            ...     return int(val)

            >>> foo.validators   # doctest: +SKIP
            mappingproxy({'bar': <function bar at 0x7ff5ad9c6e60>})
        """
        return MappingProxyType(self._validators)

    @property
    def default_values(self) -> MappingProxyType:
        """A mapping of all argument names to their associated default values
        for this :class:`ExtensionFunc <pdcast.ExtensionFunc>`.

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

    def register_arg(
        self,
        _func: Callable = None,
        *,
        default: Any = no_default
    ) -> Callable:
        """A decorator that transforms a naked validation function into a
        managed argument for :class:`ExtensionFunc <pdcast.ExtensionFunc>`.

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
            :class:`ExtensionFunc <pdcast.ExtensionFunc>` is executed.

        Raises
        ------
        TypeError
            If the decorator is applied to a non-function argument.
        KeyError
            If a managed property of the same name already exists.

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
            in their validators.  This can be mitigated by using
            :meth:`dict.get() <python:dict.get>` with a default value rather
            than relying on direct access, as well as manually applying the
            same coercions as in the referenced argument's validation function.

        Examples
        --------
        See the docs for :func:`extension_func` for
        :ref:`example <extension_func.validator>` usage.
        """

        def argument(validator: Callable) -> Callable:
            """Attach a validation function to the ExtensionFunc as a managed
            property.
            """
            # ensure validator is callable
            if not callable(validator):
                raise TypeError(
                    f"decorated function must be callable: {validator}"
                )

            # use name of validation function as argument name
            name = validator.__name__
            if name in self._validators:
                raise KeyError(f"default argument '{name}' already exists.")

            # wrap validator to accept default arguments
            @wraps(validator)
            def accept_default(val, defaults=self.default_values):
                return validator(val, defaults)

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

            # make decorated validator available from ExtensionFunc and return
            self._validators[name] = accept_default
            return accept_default

        if _func is None:
            return argument
        return argument(_func)

    def remove_arg(self, *args: str) -> None:
        """Remove a registered argument from an
        :class:`ExtensionFunc <pdcast.ExtensionFunc>` instance.

        Parameters
        ----------
        *args
            The names of one or more arguments that are being actively managed
            by this object.

        Raises
        ------
        AttributeError
            If any of the referenced arguments are not being actively managed
            by this :class:`ExtensionFunc <pdcast.ExtensionFunc>`.

        Examples
        --------
        .. doctest::

            >>> @extension_func
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

    def reset_defaults(self, *args: str) -> None:
        """Reset one or more arguments to their default values.

        Parameters
        ----------
        *args
            The names of one or more arguments that are being actively managed
            by this object.

        Raises
        ------
        AttributeError
            If any of the referenced arguments are not being actively managed
            by this :class:`ExtensionFunc <pdcast.ExtensionFunc>`.

        Examples
        --------
        .. doctest::

            >>> @extension_func
            ... def foo(bar, baz, **kwargs):
            ...     return bar, baz

            >>> @foo.register_arg(default=1)
            ... def bar(val: int, defaults: dict) -> int:
            ...     return int(val)

            >>> @foo.register_arg(default=2)
            ... def baz(val: int, defaults: dict) -> int:
            ...     return int(val)

            >>> foo.bar, foo.baz
            (1, 2)
            >>> foo.bar, foo.baz = 18, -34
            >>> foo.bar, foo.baz
            (12, -34)
            >>> foo.reset_defaults("bar", "baz")
            >>> foo.bar, foo.baz
            (1, 2)

        If no arguments are supplied to this method, then all arguments will
        be reset to their internal defaults.

        .. doctest::

            >>> foo.bar, foo.baz = -86, 17
            >>> foo.bar, foo.baz
            (-86, 17)
            >>> foo.reset_defaults()
            >>> foo.bar, foo.baz
            (1, 2)
        """
        # reset all
        if not args:
            for name in self._validators:
                delattr(self, name)

        # reset some
        else:
            # ensure each argument is being actively managed
            for name in args:
                if name not in self:
                    raise AttributeError(f"'{name}' is not a managed argument")

            # commit
            for name in args:
                delattr(self, name)

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
        for name, value in result.items():
            if name in self._validators:
                print(f"validating {name}")
                result[name] = self._validators[name](value, defaults=result)

        return result

    def _reconstruct_signature(self) -> list[str]:
        """Reconstruct the original function's signature in string form,
        incorporating the default values from this ExtensionFunc.
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
        for par in self._signature.parameters.values():
            if par.name in keywords:
                signature.append(f"{par.name} = {keywords[par.name]}")
            elif par.name == self._kwargs_name:  # flatten **kwargs
                pars = self._signature.parameters
                kwargs = {k: v for k, v in keywords.items() if k not in pars}
                signature.extend(f"{k} = {v}" for k, v in kwargs.items())
                signature.append(f"**{self._kwargs_name}")
            else:
                signature.append(par.name)  # no default

        return signature

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

        # recover *args
        args = [
            kwargs.pop(a) for a in [x for x in self._signature.parameters][:len(args)]
        ]

        return self.__wrapped__(*args, **kwargs)

    # def __call__(self, *args, **kwargs):
    #     """Execute the decorated function with the default arguments stored in
    #     this ExtensionFunc.

    #     This also validates the input to each argument using the attached
    #     validators.
    #     """
    #     # bind arguments with bind_partial()
    #     bound_args = self._signature.bind_partial(*args, **kwargs)
    #     bound_args.apply_defaults()
    #     final_args = bound_args.args
    #     final_kwargs = bound_args.kwargs

    #     # flatten remaining **kwargs
    #     if self._kwargs_name in final_kwargs:
    #         final_kwargs.update(final_kwargs[self._kwargs_name])
    #         del final_kwargs[self._kwargs_name]

    #     # apply validators
    #     for name, value in final_kwargs.items():
    #         if name in self._validators:
    #             final_kwargs[name] = self._validators[name](value, defaults=final_kwargs)

    #     # call the decorated function with both positional and keyword arguments
    #     return self.__wrapped__(*final_args, **{**self.default_values, **final_kwargs})

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
        return len(self._validators)

    def __repr__(self) -> str:
        """Return a string representation of the decorated function with a
        reconstructed signature incorporating the default values stored in this
        ExtensionFunc.
        """
        sig = self._reconstruct_signature()
        return f"{self.__wrapped__.__qualname__}({', '.join(sig)})"






# @extension_func
# def foo(bar, baz=2, **kwargs):
#     print("Hello, World!")
#     return bar, baz


# @foo.register_arg(default=1)
# def bar(val: int, defaults: dict) -> int:
#     return int(val)


# @foo.register_arg
# def baz(val: int, defaults: dict) -> int:
#     return int(val)


# class MyClass:

#     def __int__(self):
#         return 4

#     def __repr__(self):
#         return "MyClass()"

#     def foo(self, baz = 2, **kwargs):
#         print("Goodbye, World!")
#         return self, baz


# foo.attach_to(MyClass)
# foo.attach_to(MyClass, namespace="test")
