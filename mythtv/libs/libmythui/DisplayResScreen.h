#ifndef __DISPLAYRESCREEN_H__
#define __DISPLAYRESCREEN_H__

#include <algorithm>
#include <cstdint>   // for uint64_t
#include <map>
#include <vector>

#include <QString>

#include "mythuiexp.h"

class DisplayResScreen;

typedef std::vector<DisplayResScreen>     DisplayResVector;
typedef DisplayResVector::iterator        DisplayResVectorIt;
typedef DisplayResVector::const_iterator  DisplayResVectorCIt;

typedef std::map<uint64_t, DisplayResScreen> DisplayResMap;
typedef DisplayResMap::iterator           DisplayResMapIt;
typedef DisplayResMap::const_iterator     DisplayResMapCIt;

class MUI_PUBLIC DisplayResScreen
{
  public:
    // Constructors, initializers
    DisplayResScreen() = default;
    DisplayResScreen(int w, int h, int mw, int mh,
                     double aspectRatio/* = -1.0*/, double refreshRate/* = 0*/);
    DisplayResScreen(int w, int h, int mw, int mh,
                     std::vector<double> refreshRates);
    DisplayResScreen(int w, int h, int mw, int mh,
                     std::vector<double> refreshRates,
                     std::map<double, short> realrates);
    DisplayResScreen(int w, int h, int mw, int mh,
                     const double* refreshRates, uint rr_length);
    DisplayResScreen(int w, int h, int mw, int mh,
                     const short* refreshRates, uint rr_length);
    explicit DisplayResScreen(const QString &str);
    inline void Init();

    // Gets
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    int Width_mm() const { return m_width_mm; }
    int Height_mm() const { return m_height_mm; }
    bool Custom() const { return m_custom; }

    inline double AspectRatio() const;
    inline double RefreshRate() const;
    const std::vector<double>& RefreshRates() const { return m_refreshRates; }

    // Sets, adds
    void SetAspectRatio(double a);
    void AddRefreshRate(double rr)
    {
        m_refreshRates.push_back(rr);
        std::sort(m_refreshRates.begin(), m_refreshRates.end());
    }
    void ClearRefreshRates(void) { m_refreshRates.clear(); }
    void SetRefreshRate(double rr) 
    {
        ClearRefreshRates();
        AddRefreshRate(rr);
    }

    void SetCustom(bool b) { m_custom = b; }

    // Map for matching real rates and xrandr rate;
    std::map<double, short> realRates;

    // Converters & comparitors
    QString toString() const;
    inline bool operator < (const DisplayResScreen& b) const;
    inline bool operator == (const DisplayResScreen &b) const;

    // Statics
    static QStringList Convert(const DisplayResVector& dsr);
    static DisplayResVector Convert(const QStringList& slist);
    static int FindBestMatch(const DisplayResVector& dsr,
                             const DisplayResScreen& d,
                             double& target_rate);
    static inline uint64_t CalcKey(int w, int h, double rate);
    static bool compare_rates(double f1, double f2, double precision = 0.01);
    static uint64_t FindBestScreen(const DisplayResMap& resmap,
                                   int iwidth, int iheight, double frate);

  private:
  int m_width     {0};     // size in pixels
  int m_height    {0};     // size in pixels
  int m_width_mm  {0};     // physical size in millimeters
  int m_height_mm {0};     // physical size in millimeters
  double m_aspect {-1.0};  // aspect ratio, calculated or set
  std::vector<double> m_refreshRates;
  bool m_custom   {false}; // Set if resolution was defined manually
};

inline void DisplayResScreen::Init()
{
    m_width = m_height = m_width_mm = m_height_mm = 0;
    m_aspect = -1.0;
}

inline double DisplayResScreen::AspectRatio() const
{
    if (m_aspect<=0.0)
    {
        if (0 == m_height_mm)
            return 1.0;
        return ((double)(m_width_mm))/((double)(m_height_mm));
    }
    return m_aspect;
}

inline double DisplayResScreen::RefreshRate() const
{
    if (m_refreshRates.size() >= 1)
        return m_refreshRates[0];
    else return 0.0;
}

inline bool DisplayResScreen::operator < (const DisplayResScreen& b) const
{
    if (m_width < b.m_width)
        return true;
    if (m_height < b.m_height)
        return true;
    return false;
}
            
inline bool DisplayResScreen::operator == (const DisplayResScreen &b) const
{
    return m_width == b.m_width && m_height == b.m_height;
}  

inline uint64_t DisplayResScreen::CalcKey(int w, int h, double rate)
{
    uint64_t irate = (uint64_t) (rate * 1000.0);
    return ((uint64_t)w << 34) | ((uint64_t)h << 18) | irate;
}

#endif // __DISPLAYRESCREEN_H__
