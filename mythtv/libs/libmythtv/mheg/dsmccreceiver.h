/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer 
 */
#ifndef DSMCC_RECEIVER_H
#define DSMCC_RECEIVER_H

#include "dsmccbiop.h"

class DsmccDii
{
  public:
    DsmccDii() = default;
    ~DsmccDii()
    {
        delete[] m_modules;
    }

    unsigned long    m_downloadId           {0};
    unsigned short   m_blockSize            {0};
    unsigned long    m_tcDownloadScenario   {0};
    unsigned short   m_numberModules        {0};
    unsigned short   m_privateDataLen       {0};
    DsmccModuleInfo *m_modules              {nullptr};
    unsigned char   *m_privateData          {nullptr};
};

class DsmccSectionHeader
{
  public:
    char           m_tableId;  /* always 0x3B */

    std::array<uint8_t,2> m_flags;

    unsigned short m_tableIdExtension;

    /*
     *  unsigned int section_syntax_indicator : 1; UKProfile - always 1
     *  unsigned int private_indicator : 1;  UKProfile - hence always 0
     *  unsigned int reserved : 2;  always 11b
     *  unsigned int dsmcc_section_length : 12;
     */

    unsigned char  m_flags2;

    /*
     *  unsigned int reserved : 2;  always 11b
     *  unsigned int version_number : 5;  00000b
     *  unsigned int current_next_indicator : 1  1b
     */

    unsigned long  m_crc;    /* UKProfile */
};

class DsmccDb
{
  public:
    DsmccDb() = default;
    ~DsmccDb() = default;

    unsigned short m_moduleId       {0};
    unsigned char  m_moduleVersion  {0};
    unsigned short m_blockNumber    {0};
    unsigned int   m_len            {0};
};

#endif

