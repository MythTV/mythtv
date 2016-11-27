// -*- Mode: c++ -*-
// Copyright (c) 2003-2012
#ifndef TABLESTATUS_H_
#define TABLESTATUS_H_

// POSIX
#include <stdint.h>  // uint64_t

// C++
#include <vector>
using namespace std;

// Qt
#include <QMap>

class TableStatus
{
public:
    typedef vector<uint8_t>   sections_t;
    static void InitSections(sections_t &sect, uint32_t last_section);

    TableStatus() : m_version(-2) {}
    void SetVersion(int32_t version, uint32_t last_section);
    void SetSectionSeen(int32_t version, uint32_t section,
                        uint32_t last_section, uint32_t segment_last_section = 0xffff);
    bool IsSectionSeen(int32_t version, uint32_t section) const;
    bool HasAllSections() const;

    int32_t     m_version;
    sections_t  m_sections;

private:
    static const uint8_t init_bits[8];
};

class TableStatusMap : public QMap<uint64_t, TableStatus>
{
public:
    void SetVersion(uint32_t key, int32_t version, uint32_t last_section)
    {
        SetVersion(uint64_t(key), version, last_section);
    }

    void SetSectionSeen(uint32_t key, int32_t version, uint32_t section,
                        uint32_t last_section, uint32_t segment_last_section = 0xffff)
    {
        SetSectionSeen(uint64_t(key), version, section, last_section, segment_last_section);
    }

    bool IsSectionSeen(uint32_t key, int32_t version, uint32_t section) const
    {
        return IsSectionSeen(uint64_t(key), version, section);
    }

    bool HasAllSections(uint32_t key) const
    {
        return HasAllSections(uint64_t(key));
    }

    void SetVersion(uint64_t key, int32_t version, uint32_t last_section);
    void SetSectionSeen(uint64_t key, int32_t version, uint32_t section,
                        uint32_t last_section, uint32_t segment_last_section = 0xffff);
    bool IsSectionSeen(uint64_t key, int32_t version, uint32_t section) const;
    bool HasAllSections(uint64_t key) const;
};

#endif // TABLESTATUS_H_

