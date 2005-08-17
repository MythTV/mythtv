/* -*- myth -*- */
/**
 * @file keygrabber.h
 * @author Micah F. Galizia <mfgalizi@csd.uwo.ca>
 * @brief Header for popups used by mythcontrols.
 *
 * Copyright (C) 2005 Micah Galizia
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <mythtv/mythdialogs.h>



/**
 * @class KeyGrabPopupBox
 * @brief Captures a key.
 *
 * This popup box takes control over the keyboard until a key is released.
 */
class KeyGrabPopupBox : public MythPopupBox
{

    Q_OBJECT

public:

    /**
     * @brief Create a new key grabber.
     * @param window The main myth window.
     */
    KeyGrabPopupBox(MythMainWindow * window);

    /**
     * @brief Get the string containing the captured key event.
     */
    inline QString getCapturedKey() { return this->captured_key_event; }

public slots:

/**
 * @brief Accept the captured keybinding.
 *
 * The QString provided in the constructor will be set to the
 * captured key value.
 */
inline void acceptBinding(void) { done(1); }

    /**
     * @brief Reject the captured key binding.
     *
     * The QString provided in the constructor will be set to NULL.
     */
    inline void cancel(void) { done(0); }

protected:

    /**
     * @brief The key press handler.
     * @param e The key event.
     */
    void keyPressEvent(QKeyEvent *e);

    /**
     * @brief The key release handler.
     * @param e The key event.
     */
    void keyReleaseEvent(QKeyEvent *e);

private:

    bool is_capturing;
    bool has_captured;
    QString captured_key_event;
    QButton *ok_button;
    QLabel *key_label;
};



/**
 * @class InvalidBindingPopup
 * @brief Tells the user that they are being bad!!!
 */
class InvalidBindingPopup : public MythPopupBox
{
    Q_OBJECT;

public:

    /**
     * @brief Create a popup that explains that they are removing a
     * critical key binding.
     * @param window The main myth window.
     */
    InvalidBindingPopup(MythMainWindow *window);

    /**
     * @brief Tell the user that the binding conflicts with another
     * action.
     */
    InvalidBindingPopup(MythMainWindow *window, const QString &action,
                        const QString &context);

    /**
     * @brief Execute the error popup
     */
    inline int getOption(void) { return ExecPopup(this,SLOT(finish())); }

protected slots:
/**
 * @brief Close the popup.
 */
inline void finish(void) { done(0); }
};


/**
 * @class OptionsMenu
 * @brief A popup containing a list of options.
 */
class OptionsMenu : public MythPopupBox
{

    Q_OBJECT;

public:

    enum actons { SAVE, CANCEL };

    /**
     * @brief Create a new action window.
     */
    OptionsMenu(MythMainWindow *window);

    /**
     * @brief Execute the option popup.
     */
    inline int getOption(void) { return ExecPopup(this,SLOT(cancel())); }

public slots:

/**
 * @brief Slot to connect to when the save button is pressed.
 */
inline void save(void) { done(OptionsMenu::SAVE); }

    /**
     * @brief Slot to connect to when the cancel button is pressed.
     */
    inline void cancel(void) { done(OptionsMenu::CANCEL); }

};



/**
 * @class ActionMenu
 * @brief A popup listing ways to modify an action.
 */
class ActionMenu : public MythPopupBox
{

    Q_OBJECT;

public:

    enum actons { SET, REMOVE, CANCEL };

    /**
     * @brief Create a new action window.
     */
    ActionMenu(MythMainWindow *window);

    /**
     * @brief Execute the option popup.
     */
    inline int getOption(void) { return ExecPopup(this,SLOT(cancel())); }

public slots:

/**
 * @brief Slot to connect to when the set button is pressed.
 */
inline void set(void) { done(ActionMenu::SET); }

    /**
     * @brief Slot to connect to when the remove button is pressed.
     */
    inline void remove(void) { done(ActionMenu::REMOVE); }

    /**
     * @brief Slot to connect to when the cancel button is pressed.
     */
    inline void cancel(void) { done(ActionMenu::CANCEL); }

};

/**
 * @class OptionsMenu
 * @brief A popup containing a list of options.
 */
class UnsavedMenu : public MythPopupBox
{

    Q_OBJECT;

public:

    enum actons { SAVE, EXIT };

    /**
     * @brief Create a new action window.
     */
    UnsavedMenu(MythMainWindow *window);

    /**
     * @brief Execute the option popup.
     */
    inline int getOption(void) { return ExecPopup(this,SLOT(cancel())); }

public slots:

/**
 * @brief Slot to connect to when the save button is pressed.
 */
inline void save(void) { done(UnsavedMenu::SAVE); }

    /**
     * @brief Slot to connect to when the cancel button is pressed.
     */
    inline void cancel(void) { done(UnsavedMenu::EXIT); }

};
