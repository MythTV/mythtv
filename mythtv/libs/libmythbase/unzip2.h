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

// libzip
#include "zip.h"

// Qt headers
#include <QDir>
#include <QString>
#include <QFileInfo>

class UnZip
{
    // NOLINTNEXTLINE(readability-uppercase-literal-suffix)
    static constexpr uint64_t kSTATS_REQUIRED {ZIP_STAT_NAME|ZIP_STAT_INDEX|ZIP_STAT_SIZE|ZIP_STAT_MTIME|ZIP_STAT_ENCRYPTION_METHOD};

    struct zipEntry
    {
        int           m_index      {0};
        zip_stat_t    m_stats      {};
        zip_uint32_t  m_attributes {0};
        QFileInfo     m_fi;
    };

  public:
    explicit UnZip(QString zipFile);
    ~UnZip();
    bool extractFile(const QString& outDir);

  private:
    bool isValid() { return m_zip != nullptr; };
    bool getEntryStats(zipEntry& entry);
    void getEntryAttrs(zipEntry& entry);
    static QFileDevice::Permissions zipToQtPerms(const zipEntry& entry);
    static bool zipCreateDirectory(const zipEntry& entry);
    bool zipValidateFilename(const QFileInfo& fi);
    static void zipSetFileAttributes(const zipEntry& entry, QFile& outfile);
    bool zipCreateSymlink(const zipEntry& entry);
    bool zipWriteOneFile(const zipEntry& entry);

    QDir  m_outDir;

    // Info about zip file itself
    QString      m_zipFileName;
    zip_t       *m_zip           {nullptr};
    zip_int64_t  m_zipFileCount  {-1};
};
