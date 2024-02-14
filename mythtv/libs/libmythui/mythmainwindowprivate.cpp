// Qt
#include <QKeyEvent>

// MythTV
#include "mythmainwindowprivate.h"

// Make keynum in QKeyEvent be equivalent to what's in QKeySequence
int MythMainWindowPrivate::TranslateKeyNum(QKeyEvent* Event)
{
    if (Event == nullptr)
        return Qt::Key_unknown;

    int keynum = Event->key();

    if ((keynum != Qt::Key_Shift  ) && (keynum !=Qt::Key_Control   ) &&
        (keynum != Qt::Key_Meta   ) && (keynum !=Qt::Key_Alt       ) &&
        (keynum != Qt::Key_Super_L) && (keynum !=Qt::Key_Super_R   ) &&
        (keynum != Qt::Key_Hyper_L) && (keynum !=Qt::Key_Hyper_R   ) &&
        (keynum != Qt::Key_AltGr  ) && (keynum !=Qt::Key_CapsLock  ) &&
        (keynum != Qt::Key_NumLock) && (keynum !=Qt::Key_ScrollLock ))
    {
        // if modifiers have been pressed, rebuild keynum
        Qt::KeyboardModifiers modifiers = Event->modifiers();
        if (modifiers != Qt::NoModifier)
        {
            int modnum = Qt::NoModifier;
            if (((modifiers & Qt::ShiftModifier) != 0U) &&
                (keynum > 0x7f) &&
                (keynum != Qt::Key_Backtab))
                modnum |= Qt::SHIFT;
            if ((modifiers & Qt::ControlModifier) != 0U)
                modnum |= Qt::CTRL;
            if ((modifiers & Qt::MetaModifier) != 0U)
                modnum |= Qt::META;
            if ((modifiers & Qt::AltModifier) != 0U)
                modnum |= Qt::ALT;
            return (keynum | modnum);
        }
    }

    return keynum;
}
