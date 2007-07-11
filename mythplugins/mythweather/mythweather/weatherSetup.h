
#ifndef _WEATHER_SETUP_H_
#define _WEATHER_SETUP_H_
#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include <mythtv/oldsettings.h> 
#include <mythtv/mythwidgets.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/mythwizard.h>
#include <mythtv/uilistbtntype.h>
#include <qobject.h>
#include <qrect.h>
#include "defs.h"

class SourceManager;

struct TypeListInfo {
    QString name;
    QString location;
    struct ScriptInfo *src;
};

struct ScreenListInfo {
    /* 
     * TODO may need to store container name, since the text will probably get
     * translated
     */
    QDict<struct TypeListInfo> types;
    QString helptxt;
    units_t units;
    bool hasUnits;
    bool multiLoc;
};

struct SourceListInfo {
    QString name;
    QString author;
    QString email;
    QString version;
    uint update_timeout;
    uint retrieve_timeout;
    uint id;
};

class WeatherSpinBox : public MythSpinBox {
    Q_OBJECT
    public:
        WeatherSpinBox(MythThemedDialog *parent = NULL, const char* name = 0, 
                bool allow_single_step=false) 
            : MythSpinBox(parent, name, allow_single_step){
                m_parent = parent;
                m_context = -1;
            };

    protected slots:
        virtual bool eventFilter (QObject* o, QEvent* e) {
            bool handled = false;
            if (e->type() == QEvent::KeyPress) {
                QStringList actions;
                if (gContext->GetMainWindow()->TranslateKeyPress("qt", (QKeyEvent*)e, actions)) {
                    for (uint i = 0; i < actions.size(); ++i) {
                        QString action = actions[i];
                        handled = true;
                        if (action == "DOWN") {
                            m_parent->nextPrevWidgetFocus(true);
                        } else if (action == "UP") {
                            m_parent->nextPrevWidgetFocus(false);
                        } else handled = false;
                    }
                }
                if (!handled)
                    ((QKeyEvent*)e)->ignore();
            }
            /*
             * This doesn't seem kosher, we're ignoring the return value of
             * MythSpinBox::eventFilter() (implemented in QSpinBox) because it
             * seems to eat key presses that it shouldn't (MENU for example),
             * i.e. its returning true, seems like it should be false. oh well.
             * This works, though it may not be portable to other screens.
             */
            MythSpinBox::eventFilter(o,e);
            return handled;
        };

    private:
        MythThemedDialog *m_parent;
        int m_context;
};

class GlobalSetup : public MythThemedDialog {
    Q_OBJECT
    public:
        GlobalSetup(MythMainWindow*);
        ~GlobalSetup();
    protected slots:
        void keyPressEvent(QKeyEvent *e);
    private:
        void loadData();
        void saveData();
        void wireUI();
        UICheckBoxType* m_background_check;
        UICheckBoxType* m_skip_check;
        WeatherSpinBox* m_timeout_spinbox;
        WeatherSpinBox* m_hold_spinbox;
        int m_timeout;
        int m_hold_timeout;
};

class ScreenSetup : public MythThemedDialog {
    Q_OBJECT
    public:
        ScreenSetup(MythMainWindow*, SourceManager*);
    protected slots:
        void keyPressEvent(QKeyEvent *e);
        void activeListItemSelected(UIListBtnTypeItem* itm = 0 );
        void updateHelpText();
    private:
        void loadData();
        void saveData();
        void wireUI();

        void doListSelect(UIListBtnType*, UIListBtnTypeItem*);
        bool showUnitsPopup(QString, struct ScreenListInfo*);
        void doLocationDialog(struct ScreenListInfo*,bool);
        bool showLocationPopup(QStringList, QString&, struct ScriptInfo*&);

        void cursorUp(UIType*);
        void cursorDown(UIType*);
        void cursorRight(UIType*);
        void cursorLeft(UIType*);
        void cursorSelect(UIType*);
        void cursorMenu();

        SourceManager* m_src_man;
        XMLParse m_weather_screens;
        UITextType* m_help_txt;
        UIListBtnType* m_active_list;
        UIListBtnType* m_inactive_list;
        UIListBtnType* m_type_list;

};

class SourceSetup : public MythThemedDialog {
    Q_OBJECT
    public:
        SourceSetup(MythMainWindow*);
        ~SourceSetup();
        bool loadData();
    protected slots:
        void keyPressEvent(QKeyEvent *e);
        void sourceListItemSelected(UIListBtnTypeItem* itm = 0 );
        void updateSpinboxUpdate();
        void retrieveSpinboxUpdate();
    private:
        void saveData();
        void wireUI();

        WeatherSpinBox* m_update_spinbox;
        WeatherSpinBox* m_retrieve_spinbox;
        UIListBtnType* m_src_list;
};

struct ResultListInfo {
    QString idstr;
    struct ScriptInfo* src;
};

class LocationDialog : public MythThemedDialog {
    Q_OBJECT
    public:
        LocationDialog(MythMainWindow*, QStringList, SourceManager*);
        QString getLocation();
        struct ScriptInfo* getSource();
    protected slots:
        void keyPressEvent(QKeyEvent *e);
        void doSearch();
        void itemSelected(UIListBtnTypeItem*);

    private:
        void wireUI();
        void doListSelect();
        QDict<QValueList<struct ScriptInfo*> > m_cache;
        QStringList m_types;
        SourceManager* m_src_man;
        UIListBtnType* m_list;
        UIRemoteEditType* m_edit;
        UITextButtonType* m_btn;
};

#endif
