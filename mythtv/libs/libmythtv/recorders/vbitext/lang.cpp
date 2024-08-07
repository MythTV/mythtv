#include <array>
#include <cstddef>
#include <cstring>
#include "vt.h"
#include "lang.h"

int isLatin1 = -1;

static std::array<uint8_t,256> lang_char;

/* Yankable latin charset :-)
     !"#$%&'()*+,-./0123456789:;<=>?
    @ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_
    `abcdefghijklmnopqrstuvwxyz{|}~
     ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿
    ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞß
    àáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ
*/



struct mark { const std::string m_g0, m_latin1, m_latin2; };
static const std::array<const mark,16> marks =
{{
    /* none */         { "#",
                         "\xA4", /* ¤ */
                         "$"                                   },
    /* grave - ` */    { " aeiouAEIOU", /* `àèìòùÀÈÌÒÙ */
                         "`\xE0\xE8\xEC\xF2\xF9\xC0\xC8\xCC\xD2\xD9",
                         "`aeiouAEIOU"                         },
    /* acute - ' */    { " aceilnorsuyzACEILNORSUYZ",
                         "'\xE1\x63\xE9\xEDln\xF3rs\xFA\xFDz\xC1\x43\xC9\xCDLN\xD3RS\xDA\xDDZ", /* 'ácéílnórsúýzÁCÉÍLNÓRSÚÝZ */
                         "'\xE1\xE6\xE9\xED\xE5\xF1\xF3\xE0\xB6\xFA\xFD\xBC\xC1\xC6\xC9\xCD\xC5\xD1\xD3\xC0\xA6\xDAݬ"  /* 'áæéíåñóà¶úý¼ÁÆÉÍÅÑÓÀ¦ÚÝ¬ */ },
    /* cirumflex - ^ */        { " aeiouAEIOU",
                         "^\xE2\xEA\xEE\xF4\xFB\xC2\xCA\xCE\xD4\xDB", /* ^âêîôûÂÊÎÔÛ */
                         "^\xE2\x65\xEE\xF4\x75\xC2\x45\xCE\xD4\x55" }, /* ^âeîôuÂEÎÔU */
    /* tilde - ~ */    { " anoANO",
                         "~\xE3\xF1\xF5\xC3\xD1\xD5", /* ~ãñõÃÑÕ */
                         "~anoANO"                             },
    /* ??? - <AF> */   { "",
                         "",
                         ""                                    },
    /* breve - u */    { "aA",
                         "aA",
                         "\xE3\xC3" /* ãÃ */                   },
    /* abovedot - · */ { "zZ",
                         "zZ",
                         "\xBF\xAF" /* ¿¯ */                   },
    /* diaeresis ¨ */  { "aeiouAEIOU",
                         "\xE4\xEB\xEF\xF6\xFC\xC4\xCB\xCF\xD6\xDC", /* äëïöüÄËÏÖÜ */
                         "\xE4\xEB\x69\xF6\xFC\xC4\xCB\x49\xD6\xDC"  /* äëiöüÄËIÖÜ */},
    /* ??? - . */      { "",
                         "",
                         ""                                    },
    /* ringabove - ° */        { " auAU",
                         "\xB0\xE5\x75\xC5\x55", /* °åuÅU */
                         "\xB0\x61\xF9\x41\xD9"  /* °aùAÙ */   },
    /* cedilla - ¸ */  { "cstCST",
                         "\xE7\x73\x74\xC7\x53\x54", /* çstÇST */
                         "\xE7\xBA\xFE\xC7\xAA\xDE"  /* çºþÇªÞ */ },
    /* ??? - _ */      { " ",
                         "_",
                         "_"                                   },
    /* dbl acute - " */        { " ouOU",
                         "\"ouOU",
                         "\x22\xF5\xFB\xD5\xDB"   /* \"õûÕÛ */ },
    /* ogonek - \, */  { "aeAE",
                         "aeAE",
                         "\xB1\xEA\xA1\xCA" /* ±ê¡Ê */ },
    /* caron - v */    { "cdelnrstzCDELNRSTZ",
                         "cdelnrstzCDELNRSTZ",
                         "\xE8\xEF\xEC\xB5\xF2\xF8\xB9\xBB\xBE\xC8\xCF̥\xD2ة\xAB\xAE" /* èïìµòø¹»¾ÈÏÌ¥ÒØ©«® */ },
}};

static const std::string g2map_latin1 =
   /*0123456789abcdef*/
    "\x20\xA1\xA2\xA3\x24\xA5\x23\xA7\xA4\x27\x22\xAB\x20\x20\x20\x20"  /*  ¡¢£$¥#§¤'\"«     */
    "\xB0\xB1\xB2\xB3\xD7\xB5\xB6\xB7\xF7\x27\x22\xBB\xBC\xBD\xBE\xBF"  /* °±²³×µ¶·÷'\"»¼½¾¿ */
    "\x20\x60\xB4\x5E\x7E\x20\x20\x20\xA8\x2e\xB0\xB8\x5F\x22\x20\x20"  /*  `´^~   ¨.°¸_\"   */
    "\x5F\xB9\xAE\xA9\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"  /* _¹®©             */
    "\x20\xC6\xD0\xAA\x48\x20\x49\x4C\x4C\xD8\x20\xBA\xDE\x54\x4E\x6E"  /*  ÆÐªH ILLØ ºÞTNn */
    "\x4B\xE6\x64\xF0\x68\x69\x69\x6C\x6C\xF8\x20\xDF\xFE\x74\x6E\x7f"; /* Kædðhiillø ßþtn\x7f" */

static const std::string g2map_latin2 =
   /*0123456789abcdef*/
    "\x20\x69\x63\x4C\x24\x59\x23\xA7\xA4\x27\x22\x3C\x20\x20\x20\x20"  /*  icL$Y#§¤'\"<     */
    "\xB0\x20\x20\x20\xD7\x75\x20\x20\xF7\x27\x22\x3E\x20\x20\x20\x20"  /* °   ×u  ÷'\">     */
    "\x20\x60\xB4\x5E\x7E\x20\xA2\xFF\xA8\x2E\xB0\xB8\x5F\xBD\xB2\xB7"  /*  `´^~ ¢ÿ¨.°¸_½²· */
    "\x2D\x20\x52\x43\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"  /* - RC             */
    "\x20\x20\xD0\x61\x48\x20\x69\x4C\xA3\x4F\x20\x6F\x70\x54\x4E\x6e"  /*   ÐaH iL£O opTNn */
    "\x4B\x20\xF0\x64\x68\x69\x69\x6C\xB3\x6F\x20\xDF\x70\x74\x6e\x7f"; /* K ðdhiil³o ßptn\x7f" */



void
lang_init(void)
{
    int i = 0;

    lang_char.fill(0);
    for (i = 1; i <= 13; i++)
        lang_char[(unsigned char)(lang_chars[0][i])] = i;
}


void
conv2latin(unsigned char *p, int n, int lang)
{
    int gfx = 0;

    while (n--)
    {
       int c = *p;
       if (lang_char[c])
       {
           if (! gfx || (c & 0xa0) != 0x20)
               *p = lang_chars[lang + 1][lang_char[c]];
       }
       else if ((c & 0xe8) == 0)
       {
           gfx = c & 0x10;
       }
       p++;
    }
}



void
init_enhance(struct enhance *eh)
{
    eh->next_des = 0;
}

void
add_enhance(struct enhance *eh, int dcode, std::array<unsigned int,13>& data)
{
    if (dcode == eh->next_des)
    {
       memcpy(eh->trip + (static_cast<ptrdiff_t>(dcode) * 13),
              data.cbegin(), data.size() * sizeof(unsigned int));
       eh->next_des++;
    }
    else
    {
       eh->next_des = -1;
    }
}

void
do_enhancements(struct enhance *eh, struct vt_page *vtp)
{
    int8_t row = 0;

    if (eh->next_des < 1)
       return;

    for (unsigned int *p = eh->trip, *e = p + (static_cast<ptrdiff_t>(eh->next_des) * 13); p < e; p++)
    {
       if (*p % 2048 != 2047)
       {
           int8_t adr = *p % 64;
           int8_t mode = *p / 64 % 32;
           int8_t data = *p / 2048 % 128;

           //printf("%2x,%d,%d ", mode, adr, data);
           if (adr < 40)
           {
               // col functions
               switch (mode)
               {
                   case 15: // char from G2 set
                       if (adr < VT_WIDTH && row < VT_HEIGHT)
                       {
                           if (isLatin1)
                               vtp->data[row][adr] = g2map_latin1[data-32];
                           else
                               vtp->data[row][adr] = g2map_latin2[data-32];
                       }
                       break;
                   case 16 ... 31: // char from G0 set with diacritical mark
                       if (adr < VT_WIDTH && row < VT_HEIGHT)
                       {
                           const struct mark *mark = &marks[mode - 16];
                           size_t index = mark->m_g0.find(data);
                           if (index != std::string::npos)
                           {
                               if (isLatin1)
                                   data = mark->m_latin1[index];
                               else
                                   data = mark->m_latin2[index];
                           }
                           vtp->data[row][adr] = data;
                       }
                       break;
               }
           }
           else
           {
               // row functions
               adr -= 40;
               if (adr == 0)
                   adr = 24;

               switch (mode)
               {
                   case 1: // full row color
                   case 4: // set active position
                       row = adr;
                       break;
                   case 7: // address row 0 (+ full row color)
                       if (adr == 23)
                           row = 0;
                       break;
               }
           }
       }
    }
    //printf("\n");
}

