#ifndef MYTHUIVIRTUALKEYBOARD_H_
#define MYTHUIVIRTUALKEYBOARD_H_

#include "mythscreentype.h"

class QKeyEvent;

/// Preferred position to place virtual keyboard popup.
enum PopupPosition
{
    VK_POSABOVEEDIT = 1,
    VK_POSBELOWEDIT,
    VK_POSTOPDIALOG,
    VK_POSBOTTOMDIALOG,
    VK_POSCENTERDIALOG
};

typedef struct KeyDefinition
{
    QString name;
    QString type;
    QString normal, shift, alt, altshift;
    QString up, down, left, right;
};

const int numcomps = 95;

const QString comps[numcomps][3] = {
        {"!", "!", (QChar)0xa1},    {"c", "/", (QChar)0xa2},
        {"l", "-", (QChar)0xa3},    {"o", "x", (QChar)0xa4},
        {"y", "-", (QChar)0xa5},    {"|", "|", (QChar)0xa6},
        {"s", "o", (QChar)0xa7},    {"\"", "\"", (QChar)0xa8},
        {"c", "o", (QChar)0xa9},    {"-", "a", (QChar)0xaa},
        {"<", "<", (QChar)0xab},    {"-", "|", (QChar)0xac},
        {"-", "-", (QChar)0xad},    {"r", "o", (QChar)0xae},
        {"^", "-", (QChar)0xaf},    {"^", "0", (QChar)0xb0},
        {"+", "-", (QChar)0xb1},    {"^", "2", (QChar)0xb2},
        {"^", "3", (QChar)0xb3},    {"/", "/", (QChar)0xb4},
        {"/", "u", (QChar)0xb5},    {"P", "!", (QChar)0xb6},
        {"^", ".", (QChar)0xb7},    {",", ",", (QChar)0xb8},
        {"^", "1", (QChar)0xb9},    {"_", "o", (QChar)0xba},
        {">", ">", (QChar)0xbb},    {"1", "4", (QChar)0xbc},
        {"1", "2", (QChar)0xbd},    {"3", "4", (QChar)0xbe},
        {"?", "?", (QChar)0xbf},    {"A", "`", (QChar)0xc0},
        {"A", "'", (QChar)0xc1},    {"A", "^", (QChar)0xc2},
        {"A", "~", (QChar)0xc3},    {"A", "\"", (QChar)0xc4},
        {"A", "*", (QChar)0xc5},    {"A", "E", (QChar)0xc6},
        {"C", ",", (QChar)0xc7},    {"E", "`", (QChar)0xc8},
        {"E", "'", (QChar)0xc9},    {"E", "^", (QChar)0xca},
        {"E", "\"", (QChar)0xcb},   {"I", "`", (QChar)0xcc},
        {"I", "'", (QChar)0xcd},    {"I", "^", (QChar)0xce},
        {"I", "\"", (QChar)0xcf},   {"D", "-", (QChar)0xd0},
        {"N", "~", (QChar)0xd1},    {"O", "`", (QChar)0xd2},
        {"O", "'", (QChar)0xd3},    {"O", "^", (QChar)0xd4},
        {"O", "~", (QChar)0xd5},    {"O", "\"", (QChar)0xd6},
        {"x", "x", (QChar)0xd7},    {"O", "/", (QChar)0xd8},
        {"U", "`", (QChar)0xd9},    {"U", "'", (QChar)0xda},
        {"U", "^", (QChar)0xdb},    {"U", "\"", (QChar)0xdc},
        {"Y", "'", (QChar)0xdd},    {"T", "H", (QChar)0xde},
        {"s", "s", (QChar)0xdf},    {"a", "`", (QChar)0xe0},
        {"a", "'", (QChar)0xe1},    {"a", "^", (QChar)0xe2},
        {"a", "~", (QChar)0xe3},    {"a", "\"", (QChar)0xe4},
        {"a", "*", (QChar)0xe5},    {"a", "e", (QChar)0xe6},
        {"c", ",", (QChar)0xe7},    {"e", "`", (QChar)0xe8},
        {"e", "'", (QChar)0xe9},    {"e", "^", (QChar)0xea},
        {"e", "\"", (QChar)0xeb},   {"i", "`", (QChar)0xec},
        {"i", "'", (QChar)0xed},    {"i", "^", (QChar)0xee},
        {"i", "\"", (QChar)0xef},   {"d", "-", (QChar)0xf0},
        {"n", "~", (QChar)0xf1},    {"o", "`", (QChar)0xf2},
        {"o", "'", (QChar)0xf3},    {"o", "^", (QChar)0xf4},
        {"o", "~", (QChar)0xf5},    {"o", "\"", (QChar)0xf6},
        {"-", ":", (QChar)0xf7},    {"o", "/", (QChar)0xf8},
        {"u", "`", (QChar)0xf9},    {"u", "'", (QChar)0xfa},
        {"u", "^", (QChar)0xfb},    {"u", "\"", (QChar)0xfc},
        {"y", "'", (QChar)0xfd},    {"t", "h", (QChar)0xfe},
        {"y", "\"", (QChar)0xff}
};

class MPUBLIC MythUIVirtualKeyboard : public MythScreenType
{
  Q_OBJECT

  public:
    MythUIVirtualKeyboard(MythScreenStack *parentStack,  MythUITextEdit *m_parentEdit);
    ~MythUIVirtualKeyboard();
    bool Create(void);
    bool keyPressEvent(QKeyEvent *e);

  signals:
    void keyPressed(QString key);

  protected slots:
    void charClicked(void);
    void shiftClicked(void);
    void delClicked(void);
    void lockClicked(void);
    void altClicked(void);
    void compClicked(void);
    void moveleftClicked(void);
    void moverightClicked(void);
    void backClicked(void);
    void returnClicked(void);

  private:
    void loadKeyDefinitions(const QString &lang);
    void parseKey(const QDomElement &element);
    void updateKeys(bool connectSignals = false);
    QString decodeChar(QString c);
    QString getKeyText(KeyDefinition key);

    MythUITextEdit *m_parentEdit;
    PopupPosition   m_preferredPos;

    QMap<QString, KeyDefinition> m_keyMap;

    MythUIButton *m_lockButton;
    MythUIButton *m_altButton;
    MythUIButton *m_compButton;
    MythUIButton *m_shiftLButton;
    MythUIButton *m_shiftRButton;

    bool          m_shift;
    bool          m_alt;
    bool          m_lock;

    bool          m_composing;
    QString       m_composeStr;
};

#endif
