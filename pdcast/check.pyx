from typing import Any

from pdcast.detect import detect_type
from pdcast.resolve import resolve_type
from pdcast.types cimport BaseType, CompositeType
from pdcast.util.type_hints import type_specifier


# TODO: typecheck(1, "int[python]", exact=True) == False !
# -> maybe base python types should always be in their python backends.  Just
# specify this in resolve_type() docs.

# TODO: generic types should match any of their backends, even when exact=True


def typecheck(
    data: Any,
    dtype: type_specifier ,
    exact: bool = False
) -> bool:
    """Check whether example data contains elements of a specified type.

    This function is a direct analogue for ``isinstance()`` checks, but
    extended to vectorized data.  It returns ``True`` if and only if the
    example data is described by the given type specifier or one of its
    subtypes.  Just like ``isinstance()``, it can also accept multiple type
    specifiers to compare against, and will return ``True`` if **any** of them
    match the example data.

    Parameters
    ----------
    data : Any
        The example data whose type will be checked.  This can be a scalar or
        list-like iterable of any kind.
    dtype : type specifier
        The type to compare against.  This can be in any format accepted by
        :func:`resolve_type`.
    exact : bool, default False
        Specifies whether to include subtypes in comparisons (False), or only
        check for exact matches (True).

    Returns
    -------
    bool
        ``True`` if the data matches the specified type, ``False`` otherwise.

    Raises
    ------
    ValueError
        If the comparison type could not be resolved.

    See Also
    --------
    AtomicType.contains : Customizable membership checks.
    AdapterType.contains : Customizable membership checks.

    Notes
    -----
    If ``pdcast.attach`` is imported, this function is directly attached to
    ``pandas.Series`` objects, allowing users to omit the ``data`` argument.

    >>> import pandas as pd
    >>> import pdcast.attach
    >>> pd.Series([1, 2, 3]).typecheck(int)
    True
    >>> pd.Series([1, 2, 3]).typecheck("int64", exact=True)
    True

    Examples
    --------
    If ``exact=True``, then subtypes will be excluded from membership tests.

    >>> import pandas as pd
    >>> import pdcast
    >>> series = pd.Series([1, 2, 3], dtype="i2")
    >>> series
    0    1
    1    2
    2    3
    dtype: int16
    >>> pdcast.typecheck(series, "int", exact=False)
    True
    >>> pdcast.typecheck(series, "int", exact=True)
    False
    >>> pdcast.typecheck(series, "int16", exact=True)
    True
    """
    cdef BaseType data_type = detect_type(data)
    cdef CompositeType target_type = resolve_type([dtype])
    cdef set exact_target

    # enforce strict match
    if exact:
        exact_target = set(target_type)
        if isinstance(data_type, CompositeType):
            return all(t in exact_target for t in data_type)
        return data_type in exact_target

    # include subtypes
    return target_type.contains(data_type)