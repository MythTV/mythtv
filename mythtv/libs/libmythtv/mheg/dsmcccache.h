/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from dsmcc by Richard Palmer 
 */
#ifndef DSMCC_CACHE_H
#define DSMCC_CACHE_H

#include <QStringList>
#include <QMap>

class BiopBinding;

class DSMCCCacheFile;
class DSMCCCacheDir;
class DSMCCCache;
class Dsmcc;

class DSMCCCacheKey: public QByteArray
{
  public:
    DSMCCCacheKey() = default;
    DSMCCCacheKey(const char * data, int size):
        QByteArray(data, size) {}
    QString toString(void) const;
    // Operator used in < for DSMCCCacheReference
    friend bool operator < (const DSMCCCacheKey &key1,
                            const DSMCCCacheKey &key2);
};

class DSMCCCacheReference
{
  public:
    DSMCCCacheReference() = default;

    DSMCCCacheReference(unsigned long car, unsigned short m,
                        unsigned short s, const DSMCCCacheKey &k) :
        m_nCarouselId(car),             m_nModuleId(m),
        m_nStreamTag(s),                m_Key(k) {}

    DSMCCCacheReference(const DSMCCCacheReference &r) :
        m_nCarouselId(r.m_nCarouselId), m_nModuleId(r.m_nModuleId),
        m_nStreamTag(r.m_nStreamTag),   m_Key(r.m_Key) {}

    DSMCCCacheReference& operator=(const DSMCCCacheReference &rhs)
    {
        m_nCarouselId = rhs.m_nCarouselId;
        m_nModuleId = rhs.m_nModuleId;
        m_nStreamTag = rhs.m_nStreamTag;
        m_Key = rhs.m_Key;
        return *this;
    }

    bool Equal(const DSMCCCacheReference &r) const;
    bool Equal(const DSMCCCacheReference *p) const;

    QString toString(void) const;

  public:
    unsigned long  m_nCarouselId {0}; // Reference info for the module
    unsigned short m_nModuleId   {0};
    unsigned short m_nStreamTag  {0};
    DSMCCCacheKey  m_Key;

    // Operator required for QMap
    friend bool operator < (const DSMCCCacheReference&,
                            const DSMCCCacheReference&);
};

// A directory
class DSMCCCacheDir
{
  public:
    DSMCCCacheDir() = default;
    explicit DSMCCCacheDir(const DSMCCCacheReference &r) : m_Reference(r) {}

    // These maps give the cache reference for each name
    QMap<QString, DSMCCCacheReference> m_SubDirectories;
    QMap<QString, DSMCCCacheReference> m_Files;

    DSMCCCacheReference m_Reference;
};

// The contents of a file.
class DSMCCCacheFile
{
  public:
    DSMCCCacheFile() = default;
    explicit DSMCCCacheFile(const DSMCCCacheReference &r) : m_Reference(r) {}

    DSMCCCacheReference m_Reference;
    QByteArray m_Contents; // Contents of the file.
};

class DSMCCCache
{
  public:
    explicit DSMCCCache(Dsmcc *);
    ~DSMCCCache();

    // Create a new gateway.
    DSMCCCacheDir *Srg(const DSMCCCacheReference &ref);
    // Create a new directory.
    DSMCCCacheDir *Directory(const DSMCCCacheReference &ref);
    // Add a file to the directory or gateway.
    static void AddFileInfo(DSMCCCacheDir *dir, const BiopBinding *);
    // Add a directory to the directory or gateway.
    static void AddDirInfo(DSMCCCacheDir *dir, const BiopBinding *);

    // Add the contents of a file.
    void CacheFileData(const DSMCCCacheReference &ref, const QByteArray &data);

    // Set the gateway reference from a DSI message.
    void SetGateway(const DSMCCCacheReference &ref);

    // Return the contents.
    int GetDSMObject(QStringList &objectPath, QByteArray &result);

  protected:
    // Find File, Directory or Gateway by reference.
    DSMCCCacheFile *FindFileData(DSMCCCacheReference &ref);
    DSMCCCacheDir *FindDir(DSMCCCacheReference &ref);
    DSMCCCacheDir *FindGateway(DSMCCCacheReference &ref);

    DSMCCCacheReference m_GatewayRef; // Reference to the gateway

    // The set of directories, files and gateways.
    QMap<DSMCCCacheReference, DSMCCCacheDir*> m_Directories;
    QMap<DSMCCCacheReference, DSMCCCacheDir*> m_Gateways;
    QMap<DSMCCCacheReference, DSMCCCacheFile*> m_Files;

  public:
    Dsmcc *m_Dsmcc {nullptr};
};

#endif
