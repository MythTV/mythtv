#ifndef __DISPLAYRESCREEN_H__
#define __DISPLAYRESCREEN_H__

using namespace std;

#include <qstringlist.h>
#include <vector>
#include <map>

class DisplayResScreen
{
  public:
    // Constructors, initializers
    DisplayResScreen()
        : width(0), height(0), width_mm(0), height_mm(0), aspect(-1.0) {;}
    DisplayResScreen(int w, int h, int mw, int mh,
                     double aspectRatio/* = -1.0*/, short refreshRate/* = 0*/);
    DisplayResScreen(int w, int h, int mw, int mh,
                     const vector<short>& refreshRates);
    DisplayResScreen(int w, int h, int mw, int mh,
                     const short* refreshRates, uint rr_length);
    DisplayResScreen(const QString &str);
    inline void Init();

    // Gets
    int Width() const { return width; }
    int Height() const { return height; }
    int Width_mm() const { return width; }
    int Height_mm() const { return height; }
    inline double AspectRatio() const;
    inline short RefreshRate() const;
    const vector<short>& RefreshRates() const { return refreshRates; }

    // Sets, adds
    void SetAspectRatio(double a);
    void AddRefreshRate(short rr) {
        refreshRates.push_back(rr);
        sort(refreshRates.begin(), refreshRates.end());
    }

    // Converters & comparitors
    QString toString() const;
    inline bool operator < (const DisplayResScreen& b) const;
    inline bool operator == (const DisplayResScreen &b) const;

    // Statics
    static QStringList Convert(const vector<DisplayResScreen>& dsr);
    static vector<DisplayResScreen> Convert(const QStringList& slist);
    static int FindBestMatch(const vector<DisplayResScreen>& dsr,
                             const DisplayResScreen& d,
                             short& target_rate);
    static inline int CalcKey(int w, int h, int rate);

  private:
    int width, height; // size in pixels
    int width_mm, height_mm; // physical size in millimeters
    double aspect; // aspect ratio, calculated or set
    vector<short> refreshRates;
};

typedef vector<DisplayResScreen>          DisplayResVector;
typedef DisplayResVector::iterator        DisplayResVectorIt;
typedef DisplayResVector::const_iterator  DisplayResVectorCIt;

typedef map<uint, class DisplayResScreen> DisplayResMap;
typedef DisplayResMap::iterator           DisplayResMapIt;
typedef DisplayResMap::const_iterator     DisplayResMapCIt;

inline void DisplayResScreen::Init()
{
    width = height = width_mm = height_mm = 0;
    aspect = -1.0;
}

inline double DisplayResScreen::AspectRatio() const
{
    if (aspect<=0.0)
    {
        if (0 == height_mm)
            return 1.0;
        return ((double)(width_mm))/((double)(height_mm));
    }
    return aspect;
}

inline short DisplayResScreen::RefreshRate() const
{
    if (refreshRates.size() >= 1)
        return refreshRates[0];
    else return 0;
}

inline bool DisplayResScreen::operator < (const DisplayResScreen& b) const
{
    if (width < b.width)
        return true;
    if (height < b.height)
        return true;
    return false;
}
            
inline bool DisplayResScreen::operator == (const DisplayResScreen &b) const
{
    return width == b.width && height == b.height;
}  

inline int DisplayResScreen::CalcKey(int w, int h, int rate)
{
    return (w << 17) | (h << 3) | rate;
}

#endif // __DISPLAYRESCREEN_H__
