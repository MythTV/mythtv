#ifndef UIPHONEENTRY_H_
#define UIPHONEENTRY_H_

#include <qstring.h>
#include <qimage.h>
#include "xmlparse.h"

class MythDialog;

class UIPhoneEntry : public MythDialog
{
    Q_OBJECT
  public:
    UIPhoneEntry(MythMainWindow*, MythDialog *bg, const char *name = 0);
    ~UIPhoneEntry();

    enum PhoneFlagMask {
        PH_TRANS     = 0x01,
        PH_GRAYBG    = 0x02,
    };

    void Show(int x = -1, int y = -1);
    void setScale(float s) { m_scale = s; }
    void setFlags(uint f) { m_flags = f; }
    void setValue(QString v) { m_lineEdit->setText(v); }
    QString getValue() { return m_lineEdit->text(); }
    void redraw();

  signals:
    void valueChanged(QString);

  protected:
    void paintEvent(QPaintEvent*);
    void timerEvent(QTimerEvent*);
    bool eventFilter(QObject*, QEvent*);

  protected slots:
    virtual void done(int);

  private:
    void grayOut(QPainter*, QSize);
    void keyClicked(QString);
    void keyBlink(const QPoint&);
    void switchCaps();
    void loadWindow(QDomElement&);

    bool m_active;
    uint m_flags;
    float m_scale;
    bool m_trans;
    int m_lastIndex;
    int m_blinkId, m_delayId;
    bool m_drawBlink, m_inDelay;
    bool m_capsLock;
    MythDialog *m_bg;
    QString m_lastKey;
    QPixmap *m_bgBackup;
    QPoint m_buttonOffset;
    QLineEdit *m_lineEdit;
    XMLParse *m_theme;
    QSize m_buttonSize;
    QRect m_phoneRect;
    QRect m_buttonsRect;
};

#endif
