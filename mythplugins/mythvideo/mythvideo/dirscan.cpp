#include <qdir.h>

#include <map>
#include <vector>

#include "dbaccess.h"

#include "dirscan.h"

DirectoryHandler::~DirectoryHandler()
{
}

namespace
{
    class ext_lookup
    {
      private:
        typedef std::map<QString, bool> ext_map;
        ext_map m_extensions;
        bool m_list_unknown;

      public:
        ext_lookup(const FileAssociations::ext_ignore_list &ext_disposition,
                   bool list_unknown) : m_list_unknown(list_unknown)
        {
            for (FileAssociations::ext_ignore_list::const_iterator p =
                 ext_disposition.begin(); p != ext_disposition.end(); ++p)
            {
                m_extensions.insert(ext_map::value_type(p->first.lower(),
                                                        p->second));
            }
        }

        bool extension_ignored(const QString &extension) const
        {
            ext_map::const_iterator p = m_extensions.find(extension.lower());
            if (p != m_extensions.end())
                return p->second;
            return !m_list_unknown;
        }
    };

    void scan_dir(const QString &start_path, DirectoryHandler *handler,
                  const ext_lookup &ext_settings)
    {
        QDir d(start_path);

        if (!d.exists())
            return;

        const QFileInfoList *list = d.entryInfoList();
        if (!list)
            return;

        QFileInfoListIterator it(*list);
        QFileInfo *fi;

        QDir dir_tester;

        while ((fi = it.current()) != 0)
        {
            ++it;

            if (fi->fileName() == "." ||
                fi->fileName() == ".." ||
                fi->fileName() == "Thumbs.db")
            {
                continue;
            }

            if (!fi->isDir() &&
                ext_settings.extension_ignored(fi->extension(false))) continue;

            bool add_as_file = true;

            if (fi->isDir())
            {
                add_as_file = false;

                dir_tester.setPath(fi->absFilePath() + "/VIDEO_TS");
                if (dir_tester.exists())
                {
                    add_as_file = true;
                }
                else
                {
                    DirectoryHandler *dh = handler->newDir(fi->fileName(),
                                                           fi->absFilePath());
                    scan_dir(fi->absFilePath(), dh, ext_settings);
                }
            }

            if (add_as_file)
            {
                handler->handleFile(fi->fileName(), fi->absFilePath(),
                                    fi->extension(false));
            }
        }
    }
}

void ScanVideoDirectory(const QString &start_path, DirectoryHandler *handler,
        const FileAssociations::ext_ignore_list &ext_disposition,
        bool list_unknown_extensions)
{
    ext_lookup extlookup(ext_disposition, list_unknown_extensions);
    scan_dir(start_path, handler, extlookup);
}
