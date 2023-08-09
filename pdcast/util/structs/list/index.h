
// include guard prevents multiple inclusion
#ifndef INDEX_H
#define INDEX_H

#include <cstddef>  // for size_t
#include <Python.h>  // for CPython API
#include <node.h>  // for nodes
#include <view.h>  // for views


//////////////////////
////    PUBLIC    ////
//////////////////////


/* Get the index of an item within a singly-linked set or dictionary. */
template <template <typename> class ViewType, typename NodeType>
inline size_t index_single(
    ViewType<NodeType>* view,
    PyObject* item,
    size_t start,
    size_t stop
) {
    return _index_setlike(view, item, start, stop);
}


/* Get the index of an item within a singly-linked list. */
template <typename NodeType>
inline size_t index_single(
    ListView<NodeType>* view,
    PyObject* item,
    size_t start,
    size_t stop
) {
    using Node = typename ListView<NodeType>::Node;

    // skip to start index
    Node* curr = view->head;
    size_t idx;
    for (idx = 0; idx < start; idx++) {
        curr = (Node*)curr->next;
    }

    // search until we hit stop index
    while (idx < stop) {
        // C API equivalent of the == operator
        int comp = PyObject_RichCompareBool(curr->value, item, Py_EQ);
        if (comp == -1) {  // `==` raised an exception
            return MAX_SIZE_T;
        } else if (comp == 1) {  // found a match
            return idx;
        }

        // advance to next node
        curr = (Node*)curr->next;
        idx++;
    }

    // item not found
    PyErr_Format(PyExc_ValueError, "%R is not in list", item);
    return MAX_SIZE_T;
}


/* Get the index of an item within a doubly-linked set or dictionary. */
template <template <typename> class ViewType, typename NodeType>
inline size_t index_double(
    ViewType<NodeType>* view,
    PyObject* item,
    size_t start,
    size_t stop
) {
    return _index_setlike(view, item, start, stop);
}


/* Get the index of an item within a doubly-linked list. */
template <typename NodeType>
inline size_t index_double(
    ListView<NodeType>* view,
    PyObject* item,
    size_t start,
    size_t stop
) {
    // if starting index is closer to head, use singly-linked version
    if (start <= view->size / 2) {
        return index_single(view, item, start, stop);
    }

    using Node = typename ListView<NodeType>::Node;

    // else, start from tail
    Node* curr = view->tail;
    size_t idx;
    for (idx = view->size - 1; idx > stop; idx--) {  // skip to stop index
        curr = (Node*)curr->prev;
    }

    // search until we hit start index
    bool found = false;
    size_t last_observed;
    while (idx >= start) {
        // C API equivalent of the == operator
        int comp = PyObject_RichCompareBool(curr->value, item, Py_EQ);
        if (comp == -1) {  // comparison raised an exception
            return MAX_SIZE_T;
        } else if (comp == 1) {  // found a match
            last_observed = idx;
            found = true;
        }

        // advance to next node
        curr = (Node*)curr->prev;
        idx--;
    }

    // return first occurrence in range
    if (found) {
        return last_observed;
    }

    // item not found
    PyErr_Format(PyExc_ValueError, "%R is not in list", item);
    return MAX_SIZE_T;
}



///////////////////////
////    PRIVATE    ////
///////////////////////


/* Get the index of an item within a set-like list. */
template <template <typename> class ViewType, typename NodeType>
inline size_t _index_setlike(
    ViewType<NodeType>* view,
    PyObject* item,
    size_t start,
    size_t stop
) {
    using Node = typename ViewType<NodeType>::Node;

    // search for item in hash table
    Node* node = view->search(item);
    if (node == NULL) {
        PyErr_Format(PyExc_ValueError, "%R is not in the set", item);
        return MAX_SIZE_T;
    }

    // skip to start index
    Node* curr = view->head;
    size_t idx;
    for (idx = 0; idx < start; idx++) {
        if (curr == node) {  // item exists, but comes before range
            PyErr_Format(PyExc_ValueError, "%R is not in the set", item);
            return MAX_SIZE_T;
        }
        curr = (Node*)curr->next;
    }

    // iterate until we hit match or stop index
    while (curr != node && idx < stop) {
        curr = (Node*)curr->next;
        idx++;
    }
    if (curr == node) {
        return idx;
    }

    // item exists, but comes after range
    PyErr_Format(PyExc_ValueError, "%R is not in the set", item);
    return MAX_SIZE_T;
}


////////////////////////
////    WRAPPERS    ////
////////////////////////


// NOTE: Cython doesn't play well with nested templates, so we need to
// explicitly instantiate specializations for each combination of node/view
// type.  This is a bit of a pain, put it's the only way to get Cython to
// properly recognize the functions.

// Maybe in a future release we won't have to do this:


template size_t index_single(
    ListView<SingleNode>* view,
    PyObject* item,
    size_t start,
    size_t stop
);
template size_t index_single(
    SetView<SingleNode>* view,
    PyObject* item,
    size_t start,
    size_t stop
);
template size_t index_single(
    DictView<SingleNode>* view,
    PyObject* item,
    size_t start,
    size_t stop
);
template size_t index_single(
    ListView<DoubleNode>* view,
    PyObject* item,
    size_t start,
    size_t stop
);
template size_t index_single(
    SetView<DoubleNode>* view,
    PyObject* item,
    size_t start,
    size_t stop
);
template size_t index_single(
    DictView<DoubleNode>* view,
    PyObject* item,
    size_t start,
    size_t stop
);
template size_t index_double(
    ListView<DoubleNode>* view,
    PyObject* item,
    size_t start,
    size_t stop
);
template size_t index_double(
    SetView<DoubleNode>* view,
    PyObject* item,
    size_t start,
    size_t stop
);
template size_t index_double(
    DictView<DoubleNode>* view,
    PyObject* item,
    size_t start,
    size_t stop
);


#endif // INDEX_H include guard
