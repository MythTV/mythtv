#ifndef HTTPGETVAR_H_
#define HTTPGETVAR_H_
/*
	httpgetvar.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object to hold GET variables

*/

#include <qstring.h>

class HttpGetVariable
{

  public:
  
    HttpGetVariable(const QString &text_segment);
    HttpGetVariable(const QString &label, const QString &a_value);
    ~HttpGetVariable();

    const QString& getField();
    const QString& getValue();

  private:
  
    QString field;
    QString value;
};

#endif
