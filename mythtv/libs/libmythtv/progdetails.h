#ifndef PROGDETAILS_H_
#define PROGDETAILS_H_

#include <qstring.h>
#include <qimage.h>

#include "mythdialogs.h"
#include "uitypes.h"

class ProgDetails : public MythThemedDialog
{
    Q_OBJECT
  public:
      ProgDetails(MythMainWindow *parent, 
                  QString windowName,
                  QString details = "");
      ~ProgDetails();

      QString themeText(const QString &fontName, const QString &text, int size);
      void setDetails(const QString &details);

  protected slots:
    virtual void keyPressEvent(QKeyEvent *e);
    void done(void);

  private:
    void wireUpTheme(void);

    UIRichTextType   *m_richText;
    UITextButtonType *m_okButton;

    QString           m_details;
};

#endif
