
// include guard prevents multiple inclusion
#ifndef EXTEND_H
#define EXTEND_H

#include <cstddef>  // for size_t
#include <Python.h>  // for CPython API
#include <node.h>  // for nodes
#include <view.h>  // for views


// TODO: append() for sets and dicts should mimic set.update() and dict.update(),
// respectively.  If the item is already contained in the set or dict, then
// we just ignore it and move on.  Errors are only thrown if the input is
// invalid, i.e. not hashable or not a tuple of length 2 in the case of
// dictionaries, or if a memory allocation error occurs.

// in the case of dictionaries, we should replace the current node's value
// with the new value if the key is already contained in the dictionary.  This
// overwrites the current mapped value without allocating a new node.


//////////////////////
////    EXTEND    ////
//////////////////////


/* Add multiple items to the end of a list, set, or dictionary. */
template <template <typename> class ViewType, typename NodeType>
inline void extend(ViewType<NodeType>* view, PyObject* items) {
    using Node = typename ViewType<NodeType>::Node;

    _extend_left_to_right(view, view->tail, (Node*)NULL, items);
}


//////////////////////////
////    EXTENDLEFT    ////
//////////////////////////


/* Add multiple items to the beginning of a list, set, or dictionary. */
template <template <typename> class ViewType, typename NodeType>
inline void extendleft(ViewType<NodeType>* view, PyObject* items) {
    using Node = typename ViewType<NodeType>::Node;

    _extend_right_to_left(view, (Node*)NULL, view->head, items);
}


///////////////////////////
////    EXTENDAFTER    ////
///////////////////////////


/* Insert elements into a set or dictionary immediately after the given sentinel
value. */
template <template <typename> class ViewType, typename NodeType>
inline void extendafter(
    ViewType<NodeType>* view,
    PyObject* sentinel,
    PyObject* items
) {
    using Node = typename ViewType<NodeType>::Node;

    // search for sentinel
    Node* left = view->search(sentinel);
    if (left == NULL) {  // sentinel not found
        PyErr_Format(PyExc_KeyError, "%R is not contained in the list", sentinel);
        return;
    }

    // insert items after sentinel
    _extend_left_to_right(view, left, (Node*)left->next, items);
}


////////////////////////////
////    EXTENDBEFORE    ////
////////////////////////////


/* Insert elements into a singly-linked set or dictionary immediately before a given
sentinel value. */
template <template <typename> class ViewType, typename NodeType>
inline void extendbefore_single(
    ViewType<NodeType>* view,
    PyObject* sentinel,
    PyObject* items
) {
    // NOTE: due to the singly-linked nature of the list, extendafter() is
    // O(m) while extendbefore() is O(n + m).  This is because we need to
    // traverse the whole list to find the node before the sentinel.
    using Node = typename ViewType<NodeType>::Node;

    // search for sentinel
    Node* right = view->search(sentinel);
    if (right == NULL) {  // sentinel not found
        PyErr_Format(PyExc_KeyError, "%R is not contained in the list", sentinel);
        return;
    }

    // iterate from head to find left bound (O(n))
    Node* left;
    Node* next;
    if (right == view->head) {
        left = NULL;
    } else {
        left = view->head;
        next = (Node*)left->next;
        while (next != right) {
            left = next;
            next = (Node*)next->next;
        }
    }

    // insert items between the left and right bounds
    _extend_right_to_left(view, left, right, items);
}


/* Insert elements into a doubly-linked set or dictionary immediately after a given
sentinel value. */
template <template <typename> class ViewType, typename NodeType>
inline void extendbefore_double(
    ViewType<NodeType>* view,
    PyObject* sentinel,
    PyObject* items
) {
    // NOTE: doubly-linked lists can extend in either direction in O(m) time.
    using Node = typename ViewType<NodeType>::Node;

    // search for sentinel
    Node* right = view->search(sentinel);
    if (right == NULL) {  // sentinel not found
        PyErr_Format(PyExc_KeyError, "%R is not contained in the list", sentinel);
        return;
    }

    // insert items before sentinel
    _extend_right_to_left(view, (Node*)right->prev, right, items);
}


///////////////////////
////    PRIVATE    ////
///////////////////////


/* Insert items from the left node to the right node. */
template <template <typename> class ViewType, typename NodeType, typename Node>
void _extend_left_to_right(
    ViewType<NodeType>* view,
    Node* left,
    Node* right,
    PyObject* items
) {
    // CPython API equivalent of `iter(items)`
    PyObject* iterator = PyObject_GetIter(items);
    if (iterator == NULL) {  // TypeError() during iter()
        return;
    }

    // CPython API equivalent of `for item in items:`
    Node* prev = left;
    while (true) {
        PyObject* item = PyIter_Next(iterator);  // next(iterator)
        if (item == NULL) {  // end of iterator or error
            break;
        }

        // allocate a new node
        Node* node = view->node(item);
        if (node == NULL) {
            Py_DECREF(item);
            break;  // enter undo branch
        }

        // insert from left to right
        view->link(prev, node, right);
        if (PyErr_Occurred()) {  // ValueError() item is already in list
            Py_DECREF(item);
            break;  // enter undo branch
        }

        // advance to next item
        prev = node;  // left bound becomes new node
        Py_DECREF(item);
    }

    // release iterator
    Py_DECREF(iterator);

    // check for error
    if (PyErr_Occurred()) {
        _undo_left_to_right(view, left, right);  // recover original list
        if (right == NULL) {
            view->tail = right;  // replace original tail
        }
    }
}


/* Insert items from the right node to the left node. */
template <template <typename> class ViewType, typename NodeType, typename Node>
void _extend_right_to_left(
    ViewType<NodeType>* view,
    Node* left,
    Node* right,
    PyObject* items
) {
    // CPython API equivalent of `iter(items)`
    PyObject* iterator = PyObject_GetIter(items);
    if (iterator == NULL) {  // TypeError() during iter()
        return;
    }

    // CPython API equivalent of `for item in items:`
    Node* next = right;
    while (true) {
        PyObject* item = PyIter_Next(iterator);  // next(iterator)
        if (item == NULL) {  // end of iterator or error
            break;
        }

        // allocate a new node
        Node* node = view->node(item);
        if (node == NULL) {  // TypeError() during hash() / tuple unpacking
            Py_DECREF(item);
            break;  // enter undo branch
        }

        // insert from right to left
        view->link(left, node, next);
        if (PyErr_Occurred()) {  // ValueError() item is already in list
            Py_DECREF(item);
            break;  // enter undo branch
        }

        // advance to next item
        next = node;  // right bound becomes new node
        Py_DECREF(item);
    }

    // release iterator
    Py_DECREF(iterator);

    // check for error
    if (PyErr_Occurred()) {
        _undo_right_to_left(view, left, right);  // recover original list
        if (left == NULL) {
            view->head = left;  // replace original head
        }
    }
}


/* Rewind an `extend()`/`extendafter()` call in the event of an error. */
template <template <typename> class ViewType, typename NodeType, typename Node>
void _undo_left_to_right(
    ViewType<NodeType>* view,
    Node* left,
    Node* right
) {
    Node* prev = left;  // NOTE: left must not be NULL, but right can be
    Node* curr = (Node*)prev->next;
    while (curr != right) {
        Node* next = (Node*)curr->next;
        view->unlink(prev, curr, next);
        view->recycle(curr);
        curr = next;
    }

    // join left and right
    Node::join(left, right);  // handles NULLs
}


/* Rewind an `extendleft()`/`extendbefore()` call in the event of an error. */
template <template <typename> class ViewType, typename NodeType, typename Node>
void _undo_right_to_left(
    ViewType<NodeType>* view,
    Node* left,
    Node* right
) {
    // NOTE: right must not be NULL, but left can be
    Node* prev;
    if (left == NULL) {
        prev = view->head;
    } else {
        prev = left;
    }

    // free staged nodes
    Node* curr = (Node*)prev->next;
    while (curr != right) {
        Node* next = (Node*)curr->next;
        view->unlink(prev, curr, next);
        view->recycle(curr);
        curr = next;
    }

    // join left and right
    Node::join(left, right);  // handles NULLs
}


////////////////////////
////    WRAPPERS    ////
////////////////////////


// NOTE: Cython doesn't play well with nested templates, so we need to
// explicitly instantiate specializations for each combination of node/view
// type.  This is a bit of a pain, put it's the only way to get Cython to
// properly recognize the functions.

// Maybe in a future release we won't have to do this:


template void extend(ListView<SingleNode>* view, PyObject* items);
template void extend(SetView<SingleNode>* view, PyObject* items);
template void extend(DictView<SingleNode>* view, PyObject* items);
template void extend(ListView<DoubleNode>* view, PyObject* items);
template void extend(SetView<DoubleNode>* view, PyObject* items);
template void extend(DictView<DoubleNode>* view, PyObject* items);
template void extendleft(ListView<SingleNode>* view, PyObject* items);
template void extendleft(SetView<SingleNode>* view, PyObject* items);
template void extendleft(DictView<SingleNode>* view, PyObject* items);
template void extendleft(ListView<DoubleNode>* view, PyObject* items);
template void extendleft(SetView<DoubleNode>* view, PyObject* items);
template void extendleft(DictView<DoubleNode>* view, PyObject* items);
template void extendafter(
    SetView<SingleNode>* view,
    PyObject* sentinel,
    PyObject* items
);
template void extendafter(
    DictView<SingleNode>* view,
    PyObject* sentinel,
    PyObject* items
);
template void extendafter(
    SetView<DoubleNode>* view,
    PyObject* sentinel,
    PyObject* items
);
template void extendafter(
    DictView<DoubleNode>* view,
    PyObject* sentinel,
    PyObject* items
);
template void extendbefore_single(
    SetView<SingleNode>* view,
    PyObject* sentinel,
    PyObject* items
);
template void extendbefore_single(
    DictView<SingleNode>* view,
    PyObject* sentinel,
    PyObject* items
);
template void extendbefore_single(
    SetView<DoubleNode>* view,
    PyObject* sentinel,
    PyObject* items
);
template void extendbefore_single(
    DictView<DoubleNode>* view,
    PyObject* sentinel,
    PyObject* items
);
template void extendbefore_double(
    SetView<DoubleNode>* view,
    PyObject* sentinel,
    PyObject* items
);
template void extendbefore_double(
    DictView<DoubleNode>* view,
    PyObject* sentinel,
    PyObject* items
);


#endif // EXTEND_H include guard