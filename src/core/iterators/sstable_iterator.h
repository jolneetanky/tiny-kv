#ifndef SSTABLE_ITERATOR_H
#define SSTABLE_ITERATOR_H

#include "core/iterators/iterator.h"

namespace tinykv
{
    class SSTableIterator final : public Iterator
    {
    public:
        // `true` if the iterator is in a valid postition and we can access the underlying element.
        bool Valid() const override;

        // Position the iterator at the first key in the source.
        // After this, the iterator is Valid() iff the source is not empty.
        void SeekToFirst() override;

        // Position the iterator at the last key in the source.
        // After this, the iterator is Valid() iff the source is not empty.
        void SeekToLast() override;

        // Moves to the first position with matching target key.
        // If key is not found, iterator ends up in the last position and is no longer Valid().
        void Seek(const std::string &target) override;

        // Advances the iterator to the next key in the source.
        // After this, the iterator is Valid() iff we haven't reached the end of the source.
        void Next() override;

        // ACCESSORS
        // REQUIRES: Valid()

        // Return the key for the current entry pointed to.
        const std::string &Key() const override;

        // Return the value for the current entry pointed to.
        const std::string &Value() const override;

        bool isTombstone() const override;

        ~SSTableIterator() = default;
    };
}

#endif