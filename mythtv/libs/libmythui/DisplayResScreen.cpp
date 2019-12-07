// QT
#include <QStringList>

// MythTV
#include "DisplayResScreen.h"

// Std
#include <cmath>

DisplayResScreen::DisplayResScreen(int Width, int Height, int MMWidth, int MMHeight,
                                   double AspectRatio, double RefreshRate)
  : m_width(Width),
    m_height(Height),
    m_widthMM(MMWidth),
    m_heightMM(MMHeight)
{
    SetAspectRatio(AspectRatio);
    if (RefreshRate > 0)
        m_refreshRates.push_back(RefreshRate);
}

bool DisplayResScreen::operator < (const DisplayResScreen& Other) const
{
    if (m_width < Other.m_width)
        return true;
    if (m_height < Other.m_height)
        return true;
    return false;
}

bool DisplayResScreen::operator == (const DisplayResScreen &Other) const
{
    return m_width == Other.m_width && m_height == Other.m_height;
}

void DisplayResScreen::Init(void)
{
    m_width = m_height = m_widthMM = m_heightMM = 0;
    m_aspect = -1.0;
}

int DisplayResScreen::Width(void) const
{
    return m_width;
}

int DisplayResScreen::Height(void) const
{
    return m_height;
}

int DisplayResScreen::WidthMM(void) const
{
    return m_widthMM;
}

int DisplayResScreen::HeightMM(void) const
{
    return m_heightMM;
}

const std::vector<double>& DisplayResScreen::RefreshRates(void) const
{
    return m_refreshRates;
}

double DisplayResScreen::AspectRatio() const
{
    if (m_aspect <= 0.0)
    {
        if (0 == m_heightMM)
            return 1.0;
        return static_cast<double>(m_widthMM) / m_heightMM;
    }
    return m_aspect;
}

double DisplayResScreen::RefreshRate() const
{
    if (m_refreshRates.size() >= 1)
        return m_refreshRates[0];
    else return 0.0;
}

void DisplayResScreen::SetAspectRatio(double AspectRatio)
{
    if (AspectRatio > 0.0)
        m_aspect = AspectRatio;
    else if (HeightMM())
        m_aspect = static_cast<double>(m_widthMM) / m_heightMM;
}

void DisplayResScreen::AddRefreshRate(double Rate)
{
    m_refreshRates.push_back(Rate);
    std::sort(m_refreshRates.begin(), m_refreshRates.end());
}

void DisplayResScreen::ClearRefreshRates(void)
{
    m_refreshRates.clear();
}

void DisplayResScreen::SetRefreshRate(double Rate)
{
    ClearRefreshRates();
    AddRefreshRate(Rate);
}

uint64_t DisplayResScreen::CalcKey(int Width, int Height, double Rate)
{
    return (static_cast<uint64_t>(Width) << 34) |
           (static_cast<uint64_t>(Height) << 18) |
           (static_cast<uint64_t>(Rate * 1000.0));
}

/// \brief Determine whether two rates are considered equal with the given precision
bool DisplayResScreen::CompareRates(double First, double Second, double Precision)
{
    return qAbs(First - Second) < Precision;
}

int DisplayResScreen::FindBestMatch(const DisplayResVector Modes,
                                    const DisplayResScreen& Mode, double &TargetRate)
{
    double videorate = Mode.RefreshRate();
    bool rate2x = false;
    bool end = false;

    // We will give priority to refresh rates that are twice what is looked for
    if ((videorate > 24.5) && (videorate < 30.5))
    {
        rate2x = true;
        videorate *= 2.0;
    }

    // Amend vector with custom list
    for (size_t i = 0; i < Modes.size(); ++i)
    {
        if (Modes[i].Width() == Mode.Width() && Modes[i].Height() == Mode.Height())
        {
            auto rates = Modes[i].RefreshRates();
            if (!rates.empty() && !qFuzzyCompare(videorate + 1.0,  1.0))
            {
                while (!end)
                {
                    for (double precision : { 0.001, 0.01, 0.1 })
                    {
                        for (size_t j = 0; j < rates.size(); ++j)
                        {
                            // Multiple of target_rate will do
                            if (CompareRates(videorate, rates[j], precision) ||
                                (qAbs(videorate - fmod(rates[j], videorate))
                                 <= precision) ||
                                (fmod(rates[j],videorate) <= precision))
                            {
                                TargetRate = rates[j];
                                return static_cast<int>(i);
                            }
                        }
                    }
                    // Can't find exact frame rate, so try rounding to the
                    // nearest integer, so 23.97Hz will work with 24Hz etc
                    for (double precision : { 0.01, 0.1, 1.0 })
                    {
                        double rounded = round(videorate);
                        for (size_t j = 0; j < rates.size(); ++j)
                        {
                            // Multiple of target_rate will do
                            if (CompareRates(rounded, rates[j], precision) ||
                                (qAbs(rounded - fmod(rates[j], rounded)) <= precision) ||
                                (fmod(rates[j],rounded) <= precision))
                            {
                                TargetRate = rates[j];
                                return static_cast<int>(i);
                            }
                        }
                    }
                    if (rate2x)
                    {
                        videorate /= 2.0;
                        rate2x = false;
                    }
                    else
                    {
                        end = true;
                    }
                }
                TargetRate = rates[rates.size() - 1];
            }
            else if (!rates.empty())
            {
                TargetRate = rates[rates.size() - 1];
            }
            return static_cast<int>(i);
        }
    }
    return -1;
}

#define extract_key(key) { \
        rate = ((key) & ((1<<18) - 1)) / 1000.0; \
        height = ((key) >> 18) & ((1<<16) - 1); \
        width = ((key) >> 34) & ((1<<16) - 1); }

uint64_t DisplayResScreen::FindBestScreen(const DisplayResMap& Map,
                                          int Width, int Height, double Rate)
{
    int width = 0;
    int height = 0;
    double rate = 0.0;

    // 1. search for exact match (width, height, rate)
    // 2. search for resolution, ignoring rate
    // 3. search for matching height and rate (or any rate if rate = 0)
    // 4. search for 2x rate
    // 5. search for 1x rate

    auto it = Map.cbegin();
    for ( ; it != Map.cend(); ++it)
    {
        extract_key(it->first);
        if (width == Width && height == Height && CompareRates(Rate, rate, 0.01))
            return it->first;
    }
    for (it = Map.cbegin(); it != Map.cend(); ++it)
    {
        extract_key(it->first);
        if (width == Width && height == Height && qFuzzyCompare(rate + 1.0, 1.0))
            return it->first;
    }
    for (it = Map.cbegin(); it != Map.cend(); ++it)
    {
        extract_key(it->first);
        if ((width == 0 && height == Height &&
             (CompareRates(Rate, rate, 0.01) || qFuzzyCompare(rate + 1.0, 1.0))) ||
            (width == 0 && height == 0 && CompareRates(Rate, rate * 2.0, 0.01)) ||
            (width == 0 && height == 0 && CompareRates(Rate, rate, 0.01)))
        {
            return it->first;
        }
    }

    return 0;
}
