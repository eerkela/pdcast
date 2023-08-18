
// include guard prevents multiple inclusion
#ifndef BERTRAND_STRUCTS_SET_H
#define BERTRAND_STRUCTS_SET_H

#include <cstddef>  // for size_t
#include <utility>  // std::pair
#include <variant>  // std::variant
#include <Python.h>  // CPython API

// Algorithms
#include "algorithms/append.h"
#include "algorithms/compare.h"
#include "algorithms/contains.h"
#include "algorithms/count.h"
#include "algorithms/delete_slice.h"
#include "algorithms/extend.h"
#include "algorithms/get_slice.h"
#include "algorithms/index.h"
#include "algorithms/insert.h"
#include "algorithms/move.h"
#include "algorithms/pop.h"
#include "algorithms/remove.h"
#include "algorithms/reverse.h"
#include "algorithms/rotate.h"
#include "algorithms/set_slice.h"
#include "algorithms/sort.h"

// Core
#include "core/allocate.h"  // Allocator policies
#include "core/bounds.h"  // normalize_index(), normalize_bounds(), etc.
#include "core/node.h"  // Nodes
#include "core/view.h"  // Views

// List
#include "list.h"  // VariantList


//////////////////////
////    PUBLIC    ////
//////////////////////


class VariantSet : public VariantList {
public:
    using Base = VariantList;

    /* Construct a new VariantSet from an existing SetView.  This is called to
    construct a new `VariantSet` from the output of `SetView.copy()` or
    `get_slice()`. */
    template <
        template <typename, template <typename> class> class ViewType,
        typename NodeType,
        template <typename> class Allocator
    >
    VariantSet(ViewType<NodeType, Allocator>&& view) : Base(view) {}

    /* Construct an empty SetView to match the given template parameters.  This
    is called during `LinkedSet.__init__()` when no iterable is given. */
    VariantSet(bool doubly_linked, ssize_t max_size) {
        if (doubly_linked) {
            if (max_size < 0) {
                this->variant = SetView<DoubleNode, FreeListAllocator>(max_size);
            } else {
                this->variant = SetView<DoubleNode, PreAllocator>(max_size);
            }
        } else {
            if (max_size < 0) {
                this->variant = SetView<SingleNode, FreeListAllocator>(max_size);
            } else {
                this->variant = SetView<SingleNode, PreAllocator>(max_size);
            }
        }
        this->_doubly_linked = doubly_linked;
    }

    /* Unpack an iterable into a new SetView and wrap it as a VariantSet.  This
    is called to construct a LinkedSet from an initializer sequence. */
    VariantSet(
        PyObject* iterable,
        bool doubly_linked,
        bool reverse,
        ssize_t max_size,
        PyObject* spec
    ) {
        if (doubly_linked) {
            if (max_size < 0) {
                this->variant = SetView<DoubleNode, FreeListAllocator>(
                    iterable, reverse, max_size, spec
                );
            } else {
                this->variant = SetView<DoubleNode, PreAllocator>(
                    iterable, reverse, max_size, spec
                );
            }
        } else {
            if (max_size < 0) {
                this->variant = SetView<SingleNode, FreeListAllocator>(
                    iterable, reverse, max_size, spec
                );
            } else {
                this->variant = SetView<SingleNode, PreAllocator>(
                    iterable, reverse, max_size, spec
                );
            }
        }
        this->_doubly_linked = doubly_linked;
    }

    /////////////////////////////
    ////    SET INTERFACE    ////
    /////////////////////////////

    /* Dispatch to the correct implementation of add() for each variant. */
    inline void add(PyObject* item, bool left) {
        std::visit(
            [&](auto& view) {
                Ops::append(&view, item, left);
            },
            this->variant
        );
    }

    /* Dispatch to the correct implementation of discard() for each variant. */
    inline void discard(PyObject* item) {
        std::visit(
            [&](auto& view) {
                Ops::discard(&view, item);
            },
            this->variant
        );
    }


    // TODO: intersect, difference, issubset, issuperset, isdisjoint, etc.


    ///////////////////////////////////
    ////    RELATIVE OPERATIONS    ////
    ///////////////////////////////////

    /* Dispatch to the correct implementation of get_relative() for each variant. */
    inline PyObject* get_relative(PyObject* sentinel, Py_ssize_t offset) {
        PyObject* item = nullptr;
        std::visit(
            [&](auto& view) {
                return Ops::get_relative(&view, sentinel, offset);
            },
            this->variant
        );
        return item;
    }

    /* Dispatch to the correct implementation of insert_relative() for each variant. */
    inline void insert_relative(PyObject* item, PyObject* sentinel, Py_ssize_t offset) {
        std::visit(
            [&](auto& view) {
                Ops::insert_relative(&view, item, sentinel, offset);
            },
            this->variant
        );
    }

    /* Dispatch to the correct implementation of extend_relative() for each variant. */
    inline void extend_relative(
        PyObject* items,
        PyObject* sentinel,
        Py_ssize_t offset,
        bool reverse
    ) {
        std::visit(
            [&](auto& view) {
                Ops::extend_relative(&view, items, sentinel, offset, reverse);
            },
            this->variant
        );
    }

    /* Dispatch to the correct implementation of remove_relative() for each variant. */
    inline void remove_relative(PyObject* sentinel, Py_ssize_t offset) {
        std::visit(
            [&](auto& view) {
                Ops::remove_relative(&view, sentinel, offset);
            },
            this->variant
        );
    }

    /* Dispatch to the correct implementation of discard_relative() for each variant. */
    inline void discard_relative(PyObject* sentinel, Py_ssize_t offset) {
        std::visit(
            [&](auto& view) {
                Ops::discard_relative(&view, sentinel, offset);
            },
            this->variant
        );
    }

    /* Dispatch to the correct implementation of pop_relative() for each variant. */
    inline PyObject* pop_relative(PyObject* sentinel, Py_ssize_t offset) {
        PyObject* item = nullptr;
        std::visit(
            [&](auto& view) {
                item = Ops::pop_relative(&view, sentinel, offset);
            },
            this->variant
        );
        return item;
    }

    /* Dispatch to the correct implementation of clear_relative() for each variant. */
    inline void clear_relative(
        PyObject* sentinel,
        Py_ssize_t offset,
        Py_ssize_t length
    ) {
        std::visit(
            [&](auto& view) {
                Ops::clear_relative(&view, sentinel, offset, length);
            },
            this->variant
        );
    }

    ///////////////////////////////
    ////    MOVE OPERATIONS    ////
    ///////////////////////////////

    /* Dispatch to the correct implementation of edge() for each variant. */
    inline void distance(PyObject* item1, PyObject* item2) {
        std::visit(
            [&](auto& view) {
                Ops::edge(&view, item1, item2);
            },
            this->variant
        );
    }

    /* Dispatch to the correct implementation of swap() for each variant. */
    inline void swap(PyObject* item1, PyObject* item2) {
        std::visit(
            [&](auto& view) {
                Ops::swap(&view, item1, item2);
            },
            this->variant
        );
    }

    /* Dispatch to the correct implementation of move() for each variant. */
    inline void move(PyObject* item, Py_ssize_t steps) {
        std::visit(
            [&](auto& view) {
                Ops::move(&view, item, offset);
            },
            this->variant
        );
    }

    /* Dispatch to the correct implementation of move_to_index() for each variant. */
    template <typename T>
    inline void move_to_index(PyObject* item, T index) {
        std::visit(
            [&](auto& view) {
                // allow Python-style negative indexing + boundschecking
                size_t norm_index = normalize_index(index, view.size, true);
                Ops::move(&view, item, norm_index);
            },
            this->variant
        );
    }

    /* Dispatch to the correct implementation of move_relative() for each variant. */
    inline void move_relative(PyObject* item, PyObject* sentinel, Py_ssize_t offset) {
        std::visit(
            [&](auto& view) {
                Ops::move_relative(&view, item, sentinel, offset);
            },
            this->variant
        );
    }

};


#endif  // BERTRAND_STRUCTS_SET_H include guard