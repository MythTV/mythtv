/*----------------------------------------------------------------------------
** jsmenu.h
**  GPL license; Original copyright 2004 Jeremy White <jwhite@whitesen.org>
**--------------------------------------------------------------------------*/

#ifndef JSMENU_H_
#define JSMENU_H_

#include <qobject.h>
#include <qsocket.h>
#include <qstring.h>

#include "mythdialogs.h"

/*----------------------------------------------------------------------------
** JoystickMap related information
**  We build a map of how joystick buttons and axes (axes are for sticks
** and thumb controllers) are mapped into keystrokes.
**  For buttons, it's mostly very simple:  joystick button number 
** corresponds to a key sequence that is sent to MythTV.
**  We complicate it a little by allowing for 'chords', which
** means that if you hold down the 'chord' button while pressing
** the other button, we use the alternate mapping
**  For axes, it's not very complicated.  For each axis (ie up/down or
** left/right), we define a range; the first time the joystick moves
** into that range, we send the assigned keystring.  
**--------------------------------------------------------------------------*/
struct button_map_type
{
    int button;
    QString keystring;
    int chord;
};

typedef struct
{
    int axis;
    int from;
    int to;
    QString keystring;
} axis_map_type;

class JoystickMap
{
    public:
        void AddButton(int in_button, QString in_keystr, int in_chord = -1)
        {
            button_map_type new_button = { in_button, in_keystr, in_chord };
            button_map.push_back(new_button);
        }

        void AddAxis(int in_axis, int in_from, int in_to, QString in_keystr)
        {
            axis_map_type new_axis = { in_axis, in_from, in_to, in_keystr};
            axis_map.push_back(new_axis);
        }


        vector<button_map_type> button_map;
        vector<axis_map_type> axis_map;
};

/*----------------------------------------------------------------------------
** JoystickMenuClient
**  Main object for injecting key strokes based on joystick movements
**--------------------------------------------------------------------------*/
class JoystickMenuClient : public QObject
{
    Q_OBJECT
  public:
    JoystickMenuClient(QObject *main_window);
    ~JoystickMenuClient();
    int Init(QString &config_file);

    void Process(void);
    
    void ButtonUp(int button);
    void AxisChange(int axis, int value);
    void EmitKey(QString code);
    int  ReadConfig(QString config_file);

  private:
    QObject *mainWindow;
    
    QString devicename;

    int fd;

    JoystickMap map;

    /*------------------------------------------------------------------------
    ** These two arrays and their related counts track the status of the
    **   joystick buttons and axes, as we do depend slightly on state
    **----------------------------------------------------------------------*/
    unsigned char button_count;
    unsigned char axes_count;

    int *buttons;
    int *axes;

};

#endif
