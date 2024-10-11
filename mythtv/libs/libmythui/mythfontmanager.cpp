#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QList>
#include <QMutexLocker>

#include "libmythbase/mythlogging.h"

#include "mythfontmanager.h"

static MythFontManager *gFontManager = nullptr;

#define LOC      QString("MythFontManager: ")
static constexpr int8_t MAX_DIRS { 100 };

/**
 *  \brief Loads the fonts in font files within the given directory structure
 *
 *   Scans directory and its subdirectories, up to MAX_DIRS total, looking for
 *   TrueType (.ttf) and OpenType (.otf) font files or TrueType font
 *   collections (.ttc) and loads the fonts to make them available to the
 *   application.
 *
 *  \param directory      The directory to scan
 *  \param registeredFor  The user of the font. Used with releaseFonts() to
 *                        unload the font if no longer in use (by any users)
 *  \sa LoadFonts(const QString &, const QString &, int *)
 */
void MythFontManager::LoadFonts(const QString &directory,
                                const QString &registeredFor)
{
    int maxDirs = MAX_DIRS;
    LoadFonts(directory, registeredFor, &maxDirs);

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QFontDatabase database;
    QStringList families = database.families();
#else
    QStringList families = QFontDatabase::families();
#endif
    for (const QString & family : std::as_const(families))
    {
        QString result = QString("Font Family '%1': ").arg(family);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        QStringList styles = database.styles(family);
#else
        QStringList styles = QFontDatabase::styles(family);
#endif
        for (const QString & style : std::as_const(styles))
        {
            result += QString("%1(").arg(style);

            QString sizes;
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            QList<int> pointList = database.smoothSizes(family, style);
#else
            QList<int> pointList = QFontDatabase::smoothSizes(family, style);
#endif
            for (int points : std::as_const(pointList))
                sizes += QString::number(points) + ' ';

            result += QString("%1) ").arg(sizes.trimmed());
        }

        LOG(VB_GUI, LOG_DEBUG, LOC + result.trimmed());
    }
}

/**
 *  \brief Loads the fonts in font files within the given directory structure
 *
 *   Recursively scans all directories under directory looking for TrueType
 *   (.ttf) and OpenType (.otf) font files or TrueType font collections (.ttc)
 *   and loads the fonts to make them available to the application.
 *
 *  \param directory      The directory to scan
 *  \param registeredFor  The user of the font. Used with releaseFonts() to
 *                        unload the font if no longer in use (by any users)
 *  \param maxDirs        The maximum number of subdirectories to scan
 */
void MythFontManager::LoadFonts(const QString &directory,
                                const QString &registeredFor, int *maxDirs)
{
    if (directory.isEmpty() || directory == "/" || registeredFor.isEmpty())
        return;
    (*maxDirs)--;
    if (*maxDirs < 1)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Reached the maximum directory depth "
            "for a font directory structure. Terminating font scan.");
        return;
    }

    // Load the font files from this directory
    LoadFontsFromDirectory(directory, registeredFor);
    // Recurse through subdirectories
    QDir dir(directory);
    QFileInfoList files = dir.entryInfoList();
    for (const auto& info : std::as_const(files))
    {
        // Skip '.' and '..' and other files starting with "." by checking
        // baseName()
        if (!info.baseName().isEmpty() && info.isDir())
            LoadFonts(info.absoluteFilePath(), registeredFor, maxDirs);
        if (*maxDirs <= 0)
            break;
    }
}

/**
 *  \brief Removes the font references for registeredFor, and unloads the
 *         application font if it's no longer in use.
 *
 *  \param registeredFor The user of the font, as specified to LoadFonts()
 */
void MythFontManager::ReleaseFonts(const QString &registeredFor)
{
    if (registeredFor.isEmpty())
        return;

    QMutexLocker locker(&m_lock);
    for (FontPathToReference::iterator it = m_fontPathToReference.begin();
         it != m_fontPathToReference.end();)
    {
        MythFontReference *fontRef = it.value();
        if (registeredFor == fontRef->GetRegisteredFor())
        {
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("Removing application font '%1'")
                    .arg(fontRef->GetFontPath()));

            it = m_fontPathToReference.erase(it);
            if (!IsFontFileLoaded(fontRef->GetFontPath()))
            {
                if (QFontDatabase::removeApplicationFont(fontRef->GetFontID()))
                {
                    LOG(VB_FILE, LOG_DEBUG, LOC +
                        QString("Successfully removed application font '%1'")
                            .arg(fontRef->GetFontPath()));
                }
                else
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        QString("Unable to remove application font '%1'")
                            .arg(fontRef->GetFontPath()));
                }
            }
            delete fontRef;
        }
        else
        {
            ++it;
        }
    }
}

/**
 *  \brief Loads fonts from font files in the specified directory.
 *
 *  Non-recursive function called by the recursive MythFontManager::LoadFonts()
 *  function.
 *
 *  \param directory      The directory to scan
 *  \param registeredFor  The user of the font.
 */
void MythFontManager::LoadFontsFromDirectory(const QString &directory,
                                             const QString &registeredFor)
{
    if (directory.isEmpty() || directory == "/" || registeredFor.isEmpty())
        return;

    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("Scanning directory '%1' for font files.").arg(directory));

    QDir dir(directory);
    QStringList nameFilters = QStringList() << "*.ttf" << "*.otf" << "*.ttc";
    QFileInfoList fontFileInfos = dir.entryInfoList(nameFilters);
    for (const auto & info : std::as_const(fontFileInfos)) {
        LoadFontFile(info.absoluteFilePath(), registeredFor);
    }
}

/**
 *  \brief Loads fonts from the file specified in fontPath.
 *
 *  \param fontPath       The absolute path to the font file
 *  \param registeredFor  The user of the font.
 */
void MythFontManager::LoadFontFile(const QString &fontPath,
                                   const QString &registeredFor)
{
    if (fontPath.isEmpty() || fontPath == "/" || registeredFor.isEmpty())
        return;

    QMutexLocker locker(&m_lock);
    if (IsFontFileLoaded(fontPath))
    {
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
            QString("Font file '%1' already loaded")
                .arg(fontPath));

        if (!RegisterFont(fontPath, registeredFor))
        {
            LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
                QString("Unable to load font(s) in file '%1'")
                    .arg(fontPath));
        }
    }
    else
    {
        LOG(VB_GUI | VB_FILE, LOG_INFO, LOC +
            QString("Loading font file: '%1'").arg(fontPath));

        int result = QFontDatabase::addApplicationFont(fontPath);
        if (result > -1)
        {
            LOG(VB_GUI | VB_FILE, LOG_DEBUG, LOC +
                QString("In file '%1', found font(s) '%2'")
                    .arg(fontPath,
                         QFontDatabase::applicationFontFamilies(result)
                         .join(", ")));

            if (!RegisterFont(fontPath, registeredFor, result))
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("Unable to register font(s) in file '%1'")
                        .arg(fontPath));
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Unable to load font(s) in file '%1'")
                    .arg(fontPath));
        }
    }
}

/**
 *  \brief Registers the font as being used by registeredFor
 *
 *  \param fontPath       The absolute path to the font file
 *  \param registeredFor  The user of the font.
 *  \param fontID         The number provided by Qt when the font was
 *                        registered.
 */
bool MythFontManager::RegisterFont(const QString &fontPath,
                                   const QString &registeredFor,
                                   const int fontID)
{
    int id = fontID;
    if (id == -1)
    {
        QList<MythFontReference*> values;
        values = m_fontPathToReference.values(fontPath);
        if (values.isEmpty())
            return false;
        MythFontReference *ref = values.first();
        if (ref == nullptr)
            return false;
        id = ref->GetFontID();
    }
    auto *fontReference = new MythFontReference(fontPath, registeredFor, id);
    m_fontPathToReference.insert(fontPath, fontReference);
    return true;
}

/**
 *  \brief Checks whether the specified font file has already been loaded
 *
 *  \param fontPath  The absolute path to the font file
 */
bool MythFontManager::IsFontFileLoaded(const QString &fontPath)
{
    QList<MythFontReference*> values = m_fontPathToReference.values(fontPath);
    return !values.isEmpty();
}

MythFontManager *MythFontManager::GetGlobalFontManager(void)
{
    if (!gFontManager)
        gFontManager = new MythFontManager();
    return gFontManager;
}

MythFontManager *GetGlobalFontManager(void)
{
    return MythFontManager::GetGlobalFontManager();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
