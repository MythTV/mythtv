#ifndef FILTERMANAGER
#define FILTERMANAGER

#include "filter.h"
#include "mythframe.h"

// C++ headers
#include <map>
#include <vector>
using namespace std;

// Qt headers
#include <QString>

typedef map<QString,void*>       library_map_t;
typedef map<QString,FilterInfo*> filter_map_t;

#include "videoouttypes.h"

class FilterChain
{
  public:
    FilterChain() = default;
    virtual ~FilterChain();

    void ProcessFrame(VideoFrame *Frame, FrameScanType scan = kScan_Ignore);

    void Append(VideoFilter *f) { m_filters.push_back(f); }

  private:
    vector<VideoFilter*> m_filters;
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

    FilterChain *LoadFilters(const QString& filters, VideoFrameType &inpixfmt,
                             VideoFrameType &outpixfmt, int &width,
                             int &height, int &bufsize,
                             int max_threads = 1);

    const FilterInfo *GetFilterInfo(const QString &name) const;

  private:
    bool LoadFilterLib(const QString &path);

    library_map_t m_dlhandles;
    filter_map_t  m_filters;
};

#endif // #ifndef FILTERMANAGER

