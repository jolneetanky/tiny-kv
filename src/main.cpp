#include <iostream>
#include <map>
// #include "kvstore.h"
#include "core/db_impl.h"

// how are you gonna store those guys?
// naively, I will use an in-memory key-value store
// for now i will use `std::map`, which is a BST under the hood and has timeO(logn) for all operations.
// hm no let's first write a class for our KV store. Just a simple class

int main()
{
    DbImpl dbImpl; // Creates the object on the stack. Destroyed once this function returns.
    dbImpl.put("alice", "bob");
    dbImpl.get("alice");
    dbImpl.get("non-existent key");

    dbImpl.del("alice");
    dbImpl.del("non-existent key");
    dbImpl.get("alice");
    return 0;
}