/*
 *  Class UnZip
 *
 *  Copyright (c) David Hampton 2021
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "unzip2.h"

// libmythbase headers
#include "mythdate.h"
#include "mythlogging.h"

static constexpr uint32_t   ZIP_ATTR_FILE_TYPE_MASK      { 0xFE000000 };
static constexpr uint32_t   ZIP_ATTR_FILE_TYPE_SYMLINK   { 0xA0000000 };
//static constexpr uint32_t ZIP_ATTR_FILE_TYPE_NORMAL    { 0x80000000 };

static constexpr uint32_t   ZIP_ATTR_USER_PERM_MASK      { 0x01C00000 };
static constexpr uint32_t   ZIP_ATTR_GROUP_PERM_MASK     { 0x03800000 };
static constexpr uint32_t   ZIP_ATTR_OTHER_PERM_MASK     { 0x00700000 };
static constexpr uint8_t    ZIP_ATTR_USER_PERM_SHIFT     { 22 };
static constexpr uint8_t    ZIP_ATTR_GROUP_PERM_SHIFT    { 19 };
static constexpr uint8_t    ZIP_ATTR_OTHER_PERM_SHIFT    { 16 };

UnZip::UnZip(QString zipFileName)
    : m_zipFileName(std::move(zipFileName))
{
    int err { ZIP_ER_OK };
    m_zip = zip_open(qPrintable(m_zipFileName), 0, &err);
    if (m_zip != nullptr)
        return;

    LOG(VB_GENERAL, LOG_ERR,
        QString("UnZip: Unable to open zip file %1, error %2")
        .arg(m_zipFileName).arg(err));
}

UnZip::~UnZip()
{
    int err = zip_close(m_zip);
    if (err == 0)
        return;

    LOG(VB_GENERAL, LOG_DEBUG,
        QString("UnZip: Error closing zip file %1, error %2")
        .arg(m_zipFileName).arg(err));
}

bool UnZip::getEntryStats(zipEntry& entry)
{
    zip_stat_init(&entry.m_stats);
    if (-1 == zip_stat_index(m_zip, entry.m_index, 0, &entry.m_stats))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UnZip: Can't get info for index %1 in %2")
            .arg(entry.m_index).arg(m_zipFileName));
        return false;
    }
    if ((entry.m_stats.valid & kSTATS_REQUIRED) != kSTATS_REQUIRED)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UnZip: Invalid status for index %1 in %2")
            .arg(entry.m_index).arg(m_zipFileName));
        return false;
    }
    return true;
}

void UnZip::getEntryAttrs(zipEntry& entry)
{
    zip_uint8_t opsys {ZIP_OPSYS_UNIX};// NOLINT(readability-uppercase-literal-suffix)

    entry.m_attributes = 0;
    zip_file_get_external_attributes(m_zip, entry.m_index, 0, &opsys,
                                     &entry.m_attributes);
}

bool UnZip::zipCreateDirectory(const zipEntry& entry)
{
    QDir dir = entry.m_fi.absoluteDir();
    if (dir.exists())
        return true;
    if (dir.mkpath(dir.absolutePath()))
        return true;
    LOG(VB_GENERAL, LOG_ERR,
        QString("UnZip: Failed to create directory %1")
        .arg(dir.absolutePath()));
    return false;
}

// Validate that the filename is beneath the extraction directrory.
// This prevents a zip from overwriting arbitrary files by using names
// with a sequence of ".." directories.
bool UnZip::zipValidateFilename(const QFileInfo& fi)
{
    if (fi.absoluteFilePath().startsWith(m_outDir.absolutePath()))
        return true;
    LOG(VB_GENERAL, LOG_ERR,
        QString("UnZip: Attempt to write outside destination directory. File: %1")
        .arg(QString(fi.fileName())));
    return false;
}

// Would be nice if Qt provided a unix perm to Qt perm conversion.
QFileDevice::Permissions UnZip::zipToQtPerms(const zipEntry& entry)
{
    QFileDevice::Permissions qt_perms;
    zip_uint32_t attrs = entry.m_attributes;

    int32_t user  = (attrs & ZIP_ATTR_USER_PERM_MASK)  >> ZIP_ATTR_USER_PERM_SHIFT;
    int32_t group = (attrs & ZIP_ATTR_GROUP_PERM_MASK) >> ZIP_ATTR_GROUP_PERM_SHIFT;
    int32_t other = (attrs & ZIP_ATTR_OTHER_PERM_MASK) >> ZIP_ATTR_OTHER_PERM_SHIFT;

    if (user & 4)
        qt_perms |= (QFileDevice::ReadOwner  | QFileDevice::ReadUser);
    if (user & 2)
        qt_perms |= (QFileDevice::WriteOwner | QFileDevice::WriteUser);
    if (user & 1)
        qt_perms |= (QFileDevice::ExeOwner   | QFileDevice::ExeUser);
    if (group & 4)
        qt_perms |= QFileDevice::ReadGroup;
    if (group & 2)
        qt_perms |= QFileDevice::WriteGroup;
    if (group & 1)
        qt_perms |= QFileDevice::ExeGroup;
    if (other & 4)
        qt_perms |= QFileDevice::ReadOther;
    if (other & 2)
        qt_perms |= QFileDevice::WriteOther;
    if (other & 1)
        qt_perms |= QFileDevice::ExeOther;
    return qt_perms;
}

void UnZip::zipSetFileAttributes(const zipEntry& entry, QFile& outfile)
{
    // Set times
    auto dateTime = MythDate::fromSecsSinceEpoch(entry.m_stats.mtime);

    outfile.setFileTime(dateTime, QFileDevice::FileAccessTime);
    outfile.setFileTime(dateTime, QFileDevice::FileBirthTime);
    outfile.setFileTime(dateTime, QFileDevice::FileMetadataChangeTime);
    outfile.setFileTime(dateTime, QFileDevice::FileModificationTime);

    if (entry.m_attributes == 0)
        return;
    outfile.setPermissions(zipToQtPerms(entry));
}

bool UnZip::zipCreateSymlink(const zipEntry& entry)
{
    zip_file_t *infile = zip_fopen_index(m_zip, entry.m_index, 0);
    if (infile == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UnZip: Can't open index %1 in %2")
            .arg(entry.m_index).arg(m_zipFileName));
        return false;
    }

    int64_t  readLen      {0};
    static constexpr int BLOCK_SIZE { 4096 };
    QByteArray data; data.resize(BLOCK_SIZE);
    readLen = zip_fread(infile, data.data(), BLOCK_SIZE);
    if (readLen < 1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UnZip: Invalid symlink name for index %1 in %2")
            .arg(entry.m_index).arg(m_zipFileName));
        return false;
    }
    data.resize(readLen);

    auto target = QFileInfo(entry.m_fi.absolutePath() + "/" + data);
    if (!zipValidateFilename(target))
        return false;
    if (!QFile::link(target.absoluteFilePath(), entry.m_fi.absoluteFilePath()))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UnZip: Failed to create symlink from %1 to %2")
            .arg(entry.m_fi.absoluteFilePath(),
                 target.absoluteFilePath()));
        return false;
    }
    return true;
}

bool UnZip::zipWriteOneFile(const zipEntry& entry)
{
    zip_file_t *infile = zip_fopen_index(m_zip, entry.m_index, 0);
    if (infile == nullptr)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UnZip: Can't open file at index %1 in %2")
            .arg(entry.m_index).arg(m_zipFileName));
        return false;
    }

    auto outfile = QFile(entry.m_fi.absoluteFilePath());
    if (!outfile.open(QIODevice::Truncate|QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UnZip: Failed to open output file %1")
            .arg(entry.m_fi.absoluteFilePath()));
        return false;
    }

    int64_t  readLen      {0};
    uint64_t bytesRead    {0};
    uint64_t bytesWritten {0};
    static constexpr int BLOCK_SIZE { 4096 };
    QByteArray data; data.resize(BLOCK_SIZE);
    while ((readLen = zip_fread(infile, data.data(), BLOCK_SIZE)) > 0)
    {
        bytesRead += readLen;
        int64_t writeLen = outfile.write(data.data(), readLen);
        if (writeLen < 0)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("UnZip: Failed to write %1/%2 bytes to output file %3")
                .arg(writeLen).arg(readLen).arg(entry.m_fi.absoluteFilePath()));
            return false;
        }
        bytesWritten += writeLen;
    }

    if ((entry.m_stats.size != bytesRead) ||
        (entry.m_stats.size != bytesWritten))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UnZip: Failed to copy file %1. Read %2 and wrote %3 of %4.")
            .arg(entry.m_fi.fileName()).arg(bytesRead).arg(bytesWritten)
            .arg(entry.m_stats.size));
        return false;
    }

    outfile.flush();
    zipSetFileAttributes(entry, outfile);
    outfile.close();
    if (zip_fclose(infile) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UnZip: Failed to close file at index %1 in %2")
            .arg(entry.m_index).arg(entry.m_fi.fileName()));
        return false;
    }

    return true;
}

bool UnZip::extractFile(const QString& outDirName)
{
    if (!isValid())
        return false;

    m_outDir = QDir(outDirName);
    if (!m_outDir.exists())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UnZip: Target directory %1 doesn't exist")
            .arg(outDirName));
        return false;
    }

    m_zipFileCount = zip_get_num_entries(m_zip, 0);
    if (m_zipFileCount < 1)
    {
        LOG(VB_GENERAL, LOG_ERR,
                QString("UnZip: Zip archive %1 is empty")
                        .arg(m_zipFileName));
        return false;
    }

    bool ok { true };
    for (auto index = 0; ok && (index < m_zipFileCount); index++)
    {
        zipEntry entry;
        entry.m_index = index;
        if (!getEntryStats(entry))
            return false;
        if (entry.m_stats.encryption_method > 0)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("UnZip: Skipping encryped file %1 in %2")
                .arg(entry.m_index).arg(m_zipFileName));
            continue;
        }
        getEntryAttrs(entry);

        entry.m_fi = QFileInfo(outDirName + '/' + entry.m_stats.name);
        ok = zipValidateFilename(entry.m_fi);
        if (ok)
            ok = zipCreateDirectory(entry);
        if (ok && (entry.m_stats.size > 0))
        {
            switch (entry.m_attributes & ZIP_ATTR_FILE_TYPE_MASK)
            {
              case ZIP_ATTR_FILE_TYPE_SYMLINK:
                  ok = zipCreateSymlink(entry);
                break;
              default:
                  ok = zipWriteOneFile(entry);
                break;
            }
        }
    }

    return ok;
}
