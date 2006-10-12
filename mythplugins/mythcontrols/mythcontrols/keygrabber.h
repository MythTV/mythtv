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

// MythTV headers
#include <mythtv/mythdialogs.h>


/** \class KeyGrabPopupBox
 *  \brief Captures a key.
 *
 *   NOTE: This popup box takes control over the keyboard
 *         until a key is released.
 */
class KeyGrabPopupBox : public MythPopupBox
{
    Q_OBJECT

  public:
    KeyGrabPopupBox(MythMainWindow *window);

    /// \brief Get the string containing the captured key event plus
    ///        modifier keys. (note: result not thread-safe)
    QString GetCapturedKey(void) const { return m_capturedKey; }

  public slots:
    void Accept(void) { done(1); }
    void Cancel(void) { done(0); }

  protected:
    void keyPressEvent(QKeyEvent *e);
    void keyReleaseEvent(QKeyEvent *e);

  private:
    bool     m_waitingForKeyRelease;
    bool     m_keyReleaseSeen;
    QString  m_capturedKey;
    QButton *m_ok;
    QButton *m_cancel;
    QLabel  *m_label;
};


/** \class InvalidBindingPopup
 *  \brief Creates a popup that tells the user why a binding change failed.
 */
class InvalidBindingPopup : public MythPopupBox
{
    Q_OBJECT

  public:
    /// \brief Creates popup that tells the user he is breaking
    ///        a required binding.
    InvalidBindingPopup(MythMainWindow *window);

    /// \brief Tell the user that the binding conflicts with another action.
    InvalidBindingPopup(MythMainWindow *window,
                        const QString  &action,
                        const QString  &context);

    /// \brief Execute the error popup
    int GetOption(void) { return ExecPopup(this, SLOT(Finish())); }

  protected slots:
    void Finish(void) { done(0); }
};


/** \class OptionsMenu
 *  \brief Creates popup containing a list of options
 */
class OptionsMenu : public MythPopupBox
{
    Q_OBJECT

  public:
    enum actions { kSave, kCancel, };

    /// \brief Create a new action window. Does not pop-up menu.
    OptionsMenu(MythMainWindow *window);

    /// \brief Execute the option popup.
    int GetOption(void) { return ExecPopup(this,SLOT(Cancel())); }

  public slots:
    void Save(void)     { done(OptionsMenu::kSave);   }
    void Cancel(void)   { done(OptionsMenu::kCancel); }
};


/** \class ActionMenu
 *  \brief Creates popup listing ways to modify an action
 */
class ActionMenu : public MythPopupBox
{
    Q_OBJECT

  public:
    enum actions { kSet, kRemove, kCancel, };

    /// \brief Create a new action window. Does not pop-up menu.
    ActionMenu(MythMainWindow *window);

    /// \brief Execute the option popup.
    int GetOption(void) { return ExecPopup(this, SLOT(Cancel())); }

  public slots:
    void Set(void)      { done(ActionMenu::kSet);    }
    void Remove(void)   { done(ActionMenu::kRemove); }
    void Cancel(void)   { done(ActionMenu::kCancel); }
};


/** \class OptionsMenu
 *  \brief Creates popup containing a list of options
 */
class UnsavedMenu : public MythPopupBox
{
    Q_OBJECT

  public:
    enum actions { kSave, kExit, };

    /// \brief Create a new action window. Does not pop-up menu.
    UnsavedMenu(MythMainWindow *window);

    /// \brief Execute the option popup.
    int GetOption(void) { return ExecPopup(this, SLOT(Cancel())); }

  public slots:
    void Save(void)     { done(UnsavedMenu::kSave); }
    void Cancel(void)   { done(UnsavedMenu::kExit); }
};

/** \class ConfirmMenu
 *  \brief Creates popup confirming an action
 */
class ConfirmMenu : public MythPopupBox
{
    Q_OBJECT

  public:
    enum actions { kConfirm, kCancel, };

    /// \brief Create a new action window. Does not pop-up menu.
    ConfirmMenu(MythMainWindow *window, const QString &msg);

    /// \brief Execute the option popup.
    int GetOption(void) { return ExecPopup(this,SLOT(Cancel())); }

  public slots:
    void Confirm(void)  { done(ConfirmMenu::kConfirm); }
    void Cancel(void)   { done(ConfirmMenu::kCancel);  }
};
