#include "DisplayResScreen.h"
#include "mythcontext.h"

DisplayResScreen::DisplayResScreen(int w, int h, int mw, int mh,
                                   double aspectRatio, short refreshRate)
    : width(w), height(h), width_mm(mw), height_mm(mh)
{
    SetAspectRatio(aspectRatio);
    if (refreshRate > 0)
        refreshRates.push_back(refreshRate);
}

DisplayResScreen::DisplayResScreen(int w, int h, int mw, int mh,
                                   const vector<short>& rr)
    : width(w), height(h), width_mm(mw), height_mm(mh), refreshRates(rr)
{
    SetAspectRatio(-1.0);
}

DisplayResScreen::DisplayResScreen(int w, int h, int mw, int mh,
                                   const short* rr, uint rr_length)
    : width(w), height(h), width_mm(mw), height_mm(mh)
{
    SetAspectRatio(-1.0);
    for (uint i = 0; i < rr_length; ++i)
        refreshRates.push_back(rr[i]);

    sort(refreshRates.begin(), refreshRates.end());
}

DisplayResScreen::DisplayResScreen(const QString &str)
    : width(0), height(0), width_mm(0), height_mm(0), aspect(-1.0)
{
    refreshRates.clear();
    QStringList slist = QStringList::split(":", str);
    if (slist.size()<4)
        slist = QStringList::split(",", str); // for backward compatibility
    if (slist.size() >= 4)
    {
        width = slist[0].toInt();
        height = slist[1].toInt();
        width_mm = slist[2].toInt();
        height_mm = slist[3].toInt();
        aspect = slist[4].toDouble();
        for (uint i = 5; i<slist.size(); ++i)
            refreshRates.push_back(slist[i].toShort());
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

QStringList DisplayResScreen::Convert(const vector<DisplayResScreen>& dsr)
{
    QStringList slist;
    for (uint i=0; i<dsr.size(); ++i)
        slist += dsr[i].toString();
    return slist;
}

DisplayResVector DisplayResScreen::Convert(const QStringList& slist)
{
    vector<DisplayResScreen> dsr;
    for (uint i=0; i<slist.size(); ++i)
        dsr.push_back(DisplayResScreen(slist[i]));
    return dsr;
}

int DisplayResScreen::FindBestMatch(const vector<DisplayResScreen>& dsr,
                                    const DisplayResScreen& d,
                                    short& target_rate)
{
    for (uint i=0; i<dsr.size(); ++i)
    {
        if (dsr[i].Width()==d.Width() && dsr[i].Height()==d.Height())
        {
            const vector<short>& rates = dsr[i].RefreshRates();
            if (rates.size())
            {
                vector<short>::const_iterator it =
                    find(rates.begin(), rates.end(), d.RefreshRate());
                target_rate = (it == rates.end()) ? *(--rates.end()) : *it;
                return i;
            }
        }
    }
    return -1;
}
