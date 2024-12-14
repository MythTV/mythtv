#include <QCoreApplication>
#include <QDomDocument>
#include <QString>
#include <QStringList>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif
#include <QMap>
#include <QMultiMap>
#include <QStringList>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QtGui>

#include <iostream>
#include <cstdlib>

QString getFirstText(QDomElement element);
void parseElement(QDomElement &element);
void parseDirectory(QString dir);

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

    std::cout << "Searching directory: " << qPrintable(themeDir.path()) << std::endl;

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

        std::cout << "  Found: " << qPrintable((*it).filePath()) << std::endl;

        QFile fin((*it).absoluteFilePath());

        if (!fin.open(QIODevice::ReadOnly))
        {
            std::cerr << "Can't open " << qPrintable((*it).absoluteFilePath()) << std::endl;
            continue;
        }

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        QString errorMsg;
        int errorLine = 0;
        int errorColumn = 0;

        if (!doc.setContent(&fin, false, &errorMsg, &errorLine, &errorColumn))
        {
            std::cerr << "Error parsing: " << qPrintable((*it).absoluteFilePath()) << std::endl;
            std::cerr << "at line: " << errorLine << "  column: "
                      << errorColumn << std::endl;
            std::cerr << qPrintable(errorMsg) << std::endl;
            fin.close();
            continue;
        }
#else
        auto parseResult = doc.setContent(&fin);
        if (!parseResult)
        {
            std::cerr << "Error parsing: " << qPrintable((*it).absoluteFilePath()) << std::endl;
            std::cerr << "at line: " << parseResult.errorLine << "  column: "
                      << parseResult.errorColumn << std::endl;
            std::cerr << qPrintable(parseResult.errorMessage) << std::endl;
            fin.close();
            continue;
        }
#endif

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

        std::cout << "    Contains " << stringCount << " total strings" << std::endl;
    }

}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QStringList args = a.arguments();

    if (args.count() < 2)
    {
        std::cerr << "You must specify at least a starting directory." << std::endl;
        a.exit(-1);
    }

    indir = args.at(1);
    if (args.count() == 3)
        outfilebase = args.at(2);
    else
        outfilebase = indir;

    if (indir.isEmpty() || outfilebase.isEmpty())
    {
        std::cerr << "no filenames\n";
        exit(-1);
    }

    QDir themeDir(indir);
    if (!themeDir.exists())
    {
        std::cerr << "Starting directory does not exist\n";
        exit(-1);
    }

    outfile = outfilebase + '/' + "themestrings.h";

    parseDirectory(indir);

    fstringout.setFileName(outfile);

    if (!fstringout.open(QIODevice::WriteOnly))
    {
        std::cerr << "can't open " << qPrintable(outfile) << " for writing\n";
        exit(-1);
    }

    fdataout.setDevice(&fstringout);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    fdataout.setCodec("UTF-8");
#else
    fdataout.setEncoding(QStringConverter::Utf8);
#endif

    fdataout << QString("// This is an automatically generated file\n");
    fdataout << QString("// Do not edit\n\n");
    fdataout << QString("void strings_null() {\n");

#if 0
    int lineCount = 2;
#endif
    strings.sort();
    QStringList::const_iterator strit;
    for (strit = strings.begin(); strit != strings.end(); ++strit)
    {
        QString string = (*strit);
        if (string.contains("%n"))
            fdataout << QString("    ThemeUI::tr(\"%1\", 0, 1);\n")
                                .arg(string);
        else
            fdataout << QString("    ThemeUI::tr(\"%1\");\n")
                                .arg(string);

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
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                dstream.setCodec("UTF-8");
#else
                dstream.setEncoding(QStringConverter::Utf8);
#endif

                dstream << "    <message>\n"
                        << "        <location filename=\"" << qPrintable(outfile) << "\" line=\"" << lineCount << "\"/>\n"
                        << "        <source>" << Qt::escape(*strit).replace("\"", "&quot;") << "<source>\n"
                        << "        <translation>" << Qt::escape(translation).replace("\"", "&quot;") << "<translation>\n"
                        << "    <message>\n";
            }
        }
        ++lineCount;
#endif
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

    std::cout << std::endl;
    std::cout << "---------------------------------------" << std::endl;
    std::cout << "Found " << totalStringCount << " total strings" << std::endl;
    std::cout << strings.count() << " unique" << std::endl;

    return 0;
}

