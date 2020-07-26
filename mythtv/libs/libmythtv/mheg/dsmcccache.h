/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from dsmcc by Richard Palmer 
 */
#ifndef DSMCC_CACHE_H
#define DSMCC_CACHE_H

#include <utility>

// Qt headers
#include <QMap>
#include <QStringList>

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
                        unsigned short s, DSMCCCacheKey k) :
        m_nCarouselId(car),             m_nModuleId(m),
        m_nStreamTag(s),                m_key(std::move(k)) {}

    DSMCCCacheReference(const DSMCCCacheReference&) = default;
    DSMCCCacheReference& operator=(const DSMCCCacheReference&) = default;

    bool Equal(const DSMCCCacheReference &r) const;
    bool Equal(const DSMCCCacheReference *p) const;

    QString toString(void) const;

  public:
    unsigned long  m_nCarouselId {0}; // Reference info for the module
    unsigned short m_nModuleId   {0};
    unsigned short m_nStreamTag  {0};
    DSMCCCacheKey  m_key;

    // Operator required for QMap
    friend bool operator < (const DSMCCCacheReference &ref1,
                            const DSMCCCacheReference &ref2);
};

// A directory
class DSMCCCacheDir
{
  public:
    DSMCCCacheDir() = default;
    explicit DSMCCCacheDir(const DSMCCCacheReference &r) : m_reference(r) {}

    // These maps give the cache reference for each name
    QMap<QString, DSMCCCacheReference> m_subDirectories;
    QMap<QString, DSMCCCacheReference> m_files;

    DSMCCCacheReference m_reference;
};

// The contents of a file.
class DSMCCCacheFile
{
  public:
    DSMCCCacheFile() = default;
    explicit DSMCCCacheFile(const DSMCCCacheReference &r) : m_reference(r) {}

    DSMCCCacheReference m_reference;
    QByteArray m_contents; // Contents of the file.
};

class DSMCCCache
{
  public:
    explicit DSMCCCache(Dsmcc *dsmcc);
    ~DSMCCCache();

    // Create a new gateway.
    DSMCCCacheDir *Srg(const DSMCCCacheReference &ref);
    // Create a new directory.
    DSMCCCacheDir *Directory(const DSMCCCacheReference &ref);
    // Add a file to the directory or gateway.
    static void AddFileInfo(DSMCCCacheDir *dir, const BiopBinding *pBB);
    // Add a directory to the directory or gateway.
    static void AddDirInfo(DSMCCCacheDir *dir, const BiopBinding *pBB);

    // Add the contents of a file.
    void CacheFileData(const DSMCCCacheReference &ref, const QByteArray &data);

    // Set the gateway reference from a DSI message.
    void SetGateway(const DSMCCCacheReference &ref);

    // Return the contents.
    int GetDSMObject(QStringList &objectPath, QByteArray &result);

  protected:
    // Find File, Directory or Gateway by reference.
    DSMCCCacheFile *FindFileData(const DSMCCCacheReference &ref);
    DSMCCCacheDir *FindDir(const DSMCCCacheReference &ref);
    DSMCCCacheDir *FindGateway(const DSMCCCacheReference &ref);

    DSMCCCacheReference m_gatewayRef; // Reference to the gateway

    // The set of directories, files and gateways.
    QMap<DSMCCCacheReference, DSMCCCacheDir*> m_directories;
    QMap<DSMCCCacheReference, DSMCCCacheDir*> m_gateways;
    QMap<DSMCCCacheReference, DSMCCCacheFile*> m_files;

  public:
    Dsmcc *m_dsmcc {nullptr};
};

#endif
