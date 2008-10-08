#ifndef MYTHSETTINGS_H
#define MYTHSETTINGS_H

#include "mythtv/settings.h"
#include "mythtv/mythcontext.h"

class QKeyEvent;
class QEvent;
class Q3MythListView;
class Q3ListViewItem;

class MusicGeneralSettings : public ConfigurationWizard
{
  public:
    MusicGeneralSettings();
};

class MusicPlayerSettings : public QObject, public ConfigurationWizard
{
  Q_OBJECT

  public:
    MusicPlayerSettings();
    ~MusicPlayerSettings() {};

  public slots:
    void showVisEditor(void);

  private:
    HostLineEdit *visModesEdit;
};

class MusicRipperSettings : public ConfigurationWizard
{
  public:
    MusicRipperSettings();
};


class VisualizationsEditor : public MythDialog
{
  Q_OBJECT
  public:

    VisualizationsEditor(const QString &currentSelection, MythMainWindow *parent,
                         const char *name = 0);
    ~VisualizationsEditor(void);

    QString getSelectedModes(void);

  protected slots:
    void okClicked(void);
    void cancelClicked(void);
    void upClicked(void);
    void downClicked(void);
    void availableChanged(Q3ListViewItem *item);
    void selectedChanged(Q3ListViewItem *item);
    void availableOnSelect(Q3ListViewItem *item);
    void selectedOnSelect(Q3ListViewItem *item);

  protected:
    bool eventFilter(QObject *obj, QEvent *ev);
    bool handleKeyPress(QKeyEvent *e);

    private:
    void fillWidgets(const QString &currentSelection);

    Q3MythListView *availableList;
    Q3MythListView *selectedList;
};

#endif
