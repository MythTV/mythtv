// -*- Mode: c++ -*-
// Copyright (c) 2003-2012
#ifndef TABLESTATUS_H_
#define TABLESTATUS_H_

// C++
#include <cstdint>  // uint64_t
#include <vector>
using namespace std;

// Qt
#include "qmap.h"

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


class TableStatusMap : public QMap<uint32_t, TableStatus>
{
public:
    void SetVersion(uint32_t key, int32_t version, uint32_t last_section);
    void SetSectionSeen(uint32_t key, int32_t version, uint32_t section,
                        uint32_t last_section, uint32_t segment_last_section = 0xffff);
    bool IsSectionSeen(uint32_t key, int32_t version, uint32_t section) const;
    bool HasAllSections(uint32_t key) const;
};

#endif // TABLESTATUS_H_

