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
    SequenceBase() = default;
    virtual ~SequenceBase() = default;

    virtual void set(size_t _idx) = 0;

    virtual void extend(size_t items)
    {
        m_len += items;
    }

    size_t next()
    {
        ++m_idx;
        if (m_idx == m_len)
        {
            m_idx = 0;
        }
        return get();
    }

    size_t prev()
    {
        if (m_idx == 0)
        {
            m_idx = m_len;
        }
        --m_idx;
        return get();
    }

protected:
    virtual size_t get() = 0;

    size_t  m_len {0};
    size_t  m_idx {0};
};

class SequenceInc : public SequenceBase
{
protected:
    void set(size_t _idx) override // SequenceBase
    {
        m_idx = _idx;
    }

    size_t get() override // SequenceBase
    { return m_idx; }
};

class SequenceDec : public SequenceBase
{
protected:
    void set(size_t _idx) override // SequenceBase
    {
        m_idx = m_len - _idx - 1;
    }

    size_t get() override // SequenceBase
    {
        return m_len - m_idx - 1;
    }
};

class SequenceRandomBase : public SequenceBase
{
public:
    SequenceRandomBase() = default;

    void set(size_t _idx) override // SequenceBase
    {
        size_t seq_idx = (m_idx + 1) % m_seq.size();
        m_seq[seq_idx] = _idx;
    }

    void extend(size_t items) override // SequenceBase
    {
        size_t extension = std::min(m_len + items, MAX_HISTORY_SIZE) - m_len;
        SequenceBase::extend(extension);
        vector<ssize_t>::iterator insertion_iter = m_seq.begin();
        std::advance(insertion_iter, m_eviction_idx);
        m_seq.insert(insertion_iter, extension, -1);
        if (m_idx > m_eviction_idx)
        {
            m_idx += extension;
        }
        m_eviction_idx += extension;
        if (m_eviction_idx == m_len && m_len > 0)
        {
            m_eviction_idx = (m_idx + 1) % m_len;
        }
    }

protected:
    size_t get() override // SequenceBase
    {
        if (m_idx == m_eviction_idx)
        {
            // The end was reached from the left.
            evict(m_eviction_idx);
            ++m_eviction_idx;
            if (m_eviction_idx == m_len)
            {
                m_eviction_idx = 0;
            }
        }
        else if (m_len > 0 && m_idx == (m_eviction_idx + 1) % m_len)
        {
            // The end was reached from the right.
            evict(m_eviction_idx + 1);
            if (m_eviction_idx == 0)
            {
                m_eviction_idx = m_len;
            }
            --m_eviction_idx;
        }

        size_t seq_idx = m_idx % m_seq.size();
        if (m_seq[seq_idx] == -1)
        {
            m_seq[seq_idx] = create();
        }
        return m_seq[seq_idx];
    }

    virtual void evict(size_t i)
    {
        // Evict portions of the history that are far away.
        m_seq[i] = -1;
    }

    virtual size_t create() = 0;

    vector<ssize_t> m_seq;
    size_t          m_eviction_idx {0};
};

class SequenceRandom : public SequenceRandomBase
{
public:
    SequenceRandom()
    {
        SequenceRandomBase::extend(MAX_HISTORY_SIZE);
    }

    void extend(size_t _items) override // SequenceRandomBase
    {
        m_items += _items;
        // The parent len was already extended enough, so it is not called.
    }

protected:
    size_t create() override // SequenceRandomBase
    {
        return (size_t)(((double)random()) * m_items / RAND_MAX);
    }

    size_t m_items {0};
};

class SequenceShuffle : public SequenceRandomBase
{
public:
    SequenceShuffle() = default;

    void set(size_t _idx) override // SequenceRandomBase
    {
        size_t seq_idx = (m_idx + 1) % m_seq.size();
        evict(seq_idx);
        ++m_eviction_idx;
        if (m_eviction_idx == m_len)
        {
            m_eviction_idx = 0;
        }
        if (m_map[_idx])
        {
            auto old_iter = std::find(m_seq.begin(), m_seq.end(), _idx);
            if (old_iter != m_seq.end())
                *old_iter = -1;
        }
        else
        {
            m_map[_idx] = true;
            --m_unseen;
        }
        SequenceRandomBase::set(_idx);
    }

    void extend(size_t items) override // SequenceRandomBase
    {
        SequenceRandomBase::extend(items);
        m_map.resize(m_len, 0);
        m_unseen += items;
    }

protected:
    size_t create() override // SequenceRandomBase
    {
        size_t unseen_idx = (size_t)(((double)random()) * m_unseen / RAND_MAX);
        for (size_t i = 0; ; ++i)
        {
            if (!m_map[i])
            {
                if (!unseen_idx)
                {
                    m_map[i] = true;
                    --m_unseen;
                    return i;
                }
                --unseen_idx;
            }
        }
    }

    void evict(size_t i) override // SequenceRandomBase
    {
        ssize_t evicted = m_seq[i];
        if (evicted != -1)
        {
            m_map[evicted] = false;
            ++m_unseen;
        }
        SequenceRandomBase::evict(i);
    }

    vector<bool> m_map;
    size_t       m_unseen {0};
};

class SequenceWeighted : public SequenceRandom
{
public:
    SequenceWeighted() = default;

    void extend(size_t items) override // SequenceRandom
    {
        m_weights.resize(m_weights.size() + items, m_totalWeight);
        SequenceRandom::extend(items);
    }

    void add(double weight)
    {
        m_totalWeight += weight;
        m_weights[m_weightCursor++] = m_totalWeight;
    }

protected:
    size_t create() override // SequenceRandom
    {
        double slot = (((double)random()) * m_totalWeight / RAND_MAX);
        vector<double>::iterator slot_iter = std::upper_bound(
            m_weights.begin(), m_weights.end(), slot);
        size_t slot_idx = std::distance(m_weights.begin(), slot_iter);
        return slot_idx;
    }

    vector<double> m_weights;
    size_t         m_weightCursor {0};
    double         m_totalWeight  {0.0};
};

#endif // ndef SEQUENCE_H
