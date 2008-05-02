#ifndef FILTERMANAGER
#define FILTERMANAGER

extern "C" {
#include "filter.h"
#include "frame.h"
}

// C++ headers
#include <vector>
#include <map>
using namespace std;

// Qt headers
#include <QString>

typedef map<QString,void*>       library_map_t;
typedef map<QString,FilterInfo*> filter_map_t;

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

    VideoFilter *LoadFilter(FilterInfo *Filt, VideoFrameType inpixfmt,
                            VideoFrameType outpixfmt, int &width,
                            int &height, char *opts);

    FilterChain *LoadFilters(QString filters, VideoFrameType &inpixfmt,
                             VideoFrameType &outpixfmt, int &width,
                             int &height, int &bufsize);

  private:
    bool LoadFilterLib(const QString &path);
    const FilterInfo *GetFilterInfo(const QString &name) const;

    library_map_t dlhandles;
    filter_map_t  filters;
};

#endif // #ifndef FILTERMANAGER

