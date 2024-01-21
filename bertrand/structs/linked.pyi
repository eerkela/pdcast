"""Mypy stubs for pdcast/structs/linked.cpp"""
from typing import (
    Any, Callable, ClassVar, Generic, Iterable, Iterator, Mapping, TypeVar
)


K = TypeVar("K", covariant=True)
V = TypeVar("V", covariant=True)


class Lock:

    class Guard:
        
        @property
        def active(self) -> bool: ...
        @property
        def is_shared(self) -> bool: ...
        def __enter__(self) -> Lock.Guard: ...
        def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None: ...

    # basic locks
    def __call__(self) -> Lock.Guard: ...  # exclusive
    def shared(self) -> Lock.Guard: ...  # shared

    # diagnostic locks
    def count(self) -> int: ...
    def duration(self) -> float: ...
    def contention(self) -> float: ...
    def reset_diagnostics(self) -> None: ...


class MemGuard:

    @property
    def active(self) -> bool: ...
    def __enter__(self) -> MemGuard: ...
    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None: ...


##########################
####    LINKEDLIST    ####
##########################


class LinkedList(Generic[V]):

    def __init__(
        self,
        values: Iterable[V] | None = None,
        max_size: int | None = None,
        spec: Any = None,
        reverse: bool = False,
        singly_linked: bool = False
    ) -> None:
        ...

    ####################
    ####    BASE    ####
    ####################

    __hash__: ClassVar[None] = ...  # type: ignore

    @property
    def SINGLY_LINKED(self) -> bool: ...
    @property
    def DOUBLY_LINKED(self) -> bool: ...
    @property
    def FIXED_SIZE(self) -> bool: ...
    @property
    def DYNAMIC(self) -> bool: ...
    @property
    def STRICTLY_TYPED(self) -> bool: ...
    @property
    def LOOSELY_TYPED(self) -> bool: ...
    @property
    def lock(self) -> Lock: ...
    @property
    def capacity(self) -> int: ...
    @property
    def max_size(self) -> int: ...
    @property
    def frozen(self) -> bool: ...
    @property
    def specialization(self) -> Any: ...
    @property
    def nbytes(self) -> int: ...

    def reserve(self, capacity: int) -> MemGuard: ...
    def defragment(self) -> None: ...
    def specialize(self, spec: Any) -> None: ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[V]: ...
    def __reversed__(self) -> Iterator[V]: ...

    ####################
    ####    LIST    ####
    ####################

    def append(self, value: V) -> None: ...
    def append_left(self, value: V) -> None: ...
    def insert(self, index: int, value: V) -> None: ...
    def extend(self, values: Iterable[V]) -> None: ...
    def extend_left(self, values: Iterable[V]) -> None: ...
    def pop(self, index: int = -1) -> V: ...
    def remove(self, value: V) -> None: ...
    def clear(self) -> None: ...
    def copy(self) -> LinkedList[V]: ...
    def sort(
        self,
        *,
        key: Callable[[V], Any] | None = None,
        reverse: bool = False
    ) -> None:
        ...
    def reverse(self) -> None: ...
    def rotate(self, shift: int = 1) -> None: ...
    def count(self, value: V) -> int: ...
    def index(self, value: V, start: int = 0, stop: int = -1) -> int: ...
    def __contains__(self, item: V) -> bool: ...
    def __getitem__(self, index: int | slice) -> V | LinkedList[V]: ...
    def __setitem__(self, index: int | slice, value: V | Iterable[V]) -> None: ...
    def __delitem__(self, index: int | slice) -> None: ...
    def __add__(self, other: Iterable[V]) -> LinkedList[V]: ...
    def __iadd__(self, other: Iterable[V]) -> LinkedList[V]: ...
    def __mul__(self, repeat: int) -> LinkedList[V]: ...
    def __imul__(self, repeat: int) -> LinkedList[V]: ...
    def __lt__(self, other: Iterable[V]) -> bool: ...
    def __le__(self, other: Iterable[V]) -> bool: ...
    def __eq__(self, other: Any) -> bool: ...
    def __ne__(self, other: Any) -> bool: ...
    def __gt__(self, other: Iterable[V]) -> bool: ...
    def __ge__(self, other: Iterable[V]) -> bool: ...
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...


#########################
####    LINKEDSET    ####
#########################


class LinkedSet(Generic[K]):

    def __init__(
        self,
        values: Iterable[K] | None = None,
        max_size: int | None = None,
        spec: Any = None,
        reverse: bool = False,
        singly_linked: bool = False
    ) -> None:
        ...

    ####################
    ####    BASE    ####
    ####################

    __hash__: ClassVar[None] = ...  # type: ignore

    @property
    def SINGLY_LINKED(self) -> bool: ...
    @property
    def DOUBLY_LINKED(self) -> bool: ...
    @property
    def FIXED_SIZE(self) -> bool: ...
    @property
    def DYNAMIC(self) -> bool: ...
    @property
    def STRICTLY_TYPED(self) -> bool: ...
    @property
    def LOOSELY_TYPED(self) -> bool: ...
    @property
    def lock(self) -> Lock: ...
    @property
    def capacity(self) -> int: ...
    @property
    def max_size(self) -> int: ...
    @property
    def frozen(self) -> bool: ...
    @property
    def specialization(self) -> Any: ...
    @property
    def nbytes(self) -> int: ...

    def reserve(self, capacity: int) -> MemGuard: ...
    def defragment(self) -> None: ...
    def specialize(self, spec: Any) -> None: ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[K]: ...
    def __reversed__(self) -> Iterator[K]: ...

    ####################
    ####    LIST    ####
    ####################

    def insert(self, index: int, value: K) -> None: ...
    def pop(self, index: int = -1) -> K: ...
    def clear(self) -> None: ...
    def copy(self) -> LinkedSet[K]: ...
    def sort(
        self,
        *,
        key: Callable[[K], Any] | None = None,
        reverse: bool = False
    ) -> None:
        ...
    def reverse(self) -> None: ...
    def rotate(self, shift: int = 1) -> None: ...
    def count(self, value: K) -> int: ...
    def index(self, value: K, start: int = 0, stop: int = -1) -> int: ...
    def __getitem__(self, index: int | slice) -> K | LinkedSet[K]: ...
    def __setitem__(self, index: int | slice, value: K | Iterable[K]) -> None: ...
    def __delitem__(self, index: int | slice) -> None: ...

    ###################
    ####    SET    ####
    ###################

    def add(self, value: K) -> None: ...
    def add_left(self, value: K) -> None: ...
    def lru_add(self, value: K) -> None: ...
    def remove(self, value: K) -> None: ...
    def discard(self, value: K) -> None: ...
    def union(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def union_left(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def intersection(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def difference(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def symmetric_difference(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def update(self, other: Iterable[K]) -> None: ...
    def update_left(self, other: Iterable[K]) -> None: ...
    def lru_update(self, other: Iterable[K]) -> None: ...
    def intersection_update(self, other: Iterable[K]) -> None: ...
    def difference_update(self, other: Iterable[K]) -> None: ...
    def symmetric_difference_update(self, other: Iterable[K]) -> None: ...
    def symmetric_difference_update_left(self, other: Iterable[K]) -> None: ...
    def isdisjoint(self, other: Iterable[K]) -> bool: ...
    def issubset(self, other: Iterable[K]) -> bool: ...
    def issuperset(self, other: Iterable[K]) -> bool: ...
    def distance(self, value1: K, value2: K) -> int: ...
    def swap(self, value1: K, value2: K) -> None: ...
    def move(self, value: K, shift: int) -> None: ...
    def move_to_index(self, value: K, index: int) -> None: ...
    def lru_contains(self, value: K) -> bool: ...
    def __contains__(self, item: K) -> bool: ...
    def __or__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __and__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __sub__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __xor__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __ior__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __iand__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __isub__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __ixor__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __lt__(self, other: Iterable[K]) -> bool: ...
    def __le__(self, other: Iterable[K]) -> bool: ...
    def __eq__(self, other: Any) -> bool: ...
    def __ne__(self, other: Any) -> bool: ...
    def __gt__(self, other: Iterable[K]) -> bool: ...
    def __ge__(self, other: Iterable[K]) -> bool: ...
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...


##########################
####    LINKEDDICT    ####
##########################


class KeysProxy(Generic[K, V]):

    @property
    def mapping(self) -> Mapping[K, V]: ...

    def index(self, key: K) -> int: ...
    def count(self, key: K) -> int: ...
    def isdisjoint(self, other: Iterable[K]) -> bool: ...
    def issubset(self, other: Iterable[K]) -> bool: ...
    def issuperset(self, other: Iterable[K]) -> bool: ...
    def union(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def union_left(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def intersection(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def difference(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def symmetric_difference(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def symmetric_difference_left(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __contains__(self, key: K) -> bool: ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[K]: ...
    def __reversed__(self) -> Iterator[K]: ...
    def __getitem__(self, index: int | slice) -> K | KeysProxy[K, V]: ...
    def __or__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __and__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __sub__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __xor__(self, other: Iterable[K]) -> LinkedSet[K]: ...
    def __lt__(self, other: Iterable[K]) -> bool: ...
    def __le__(self, other: Iterable[K]) -> bool: ...
    def __eq__(self, other: Any) -> bool: ...
    def __ne__(self, other: Any) -> bool: ...
    def __gt__(self, other: Iterable[K]) -> bool: ...
    def __ge__(self, other: Iterable[K]) -> bool: ...
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...


class ValuesProxy(Generic[K, V]):

    @property
    def mapping(self) -> Mapping[K, V]: ...

    def index(self, value: V) -> int: ...
    def count(self, value: V) -> int: ...
    def __contains__(self, value: V) -> bool: ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[V]: ...
    def __reversed__(self) -> Iterator[V]: ...
    def __getitem__(self, index: int | slice) -> V | ValuesProxy[K, V]: ...
    def __add__(self, other: Iterable[Any]) -> LinkedList[V]: ...
    def __mul__(self, repeat: int) -> LinkedList[V]: ...
    def __lt__(self, other: Iterable[V]) -> bool: ...
    def __le__(self, other: Iterable[V]) -> bool: ...
    def __eq__(self, other: Any) -> bool: ...
    def __ne__(self, other: Any) -> bool: ...
    def __gt__(self, other: Iterable[V]) -> bool: ...
    def __ge__(self, other: Iterable[V]) -> bool: ...
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...


class ItemsProxy(Generic[K, V]):

    @property
    def mapping(self) -> Mapping[K, V]: ...

    def index(self, item: tuple[K, V]) -> int: ...
    def count(self, item: tuple[K, V]) -> int: ...
    def __contains__(self, item: tuple[K, V]) -> bool: ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[tuple[K, V]]: ...
    def __reversed__(self) -> Iterator[tuple[K, V]]: ...
    def __getitem__(self, index: int | slice) -> tuple[K, V] | ItemsProxy[K, V]: ...
    def __add__(self, other: Iterable[Any]) -> LinkedList[tuple[K, V]]: ...
    def __mul__(self, repeat: int) -> LinkedList[tuple[K, V]]: ...
    def __lt__(self, other: Iterable[tuple[K, V]]) -> bool: ...
    def __le__(self, other: Iterable[tuple[K, V]]) -> bool: ...
    def __eq__(self, other: Any) -> bool: ...
    def __ne__(self, other: Any) -> bool: ...
    def __gt__(self, other: Iterable[tuple[K, V]]) -> bool: ...
    def __ge__(self, other: Iterable[tuple[K, V]]) -> bool: ...
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...


class LinkedDict(Generic[K, V]):

    def __init__(
        self,
        items: dict[K, V] | LinkedDict[K, V] | Iterable[tuple[K, V]] | None = None,
        max_size: int | None = None,
        spec: Any = None,
        reverse: bool = False,
        singly_linked: bool = False
    ) -> None:
        ...

    @classmethod
    def fromkeys(
        cls,
        keys: Iterable[K], value: V | None = None,
        max_size: int | None = None,
        spec: Any = None,
        singly_linked: bool = False
    ) -> LinkedDict[K, V]:
        ...

    ####################
    ####    BASE    ####
    ####################

    __hash__: ClassVar[None] = ...  # type: ignore

    @property
    def SINGLY_LINKED(self) -> bool: ...
    @property
    def DOUBLY_LINKED(self) -> bool: ...
    @property
    def FIXED_SIZE(self) -> bool: ...
    @property
    def DYNAMIC(self) -> bool: ...
    @property
    def STRICTLY_TYPED(self) -> bool: ...
    @property
    def LOOSELY_TYPED(self) -> bool: ...
    @property
    def lock(self) -> Lock: ...
    @property
    def capacity(self) -> int: ...
    @property
    def max_size(self) -> int: ...
    @property
    def frozen(self) -> bool: ...
    @property
    def specialization(self) -> Any: ...
    @property
    def nbytes(self) -> int: ...

    def reserve(self, capacity: int) -> MemGuard: ...
    def defragment(self) -> None: ...
    def specialize(self, spec: Any) -> None: ...
    def __len__(self) -> int: ...
    def __iter__(self) -> Iterator[K]: ...
    def __reversed__(self) -> Iterator[K]: ...

    ####################
    ####    LIST    ####
    ####################

    def clear(self) -> None: ...
    def copy(self) -> LinkedDict[K, V]: ...
    def sort(
        self,
        *,
        key: Callable[[K, V], Any] | None = None,
        reverse: bool = False
    ) -> None:
        ...
    def reverse(self) -> None: ...
    def rotate(self, shift: int = 1) -> None: ...
    def count(self, key: K) -> int: ...
    def index(self, key: K, start: int = 0, stop: int = -1) -> int: ...

    ###################
    ####    SET    ####
    ###################

    def remove(self, key: K) -> None: ...
    def discard(self, key: K) -> None: ...
    def union(
        self, other: dict[K, V] | LinkedDict[K, V] | Iterable[tuple[K, V]]
    ) -> LinkedDict[K, V]:
        ...
    def union_left(
        self, other: dict[K, V] | LinkedDict[K, V] | Iterable[tuple[K, V]]
    ) -> LinkedDict[K, V]:
        ...
    def intersection(self, other: Iterable[K]) -> LinkedDict[K, V]: ...
    def difference(self, other: Iterable[K]) -> LinkedDict[K, V]: ...
    def symmetric_difference(
        self, other: dict[K, V] | LinkedDict[K, V] | Iterable[tuple[K, V]]
    ) -> LinkedDict[K, V]:
        ...
    def symmetric_difference_left(
        self, other: dict[K, V] | LinkedDict[K, V] | Iterable[tuple[K, V]]
    ) -> LinkedDict[K, V]:
        ...
    def update(
        self, other: dict[K, V] | LinkedDict[K, V] | Iterable[tuple[K, V]]
    ) -> None:
        ...
    def update_left(
        self, other: dict[K, V] | LinkedDict[K, V] | Iterable[tuple[K, V]]
    ) -> None:
        ...
    def lru_update(
        self, other: dict[K, V] | LinkedDict[K, V] | Iterable[tuple[K, V]]
    ) -> None:
        ...
    def intersection_update(self, other: Iterable[K]) -> None: ...
    def difference_update(self, other: Iterable[K]) -> None: ...
    def symmetric_difference_update(
        self, other: dict[K, V] | LinkedDict[K, V] | Iterable[tuple[K, V]]
    ) -> None:
        ...
    def symmetric_difference_update_left(
        self, other: dict[K, V] | LinkedDict[K, V] | Iterable[tuple[K, V]]
    ) -> None:
        ...

    ####################
    ####    DICT    ####
    ####################

    def add(self, key: K, value: V) -> None: ...
    def add_left(self, key: K, value: V) -> None: ...
    def lru_add(self, key: K, value: V) -> None: ...
    def insert(self, index: int, key: K, value: V) -> None: ...
    def pop(self, key: K, default: V | None = None) -> V: ...
    def popitem(self, index: int = -1) -> tuple[K, V]: ...
    def get(self, key: K, default: V | None = None) -> V: ...
    def lru_get(self, key: K, default: V | None = None) -> V: ...
    def setdefault(self, key: K, default: V | None = None) -> V: ...
    def setdefault_left(self, key: K, default: V | None = None) -> V: ...
    def lru_setdefault(self, key: K, default: V | None = None) -> V: ...
    def keys(self) -> KeysProxy[K, V]: ...
    def values(self) -> ValuesProxy[K, V]: ...
    def items(self) -> ItemsProxy[K, V]: ...
    def __getitem__(self, key: K) -> V: ...
    def __setitem__(self, key: K, value: V) -> None: ...
    def __delitem__(self, key: K) -> None: ...
    def __eq__(self, other: Any) -> bool: ...
    def __ne__(self, other: Any) -> bool: ...
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...
