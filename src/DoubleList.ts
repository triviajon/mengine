interface DoublyLinkedListNode<T> {
    value: T;
    next: DoublyLinkedListNode<T> | null;
    prev: DoublyLinkedListNode<T> | null;
}

interface IDoublyLinkedList<T> {
    head: DoublyLinkedListNode<T> | null;
    tail: DoublyLinkedListNode<T> | null;

    addBefore(node: DoublyLinkedListNode<T>, newNode: DoublyLinkedListNode<T>): void;
    addAfter(node: DoublyLinkedListNode<T>, newNode: DoublyLinkedListNode<T>): void;
    addBeginning(newValue: T): void;
    remove(node: DoublyLinkedListNode<T>): boolean;
    app<U>(f: (value: T) => U): DoublyLinkedList<U>;
    // find(value: T): DoublyLinkedListNode<T> | null;
    isEmpty(): boolean;
    size(): number;
    clear(): void;
    [Symbol.iterator](): Iterator<T>;
}


class DoublyLinkedList<T> implements IDoublyLinkedList<T> {
    constructor(
        public head: DoublyLinkedListNode<T> | null,
        public tail: DoublyLinkedListNode<T> | null,
    ) {
        this.checkRep();
     }

    private checkRep(): void {
        if (this.head === null) {
            if (this.tail !== null) {
                throw new Error("Invariant violated: head is null but tail is not null.");
            }
        } else if (this.tail === null) {
            throw new Error("Invariant violated: tail is null but head is not null.");
        }

        if (this.head?.prev !== null) {
            throw new Error("Invariant violated: head.prev should be null.");
        }

        if (this.tail?.next !== null) {
            throw new Error("Invariant violated: tail.next should be null.");
        }

        let currentNode: DoublyLinkedListNode<T> | null = this.head;
        let prevNode: DoublyLinkedListNode<T> | null = null;

        while (currentNode !== null) {
            if (currentNode.prev !== prevNode) {
                throw new Error("Invariant violated: node.prev does not match the previous node.");
            }

            prevNode = currentNode;
            currentNode = currentNode.next;
        }

        if (prevNode !== this.tail) {
            throw new Error("Invariant violated: last node in the list is not the tail.");
        }
    }


    public addBefore(node: DoublyLinkedListNode<T>, newNode: DoublyLinkedListNode<T>): void {
        this.checkRep();
        newNode.next = node
        if (node.prev === null) {
            newNode.prev = null;
            this.head = newNode;
        } else {
            newNode.prev = node.prev;
            node.prev.next = newNode;
        }
        node.prev = newNode
        this.checkRep();
    }

    public addAfter(node: DoublyLinkedListNode<T>, newNode: DoublyLinkedListNode<T>): void {
        this.checkRep();
        newNode.prev = node
        if (node.next === null) {
            newNode.next = null;
            this.tail = newNode;
        } else {
            newNode.next = node.next;
            node.next.prev = newNode;
        }
        node.next = newNode
        this.checkRep();
    }

    public addBeginning(newValue: T): void {
        this.checkRep();
        const newNode: DoublyLinkedListNode<T> = {
            value: newValue,
            next: null,
            prev: null
        }

        if (this.head === null) {
            this.head = newNode;
            this.tail = newNode;
            newNode.prev = null;
            newNode.next = null;
        } else {
            this.addBefore(this.head, newNode);
        }
        this.checkRep();
    }

    public remove(node: DoublyLinkedListNode<T>): boolean {
        this.checkRep();
        if (node.prev === null && node.next === null) {
            return false;
        }

        if (node.prev === null) {
            this.head = node.next;
        } else {
            node.prev.next = node.next;
        }

        if (node.next === null) {
            this.tail = node.prev;
        } else {
            node.next.prev = node.prev;
        }

        this.checkRep();
        return true;
    }

    public app<U>(f: (value: T) => U): DoublyLinkedList<U> {
        this.checkRep();
        let currentNode = this.tail;
        const newList = new DoublyLinkedList<U>(null, null);
        while (currentNode !== null) {
            newList.addBeginning(f(currentNode.value));
            currentNode = currentNode.prev;
        }
        this.checkRep();
        return newList;
    }

    public isEmpty(): boolean {
        this.checkRep();
        return this.tail === null;
    }

    public size(): number {
        this.checkRep();
        let currentNode = this.head;
        let count = 0;
        while (currentNode !== null) {
            count++;
            currentNode = currentNode.next;
        }
        this.checkRep();
        return count;
    }

    public clear(): void {
        this.checkRep();
        this.head = null;
        this.tail = null;
    }

    [Symbol.iterator](): Iterator<T> {
        let currentNode = this.head;

        return {
            next: (): IteratorResult<T> => {
                if (currentNode !== null) {
                    const value = currentNode.value;
                    currentNode = currentNode.next;
                    return { value, done: false };
                } else {
                    return { value: undefined, done: true };
                }
            }
        };
    }
}
