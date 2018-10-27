/* ============================================================
 * File  : sequence.h
 * Description :
 *   A set of classes providing a sequence of numbers within a
 *   specified range allowing navigation in the sequence.
 *     SequenceBase       - Base for all Sequence classes.
 *     SequenceInc        - An incrementing sequence.
 *     SequenceDec        - A decrementing sequence.
 *     SequenceRandomBase - Base for all 'random' Sequence classes.
 *     SequenceRandom     - A random sequence (can have duplicates).
 *     SequenceShuffle    - A random sequence (no duplicates).
 *     SequenceWeighted   - A random sequence (duplicates and weighted items).
 *

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <algorithm>
#include <iterator>
#include <vector>
#if defined _MSC_VER || defined __MINGW32__
#  include "compat.h"                   // for random
#endif

const size_t MAX_HISTORY_SIZE = 1024;

class SequenceBase
{
public:
    SequenceBase()
    : len(0), idx(0)
    { }

    virtual ~SequenceBase() = default;

    virtual void set(size_t _idx) = 0;

    virtual void extend(size_t items)
    {
        len += items;
    }

    size_t next()
    {
        ++idx;
        if (idx == len)
        {
            idx = 0;
        }
        return get();
    }

    size_t prev()
    {
        if (idx == 0)
        {
            idx = len;
        }
        --idx;
        return get();
    }

protected:
    virtual size_t get() = 0;

    size_t  len;
    size_t  idx;
};

class SequenceInc : public SequenceBase
{
protected:
    void set(size_t _idx) override // SequenceBase
    {
        idx = _idx;
    }

    size_t get() override // SequenceBase
    { return idx; }
};

class SequenceDec : public SequenceBase
{
protected:
    void set(size_t _idx) override // SequenceBase
    {
        idx = len - _idx - 1;
    }

    size_t get() override // SequenceBase
    {
        return len - idx - 1;
    }
};

class SequenceRandomBase : public SequenceBase
{
public:
    SequenceRandomBase()
    : eviction_idx(0)
    { }

    void set(size_t _idx) override // SequenceBase
    {
        size_t seq_idx = (idx + 1) % seq.size();
        seq[seq_idx] = _idx;
    }

    void extend(size_t items) override // SequenceBase
    {
        size_t extension = std::min(len + items, MAX_HISTORY_SIZE) - len;
        SequenceBase::extend(extension);
        vector<ssize_t>::iterator insertion_iter = seq.begin();
        std::advance(insertion_iter, eviction_idx);
        seq.insert(insertion_iter, extension, -1);
        if (idx > eviction_idx)
        {
            idx += extension;
        }
        eviction_idx += extension;
        if (eviction_idx == len && len > 0)
        {
            eviction_idx = (idx + 1) % len;
        }
    }

protected:
    size_t get() override // SequenceBase
    {
        if (idx == eviction_idx)
        {
            // The end was reached from the left.
            evict(eviction_idx);
            ++eviction_idx;
            if (eviction_idx == len)
            {
                eviction_idx = 0;
            }
        }
        else if (len > 0 && idx == (eviction_idx + 1) % len)
        {
            // The end was reached from the right.
            evict(eviction_idx + 1);
            if (eviction_idx == 0)
            {
                eviction_idx = len;
            }
            --eviction_idx;
        }

        size_t seq_idx = idx % seq.size();
        if (seq[seq_idx] == -1)
        {
            seq[seq_idx] = create();
        }
        return seq[seq_idx];
    }

    virtual void evict(size_t i)
    {
        // Evict portions of the history that are far away.
        seq[i] = -1;
    }

    virtual size_t create() = 0;

    vector<ssize_t> seq;
    size_t eviction_idx;
};

class SequenceRandom : public SequenceRandomBase
{
public:
    SequenceRandom()
    : items(0)
    {
        SequenceRandomBase::extend(MAX_HISTORY_SIZE);
    }

    void extend(size_t _items) override // SequenceRandomBase
    {
        items += _items;
        // The parent len was already extended enough, so it is not called.
    }

protected:
    size_t create() override // SequenceRandomBase
    {
        return (size_t)(((double)random()) * items / RAND_MAX);
    }

    size_t items;
};

class SequenceShuffle : public SequenceRandomBase
{
public:
    SequenceShuffle()
    : unseen(0)
    { }

    void set(size_t _idx) override // SequenceRandomBase
    {
        size_t seq_idx = (idx + 1) % seq.size();
        evict(seq_idx);
        ++eviction_idx;
        if (eviction_idx == len)
        {
            eviction_idx = 0;
        }
        if (map[_idx])
        {
            vector<ssize_t>::iterator old_iter = std::find(seq.begin(),
                seq.end(), _idx);
            if (old_iter != seq.end())
                *old_iter = -1;
        }
        else
        {
            map[_idx] = true;
            --unseen;
        }
        SequenceRandomBase::set(_idx);
    }

    void extend(size_t items) override // SequenceRandomBase
    {
        SequenceRandomBase::extend(items);
        map.resize(len, 0);
        unseen += items;
    }

protected:
    size_t create() override // SequenceRandomBase
    {
        size_t unseen_idx = (size_t)(((double)random()) * unseen / RAND_MAX);
        for (size_t i = 0; ; ++i)
        {
            if (!map[i])
            {
                if (!unseen_idx)
                {
                    map[i] = true;
                    --unseen;
                    return i;
                }
                --unseen_idx;
            }
        }
    }

    void evict(size_t i) override // SequenceRandomBase
    {
        ssize_t evicted = seq[i];
        if (evicted != -1)
        {
            map[evicted] = false;
            ++unseen;
        }
        SequenceRandomBase::evict(i);
    }

    vector<bool> map;
    size_t unseen;
};

class SequenceWeighted : public SequenceRandom
{
public:
    SequenceWeighted()
    : weightCursor(0), totalWeight(0)
    { }

    void extend(size_t items) override // SequenceRandom
    {
        weights.resize(weights.size() + items, totalWeight);
        SequenceRandom::extend(items);
    }

    void add(double weight)
    {
        totalWeight += weight;
        weights[weightCursor++] = totalWeight;
    }

protected:
    size_t create() override // SequenceRandom
    {
        double slot = (((double)random()) * totalWeight / RAND_MAX);
        vector<double>::iterator slot_iter = std::upper_bound(
            weights.begin(), weights.end(), slot);
        size_t slot_idx = std::distance(weights.begin(), slot_iter);
        return slot_idx;
    }

    vector<double> weights;
    size_t weightCursor;
    double totalWeight;
};

#endif // ndef SEQUENCE_H
