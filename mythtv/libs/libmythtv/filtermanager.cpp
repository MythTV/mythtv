#include <dlfcn.h>
#include <iostream>
#include <qdir.h>
#include <qstringlist.h>
#include <qstrlist.h>

using namespace std;
#include "filtermanager.h"
#include "mythcontext.h"

static const char *FmtToString(VideoFrameType ft)
{
    switch(ft)
    {
        case FMT_NONE:
            return "NONE";
        case FMT_RGB24:
            return "RGB24";
        case FMT_YV12:
            return "YV12";
        case FMT_ARGB32:
            return "ARGB32";
        case FMT_YUV422P:
            return "YUV422P";
        default:
            return "INVALID";
    }
}

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
        delete [] (i.current()->formats);
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
                                        int &height, int &bufsize)
{
    QPtrList<FilterInfo> FiltInfoChain;
    FilterChain *FiltChain = new FilterChain;
    QPtrList<FmtConv> FmtList;
    FilterInfo *FI, *FI2;
    QString Opts;
    FilterInfo *Convert = GetFilterInfoByName("convert");
    QStrList OptsList (TRUE);
    QStringList FilterList = QStringList::split(",", Filters);
    VideoFilter *NewFilt = NULL;
    FmtConv *FC, *FC2;
    VideoFrameType ofmt;
    unsigned int i;
    int nbufsize;
    int cbufsize;
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

        if (FI)
        {
            FiltInfoChain.append(FI);
            OptsList.append(FiltOpts);
        }
        else
        {
            VERBOSE(VB_IMPORTANT,QString("FilterManager: failed to load "
                    "filter '%1'").arg(FiltName));
            FiltInfoChain.clear();
            break;
        }
    }
    if (FiltInfoChain.count())
    {
        if (inpixfmt == -1)
            inpixfmt = FiltInfoChain.first()->formats[0].in;
        if (outpixfmt == -1)
            outpixfmt = FiltInfoChain.last()->formats[0].out;
    }
    else if (inpixfmt == -1)
        inpixfmt = outpixfmt;
    else if (outpixfmt == -1)
        outpixfmt = inpixfmt;
    ofmt = outpixfmt;

    i = FiltInfoChain.count();
    while (i > 0)
    {
        i--;
        FI = FiltInfoChain.at(i);
        Opts = OptsList.at(i);
        if (!i)
        {
            for (FC = FI->formats; FC->in != -1; FC++)
                if (FC->out == ofmt && FC->in == inpixfmt)
                    break;

            if (FC->in == -1)
                for (FC = FI->formats; FC->in != -1; FC++)
                    if (FC->in == inpixfmt)
                        break;
        }
        else
        {
            FI2 = FiltInfoChain.at(i-1);
            for (FC = FI->formats; FC->in != -1; FC++)
            {
                for (FC2 = FI2->formats; FC2->in != -1; FC2++)
                    if (FC2->out == FC->in)
                        break;
                if (FC2->out == FC->in)
                    break;
            }
        }
        if (FC->in == -1)
            for (FC = FI->formats; FC->in != -1; FC++)
                if (FC->out == ofmt)
                    break;

        if (FC->in == -1)
            FC = FI->formats;

        if (FC->out != ofmt)
        {
            if (!Convert)
            {
                VERBOSE(VB_IMPORTANT,"FilterManager: format conversion needed "
                        "but convert filter not found");
                FiltInfoChain.clear();
                break;
            }
            FiltInfoChain.insert(i + 1, Convert);
            OptsList.insert(i + 1, QString ());
            FmtList.insert(0, new FmtConv);
            if (FmtList.first())
            {
                FmtList.first()->in = FC->out;
                FmtList.first()->out = ofmt;
            }
            else
            {
                FiltInfoChain.clear();
                break;
            }
        }
        FmtList.insert(0, new FmtConv);
        if (FmtList.first())
        {
            FmtList.first()->in = FC->in;
            FmtList.first()->out = FC->out;
        }
        else
        {
            FiltInfoChain.clear();
            break;
        }

        ofmt = FC->in;
    }

    if (ofmt != inpixfmt)
    {
        if (!Convert)
        {
            VERBOSE(VB_IMPORTANT,"FilterManager: format conversion needed but "
                    "convert filter not found");
            FiltInfoChain.clear();
        }
        else
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
                FiltInfoChain.clear();
        }
    }

    switch (inpixfmt)
    {
        case FMT_YV12:
            bufsize = postfilt_width * postfilt_height * 3 / 2;
            break;
        case FMT_YUV422P:
            bufsize = postfilt_width * postfilt_height * 2;
            break;
        case FMT_RGB24:
            bufsize = postfilt_width * postfilt_height * 3;
            break;
        case FMT_ARGB32:
            bufsize = postfilt_width * postfilt_height * 4;
            break;
        default:
            bufsize = 0;
    }
    nbufsize = bufsize;

    if (!FiltInfoChain.count())
    {
        delete FiltChain;
        FiltChain = NULL;
    }
    
    for (i = 0; i < FiltInfoChain.count(); i++)
    {
        NewFilt = LoadFilter(FiltInfoChain.at(i), FmtList.at(i)->in,
                             FmtList.at(i)->out, postfilt_width, 
                             postfilt_height, OptsList.at(i));

        if (!NewFilt)
        {
            delete FiltChain;
            VERBOSE(VB_IMPORTANT,QString("FilterManager: failed to load "
                        "filter %1 %2->%3 with args %4")
                    .arg(FiltInfoChain.at(i)->name)
                    .arg(FmtToString(FmtList.at(i)->in))
                    .arg(FmtToString(FmtList.at(i)->out))
                    .arg(OptsList.at(i)?OptsList.at(i):"NULL")
                   );
            FiltChain = NULL;
            break;
        }

        if (NewFilt->filter)
            FiltChain->append(NewFilt);
        else
        {
            if (NewFilt->opts)
                free(NewFilt->opts);
            if (NewFilt->cleanup)
                NewFilt->cleanup(NewFilt);
            dlclose(NewFilt->handle);
            free(NewFilt);
        }

        switch (inpixfmt)
        {
            case FMT_YV12:
                cbufsize = postfilt_width * postfilt_height * 3 / 2;
                break;
            case FMT_YUV422P:
                cbufsize = postfilt_width * postfilt_height * 2;
                break;
            case FMT_RGB24:
                cbufsize = postfilt_width * postfilt_height * 3;
                break;
            case FMT_ARGB32:
                cbufsize = postfilt_width * postfilt_height * 4;
                break;
            default:
                cbufsize = 0;
        }

        if (cbufsize > nbufsize)
            nbufsize = cbufsize;
    }

    if (FiltChain)
    {
        nbufsize = cbufsize;
        width = postfilt_width;
        height = postfilt_height;
    }

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

