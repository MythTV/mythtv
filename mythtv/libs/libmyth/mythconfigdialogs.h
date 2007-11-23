// -*- Mode: c++ -*-

#ifndef MYTH_CONFIG_DIALOGS_H
#define MYTH_CONFIG_DIALOGS_H

// Qt headers
#include <qobject.h>
#include <qstring.h>
#include <qdeepcopy.h>

// MythTV headers
#include "mythexp.h"
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "mythdbcon.h"
#include "mythstorage.h"
#include "mythconfiggroups.h"

class MPUBLIC ConfigPopupDialogWidget : public MythPopupBox
{
    Q_OBJECT

        public:
    ConfigPopupDialogWidget(MythMainWindow *parent, const char *widgetName) :
        MythPopupBox(parent, widgetName) { }

    virtual void keyPressEvent(QKeyEvent *e);
    void accept() { MythPopupBox::accept(); }
    void reject() { MythPopupBox::reject(); }

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~ConfigPopupDialogWidget() { }
};

class MPUBLIC ConfigurationPopupDialog : public VerticalConfigurationGroup
{
    Q_OBJECT

        public:
    ConfigurationPopupDialog() :
        VerticalConfigurationGroup(), dialog(NULL), label(NULL) { }

    virtual void deleteLater(void);

    virtual MythDialog *dialogWidget(
        MythMainWindow *parent, const char *widgetName);

    virtual DialogCode exec(bool saveOnAccept = true);

    virtual void setLabel(QString str);

    public slots:
        void accept(void) { if (dialog) dialog->accept(); }
    void reject(void) { if (dialog) dialog->reject(); }

  signals:
    void popupDone(int);

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~ConfigurationPopupDialog() { }

  protected:
    ConfigPopupDialogWidget *dialog;
    QLabel                  *label;
};

class MPUBLIC ConfigurationDialogWidget : public MythDialog
{
    Q_OBJECT

        public:
    ConfigurationDialogWidget(MythMainWindow *parent,
                              const char     *widgetName) :
        MythDialog(parent, widgetName) { }

    virtual void keyPressEvent(QKeyEvent *e);

  signals:
    void editButtonPressed(void);
    void deleteButtonPressed(void);
};

/** \class ConfigurationDialog
 *  \brief A ConfigurationDialog that uses a ConfigurationGroup
 *         all children on one page in a vertical layout.
 */
class MPUBLIC ConfigurationDialog : public Storage
{
  public:
    ConfigurationDialog() : dialog(NULL), cfgGrp(new ConfigurationGroup()) { }
    virtual ~ConfigurationDialog();

    // Make a modal dialog containing configWidget
    virtual MythDialog *dialogWidget(MythMainWindow *parent,
                                     const char     *widgetName);

    // Show a dialogWidget, and save if accepted
    virtual DialogCode exec(bool saveOnExec = true, bool doLoad = true);

    virtual void load(void) { cfgGrp->load(); }
    virtual void save(void) { cfgGrp->save(); }
    virtual void save(QString destination) { cfgGrp->save(destination); }

    virtual void addChild(Configurable *child);

    virtual Setting *byName(const QString &settingName)
        { return cfgGrp->byName(settingName); }

    void setLabel(const QString &label);

  protected:
    typedef vector<Configurable*> ChildList;

    ChildList           cfgChildren;
    vector<QWidget*>    childwidget;
    MythDialog         *dialog;
    ConfigurationGroup *cfgGrp;
};

/** \class ConfigurationWizard
 *  \brief A ConfigurationDialog that uses a ConfigurationGroup
 *         with one child per page.
 */
class MPUBLIC ConfigurationWizard : public ConfigurationDialog
{
  public:
    ConfigurationWizard() : ConfigurationDialog() {}

    virtual MythDialog *dialogWidget(MythMainWindow *parent,
                                     const char *widgetName);
};

/** \class JumpConfigurationWizard
 *  \brief A jump wizard is a group with one child per page, and jump buttons
 */
class MPUBLIC JumpConfigurationWizard :
public QObject, public ConfigurationWizard
{
    Q_OBJECT

        public:
    virtual MythDialog *dialogWidget(MythMainWindow *parent,
                                     const char     *widgetName);

    virtual void deleteLater(void);

    protected slots:
        void showPage(QString);

  protected:
    /// You need to call deleteLater to delete QObject
    virtual ~JumpConfigurationWizard();
};

#endif // MYTH_CONFIG_DIALOGS_H
