#ifndef FILTERMANAGER
#define FILTERMANAGER

extern "C" {
#include "filter.h"
#include "frame.h"
}

#include <qdict.h>
#include <qptrlist.h>
#include <qstring.h>
#include <cstdlib>
#include <dlfcn.h>

using namespace std;

class FilterChain : public QPtrList<VideoFilter>
{
  public:
    FilterChain();

    virtual ~FilterChain();

    void ProcessFrame(VideoFrame *Frame);

  private:
    void deleteItem(QPtrCollection::Item d);
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

    QPtrList <FilterInfo> GetAllFilterInfo()
    {
        return QPtrList<FilterInfo>(Filters);
    }


  private:
    QPtrList<FilterInfo> Filters;
    QDict<FilterInfo> FilterByName;
};

#endif // #ifndef FILTERMANAGER

