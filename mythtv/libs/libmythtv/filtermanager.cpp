#include <dlfcn.h>
#include <iostream>
#include <qdir.h>
#include <qstringlist.h>
#include <qstrlist.h>

using namespace std;
#include "filtermanager.h"

FilterChain::FilterChain(void)
{
    setAutoDelete(true);
}

FilterChain::~FilterChain()
{
    clear();
}

void FilterChain::ProcessFrame(VideoFrame *Frame)
{
    if (!Frame)
        return;

    VideoFilter *VF = first();
    while (VF)
    {
        VF->filter(VF, Frame);
        VF = next();
    }
}

void FilterChain::deleteItem(QPtrCollection::Item d)
{
    if (del_item)
    {
        VideoFilter *Filter = (VideoFilter *)d;
        if (Filter->opts)
            free(Filter->opts);
        if (Filter->cleanup)
            Filter->cleanup(Filter);
        dlclose(Filter->handle);
        free(Filter);
    }
}

FilterManager::FilterManager()
{
    QDir FiltDir(MYTHTV_FILTER_PATH);
    QString Path;

    FiltDir.setFilter(QDir::Files | QDir::Readable);
    if (FiltDir.exists())
    {
        QStringList LibList = FiltDir.entryList();
        for (QStringList::iterator i = LibList.begin(); i != LibList.end();
             i++)
        {
            Path = FiltDir.filePath(*i);
            LoadFilterLib(Path);
        }
    }
}

FilterManager::~FilterManager()
{
    for (QPtrListIterator<FilterInfo> i(Filters); i.current (); ++i)
    {
        free(i.current()->symbol);
        free(i.current()->name);
        free(i.current()->descript);
        free(i.current()->libname);
        delete (i.current()->formats);
        delete i.current();
    }
}

void FilterManager::LoadFilterLib(QString Path)
{
    void *handle;
    int i;
    FilterInfo *FiltInfo;

    handle = dlopen(Path.ascii(), RTLD_LAZY);
    if (handle)
    {
        FiltInfo = (FilterInfo *)dlsym(handle, "filter_table");
        if (dlerror() == NULL)
        {
            for (; FiltInfo->symbol != NULL; FiltInfo++)
            {
                if (FiltInfo->symbol == NULL || FiltInfo->name == NULL
                    || FiltInfo->formats == NULL)
                    break;

                FilterInfo *NewFilt = new FilterInfo;
                NewFilt->symbol = strdup(FiltInfo->symbol);
                NewFilt->name = strdup(FiltInfo->name);
                NewFilt->descript = strdup(FiltInfo->descript);

                for (i = 0; FiltInfo->formats[i].in >= 0; i++)
                     ;

                NewFilt->formats = new FmtConv[i + 1];
                memcpy(NewFilt->formats, FiltInfo->formats,
                       sizeof(FmtConv) * (i + 1));

                NewFilt->libname = strdup(Path.ascii());
                FilterByName.insert(NewFilt->name, NewFilt);
                Filters.append(NewFilt);
            }
        }
    }
}

FilterChain *FilterManager::LoadFilters(QString Filters, 
                                        VideoFrameType &inpixfmt,
                                        VideoFrameType &outpixfmt, int &width,
                                        int &height)
{
    QPtrList<FilterInfo> FiltInfoChain;
    FilterChain *FiltChain = new FilterChain;
    QPtrList<FmtConv> FmtList;
    FilterInfo *FI;
    QString Opts;
    FilterInfo *Convert = GetFilterInfoByName("convert");
    QStrList OptsList (TRUE);
    QStringList FilterList = QStringList::split(",", Filters);
    VideoFilter *NewFilt = NULL;
    FmtConv *FC;
    VideoFrameType ofmt;
    unsigned int i;
    int postfilt_width = width;
    int postfilt_height = height;
    FmtList.setAutoDelete(TRUE);
    OptsList.setAutoDelete(TRUE);

    for (QStringList::Iterator i = FilterList.begin();
         i != FilterList.end(); ++i)
    {
        QString FiltName = (*i).section('=', 0, 0);
        QString FiltOpts = (*i).section('=', 1);
        FI = GetFilterInfoByName(FiltName);

        if (FI != NULL)
        {
            FiltInfoChain.append(FI);
            OptsList.append(FiltOpts);
        }
        else
            return NULL;
    }
    if (FiltInfoChain.count() == 0)
        return NULL;

    if (inpixfmt == -1)
        inpixfmt = FiltInfoChain.first()->formats[0].in;
    if (outpixfmt == -1)
        outpixfmt = FiltInfoChain.last()->formats[0].out;
    ofmt = outpixfmt;

    i = FiltInfoChain.count();
    while (i > 0)
    {
        i--;
        FI = FiltInfoChain.at(i);
        Opts = OptsList.at(i);
        if (FiltInfoChain.count() == 0)
        {
            for (FC = FI->formats;
                 FC->in != -1 && FC->out != ofmt && FC->in != inpixfmt; FC++)
                ;

            if (FC->in == -1)
                for (FC = FI->formats; FC->in != -1 && FC->in != inpixfmt; FC++)
                    ;

            if (FC->in == -1)
                for (FC = FI->formats; FC->in != -1 && FC->out != ofmt; FC++)
                    ;
        }
        else
            for (FC = FI->formats; FC->in != -1 && FC->out != ofmt; FC++)
                ;

        if (FC->in == -1)
            FC = FI->formats;

        if (FC->out != ofmt)
        {
            FiltInfoChain.insert(i + 1, Convert);
            OptsList.insert(i + 1, QString ());
            FmtList.insert(0, new FmtConv);
            if (FmtList.first())
            {
                FmtList.first()->in = FC->out;
                FmtList.first()->out = ofmt;
            }
            else
                return NULL;
        }
        FmtList.insert(0, new FmtConv);
        if (FmtList.first())
        {
            FmtList.first()->in = FC->in;
            FmtList.first()->out = FC->out;
        }
        else
            return NULL;

        ofmt = FC->in;
    }

    if (ofmt != inpixfmt)
    {
        FiltInfoChain.insert(0, Convert);
        OptsList.insert(0, QString());
        FmtList.insert(0, new FmtConv);
        if (FmtList.first())
        {
            FmtList.first()->in = inpixfmt;
            FmtList.first()->out = ofmt;
        }
        else
            delete FiltChain;

        return NULL;
    }

    if (!FiltInfoChain.count())
        return NULL;

    for (i = 0; i < FiltInfoChain.count(); i++)
    {
        if (FiltInfoChain.at(i) == NULL)
        {
            return NULL;
        }
        NewFilt = LoadFilter(FiltInfoChain.at(i), FmtList.at(i)->in,
                             FmtList.at(i)->out, postfilt_width, 
                             postfilt_height, OptsList.at(i));

        if (NewFilt)
            FiltChain->append(NewFilt);
        else
        {
            delete FiltChain;
        }
    }

    width = postfilt_width;
    height = postfilt_height;
    return FiltChain;
}

VideoFilter * FilterManager::LoadFilter(FilterInfo *FiltInfo, 
                                        VideoFrameType inpixfmt,
                                        VideoFrameType outpixfmt, int &width, 
                                        int &height, char *opts)
{
    void *handle;
    char *error;
    VideoFilter *Filter;
    VideoFilter *(*InitFilter)(int, int, int *, int *, char *);

    if (FiltInfo == NULL)
    {
        cerr << "FilterManager::LoadFilter: called with NULL FilterInfo"
             << endl;
        return NULL;
    }

    handle = dlopen(FiltInfo->libname, RTLD_NOW);

    if (!handle)
    {
        cerr << "FilterManager::LoadFilter: unable to load shared library "
             << FiltInfo->libname << endl;
        if ((error = dlerror()))
            cerr << "Dlopen error: " << error << endl;
        return NULL;
    }

    InitFilter =
        (VideoFilter * (*)(int, int, int *, int *, char *))dlsym(handle,
                                                                 FiltInfo->
                                                                 symbol);

    if ((error = dlerror()))
    {
        cerr << "FilterManager::LoadFilter: failed to load symbol "
             << FiltInfo->symbol << " from " << FiltInfo->libname << endl
             << "Dlopen error: " << error << endl;
        dlclose (handle);
        return NULL;
    }

    Filter = (*InitFilter)(inpixfmt, outpixfmt, &width, &height, opts);

    if (!Filter)
    {
        return NULL;
    }

    Filter->handle = handle;
    Filter->inpixfmt = inpixfmt;
    Filter->outpixfmt = outpixfmt;
    if (opts)
        Filter->opts = strdup(opts);
    else
        Filter->opts = NULL;
    Filter->info = FiltInfo;
    return Filter;
}

