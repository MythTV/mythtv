/*
  playlistfile (.pls) parser
  Eskil Heyn Olsen, 2005, distributed under the GPL as part of mythtv.

  Update July 2010 updated for Qt4 (Paul Harrison)
*/

#ifndef PLS_H_
#define PLS_H_

#include <QString>
#include <QBuffer>
#include <QList>
#include <QTextStream>

/** \brief Class for representing entries in a pls file
 */
class PlayListFileEntry
{
  public:
    PlayListFileEntry(void) {}
    ~PlayListFileEntry(void) {}

    QString File(void) { return m_file; }
    QString Title(void) { return m_title; }
    int Length(void) { return m_length; }

    void setFile(const QString &f) { m_file = f; }
    void setTitle(const QString &t) { m_title = t; }
    void setLength(int l) { m_length = l; }

  private:
    QString m_file;
    QString m_title;
    int     m_length;
};

/** \brief Class for containing the info of a pls file
 */
class PlayListFile
{
  public:
    PlayListFile(void);
    ~PlayListFile(void);

    /** \brief Get the number of entries in the pls file

        This returns the number of actual parsed entries, not the
        <tt>numberofentries</tt> field.

        \returns the number of entries
    */
    int size(void) const { return m_entries.count(); }

    /** \brief Get a pls file entry
        \param i which entry to get, between 0 and the value returned by calling \p PlayListParser::size
        \returns a pointer to a \p PlayListEntry
    */
    PlayListFileEntry* get(int i) 
    {
        if (i >= 0 && i < m_entries.count())
            return m_entries.at(i);

        return NULL;
    }

    /** \brief Version of the parsed pls file 

        Returns the version number specified in the <tt>version</tt>
        field of the pls file.

        \returns the version number
     */
    int version(void) const { return m_version; }

    /** Add a entry to the playlist 
        \param e a \p PlayListFileEntry object
     */
    void add(PlayListFileEntry *e) { m_entries.append(e); }

    /** Clear out all the entries */
    void clear(void) 
    {
        while (!m_entries.isEmpty())
            delete m_entries.takeFirst();
    }

    /** \brief Parse a pls file.
        \param stream the playlist file in a \p QTextStream
        \returns the number of entries parsed 
    */
    static int parse(PlayListFile *pls, QTextStream *stream);

  private:
    QList<PlayListFileEntry*> m_entries;
    int m_version;
};

#endif /* PLS_H_ */
