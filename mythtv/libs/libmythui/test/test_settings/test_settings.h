#include <QTest>

#include "libmythui/standardsettings.h"

class ComboBox : public MythUIComboBoxSetting {
};

class TestSettings: public QObject
{
    Q_OBJECT

  private slots:

    static void ByName(void)
    {
        GroupSetting parent;
        auto *setting = new ButtonStandardSetting("setting");
        setting->setName("setting");
        parent.addChild(setting);

        auto *combobox = new ComboBox();
        auto *targetedSetting = new ButtonStandardSetting("targetedsetting");
        targetedSetting->setName("targetedsetting");
        combobox->addTargetedChild("target", targetedSetting);
        parent.addChild(combobox);

        QVERIFY(parent.byName("setting") == setting);
        QVERIFY(parent.byName("targetedsetting") == nullptr);
        combobox->setValue("target");
        QVERIFY(parent.byName("targetedsetting") == targetedSetting);
    }
};
