#ifndef ITERATOR_H
#define ITERATOR_H

#include <string>

// The underlying entry MUST outlive the iterator.
namespace tinykv
{
    class Iterator
    {
    public:
        // Delete copy ctor & copy assign
        Iterator(const Iterator &) = delete;
        Iterator &operator=(const Iterator &) = delete;

        virtual ~Iterator() = default; // so that the actual, runtime object will have its dtor called

        // `true` if the iterator is in a valid postition and we can access the underlying element.
        virtual bool Valid() const = 0;

        // Position the iterator at the first key in the source.
        // After this, the iterator is Valid() iff the source is not empty.
        virtual void SeekToFirst() = 0;

        // Position the iterator at the last key in the source.
        // After this, the iterator is Valid() iff the source is not empty.
        virtual void SeekToLast() = 0;

        // Moves to the first position with matching target key.
        // If key is not found, iterator ends up in the last position and is no longer Valid().
        virtual void Seek(const std::string &target) = 0;

        // Advances the iterator to the next key in the source.
        // After this, the iterator is Valid() iff we haven't reached the end of the source.
        virtual void Next() = 0;

        // ACCESSORS
        // REQUIRES: Valid()

        // Return the key for the current entry pointed to.
        virtual const std::string &Key() const = 0;

        // Return the value for the current entry pointed to.
        virtual const std::string &Value() const = 0;

        virtual bool isTombstone() const = 0;
    };
}

#endif