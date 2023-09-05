// include guard prevents multiple inclusion
#ifndef BERTRAND_STRUCTS_CORE_VIEW_H
#define BERTRAND_STRUCTS_CORE_VIEW_H

#include <chrono>  // std::chrono
#include <cstddef>  // size_t
#include <memory>  // std::shared_ptr, std::weak_ptr
#include <mutex>  // std::mutex, std::lock_guard
#include <optional>  // std::optional
#include <queue>  // std::queue
#include <tuple>  // std::tuple
#include <type_traits>  // std::integral_constant, std::is_base_of_v
#include <Python.h>  // CPython API
#include "node.h"  // Hashed<T>, Mapped<T>
#include "allocate.h"  // Allocator
#include "table.h"  // HashTable

/*
algorithms/
    add.h
    append.h
    compare.h  (isdisjoint(), issubset(), lexical_lt(), lexical_gt(), etc.)
    contains.h
    count.h
    delete_slice.h
    discard.h
    extend.h
    get_slice.h
    index.h
    insert.h
    lookup.h
    move.h  (move(), move_to_index())
    pop.h
    relative.h  (distance(), get_relative(), insert_relative(), etc.)
    remove.h
    reverse.h
    rotate.h
    set_slice.h
    sort.h
    union.h  (union(), intersection(), difference(), symmetric_difference())
    update.h
*/


////////////////////////////////////
////    FORWARD DECLARATIONS    ////
////////////////////////////////////


template <typename ViewType>
class SliceProxy;


template <typename ViewType>
class ThreadLock;


//////////////////////
////    MIXINS    ////
//////////////////////


////////////////////////
////    LISTVIEW    ////
////////////////////////


/*
With the new slice proxy architecture, we can do something like:

for (auto node : list.slice(3, 8, 2)) {
    // extract every node in slice, potentially not in the same order as the slice
    // parameters.  If order is important, we can use the reverse() method to
    // resolve it.
}

// -> list.slice() should have an `in_order()` method that uses a stack to reverse
// the order of iteration if necessary.  This is less efficient than accessing the
// iterator's ``


We should probably be able to do something similar with the list itself:

for (auto node : list) {
    // iterate through every node in the list
    // -> this maybe just returns a SliceProxy that covers the entire list
}

Similar accessors would probably be pretty useful for index() and relative() as well:

auto node = list.index(3).get();  // get the node at index 3
list.index(3).insert(new_node);  // insert a new node at index 3
list.index(3).replace(new_node);  // replace the node at index 3
auto node = list.index(3).drop();  // remove the node at index 3 and return it


auto node = set.relative("c", 3).get()  // get the node 3 steps after "c"
set.relative("c", 3).insert(new_node);  // insert a new node 3 steps after "c"
set.relative("c", 3).replace(new_node);  // replace the node 3 steps after "c"
auto node = set.relative("c", 3).drop();  // remove the node 3 steps after "c" and return it


`index` could actually be an instance of a callable object, which would allow us to do
this:

size_t index = list.index.normalize(-1);  // wraparound + boundscheck
*/


/* A pure C++ linked list data structure with customizable node types and allocation
strategies. */
template <
    typename NodeType = DoubleNode,
    template <typename> class Allocator = DynamicAllocator
>
class ListView {
public:
    using View = ListView<NodeType, Allocator>;
    using Node = NodeType;
    using Lock = ThreadLock<View>;
    using Slice = SliceProxy<View>;

    Node* head;
    Node* tail;
    size_t size;
    Py_ssize_t max_size;
    PyObject* specialization;

    // A ThreadLock functor that manages an internal mutex for thread safety.
    const Lock lock;  // lock(), lock.context(), lock.diagnostics, etc.

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    /* Construct an empty ListView. */
    ListView(Py_ssize_t max_size = -1, PyObject* spec = nullptr) :
        head(nullptr), tail(nullptr), size(0), max_size(max_size),
        specialization(spec), allocator(max_size)
    {
        if (spec != nullptr) {
            Py_INCREF(spec);  // hold reference to specialization if given
        }
    }

    /* Construct a ListView from an input iterable. */
    ListView(
        PyObject* iterable,
        bool reverse = false,
        Py_ssize_t max_size = -1,
        PyObject* spec = nullptr
    ) : head(nullptr), tail(nullptr), size(0), max_size(max_size),
        specialization(spec), allocator(max_size)
    {
        // hold reference to specialization, if given
        if (spec != nullptr) {
            Py_INCREF(spec);
        }

        // C API equivalent of iter(iterable)
        PyObject* iterator = PyObject_GetIter(iterable);
        if (iterator == nullptr) {  // TypeError()
            self_destruct();
            throw std::invalid_argument("Value is not iterable");
        }

        // unpack iterator into ListView
        PyObject* item;
        while (true) {
            // C API equivalent of next(iterator)
            item = PyIter_Next(iterator);
            if (item == nullptr) { // end of iterator or error
                if (PyErr_Occurred()) {
                    Py_DECREF(iterator);
                    self_destruct();
                    throw std::invalid_argument("could not get item from iterator");
                }
                break;
            }

            // allocate a new node and link it to the list
            stage(item, reverse);
            if (PyErr_Occurred()) {
                Py_DECREF(iterator);
                Py_DECREF(item);
                self_destruct();
                throw std::invalid_argument("could not stage item");
            }

            // advance to next item
            Py_DECREF(item);
        }

        // release reference on iterator
        Py_DECREF(iterator);
    }

    /* Move constructor: transfer ownership from one ListView to another. */
    ListView(ListView&& other) :
        head(other.head), tail(other.tail), size(other.size), max_size(other.max_size),
        specialization(other.specialization), allocator(std::move(other.allocator))
    {
        // reset other ListView
        other.head = nullptr;
        other.tail = nullptr;
        other.size = 0;
        other.max_size = 0;
        other.specialization = nullptr;
    }

    /* Move assignment: transfer ownership from one ListView to another. */
    ListView& operator=(ListView&& other) {
        // check for self-assignment
        if (this == &other) {
            return *this;
        }

        // free old nodes
        self_destruct();

        // transfer ownership of nodes
        head = other.head;
        tail = other.tail;
        size = other.size;
        max_size = other.max_size;
        specialization = other.specialization;
        allocator = std::move(other.allocator);

        // reset other ListView
        other.head = nullptr;
        other.tail = nullptr;
        other.size = 0;
        other.max_size = 0;
        other.specialization = nullptr;

        return *this;
    }

    /* Copy constructors. These are disabled for the sake of efficiency, preventing us
    from unintentionally copying data.  Use the explicit copy() method instead. */
    ListView(const ListView& other) = delete;
    ListView& operator=(const ListView&) = delete;

    /* Destroy a ListView and free all its nodes. */
    ~ListView() {
        self_destruct();
    }

    ////////////////////////////////
    ////    LOW-LEVEL ACCESS    ////
    ////////////////////////////////

    // NOTE: these methods allow for the direct manipulation of the underlying linked
    // list, including the allocation/deallocation and links between individual nodes.
    // They are quite user friendly given their low-level nature, but users should
    // still be careful to avoid accidental memory leaks and segfaults.

    // TODO: node() should be able to accept other nodes as input, in which case we
    // call Node::init_copy() instead of Node::init().  This would allow us to

    // -> or perhaps Node::init() can be overloaded to accept either a node or
    // PyObject*.

    /* Construct a new node for the list. */
    template <typename... Args>
    inline Node* node(Args... args) const {
        // variadic dispatch to Node::init()
        Node* result = allocator.create(args...);
        if (specialization != nullptr && result != nullptr) {
            if (!Node::typecheck(result, specialization)) {
                recycle(result);  // clean up allocated node
                return nullptr;  // propagate TypeError()
            }
        }
        return result;
    }

    /* Release a node, returning it to the allocator. */
    inline void recycle(Node* node) const {
        allocator.recycle(node);
    }

    /* Copy a node in the list. */
    inline Node* copy(Node* node) const {
        return allocator.copy(node);
    }

    /* Link a node to its neighbors to form a linked list. */
    inline void link(Node* prev, Node* curr, Node* next) {
        // delegate to node-specific link() helper
        Node::link(prev, curr, next);

        // update list parameters
        size++;
        if (prev == nullptr) {
            head = curr;
        }
        if (next == nullptr) {
            tail = curr;
        }
    }

    /* Unlink a node from its neighbors. */
    inline void unlink(Node* prev, Node* curr, Node* next) {
        // delegate to node-specific unlink() helper
        Node::unlink(prev, curr, next);

        // update list parameters
        size--;
        if (prev == nullptr) {
            head = next;
        }
        if (next == nullptr) {
            tail = prev;
        }
    }

    //////////////////////////////
    ////    LIST INTERFACE    ////
    //////////////////////////////

    /* Make a shallow copy of the entire list. */
    std::optional<View> copy() const {
        try {
            View result(max_size, specialization);

            // copy nodes into new list
            copy_into(result);
            if (PyErr_Occurred()) {
                return std::nullopt;
            }
            return std::make_optional(std::move(result));

        } catch (std::invalid_argument&) {
            return std::nullopt;  // propagate
        }
    }

    /* Clear the list. */
    void clear() {
        Node* curr = head;  // store temporary reference to head

        // reset list parameters
        head = nullptr;
        tail = nullptr;
        size = 0;

        // recycle all nodes
        while (curr != nullptr) {
            Node* next = static_cast<Node*>(curr->next);
            recycle(curr);
            curr = next;
        }
    }

    /* Normalize a numeric index, allowing Python-style wraparound and
    bounds checking. */
    template <typename T>
    std::optional<size_t> index(T index, bool truncate) {
        bool index_lt_zero = index < 0;

        // wraparound negative indices
        if (index_lt_zero) {
            index += size;
            index_lt_zero = index < 0;
        }

        // boundscheck
        if (index_lt_zero || index >= static_cast<T>(size)) {
            if (truncate) {
                if (index_lt_zero) {
                    return 0;
                }
                return size - 1;
            }
            PyErr_SetString(PyExc_IndexError, "list index out of range");
            return std::nullopt;
        }

        // return as size_t
        return std::optional<size_t>{ static_cast<size_t>(index) };
    }

    /* Normalize a Python integer for use as an index to the list. */
    std::optional<size_t> index(PyObject* index, bool truncate) {
        // check that index is a Python integer
        if (!PyLong_Check(index)) {
            PyErr_SetString(PyExc_TypeError, "index must be a Python integer");
            return std::nullopt;
        }

        // comparisons are kept at the python level until we're ready to return
        PyObject* py_zero = PyLong_FromSize_t(0);  // new reference
        PyObject* py_size = PyLong_FromSize_t(size);  // new reference
        int index_lt_zero = PyObject_RichCompareBool(index, py_zero, Py_LT);

        // wraparound negative indices
        bool release_index = false;
        if (index_lt_zero) {
            index = PyNumber_Add(index, py_size);  // new reference
            index_lt_zero = PyObject_RichCompareBool(index, py_zero, Py_LT);
            release_index = true;  // remember to DECREF index later
        }

        // boundscheck
        if (index_lt_zero || PyObject_RichCompareBool(index, py_size, Py_GE)) {
            // clean up references
            Py_DECREF(py_zero);
            Py_DECREF(py_size);
            if (release_index) {
                Py_DECREF(index);
            }

            // apply truncation if directed
            if (truncate) {
                if (index_lt_zero) {
                    return std::optional<size_t>{ 0 };
                }
                return std::optional<size_t>{ size - 1 };
            }

            // raise IndexError
            PyErr_SetString(PyExc_IndexError, "list index out of range");
            return std::nullopt;
        }

        // value is good - convert to size_t
        size_t result = PyLong_AsSize_t(index);

        // clean up references
        Py_DECREF(py_zero);
        Py_DECREF(py_size);
        if (release_index) {
            Py_DECREF(index);
        }

        return std::optional<size_t>{ result };
    }

    /* Generate a proxy for the list that references a particular slice. */
    std::optional<Slice> slice(
        std::optional<long long> start = std::nullopt,
        std::optional<long long> stop = std::nullopt,
        std::optional<long long> step = std::nullopt
    ) {
        // normalize slice indices
        long long size = static_cast<long long>(this->size);
        long long default_start = (step.value_or(0) < 0) ? (size - 1) : (0);
        long long default_stop = (step.value_or(0) < 0) ? (-1) : (size);
        long long default_step = 1;

        // normalize step
        long long _step = step.value_or(default_step);
        if (_step == 0) {
            PyErr_SetString(PyExc_ValueError, "slice step cannot be zero");
            return std::nullopt;
        }

        // normalize start index
        long long _start = start.value_or(default_start);
        if (_start < 0) {
            _start += size;
            if (_start < 0) {
                _start = (_step < 0) ? (-1) : (0);
            }
        } else if (_start >= size) {
            _start = (_step < 0) ? (size - 1) : (size);
        }

        // normalize stop index
        long long _stop = stop.value_or(default_stop);
        if (_stop < 0) {
            _stop += size;
            if (_stop < 0) {
                _stop = (step < 0) ? -1 : 0;
            }
        } else if (_stop > size) {
            _stop = (step < 0) ? (size - 1) : (size);
        }

        // convert to closed interval and check for empty slice
        long long closed = Slice::closed_interval(_start, _stop, _step);
        if ((_step > 0 && _start > closed) || (_step < 0 && _start < closed)) {
            return std::make_optional(Slice(*this, _start, _stop, _step));
        }

        // get length of slice
        long long length = Slice::slice_length(_start, _stop, _step);

        // slice has at least one element
        return std::make_optional(Slice(*this, _start, _stop, _step, closed, length));
    }

    std::optional<Slice> slice(PyObject* py_slice) {
        // check that py_slice is a Python slice
        if (!PySlice_Check(py_slice)) {
            PyErr_SetString(PyExc_TypeError, "index must be a Python slice");
            return std::nullopt;
        }

        // unpack slice using C API
        Py_ssize_t py_start, py_stop, py_step, py_length;
        int parsed = PySlice_GetIndicesEx(
            py_slice, this->size, &py_start, &py_stop, &py_step, &py_length
        );
        if (parsed < 0) {
            return std::nullopt;  // propagate error
        }

        // convert to long long
        long long start = static_cast<long long>(py_start);
        long long stop = static_cast<long long>(py_stop);
        long long step = static_cast<long long>(py_step);
        long long length = static_cast<long long>(py_length);

        // convert to closed interval and check for empty slice
        long long closed = Slice::closed_interval(start, stop, step);
        if ((step > 0 && start > closed) || (step < 0 && start < closed)) {
            return std::make_optional(Slice(*this, start, stop, step));
        }

        // slice has at least one element
        return std::make_optional(Slice(*this, start, stop, step, closed, length));
    }

    /* Enforce strict type checking for elements of this list. */
    void specialize(PyObject* spec) {
        // handle null assignment
        if (spec == nullptr) {
            if (specialization != nullptr) {
                Py_DECREF(specialization);  // remember to release old spec
                specialization = nullptr;
            }
            return;
        }

        // early return if new spec is same as old spec
        if (specialization != nullptr) {
            int comp = PyObject_RichCompareBool(spec, specialization, Py_EQ);
            if (comp == -1) {  // comparison raised an exception
                return;  // propagate error
            } else if (comp == 1) {  // spec is identical
                return;  // do nothing
            }
        }

        // check the contents of the list
        Node* curr = head;
        for (size_t i = 0; i < size; i++) {
            if (!Node::typecheck(curr, spec)) {
                return;  // propagate TypeError()
            }
            curr = static_cast<Node*>(curr->next);
        }

        // replace old specialization
        Py_INCREF(spec);
        if (specialization != nullptr) {
            Py_DECREF(specialization);
        }
        specialization = spec;
    }

    /* Get the total memory consumed by the list (in bytes). */
    inline size_t nbytes() const {
        return allocator.nbytes() + sizeof(*this);
    }

protected:
    mutable Allocator<Node> allocator;

    /* Allocate a new node for the item and append it to the list, discarding
    it in the event of an error. */
    inline void stage(PyObject* item, bool reverse) {
        // allocate a new node
        Node* curr = node(item);
        if (curr == nullptr) {  // error during node initialization
            if constexpr (DEBUG) {
                // QoL - nothing has been allocated, so we don't actually free
                printf("    -> free: %s\n", repr(item));
            }
            return;
        }

        // link the node to the staged list
        if (reverse) {
            link(nullptr, curr, head);
        } else {
            link(tail, curr, nullptr);
        }
        if (PyErr_Occurred()) {
            recycle(curr);  // clean up allocated node
            return;
        }
    }

    /* Release the resources being managed by the ListView. */
    inline void self_destruct() {
        clear();  // clear all nodes in list
        if (specialization != nullptr) {
            Py_DECREF(specialization);
        }
    }

    /* Copy all the nodes from this list into a newly-allocated view. */
    void copy_into(View& other) const {
        Node* curr = head;
        Node* copied = nullptr;
        Node* copied_tail = nullptr;

        // copy each node in list
        while (curr != nullptr) {
            copied = copy(curr);  // copy node
            if (copied == nullptr) {  // error during copy(node)
                return;  // propagate error
            }

            // link to tail of copied list
            other.link(copied_tail, copied, nullptr);
            if (PyErr_Occurred()) {  // error during link()
                return;  // propagate error
            }

            // advance to next node
            copied_tail = copied;
            curr = static_cast<Node*>(curr->next);
        }
    }

};


///////////////////////
////    PROXIES    ////
///////////////////////


/* A proxy class that allows for operations on slices within the list. */
template <typename ViewType>
class SliceProxy {
public:
    using View = ViewType;
    using Node = typename View::Node;
    class Iterator;
    class IteratorPair;

    /* normalized inputs to slice() (half-open) */
    const long long start;
    const long long stop;
    const long long step;

    /* Get the underlying view being referenced by the proxy. */
    inline View& view() const {
        return _view;
    }

    /* Get the first index to be included by a Iterator. */
    inline size_t first() const {
        return _first;
    }

    /* Get the last index to be included by a Iterator. */
    inline size_t last() const {
        return _last;
    }

    /* Get the total number of items in the slice. */
    inline size_t length() const {
        return _length;
    }

    /* Return an iterator to the start of the slice. */
    inline Iterator begin() const {
        if (_length == 0) {
            return Iterator(_view, _length);  // empty iterator
        }
        bool backward = _first > _last;
        return Iterator(_view, origin, abs_step, backward, reversed, _length);
    }

    /* Return an iterator to the end of the slice. */
    inline Iterator end() const {
        return Iterator(_view, _length);
    }

    /* Return a coupled pair of iterators for more fine-grained control. */
    inline IteratorPair iter(std::optional<size_t> length = std::nullopt) const {
        if (length.has_value()) {
            bool backward = _first > _last;
            return IteratorPair(
                Iterator(_view, origin, abs_step, backward, reversed, length.value()),
                Iterator(_view, length.value())
            );
        }
        return IteratorPair(begin(), end());
    }

    /////////////////////////////
    ////    INNER CLASSES    ////
    /////////////////////////////

    /* An iterator that traverses through all the nodes that are contained within
    a slice. */
    class Iterator {
    public:
        // neighboring nodes at the current position
        Node* prev;
        Node* curr;
        Node* next;

        /////////////////////////////////
        ////    ITERATOR PROTOCOL    ////
        /////////////////////////////////

        // iterator tags for std::iterator_traits
        using iterator_category     = std::forward_iterator_tag;
        using difference_type       = std::ptrdiff_t;
        using value_type            = Node*;
        using pointer               = Node**;
        using reference             = Node*&;

        /* Dereference the iterator to get the node at the current position. */
        inline Node* operator*() const {
            return curr;
        }

        /* Prefix increment to advance the iterator to the next node in the slice. */
        inline Iterator& operator++() {
            ++idx;
            if (idx == length) {
                // NOTE: we don't jump on the last iteration.  This prevents
                // unnecessary iterations and stops us from accidentally walking
                // off the end of the list.
                return *this;
            }

            if constexpr (has_prev<Node>::value) {
                if (backward) {  // backward traversal
                    for (size_t i = implicit_skip; i < step; ++i) {
                        next = curr;
                        curr = prev;
                        prev = static_cast<Node*>(curr->prev);
                    }
                    return *this;
                }
            }

            // forward traversal
            for (size_t i = implicit_skip; i < step; ++i) {
                prev = curr;
                curr = next;
                next = static_cast<Node*>(curr->next);
            }
            return *this;
        }

        /* Inequality comparison to terminate the slice. */
        inline bool operator!=(const Iterator& other) const {
            return idx != other.idx;
        }

        //////////////////////////////
        ////    HELPER METHODS    ////
        //////////////////////////////

        /* Get the current index of the iterator within the slice.

        NOTE: this can be used to index into an array or similar data structure
        during iteration. */
        size_t index() const {
            return idx;
        }

        /* Remove the node at the current position. */
        Node* remove() {
            Node* removed = curr;
            view.unlink(prev, curr, next);
            ++implicit_skip;

            // update iterator
            if constexpr (has_prev<Node>::value) {
                if (backward) {  // backward traversal
                    curr = prev;
                    if (prev != nullptr) {
                        prev = static_cast<Node*>(prev->prev);
                    }
                    return removed;
                }
            }

            // forward traversal
            curr = next;
            if (next != nullptr) {
                next = static_cast<Node*>(next->next);
            }
            return removed;
        }

        /* Insert a node at the current position. */
        void insert(Node* node) {
            if constexpr (has_prev<Node>::value) {
                if (backward) {  // backward traversal
                    view.link(curr, node, next);
                    prev = curr;
                    curr = node;
                    return;
                }
            }

            // forward traversal
            view.link(prev, node, curr);
            next = curr;
            curr = node;
        }

        /* Indicates whether the direction of an Iterator matches the sign of the
        step size.

        If this is false, then the iterator will yield items in the same order as
        expected from the slice parameters.  Otherwise, it will yield items in the
        opposite order, and the user will have to account for this when getting/setting
        items within the list.  This is done to minimize the total number of iterations
        required to traverse the slice without backtracking. */
        inline bool reverse() const {
            return reversed;
        }

    private:
        View& view;
        size_t implicit_skip;
        size_t step;
        size_t idx;
        bool backward;
        bool reversed;
        size_t length;

        ////////////////////////////
        ////    CONSTRUCTORS    ////
        ////////////////////////////

        // force the use of Slice::begin()/Slice::end()/Slice::iter() factory methods
        friend class SliceProxy;

        /* Get an iterator to the start of the slice. */
        Iterator(
            View& view,
            Node* origin,
            size_t step,
            bool backward,
            bool reversed,
            size_t length
        ) :
            view(view), implicit_skip(0), step(step), idx(0), backward(backward),
            reversed(reversed), length(length)
        {
            init_from_origin(origin);
        }

        /* Get an iterator to terminate the slice. */
        Iterator(View& view, size_t length) :
            view(view), implicit_skip(0), step(0), idx(length), backward(false),
            reversed(false), length(length)
        {}

        /* Get the initial values for the iterator based on an origin node. */
        inline void init_from_origin(Node* origin) {
            if constexpr (has_prev<Node>::value) {
                if (backward) {  // backward traversal
                    next = origin;
                    if (origin == nullptr) {
                        curr = view.tail;
                    } else {
                        curr = static_cast<Node*>(origin->prev);
                    }
                    prev = static_cast<Node*>(curr->prev);
                    return;
                }
            }

            // forward traversal
            prev = origin;
            if (origin == nullptr) {
                curr = view.head;
            } else {
                curr = static_cast<Node*>(origin->next);
            }
            next = static_cast<Node*>(curr->next);
        }

    };

    /* A coupled pair of begin() and end() iterators to simplify the iterator
    interface. */
    class IteratorPair {
    public:
        // couple the begin() and end() iterators into a single object
        IteratorPair(Iterator begin, Iterator end) : first(begin), second(end) {}

        // this allows us to use the IteratorPair as a range in a for loop
        Iterator begin() const { return first; }
        Iterator end() const { return second; }

        // Pass all other methods through to the begin() iterator
        inline Node* operator*() const { return first.curr; }
        inline IteratorPair& operator++() { ++first; return *this; }
        inline bool operator!=(const Iterator& other) const { return first != other; }
        inline size_t index() const { return first.index(); }
        inline Node* remove() { return first.remove(); }
        inline void insert(Node* node) { first.insert(node); }
        inline bool reverse() const { return first.reverse(); }

    private:
        Iterator first, second;
    };

private:
    View& _view;
    size_t _first;  // normalized and chosen to minimize total iterations
    size_t _last;
    size_t _length;
    size_t abs_step;
    bool reversed;  // accounts for singly-/doubly-linked lists
    Node* origin;  // node that immediately precedes the slice (can be NULL)

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    // force the use of the View::slice() factory method
    friend View;

    // TODO: empty slice still needs to find origin node/etc in the case of a
    // set() insertion.

    /* Construct an empty SliceProxy. */
    SliceProxy(View& view, long long start, long long stop, long long step) :
        start(start), stop(stop), step(step), _view(view), _first(0), _last(0),
        _length(0), abs_step(llabs(step)), reversed(false), origin(nullptr)
    {}

    /* Construct a SliceProxy with at least one element. */
    SliceProxy(
        View& view,
        long long start,
        long long stop,
        long long step,
        long long closed,
        long long length
    ) : start(start), stop(stop), step(step), _view(view), _first(0), _last(0),
        _length(length), abs_step(llabs(step)), reversed(false), origin(nullptr)
    {
        // get direction in which to traverse slice that minimizes total iterations
        std::pair<size_t, size_t> bounds = slice_direction(closed);
        _first = bounds.first;
        _last = bounds.second;

        // determine whether the slice is reversed with respect to step size
        reversed = (step < 0) ^ (_first > _last);

        // find the origin for the slice
        origin = find_origin();
    }

    /* Swap the start and stop indices based on the singly-/doubly-linked nature
    of the list. */
    inline std::pair<size_t, size_t> slice_direction(long long stop_closed) {
        // NOTE: if the list is doubly-linked, then we start at whichever end is
        // closest to its respective slice boundary.  This minimizes the total
        // number of iterations, but means that we may not be iterating in the same
        // direction as the step size would indicate
        if constexpr (has_prev<Node>::value) {
            long long size = static_cast<long long>(_view.size);
            if (
                (step > 0 && start <= size - stop_closed) ||
                (step < 0 && size - start <= stop_closed)
            ) {
                return std::make_pair(start, stop_closed);  // consistent with step
            }
            return std::make_pair(stop_closed, start);  // opposite of step
        }

        // NOTE: if the list is singly-linked, then we always have to iterate from
        // the head of the list.  If the step size is negative, this means that we
        // will end up iterating in the opposite direction.
        if (step > 0) {
            return std::make_pair(start, stop_closed);  // consistent with step
        }
        return std::make_pair(stop_closed, start);  // opposite of step
    }

    /* Iterate to find the origin node for the slice. */
    Node* find_origin() {
        if constexpr (has_prev<Node>::value) {
            if (_first > _last) {  // backward traversal
                Node* next = nullptr;
                Node* curr = _view.tail;
                for (size_t i = _view.size - 1; i > _first; i--) {
                    next = curr;
                    curr = static_cast<Node*>(curr->prev);
                }
                return next;
            }
        }

        // forward traversal
        Node* prev = nullptr;
        Node* curr = _view.head;
        for (size_t i = 0; i < _first; i++) {
            prev = curr;
            curr = static_cast<Node*>(curr->next);
        }
        return prev;
    }

    ///////////////////////////////
    ////    UTILITY METHODS    ////
    ///////////////////////////////

    /* Get the total number of items included in a slice. */
    inline static long long slice_length(
        long long start,
        long long stop,
        long long step
    ) {
        long long numerator = (start < stop) ? (stop - start) : (start - stop);
        long long denominator = (step < 0) ? (-step) : (step);
        return (numerator / denominator);
    }

    /* Adjust the stop index in a slice to make it closed on both ends. */
    template <typename T>
    inline static T closed_interval(T start, T stop, T step) {
        T remainder = py_modulo((stop - start), step);
        if (remainder == 0) {
            return stop - step; // decrement by 1 full step
        }
        return stop - remainder;  // decrement to nearest multiple of step
    }

    /* A modulo operator (%) that matches Python's behavior with respect to
    negative numbers. */
    template <typename T>
    inline static T py_modulo(T a, T b) {
        // NOTE: Python's `%` operator is defined such that the result has
        // the same sign as the divisor (b).  This differs from C/C++, where
        // the result has the same sign as the dividend (a).
        return (a % b + b) % b;
    }

};


/* A callable functor that produces iterators to a given index of a list. */
template <typename ViewType>
class IteratorFactory {
public:
    using View = ViewType;

private:
    friend View;
    View& view;

    IteratorFactory(View& view) : view(view) {}
};


/* A callable functor that allows a list to be locked for use from a multithreaded
context. */
template <typename ViewType>
class ThreadLock {
public:
    using View = ViewType;
    using Guard = std::lock_guard<std::mutex>;
    using Clock = std::chrono::high_resolution_clock;
    using Resolution = std::chrono::nanoseconds;

    /* Return a std::lock_guard for the internal mutex using RAII semantics.  The mutex
    is automatically acquired when the guard is constructed and released when it goes
    out of scope.  Any operations in between are guaranteed to be atomic. */
    inline Guard operator()() const {
        if (track_diagnostics) {
            auto start = Clock::now();

            // acquire lock
            mtx.lock();

            auto end = Clock::now();
            lock_time += std::chrono::duration_cast<Resolution>(end - start).count();
            ++lock_count;

            // create a guard using the acquired lock
            return Guard(mtx, std::adopt_lock);
        }

        return Guard(mtx);
    }

    /* Return a heap-allocated std::lock_guard for the internal mutex.  The mutex is
    automatically acquired when the guard is constructed and released when it is
    manually deleted.  Any operations in between are guaranteed to be atomic.

    NOTE: this method is generally less safe than using the standard functor operator,
    but can be used for compatibility with Python's context manager protocol. */
    inline Guard* context() const {
        if (track_diagnostics) {
            auto start = Clock::now();

            // acquire lock
            Guard* lock = new Guard(mtx);

            auto end = Clock::now();
            lock_time += std::chrono::duration_cast<Resolution>(end - start).count();
            ++lock_count;

            return lock;
        }

        return new Guard(mtx);
    }

    /* Toggle diagnostics on or off and return its current setting. */
    inline bool diagnostics(std::optional<bool> enabled = std::nullopt) const {
        if (enabled.has_value()) {
            track_diagnostics = enabled.value();
        }
        return track_diagnostics;
    }

    /* Get the total number of times the mutex has been locked. */
    inline size_t count() const {
        return lock_count;
    }

    /* Get the total time spent waiting to acquire the lock. */
    inline size_t duration() const {
        return lock_time;  // includes a small overhead for lock acquisition
    }

    /* Get the average time spent waiting to acquire the lock. */
    inline double contention() const {
        return static_cast<double>(lock_time) / lock_count;
    }

    /* Reset the internal diagnostic counters. */
    inline void reset_diagnostics() const {
        lock_count = 0;
        lock_time = 0;
    }

private:
    friend View;
    mutable std::mutex mtx;
    mutable bool track_diagnostics = false;
    mutable size_t lock_count = 0;
    mutable size_t lock_time = 0;
};


///////////////////////
////    SETVIEW    ////
///////////////////////


template <typename NodeType, template <typename> class Allocator>
class SetView : public ListView<Hashed<NodeType>, Allocator> {
public:
    using Base = ListView<Hashed<NodeType>, Allocator>;
    using View = SetView<NodeType, Allocator>;
    using Node = Hashed<NodeType>;

    /* Construct an empty SetView. */
    SetView(Py_ssize_t max_size = -1, PyObject* spec = nullptr) :
        Base(max_size, spec), table()
    {}

    /* Construct a SetView from an input iterable. */
    SetView(
        PyObject* iterable,
        bool reverse = false,
        Py_ssize_t max_size = -1,
        PyObject* spec = nullptr
    ) : Base(max_size, spec), table()
    {
        // C API equivalent of iter(iterable)
        PyObject* iterator = PyObject_GetIter(iterable);
        if (iterator == nullptr) {  // TypeError()
            this->self_destruct();
            throw std::invalid_argument("Value is not iterable");
        }

        // unpack iterator into SetView
        PyObject* item;
        while (true) {
            // C API equivalent of next(iterator)
            item = PyIter_Next(iterator);
            if (item == nullptr) { // end of iterator or error
                if (PyErr_Occurred()) {
                    Py_DECREF(iterator);
                    this->self_destruct();
                    throw std::invalid_argument("could not get item from iterator");
                }
                break;
            }

            // allocate a new node and link it to the list
            stage(item, reverse);
            if (PyErr_Occurred()) {
                Py_DECREF(iterator);
                Py_DECREF(item);
                this->self_destruct();
                throw std::invalid_argument("could not stage item");
            }

            // advance to next item
            Py_DECREF(item);
        }

        // release reference on iterator
        Py_DECREF(iterator);
    }

    /* Move ownership from one SetView to another (move constructor). */
    SetView(SetView&& other) : Base(std::move(other)), table(std::move(other.table)) {}

    /* Move ownership from one SetView to another (move assignment). */
    SetView& operator=(SetView&& other) {
        // check for self-assignment
        if (this == &other) {
            return *this;
        }

        // call parent move assignment operator
        Base::operator=(std::move(other));

        // transfer ownership of hash table
        table = std::move(other.table);
        return *this;
    }

    /* Copy a node in the list. */
    inline Node* copy(Node* node) const {
        return Base::copy(node);
    }

    /* Make a shallow copy of the entire list. */
    std::optional<View> copy() const {
        try {
            View result(this->max_size, this->specialization);

            // copy nodes into new list
            Base::copy_into(result);
            if (PyErr_Occurred()) {
                return std::nullopt;
            }

            return std::make_optional(std::move(result));
        } catch (std::invalid_argument&) {
            return std::nullopt;
        }
    }

    /* Clear the list and reset the associated hash table. */
    inline void clear() {
        Base::clear();  // free all nodes
        table.reset();  // reset hash table
    }

    /* Link a node to its neighbors to form a linked list. */
    void link(Node* prev, Node* curr, Node* next) {
        // add node to hash table
        table.remember(curr);
        if (PyErr_Occurred()) {  // node is already present in table
            return;
        }

        // delegate to ListView
        Base::link(prev, curr, next);
    }

    /* Unlink a node from its neighbors. */
    void unlink(Node* prev, Node* curr, Node* next) {
        // remove node from hash table
        table.forget(curr);
        if (PyErr_Occurred()) {  // node is not present in table
            return;
        }

        // delegate to ListView
        Base::unlink(prev, curr, next);
    }

    /* Search for a node by its value. */
    template <typename T>
    inline Node* search(T* key) const {
        // NOTE: T can be either a PyObject or node pointer.  If a node is provided,
        // then its precomputed hash will be reused if available.  Otherwise, the value
        // will be passed through `PyObject_Hash()` before searching the table.
        return table.search(key);
    }

    /* A proxy class that allows for operations relative to a particular value
    within the set. */
    class RelativeProxy {
    public:
        using View = SetView<NodeType, Allocator>;
        using Node = View::Node;

        View* view;
        Node* sentinel;
        Py_ssize_t offset;

        // TODO: truncate could be handled at the proxy level.  It would just be
        // another constructor argument

        /* Construct a new RelativeProxy for the set. */
        RelativeProxy(View* view, Node* sentinel, Py_ssize_t offset) :
            view(view), sentinel(sentinel), offset(offset)
        {}

        // TODO: relative() could just return a RelativeProxy by value, which would
        // be deleted as soon as it falls out of scope.  This means we create a new
        // proxy every time a variant method is called, but we can reuse them in a
        // C++ context.

        /* Execute a function with the RelativeProxy as its first argument. */
        template <typename Func, typename... Args>
        auto execute(Func func, Args... args) {
            // function pointer must accept a RelativeProxy* as its first argument
            using ReturnType = decltype(func(std::declval<RelativeProxy*>(), args...));

            // call function with proxy
            if constexpr (std::is_void_v<ReturnType>) {
                func(this, args...);
            } else {
                return func(this, args...);
            }
        }


        // TODO: these could maybe just get the proxy's curr(), prev(), and next()
        // nodes, respectively.  We can then derive the other nodes from whichever one
        // is populated.  For example, if we've already found and cached the prev()
        // node, then curr() is just generated by getting prev()->next, and same with
        // next() and curr()->next.  If none of the nodes are cached, then we have to
        // iterate like we do now to find and cache them.  This means that in any
        // situation where we need to get all three nodes, we should always start with
        // prev().

        /* Return the node at the proxy's current location. */
        Node* walk(Py_ssize_t offset, bool truncate) {
            // check for no-op
            if (offset == 0) {
                return sentinel;
            }

            // TODO: introduce caching for the proxy's current position.  Probably
            // need to use std::optional<Node*> for these, since nullptr might be a
            // valid value.

            // if we're traversing forward from the sentinel, then the process is the
            // same for both singly- and doubly-linked lists
            if (offset > 0) {
                curr = sentinel;
                for (Py_ssize_t i = 0; i < offset; i++) {
                    if (curr == nullptr) {
                        if (truncate) {
                            return view->tail;  // truncate to end of list
                        } else {
                            return nullptr;  // index out of range
                        }
                    }
                    curr = static_cast<Node*>(curr->next);
                }
                return curr;
            }

            // if the list is doubly-linked, we can traverse backward just as easily
            if constexpr (has_prev<Node>::value) {
                curr = sentinel;
                for (Py_ssize_t i = 0; i > offset; i--) {
                    if (curr == nullptr) {
                        if (truncate) {
                            return view->head;  // truncate to beginning of list
                        } else {
                            return nullptr;  // index out of range
                        }
                    }
                    curr = static_cast<Node*>(curr->prev);
                }
                return curr;
            }

            // Otherwise, we have to iterate from the head of the list.  We do this
            // using a two-pointer approach where the `lookahead` pointer is offset
            // from the `curr` pointer by the specified number of steps.  When it
            // reaches the sentinel, then `curr` will be at the correct position.
            Node* lookahead = view->head;
            for (Py_ssize_t i = 0; i > offset; i--) {  // advance lookahead to offset
                if (lookahead == sentinel) {
                    if (truncate) {
                        return view->head;  // truncate to beginning of list
                    } else {
                        return nullptr;  // index out of range
                    }
                }
                lookahead = static_cast<Node*>(lookahead->next);
            }

            // advance both pointers until lookahead reaches sentinel
            curr = view->head;
            while (lookahead != sentinel) {
                curr = static_cast<Node*>(curr->next);
                lookahead = static_cast<Node*>(lookahead->next);
            }
            return curr;
        }

        /* Find the left and right bounds for an insertion. */
        std::pair<Node*, Node*> junction(Py_ssize_t offset, bool truncate) {
            // get the previous node for the insertion point
            prev = walk(offset - 1, truncate);

            // apply truncate rule
            if (prev == nullptr) {  // walked off end of list
                if (!truncate) {
                    return std::make_pair(nullptr, nullptr);  // error code
                }
                if (offset < 0) {
                    return std::make_pair(nullptr, view->head);  // beginning of list
                }
                return std::make_pair(view->tail, nullptr);  // end of list
            }

            // return the previous node and its successor
            curr = static_cast<Node*>(prev->next);
            return std::make_pair(prev, curr);
        }

        /* Find the left and right bounds for a removal. */
        std::tuple<Node*, Node*, Node*> neighbors(Py_ssize_t offset, bool truncate) {
            // NOTE: we can't reuse junction() here because we need access to the node
            // preceding the tail in the event that we walk off the end of the list and
            // truncate=true.
            curr = sentinel;

            // NOTE: this is trivial for doubly-linked lists
            if constexpr (has_prev<Node>::value) {
                if (offset > 0) {  // forward traversal
                    next = static_cast<Node*>(curr->next);
                    for (Py_ssize_t i = 0; i < offset; i++) {
                        if (next == nullptr) {
                            if (truncate) {
                                break;  // truncate to end of list
                            } else {
                                return std::make_tuple(nullptr, nullptr, nullptr);
                            }
                        }
                        curr = next;
                        next = static_cast<Node*>(curr->next);
                    }
                    prev = static_cast<Node*>(curr->prev);
                } else {  // backward traversal
                    prev = static_cast<Node*>(curr->prev);
                    for (Py_ssize_t i = 0; i > offset; i--) {
                        if (prev == nullptr) {
                            if (truncate) {
                                break;  // truncate to beginning of list
                            } else {
                                return std::make_tuple(nullptr, nullptr, nullptr);
                            }
                        }
                        curr = prev;
                        prev = static_cast<Node*>(curr->prev);
                    }
                    next = static_cast<Node*>(curr->next);
                }
                return std::make_tuple(prev, curr, next);
            }

            // NOTE: It gets significantly more complicated if the list is singly-linked.
            // In this case, we can only optimize the forward traversal branch if we're
            // advancing at least one node and the current node is not the tail of the
            // list.
            if (truncate && offset > 0 && curr == view->tail) {
                offset = 0;  // skip forward iteration branch
            }

            // forward iteration (efficient)
            if (offset > 0) {
                prev = nullptr;
                next = static_cast<Node*>(curr->next);
                for (Py_ssize_t i = 0; i < offset; i++) {
                    if (next == nullptr) {  // walked off end of list
                        if (truncate) {
                            break;
                        } else {
                            return std::make_tuple(nullptr, nullptr, nullptr);
                        }
                    }
                    if (prev == nullptr) {
                        prev = curr;
                    }
                    curr = next;
                    next = static_cast<Node*>(curr->next);
                }
                return std::make_tuple(prev, curr, next);
            }

            // backward iteration (inefficient)
            Node* lookahead = view->head;
            for (size_t i = 0; i > offset; i--) {  // advance lookahead to offset
                if (lookahead == curr) {
                    if (truncate) {  // truncate to beginning of list
                        next = static_cast<Node*>(view->head->next);
                        return std::make_tuple(nullptr, view->head, next);
                    } else {  // index out of range
                        return std::make_tuple(nullptr, nullptr, nullptr);
                    }
                }
                lookahead = static_cast<Node*>(lookahead->next);
            }

            // advance both pointers until lookahead reaches sentinel
            prev = nullptr;
            Node* temp = view->head;
            while (lookahead != curr) {
                prev = temp;
                temp = static_cast<Node*>(temp->next);
                lookahead = static_cast<Node*>(lookahead->next);
            }
            next = static_cast<Node*>(temp->next);
            return std::make_tuple(prev, temp, next);
        }

    private:
        // cache the proxy's current position in the set
        Node* prev;
        Node* curr;
        Node* next;
    };

    /* Generate a proxy for a set that allows operations relative to a particular
    sentinel value. */
    template <typename T, typename Func, typename... Args>
    auto relative(T* sentinel, Py_ssize_t offset, Func func, Args... args) {
        // function pointer must accept a RelativeProxy* as its first argument
        using ReturnType = decltype(func(std::declval<RelativeProxy*>(), args...));

        // search for sentinel
        Node* sentinel_node = search(sentinel);
        if (sentinel_node == nullptr) {  // sentinel not found
            PyErr_Format(PyExc_KeyError, "%R is not contained in the set", sentinel);
            return nullptr;  // propagate
        }

        // stack-allocate a temporary proxy for the set (memory-safe)
        RelativeProxy proxy(this, sentinel_node, offset);

        // call function with proxy
        if constexpr (std::is_void_v<ReturnType>) {
            func(&proxy, args...);
        } else {
            return func(&proxy, args...);
        }
    }

    /* Clear all tombstones from the hash table. */
    inline void clear_tombstones() {
        table.clear_tombstones();
    }

    /* Get the total amount of memory consumed by the set (in bytes).  */
    inline size_t nbytes() const {
        return Base::nbytes() + table.nbytes();
    }

protected:

    /* Allocate a new node for the item and add it to the set, discarding it in
    the event of an error. */
    inline void stage(PyObject* item, bool reverse) {
        // allocate a new node
        Node* curr = this->node(item);
        if (curr == nullptr) {  // error during node initialization
            if constexpr (DEBUG) {
                // QoL - nothing has been allocated, so we don't actually free
                printf("    -> free: %s\n", repr(item));
            }
            return;
        }

        // search for node in hash table
        Node* existing = search(curr);
        if (existing != nullptr) {  // item already exists
            if constexpr (has_mapped<Node>::value) {
                // update mapped value
                Py_DECREF(existing->mapped);
                Py_INCREF(curr->mapped);
                existing->mapped = curr->mapped;
            }
            this->recycle(curr);
            return;
        }

        // link the node to the staged list
        if (reverse) {
            link(nullptr, curr, this->head);
        } else {
            link(this->tail, curr, nullptr);
        }
        if (PyErr_Occurred()) {
            this->recycle(curr);  // clean up allocated node
            return;
        }
    }

private:
    HashTable<Node> table;  // stack allocated
};


////////////////////////
////    DICTVIEW    ////
////////////////////////


// TODO: we can add a separate specialization for an LRU cache that is always
// implemented as a DictView<DoubleNode, PreAllocator>.  This would allow us to
// use a pure C++ implementation for the LRU cache, which isn't even wrapped
// in a Cython class.  We could export this as a Cython alias for use in type
// inference.  Maybe the instance factories have one of these as a C-level
// member.



// TODO: If we inherit from SetView<Mapped<NodeType>, Allocator>, then we need
// to remove the hashing-related code from Mapped<>.


template <typename NodeType, template <typename> class Allocator>
class DictView {
public:
    using Node = Mapped<NodeType>;
    Node* head;
    Node* tail;
    size_t size;

    /* Disabled copy/move constructors.  These are dangerous because we're
    manually managing memory for each node. */
    DictView(const DictView& other) = delete;       // copy constructor
    DictView& operator=(const DictView&) = delete;  // copy assignment
    DictView(DictView&&) = delete;                  // move constructor
    DictView& operator=(DictView&&) = delete;       // move assignment

    /* Construct an empty DictView. */
    DictView(Py_ssize_t max_size = -1) :
        head(nullptr), tail(nullptr), size(0), specialization(nullptr),
        table(), allocator(max_size) {}

    /* Construct a DictView from an input iterable. */
    DictView(
        PyObject* iterable,
        bool reverse = false,
        PyObject* spec = nullptr,
        Py_ssize_t max_size = -1
    ) : head(nullptr), tail(nullptr), size(0), specialization(nullptr),
        table(), allocator(max_size)
    {
        // C API equivalent of iter(iterable)
        PyObject* iterator = PyObject_GetIter(iterable);
        if (iterator == nullptr) {
            throw std::invalid_argument("Value is not iterable");
        }

        // hold reference to specialization, if given
        if (spec != nullptr) {
            Py_INCREF(spec);
        }

        // unpack iterator into DictView
        PyObject* item;
        while (true) {
            // C API equivalent of next(iterator)
            item = PyIter_Next(iterator);
            if (item == nullptr) { // end of iterator or error
                if (PyErr_Occurred()) {
                    Py_DECREF(iterator);
                    self_destruct();
                    throw std::runtime_error("could not get item from iterator");
                }
                break;  // end of iterator
            }

            // allocate a new node and link it to the list
            stage(item, reverse);
            if (PyErr_Occurred()) {
                Py_DECREF(iterator);
                Py_DECREF(item);
                self_destruct();
                throw std::runtime_error("could not stage item");
            }

            // advance to next item
            Py_DECREF(item);
        }

        // release reference on iterator
        Py_DECREF(iterator);
    };

    /* Destroy a DictView and free all its resources. */
    ~DictView() {
        self_destruct();
    }

    /* Construct a new node for the list. */
    template <typename... Args>
    inline Node* node(PyObject* value, Args... args) const {
        // variadic dispatch to Node::init()
        Node* result = allocator.create(value, args...);
        if (specialization != nullptr && result != nullptr) {
            if (!Node::typecheck(result, specialization)) {
                recycle(result);  // clean up allocated node
                return nullptr;  // propagate TypeError()
            }
        }

        return result;
    }

    /* Release a node, returning it to the allocator. */
    inline void recycle(Node* node) const {
        allocator.recycle(node);
    }

    /* Copy a single node in the list. */
    inline Node* copy(Node* node) const {
        return allocator.copy(node);
    }

    /* Make a shallow copy of the list. */
    DictView<NodeType, Allocator>* copy() const {
        DictView<NodeType, Allocator>* copied = new DictView<NodeType, Allocator>();
        Node* old_node = head;
        Node* new_node = nullptr;
        Node* new_prev = nullptr;

        // copy each node in list
        while (old_node != nullptr) {
            new_node = copy(old_node);  // copy node
            if (new_node == nullptr) {  // error during copy()
                delete copied;  // discard staged list
                return nullptr;
            }

            // link to tail of copied list
            copied->link(new_prev, new_node, nullptr);
            if (PyErr_Occurred()) {  // error during link()
                delete copied;  // discard staged list
                return nullptr;
            }

            // advance to next node
            new_prev = new_node;
            old_node = static_cast<Node*>(old_node->next);
        }

        // return copied view
        return copied;
    }

    /* Clear the list and reset the associated hash table. */
    inline void clear() {
        purge_list();  // free all nodes
        table.reset();  // reset hash table to initial size
    }

    /* Link a node to its neighbors to form a linked list. */
    void link(Node* prev, Node* curr, Node* next) {
        // add node to hash table
        table.remember(curr);
        if (PyErr_Occurred()) {
            return;
        }

        // delegate to node-specific link() helper
        Node::link(prev, curr, next);

        // update list parameters
        size++;
        if (prev == nullptr) {
            head = curr;
        }
        if (next == nullptr) {
            tail = curr;
        }
    }

    /* Unlink a node from its neighbors. */
    void unlink(Node* prev, Node* curr, Node* next) {
        // remove node from hash table
        table.forget(curr);
        if (PyErr_Occurred()) {
            return;
        }

        // delegate to node-specific unlink() helper
        Node::unlink(prev, curr, next);

        // update list parameters
        size--;
        if (prev == nullptr) {
            head = next;
        }
        if (next == nullptr) {
            tail = prev;
        }
    }

    /* Enforce strict type checking for elements of this list. */
    void specialize(PyObject* spec) {
        // check the contents of the list
        if (spec != nullptr) {
            Node* curr = head;
            for (size_t i = 0; i < size; i++) {
                if (!Node::typecheck(curr, spec)) {
                    return;  // propagate TypeError()
                }
                curr = static_cast<Node*>(curr->next);
            }
            Py_INCREF(spec);
        }

        // replace old specialization
        if (specialization != nullptr) {
            Py_DECREF(specialization);
        }
        specialization = spec;
    }

    /* Get the type specialization for elements of this list. */
    inline PyObject* get_specialization() const {
        if (specialization != nullptr) {
            Py_INCREF(specialization);
        }
        return specialization;  // return a new reference or NULL
    }

    /* Search for a node by its value. */
    inline Node* search(PyObject* value) const {
        return table.search(value);
    }

    /* Search for a node by its value. */
    inline Node* search(Node* value) const {
        return table.search(value);
    }

    /* Search for a node and move it to the front of the list at the same time. */
    inline Node* lru_search(PyObject* value) {
        // move node to head of list
        Node* curr = table.search(value);
        if (curr != nullptr && curr != head) {
            if (curr == tail) {
                tail = static_cast<Node*>(curr->prev);
            }
            Node* prev = static_cast<Node*>(curr->prev);
            Node* next = static_cast<Node*>(curr->next);
            Node::unlink(prev, curr, next);
            Node::link(nullptr, curr, head);
            head = curr;
        }

        return curr;
    }

    /* Clear all tombstones from the hash table. */
    inline void clear_tombstones() {
        table.clear_tombstones();
    }

    /* Get the total amount of memory consumed by the dictionary (in bytes). */
    inline size_t nbytes() const {
        return allocator.nbytes() + table.nbytes() + sizeof(*this);
    }

private:
    PyObject* specialization;  // specialized type for elements of this list
    mutable Allocator<Node>allocator;  // stack allocated
    HashTable<Node> table;  // stack allocated

    /* Allocate a new node for the item and append it to the list, discarding
    it in the event of an error. */
    void stage(PyObject* item, bool reverse) {
        // allocate a new node
        Node* curr = node(item);
        if (PyErr_Occurred()) {
            // QoL - nothing has been allocated, so we don't actually free anything
            if constexpr (DEBUG) {
                printf("    -> free: %s\n", repr(item));
            }
            return;
        }

        // link the node to the staged list
        if (reverse) {
            link(nullptr, curr, head);
        } else {
            link(tail, curr, nullptr);
        }
        if (PyErr_Occurred()) {  // node already exists
            recycle(curr);
            return;
        }
    }

    /* Clear all nodes in the list. */
    void purge_list() {
        // NOTE: this does not reset the hash table, and is therefore unsafe.
        // It should only be used to destroy a DictView or clear its contents.
        Node* curr = head;  // store temporary reference to head

        // reset list parameters
        head = nullptr;
        tail = nullptr;
        size = 0;

        // recycle all nodes
        while (curr != nullptr) {
            Node* next = static_cast<Node*>(curr->next);
            recycle(curr);
            curr = next;
        }
    }

    /* Release the resources being managed by the DictView. */
    inline void self_destruct() {
        // NOTE: allocator and table are stack allocated, so they don't need to
        // be freed here.  Their destructors will be called automatically when
        // the DictView is destroyed.
        purge_list();
        if (specialization != nullptr) {
            Py_DECREF(specialization);
        }
    }

};


///////////////////////////
////    VIEW TRAITS    ////
///////////////////////////


/* A trait that detects whether the templated view is set-like (i.e. has a
search() method). */
template <typename View>
struct is_setlike {
private:
    // Helper template to detect whether View has a search() method
    template <typename T>
    static auto test(T* t) -> decltype(t->search(nullptr), std::true_type());

    // Overload for when View does not have a search() method
    template <typename T>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<View>(nullptr))::value;
};


///////////////////////////////
////    VIEW DECORATORS    ////
///////////////////////////////


// TODO: Sorted<> becomes a decorator for a view, not a node.  It automatically
// converts a view of any type into a sorted view, which stores its nodes in a
// skip list.  This makes the sortedness immutable, and blocks operations that
// would unsort the list.  Every node in the list is decorated with a key value
// that is supplied by the user.  This key is provided in the constructor, and
// is cached on the node itself under a universal `key` attribute.  The SortKey
// template parameter defines what is stored in this key, and under what
// circumstances it is modified.

// using MFUCache = typename Sorted<DictView<DoubleNode>, Frequency, Descending>;

// This would create a doubly-linked skip list where each node maintains a
// value, mapped value, frequency count, hash, and prev/next pointers.  The
// view itself would maintain a hash map for fast lookups.  If the default
// SortKey is used, then we can also make the the index() method run in log(n)
// by exploiting the skip list.  These can be specific overloads in the methods
// themselves.

// This decorator can be extended to any of the existing views.



// template <
//     template <typename> class ViewType,
//     typename NodeType,
//     typename SortKey = Value,
//     typename SortOrder = Ascending
// >
// class Sorted : public ViewType<NodeType> {
// public:
//     /* A node decorator that maintains vectors of next and prev pointers for use in
//     sorted, skip list-based data structures. */
//     struct Node : public ViewType::Node {
//         std::vector<Node*> skip_next;
//         std::vector<Node*> skip_prev;

//         /* Initialize a newly-allocated node. */
//         inline static Node* init(Node* node, PyObject* value) {
//             node = static_cast<Node*>(NodeType::init(node, value));
//             if (node == nullptr) {  // Error during decorated init()
//                 return nullptr;  // propagate
//             }

//             // NOTE: skip_next and skip_prev are stack-allocated, so there's no
//             // need to initialize them here.

//             // return initialized node
//             return node;
//         }

//         /* Initialize a copied node. */
//         inline static Node* init_copy(Node* new_node, Node* old_node) {
//             // delegate to templated init_copy() method
//             new_node = static_cast<Node*>(NodeType::init_copy(new_node, old_node));
//             if (new_node == nullptr) {  // Error during templated init_copy()
//                 return nullptr;  // propagate
//             }

//             // copy skip pointers
//             new_node->skip_next = old_node->skip_next;
//             new_node->skip_prev = old_node->skip_prev;
//             return new_node;
//         }

//         /* Tear down a node before freeing it. */
//         inline static void teardown(Node* node) {
//             node->skip_next.clear();  // reset skip pointers
//             node->skip_prev.clear();
//             NodeType::teardown(node);
//         }

//         // TODO: override link() and unlink() to update skip pointers and maintain
//         // sorted order
//     }
// }


// ////////////////////////
// ////    POLICIES    ////
// ////////////////////////


// // TODO: Value and Frequency should be decorators for nodes to give them full
// // type information.  They can even wrap


// /* A SortKey that stores a reference to a node's value in its key. */
// struct Value {
//     /* Decorate a freshly-initialized node. */
//     template <typename Node>
//     inline static void decorate(Node* node) {
//         node->key = node->value;
//     }

//     /* Clear a node's sort key. */
//     template <typename Node>
//     inline static void undecorate(Node* node) {
//         node->key = nullptr;
//     }
// };


// /* A SortKey that stores a frequency counter as a node's key. */
// struct Frequency {
//     /* Initialize a node's sort key. */
//     template <typename Node>
//     inline static void decorate(Node* node) {
//         node->key = 0;
//     }

//     /* Clear a node's sort key */

// };


// /* A Sorted<> policy that sorts nodes in ascending order based on key. */
// template <typename SortValue>
// struct Ascending {
//     /* Check whether two keys are in sorted order relative to one another. */
//     template <typename KeyValue>
//     inline static bool compare(KeyValue left, KeyValue right) {
//         return left <= right;
//     }

//     /* A specialization for compare to use with Python objects as keys. */
//     template <>
//     inline static bool compare(PyObject* left, PyObject* right) {
//         return PyObject_RichCompareBool(left, right, Py_LE);  // -1 signals error
//     }
// };


// /* A specialized version of Ascending that compares PyObject* references. */
// template <>
// struct Ascending<Value> {
    
// };


// /* A Sorted<> policy that sorts nodes in descending order based on key. */
// struct Descending {
//     /* Check whether two keys are in sorted order relative to one another. */
//     template <typename KeyValue>
//     inline static int compare(KeyValue left, KeyValue right) {
//         return left >= right;
//     }

//     /* A specialization for compare() to use with Python objects as keys. */
//     template <>
//     inline static int compare(PyObject* left, PyObject* right) {
//         return PyObject_RichCompareBool(left, right, Py_GE);  // -1 signals error
//     }
// };


#endif // BERTRAND_STRUCTS_CORE_VIEW_H include guard
