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

    unsigned long    m_download_id          {0};
    unsigned short   m_block_size           {0};
    unsigned long    m_tc_download_scenario {0};
    unsigned short   m_number_modules       {0};
    unsigned short   m_private_data_len     {0};
    DsmccModuleInfo *m_modules              {nullptr};
    unsigned char   *m_private_data         {nullptr};
};

class DsmccSectionHeader
{
  public:
    char           m_table_id;  /* always 0x3B */

    unsigned char  m_flags[2];

    unsigned short m_table_id_extension;

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

    unsigned short m_module_id      {0};
    unsigned char  m_module_version {0};
    unsigned short m_block_number   {0};
    unsigned int   m_len            {0};
};

#endif

