/*
	fileobs.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Implementation for file objects that know
	how to do clever things

*/

#include <unistd.h>

#include <mythtv/mythcontext.h>
#include "fileobs.h"
#include "qdir.h"

RipFile::RipFile(const QString &a_base, const QString &an_extension,
        bool auto_remove_bad) : base_name(a_base), extension(an_extension),
                                   active_file(NULL), bytes_written(0),
                                   use_multiple_files(true),
                                   auto_remove_bad_rips(auto_remove_bad)
{
    filesize = gContext->GetNumSetting("MTDRipSize", 0) * 1024 * 1024;
    files.setAutoDelete(true);
}

bool RipFile::open(int mode, bool multiple_files)
{
    use_multiple_files = multiple_files;
    access_mode = mode;
    QString filename = base_name + "_001of"; 
    active_file = new QFile(filename);
    files.append(active_file);
    return active_file->open(mode);
/*
    if(!use_multiple_files)
    {
        filesize = 0;
    }
*/
}

QStringList RipFile::close()
{
    QStringList output_file_names;

    auto_remove_bad_rips = false;
    if(active_file)
    {
        active_file->close();
    }
    if(files.count() == 1)
    {
        QString new_name = base_name + extension;
        if(!QDir::current().rename(active_file->name(), new_name))
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Couldn't rename a ripped file on close ... "
                            " that sucks.\n"
                            "   old name: \"%1\"\n"
                            "   new name: \"%1\"")
                    .arg(active_file->name()).arg(new_name));
        }
        else
        {
            active_file->setName(new_name);
            output_file_names.push_back(new_name);
        }
    }
    else
    {
        QFile *iter;
        for(iter = files.first(); iter; iter = files.next())
        {
            QString new_name = iter->name() + QString("%1").arg(files.count()) +
                    extension;
            if(!QDir::current().rename(iter->name(), new_name))
            {
                VERBOSE(VB_IMPORTANT, QString("Couldn't rename '%1' to '%2'")
                        .arg(iter->name()).arg(new_name));
            }
            else
            {
                iter->setName(new_name);
                output_file_names.push_back(new_name);
            }
        }
    }
    return output_file_names;
}

void RipFile::remove()
{
    active_file->close();
    QFile *iter;
    for(iter = files.first(); iter; iter=files.next())
    {
        iter->remove();
    }
    files.clear();
}

QString RipFile::name()
{
    return active_file->name();
}

bool RipFile::writeBlocks(unsigned char *the_data, int how_much)
{
    if(filesize > 0 && how_much + bytes_written > filesize)
    {
        //
        //  Need to break here
        //
        active_file->close();
    
        //
        //  and start a new file
        //
    
        QString number_extension;
        number_extension.sprintf("_%03dof", files.count() + 1);
        QString filename = base_name + number_extension;
        QFile *new_file = new QFile(filename);
        active_file = new_file;
        files.append(new_file);
        if(!active_file->open(access_mode))
        {
            VERBOSE(VB_IMPORTANT,
                    "couldn't open another file in a set of rip files.");
            return false;               
        }
        bytes_written = 0;
    }
    int result = write(active_file->handle(), the_data, how_much);
    if(result < 0)
    {
        VERBOSE(VB_IMPORTANT, "Got a negative result while writing blocks."
                " World may end shortly.");
        return false;
    }
    if(result == 0)
    {
        if(how_much == 0)
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
    if(result != how_much)
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
    if (active_file && auto_remove_bad_rips)
    {
        remove();
    }
    files.clear();
}
