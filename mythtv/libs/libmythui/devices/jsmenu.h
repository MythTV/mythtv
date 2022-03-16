/*----------------------------------------------------------------------------
** jsmenu.h
**  GPL license; Original copyright 2004 Jeremy White <jwhite@whitesen.org>
**--------------------------------------------------------------------------*/

#ifndef JSMENU_H_
#define JSMENU_H_

// C++ headers
#include <utility>
#include <vector>

// QT headers
#include <QString>

// MythTV headers
#include "libmythbase/mthread.h"

struct buttonMapType
{
    int button;
    QString keystring;
    int chord;
};

struct axisMapType
{
    int axis;
    int from;
    int to;
    QString keystring;
};

/**
 *  \class JoystickMap
 *
 *  \brief Holds the buttonMapType and axisMapType structs which map actions
 *         to events.
 *
 *  We build a map of how joystick buttons and axes (axes are for sticks
 *  and thumb controllers) are mapped into keystrokes.
 *  For buttons, it's mostly very simple:  joystick button number
 *  corresponds to a key sequence that is sent to MythTV.
 *  We complicate it a little by allowing for 'chords', which
 *  means that if you hold down the 'chord' button while pressing
 *  the other button, we use the alternate mapping
 *  For axes, it's not very complicated.  For each axis (ie up/down or
 *  left/right), we define a range; the first time the joystick moves
 *  into that range, we send the assigned keystring.
 */
class JoystickMap
{
    public:
        void AddButton(int in_button, QString in_keystr, int in_chord = -1)
        {
            buttonMapType new_button = { in_button, std::move(in_keystr), in_chord };
            m_buttonMap.push_back(new_button);
        }

        void AddAxis(int in_axis, int in_from, int in_to, QString in_keystr)
        {
            axisMapType new_axis = { in_axis, in_from, in_to, std::move(in_keystr)};
            m_axisMap.push_back(new_axis);
        }

        void Clear(){
            m_axisMap.clear();
            m_buttonMap.clear();
        }

        using button_map_t = std::vector<buttonMapType>;
        using axis_map_t = std::vector<axisMapType>;
        const button_map_t &buttonMap() const { return m_buttonMap; }
        const axis_map_t &axisMap() const { return m_axisMap; }

        button_map_t m_buttonMap;
        axis_map_t m_axisMap;
};

/**
 * \class JoystickMenuThread
 *
 * \brief Main object for injecting key strokes based on joystick movements
 *
 * \ingroup MythUI_Input
 */
class JoystickMenuThread : public MThread
{
  public:
    explicit JoystickMenuThread(QObject *main_window)
        : MThread("JoystickMenu"), m_mainWindow(main_window) {}
    ~JoystickMenuThread() override;
    bool Init(QString &config_file);

    void ButtonUp(int button);
    void AxisChange(int axis, int value);
    void EmitKey(const QString& code);
    bool ReadConfig(const QString& config_file);
    void Stop(void) { m_bStop = true; }

  private:
    void run() override; // MThread

    bool m_configRead           {false};
    bool m_readError            {false};
    // put here to avoid changing other classes
    QString m_configFile;

    QObject *m_mainWindow       {nullptr};
    QString m_devicename;
    int     m_fd                {-1};
    JoystickMap m_map;

    /**
     * Track the status of the joystick buttons as we do depend slightly on
     * state
     */
    unsigned char m_buttonCount {0};

    /**
     * Track the status of the joystick axes as we do depend slightly on
     * state
     */
    unsigned char m_axesCount   {0};

    int *m_buttons              {nullptr};
    int *m_axes                 {nullptr};

    volatile bool m_bStop       {false};
};

#endif
