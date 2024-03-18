#ifndef MYTHDIALOGBOX_H_
#define MYTHDIALOGBOX_H_

#include <functional>
#include <utility>

// Qt headers
#include <QDir>
#include <QEvent>
#include <QString>
#include <QStringList>

// MythTV headers
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythscreentype.h"
#include "libmythui/mythuitextedit.h"


class QTimer;

class MythUIButtonListItem;
class MythUIButtonList;
class MythUIButton;
class MythUITextEdit;
class MythUISpinBox;
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
    DialogCompletionEvent(QString id, int result, QString text,
                          QVariant data)
        : QEvent(kEventType),
          m_id(std::move(id)), m_result(result),
          m_resultText(std::move(text)),
          m_resultData(std::move(data)) { }
    ~DialogCompletionEvent() override;

    QString GetId() { return m_id; }
    int GetResult() const { return m_result; }
    QString GetResultText() { return m_resultText; }
    QVariant GetData() { return m_resultData; }

    static const Type kEventType;

  private:
    QString m_id;
    int m_result;
    QString m_resultText;
    QVariant m_resultData;
};


class MUI_PUBLIC MythMenuItem
{
  public:
    MythMenuItem(QString text, bool checked = false, MythMenu *subMenu = nullptr) :
        m_text(std::move(text)), m_checked(checked), m_subMenu(subMenu) { Init(); }
    // For non-class, static class, or lambda functions.
    MythMenuItem(QString text, const MythUICallbackNMF &slot,
                 bool checked = false, MythMenu *subMenu = nullptr) :
        m_text(std::move(text)), m_data(QVariant::fromValue(slot)),
        m_checked(checked), m_subMenu(subMenu) { Init(); }
    // For class member functions.
    MythMenuItem(QString text, MythUICallbackMF slot,
                 bool checked = false, MythMenu *subMenu = nullptr) :
    m_text(std::move(text)), m_data(QVariant::fromValue(slot)),
    m_checked(checked), m_subMenu(subMenu) { Init(); }
    // For const class member functions.
    MythMenuItem(QString text, MythUICallbackMFc slot,
                 bool checked = false, MythMenu *subMenu = nullptr) :
        m_text(std::move(text)), m_data(QVariant::fromValue(slot)),
        m_checked(checked), m_subMenu(subMenu) { Init(); }
    void SetData(QVariant data) { m_data = std::move(data); }

    QString   m_text;
    QVariant  m_data    {0};
    bool      m_checked {false};
    MythMenu *m_subMenu {nullptr};
    bool      m_useSlot {true};

  private:
    void Init(void) {}
};

class MUI_PUBLIC MythMenu
{
  friend class MythDialogBox;

  public:
    MythMenu(QString text, QObject *retobject, QString resultid);
    MythMenu(QString title, QString text, QObject *retobject, QString resultid);
    ~MythMenu(void);

    void AddItemV(const QString &title, QVariant data = 0, MythMenu *subMenu = nullptr,
                 bool selected = false, bool checked = false);
    void AddItem(const QString &title) { AddItemV(title); };
    // For non-class, static class, or lambda functions.
    void AddItem(const QString &title, const MythUICallbackNMF &slot,
                 MythMenu *subMenu = nullptr, bool selected = false,
                 bool checked = false);
    // For class member non-const functions.
    template <typename SLOT>
    typename std::enable_if_t<FunctionPointerTest<SLOT>::MemberFunction>
    AddItem(const QString &title, const SLOT &slot,
                  MythMenu *subMenu = nullptr, bool selected = false,
                  bool checked = false)
    {
        auto slot2 = static_cast<MythUICallbackMF>(slot);
        auto *item = new MythMenuItem(title, slot2, checked, subMenu);
        AddItem(item, selected, subMenu);
    }
    // For class member const functions.
    template <typename SLOT>
    typename std::enable_if_t<FunctionPointerTest<SLOT>::MemberConstFunction>
    AddItem(const QString &title, const SLOT &slot,
                  MythMenu *subMenu = nullptr, bool selected = false,
                  bool checked = false)
    {
        auto slot2 = static_cast<MythUICallbackMFc>(slot);
        auto *item = new MythMenuItem(title, slot2, checked, subMenu);
        AddItem(item, selected, subMenu);
    }

    void SetSelectedByTitle(const QString &title);
    void SetSelectedByData(const QVariant& data);

    void SetParent(MythMenu *parent) { m_parentMenu = parent; }

    bool IsEmpty()  { return m_menuItems.isEmpty(); }

  private:
    void Init(void) {}
    void AddItem(MythMenuItem *item, bool selected, MythMenu *subMenu);

    MythMenu *m_parentMenu   {nullptr};
    QString   m_title;
    QString   m_text;
    QString   m_resultid;
    QObject  *m_retObject    {nullptr};
    QList<MythMenuItem*> m_menuItems;
    int       m_selectedItem {0};
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
    MythDialogBox(QString text,
                  MythScreenStack *parent, const char *name,
                  bool fullscreen = false, bool osd = false)
        : MythScreenType(parent, name, false), m_fullscreen(fullscreen),
          m_osdDialog(osd), m_text(std::move(text)) {}
    MythDialogBox(QString title, QString text,
                  MythScreenStack *parent, const char *name,
                  bool fullscreen = false, bool osd = false)
        : MythScreenType(parent, name, false), m_fullscreen(fullscreen),
          m_osdDialog(osd), m_title(std::move(title)),m_text(std::move(text)) {}
    MythDialogBox(MythMenu* menu, MythScreenStack *parent, const char *name,
                   bool fullscreen = false, bool osd = false)
        : MythScreenType(parent, name, false), m_fullscreen(fullscreen),
          m_osdDialog(osd), m_menu(menu), m_currentMenu(menu) {}
    ~MythDialogBox(void) override;

    bool Create(void) override; // MythScreenType

    void SetMenuItems(MythMenu *menu);

    void SetReturnEvent(QObject *retobject, const QString &resultid);
    void SetBackAction(const QString &text, QVariant data);
    void SetExitAction(const QString &text, QVariant data);
    void SetText(const QString &text);

    void AddButtonV(const QString &title, QVariant data = 0,
                   bool newMenu = false, bool setCurrent = false);
    void AddButtonD(const QString &title, bool setCurrent) { AddButtonV(title, 0,false, setCurrent); }
    void AddButton(const QString &title) { AddButtonV(title, 0,false, false); }
    // For non-class, static class, or lambda functions.
    void AddButton(const QString &title, const MythUICallbackNMF &slot,
                   bool newMenu = false, bool setCurrent = false)
    {
        AddButtonV(title, QVariant::fromValue(slot), newMenu, setCurrent);
        m_useSlots = true;
    }
    // For class member non-const functions.
    template <typename SLOT>
    typename std::enable_if_t<FunctionPointerTest<SLOT>::MemberFunction>
    AddButton(const QString &title, const SLOT &slot,
                    bool newMenu = false, bool setCurrent = false)
    {
        auto slot2 = static_cast<MythUICallbackMF>(slot);
        AddButtonV(title, QVariant::fromValue(slot2), newMenu, setCurrent);
        m_useSlots = true;
    }
    // For class member const functions.
    template <typename SLOT>
    typename std::enable_if_t<FunctionPointerTest<SLOT>::MemberConstFunction>
    AddButton(const QString &title, const SLOT &slot,
                    bool newMenu = false, bool setCurrent = false)
    {
        auto slot2 = static_cast<MythUICallbackMFc>(slot);
        AddButtonV(title, QVariant::fromValue(slot2), newMenu, setCurrent);
        m_useSlots = true;
    }

    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    bool inputMethodEvent(QInputMethodEvent *event) override;// MythScreenType
    bool gestureEvent(MythGestureEvent *event) override; // MythScreenType

  public slots:
    void Select(MythUIButtonListItem* item);

  signals:
    void Selected();
    void Closed(QString, int);

  protected:
    void SendEvent(int res, const QString& text = "", const QVariant& data = 0);
    void updateMenu(void);

    MythUIText       *m_titlearea   {nullptr};
    MythUIText       *m_textarea    {nullptr};
    MythUIButtonList *m_buttonList  {nullptr};
    QObject          *m_retObject   {nullptr};
    QString           m_id;
    bool              m_useSlots    {false};

    bool              m_fullscreen  {false};
    bool              m_osdDialog   {false};
    QString           m_title;
    QString           m_text;

    QString           m_backtext;
    QVariant          m_backdata    {0};
    QString           m_exittext;
    QVariant          m_exitdata    {0};

    MythMenu         *m_menu        {nullptr};
    MythMenu         *m_currentMenu {nullptr};
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
    MythConfirmationDialog(MythScreenStack *parent, QString message,
                           bool showCancel = true)
        : MythScreenType(parent, "mythconfirmpopup"),
          m_message(std::move(message)), m_showCancel(showCancel) {}

    bool Create(void) override; // MythScreenType
    void SetReturnEvent(QObject *retobject, const QString &resultid);
    void SetData(QVariant data) { m_resultData = std::move(data); }
    void SetMessage(const QString &message);

    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

 signals:
     void haveResult(bool);

  private:
    void sendResult(bool ok);
    MythUIText *m_messageText {nullptr};
    QString     m_message;
    bool        m_showCancel  {true};
    QObject    *m_retObject   {nullptr};
    QString     m_id;
    QVariant    m_resultData;

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
    MythTextInputDialog(MythScreenStack *parent, QString message,
                        InputFilter filter = FilterNone,
                        bool isPassword = false,
                        QString defaultValue = "")
        : MythScreenType(parent, "mythtextinputpopup"),
          m_message(std::move(message)), m_defaultValue(std::move(defaultValue)),
          m_filter(filter), m_isPassword(isPassword) {}

    bool Create(void) override; // MythScreenType
    void SetReturnEvent(QObject *retobject, const QString &resultid);

  signals:
     void haveResult(QString);

  protected:
    MythUITextEdit *m_textEdit     {nullptr};
    QString         m_message;
    QString         m_defaultValue;
    InputFilter     m_filter       {FilterNone};
    bool            m_isPassword   {false};
    QObject        *m_retObject    {nullptr};
    QString         m_id;

  protected slots:
    void sendResult();
};


/**
 *  \class MythSpinBoxDialog
 *
 *  \brief Dialog prompting the user to enter a number using a spin box
 *
 *  Sends out a DialogCompletionEvent event and the haveResult() signal
 *  containing the result when the user selects the Ok button.
 */
class MUI_PUBLIC MythSpinBoxDialog : public MythScreenType
{
    Q_OBJECT

  public:
    MythSpinBoxDialog(MythScreenStack *parent, QString message);

    bool Create(void) override; // MythScreenType
    void SetReturnEvent(QObject *retobject, const QString &resultid);

    void SetRange(int low, int high, int step, uint pageMultiple=5);
    void AddSelection(const QString& label, int value);
    void SetValue(const QString & value);
    void SetValue(int value);

  signals:
     void haveResult(QString);

  protected:
    MythUISpinBox *m_spinBox { nullptr };
    QString m_message;
    QString m_defaultValue;
    QObject *m_retObject     { nullptr };
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
    /** \brief the classes constructor
     *  \param parent the MythScreenStack this widget belongs to
     *  \param title  the text to show as the title
     *  \param list   the list of text strings to search
     *  \param matchAnywhere if true will match the input text anywhere in the string.
     *                       if false will match only strings that start with the input text.
     *                       Default is false.
     *  \param defaultValue  The initial value for the input text. Default is ""
     */
    MythUISearchDialog(MythScreenStack *parent,
                     QString title,
                     QStringList list,
                     bool  matchAnywhere = false,
                     QString defaultValue = "")
        : MythScreenType(parent, "mythsearchdialogpopup"),
          m_title(std::move(title)),
          m_defaultValue(std::move(defaultValue)),
          m_list(std::move(list)),
          m_matchAnywhere(matchAnywhere),
          m_id("") {};

    bool Create(void) override; // MythScreenType
    void SetReturnEvent(QObject *retobject, const QString &resultid);

  signals:
     void haveResult(QString);

  private:
    MythUIButtonList *m_itemList      { nullptr };
    MythUITextEdit   *m_textEdit      { nullptr };
    MythUIText       *m_titleText     { nullptr };
    MythUIText       *m_matchesText   { nullptr };

    QString           m_title;
    QString           m_defaultValue;
    QStringList       m_list;
    bool              m_matchAnywhere { false   };

    QObject          *m_retObject     { nullptr };
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
    enum TimeInputResolution : std::uint16_t {
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

    MythTimeInputDialog(MythScreenStack *parent, QString message,
                        int resolutionFlags,
                        QDateTime startTime = QDateTime::currentDateTime(),
                        int rangeLimit = 14);

    bool Create() override; // MythScreenType
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

    MythUIButtonList *m_dateList  { nullptr };
    MythUIButtonList *m_timeList  { nullptr };

    QObject          *m_retObject { nullptr };
    QString           m_id;
};

MUI_PUBLIC MythConfirmationDialog  *ShowOkPopup(const QString &message, bool showCancel = false);
template <class OBJ, typename FUNC>
MythConfirmationDialog  *ShowOkPopup(const QString &message, const OBJ *parent,
                                     FUNC slot, bool showCancel = false)
{
    QString                  LOC = "ShowOkPopup('" + message + "') - ";
    MythScreenStack         *stk = nullptr;

    MythMainWindow *win = GetMythMainWindow();

    if (win)
        stk = win->GetStack("popup stack");
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "no main window?");
        return nullptr;
    }

    if (!stk)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "no popup stack? "
                                       "Is there a MythThemeBase?");
        return nullptr;
    }

    auto *pop = new MythConfirmationDialog(stk, message, showCancel);
    if (pop->Create())
    {
        stk->AddScreen(pop);
        if (parent)
            QObject::connect(pop, &MythConfirmationDialog::haveResult, parent, slot,
                             Qt::QueuedConnection);
    }
    else
    {
        delete pop;
        pop = nullptr;
        LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't Create() Dialog");
    }

    return pop;
}

/// Blocks until confirmation dialog exits
bool MUI_PUBLIC WaitFor(MythConfirmationDialog* dialog);

Q_DECLARE_METATYPE(MythMenuItem*)
Q_DECLARE_METATYPE(const char*)

#endif
