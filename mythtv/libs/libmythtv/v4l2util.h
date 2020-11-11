#ifndef V4L2_UTIL_H
#define V4L2_UTIL_H

#ifdef USING_V4L2
#include <linux/videodev2.h>
#endif

#include "tv.h"
#include "mythtvexp.h"

#include <QStringList>
#include "driveroption.h"

struct DriverOption;

class MTV_PUBLIC V4L2util
{
  public:
    V4L2util(void) = default;
    explicit V4L2util(const QString& dev_name);
    V4L2util(const QString& dev_name, const QString& vbi_dev_name);
    ~V4L2util(void);

    bool Open(const QString& dev_name, const QString& vbi_dev_name = "");
    void Close(void);
    int  FD(void) const { return m_fd; }

    bool operator!(void) const { return m_fd < 0; }
    bool IsOpen(void) const    { return m_fd >= 0; }

    bool GetOptions(DriverOption::Options& options);
    int  GetOptionValue(DriverOption::category_t cat, const QString& desc);
    bool GetFormats(QStringList& formats);
    bool GetVideoStandard(QString& name) const;
    int  GetSignalStrength(void) const;
    bool GetResolution(int& width, int& height) const;
    uint32_t GetCapabilities(void) const;
    QString  GetDeviceName(void) const;
    QString  GetDriverName(void) const;

    bool HasTuner(void) const;
    bool HasAudioSupport(void) const;
    bool HasStreaming(void) const;
    bool HasSlicedVBI(void) const;
    bool IsEncoder(void) const;
    bool UserAdjustableResolution(void) const;

    QString DriverName(void)  const { return m_driverName; }
    QString CardName(void)    const { return m_cardName; }
    QString ProfileName(void) const { return "V4L2:" + m_driverName; }

    int  GetStreamType(void) const;
    bool SetStreamType(int value);
    // Video controls
    bool SetVideoAspect(int value);
    bool SetVideoBitrateMode(int value);
    bool SetVideoBitrate(int value);
    bool SetVideoBitratePeak(int value);
    bool SetResolution(uint32_t width, uint32_t height);
    // Audio controls
    bool SetAudioInput(int value);
    bool SetAudioCodec(int value);
    bool SetVolume(int volume);
    bool SetLanguageMode(int mode);
    bool SetAudioSamplingRate(int value);
    bool SetAudioBitrateL2(int value);

    // Actions
    bool StartEncoding(void);
    bool StopEncoding(void);
    bool PauseEncoding(void);
    bool ResumeEncoding(void);

    static QString StreamTypeDesc(int value);

  protected:
    // VBI
    static bool OpenVBI(const QString& vbi_dev_name);
    bool SetSlicedVBI(VBIMode::vbimode_t vbimode);

    int  GetExtControl(int request, const QString& ctrl_desc = "") const;
    bool SetExtControl(int request, int value, const QString& ctrl_desc,
                       const QString& value_desc);
    bool SetEncoderState(int mode, const QString& desc);
    void SetDefaultOptions(DriverOption::Options& options);

    static void    bitmask_toString(QString& result, uint32_t flags,
                                uint32_t mask, const QString& desc);
    static QString ctrlflags_toString(uint32_t flags);
    static QString queryctrl_toString(int type);

    void log_qctrl(struct v4l2_queryctrl& queryctrl, DriverOption& drv_opt,
                   QString& msg);
    bool log_control(struct v4l2_queryctrl& qctrl, DriverOption& drv_opt,
                     QString& msg);
    void log_controls(bool show_menus);

  private:
    int      m_fd                   {-1};
    int      m_vbiFd                {-1};
    DriverOption::Options m_options;
    QString  m_deviceName;
    QString  m_driverName;
    QString  m_cardName;
    int      m_version              {0};
    uint32_t m_capabilities         {0};
    bool     m_haveQueryExtCtrl     {false};
};

#endif // V4L2_UTIL_H
