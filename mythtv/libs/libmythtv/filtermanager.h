#ifndef FILTERMANAGER
#define FILTERMANAGER

#include "filter.h"
#include "mythframe.h"

// C++ headers
#include <vector>
#include <map>
using namespace std;

// Qt headers
#include <QString>

typedef map<QString,void*>       library_map_t;
typedef map<QString,FilterInfo*> filter_map_t;

#include "videoouttypes.h"

class FilterChain
{
  public:
    FilterChain() { }
    virtual ~FilterChain();

    void ProcessFrame(VideoFrame *Frame, FrameScanType scan = kScan_Ignore);

    void Append(VideoFilter *f) { filters.push_back(f); }

  private:
    vector<VideoFilter*> filters;
};

class FilterManager
{
  public:
    FilterManager();
   ~FilterManager();

    VideoFilter *LoadFilter(const FilterInfo *Filt, VideoFrameType inpixfmt,
                            VideoFrameType outpixfmt, int &width,
                            int &height, const char *opts,
                            int max_threads);

    FilterChain *LoadFilters(QString filters, VideoFrameType &inpixfmt,
                             VideoFrameType &outpixfmt, int &width,
                             int &height, int &bufsize,
                             int max_threads = 1);

  private:
    bool LoadFilterLib(const QString &path);
    const FilterInfo *GetFilterInfo(const QString &name) const;

    library_map_t dlhandles;
    filter_map_t  filters;
};

#endif // #ifndef FILTERMANAGER

