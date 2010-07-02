
// Own header
#include "DisplayResScreen.h"

// QT headers
#include <QStringList>

// C++ headers
#include <cmath>

DisplayResScreen::DisplayResScreen(int w, int h, int mw, int mh,
                                   double aspectRatio, double refreshRate)
    : width(w), height(h), width_mm(mw), height_mm(mh), custom(false)
{
    SetAspectRatio(aspectRatio);
    if (refreshRate > 0)
        refreshRates.push_back(refreshRate);
}

DisplayResScreen::DisplayResScreen(int w, int h, int mw, int mh,
                                   const std::vector<double>& rr)
    : width(w), height(h), width_mm(mw), height_mm(mh), refreshRates(rr), custom(false)
{
    SetAspectRatio(-1.0);
}

DisplayResScreen::DisplayResScreen(int w, int h, int mw, int mh,
                                   const std::vector<double>& rr,
                                   const std::map<double, short>& rr2)
: realRates(rr2),   width(w),    height(h), width_mm(mw), height_mm(mh),
  refreshRates(rr), custom(true)
{
    SetAspectRatio(-1.0);
}

DisplayResScreen::DisplayResScreen(int w, int h, int mw, int mh,
                                   const double* rr, uint rr_length)
    : width(w), height(h), width_mm(mw), height_mm(mh), custom(false)
{
    SetAspectRatio(-1.0);
    for (uint i = 0; i < rr_length; ++i)
        refreshRates.push_back(rr[i]);

    std::sort(refreshRates.begin(), refreshRates.end());
}

DisplayResScreen::DisplayResScreen(int w, int h, int mw, int mh,
                                   const short* rr, uint rr_length)
    : width(w), height(h), width_mm(mw), height_mm(mh), custom(false)
{
    SetAspectRatio(-1.0);
    for (uint i = 0; i < rr_length; ++i)
        refreshRates.push_back((double)rr[i]);
    std::sort(refreshRates.begin(), refreshRates.end());
}

DisplayResScreen::DisplayResScreen(const QString &str)
    : width(0), height(0), width_mm(0), height_mm(0), aspect(-1.0), custom(false)
{
    refreshRates.clear();
    QStringList slist = str.split(':');
    if (slist.size()<4)
        slist = str.split(','); // for backward compatibility
    if (slist.size() >= 4)
    {
        width = slist[0].toInt();
        height = slist[1].toInt();
        width_mm = slist[2].toInt();
        height_mm = slist[3].toInt();
        aspect = slist[4].toDouble();
        for (int i = 5; i<slist.size(); ++i)
            refreshRates.push_back(slist[i].toDouble());
    }
}

void DisplayResScreen::SetAspectRatio(double a)
{
    if (a>0.0)
        aspect = a;
    else if (Height_mm())
        aspect = ((double)(Width_mm())) / ((double)(Height_mm()));
}

QString DisplayResScreen::toString() const
{
    QString str = QString("%1:%2:%3:%4:%5")
        .arg(width).arg(height).arg(width_mm).arg(height_mm).arg(aspect);
    for (uint i=0; i<refreshRates.size(); ++i)
        str.append(QString(":%1").arg(refreshRates[i]));
    return str;
}

QStringList DisplayResScreen::Convert(const DisplayResVector& dsr)
{
    QStringList slist;
    for (uint i=0; i<dsr.size(); ++i)
        slist += dsr[i].toString();
    return slist;
}

DisplayResVector DisplayResScreen::Convert(const QStringList& slist)
{
    std::vector<DisplayResScreen> dsr;
    for (int i=0; i<slist.size(); ++i)
        dsr.push_back(DisplayResScreen(slist[i]));
    return dsr;
}

//compares if the double f1 is equal with f2 and returns 1 if true and 0 if false
bool DisplayResScreen::compare_rates(double f1, double f2, double precision)
{
    if (((f1 - precision) < f2) && 
        ((f1 + precision) > f2))
        return true;
    else
        return false;
}

int DisplayResScreen::FindBestMatch(const DisplayResVector& dsr,
                                    const DisplayResScreen& d,
                                    double& target_rate)
{
    double videorate = d.RefreshRate();
    bool rate2x = false;
    bool end = false;

    // We will give priority to refresh rates that are twice what is looked for
    if ((videorate > 24.5) && (videorate < 30.5))
    {
        rate2x = true;
        videorate *= 2.0;
    }
   
    // Amend vector with custom list
    for (uint i=0; i<dsr.size(); ++i)
    {
        if (dsr[i].Width()==d.Width() && dsr[i].Height()==d.Height())
        {
            const std::vector<double>& rates = dsr[i].RefreshRates();
            if (rates.size() && videorate != 0)
            {
                while (!end)
                {
                    for (double precision = 0.001;
                         precision < 1.0;
                         precision *= 10.0)
                    {
                        for (uint j=0; j < rates.size(); ++j)
                        {
                            // Multiple of target_rate will do
                            if (compare_rates(videorate,rates[j], precision) ||
                                (fabs(videorate - fmod(rates[j],videorate))
                                 <= precision) ||
                                (fmod(rates[j],videorate) <= precision))
                            {
                                target_rate = rates[j];
                                return i;
                            }
                        }
                    }
                    // Can't find exact frame rate, so try rounding to the
                    // nearest integer, so 23.97Hz will work with 24Hz etc
                    for (double precision = 0.01;
                         precision < 2.0;
                         precision *= 10.0)
                    {
                        double rounded = (double) ((int) (videorate + 0.5));
                        for (uint j=0; j < rates.size(); ++j)
                        {
                            // Multiple of target_rate will do
                            if (compare_rates(rounded,rates[j], precision) ||
                                (fabs(rounded - fmod(rates[j],rounded))
                                 <= precision) ||
                                (fmod(rates[j],rounded) <= precision))
                            {
                                target_rate = rates[j];
                                return i;
                            }
                        }
                    }
                    if (rate2x)
                    {
                        videorate /= 2.0;
                        rate2x = false;
                    }
                    else
                        end = true;
                }
                target_rate = rates[rates.size() - 1];
            }
            return i;
        }
    }
    return -1;
}

#define extract_key(key) { \
        r = (key & ((1<<18) - 1)) / 1000.0; \
        h = (key >> 18) & ((1<<16) - 1); \
        w = (key >> 34) & ((1<<16) - 1); }

uint64_t DisplayResScreen::FindBestScreen(const DisplayResMap& resmap,
                                          int iwidth, int iheight, double frate)
{
    DisplayResMapCIt it;
    int w, h;
    double r;

        // 1. search for exact match (width, height, rate)
        // 2. search for resolution, ignoring rate
        // 3. search for matching height and rate (or any rate if rate = 0)
        // 4. search for 2x rate
        // 5. search for 1x rate
    for (it = resmap.begin(); it != resmap.end(); it++)
    {
        extract_key(it->first);
        if (w == iwidth && h == iheight && compare_rates(frate, r, 0.01))
            return it->first;
    }
    for (it = resmap.begin(); it != resmap.end(); it++)
    {
        extract_key(it->first);
        if (w == iwidth && h == iheight && r == 0)
            return it->first;
    }
    for (it = resmap.begin(); it != resmap.end(); it++)
    {
        extract_key(it->first);
        if ((w == 0 && h == iheight &&
             (compare_rates(frate, r, 0.01) || r == 0)) ||
            (w == 0 && h == 0 && compare_rates(frate, r * 2.0, 0.01)) ||
            (w == 0 && h == 0 && compare_rates(frate, r, 0.01)))
        {
            return it->first;
        }
    }
    // not found
    return 0;
}
