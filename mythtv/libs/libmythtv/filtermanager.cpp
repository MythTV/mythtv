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
    QDir FiltDir(gContext->GetFiltersDir());
    QString Path;

    FiltDir.setFilter(QDir::Files | QDir::Readable);
    if (FiltDir.exists())
    {
        QStringList LibList = FiltDir.entryList();
        for (QStringList::iterator i = LibList.begin(); i != LibList.end();
             i++)
        {
            Path = FiltDir.filePath(*i);
            if (Path.length() > 1)
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

                for (i = 0; FiltInfo->formats[i].in != FMT_NONE; i++)
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
    FmtConv *FC, *FC2, *S1, *S2, *S3;
    VideoFrameType ifmt;
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
                    "filter '%1', no such filter exists").arg(FiltName));
            FiltInfoChain.clear();
            break;
        }
    }

    ifmt = inpixfmt;
    for (i = 0; i < FiltInfoChain.count(); i++)
    {
        S1 = S2 = S3 = NULL;
        FI = FiltInfoChain.at(i);
        if (FiltInfoChain.count() - i == 1)
        {
            for (FC = FI->formats; FC->in != FMT_NONE; FC++)
            {
                if (FC->out == outpixfmt && FC->in == ifmt)
                {
                    S1 = FC;
                    break;
                }
                if (FC->in == ifmt && !S2)
                    S2 = FC;
                if (FC->out == outpixfmt && !S3)
                    S3 = FC;
            }
        }
        else
        {
            FI2 = FiltInfoChain.at(i+1);
            for (FC = FI->formats; FC->in != FMT_NONE; FC++)
            {
                for (FC2 = FI2->formats; FC2->in != FMT_NONE; FC2++)
                {
                    if (FC->in == ifmt && FC->out == FC2->in)
                    {
                        S1 = FC;
                        break;
                    }
                    if (FC->out == FC2->in && !S3)
                        S3 = FC;
                }
                if (S1)
                    break;
                if (FC->in == ifmt && !S2)
                    S2 = FC;
            }
        }
                
        if (S1)
            FC = S1;
        else if (S2)
            FC = S2;
        else if (S3)
            FC = S3;
        else
            FC = FI->formats;

        if (FC->in != ifmt && (i > 0 || ifmt != FMT_NONE))
        {
            if (!Convert)
            {
                VERBOSE(VB_IMPORTANT, "FilterManager: format conversion "
                        "needed but convert filter not found");
                FiltInfoChain.clear();
                break;
            }
            FiltInfoChain.insert(i, Convert);
            OptsList.insert(i, QString ());
            FmtList.append(new FmtConv);
            if (FmtList.last())
            {
                FmtList.last()->in = ifmt;
                FmtList.last()->out = FC->in;
                i++;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "FilterManager: memory allocation "
                        "failure, returning empty filter chain");
                FiltInfoChain.clear();
                break;
            }
        }
        FmtList.append(new FmtConv);
        if (FmtList.last())
        {
            FmtList.last()->in = FC->in;
            FmtList.last()->out = FC->out;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "FilterManager: memory allocation failure, "
                    "returning empty filter chain");
            FiltInfoChain.clear();
            break;
        }
        ifmt = FC->out;
    }

    if (ifmt != outpixfmt && outpixfmt != FMT_NONE &&
        (FiltInfoChain.count() || inpixfmt != FMT_NONE))
    {
        if (!Convert)
        {
            VERBOSE(VB_IMPORTANT, "FilterManager: format conversion "
                    "needed but convert filter not found");
            FiltInfoChain.clear();
        }
        else
        {
            FiltInfoChain.append(Convert);
            OptsList.append( QString ());
            FmtList.append(new FmtConv);
            if (FmtList.last())
            {
                FmtList.last()->in = ifmt;
                FmtList.last()->out = outpixfmt;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "FilterManager: memory allocation "
                        "failure, returning empty filter chain");
                FiltInfoChain.clear();
            }
        }
    }

    nbufsize = -1;

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
            nbufsize = -1;
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

        switch (FmtList.at(i)->out)
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
        if (inpixfmt == FMT_NONE)
            inpixfmt = FmtList.first()->in;
        if (outpixfmt == FMT_NONE)
            inpixfmt = FmtList.last()->out;
        width = postfilt_width;
        height = postfilt_height;
    }
    else
    {
        if (inpixfmt == FMT_NONE && outpixfmt == FMT_NONE)
            inpixfmt = outpixfmt = FMT_YV12;
        else if (inpixfmt == FMT_NONE)
            inpixfmt = outpixfmt;
        else if (outpixfmt == FMT_NONE)
            outpixfmt = inpixfmt;
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

    bufsize = nbufsize;

    return FiltChain;
}

VideoFilter * FilterManager::LoadFilter(FilterInfo *FiltInfo, 
                                        VideoFrameType inpixfmt,
                                        VideoFrameType outpixfmt, int &width, 
                                        int &height, char *opts)
{
    void *handle;
    const char *error;
    VideoFilter *Filter;
    VideoFilter *(*InitFilter)(int, int, int *, int *, char *);

    if (FiltInfo == NULL)
    {
        VERBOSE(VB_IMPORTANT, "FilterManager: LoadFilter called with NULL"
                "FilterInfo");
        return NULL;
    }

    if (FiltInfo->libname == NULL)
    {
        VERBOSE(VB_IMPORTANT, "FilterManager: LoadFilter called with invalid "
                "FilterInfo (libname is NULL)");
        return NULL;
    }

    if (FiltInfo->symbol == NULL)
    {
        VERBOSE(VB_IMPORTANT, "FilterManager: LoadFilter called with invalid "
                "FilterInfo (symbol is NULL)");
        return NULL;
    }

    handle = dlopen(FiltInfo->libname, RTLD_NOW);

    if ((error = dlerror()))
    {
        VERBOSE(VB_IMPORTANT, QString("FilterManager: unable to load "
                "shared library '%1', dlopen reports error '%2'")
                .arg(FiltInfo->libname)
                .arg(error));
        return NULL;
    }

    if (handle == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString("FilterManager: dlopen did not report "
                "an error, but returned a NULL handle for shared library '%1'")
                .arg(FiltInfo->libname));
        return NULL;
    }

    InitFilter =
        (VideoFilter * (*)(int, int, int *, int *, char *))dlsym(handle,
                                                                 FiltInfo->
                                                                 symbol);

    if ((error = dlerror()))
    {
        VERBOSE(VB_IMPORTANT, QString("FilterManager: unable to load symbol "
                "'%1' from shared library '%2', dlopen reports error '%3'")
                .arg(FiltInfo->symbol)
                .arg(FiltInfo->libname)
                .arg(error));
        return NULL;
    }

    if (InitFilter == NULL)
    {
        VERBOSE(VB_IMPORTANT, QString("FilterManager: dlopen did not report "
                "an error, but returned NULL for symbol '%1' from shared "
                "library '%2'")
                .arg(FiltInfo->symbol)
		.arg(FiltInfo->libname));
        return NULL;
    }

    Filter = (*InitFilter)(inpixfmt, outpixfmt, &width, &height, opts);

    if (Filter == NULL)
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
