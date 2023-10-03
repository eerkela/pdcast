// include guard prevents multiple inclusion
#ifndef BERTRAND_STRUCTS_LIST_LIST_H
#define BERTRAND_STRUCTS_LIST_LIST_H

// TODO: additional includes as necessary

#include <sstream>  // std::ostringstream
#include <stack>  // std::stack

#include "core/util.h"
#include "core/view.h"
#include "core/sort.h"

#include "base.h"  // LinkedBase


// TODO: ListInterface can be further broken into mixins

// Indexable  // problem: used internally within pop(), insert(), etc.
// Sliceable

// This would help with composability and code reuse.


// TODO: might just reimplement pop(), insert() in ListInterface.  This would decouple
// it from indexing and allow us to customize the two independently.


////////////////////////////////////
////    FORWARD DECLARATIONS    ////
////////////////////////////////////


template <typename Derived, typename ViewType, typename SortPolicy>
class ListInterface;


template <typename Derived>
class Concatenateable;


template <typename Derived>
class Repeatable;


template <typename Derived>
class Lexicographic;


//////////////////////
////    PUBLIC    ////
//////////////////////


/* Name of the equivalent Python class, to form dotted names for Python iterators. */
inline constexpr std::string_view linked_list_name { "LinkedList" };


/* A modular linked list class that mimics the Python list interface in C++. */
template <
    typename NodeType = DoubleNode<PyObject*>,
    typename SortPolicy = MergeSort,
    typename LockPolicy = BasicLock
>
class LinkedList :
    public LinkedBase<ListView<NodeType>, LockPolicy, linked_list_name>,
    public ListInterface<
        LinkedList<NodeType, SortPolicy, LockPolicy>,
        ListView<NodeType>,
        SortPolicy
    >,
    public Concatenateable<LinkedList<NodeType, SortPolicy, LockPolicy>>,
    public Repeatable<LinkedList<NodeType, SortPolicy, LockPolicy>>,
    public Lexicographic<LinkedList<NodeType, SortPolicy, LockPolicy>>
{
public:
    using View = ListView<NodeType>;
    using Node = typename View::Node;
    using Value = typename Node::Value;
    using Base = LinkedBase<View, LockPolicy, linked_list_name>;
    static constexpr std::string_view name { linked_list_name };

    // TODO: type aliases for Iterator, doubly_linked, etc.

    ////////////////////////////
    ////    CONSTRUCTORS    ////
    ////////////////////////////

    /* Construct an empty list. */
    LinkedList(
        std::optional<size_t> max_size = std::nullopt,
        PyObject* spec = nullptr
    ) : Base(max_size, spec)
    {}

    /* Construct a list from an input iterable. */
    LinkedList(
        PyObject* iterable,
        bool reverse = false,
        std::optional<size_t> max_size = std::nullopt,
        PyObject* spec = nullptr
    ) : Base(iterable, reverse, max_size, spec)
    {}

    /* Construct a list from a base view. */
    LinkedList(View&& view) : Base(std::move(view)) {}

    /* Copy constructor. */
    LinkedList(const LinkedList& other) : Base(other.view) {}

    /* Move constructor. */
    LinkedList(LinkedList&& other) : Base(std::move(other.view)) {}

    /* Copy assignment operator. */
    LinkedList& operator=(const LinkedList& other) {
        Base::operator=(other);
        return *this;
    }

    /* Move assignment operator. */
    LinkedList& operator=(LinkedList&& other) {
        Base::operator=(std::move(other));
        return *this;
    }

};


///////////////////////
////    METHODS    ////
///////////////////////


// NOTE: ListInterface is implemented as a mixin to allow code reuse with other Linked
// data structures.


/* A mixin that implements the full Python list interface. */
template <typename Derived, typename ViewType, typename SortPolicy>
class ListInterface {
    using View = ViewType;
    using Node = typename View::Node;
    using Value = typename Node::Value;

    template <Direction dir, typename = void>
    using ViewIter = typename View::template Iterator<dir>;

    class SliceIndices;

public:

    // forward declarations
    class ElementProxy;
    class SliceProxy;

    /* Append an item to the end of a list. */
    inline void append(PyObject* item, bool left = false) {
        View& view = self().view;
        Node* node = view.node(item);  // allocate a new node
        if (left) {
            view.link(nullptr, node, view.head());
        } else {
            view.link(view.tail(), node, nullptr);
        }
    }

    /* Insert an item into a list at the specified index. */
    template <typename T>
    inline void insert(T index, PyObject* item) {
        (*this)[index].insert(item);
    }

    /* Extend a list by appending elements from the iterable. */
    void extend(PyObject* items, bool left = false) {
        View& view = self().view;

        // note original head/tail in case of error
        Node* original;
        if (left) {
            original = view.head();
        } else {
            original = view.tail();
        }

        // proceed with extend
        try {
            PyIterable sequence(items);
            for (PyObject* item : sequence) {
                append(item, left);
            }

        // if an error occurs, clean up any nodes that were added to the list
        } catch (...) {
            if (left) {
                // if we appended to the left, then just remove until we reach the
                // original head
                Node* curr = view.head();
                while (curr != original) {
                    Node* next = curr->next();
                    view.unlink(nullptr, curr, next);
                    view.recycle(curr);
                    curr = next;
                }
            } else {
                // otherwise, start from the original tail and remove until we reach
                // the end of the list
                Node* curr = original->next();
                while (curr != nullptr) {
                    Node* next = curr->next();
                    view.unlink(original, curr, next);
                    view.recycle(curr);
                    curr = next;
                }
            }
            throw;  // propagate error
        }
    }

    /* Get the index of an item within a list. */
    template <typename T>
    size_t index(PyObject* item, T start = 0, T stop = -1) const {
        const View& view = self().view;

        // trivial case: empty list
        if (view.size() == 0) {
            std::ostringstream msg;
            msg << repr(item) << " is not in list";
            throw std::invalid_argument(msg.str());
        }

        // normalize start/stop indices
        size_t norm_start = normalize_index(start, true);
        size_t norm_stop = normalize_index(stop, true);
        if (norm_start > norm_stop) {
            throw std::invalid_argument(
                "start index cannot be greater than stop index"
            );
        }

        // if list is doubly-linked and stop is closer to tail than start is to head,
        // then we iterate backward from the tail
        if constexpr (Node::doubly_linked) {
            if ((view.size() - 1 - norm_stop) < norm_start) {
                // get backwards iterator to stop index
                auto iter = view.rbegin();
                size_t idx = view.size() - 1;
                while (idx >= norm_stop) {
                    ++iter;
                    --idx;
                }

                // search until we hit start index
                bool found = false;
                size_t last_observed;
                while (idx >= norm_start) {
                    Node* node = *iter;
                    if (node->eq(item)) {
                        found = true;
                        last_observed = idx;
                    }
                    ++iter;
                    --idx;
                }
                if (found) {
                    return last_observed;
                }

                // item not found
                std::ostringstream msg;
                msg << repr(item) << " is not in list";
                throw std::invalid_argument(msg.str());
            }
        }

        // otherwise, we iterate forward from the head
        auto iter = view.begin();
        size_t idx = 0;
        while (idx < norm_start) {
            ++iter;
            ++idx;
        }

        // search until we hit item or stop index
        while (idx < norm_stop) {
            Node* node = *iter;
            if (node->eq(item)) {
                return idx;
            }
            ++iter;
            ++idx;
        }

        // item not found
        std::ostringstream msg;
        msg << repr(item) << " is not in list";
        throw std::invalid_argument(msg.str());
    }

    /* Count the number of occurrences of an item within a list. */
    template <typename T>
    size_t count(PyObject* item, T start = 0, T stop = -1) const {
        const View& view = self().view;

        // trivial case: empty list
        if (view.size() == 0) {
            return 0;
        }

        // normalize start/stop indices
        size_t norm_start = normalize_index(start, true);
        size_t norm_stop = normalize_index(stop, true);
        if (norm_start > norm_stop) {
            throw std::invalid_argument(
                "start index cannot be greater than stop index"
            );
        }

        // if list is doubly-linked and stop is closer to tail than start is to head,
        // then we iterate backward from the tail
        if constexpr (Node::doubly_linked) {
            if ((view.size() - 1 - norm_stop) < norm_start) {
                // get backwards iterator to stop index
                auto iter = view.iter.reverse();
                size_t idx = view.size() - 1;
                while (idx > norm_stop) {
                    ++iter;
                    --idx;
                }

                // search until we hit start index
                size_t count = 0;
                while (idx >= norm_start) {
                    Node* node = *iter;
                    count += node->eq(item);  // branchless
                    ++iter;
                    --idx;
                }
                return count;
            }
        }

        // otherwise, we iterate forward from the head
        auto iter = view.begin();
        size_t idx = 0;
        while (idx < norm_start) {
            ++iter;
            ++idx;
        }

        // search until we hit item or stop index
        size_t count = 0;
        while (idx < norm_stop) {
            Node* node = *iter;
            count += node->eq(item);  // branchless
            ++iter;
            ++idx;
        }
        return count;
    }

    /* Check if the list contains a certain item. */
    bool contains(PyObject* item) const {
        for (Node* node : self().view) {
            if (node->eq(item)) {
                return true;
            }
        }
        return false;
    }

    /* Remove the first occurrence of an item from a list. */
    void remove(PyObject* item) {
        View& view = self().view;

        // find item in list
        for (auto iter = view.iter(); iter != iter.end(); ++iter) {
            Node* node = *iter;
            if (node->eq(item)) {
                view.recycle(iter.remove());
                return;
            }
        }

        // item not found
        std::ostringstream msg;
        msg << repr(item) << " is not in list";
        throw std::invalid_argument(msg.str());     
    }

    /* Remove an item from a list and return its value. */
    template <typename T>
    PyObject* pop(T index) {
        return (*this)[index].pop();
    }

    /* Remove all elements from a list. */
    void clear() {
        self().view.clear();
    }

    /* Return a shallow copy of the list. */
    Derived copy() const {
        return Derived(self().view.copy());
    }

    /* Sort a list in-place. */
    template <typename Func>
    void sort(Func key = nullptr, bool reverse = false) {
        SortFunc<SortPolicy, Func>::sort(self().view, key, reverse);
    }

    /* Reverse a list in-place. */
    void reverse() {
        View& view = self().view;

        // save original `head` pointer
        Node* head = view.head();
        Node* curr = head;
        
        if constexpr (Node::doubly_linked) {
            // swap all `next`/`prev` pointers
            while (curr != nullptr) {
                Node* next = curr->next();
                curr->next(curr->prev());
                curr->prev(next);
                curr = next;
            }
        } else {
            // swap all `next` pointers
            Node* prev = nullptr;
            while (curr != nullptr) {
                Node* next = curr->next();
                curr->next(prev);
                prev = curr;
                curr = next;
            }
        }

        // swap `head`/`tail` pointers
        view.head(view.tail());
        view.tail(head);
    }

    /* Rotate a list to the right by the specified number of steps. */
    void rotate(long long steps = 1) {
        View& view = self().view;

        // normalize steps
        size_t norm_steps = llabs(steps) % view.size();
        if (norm_steps == 0) {
            return;  // rotated list is identical to original
        }

        // get index at which to split the list
        size_t index;
        size_t rotate_left = (steps < 0);
        if (rotate_left) {  // count from head
            index = norm_steps;
        } else {  // count from tail
            index = view.size() - norm_steps;
        }

        Node* new_head;
        Node* new_tail;

        // identify new head and tail of rotated list
        if constexpr (Node::doubly_linked) {
            // NOTE: if the list is doubly-linked, then we can iterate in either
            // direction to find the junction point.
            if (index > view.size() / 2) {  // backward traversal
                new_head = view.tail();
                for (size_t i = view.size() - 1; i > index; i--) {
                    new_head = new_head->prev();
                }
                new_tail = new_head->prev();

                // join previous head/tail and split at new junction 
                Node::join(view.tail(), view.head());
                Node::split(new_tail, new_head);

                // update head/tail pointers
                view.head(new_head);
                view.tail(new_tail);
                return;
            }
        }

        // forward traversal
        new_tail = view.head();
        for (size_t i = 1; i < index; i++) {
            new_tail = new_tail->next();
        }
        new_head = new_tail->next();

        // split at junction and join previous head/tail
        Node::split(new_tail, new_head);
        Node::join(view.tail(), view.head());

        // update head/tail pointers
        view.head(new_head);
        view.tail(new_tail);
    }

    /* Get a proxy for a value at a particular index of the list. */
    template <typename T>
    ElementProxy operator[](T index) {
        // normalize index (can throw std::out_of_range, type_error)
        size_t norm_index = normalize_index(index);

        // get iterator to index
        View& view = self().view;
        if constexpr (Node::doubly_linked) {
            size_t threshold = (view.size() - (view.size() > 0)) / 2;
            if (norm_index > threshold) {  // backward traversal
                ViewIter<Direction::backward> iter = view.rbegin();
                for (size_t i = view.size() - 1; i > norm_index; --i) {
                    ++iter;
                }
                return ElementProxy(view, iter);
            }
        }

        // forward traversal
        ViewIter<Direction::forward> iter = view.begin();
        for (size_t i = 0; i < norm_index; ++i) {
            ++iter;
        }
        return ElementProxy(view, iter);
    }

    /* Get a proxy for a slice within the list. */
    template <typename... Args>
    SliceProxy slice(Args... args) {
        // can throw type_error, std::invalid_argument, std::runtime_error
        return SliceProxy(self().view, normalize_slice(std::forward<Args>(args)...));
    }

    ///////////////////////
    ////    PROXIES    ////
    ///////////////////////

    /* A proxy for an element at a particular index of the list, as returned by the []
    operator. */
    class ElementProxy {
    public:

        /* Get the value at the current index. */
        inline Value get() const {
            return (*iter)->value();
        }

        /* Set the value at the current index. */
        inline void set(const Value value) {
            Node* node = view.node(value);
            iter.replace(node);
        }

        /* Insert a value at the current index. */
        inline void insert(const Value value) {
            Node* node = view.node(value);
            iter.insert(node);
        }

        /* Delete the value at the current index. */
        inline void del() {
            iter.remove();
        }

        /* Remove the node at the current index and return its value. */
        inline Value pop() {
            Node* node = iter.remove();
            Value result = node->value();
            Py_INCREF(result);  // ensure value is not garbage collected during recycle()
            view.recycle(node);
            return result;
        }

        /* Implicitly convert the proxy to the value where applicable.

        This is syntactic sugar for get() such that `Value value = list[i]` is
        equivalent to `Value value = list[i].get()`.  The same implicit conversion
        is also applied if the proxy is passed to a function that expects a value,
        unless that function is marked as `explicit`. */
        inline operator Value() const {
            return get();
        }

        /* Assign the value at the current index.

        This is syntactic sugar for set() such that `list[i] = value` is equivalent to
        `list[i].set(value)`. */
        inline ElementProxy& operator=(const Value& value) {
            set(value);
            return *this;
        }

    private:
        friend ListInterface;
        View& view;
        Bidirectional<ViewIter> iter;

        template <Direction dir>
        ElementProxy(View& view, ViewIter<dir>& iter) : view(view), iter(iter) {}
    };

    /* A proxy for a slice within a list, as returned by the slice() factory method. */
    class SliceProxy {
    public:

        template <Direction dir = Direction::forward>
        class Iterator;

        /* Extract a slice from a linked list. */
        Derived get() const {
            // allocate a new list to hold the slice
            std::optional<size_t> max_size = view.max_size(); 
            if (max_size.has_value()) {
                max_size = static_cast<size_t>(length());  // adjust to slice length
            }
            View result(max_size, view.specialization());

            // if slice is empty, return empty view
            if (empty()) {
                return Derived(std::move(result));
            }

            // copy nodes from original view into result
            for (Node* node : *this) {
                Node* copy = result.node(*node);
                if (inverted()) {
                    result.link(nullptr, copy, result.head());
                } else {
                    result.link(result.tail(), copy, nullptr);
                }
            }
            return Derived(std::move(result));
        }

        // TODO:
        // l = LinkedList("abcdef")
        // l[2:2] = "xyz"
        // LinkedList(['a', 'b', 'x', 'y', 'c', 'd', 'e', 'f'])  // missing 'z'

        /* Replace a slice within a linked list. */
        void set(PyObject* items) {
            // unpack iterable into reversible sequence
            PySequence sequence(items, "can only assign an iterable");

            // trvial case: both slice and sequence are empty
            if (empty() && sequence.size() == 0) {
                return;
            }

            // check slice length matches sequence length
            if (length() != sequence.size() && step() != 1) {
                // NOTE: Python allows forced insertion if and only if the step size is 1
                std::ostringstream msg;
                msg << "attempt to assign sequence of size " << sequence.size();
                msg << " to extended slice of size " << length();
                throw std::invalid_argument(msg.str());
            }

            // helper struct to undo changes in case of error
            struct RecoveryArray {
                Node* nodes;
                size_t length;

                RecoveryArray(size_t length) : length(length) {
                    nodes = static_cast<Node*>(malloc(sizeof(Node) * length));
                    if (nodes == nullptr) {
                        throw std::bad_alloc();
                    }
                }

                ~RecoveryArray() { free(nodes); }

                Node& operator[](size_t index) { return nodes[index]; }
            };

            // allocate recovery array
            RecoveryArray recovery(length());

            // loop 1: remove current nodes in slice
            for (auto iter = this->iter(); iter != iter.end(); ++iter) {
                Node* node = iter.remove();  // remove node from list
                new (&recovery[iter.index()]) Node(*node);  // copy into recovery array
                view.recycle(node);  // recycle original node
            }

            // loop 2: insert new nodes from sequence into vacated slice
            for (auto iter = this->iter(sequence.size()); iter != iter.end(); ++iter) {
                PyObject* item;
                if (inverted()) {  // count from back
                    item = sequence[sequence.size() - 1 - iter.index()];
                } else {  // count from front
                    item = sequence[iter.index()];
                }

                // allocate a new node for the list
                try {
                    Node* node = view.node(item);
                    iter.insert(node);

                // rewind if an error occurs
                } catch (...) {
                    // loop 3: remove nodes that have already been added to list
                    for (auto i = this->iter(iter.index()); i != i.end(); ++i) {
                        Node* node = i.remove();  // remove from list
                        view.recycle(node);
                    }

                    // loop 4: reinsert original nodes from recovery array
                    for (auto i = this->iter(); i != i.end(); ++i) {
                        Node* recovery_node = &recovery[i.index()];
                        i.insert(view.node(*recovery_node));  // copy into list
                        recovery_node->~Node();  // destroy recovery node
                    }

                    throw;  // propagate
                }
            }

            // loop 3: deallocate removed nodes
            for (size_t i = 0; i < length(); i++) {
                (&recovery[i])->~Node();  // release recovery node
            }
        }

        /* Delete a slice within a linked list. */
        void del() {
            // trivial case: slice is empty
            if (empty()) {
                return;
            }

            // recycle every node in slice
            for (auto iter = this->iter(); iter != iter.end(); ++iter) {
                Node* node = iter.remove();  // remove from list
                view.recycle(node);
            }
        }

        /* Pass through to SliceIndices. */
        inline long long start() const { return indices.start; }
        inline long long stop() const { return indices.stop; }
        inline long long step() const { return indices.step; }
        inline size_t abs_step() const { return indices.abs_step; }
        inline size_t first() const { return indices.first; }
        inline size_t last() const { return indices.last; }
        inline size_t length() const { return indices.length; }
        inline bool empty() const { return indices.length == 0; }
        inline bool backward() const { return indices.backward; }
        inline bool inverted() const { return indices.inverted; }

        // TODO: SliceProxy might be better off outside the class and templated on the
        // variant.  It could also have a full battery of iterators.  The reverse
        // iterators would all use a stack to traverse the list in reverse order.

        // list.slice().iter()
        // list.slice().iter.reverse()
        // list.slice().iter.begin()
        // list.slice().iter.end()
        // list.slice().iter.rbegin()
        // list.slice().iter.rend()
        // list.slice().iter.python()
        // list.slice().iter.rpython()

        /* Return a coupled pair of iterators with a possible length override. */
        inline auto iter(std::optional<size_t> length = std::nullopt) const {
            using Forward = Iterator<Direction::forward>;

            // default to length of slice
            if (!length.has_value()) {
                return CoupledIterator<Bidirectional<Iterator>>(begin(), end());  
            }

            // use length override if given
            size_t len = length.value();

            // backward traversal
            if constexpr (Node::doubly_linked) {
                using Backward = Iterator<Direction::backward>;
                if (backward()) {
                    return CoupledIterator<Bidirectional<Iterator>>(
                        Bidirectional(Backward(view, origin(), indices, len)),
                        Bidirectional(Backward(view, indices, len))
                    );
                }
            }

            // forward traversal
            return CoupledIterator<Bidirectional<Iterator>>(
                Bidirectional(Forward(view, origin(), indices, len)),
                Bidirectional(Forward(view, indices, len))
            );
        }

        /* Return an iterator to the start of the slice. */
        inline auto begin() const {
            using Forward = Iterator<Direction::forward>;

            // account for empty sequence
            if (empty()) {
                return Bidirectional(Forward(view, indices, length()));
            }

            // backward traversal
            if constexpr (Node::doubly_linked) {
                using Backward = Iterator<Direction::backward>;
                if (backward()) {
                    return Bidirectional(Backward(view, origin(), indices, length()));
                }
            }

            // forward traversal
            return Bidirectional(Forward(view, origin(), indices, length()));        
        }

        /* Return an iterator to the end of the slice. */
        inline auto end() const {
            using Forward = Iterator<Direction::forward>;

            // return same orientation as begin()
            if (empty()) {
                return Bidirectional(Forward(view, indices, length()));
            }

            // backward traversal
            if constexpr (Node::doubly_linked) {
                using Backward = Iterator<Direction::backward>;
                if (backward()) {
                    return Bidirectional(Backward(view, indices, length()));
                }
            }

            // forward traversal
            return Bidirectional(Forward(view, indices, length()));
        }

        // TODO: Slice iterators should yield the contents of each node rather than
        // the nodes themselves, just like the regular iterators.

        /* A specialized iterator built for slice traversal. */
        template <Direction dir>
        class Iterator : public ViewIter<dir> {
            using Base = ViewIter<dir>;

        public:
            /* Prefix increment to advance the iterator to the next node in the slice. */
            inline Iterator& operator++() {
                ++this->idx;
                if (this->idx == length_override) {
                    return *this;  // don't jump on last iteration
                }

                if constexpr (dir == Direction::backward) {
                    for (size_t i = implicit_skip; i < indices.abs_step; ++i) {
                        this->next = this->curr;
                        this->curr = this->prev;
                        this->prev = this->curr->prev();
                    }
                } else {
                    for (size_t i = implicit_skip; i < indices.abs_step; ++i) {
                        this->prev = this->curr;
                        this->curr = this->next;
                        this->next = this->curr->next();
                    }
                }
                return *this;
            }

            /* Inequality comparison to terminate the slice. */
            template <Direction T>
            inline bool operator!=(const Iterator<T>& other) const {
                return idx != other.idx;
            }

            //////////////////////////////
            ////    HELPER METHODS    ////
            //////////////////////////////

            /* Get the current index of the iterator within the list. */
            inline size_t index() const {
                return idx;
            }

            /* Remove the node at the current position. */
            inline Node* remove() {
                ++implicit_skip;
                return Base::remove();
            }

            /* Copy constructor. */
            Iterator(const Iterator& other) :
                Base(other), indices(other.indices), idx(other.idx),
                length_override(other.length_override),
                implicit_skip(other.implicit_skip)
            {}

            /* Move constructor. */
            Iterator(Iterator&& other) :
                Base(std::move(other)), indices(std::move(other.indices)),
                idx(other.idx), length_override(other.length_override),
                implicit_skip(other.implicit_skip)
            {}

        protected:
            friend SliceProxy;
            const SliceIndices& indices;
            size_t idx;
            size_t length_override;
            size_t implicit_skip;

            ////////////////////////////
            ////    CONSTRUCTORS    ////
            ////////////////////////////

            /* Get an iterator to the start of the slice. */
            Iterator(
                View& view,
                Node* origin,
                const SliceIndices& indices,
                size_t length_override
            ) : Base(view), indices(indices), idx(0), length_override(length_override),
                implicit_skip(0)
            {
                if constexpr (dir == Direction::backward) {
                    this->next = origin;
                    if (this->next == nullptr) {
                        this->curr = this->view.tail();
                    } else {
                        this->curr = this->next->prev();
                    }
                    if (this->curr != nullptr) {
                        this->prev = this->curr->prev();
                    }
                } else {
                    this->prev = origin;
                    if (this->prev == nullptr) {
                        this->curr = this->view.head();
                    } else {
                        this->curr = this->prev->next();
                    }
                    if (this->curr != nullptr) {
                        this->next = this->curr->next();
                    }
                }
            }

            /* Get an iterator to terminate the slice. */
            Iterator(View& view, const SliceIndices& indices, size_t length_override) :
                Base(view), indices(indices), idx(length_override),
                length_override(length_override), implicit_skip(0)
            {}

        };

    private:
        friend ListInterface;
        View& view;
        const SliceIndices indices;
        mutable bool found;  // indicates whether we've cached the origin node
        mutable Node* _origin;  // node that immediately precedes slice (can be NULL)

        /* Construct a SliceProxy with at least one element. */
        SliceProxy(View& view, SliceIndices&& indices) :
            view(view), indices(indices), found(false), _origin(nullptr)
        {}

        /* Find and cache the origin node for the slice. */
        Node* origin() const {
            if (found) {
                return _origin;
            }

            // find origin node
            if constexpr (Node::doubly_linked) {
                if (backward()) {  // backward traversal
                    Node* next = nullptr;
                    Node* curr = view.tail();
                    for (size_t i = view.size() - 1; i > first(); i--) {
                        next = curr;
                        curr = curr->prev();
                    }
                    found = true;
                    _origin = next;
                    return _origin;
                }
            }

            // forward traversal
            Node* prev = nullptr;
            Node* curr = view.head();
            for (size_t i = 0; i < first(); i++) {
                prev = curr;
                curr = curr->next();
            }
            found = true;
            _origin = prev;
            return _origin;
        }

    };

private:

    /* Enable access to members of the Derived type. */
    inline Derived& self() {
        return static_cast<Derived&>(*this);
    }

    /* Enable access to members of the Derived type in a const context. */
    inline const Derived& self() const {
        return static_cast<const Derived&>(*this);
    }

    /* A simple class representing the normalized indices needed to construct a
    coherent slice. */
    class SliceIndices {
    public:

        /* Get the original indices that were supplied to the constructor. */
        const long long start;
        const long long stop;
        const long long step;
        const size_t abs_step;

        /* Get the first and last included indices. */
        size_t first;
        size_t last;

        /* Get the number of items included in the slice. */
        const size_t length;

        /* Check if the first and last indices conform to the expected step size. */
        bool inverted;
        bool backward;

        /* Copy constructor. */
        SliceIndices(const SliceIndices& other) :
            start(other.start), stop(other.stop), step(other.step),
            abs_step(other.abs_step), first(other.first), last(other.last),
            length(other.length), inverted(other.inverted), backward(other.backward)
        {}

        /* Move constructor. */
        SliceIndices(SliceIndices&& other) :
            start(other.start), stop(other.stop), step(other.step),
            abs_step(other.abs_step), first(other.first), last(other.last),
            length(other.length), inverted(other.inverted), backward(other.backward)
        {}

        /* Assignment operators deleted due to presence of const members. */
        SliceIndices& operator=(const SliceIndices& other) = delete;
        SliceIndices& operator=(SliceIndices&& other) = delete;

    private:
        friend ListInterface;

        SliceIndices(
            const long long start,
            const long long stop,
            const long long step,
            const size_t length,
            const size_t view_size
        ) : start(start), stop(stop), step(step), abs_step(llabs(step)),
            first(0), last(0), length(length), inverted(false), backward(false)
        {
            // convert to closed interval [start, closed]
            long long mod = py_modulo((stop - start), step);
            long long closed = (mod == 0) ? (stop - step) : (stop - mod);

            // get direction to traverse slice based on singly-/doubly-linked status
            std::pair<size_t, size_t> dir = slice_direction(closed, view_size);
            first = dir.first;
            last = dir.second;

            // Because we've adjusted our indices to minimize total iterations, we might
            // not be iterating in the same direction as the step size would indicate.
            // We must account for this when getting/setting items in the slice.
            backward = (first > ((view_size - (view_size > 0)) / 2));
            inverted = backward ^ (step < 0);
        }

        /* A Python-style modulo operator (%). */
        template <typename T>
        inline static T py_modulo(T a, T b) {
            // NOTE: Python's `%` operator is defined such that the result has
            // the same sign as the divisor (b).  This differs from C/C++, where
            // the result has the same sign as the dividend (a).
            return (a % b + b) % b;
        }

        /* Swap the start and stop indices based on singly-/doubly-linked status. */
        std::pair<long long, long long> slice_direction(
            long long closed,
            size_t view_size
        ) {
            // if doubly-linked, start at whichever end is closest to slice boundary
            if constexpr (Node::doubly_linked) {
                long long size = static_cast<long long>(view_size);
                if (
                    (step > 0 && start <= size - closed) ||
                    (step < 0 && size - start <= closed)
                ) {
                    return std::make_pair(start, closed);
                }
                return std::make_pair(closed, start);
            }

            // if singly-linked, always start from head of list
            if (step > 0) {
                return std::make_pair(start, closed);
            }
            return std::make_pair(closed, start);
        }
    };

protected:

    /* Normalize a numeric index, applying Python-style wraparound and bounds
    checking. */
    template <typename T>
    size_t normalize_index(T index, bool truncate = false) const {
        const View& view = self().view;

        // wraparound negative indices
        bool lt_zero = index < 0;
        if (lt_zero) {
            index += view.size();
            lt_zero = index < 0;
        }

        // boundscheck
        if (lt_zero || index >= static_cast<T>(view.size())) {
            if (truncate) {
                if (lt_zero) {
                    return 0;
                }
                return view.size() - 1;
            }
            throw std::out_of_range("list index out of range");
        }

        // return as size_t
        return static_cast<size_t>(index);
    }

    /* Normalize a Python integer for use as an index to the list. */
    size_t normalize_index(PyObject* index, bool truncate = false) const {
        // check that index is a Python integer
        if (!PyLong_Check(index)) {
            throw type_error("index must be a Python integer");
        }

        const View& view = self().view;

        // comparisons are kept at the python level until we're ready to return
        PyObject* py_zero = PyLong_FromSize_t(0);  // new reference
        PyObject* py_size = PyLong_FromSize_t(view.size());  // new reference
        int lt_zero = PyObject_RichCompareBool(index, py_zero, Py_LT);

        // wraparound negative indices
        bool release_index = false;
        if (lt_zero) {
            index = PyNumber_Add(index, py_size);  // new reference
            lt_zero = PyObject_RichCompareBool(index, py_zero, Py_LT);
            release_index = true;  // remember to DECREF index later
        }

        // boundscheck - value is bad
        if (lt_zero || PyObject_RichCompareBool(index, py_size, Py_GE)) {
            Py_DECREF(py_zero);
            Py_DECREF(py_size);
            if (release_index) {
                Py_DECREF(index);
            }

            // apply truncation if directed
            if (truncate) {
                if (lt_zero) {
                    return 0;
                }
                return view.size() - 1;
            }

            // raise IndexError
            throw std::out_of_range("list index out of range");
        }

        // value is good - cast to size_t
        size_t result = PyLong_AsSize_t(index);

        // clean up references
        Py_DECREF(py_zero);
        Py_DECREF(py_size);
        if (release_index) {
            Py_DECREF(index);
        }

        return result;
    }

    /* Normalize slice indices, applying Python-style wraparound and bounds
    checking. */
    SliceIndices normalize_slice(
        std::optional<long long> start = std::nullopt,
        std::optional<long long> stop = std::nullopt,
        std::optional<long long> step = std::nullopt
    ) const {
        // normalize slice indices
        long long size = static_cast<long long>(self().view.size());
        long long default_start = (step.value_or(0) < 0) ? (size - 1) : (0);
        long long default_stop = (step.value_or(0) < 0) ? (-1) : (size);
        long long default_step = 1;

        // normalize step
        long long step_ = step.value_or(default_step);
        if (step_ == 0) {
            throw std::invalid_argument("slice step cannot be zero");
        }

        // normalize start index
        long long start_ = start.value_or(default_start);
        if (start_ < 0) {
            start_ += size;
            if (start_ < 0) {
                start_ = (step_ < 0) ? (-1) : (0);
            }
        } else if (start_ >= size) {
            start_ = (step_ < 0) ? (size - 1) : (size);
        }

        // normalize stop index
        long long stop_ = stop.value_or(default_stop);
        if (stop_ < 0) {
            stop_ += size;
            if (stop_ < 0) {
                stop_ = (step_ < 0) ? -1 : 0;
            }
        } else if (stop_ > size) {
            stop_ = (step_ < 0) ? (size - 1) : (size);
        }

        // get length of slice
        size_t length = std::max(
            (stop_ - start_ + step_ - (step_ > 0 ? 1 : -1)) / step_,
            static_cast<long long>(0)
        );

        // return as SliceIndices
        return SliceIndices(start_, stop_, step_, length, size);
    }

    /* Normalize a Python slice object, applying Python-style wraparound and bounds
    checking. */
    SliceIndices normalize_slice(PyObject* py_slice) const {
        // check that input is a Python slice object
        if (!PySlice_Check(py_slice)) {
            throw type_error("index must be a Python slice");
        }

        size_t size = self().size();

        // use CPython API to get slice indices
        Py_ssize_t py_start, py_stop, py_step, py_length;
        int err = PySlice_GetIndicesEx(
            py_slice, size, &py_start, &py_stop, &py_step, &py_length
        );
        if (err == -1) {
            throw std::runtime_error("failed to normalize slice");
        }

        // cast from Py_ssize_t
        long long start = static_cast<long long>(py_start);
        long long stop = static_cast<long long>(py_stop);
        long long step = static_cast<long long>(py_step);
        size_t length = static_cast<size_t>(py_length);

        // return as SliceIndices
        return SliceIndices(start, stop, step, length, size);
    }

};


/////////////////////////////
////    CONCATENATION    ////
/////////////////////////////


/* A mixin that adds operator overloads that mimic the behavior of Python lists with
respect to concatenation, repetition, and lexicographic comparison. */
template <typename Derived>
struct Concatenateable {
    static constexpr bool enable = std::is_base_of_v<Concatenateable<Derived>, Derived>;

    /* Overload the + operator to allow concatenation of Derived types from both
    Python and C++. */
    template <typename T>
    friend Derived operator+(const Derived& lhs, const T& rhs);
    template <typename T>
    friend T operator+(const T& lhs, const Derived& rhs);

    /* Overload the += operator to allow in-place concatenation of Derived types from
    both Python and C++. */
    template <typename T>
    friend Derived& operator+=(Derived& lhs, const T& rhs);

};


/* Allow Python-style concatenation between Linked data structures and arbitrary
Python/C++ containers. */
template <typename T, typename Derived>
inline auto operator+(const Derived& lhs, const T& rhs)
    -> std::enable_if_t<Concatenateable<Derived>::enable, Derived>
{
    std::optional<Derived> result = lhs.copy();
    if (!result.has_value()) {
        throw std::runtime_error("could not copy list");
    }
    result.value().extend(rhs);  // must be specialized for T
    return Derived(std::move(result.value()));
}


/* Allow Python-style concatenation between list-like C++ containers and Linked data
structures. */
template <typename T, typename Derived>
inline auto operator+(const T& lhs, const Derived& rhs)
    -> std::enable_if_t<
        // first, check that T is a list-like container with a range-based insert
        // method that returns an iterator.  This is true for all STL containers.
        std::is_same_v<
            decltype(
                std::declval<T>().insert(
                    std::declval<T>().end(),
                    std::declval<Derived>().begin(),
                    std::declval<Derived>().end()
                )
            ),
            typename T::iterator
        >,
        // next, check that Derived inherits from Concatenateable
        std::enable_if_t<Concatenateable<Derived>::enable, T>
    >
{
    T result = lhs;
    result.insert(result.end(), rhs.begin(), rhs.end());  // STL compliant
    return result;
}


/* Allow Python-style concatenation between Python sequences and Linked data
structures. */
template <typename T, typename Derived>
inline auto operator+(const PyObject* lhs, const Derived& rhs)
    -> std::enable_if_t<Concatenateable<Derived>::enable, PyObject*>
{
    // Check that lhs is a Python sequence
    if (!PySequence_Check(lhs)) {
        std::ostringstream msg;
        msg << "can only concatenate sequence (not '";
        msg << lhs->ob_type->tp_name << "') to sequence";
        throw type_error(msg.str());
    }

    // unpack list into Python sequence
    PyObject* seq = PySequence_List(rhs.iter.python());  // new ref
    if (seq == nullptr) {
        return nullptr;  // propagate error
    }

    // concatenate using Python API
    PyObject* concat = PySequence_Concat(lhs, seq);
    Py_DECREF(seq);
    return concat;
}


/* Allow in-place concatenation for Linked data structures using the += operator. */
template <typename T, typename Derived>
inline auto operator+=(Derived& lhs, const T& rhs)
    -> std::enable_if_t<Concatenateable<Derived>::enable, Derived&>
{
    lhs.extend(rhs);  // must be specialized for T
    return lhs;
}


//////////////////////////
////    REPETITION    ////
//////////////////////////


// TODO: we could probably optimize repetition by allocating a contiguous block of
// nodes equal to list.size() * rhs.  We could also remove the extra copy in *= by
// using an iterator to the end of the list and reusing it for each iteration.


/* A mixin that adds operator overloads that mimic the behavior of Python lists with
respect to concatenation, repetition, and lexicographic comparison. */
template <typename Derived>
struct Repeatable {
    static constexpr bool enable = std::is_base_of_v<Repeatable<Derived>, Derived>;

    // NOTE: We use a dummy typename to avoid forward declarations of operator* and
    // operator*=.  It doesn't actually affect the implementation of either overload.

    /* Overload the * operator to allow repetition of Derived types from both Python
    and C++. */
    template <typename>
    friend Derived operator*(const Derived& lhs, const ssize_t rhs);
    template <typename>
    friend Derived operator*(const Derived& lhs, const PyObject* rhs);
    template <typename>
    friend Derived operator*(const ssize_t lhs, const Derived& rhs);
    template <typename>
    friend Derived operator*(const PyObject* lhs, const Derived& rhs);

    /* Overload the *= operator to allow in-place repetition of Derived types from
    both Python and C++. */
    template <typename>
    friend Derived& operator*=(Derived& lhs, const ssize_t rhs);
    template <typename>
    friend Derived& operator*=(Derived& lhs, const PyObject* rhs);

};


/* Allow Python-style repetition for Linked data structures using the * operator. */
template <typename = void, typename Derived>
auto operator*(const Derived& lhs, const ssize_t rhs)
    -> std::enable_if_t<Repeatable<Derived>::enable, Derived>
{
    // handle empty repitition
    if (rhs <= 0 || lhs.size() == 0) {
        return Derived(lhs.max_size(), lhs.specialization());
    }

    // copy lhs
    std::optional<Derived> result = lhs.copy();
    if (!result.has_value()) {
        throw std::runtime_error("could not copy list");
    }

    // extend copy rhs - 1 times
    for (ssize_t i = 1; i < rhs; ++i) {
        result.value().extend(lhs);
    }

    // move result into return value
    return Derived(std::move(result.value()));
}


/* Allow Python-style repetition for Linked data structures using the * operator. */
template <typename = void, typename Derived>
inline auto operator*(const ssize_t lhs, const Derived& rhs)
    -> std::enable_if_t<Repeatable<Derived>::enable, Derived>
{
    return rhs * lhs;  // symmetric
}


/* Allow Python-style repetition for Linked data structures using the * operator. */
template <typename = void, typename Derived>
auto operator*(const Derived& lhs, const PyObject* rhs)
    -> std::enable_if_t<Repeatable<Derived>::enable, Derived>
{
    // Check that rhs is a Python integer
    if (!PyLong_Check(rhs)) {
        std::ostringstream msg;
        msg << "can't multiply sequence by non-int of type '";
        msg << rhs->ob_type->tp_name << "'";
        throw type_error(msg.str());
    }

    // convert to C++ integer
    ssize_t val = PyLong_AsSsize_t(rhs);
    if (val == -1 && PyErr_Occurred()) {
        throw catch_python<type_error>();
    }

    // delegate to C++ overload
    return lhs * val;
}


/* Allow Python-style repetition for Linked data structures using the * operator. */
template <typename = void, typename Derived>
inline auto operator*(const PyObject* lhs, const Derived& rhs)
    -> std::enable_if_t<Repeatable<Derived>::enable, Derived>
{
    return rhs * lhs;  // symmetric
}


/* Allow in-place repetition for Linked data structures using the *= operator. */
template <typename = void, typename Derived>
auto operator*=(Derived& lhs, const ssize_t rhs)
    -> std::enable_if_t<Repeatable<Derived>::enable, Derived&>
{
    // handle empty repitition
    if (rhs <= 0 || lhs.size() == 0) {
        lhs.clear();
        return lhs;
    }

    // copy lhs
    std::optional<Derived> copy = lhs.copy();
    if (!copy.has_value()) {
        throw std::runtime_error("could not copy list");
    }

    // extend lhs rhs - 1 times
    for (ssize_t i = 1; i < rhs; ++i) {
        lhs.extend(copy.value());
    }
    return lhs;
}


/* Allow in-place repetition for Linked data structures using the *= operator. */
template <typename = void, typename Derived>
inline auto operator*=(Derived& lhs, const PyObject* rhs)
    -> std::enable_if_t<Repeatable<Derived>::enable, Derived&>
{
    // Check that rhs is a Python integer
    if (!PyLong_Check(rhs)) {
        std::ostringstream msg;
        msg << "can't multiply sequence by non-int of type '";
        msg << rhs->ob_type->tp_name << "'";
        throw type_error(msg.str());
    }

    // convert to C++ integer
    ssize_t val = PyLong_AsSsize_t(rhs);
    if (val == -1 && PyErr_Occurred()) {
        throw catch_python<type_error>();
    }

    // delegate to C++ overload
    return lhs *= val;
}


/////////////////////////////////////////
////    LEXICOGRAPHIC COMPARISONS    ////
/////////////////////////////////////////


/* A mixin that adds operator overloads that mimic the behavior of Python lists with
respect to concatenation, repetition, and lexicographic comparison. */
template <typename Derived>
struct Lexicographic {
    static constexpr bool enable = std::is_base_of_v<Lexicographic<Derived>, Derived>;

    /* Overload the < operator to allow lexicographic comparison between Derived types
    and arbitrary C++ containers/Python sequences. */
    template <typename T>
    friend bool operator<(const Derived& lhs, const T& rhs);
    template <typename T>
    friend bool operator<(const T& lhs, const Derived& rhs);

    /* Overload the <= operator to allow lexicographic comparison between Derived types
    and arbitrary C++ containers/Python sequences. */
    template <typename T>
    friend bool operator<=(const Derived& lhs, const T& rhs);
    template <typename T>
    friend bool operator<=(const T& lhs, const Derived& rhs);

    /* Overload the == operator to allow lexicographic comparison between Derived types
    and arbitrary C++ containers/Python sequences. */
    template <typename T>
    friend bool operator==(const Derived& lhs, const T& rhs);
    template <typename T>
    friend bool operator==(const T& lhs, const Derived& rhs);

    /* Overload the != operator to allow lexicographic comparison between Derived types
    and arbitrary C++ containers/Python sequences. */
    template <typename T>
    friend bool operator!=(const Derived& lhs, const T& rhs);
    template <typename T>
    friend bool operator!=(const T& lhs, const Derived& rhs);

    /* Overload the > operator to allow lexicographic comparison between Derived types
    and arbitrary C++ containers/Python sequences. */
    template <typename T>
    friend bool operator>(const Derived& lhs, const T& rhs);
    template <typename T>
    friend bool operator>(const T& lhs, const Derived& rhs);

    /* Overload the >= operator to allow lexicographic comparison between Derived types
    and arbitrary C++ containers/Python sequences. */
    template <typename T>
    friend bool operator>=(const Derived& lhs, const T& rhs);
    template <typename T>
    friend bool operator>=(const T& lhs, const Derived& rhs);

};


/* Allow lexicographic < comparison between Linked data structures and compatible C++
containers. */
template <typename T, typename Derived>
auto operator<(const Derived& lhs, const T& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    // get coupled iterators
    auto iter_lhs = std::begin(lhs);
    auto end_lhs = std::end(lhs);
    auto iter_rhs = std::begin(rhs);
    auto end_rhs = std::end(rhs);

    // loop until one of the sequences is exhausted
    while (iter_lhs != end_lhs && iter_rhs != end_rhs) {
        if ((*iter_lhs)->value() < *iter_rhs) return true;
        if (*iter_rhs < (*iter_lhs)->value()) return false;
        ++iter_lhs;
        ++iter_rhs;
    }

    // check if lhs is shorter than rhs
    return (iter_lhs == end_lhs && iter_rhs != end_rhs);
}


/* Allow lexicographic < comparison between Linked data structures and Python
sequences. */
template <typename Derived>
auto operator<(const Derived& lhs, const PyObject* rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    using Node = typename Derived::Node;

    // check that rhs is a Python sequence
    if (!PySequence_Check(rhs)) {
        std::ostringstream msg;
        msg << "can only compare list to sequence (not '";
        msg << rhs->ob_type->tp_name << "')";
        throw type_error(msg.str());
    }

    // get coupled iterators
    auto iter_lhs = std::begin(lhs);
    auto end_lhs = std::end(lhs);
    PyIterable pyiter_rhs(rhs);  // handles reference counts
    auto iter_rhs = pyiter_rhs.begin();
    auto end_rhs = pyiter_rhs.end();

    // loop until one of the sequences is exhausted
    while (iter_lhs != end_lhs && iter_rhs != end_rhs) {
        Node* node = *iter_lhs;
        if (node->lt(*iter_rhs)) {
            return true;
        }

        // compare rhs < lhs
        int comp = PyObject_RichCompareBool(*iter_rhs, node->value(), Py_LT);
        if (comp == -1) {
            throw catch_python<type_error>();
        } else if (comp == 1) {
            return false;
        }

        // advance iterators
        ++iter_lhs;
        ++iter_rhs;
    }

    // check if lhs is shorter than rhs
    return (iter_lhs == end_lhs && iter_rhs != end_rhs);
}


/* Allow lexicographic < comparison between compatible C++ containers and Linked data
structures. */
template <typename T, typename Derived>
inline auto operator<(const T& lhs, const Derived& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    return rhs > lhs;  // implies lhs < rhs
}


/* Allow lexicographic <= comparison between Linked data structures and compatible C++
containers. */
template <typename T, typename Derived>
auto operator<=(const Derived& lhs, const T& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    // get coupled iterators
    auto iter_lhs = std::begin(lhs);
    auto end_lhs = std::end(lhs);
    auto iter_rhs = std::begin(rhs);
    auto end_rhs = std::end(rhs);

    // loop until one of the sequences is exhausted
    while (iter_lhs != end_lhs && iter_rhs != end_rhs) {
        if ((*iter_lhs)->value() < *iter_rhs) return true;
        if (*iter_rhs < (*iter_lhs)->value()) return false;
        ++iter_lhs;
        ++iter_rhs;
    }

    // check if lhs is exhausted
    return (iter_lhs == end_lhs);
}


/* Allow lexicographic <= comparison between Linked data structures and Python
sequences. */
template <typename Derived>
auto operator<=(const Derived& lhs, const PyObject* rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    using Node = typename Derived::Node;

    // check that rhs is a Python sequence
    if (!PySequence_Check(rhs)) {
        std::ostringstream msg;
        msg << "can only compare list to sequence (not '";
        msg << rhs->ob_type->tp_name << "')";
        throw type_error(msg.str());
    }

    // get coupled iterators
    auto iter_lhs = std::begin(lhs);
    auto end_lhs = std::end(lhs);
    PyIterable pyiter_rhs(rhs);  // handles reference counts
    auto iter_rhs = pyiter_rhs.begin();
    auto end_rhs = pyiter_rhs.end();

    // loop until one of the sequences is exhausted
    while (iter_lhs != end_lhs && iter_rhs != end_rhs) {
        Node* node = *iter_lhs;
        if (node->lt(*iter_rhs)) {
            return true;
        }

        // compare rhs < lhs
        int comp = PyObject_RichCompareBool(*iter_rhs, node->value(), Py_LT);
        if (comp == -1) {
            throw std::runtime_error("could not compare list elements");
        } else if (comp == 1) {
            return false;
        }

        // advance iterators
        ++iter_lhs;
        ++iter_rhs;
    }

    // check if lhs is exhausted
    return (iter_lhs == end_lhs);
}


/* Allow lexicographic <= comparison between compatible C++ containers and Linked data
structures. */
template <typename T, typename Derived>
inline auto operator<=(const T& lhs, const Derived& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    return rhs >= lhs;  // implies lhs <= rhs
}


/* Allow == comparison between Linked data structures and compatible C++ containers. */
template <typename T, typename Derived>
auto operator==(const Derived& lhs, const T& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    using Node = typename Derived::Node;

    if (lhs.size() != rhs.size()) {
        return false;
    }

    // compare elements in order
    auto iter_rhs = std::begin(rhs);
    for (const Node& item : lhs) {
        if (item->value() != *iter_rhs) {
            return false;
        }
        ++iter_rhs;
    }

    return true;
}


/* Allow == comparison betwen Linked data structures and Python sequences. */
template <typename Derived>
auto operator==(const Derived& lhs, const PyObject* rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    using Node = typename Derived::Node;

    // check that rhs is a Python sequence
    if (!PySequence_Check(rhs)) {
        std::ostringstream msg;
        msg << "can only compare list to sequence (not '";
        msg << rhs->ob_type->tp_name << "')";
        throw type_error(msg.str());
    }

    // check that lhs and rhs have the same length
    Py_ssize_t len = PySequence_Length(rhs);
    if (len == -1) {
        std::ostringstream msg;
        msg << "could not get length of sequence (of type '";
        msg << rhs->ob_type->tp_name << "')";
        throw type_error(msg.str());
    } else if (lhs.size() != static_cast<size_t>(len)) {
        return false;
    }

    // compare elements in order
    PyIterable pyiter_rhs(rhs);  // handles reference counts
    auto iter_rhs = pyiter_rhs.begin();
    for (const Node& item : lhs) {
        if (item->ne(*iter_rhs)) {
            return false;
        }
        ++iter_rhs;
    }

    return true;
}


/* Allow == comparison between compatible C++ containers and Linked data structures. */
template <typename T, typename Derived>
inline auto operator==(const T& lhs, const Derived& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    return rhs == lhs;
}


/* Allow != comparison between Linked data structures and compatible C++ containers. */
template <typename T, typename Derived>
inline auto operator!=(const Derived& lhs, const T& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    return !(lhs == rhs);
}


/* Allow != comparison between compatible C++ containers Linked data structures. */
template <typename T, typename Derived>
inline auto operator!=(const T& lhs, const Derived& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    return !(lhs == rhs);
}


/* Allow lexicographic >= comparison between Linked data structures and compatible C++
containers. */
template <typename T, typename Derived>
inline auto operator>=(const Derived& lhs, const T& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    return !(lhs < rhs);
}


/* Allow lexicographic >= comparison between compatible C++ containers and Linked data
structures. */
template <typename T, typename Derived>
inline auto operator>=(const T& lhs, const Derived& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    return !(lhs < rhs);
}


/* Allow lexicographic > comparison between Linked data structures and compatible C++
containers. */
template <typename T, typename Derived>
inline auto operator>(const Derived& lhs, const T& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    return !(lhs <= rhs);
}


/* Allow lexicographic > comparison between compatible C++ containers and Linked data
structures. */
template <typename T, typename Derived>
inline auto operator>(const T& lhs, const Derived& rhs)
    -> std::enable_if_t<Lexicographic<Derived>::enable, bool>
{
    return !(lhs <= rhs);
}


#endif  // BERTRAND_STRUCTS_LIST_LIST_H include guard
