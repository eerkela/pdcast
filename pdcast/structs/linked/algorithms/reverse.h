// include guard prevents multiple inclusion
#ifndef BERTRAND_STRUCTS_ALGORITHMS_REVERSE_H
#define BERTRAND_STRUCTS_ALGORITHMS_REVERSE_H

#include <type_traits>  // std::enable_if_t<>
#include "../core/view.h"  // ViewTraits


namespace bertrand {
namespace structs {
namespace linked {


    /* Reverse a linked list in-place. */
    template <typename View>
    auto reverse(View& view)
        -> std::enable_if_t<ViewTraits<View>::listlike, void>
    {
        using Node = typename View::Node;

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


}  // namespace linked
}  // namespace structs
}  // namespace bertrand


#endif // BERTRAND_STRUCTS_ALGORITHMS_REVERSE_H include guard
