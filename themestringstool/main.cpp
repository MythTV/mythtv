#include <QApplication>
#include <QDomDocument>
#include <QString>
#include <QMap>
#include <QMultiMap>
#include <QStringList>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QtGui>

#include <iostream>
#include <cstdlib>

using namespace std;

QString getFirstText(QDomElement element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return QString();
}

QString indir;
QString outfilebase;
QString outfile;
QFile fstringout;
QTextStream fdataout;
QMap<QString, QFile *> transFiles;
QMultiMap<QString, QString> translatedStrings;
QString laststring;
int totalStringCount = 0;
int stringCount = 0;
QStringList strings;

void parseElement(QDomElement &element)
{
    laststring.clear();

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "value" || // UI theme
                info.tagName() == "template" || // UI theme
                info.tagName() == "helptext" || // UI theme
                info.tagName() == "text" || // Menu theme
                info.tagName() == "description") // Menu theme
            {
                if (info.attribute("lang", "").isEmpty())
                {
                    laststring = getFirstText(info);
                    if (!laststring.trimmed().isEmpty())
                    {
                        // Escape quotes
                        laststring.replace("\"", QString("\\\""));
                        // Escape xml-escaped newline
                        laststring.replace("\\n", QString("<newline>"));

                        // Remove newline whitespace added by
                        // xml formatting
                        QStringList lines = laststring.split('\n');
                        QStringList::iterator lineIt;
                        for (lineIt = lines.begin(); lineIt != lines.end();
                             ++lineIt)
                        {
                            (*lineIt) = (*lineIt).trimmed();
                        }
                        laststring = lines.join(" ");

                        laststring.replace(QString("<newline>"), QString("\\n"));

                        if (!strings.contains(laststring))
                            strings << laststring;
                        ++stringCount;
                        ++totalStringCount;
                    }
                }
                else
                {
                    QString language = info.attribute("lang", "").toLower();
                    translatedStrings.insert(laststring, language + "{}" + getFirstText(info));
                }
            }
            else
                parseElement(info);
        }
    }
}

void parseDirectory(QString dir)
{
    QDir themeDir(dir);

    cout << "Searching directory: " << qPrintable(themeDir.path()) << endl;

    themeDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    themeDir.setSorting(QDir::DirsFirst);

    QDomDocument doc;
    QFileInfoList themeFiles = themeDir.entryInfoList();
    QFileInfoList::const_iterator it;
    for (it = themeFiles.begin(); it != themeFiles.end(); ++it)
    {
        if ((*it).isDir())
        {
            parseDirectory((*it).filePath());
            continue;
        }

        if ((*it).suffix() != "xml")
            continue;

        cout << "  Found: " << qPrintable((*it).filePath()) << endl;

        QFile fin((*it).absoluteFilePath());

        if (!fin.open(QIODevice::ReadOnly))
        {
            cerr << "Can't open " << qPrintable((*it).absoluteFilePath()) << endl;
            continue;
        }

        QString errorMsg;
        int errorLine = 0;
        int errorColumn = 0;

        if (!doc.setContent(&fin, false, &errorMsg, &errorLine, &errorColumn))
        {
            cerr << "Error parsing: " << qPrintable((*it).absoluteFilePath()) << endl;
            cerr << "at line: " << errorLine << "  column: "
                 << errorColumn << endl;
            cerr << qPrintable(errorMsg) << endl;
            fin.close();
            continue;
        }

        fin.close();

        stringCount = 0;

        QDomElement docElem = doc.documentElement();
        QDomNode n = docElem.firstChild();
        while (!n.isNull())
        {
            QDomElement e = n.toElement();
            if (!e.isNull())
            {
                parseElement(e);
            }
            n = n.nextSibling();
        }

        cout << "    Contains " << stringCount << " total strings" << endl;
    }

}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);

    if (a.argc() < 2)
    {
        cerr << "You must specify at least a starting directory." << endl;
        a.exit(-1);
    }

    indir = a.argv()[1];
    if (a.argc() == 3)
        outfilebase = a.argv()[2];
    else
        outfilebase = indir;

    if (indir.isEmpty() || outfilebase.isEmpty())
    {
        cerr << "no filenames\n";
        exit(-1);
    }

    QDir themeDir(indir);
    if (!themeDir.exists())
    {
        cerr << "Starting directory does not exist\n";
        exit(-1);
    }

    outfile = outfilebase + '/' + "themestrings.h";

    parseDirectory(indir);

    fstringout.setFileName(outfile);

    if (!fstringout.open(QIODevice::WriteOnly))
    {
        cerr << "can't open " << qPrintable(outfile) << " for writing\n";
        exit(-1);
    }

    fdataout.setDevice(&fstringout);

    fdataout << QString("void strings_null() {\n");

    int lineCount = 2;
    QStringList::const_iterator strit;
    for (strit = strings.begin(); strit != strings.end(); ++strit)
    {
        QString string = (*strit);
        if (string.contains("%n"))
            fdataout << QString("    ThemeUI::tr(\"%1\", 0, 1);\n")
                                .arg((*strit).toUtf8().data());
        else
            fdataout << QString("    ThemeUI::tr(\"%1\");\n")
                                .arg((*strit).toUtf8().data());

#if 0
        if (translatedStrings.contains(*strit))
        {
            QStringList prevSeenLanguages;
            QList<QString> values = translatedStrings.values(*strit);
            for (int i = 0; i < values.size(); ++i)
            {
                QString language = values.at(i).section("{}", 0, 0);
                if (prevSeenLanguages.contains(language))
                    continue;
                prevSeenLanguages << language;

                QString translation = values.at(i).section("{}", 1, 1);
                if (!transFiles.contains(language))
                {
                    QFile *tmp = new QFile(outfile + '_' + language + ".ts");
                    if (!tmp->open(QIODevice::WriteOnly))
                    {
                        cerr << "couldn't open language file\n";
                        exit(-1);
                    }

                    transFiles[language] = tmp;
                }

                QTextStream dstream(transFiles[language]);
                dstream.setCodec("UTF-8");

                dstream << "    <message>\n"
                        << "        <location filename=\"" << qPrintable(outfile) << "\" line=\"" << lineCount << "\"/>\n"
                        << "        <source>" << Qt::escape(*strit).replace("\"", "&quot;") << "<source>\n"
                        << "        <translation>" << Qt::escape(translation).replace("\"", "&quot;") << "<translation>\n"
                        << "    <message>\n";
            }
        }
#endif
        ++lineCount;
    }

    fdataout << QString("}\n");
    fstringout.close();

#if 0
    QMap<QString, QFile *>::Iterator it;
    for (it = transFiles.begin(); it != transFiles.end(); ++it)
    {
        it.value()->close();
    }
#endif

    cout << endl;
    cout << "---------------------------------------" << endl;
    cout << "Found " << totalStringCount << " total strings" << endl;
    cout << strings.count() << " unique" << endl;

    return 0;
}

