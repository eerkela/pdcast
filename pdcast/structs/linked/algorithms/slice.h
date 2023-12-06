#ifndef BERTRAND_STRUCTS_LINKED_ALGORITHMS_SLICE_H
#define BERTRAND_STRUCTS_LINKED_ALGORITHMS_SLICE_H

#include <cstddef>  // size_t
#include <optional>  // std::optional
#include <sstream>  // std::ostringstream
#include <stack>  // std::stack
#include <Python.h>  // CPython API
#include "../../util/container.h"  // PySlice, PySequence
#include "../../util/except.h"  // TypeError()
#include "../../util/iter.h"  // iter()
#include "../../util/math.h"  // py_modulo()
#include "../core/view.h"  // ViewTraits, Direction


namespace bertrand {
namespace structs {
namespace linked {


    ///////////////////////////////////
    ////    SLICE NORMALIZATION    ////
    ///////////////////////////////////


    /* Data class representing normalized indices needed to construct a SliceProxy. */
    template <typename View>
    class SliceIndices {
        template <typename _View>
        friend SliceIndices<_View> normalize_slice(
            const _View& view,
            std::optional<long long> start,
            std::optional<long long> stop,
            std::optional<long long> step
        );

        template <typename _View>
        friend SliceIndices<_View> normalize_slice(const _View& view, PyObject* slice);

        /* Construct a SliceIndices object from normalized indices. */
        SliceIndices(
            long long start,
            long long stop,
            long long step,
            size_t length,
            size_t view_size
        ) : start(start), stop(stop), step(step), abs_step(std::llabs(step)),
            first(0), last(0), length(length), inverted(false), backward(false)
        {
            using Node = typename View::Node;

            // make closed interval
            long long mod = bertrand::util::py_modulo((stop - start), step);
            long long closed = (mod == 0) ? (stop - step) : (stop - mod);

            // flip start/stop based on singly-/doubly-linked status
            if constexpr (NodeTraits<Node>::has_prev) {
                long long lsize = static_cast<long long>(view_size);
                bool congruent = (
                    (step > 0 && start <= lsize - closed) ||
                    (step < 0 && lsize - start <= closed)
                );
                first = congruent ? start : closed;
                last = congruent ? closed : start;
                backward = (
                    first > last ||
                    (first == last && first > ((view_size - (view_size > 0)) / 2))
                );
            } else {
                first = step > 0 ? start : closed;
                last = step > 0 ? closed : start;
            }
            inverted = backward ^ (step < 0);
        }

    public:
        long long start, stop, step;  // original indices supplied to constructor
        size_t abs_step;
        size_t first, last;  // first and last indices included in slice
        size_t length;  // total number of items
        bool inverted;  // if true, first and last indices contradict step size
        bool backward;  // if true, traverse from tail

    };


    /* Normalize slice indices, applying Python-style wraparound and bounds
    checking. */
    template <typename View>
    SliceIndices<View> normalize_slice(
        const View& view,
        std::optional<long long> start = std::nullopt,
        std::optional<long long> stop = std::nullopt,
        std::optional<long long> step = std::nullopt
    ) {
        // apply defaults
        long long lsize = view.size();
        long long default_start = step.value_or(0) < 0 ? lsize - 1 : 0;
        long long default_stop = step.value_or(0) < 0 ? -1 : lsize;

        // normalize step
        long long nstep = step.value_or(1);
        if (nstep == 0) throw ValueError("slice step cannot be zero");

        // normalize start
        long long nstart = start.value_or(default_start);
        if (nstart < 0) {
            nstart += lsize;
            if (nstart < 0) {
                nstart = nstep < 0 ? -1 : 0;
            }
        } else if (nstart >= lsize) {
            nstart = nstep < 0 ? lsize - 1 : lsize;
        }

        // normalize stop
        long long nstop = stop.value_or(default_stop);
        if (nstop < 0) {
            nstop += lsize;
            if (nstop < 0) {
                nstop = nstep < 0 ? -1 : 0;
            }
        } else if (nstop > lsize) {
            nstop = nstep < 0 ? lsize - 1 : lsize;
        }

        long long length = (nstop - nstart + nstep - (nstep > 0 ? 1 : -1)) / nstep;
        return SliceIndices<View>(nstart, nstop, nstep, length < 0 ? 0 : length, lsize);
    }


    /* Normalize a Python slice object, applying Python-style wraparound and bounds
    checking. */
    template <typename View>
    SliceIndices<View> normalize_slice(const View& view, PyObject* slice) {
        using Indices = std::tuple<long long, long long, long long, size_t>;

        PySlice py_slice(slice);
        Indices indices(py_slice.normalize(view.size()));
        return SliceIndices<View>(
            std::get<0>(indices),
            std::get<1>(indices),
            std::get<2>(indices),
            std::get<3>(indices),
            view.size()
        );
    }


    /////////////////////
    ////    PROXY    ////
    /////////////////////


    /* A proxy for a slice within a list, as returned by the slice() factory method. */
    template <typename View, typename Result>
    class SliceProxy {
        using Node = typename View::Node;

        template <Direction dir>
        using ViewIter = typename View::template Iterator<dir>;

        View& view;
        const SliceIndices<View> indices;
        mutable bool cached;
        mutable Node* _origin;  // node immediately preceding slice (can be null)

        template <typename _View, typename _Result, typename... Args>
        friend auto slice(_View& view, Args&&... args)
            -> std::enable_if_t<ViewTraits<_View>::linked, SliceProxy<_View, _Result>>;

        /* Construct a SliceProxy using the normalized indices. */
        SliceProxy(View& view, SliceIndices<View>&& indices) :
            view(view), indices(indices), cached(false), _origin(nullptr)
        {}

        /* Find the origin node for the slice. */
        Node* origin() const {
            if (cached) {
                return _origin;
            }

            if constexpr (NodeTraits<Node>::has_prev) {
                if (indices.backward) {
                    auto it = view.rbegin();
                    for (size_t i = 1; i < view.size() - indices.first; ++i, ++it);
                    cached = true;
                    _origin = it.next();
                    return _origin;
                }
            }

            auto it = view.begin();
            for (size_t i = 0; i < indices.first; ++i, ++it);
            cached = true;
            _origin = it.prev();
            return _origin;
        }

        /* Simple C array used in set() to hold a temporary buffer of replaced nodes. */
        struct RecoveryArray {
            Node* array;
    
            RecoveryArray(size_t length) {
                array = static_cast<Node*>(malloc(sizeof(Node) * length));
                if (array == nullptr) throw MemoryError();
            }

            Node& operator[](size_t index) {
                return array[index];
            }

            ~RecoveryArray() {
                free(array);  // no need to call destructors
            }

        };

        /* Container-independent implementation for slice().set(). */
        template <typename Container>
        void _set_impl(const Container& items) {
            using MemGuard = typename View::MemGuard;

            // unpack items into indexable sequence with known length
            auto seq = sequence(items);  // possibly a no-op if already a sequence
            if (indices.length != seq.size()) {
                // NOTE: Python allows the slice and sequence lengths to differ if and
                // only if the step size is 1.  This can possibly change the overall
                // length of the list, and makes everything much more complicated.
                if (indices.step != 1) {
                    std::ostringstream msg;
                    msg << "attempt to assign sequence of size " << seq.size();
                    msg << " to extended slice of size " << indices.length;
                    throw ValueError(msg.str());
                }
            } else if (indices.length == 0) {
                return;
            }

            // allocate recovery array and freeze list allocator during iteration
            RecoveryArray recovery(indices.length);
            size_t fsize = view.size() - indices.length + seq.size();
            MemGuard guard = view.reserve(fsize < view.size() ? view.size() : fsize);

            // if doubly-linked, then we can potentially use a reverse iterator
            if constexpr (NodeTraits<Node>::has_prev) {
                if (indices.backward) {
                    auto iter = [&]() -> ViewIter<Direction::backward> {
                        Node* next = origin();
                        Node* curr = (next == nullptr) ? view.tail() : next->prev();
                        Node* prev = (curr == nullptr) ? nullptr : curr->prev();
                        return ViewIter<Direction::backward>(view, prev, curr, next);
                    };

                    // remove current occupants
                    if (indices.length > 0) {
                        size_t idx = 0;
                        auto loop1 = iter();
                        while (true) {
                            Node* node = loop1.drop();
                            new (&recovery[idx]) Node(std::move(*node));
                            view.recycle(node);
                            if (++idx == indices.length) {
                                break;
                            }
                            for (size_t i = 1; i < indices.abs_step; ++i, ++loop1);
                        }
                    }

                    // insert new nodes from sequence
                    if (seq.size() > 0) {
                        size_t idx = 0;
                        try {
                            auto loop2 = iter();
                            while (true) {
                                size_t i = indices.inverted ? seq.size() - idx - 1 : idx;
                                loop2.insert(view.node(seq[i]));
                                if (++idx == seq.size()) {
                                    break;
                                }
                                for (size_t i = 0; i < indices.abs_step; ++i, ++loop2);
                            }
                        } catch (...) {
                            // remove nodes that have already been added
                            if (idx > 0) {
                                size_t i = 0;
                                auto loop3 = iter();
                                while (true) {
                                    view.recycle(loop3.drop());
                                    if (++i == idx) {
                                        break;
                                    }
                                    for (size_t i = 1; i < indices.abs_step; ++i, ++loop3);
                                }
                            }

                            // reinsert originals from recovery array
                            if (indices.length > 0) {
                                size_t i = 0;
                                auto loop4 = iter();
                                while (true) {
                                    loop4.insert(view.node(std::move(recovery[i])));
                                    if (++i == indices.length) {
                                        break;
                                    }
                                    for (size_t i = 0; i < indices.abs_step; ++i, ++loop4);
                                }
                            }
                            throw;
                        }
                    }

                    // deallocate recovery array
                    for (size_t idx = 0; idx < indices.length; ++idx) {
                        recovery[idx].~Node();
                    }
                    return;
                }
            }

            // otherwise we do the same thing, but with a forward iterator
            auto iter = [&]() -> ViewIter<Direction::forward> {
                Node* prev = origin();
                Node* curr = (prev == nullptr) ? view.head() : prev->next();
                Node* next = (curr == nullptr) ? nullptr : curr->next();
                return ViewIter<Direction::forward>(view, prev, curr, next);
            };

            // remove current occupants
            if (indices.length > 0) {
                size_t idx = 0;
                auto loop1 = iter();
                while (true) {
                    Node* node = loop1.drop();
                    new (&recovery[idx]) Node(std::move(*node));
                    view.recycle(node);
                    if (++idx == indices.length) {
                        break;
                    }
                    for (size_t i = 1; i < indices.abs_step; ++i, ++loop1);
                }
            }

            // insert new nodes from sequence
            if (seq.size() > 0) {
                size_t idx = 0;
                try {
                    auto loop2 = iter();
                    while (true) {
                        size_t i = indices.inverted ? seq.size() - idx - 1 : idx;
                        loop2.insert(view.node(seq[i]));
                        if (++idx == seq.size()) {
                            break;
                        }
                        for (size_t i = 0; i < indices.abs_step; ++i, ++loop2);
                    }
                } catch (...) {
                    // remove nodes that have already been added
                    if (idx > 0) {
                        size_t i = 0;
                        auto loop3 = iter();
                        while (true) {
                            view.recycle(loop3.drop());
                            if (++i == idx) {
                                break;
                            }
                            for (size_t i = 1; i < indices.abs_step; ++i, ++loop3);
                        }
                    }

                    // reinsert originals from recovery array
                    if (indices.length > 0) {
                        size_t i = 0;
                        auto loop4 = iter();
                        while (true) {
                            loop4.insert(view.node(std::move(recovery[i])));
                            if (++i == indices.length) {
                                break;
                            }
                            for (size_t i = 0; i < indices.abs_step; ++i, ++loop4);
                        }
                    }
                    throw;
                }
            }

            // deallocate recovery array
            for (size_t idx = 0; idx < indices.length; ++idx) {
                recovery[idx].~Node();
            }
        }

    public:
        /* Disallow SliceProxies from being stored as lvalues. */
        SliceProxy(const SliceProxy&) = delete;
        SliceProxy(SliceProxy&&) = delete;
        SliceProxy& operator=(const SliceProxy&) = delete;
        SliceProxy& operator=(SliceProxy&&) = delete;

        /////////////////////////
        ////    ITERATORS    ////
        /////////////////////////

        /* A specialized iterator that directly traverses over a slice without any
        copies.  This automatically corrects for inverted traversal and always yields
        items in the same order as the step size (reversed if called from rbegin). */
        template <Direction dir>
        class Iterator {
            using Node = typename View::Node;
            using Value = typename View::Value;

            /* NOTE: this iterator is tricky.  It is essentially a wrapper around a
             * standard view iterator, but it must also handle inverted traversal and
             * negative step sizes.  This means that the direction of the view iterator
             * might not match the direction of the slice iterator, depending on the
             * singly-/doubly-linked status of the list and the input to the slice()
             * factory method.
             *
             * To handle this, we use a type-erased union of view iterators and a stack
             * of nodes that can be used to cancel out inverted traversal.  The stack
             * is only populated if necessary, and also takes into account whether the
             * iterator is generated from the begin()/end() or rbegin()/rend() methods.
             *
             * These iterators are only meant for direct iteration (no copying) over
             * the slice.  The get(), set() and del() methods use the view iterators
             * instead, and have more efficient methods of handling inverted traversal
             * that don't require any backtracking or auxiliary data structures.
             */

            union {
                ViewIter<Direction::forward> fwd;
                ViewIter<Direction::backward> bwd;
            };

            std::stack<Node*> stack;
            const View& view;
            const SliceIndices<View> indices;
            size_t idx;

            friend SliceProxy;

            /* Get an iterator to the start of a non-empty slice. */
            Iterator(View& view, const SliceIndices<View>& indices, Node* origin) :
                view(view), indices(indices), idx(0)
            {
                if (!indices.backward) {
                    Node* prev = origin;
                    Node* curr = (prev == nullptr) ? view.head() : prev->next();
                    Node* next = (curr == nullptr) ? nullptr : curr->next();
                    new (&fwd) ViewIter<Direction::forward>(view, prev, curr, next);

                    // use stack to cancel out inverted traversal
                    if (indices.inverted ^ (dir == Direction::backward)) {
                        while (true) {
                            stack.push(fwd.curr());
                            if (++idx >= indices.length) {
                                break;
                            }
                            for (size_t i = 0; i < indices.abs_step; ++i, ++fwd);
                        }
                        idx = 0;
                    }

                } else {
                    if constexpr (NodeTraits<Node>::has_prev) {
                        Node* next = origin;
                        Node* curr = (next == nullptr) ? view.tail() : next->prev();
                        Node* prev = (curr == nullptr) ? nullptr : curr->prev();
                        new (&bwd) ViewIter<Direction::backward>(view, prev, curr, next);

                        // use stack to cancel out inverted traversal
                        if (indices.inverted ^ (dir == Direction::backward)) {
                            while (true) {
                                stack.push(bwd.curr());
                                if (++idx >= indices.length) {
                                    break;
                                }
                                for (size_t i = 0; i < indices.abs_step; ++i, ++bwd);
                            }
                            idx = 0;
                        }

                    // unreachable: indices.backward is always false if singly-linked
                    } else {
                        throw ValueError(
                            "backwards traversal is not supported for "
                            "singly-linked lists"
                        );
                    }
                }
            }

            /* Get an iterator to terminate a slice. */
            Iterator(View& view, const SliceIndices<View>& indices) :
                view(view), indices(indices), idx(indices.length)
            {}

        public:
            using iterator_tag          = std::forward_iterator_tag;
            using difference_type       = std::ptrdiff_t;
            using value_type            = Value;
            using pointer               = Value*;
            using reference             = Value&;

            /* Copy constructor. */
            Iterator(const Iterator& other) noexcept :
                stack(other.stack), view(other.view), indices(other.indices),
                idx(other.idx)
            {
                if (indices.backward) {
                    new (&bwd) ViewIter<Direction::backward>(other.bwd);
                } else {
                    new (&fwd) ViewIter<Direction::forward>(other.fwd);
                }
            }

            /* Move constructor. */
            Iterator(Iterator&& other) noexcept :
                stack(std::move(other.stack)), view(other.view),
                indices(other.indices), idx(other.idx)
            {
                if (indices.backward) {
                    new (&bwd) ViewIter<Direction::backward>(std::move(other.bwd));
                } else {
                    new (&fwd) ViewIter<Direction::forward>(std::move(other.fwd));
                }
            }

            /* Clean up union iterator on destruction. */
            ~Iterator() {
                if (indices.backward) {
                    bwd.~ViewIter<Direction::backward>();
                } else {
                    fwd.~ViewIter<Direction::forward>();
                }
            }

            /* Dereference the iterator to get the value at the current index. */
            inline Value operator*() const {
                if (!stack.empty()) {
                    return stack.top()->value();
                } else {
                    return indices.backward ? *bwd : *fwd;
                }
            }

            /* Advance the iterator to the next element in the slice. */
            inline Iterator& operator++() noexcept {
                ++idx;
                if (!stack.empty()) {
                    stack.pop();
                } else if (idx < indices.length) {  // don't advance on final iteration
                    if (indices.backward) {
                        for (size_t i = 0; i < indices.abs_step; ++i, ++bwd);
                    } else {
                        for (size_t i = 0; i < indices.abs_step; ++i, ++fwd);
                    }
                }
                return *this;
            }

            /* Compare iterators to terminate the slice. */
            inline bool operator!=(const Iterator& other) const noexcept {
                return idx != other.idx;
            }

        };

        /* Return an iterator to the start of the slice. */
        inline Iterator<Direction::forward> begin() const {
            if (indices.length == 0) {
                return end();
            }
            return Iterator<Direction::forward>(view, indices, origin());
        }

        /* Return an explicitly const iterator to the start of the slice. */
        inline Iterator<Direction::forward> cbegin() const {
            return begin();
        }

        /* Return an iterator to terminate the slice. */
        inline Iterator<Direction::forward> end() const {
            return Iterator<Direction::forward>(view, indices);
        }

        /* Return an explicitly const iterator to terminate the slice. */
        inline Iterator<Direction::forward> cend() const {
            return end();
        }

        /* Return a reverse iterator over the slice. */
        inline Iterator<Direction::backward> rbegin() const {
            if (indices.length == 0) {
                return rend();
            }
            return Iterator<Direction::backward>(view, indices, origin());
        }

        /* Return an explicitly const reverse iterator over the slice. */
        inline Iterator<Direction::backward> crbegin() const {
            return rbegin();
        }

        /* Return a reverse iterator to terminate the slice. */
        inline Iterator<Direction::backward> rend() const {
            return Iterator<Direction::backward>(view, indices);
        }

        /* Return an explicitly const reverse iterator to terminate the slice. */
        inline Iterator<Direction::backward> crend() const {
            return rend();
        }

        //////////////////////
        ////    PUBLIC    ////
        //////////////////////

        /* Extract a slice from a linked list. */
        Result get() const {
            // preallocate to exact size
            View result(indices.length, view.specialization());
            if (indices.length == 0) {
                if constexpr (std::is_same_v<View, Result>) {
                    return result;
                } else {
                    return Result(std::move(result));
                }
            }

            // if doubly-linked, then we can potentially use a reverse iterator
            if constexpr (NodeTraits<Node>::has_prev) {
                if (indices.backward) {
                    size_t idx = 0;
                    Node* next = origin();
                    Node* curr = (next == nullptr) ? view.tail() : next->prev();
                    Node* prev = (curr == nullptr) ? nullptr : curr->prev();
                    ViewIter<Direction::backward> it(view, prev, curr, next);
                    if (indices.inverted) {
                        while (true) {
                            Node* copy = result.node(*(it.curr()));
                            result.link(nullptr, copy, result.head());  // prepend
                            if (++idx == indices.length) {
                                break;
                            }
                            for (size_t i = 0; i < indices.abs_step; ++i, ++it);
                        }
                    } else {
                        while (true) {
                            Node* copy = result.node(*(it.curr()));
                            result.link(result.tail(), copy, nullptr);  // append
                            if (++idx == indices.length) {
                                break;
                            }
                            for (size_t i = 0; i < indices.abs_step; ++i, ++it);
                        }
                    }

                    // possibly wrap in higher-level container
                    if constexpr (std::is_same_v<View, Result>) {
                        return result;
                    } else {
                        return Result(std::move(result));
                    }
                }
            }

            // otherwise, we use a forward iterator
            size_t idx = 0;
            Node* prev = origin();
            Node* curr = (prev == nullptr) ? view.head() : prev->next();
            Node* next = (curr == nullptr) ? nullptr : curr->next();
            ViewIter<Direction::forward> it(view, prev, curr, next);
            if (indices.inverted) {
                while (true) {
                    Node* copy = result.node(*(it.curr()));
                    result.link(nullptr, copy, result.head());  // prepend
                    if (++idx == indices.length) {
                        break;
                    }
                    for (size_t i = 0; i < indices.abs_step; ++i, ++it);
                }
            } else {
                while (true) {
                    Node* copy = result.node(*(it.curr()));
                    result.link(result.tail(), copy, nullptr);  // append
                    if (++idx == indices.length) {
                        break;
                    }
                    for (size_t i = 0; i < indices.abs_step; ++i, ++it);
                }
            }

            // possibly wrap in higher-level container
            if constexpr (std::is_same_v<View, Result>) {
                return result;
            } else {
                return Result(std::move(result));
            }
        }

        /* Replace a slice within a linked list. */
        template <typename Container>
        inline void set(const Container& items) {
            _set_impl(items);
        }

        /* A special case of slice().set() on dictlike views that accounts for both
        keys and values of Python dictionary inputs. */
        template <bool cond = ViewTraits<View>::dictlike>
        inline auto set(const PyObject* items)-> std::enable_if_t<cond, void> {
            // wrap Python dictionaries to yield key-value pairs during iteration
            if (PyDict_Check(items)) {
                PyDict dict(items);
                _set_impl(dict);
            } else {
                _set_impl(items);
            }
        }

        /* Delete a slice within a linked list. */
        void del() {
            if (indices.length > 0) {
                typename View::MemGuard guard = view.reserve();

                // if doubly-linked, then we can potentially use a reverse iterator
                if constexpr (NodeTraits<Node>::has_prev) {
                    if (indices.backward) {
                        size_t idx = 0;
                        Node* next = origin();
                        Node* curr = (next == nullptr) ? view.tail() : next->prev();
                        Node* prev = (curr == nullptr) ? nullptr : curr->prev();
                        ViewIter<Direction::backward> it(view, prev, curr, next);
                        while (true) {
                            view.recycle(it.drop());
                            if (++idx == indices.length) {
                                break;
                            }
                            for (size_t i = 1; i < indices.abs_step; ++i, ++it);
                        }
                        return;
                    }
                }

                // otherwise, we use a forward iterator
                size_t idx = 0;
                Node* prev = origin();
                Node* curr = (prev == nullptr) ? view.head() : prev->next();
                Node* next = (curr == nullptr) ? nullptr : curr->next();
                ViewIter<Direction::forward> it(view, prev, curr, next);
                while (true) {
                    view.recycle(it.drop());
                    if (++idx == indices.length) {
                        break;
                    }
                    for (size_t i = 1; i < indices.abs_step; ++i, ++it);
                }
            }
        }

        /* Implicitly convert the proxy into a result where applicable.  This is
        syntactic sugar for get(), such that `LinkedList<T> list = list.slice(i, j, k)`
        is equivalent to `LinkedList<T> list = list.slice(i, j, k).get()`. */
        inline operator Result() const {
            return get();
        }

        /* Assign the slice in-place.  This is syntactic sugar for set(), such that
        `list.slice(i, j, k) = items` is equivalent to
        `list.slice(i, j, k).set(items)`. */
        template <typename Container>
        inline SliceProxy& operator=(const Container& items) {
            set(items);
            return *this;
        }

    };


    /* Get a proxy for a slice within the list. */
    template <typename View, typename Result = View, typename... Args>
    auto slice(View& view, Args&&... args)
        -> std::enable_if_t<ViewTraits<View>::linked, SliceProxy<View, Result>>
    {
        return SliceProxy<View, Result>(
            view, normalize_slice(view, std::forward<Args>(args)...))
        ;
    }


    /* Get a const proxy for a slice within a const list. */
    template <typename View, typename Result = View, typename... Args>
    auto slice(const View& view, Args&&... args)
        -> std::enable_if_t<ViewTraits<View>::linked, const SliceProxy<View, Result>>
    {
        return SliceProxy<View, Result>(
            view, normalize_slice(view, std::forward<Args>(args)...)
        );
    }


}  // namespace linked
}  // namespace structs
}  // namespace bertrand


#endif // BERTRAND_STRUCTS_LINKED_ALGORITHMS_SLICE_H
