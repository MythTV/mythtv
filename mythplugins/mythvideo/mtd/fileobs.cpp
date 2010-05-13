/*
    fileobs.cpp

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Implementation for file objects that know
    how to do clever things

*/

#include <unistd.h>

#include <mythcontext.h>
#include "fileobs.h"
#include <QDir>

RipFile::RipFile(const QString &a_base, const QString &an_extension,
                 bool auto_remove_bad) :
    base_name(a_base), extension(an_extension),
    active_file(-1), bytes_written(0),
    use_multiple_files(true),
    auto_remove_bad_rips(auto_remove_bad)
{
    filesize = gCoreContext->GetNumSetting("MTDRipSize", 0) * 1024 * 1024;
}

bool RipFile::open(const QIODevice::OpenMode &mode, bool multiple_files)
{
    use_multiple_files = multiple_files;
    access_mode        = mode;
    active_file        = files.size();
    files.push_back(new QFile(base_name + "_001of"));

    return files[active_file]->open(access_mode);

/*
    if (!use_multiple_files)
    {
        filesize = 0;
    }
*/
}

QStringList RipFile::close()
{
    QStringList output_file_names;

    auto_remove_bad_rips = false;

    if (((uint)active_file) < (uint)files.size())
        files[active_file]->close();

    if (files.size() == 1)
    {
        active_file = 0;
        QString new_name = base_name + extension;
        if (!QDir::current().rename(files[active_file]->fileName(), new_name))
        {
            VERBOSE(VB_IMPORTANT,
                    "Couldn't rename a ripped file on close ... \n\t\t\t" +
                    QString("old name: '%1'\n\t\t\t"
                            "new name: '%2'")
                    .arg(files[active_file]->fileName()).arg(new_name));
        }
        else
        {
            files[active_file]->setFileName(new_name);
            output_file_names.push_back(new_name);
        }
    }
    else
    {
        QList<QFile*>::iterator it = files.begin();
        for (; it != files.end(); ++it)
        {
            // TODO FIXME This doesn't sense, Thor probably meant to
            // use the index not files.size() as the middle value.
            QString new_name = QString("%1%2%3")
                .arg((*it)->fileName()).arg(files.size()).arg(extension);

            if (!QDir::current().rename((*it)->fileName(), new_name))
            {
                VERBOSE(VB_IMPORTANT, QString("Couldn't rename '%1' to '%2'")
                        .arg((*it)->fileName()).arg(new_name));
            }
            else
            {
                (*it)->setFileName(new_name);
                output_file_names.push_back(new_name);
            }
        }
    }

    return output_file_names;
}

void RipFile::remove()
{
    if (((uint)active_file) < (uint)files.size())
        files[active_file]->close();

    while (!files.empty())
    {
        files.back()->remove();
        delete files.back();
        files.pop_back();
    }
}

QString RipFile::name(void) const
{
    if (((uint)active_file) < (uint)files.size())
        return files[active_file]->fileName();
    else
        return QString::null;
}

bool RipFile::writeBlocks(unsigned char *the_data, int how_much)
{
    if (((uint)active_file) >= (uint)files.size())
        return false;

    if (filesize > 0 && how_much + bytes_written > filesize)
    {
        //
        //  Need to break here
        //
        files[active_file]->close();
    
        //
        //  and start a new file
        //
    
        QString number_extension;
        number_extension.sprintf("_%03dof", files.size() + 1);
        active_file = files.size();
        files.push_back(new QFile(base_name + number_extension));
        if (!files[active_file]->open(access_mode))
        {
            VERBOSE(VB_IMPORTANT,
                    "couldn't open another file in a set of rip files.");
            return false;               
        }
        bytes_written = 0;
    }

    int result = write(files[active_file]->handle(), the_data, how_much);
    if (result < 0)
    {
        VERBOSE(VB_IMPORTANT, "Got a negative result while writing blocks."
                " World may end shortly.");
        return false;
    }

    if (result == 0)
    {
        if (how_much == 0)
        {
            VERBOSE(VB_IMPORTANT, "Ripfile wrote 0 bytes, but that's all it"
                    " was asked to. Unlikely coincidence?");
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Ripfile writing 0 bytes of data. That's"
                    " probably not a good sign.");
            return false;
        }
    }

    if (result != how_much)
    {
        VERBOSE(VB_IMPORTANT, QString("Ripfile tried to write %1 bytes, but"
                                      " only managed to write %2")
                                      .arg(how_much).arg(result));
        return false;
    }

    bytes_written += result;

    return true;
}

RipFile::~RipFile()
{
    if ((active_file >= 0) && auto_remove_bad_rips)
        remove();

    while (!files.empty())
    {
        delete files.back();
        files.pop_back();
    }
}
