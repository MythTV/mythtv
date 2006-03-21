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
    DsmccDii() :
        download_id(0),          block_size(0),
        tc_download_scenario(0), number_modules(0),
        private_data_len(0),
        modules(NULL),           private_data(NULL) {}

    ~DsmccDii()
    {
        if (modules)
            delete[] modules;
    }

    unsigned long    download_id;
    unsigned short   block_size;
    unsigned long    tc_download_scenario;
    unsigned short   number_modules;
    unsigned short   private_data_len;
    DsmccModuleInfo *modules;
    unsigned char   *private_data;
};

class DsmccSectionHeader
{
  public:
    char table_id;  /* always 0x3B */

    unsigned char  flags[2];

    unsigned short table_id_extension;

    /*
     *  unsigned int section_syntax_indicator : 1; UKProfile - always 1
     *  unsigned int private_indicator : 1;  UKProfile - hence always 0
     *  unsigned int reserved : 2;  always 11b
     *  unsigned int dsmcc_section_length : 12;
     */

    unsigned char  flags2;

    /*
     *  unsigned int reserved : 2;  always 11b
     *  unsigned int version_number : 5;  00000b
     *  unsigned int current_next_indicator : 1  1b
     */

    unsigned long  crc;    /* UKProfile */
};

class DsmccDb
{
  public:
    DsmccDb() :
        module_id(0),    module_version(0),
        block_number(0), len (0) {}

    ~DsmccDb() {}

    unsigned short module_id;
    unsigned char  module_version;
    unsigned short block_number;
    unsigned int   len;
};

#endif

