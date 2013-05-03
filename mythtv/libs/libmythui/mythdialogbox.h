#ifndef MYTHDIALOGBOX_H_
#define MYTHDIALOGBOX_H_

#include <QDir>
#include <QEvent>

#include "mythscreentype.h"
#include "mythuitextedit.h"

class QString;
class QStringList;
class QTimer;

class MythUIButtonListItem;
class MythUIButtonList;
class MythUIButton;
class MythUITextEdit;
class MythUIImage;
class MythUIStateType;
class MythMenu;


/**
 *  \class DialogCompletionEvent
 *
 *  \brief Event dispatched from MythUI modal dialogs to a listening class
 *         containing a result of some form
 *
 *  The result may be in the format of an int, text or a void pointer
 *  dependant on the dialog type and information it is conveying.
 */
class MUI_PUBLIC DialogCompletionEvent : public QEvent
{
  public:
    DialogCompletionEvent(const QString &id, int result, QString text,
                          QVariant data)
        : QEvent(kEventType),
          m_id(id), m_result(result), m_resultText(text), m_resultData(data) { }

    QString GetId() { return m_id; }
    int GetResult() { return m_result; }
    QString GetResultText() { return m_resultText; }
    QVariant GetData() { return m_resultData; }

    static Type kEventType;

  private:
    QString m_id;
    int m_result;
    QString m_resultText;
    QVariant m_resultData;
};


class MUI_PUBLIC MythMenuItem
{
  public:
    MythMenuItem(const QString &text, QVariant data = 0, bool checked = false, MythMenu *subMenu = NULL) :
        Text(text), Data(data), Checked(checked), SubMenu(subMenu), UseSlot(false) { Init(); }
    MythMenuItem(const QString &text, const char *slot, bool checked = false, MythMenu *subMenu = NULL) :
        Text(text), Data(qVariantFromValue(slot)), Checked(checked), SubMenu(subMenu), UseSlot(true) { Init(); }

    QString   Text;
    QVariant  Data;
    bool      Checked;
    MythMenu *SubMenu;
    bool      UseSlot;

  private:
    void Init(void) { Text.detach(); }
};

class MUI_PUBLIC MythMenu
{
  friend class MythDialogBox;

  public:
    MythMenu(const QString &text, QObject *retobject, const QString &resultid);
    MythMenu(const QString &title, const QString &text, QObject *retobject, const QString &resultid);
    ~MythMenu(void);

    void AddItem(const QString &title, QVariant data = 0, MythMenu *subMenu = NULL,
                 bool selected = false, bool checked = false);
    void AddItem(const QString &title, const char *slot, MythMenu *subMenu = NULL,
                 bool selected = false, bool checked = false);

    void SetParent(MythMenu *parent) { m_parentMenu = parent; }

  private:
    void Init(void);

    MythMenu *m_parentMenu;
    QString   m_title;
    QString   m_text;
    QString   m_resultid;
    QObject  *m_retObject;
    QList<MythMenuItem*> m_menuItems;
    int m_selectedItem;
};

/**
 *  \class MythDialogBox
 *
 *  \brief Basic menu dialog, message and a list of options
 *
 *  Sends out a DialogCompletionEvent event and the Selected() signal
 *  containing the result when the user selects the Ok button.
 */
class MUI_PUBLIC MythDialogBox : public MythScreenType
{
    Q_OBJECT
  public:
    MythDialogBox(const QString &text,
                  MythScreenStack *parent, const char *name,
                  bool fullscreen = false, bool osd = false);
    MythDialogBox(const QString &title, const QString &text,
                  MythScreenStack *parent, const char *name,
                  bool fullscreen = false, bool osd = false);
    MythDialogBox(MythMenu* menu, MythScreenStack *parent, const char *name,
                   bool fullscreen = false, bool osd = false);

    ~MythDialogBox(void);

    virtual bool Create(void);

    void SetMenuItems(MythMenu *menu);

    void SetReturnEvent(QObject *retobject, const QString &resultid);
    void SetBackAction(const QString &text, QVariant data);
    void SetExitAction(const QString &text, QVariant data);
    void SetText(const QString &text);

    void AddButton(const QString &title, QVariant data = 0,
                   bool newMenu = false, bool setCurrent = false);
    void AddButton(const QString &title, const char *slot,
                   bool newMenu = false, bool setCurrent = false);

    virtual bool keyPressEvent(QKeyEvent *event);
    virtual bool gestureEvent(MythGestureEvent *event);

  public slots:
    void Select(MythUIButtonListItem* item);

  signals:
    void Selected();
    void Closed(QString, int);

  protected:
    void SendEvent(int res, QString text = "", QVariant data = 0);
    void updateMenu(void);

    MythUIText       *m_titlearea;
    MythUIText       *m_textarea;
    MythUIButtonList *m_buttonList;
    QObject          *m_retObject;
    QString m_id;
    bool m_useSlots;

    bool    m_fullscreen;
    bool    m_osdDialog;
    QString m_title;
    QString m_text;

    QString  m_backtext;
    QVariant m_backdata;
    QString  m_exittext;
    QVariant m_exitdata;

    MythMenu *m_menu;
    MythMenu *m_currentMenu;
};


/**
 *  \class MythConfirmationDialog
 *
 *  \brief Dialog asking for user confirmation. Ok and optional Cancel button.
 *
 *  Sends out a DialogCompletionEvent event and the haveResult() signal
 *  containing the result.
 */
class MUI_PUBLIC MythConfirmationDialog : public MythScreenType
{
    Q_OBJECT

  public:
    MythConfirmationDialog(MythScreenStack *parent, const QString &message,
                           bool showCancel = true);

    bool Create(void);
    void SetReturnEvent(QObject *retobject, const QString &resultid);
    void SetData(QVariant data) { m_resultData = data; }
    void SetMessage(const QString &message);

    bool keyPressEvent(QKeyEvent *event);

 signals:
     void haveResult(bool);

  private:
    void sendResult(bool);
    MythUIText *m_messageText;
    QString m_message;
    bool m_showCancel;
    QObject *m_retObject;
    QString m_id;
    QVariant m_resultData;

  private slots:
    void Confirm(void);
    void Cancel();
};

/**
 *  \class MythTextInputDialog
 *
 *  \brief Dialog prompting the user to enter a text string
 *
 *  Sends out a DialogCompletionEvent event and the haveResult() signal
 *  containing the result when the user selects the Ok button.
 */
class MUI_PUBLIC MythTextInputDialog : public MythScreenType
{
    Q_OBJECT

  public:
    MythTextInputDialog(MythScreenStack *parent, const QString &message,
                        InputFilter filter = FilterNone,
                        bool isPassword = false,
                        const QString &defaultValue = "");

    bool Create(void);
    void SetReturnEvent(QObject *retobject, const QString &resultid);

  signals:
     void haveResult(QString);

  protected:
    MythUITextEdit *m_textEdit;
    QString m_message;
    QString m_defaultValue;
    InputFilter m_filter;
    bool m_isPassword;
    QObject *m_retObject;
    QString m_id;

  protected slots:
    void sendResult();
};


/**
 * \class MythUISearchDialog
 * \brief Provide a dialog to quickly find an entry in a list
 *
 * You pass a QStringList containing the list you want to search.
 * As the user enters a text string in the edit the list of entries
 * changes to show only the entries that match the input string. You
 * have the option to search anywhere in the string or only the start.
 *
 * When the user either clicks an entry in the list or presses the OK
 * button the dialog will either send a DialogCompletionEvent or an
 * haveResult(QString) signal will be generated. Both pass the selected
 * string back to the caller.
 */
class MUI_PUBLIC MythUISearchDialog : public MythScreenType
{
  Q_OBJECT

  public:
    MythUISearchDialog(MythScreenStack *parent,
                     const QString &message,
                     const QStringList &list,
                     bool  matchAnywhere = false,
                     const QString &defaultValue = "");

    bool Create(void);
    void SetReturnEvent(QObject *retobject, const QString &resultid);

  signals:
     void haveResult(QString);

  private:
    MythUIButtonList *m_itemList;
    MythUITextEdit   *m_textEdit;
    MythUIText       *m_titleText;
    MythUIText       *m_matchesText;

    QString           m_title;
    QString           m_defaultValue;
    QStringList       m_list;
    bool              m_matchAnywhere;

    QObject          *m_retObject;
    QString           m_id;

  private slots:
    void slotSendResult(void);
    void slotUpdateList(void);
};

/**
 * \class MythUITimeInputDialog
 * \brief Provide a dialog for inputting a date/time or both
 *
 * \param message The message to display to the user explaining what you are
 *                asking for
 * \param startTime The date/time to start the list, defaults to now
 */
class MUI_PUBLIC MythTimeInputDialog : public MythScreenType
{
    Q_OBJECT

  public:
    // FIXME Not sure about this enum
    enum TimeInputResolution {
        // Date Resolution
        kNoDate       = 0x01,
        kYear         = 0x02,
        kMonth        = 0x04,
        kDay          = 0x08,

        // Time Resolution
        kNoTime       = 0x10,
        kHours        = 0x20,
        kMinutes      = 0x40,

        // Work forward/backwards or backwards and fowards from start time
        kFutureDates  = 0x100,
        kPastDates    = 0x200,
        kAllDates     = 0x300
    };

    MythTimeInputDialog(MythScreenStack *parent, const QString &message,
                        int resolutionFlags,
                        QDateTime startTime = QDateTime::currentDateTime(),
                        int dayLimit = 14);

    bool Create();
    void SetReturnEvent(QObject *retobject, const QString &resultid);

  signals:
    void haveResult(QDateTime time);

  private slots:
    void okClicked(void);

  private:
    QString           m_message;
    QDateTime         m_startTime;
    int               m_resolution;
    int               m_rangeLimit;
    QStringList       m_list;
    QString           m_currentValue;

    MythUIButtonList *m_dateList;
    MythUIButtonList *m_timeList;

    QObject          *m_retObject;
    QString           m_id;
};

MUI_PUBLIC MythConfirmationDialog  *ShowOkPopup(const QString &message, QObject *parent = NULL,
                                             const char *slot = NULL, bool showCancel = false);

Q_DECLARE_METATYPE(MythMenuItem*)
Q_DECLARE_METATYPE(const char*)

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_DECLARE_METATYPE(QFileInfo)
#endif

#endif
