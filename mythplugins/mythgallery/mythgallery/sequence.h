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

#include <stdlib.h>

class SequenceBase  
{
public:
    SequenceBase(int _len, bool _reset = true) 
    : len(_len), idx(0) 
    { 
        if (_reset)
        {
            reset(_len); 
        }
    };

    virtual ~SequenceBase() { };

    virtual void reset(int _len) 
    {
        len = _len; 
        idx = 0;
    };

    virtual void extend(int _len)
    {
        len = _len; 
    };

    int index(int _idx) { idx = _idx; return index(); };
    int next(void)      { idx++; return index(); };
    int prev(void)      { idx--; return index(); };
    
    
protected:
    int index(void)
    {
        if( idx < 0 )
        {
            idx += len; 
        } 
        idx %= len; 
        int retval = get(); 
        return retval;
    };

    virtual int get(void) = 0;

    int  len;
    int  idx;
};

class SequenceInc : public SequenceBase
{
public: 
    SequenceInc(int _len) 
    : SequenceBase(_len) { };

protected:
    virtual int get(void) { return idx; };
};

class SequenceDec : public SequenceBase
{
public: 
    SequenceDec(int _len) 
    : SequenceBase(_len) { };

protected:
    virtual int get(void)
    { 
        return len - idx - 1; 
    };
};

class SequenceRandomBase : public SequenceBase
{
  public:
    SequenceRandomBase(int _len, bool _reset = true)
    : SequenceBase(_len, _reset), seq(0)
    { 
        if( _reset )
        {
            reset(_len);
        } 
    };

    virtual ~SequenceRandomBase()
    { 
        if(seq)
        { 
            delete[] seq; 
        }
    };

    virtual void reset(int _len)
    { 
        SequenceBase::reset(_len); 
        if(seq)
        { 
            delete[] seq; 
        } 

        seq = new int[len];

        for( int i = 0 ; i < len ; i++ )
        { 
            seq[i] = -1; 
        } 
    };

protected:
    virtual int get(void)
    { 
        if( seq[idx] == -1 )
        { 
            seq[idx] = create(); 
        } 
        return seq[idx]; 
    };

    virtual int create(void) = 0;

    int *seq;
};

class SequenceRandom : public SequenceRandomBase
{
public:
    SequenceRandom(int _len) 
    : SequenceRandomBase(_len) {};

protected:
    virtual int create(void)
    { 
        return (int)(((double)random()) * len / RAND_MAX); 
    };
};

#define MAP_IDX(idx)     map[((idx) / sizeof(int))]
#define MAP_MSK(idx)     (1 << ((idx) % sizeof(int)))
#define MAP_SET(map,idx) MAP_IDX(idx) |= MAP_MSK(idx)
#define MAP_CLR(map,idx) MAP_IDX(idx) &= ~MAP_MSK(idx)
#define MAP(map,idx)     (MAP_IDX(idx) & MAP_MSK(idx))

class SequenceShuffle : public SequenceRandomBase
{
public:
    SequenceShuffle(int _len)
      : SequenceRandomBase(_len, false), map(0), used(0)
    { 
        reset(_len); 
    };

    virtual ~SequenceShuffle()
    { 
        if (map)
        { 
            delete[] map; 
        } 
    };

    virtual void reset(int _len)
    { 
        SequenceRandomBase::reset(_len); 

        if(map)
        { 
           delete[] map;
        } 

        map = new int[(len / sizeof(int)) + 1];

        for( int i = 0 ; i < len ; i++ )
        { 
           MAP_CLR(map,i); 
        } 
    }; 

protected:
    virtual int create(void)
    { 
        while(1)
        { 
            int i = (int)(((double)random()) * len / RAND_MAX); 
            if( !MAP(map,i) )
            { 
                MAP_SET(map,i); 
                return i; 
            } 
        } 
    };

    int *map;
    int used;
};

#endif // ndef SEQUENCE_H
