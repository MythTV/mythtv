/*----------------------------------------------------------------------------
** jsmenu.h
**  GPL license; Original copyright 2004 Jeremy White <jwhite@whitesen.org>
**--------------------------------------------------------------------------*/

#ifndef JSMENU_H_
#define JSMENU_H_

// C++ headers
#include <vector>

// QT headers
#include <QString>
#include <QThread>

typedef struct
{
    int button;
    QString keystring;
    int chord;
}  buttonMapType;

typedef struct
{
    int axis;
    int from;
    int to;
    QString keystring;
} axisMapType;

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
            buttonMapType new_button = { in_button, in_keystr, in_chord };
            m_buttonMap.push_back(new_button);
        }

        void AddAxis(int in_axis, int in_from, int in_to, QString in_keystr)
        {
            axisMapType new_axis = { in_axis, in_from, in_to, in_keystr};
            m_axisMap.push_back(new_axis);
        }

        typedef std::vector<buttonMapType> button_map_t;
        typedef std::vector<axisMapType> axis_map_t;
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
class JoystickMenuThread : public QThread
{
  public:
    JoystickMenuThread(QObject *main_window);
    ~JoystickMenuThread();
    int Init(QString &config_file);

    void ButtonUp(int button);
    void AxisChange(int axis, int value);
    void EmitKey(QString code);
    int  ReadConfig(QString config_file);
    void Stop(void) { m_bStop = true; }

  private:
    void run(void);

    QObject *m_mainWindow;
    QString m_devicename;
    int m_fd;
    JoystickMap m_map;

    /**
     * Track the status of the joystick buttons as we do depend slightly on
     * state
     */
    unsigned char m_buttonCount;

    /**
     * Track the status of the joystick axes as we do depend slightly on
     * state
     */
    unsigned char m_axesCount;

    int *m_buttons;
    int *m_axes;

    volatile bool m_bStop;
};

#endif
