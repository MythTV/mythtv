/*
	fileobs.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Implementation for file objects that know
	how to do clever things

*/

#include <unistd.h>
#include <iostream>
using namespace std;

#include <mythtv/mythcontext.h>
#include "fileobs.h"
#include "qdir.h"

RipFile::RipFile(const QString &a_base, const QString &an_extension,
        bool auto_remove_bad) : base_name(a_base), extension(an_extension),
                                   auto_remove_bad_rips(auto_remove_bad)
{
    filesize = gContext->GetNumSetting("MTDRipSize", 0) * 1024 * 1024;
    active_file = NULL;
    files.clear();
    files.setAutoDelete(true);
    bytes_written = 0;
    use_multiple_files = true;
}

bool RipFile::open(int mode, bool multiple_files)
{
    use_multiple_files = multiple_files;
    access_mode = mode;
    QString filename = base_name + "_001of"; 
    QFile *new_file = new QFile(filename);
    files.append(new_file);
    active_file = new_file;
    return active_file->open(mode);
    if(!use_multiple_files)
    {
        filesize = 0;
    }
}

void RipFile::close()
{
    auto_remove_bad_rips = false;
    if(active_file)
    {
        active_file->close();
    }
    if(files.count() == 1)
    {
        QString new_name = base_name + extension;
        QDir stupid_qdir("this_is_stupid");
        if(!stupid_qdir.rename(active_file->name(), new_name))
        {
            cerr << "fileobs.o: Couldn't rename a ripped file on close ... that sucks." << endl;
            cerr << "   old name: \"" << active_file->name() << "\"" << endl;
            cerr << "   new name: \"" << new_name << "\"" << endl;
        }
        else
        {
            active_file->setName(new_name);
        }
    }
    else
    {
        QDir stupid_qdir("this_is_stupid");
        QFile *iter;
        for(iter = files.first(); iter; iter = files.next())
        {
            QString new_name = iter->name() + QString("%1").arg(files.count()) + extension;
            if(!stupid_qdir.rename(iter->name(), new_name))
            {
                cerr << "fileobs.o: Couldn't rename \"" << iter->name() << "\" to \"" << new_name << "\"" << endl;
            }
            else
            {
                iter->setName(new_name);
            }
        }
    }
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
            cerr << "fileobs.o: couldn't open another file in a set of rip files." << endl;
            return false;               
        }
        bytes_written = 0;
    }
    int result = write(active_file->handle(), the_data, how_much);
    if(result < 0)
    {
        cerr << "fileobs.o: Got a negative result while writing blocks. World may end shortly." << endl;
        return false;
    }
    if(result == 0)
    {
        if(how_much == 0)
        {
            cerr << "fileobs.o: Ripfile wrote 0 bytes, but that's all it was asked to. Unlikely coincidence?" << endl;
        }
        else
        {
            cerr << "fileobs.o: Ripfile writing 0 bytes of data. That's probably not a good sign." << endl;
            return false;
        }
    }
    if(result != how_much)
    {
        cerr << "fileobs.o: Ripfile tried to write " << how_much << " bytes, but only managed to write " << result << endl ;
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
