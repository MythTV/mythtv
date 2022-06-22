// -*- Mode: c++ -*-
// Copyright (c) 2003-2012

#include "tablestatus.h"

static inline uint8_t BIT_SEL(uint8_t x) { return 1 << x; }

const std::array<const uint8_t,8> TableStatus::kInitBits = {0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00};

void TableStatus::InitSections(sections_t &sect, uint32_t last_section)
{
    sect.clear();

    uint endz = last_section >> 3;
    if (endz)
        sect.resize(endz, 0x00);
    sect.resize(32, 0xff);
    sect[endz] = kInitBits[last_section & 0x7];
}

void TableStatus::SetVersion(int32_t version, uint32_t last_section)
{
    if (m_version == version)
        return;

    m_version = version;
    InitSections(m_sections, last_section);
}

void TableStatus::SetSectionSeen(int32_t version, uint32_t section,
                                 uint32_t last_section, uint32_t segment_last_section)
{
    SetVersion(version, last_section);
    m_sections[section>>3] |= BIT_SEL(section & 0x7);
    if ((segment_last_section != 0xffff) && (last_section != segment_last_section))
        m_sections[segment_last_section >> 3]
                   |= kInitBits[segment_last_section & 0x7];

}

bool TableStatus::IsSectionSeen(int32_t version, uint32_t section) const
{
    if (m_version != version)
        return false;

    return (bool) (m_sections[section>>3] & BIT_SEL(section & 0x7));
}

bool TableStatus::HasAllSections() const
{
    for (uint32_t i = 0; i < 32; i++)
        if (m_sections[i] != 0xff)
            return false;
    return true;
}


void TableStatusMap::SetVersion(uint32_t key, int32_t version, uint32_t last_section)
{
    TableStatus &status = (*this)[key];
    //NOTE: relies on status.m_version being invalid(-2) if a new entry was just added to the map
    status.SetVersion(version, last_section);
}

void TableStatusMap::SetSectionSeen(uint32_t key, int32_t version, uint32_t section,
                                    uint32_t last_section, uint32_t segment_last_section)
{
    TableStatus &status = (*this)[key];
    //NOTE: relies on status.m_version being invalid(-2) if a new entry was just added to the map
    status.SetSectionSeen(version, section, last_section, segment_last_section);
}

bool TableStatusMap::IsSectionSeen(uint32_t key, int32_t version, uint32_t section) const
{
    const_iterator it = this->find(key);
    if (it == this->end() || it->m_version != version)
        return false;
    return (bool) (it->m_sections[section>>3] & BIT_SEL(section & 0x7));
}

bool TableStatusMap::HasAllSections(uint32_t key) const
{
    const_iterator it = this->find(key);
    if (it == this->end())
        return false;

    return it->HasAllSections();
}

