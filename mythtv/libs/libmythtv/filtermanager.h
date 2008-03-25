#ifndef FILTERMANAGER
#define FILTERMANAGER

extern "C" {
#include "filter.h"
#include "frame.h"
}

#include <q3dict.h>
#include <q3ptrlist.h>
#include <qstring.h>

using namespace std;

class FilterChain : public Q3PtrList<VideoFilter>
{
  public:
    FilterChain();

    virtual ~FilterChain();

    void ProcessFrame(VideoFrame *Frame);

  private:
    void deleteItem(Q3PtrCollection::Item d);
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
                             int &height, int &bufsize);

    FilterInfo *GetFilterInfoByName(QString name)
    {
        return FilterByName.find(name);
    }

    Q3PtrList <FilterInfo> GetAllFilterInfo()
    {
        return Q3PtrList<FilterInfo>(Filters);
    }


  private:
    Q3PtrList<FilterInfo> Filters;
    Q3Dict<FilterInfo> FilterByName;
};

#endif // #ifndef FILTERMANAGER

