#ifndef LINKED_LIST_H
#define LINKED_LIST_H

template <class T>
struct Node {
    T value;
    Node<T> *next;
};

template <class T>
class LinkedList {

public:
    LinkedList() {
        head = new Node<T>;
    }

    ~LinkedList() {
        Node<T> *node = head;
        while (node->next) {
            delete node;
            node = node->next;
        }
    }

    // Add a value to the head of the linked list.
    void add(T value) {
        Node<T> *n = new Node<T>;
        n->value = value;
        n->next = head;
        head = n;
    }

    // R
    T pop() {
        Node<T> *next = head->next;
        T value = head->value;
        head = next;
        return value;
    }

private:
    Node<T> *head;
};

#endif
