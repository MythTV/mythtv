/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer 
 */
#ifndef DSMCC_BIOP_H
#define DSMCC_BIOP_H

#include <array>
#include <cstdlib>

#include "dsmcccache.h"

static constexpr uint8_t BIOP_OBJ_OFFSET { 11 };
static constexpr uint8_t BIOP_TAG_OFFSET { 17 };

class DSMCCCacheModuleData;
class Dsmcc;

class BiopNameComp
{
  public:
    BiopNameComp() = default;
    ~BiopNameComp();

    int Process(const unsigned char *data);

    unsigned char  m_idLen    {0};
    unsigned char  m_kindLen  {0};
    char          *m_id       {nullptr};
    char          *m_kind     {nullptr};
};

class BiopName
{
  public:
    BiopName() = default;
    ~BiopName();

    int Process(const unsigned char *data);

    unsigned char  m_compCount  {0};
    BiopNameComp  *m_comps      {nullptr};
};

class BiopTap
{
  public:
    BiopTap() = default;
    ~BiopTap();

    int Process(const unsigned char *data);

    unsigned short  m_id            {0};
    unsigned short  m_use           {0};
    // Only the association tag is currently used.
    unsigned short  m_assocTag      {0};
    unsigned short  m_selectorLen   {0};
    char           *m_selectorData  {nullptr};
};

class BiopConnbinder
{
  public:
    BiopConnbinder() = default;
    int Process(const unsigned char *data);

    unsigned long m_componentTag      {0};
    unsigned char m_componentDataLen  {0};
    unsigned char m_tapsCount         {0};
    BiopTap       m_tap;
};

class BiopObjLocation
{
  public:
    BiopObjLocation() = default;
    ~BiopObjLocation() = default;

    int Process(const unsigned char *data);

    unsigned long       m_componentTag      {0};
    char                m_componentDataLen  {0};
    char                m_versionMajor      {0};
    char                m_versionMinor      {0};
    DSMCCCacheReference m_reference;
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
    ~ProfileBodyFull() override = default;
    int Process(const unsigned char *data) override; // ProfileBody
    DSMCCCacheReference *GetReference() override // ProfileBody
        { return &m_objLoc.m_reference; }

  protected:
    unsigned long   m_dataLen              {0};
    char            m_byteOrder            {0};
    char            m_liteComponentsCount  {0};
    BiopObjLocation m_objLoc;

    /* Just for the moment make this public */
  public:
    BiopConnbinder  m_dsmConn;
    /* ignore the rest  */
};

class ProfileBodyLite: public ProfileBody
{
  public:

    int Process(const unsigned char *data) override; // ProfileBody

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
        free(m_typeId);
        delete m_profileBody;
    }
 
    int Process(const unsigned char *data);
    void AddTap(Dsmcc *pStatus) const;

    unsigned long  m_typeIdLen            {0};
    char          *m_typeId               {nullptr};
    unsigned long  m_taggedProfilesCount  {0};
    unsigned long  m_profileIdTag         {0};
    ProfileBody   *m_profileBody          {nullptr};

    /* UKProfile - ignore other profiles */
};
    
class BiopBinding
{
  public:
    BiopBinding() = default;
    ~BiopBinding();

    int Process(const unsigned char *data);

    BiopName      m_name;
    char          m_bindingType  {0};
    BiopIor       m_ior;
    unsigned int  m_objInfoLen   {0};
    char         *m_objInfo      {nullptr};
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
    unsigned char  m_versionMajor {0};
    unsigned char  m_versionMinor {0};
    unsigned int   m_messageSize  {0};
    DSMCCCacheKey  m_objKey;
    unsigned long  m_objKindLen   {0};
    unsigned int   m_objInfoLen   {0};
    char          *m_objInfo      {nullptr};

  public:
    char          *m_objKind       {nullptr};
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

    unsigned long        m_modTimeout   {0};
    unsigned long        m_blockTimeout {0};
    unsigned long        m_minBlockTime {0};
    unsigned char        m_tapsCount    {0};
    BiopTap              m_tap;

    ModuleDescriptorData m_descriptorData;
};

class DsmccModuleInfo
{
  public:
    unsigned short  m_moduleId        {0};
    unsigned long   m_moduleSize      {0};
    unsigned char   m_moduleVersion   {0};
    unsigned char   m_moduleInfoLen   {0};
    unsigned char  *m_data            {nullptr};
    unsigned int    m_curp            {0};
    BiopModuleInfo  m_modInfo;
};

#endif
