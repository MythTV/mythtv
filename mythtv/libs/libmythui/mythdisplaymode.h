#ifndef MYTHDISPLAYMODE_H_
#define MYTHDISPLAYMODE_H_

// Qt
#include <QString>

// MythTV
#include "mythuiexp.h"

// Std
#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

class MythDisplayMode;
using namespace std;
typedef vector<MythDisplayMode> DisplayModeVector;
typedef map<uint64_t, MythDisplayMode> DisplayModeMap;

class MUI_PUBLIC MythDisplayMode
{
  public:
    bool operator <  (const MythDisplayMode& Other) const;
    bool operator == (const MythDisplayMode& Other) const;

    MythDisplayMode() = default;
    MythDisplayMode(int Width, int Height, int MMWidth, int MMHeight,
                    double AspectRatio, double RefreshRate);
    void   Init          (void);
    int    Width         (void) const;
    int    Height        (void) const;
    int    WidthMM       (void) const;
    int    HeightMM      (void) const;
    double AspectRatio   (void) const;
    double RefreshRate   (void) const;
    void   SetAspectRatio(double AspectRatio);
    void   AddRefreshRate(double Rate);
    void   ClearRefreshRates(void);
    void   SetRefreshRate(double Rate);
    const std::vector<double>& RefreshRates(void) const;
    static int      FindBestMatch (const DisplayModeVector Modes,
                                   const MythDisplayMode& Mode, double& TargetRate);
    static uint64_t CalcKey       (int Width, int Height, double Rate);
    static bool     CompareRates  (double First, double Second, double Precision = 0.01);
    static uint64_t FindBestScreen(const DisplayModeMap& Map,
                                   int Width, int Height, double Rate);

  private:
    int m_width     { 0 };
    int m_height    { 0 };
    int m_widthMM   { 0 };
    int m_heightMM  { 0 };
    double m_aspect { -1.0 };
    vector<double> m_refreshRates { };
};
#endif // MYTHDISPLAYMODE_H_
