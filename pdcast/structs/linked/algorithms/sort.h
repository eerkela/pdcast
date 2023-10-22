// include guard prevents multiple inclusion
#ifndef BERTRAND_STRUCTS_LINKED_ALGORITHMS_SORT_H
#define BERTRAND_STRUCTS_LINKED_ALGORITHMS_SORT_H

#include <cstddef>  // size_t
#include <iostream>  // std::cout, std::endl
#include <queue>  // std::queue
#include <optional>  // std::optional
#include <utility>  // std::pair
#include <Python.h>  // CPython API
#include "../node.h"  // Keyed<>
#include "../view.h"  // views


// TODO: we also need to figure out how to transfer ownership temporarily to a list
// to apply the sorting algorithm.


namespace bertrand {
namespace structs {
namespace linked {
namespace algorithms {


namespace list {

//////////////////////
////    PUBLIC    ////
//////////////////////


/* A wrapper around a SortPolicy that handles casting to ListView and
decorating/undecorating according to key functions.  All the SortPolicy has to
implement is the actual sorting algorithm itself. */
template <typename SortPolicy, typename Func>
class SortFunc {
private:
    template <typename Node>
    using Decorated = Keyed<Node, Func>;

    /* Apply a key function to a list, decorating it with the computed result. */
    template <typename Node>
    inline static ListView<Decorated<Node>> decorate(ListView<Node>& view, Func func) {
        // create temporary ListView to hold the keyed list (preallocated to exact size)
        ListView<Decorated<Node>> decorated(view.size(), nullptr);

        // decorate each node in original list
        for (Node* node : view) {
            Decorated<Node>* keyed = decorated.node(node, func);
            decorated.link(decorated.tail(), keyed, nullptr);
        }
        return decorated;
    }

    /* Rearrange the underlying list in-place to reflect changes from a keyed sort. */
    template <typename Node>
    static void undecorate(ListView<Decorated<Node>>& decorated, ListView<Node>& view) {
        Node* new_head = nullptr;
        Node* new_tail = nullptr;

        // NOTE: we recycle the decorators as we go in order to avoid a second loop
        auto iter = decorated.iter();
        while (iter != iter.end()) {
            Node* unwrapped = (*iter)->node();

            // link to sorted list
            if (new_head == nullptr) {
                new_head = unwrapped;
            } else {
                Node::link(new_tail, unwrapped, nullptr);
            }
            new_tail = unwrapped;

            // remove and recycle wrapper
            decorated.recycle(iter.remove());  // NOTE: implicitly advances iter
        }

        // update head/tail of sorted list
        view.head(new_head);
        view.tail(new_tail);
    }

    /* Execute the sorting algorithm. */
    template <typename Node>
    static void execute(ListView<Node>& view, PyObject* key, bool reverse) {
        // if no key function is given, sort the list in-place
        if (key == nullptr) {
            SortPolicy::sort(view, reverse);
            return;
        }

        // apply key function to each node in list
        ListView<Decorated<Node>> decorated = decorate(view, key);

        // sort decorated list
        SortPolicy::sort(decorated, reverse);
        if (PyErr_Occurred()) {
            return;  // propagate without modifying original list
        }

        // rearrange the original list to reflect the sorted order
        undecorate(decorated, view);
    }

public:

    /* Invoke the functor, decorating and sorting the view in-place. */
    template <template <typename> class ViewType, typename Node>
    static void sort(ViewType<Node>& view, PyObject* key, bool reverse) {
        using View = ViewType<Node>;
        using List = ListView<Node>;

        // trivial case: empty view
        if (view.size() == 0) {
            return;
        }

        // if the view is already a ListView, then we can sort it directly
        if constexpr (std::is_same_v<View, List>) {
            execute(view, key, reverse);
            return;
        }

        // TODO: we might just move the view into a ListView, but that would
        // require a converting move constructor, which is a bit of a pain.  It might
        // be what we need to do, though.
        // TODO: we could also template the ListView on the allocator, and then offer
        // a move constructor that takes a naked allocator.  That would allow us to
        // view the list without copying it, transferring ownership from one view to
        // another.
        // TODO: Alternatively, we could build really robust copy/move semantics
        // into the View constructor itself.  This would allow us to keep the
        // the allocators internal to the view.  We would just assume that they have
        // the correct structure.  This could possibly be accomplished via
        // static_assert statements in the templated constructors.

        // TODO: we should probably use both approaches.  We should template the
        // ListView on the allocator, but default to ListAllocator.  Then we can
        // provide a copy/move constructor that constructs a ListView around another
        // view's allocator.  This is not safe, but if used correctly, it should allow
        // us to sort a view without copying it.  As long as the methods that are used
        // are valid for the new allocator, everything will compile just fine.


        // otherwise, we create a temporary ListView into the view and sort that
        // instead.
        List list_view(view.size());  // preallocated to current size
        list_view.head(view.head());
        list_view.tail(view.tail());
        // list_view.size = view.size();  // TODO: size is now private to allocator

        // sort the viewed list in place
        execute(list_view, key, reverse);

        // free the temporary ListView
        list_view.head(nullptr);  // avoids calling destructor on nodes
        list_view.tail(nullptr);
        // list_view.size = 0;
    }

};


////////////////////////
////    POLICIES    ////
////////////////////////


// TODO: allocate temporary node on the view itself.


/* An iterative merge sort algorithm with error recovery. */
class MergeSort {
protected:

    /* Walk along the list by the specified number of nodes. */
    template <typename Node>
    inline static Node* walk(Node* curr, size_t length) {
        // if we're at the end of the list, there's nothing left to traverse
        if (curr == nullptr) {
            return nullptr;
        }

        // walk forward `length` nodes from `curr`
        for (size_t i = 0; i < length; i++) {
            if (curr->next() == nullptr) {  // list terminates before `length`
                break;
            }
            curr = curr->next();
        }
        return curr;
    }

    /* Merge two sublists in sorted order. */
    template <typename Node>
    static std::pair<Node*, Node*> merge(
        std::pair<Node*, Node*> left,
        std::pair<Node*, Node*> right,
        Node* temp,
        bool reverse
    ) {
        Node* curr = temp;  // temporary head of merged list

        // NOTE: the way we merge sublists is by comparing the head of each sublist
        // and appending the smaller of the two elements to the merged result.  We
        // repeat this process until one of the sublists has been exhausted, giving us
        // a sorted list of size `length * 2`.
        while (left.first != nullptr && right.first != nullptr) {
            bool comp = left.first->lt(right.first->value());  // left < right

            // append the smaller of the two candidates to the merged list
            if (reverse ^ comp) {  // [not] left < right
                Node::join(curr, left.first);
                left.first = left.first->next();
            } else {
                Node::join(curr, right.first);
                right.first = right.first->next();
            }
            curr = curr->next();
        }

        // NOTE: at this point, one of the sublists has been exhausted, so we can
        // safely append the remaining nodes to the merged result.
        Node* tail;
        if (left.first != nullptr) {
            Node::join(curr, left.first);
            tail = left.second;
        } else {
            Node::join(curr, right.first);
            tail = right.second;
        }

        // unlink temporary head from list and return the proper head and tail
        curr = temp->next();
        Node::split(temp, curr);  // `temp` can be reused
        return std::make_pair(curr, tail);
    }

    /* Undo the split() step to recover a valid list in case of an error. */
    template <typename Node>
    inline static std::pair<Node*, Node*> recover(
        std::pair<Node*, Node*> sorted,
        std::pair<Node*, Node*> left,
        std::pair<Node*, Node*> right,
        std::pair<Node*, Node*> unsorted
    ) {
        // link each sublist into a single, partially-sorted list
        Node::join(sorted.second, left.first);  // sorted tail <-> left head
        Node::join(left.second, right.first);  // left tail <-> right head
        Node::join(right.second, unsorted.first);  // right tail <-> unsorted head

        // return the head and tail of the recovered list
        return std::make_pair(sorted.first, unsorted.second);
    }

public:

    /* Sort a linked list in-place using an iterative merge sort algorithm. */
    template <typename Node>
    static void sort(ListView<Node>& view, bool reverse) {
        // NOTE: we need a temporary node to act as the head of the merged sublists.
        // If we allocate it here, we can pass it to `merge()` as an argument and
        // reuse it for every sublist.  This avoids an extra malloc/free cycle in
        // each iteration.
        if constexpr (DEBUG) {
            std::cout << "    -> malloc: temp node" << std::endl;
        }
        Node* temp = static_cast<Node*>(malloc(sizeof(Node)));
        if (temp == nullptr) {
            PyErr_NoMemory();
            return;
        }

        // NOTE: we use a series of pairs to keep track of the head and tail of
        // each sublist used in the sort algorithm.  `unsorted` keeps track of the
        // nodes that still need to be processed, while `sorted` does the same for
        // those that have already been sorted.  The `left`, `right`, and `merged`
        // pairs are used to keep track of the sublists that are used in each
        // iteration of the merge loop.
        std::pair<Node*, Node*> unsorted = std::make_pair(view.head(), view.tail());
        std::pair<Node*, Node*> sorted = std::make_pair(nullptr, nullptr);
        std::pair<Node*, Node*> left = std::make_pair(nullptr, nullptr);
        std::pair<Node*, Node*> right = std::make_pair(nullptr, nullptr);
        std::pair<Node*, Node*> merged;

        // NOTE: as a refresher, the general merge sort algorithm is as follows:
        //  1) divide the list into sublists of length 1 (bottom-up)
        //  2) merge adjacent sublists into sorted mixtures with twice the length
        //  3) repeat step 2 until the entire list is sorted
        size_t length = 1;  // length of sublists for current iteration
        while (length <= view.size()) {
            // reset head and tail of sorted list
            sorted.first = nullptr;
            sorted.second = nullptr;

            // divide and conquer
            while (unsorted.first != nullptr) {
                // split the list into two sublists of size `length`
                left.first = unsorted.first;
                left.second = walk(left.first, length - 1);
                right.first = left.second->next();
                right.second = walk(right.first, length - 1);
                if (right.second == nullptr) {  // right sublist is empty
                    unsorted.first = nullptr;  // terminate the loop
                } else {
                    unsorted.first = right.second->next();
                }

                // unlink the sublists from the original list
                Node::split(sorted.second, left.first);  // sorted <-/-> left
                Node::split(left.second, right.first);  // left <-/-> right
                Node::split(right.second, unsorted.first);  // right <-/-> unsorted

                // merge the left and right sublists in sorted order
                try {
                    merged = merge(left, right, temp, reverse);
                } catch (...) {
                    // undo the splits to recover a coherent list
                    merged = recover(sorted, left, right, unsorted);
                    view.head(merged.first);  // view is partially sorted, but valid
                    view.tail(merged.second);
                    if constexpr (DEBUG) {
                        std::cout << "    -> free: temp node" << std::endl;
                    }
                    free(temp);  // clean up temporary node
                    throw;  // propagate the error
                }

                // link merged sublist to sorted
                if (sorted.first == nullptr) {
                    sorted.first = merged.first;
                } else {
                    Node::join(sorted.second, merged.first);
                }
                sorted.second = merged.second;  // update tail of sorted list
            }

            // partially-sorted list becomes new unsorted list for next iteration
            unsorted.first = sorted.first;
            unsorted.second = sorted.second;
            length *= 2;  // double the length of each sublist
        }

        // clean up temporary node
        if constexpr (DEBUG) {
            std::cout << "    -> free: temp node" << std::endl;
        }
        free(temp);

        // update view parameters in-place
        view.head(sorted.first);
        view.tail(sorted.second);
    }

};


}  // namespace list


}  // namespace algorithms
}  // namespace linked
}  // namespace structs
}  // namespace bertrand


#endif  // BERTRAND_STRUCTS_CORE_SORT_H