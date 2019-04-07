#ifndef _NEWSARTICLE_H_
#define _NEWSARTICLE_H_

// C++ headers
#include <vector>
using namespace std;

// QT headers
#include <QString>

class NewsArticle
{
  public:
    typedef vector<NewsArticle> List;

    NewsArticle(const QString &title,
                const QString &desc, const QString &articleURL,
                const QString &thumbnail, const QString &mediaURL,
                const QString &enclosure);
    NewsArticle(const QString &title,
                const QString &desc, const QString &articleURL);
    explicit NewsArticle(const QString &title);

    NewsArticle() = default;

    QString title(void)       const { return m_title;      }
    QString description(void) const { return m_desc;       }
    QString articleURL(void)  const { return m_articleURL; }
    QString thumbnail(void)   const { return m_thumbnail;  }
    QString mediaURL(void)    const { return m_mediaURL;   }
    QString enclosure(void)   const { return m_enclosure;  }

  private:
    QString   m_title;
    QString   m_desc;
    QString   m_articleURL;
    QString   m_thumbnail;
    QString   m_mediaURL;
    QString   m_enclosure;
    QString   m_enclosureType;
};

#endif // _NEWSARTICLE_H_
