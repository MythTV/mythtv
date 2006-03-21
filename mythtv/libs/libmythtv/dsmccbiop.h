/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer 
 */
#ifndef DSMCC_BIOP_H
#define DSMCC_BIOP_H

#include "dsmcccache.h"

#define BIOP_OBJ_OFFSET 11
#define BIOP_TAG_OFFSET 17

class DSMCCCacheModuleData;
class Dsmcc;

class BiopNameComp
{
  public:
    BiopNameComp() :
        m_id_len(0), m_kind_len(0), m_id(NULL), m_kind(NULL) {}

    ~BiopNameComp();

    int Process(const unsigned char *);

    unsigned char  m_id_len;
    unsigned char  m_kind_len;
    char          *m_id;
    char          *m_kind;
};

class BiopName
{
  public:
    BiopName();
    ~BiopName();

    int Process(const unsigned char*);

    unsigned char  m_comp_count;
    BiopNameComp  *m_comps;
};

class BiopTap
{
  public:
    BiopTap() { selector_data = NULL; }
    ~BiopTap();

    int Process(const unsigned char*);

    unsigned short  id;
    unsigned short  use;
    // Only the association tag is currently used.
    unsigned short  assoc_tag;
    unsigned short  selector_len;
    char           *selector_data;
};

class BiopConnbinder
{
  public:
    int Process(const unsigned char*); 

    unsigned long component_tag;
    unsigned char component_data_len;
    unsigned char taps_count;
    BiopTap       tap;
};

class BiopObjLocation
{
  public:
    BiopObjLocation() { }
    ~BiopObjLocation() { }

    int Process(const unsigned char*);

    unsigned long  component_tag;
    char           component_data_len;
    char           version_major;
    char           version_minor;
    DSMCCCacheReference m_Reference;
};

class ProfileBody
{
  public:
    virtual ~ProfileBody() {}
    virtual DSMCCCacheReference *GetReference() = 0;
    virtual int Process(const unsigned char *) = 0;
};

class ProfileBodyFull: public ProfileBody
{
  public:
    ProfileBodyFull() { }
    virtual ~ProfileBodyFull() { }
    virtual int Process(const unsigned char *);
    virtual DSMCCCacheReference *GetReference()
        { return &obj_loc.m_Reference; }

  protected:
    unsigned long   data_len;
    char            byte_order;
    char            lite_components_count;
    BiopObjLocation obj_loc;

    /* Just for the moment make this public */
  public:
    BiopConnbinder  dsm_conn;
    /* ignore the rest  */
};

class ProfileBodyLite: public ProfileBody
{
  public:

    virtual int Process(const unsigned char *);

    // TODO Not currently implemented
    virtual DSMCCCacheReference *GetReference() { return NULL; }
};

// IOR - Interoperable Object Reference.
class BiopIor
{
  public:
    BiopIor() { type_id_len = 0; type_id = 0; m_profile_body = NULL; }
    ~BiopIor() { free(type_id); delete(m_profile_body); }
 
    int Process(const unsigned char *);
    void AddTap(Dsmcc *pStatus);

    unsigned long  type_id_len; 
    char          *type_id;
    unsigned long  tagged_profiles_count;
    unsigned long  profile_id_tag;
    ProfileBody   *m_profile_body;

    /* UKProfile - ignore other profiles */
};
    
class BiopBinding
{
  public:
    BiopBinding() : m_objinfo_len(0), m_objinfo(0) {}
    ~BiopBinding();

    int Process(const unsigned char *data);

    BiopName      m_name;
    char          m_binding_type;
    BiopIor       m_ior;
    unsigned int  m_objinfo_len;
    char         *m_objinfo;
};

class ObjCarousel;

class BiopMessage
{
  public:
    BiopMessage() : m_objinfo(NULL), m_objkind(NULL) {}
    ~BiopMessage();
    bool Process(DSMCCCacheModuleData *cachep, DSMCCCache *cache,
                 unsigned char *data, unsigned long *curp);

  protected:
    // Process directories and service gateways.
    bool ProcessDir(bool isSrg,
                    DSMCCCacheModuleData *cachep, DSMCCCache *cache,
                    unsigned char *data, unsigned long *curp);
    // Process files.
    bool ProcessFile(DSMCCCacheModuleData *cachep, DSMCCCache *cache,
                     unsigned char *data, unsigned long *curp);

    bool ProcessMsgHdr(unsigned char *data, unsigned long *curp);

  protected:
    unsigned char  m_version_major;
    unsigned char  m_version_minor;
    unsigned int   m_message_size;
    DSMCCCacheKey  m_objkey;
    unsigned long  m_objkind_len;
    unsigned int   m_objinfo_len;
    char          *m_objinfo;

  public:
    char          *m_objkind;
};

// Data extracted from the descriptors in a BiopModuleInfo message
class ModuleDescriptorData
{
  public:
    ModuleDescriptorData(): isCompressed(false), originalSize(0) { }

    void Process(const unsigned char *data, int length);

    bool          isCompressed;
    unsigned long originalSize;
};

class BiopModuleInfo
{
  public:
    int Process(const unsigned char *Data);

    unsigned long   mod_timeout;
    unsigned long   block_timeout;
    unsigned long   min_blocktime;
    unsigned char   taps_count;
    BiopTap         tap;

    ModuleDescriptorData descriptorData;
};

class DsmccModuleInfo
{
  public:
    unsigned short  module_id;
    unsigned long   module_size;
    unsigned char   module_version;
    unsigned char   module_info_len;
    unsigned char  *data;
    unsigned int    curp;
    BiopModuleInfo  modinfo;
};

#endif
