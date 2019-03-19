/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer 
 */
#ifndef DSMCC_BIOP_H
#define DSMCC_BIOP_H

#include <cstdlib>

#include "dsmcccache.h"

#define BIOP_OBJ_OFFSET 11
#define BIOP_TAG_OFFSET 17

class DSMCCCacheModuleData;
class Dsmcc;

class BiopNameComp
{
  public:
    BiopNameComp() = default;
    ~BiopNameComp();

    int Process(const unsigned char *);

    unsigned char  m_id_len   {0};
    unsigned char  m_kind_len {0};
    char          *m_id       {nullptr};
    char          *m_kind     {nullptr};
};

class BiopName
{
  public:
    BiopName() = default;
    ~BiopName();

    int Process(const unsigned char*);

    unsigned char  m_comp_count {0};
    BiopNameComp  *m_comps      {nullptr};
};

class BiopTap
{
  public:
    BiopTap() = default;
    ~BiopTap();

    int Process(const unsigned char*);

    unsigned short  m_id            {0};
    unsigned short  m_use           {0};
    // Only the association tag is currently used.
    unsigned short  m_assoc_tag     {0};
    unsigned short  m_selector_len  {0};
    char           *m_selector_data {nullptr};
};

class BiopConnbinder
{
  public:
    BiopConnbinder() = default;
    int Process(const unsigned char*); 

    unsigned long m_component_tag      {0};
    unsigned char m_component_data_len {0};
    unsigned char m_taps_count         {0};
    BiopTap       m_tap;
};

class BiopObjLocation
{
  public:
    BiopObjLocation() = default;
    ~BiopObjLocation() = default;

    int Process(const unsigned char*);

    unsigned long       m_component_tag      {0};
    char                m_component_data_len {0};
    char                m_version_major      {0};
    char                m_version_minor      {0};
    DSMCCCacheReference m_Reference;
};

class ProfileBody
{
  public:
    virtual ~ProfileBody() = default;
    virtual DSMCCCacheReference *GetReference() = 0;
    virtual int Process(const unsigned char *) = 0;
};

class ProfileBodyFull: public ProfileBody
{
  public:
    ProfileBodyFull() = default;
    virtual ~ProfileBodyFull() = default;
    int Process(const unsigned char *) override; // ProfileBody
    DSMCCCacheReference *GetReference() override // ProfileBody
        { return &m_obj_loc.m_Reference; }

  protected:
    unsigned long   m_data_len              {0};
    char            m_byte_order            {0};
    char            m_lite_components_count {0};
    BiopObjLocation m_obj_loc;

    /* Just for the moment make this public */
  public:
    BiopConnbinder  m_dsm_conn;
    /* ignore the rest  */
};

class ProfileBodyLite: public ProfileBody
{
  public:

    int Process(const unsigned char *) override; // ProfileBody

    // TODO Not currently implemented
    DSMCCCacheReference *GetReference() override // ProfileBody
        { return nullptr; }
};

// IOR - Interoperable Object Reference.
class BiopIor
{
  public:
    BiopIor() = default;
    ~BiopIor()
    {
        free(m_type_id);
        delete m_profile_body;
    }
 
    int Process(const unsigned char *);
    void AddTap(Dsmcc *pStatus);

    unsigned long  m_type_id_len           {0};
    char          *m_type_id               {nullptr};
    unsigned long  m_tagged_profiles_count {0};
    unsigned long  m_profile_id_tag        {0};
    ProfileBody   *m_profile_body          {nullptr};

    /* UKProfile - ignore other profiles */
};
    
class BiopBinding
{
  public:
    BiopBinding() = default;
    ~BiopBinding();

    int Process(const unsigned char *data);

    BiopName      m_name;
    char          m_binding_type {0};
    BiopIor       m_ior;
    unsigned int  m_objinfo_len  {0};
    char         *m_objinfo      {nullptr};
};

class ObjCarousel;

class BiopMessage
{
  public:
    BiopMessage() = default;
    ~BiopMessage();

    bool Process(DSMCCCacheModuleData *cachep, DSMCCCache *cache,
                 unsigned char *data, unsigned long *curp);

  protected:
    // Process directories and service gateways.
    bool ProcessDir(bool isSrg,
                    DSMCCCacheModuleData *cachep, DSMCCCache *cache,
                    const unsigned char *data, unsigned long *curp);
    // Process files.
    bool ProcessFile(DSMCCCacheModuleData *cachep, DSMCCCache *cache,
                     unsigned char *data, unsigned long *curp);

    bool ProcessMsgHdr(const unsigned char *data, unsigned long *curp);

  protected:
    unsigned char  m_version_major {0};
    unsigned char  m_version_minor {0};
    unsigned int   m_message_size  {0};
    DSMCCCacheKey  m_objkey;
    unsigned long  m_objkind_len   {0};
    unsigned int   m_objinfo_len   {0};
    char          *m_objinfo       {nullptr};

  public:
    char          *m_objkind       {nullptr};
};

// Data extracted from the descriptors in a BiopModuleInfo message
class ModuleDescriptorData
{
  public:
    ModuleDescriptorData() = default;

    void Process(const unsigned char *data, int length);

    bool          m_isCompressed {false};
    unsigned long m_originalSize {0};
};

class BiopModuleInfo
{
  public:
    int Process(const unsigned char *Data);

    unsigned long        m_mod_timeout;
    unsigned long        m_block_timeout;
    unsigned long        m_min_blocktime;
    unsigned char        m_taps_count;
    BiopTap              m_tap;

    ModuleDescriptorData m_descriptorData;
};

class DsmccModuleInfo
{
  public:
    unsigned short  m_module_id;
    unsigned long   m_module_size;
    unsigned char   m_module_version;
    unsigned char   m_module_info_len;
    unsigned char  *m_data            {nullptr};
    unsigned int    m_curp;
    BiopModuleInfo  m_modinfo;
};

#endif
