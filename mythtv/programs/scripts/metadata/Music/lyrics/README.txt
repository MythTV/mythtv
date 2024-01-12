 INFO FOR SKINNERS & ADDON DEVS
================================

control id's:
110 - list (lyrics text)
200 - label (used lyrics scraper)


listitem properties:
Container(110).ListItem.Property(part1)    - first word of the current line
Container(110).ListItem.Property(part2)    - second word of the current line
Container(110).ListItem.Property(part3)    - third word of the current line
Container(110).ListItem.Property(part4)    - rest of the current line
Container(110).ListItem.Property(duration) - time the current line will be shown


window properties:
Window(Home).Property(culrc.lyrics)   - shows the current lyrics, including timing info in case of lrc lyrics.
Window(Home).Property(culrc.source)   - source or scraper that was used to find the current lyrics.
Window(Home).Property(culrc.haslist)  - will be 'true' if multiple lyrics are available, empty if not.
Window(Home).Property(culrc.islrc)    - returns 'true' when the lyrics are lrc based, empty if not.
Window(Home).Property(culrc.running)  - returns 'true' when the lyrics script is running, empty if not.


music addons can add lyrics to their items this way:
https://codedocs.xyz/xbmc/xbmc/group__python__xbmcgui__listitem.html
listitem.setInfo('music', {'lyrics': 'lyrics here'})
listitem.setProperty('culrc.source', 'addon name here')
