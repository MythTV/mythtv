#ifndef FILTERMANAGER
#define FILTERMANAGER

#include "filter.h"
#include "frame.h"
#include <qdict.h>
#include <qptrlist.h>
#include <qstring.h>
#include <stdlib.h>
#include <dlfcn.h>

using namespace std;

class FilterChain : public QPtrList<VideoFilter>
{
  public:
    FilterChain() { setAutoDelete(TRUE); }

    virtual ~FilterChain() { clear(); }

    void ProcessFrame(VideoFrame * Frame)
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

  private:

    void deleteItem(QPtrCollection::Item d)
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
};

class FilterManager
{
  public:
    FilterManager();
   ~FilterManager();

    void LoadFilterLib(QString Path);
    VideoFilter *LoadFilter(FilterInfo *Filt, VideoFrameType inpixfmt,
                            VideoFrameType outpixfmt, int &width,
                            int &height, char *opts);

    FilterChain *LoadFilters(QString filters, VideoFrameType &inpixfmt,
                             VideoFrameType &outpixfmt, int &width,
                             int &height);

    FilterInfo *GetFilterInfoByName(QString name)
    {
        return FilterByName.find(name);
    }

    QPtrList <FilterInfo> GetAllFilterInfo()
    {
        return QPtrList<FilterInfo>(Filters);
    }


  private:
    QPtrList<FilterInfo> Filters;
    QDict<FilterInfo> FilterByName;
};

#endif // #ifndef FILTERMANAGER

