// MythNews headers
#include "newsarticle.h"

NewsArticle::NewsArticle(const QString &title,
                         const QString &desc, const QString &articleURL,
                         const QString &thumbnail, const QString &mediaURL,
                         const QString &enclosure) :
    m_title(title),
    m_desc(desc),
    m_articleURL(articleURL),
    m_thumbnail(thumbnail),
    m_mediaURL(mediaURL),
    m_enclosure(enclosure)
{
}

NewsArticle::NewsArticle(const QString &title, const QString &desc,
                         const QString &articleURL) :
    m_title(title),
    m_desc(desc),
    m_articleURL(articleURL),
    m_thumbnail(QString::null),
    m_mediaURL(QString::null),
    m_enclosure(QString::null)
{
}

NewsArticle::NewsArticle(const QString &title) :
    m_title(title),
    m_desc(QString::null),
    m_articleURL(QString::null),
    m_thumbnail(QString::null),
    m_mediaURL(QString::null),
    m_enclosure(QString::null)
{
}

NewsArticle::NewsArticle() :
    m_title(QString::null),
    m_desc(QString::null),
    m_articleURL(QString::null),
    m_thumbnail(QString::null),
    m_mediaURL(QString::null),
    m_enclosure(QString::null)
{
}
