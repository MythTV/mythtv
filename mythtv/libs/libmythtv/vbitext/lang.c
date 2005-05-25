#include <string.h>
#include "vt.h"
#include "lang.h"

int latin1 = -1;

static unsigned char lang_char[256];

static unsigned char lang_chars[1+8+8][16] =
{
    { 0, 0x23,0x24,0x40,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0x7b,0x7c,0x7d,0x7e },

    // for latin-1 font
    // English (100%)
    { 0,  '£', '$', '@', '«', '½', '»', '¬', '#', '­', '¼', '¦', '¾', '÷' },
    // German (100%)
    { 0,  '#', '$', '§', 'Ä', 'Ö', 'Ü', '^', '_', '°', 'ä', 'ö', 'ü', 'ß' },
    // Swedish/Finnish/Hungarian (100%)
    { 0,  '#', '¤', 'É', 'Ä', 'Ö', 'Å', 'Ü', '_', 'é', 'ä', 'ö', 'å', 'ü' },
    // Italian (100%)
    { 0,  '£', '$', 'é', '°', 'ç', '»', '¬', '#', 'ù', 'à', 'ò', 'è', 'ì' },
    // French (100%)
    { 0,  'é', 'ï', 'à', 'ë', 'ê', 'ù', 'î', '#', 'è', 'â', 'ô', 'û', 'ç' },
    // Portuguese/Spanish (100%)
    { 0,  'ç', '$', '¡', 'á', 'é', 'í', 'ó', 'ú', '¿', 'ü', 'ñ', 'è', 'à' },
    // Czech/Slovak (60%)
    { 0,  '#', 'u', 'c', 't', 'z', 'ý', 'í', 'r', 'é', 'á', 'e', 'ú', 's' },
    // reserved (English mapping)
    { 0,  '£', '$', '@', '«', '½', '»', '¬', '#', '­', '¼', '¦', '¾', '÷' },

    // for latin-2 font
    // Polish (100%)
    { 0,  '#', 'ñ', '±', '¯', '¦', '£', 'æ', 'ó', 'ê', '¿', '¶', '³', '¼' },
    // German (100%)
    { 0,  '#', '$', '§', 'Ä', 'Ö', 'Ü', '^', '_', '°', 'ä', 'ö', 'ü', 'ß' },
    // Estonian (100%)
    { 0,  '#', 'õ', '©', 'Ä', 'Ö', '®', 'Ü', 'Õ', '¹', 'ä', 'ö', '¾', 'ü' },
    // Lettish/Lithuanian (90%)
    { 0,  '#', '$', '©', 'ë', 'ê', '®', 'è', 'ü', '¹', '±', 'u', '¾', 'i' },
    // French (90%)
    { 0,  'é', 'i', 'a', 'ë', 'ì', 'u', 'î', '#', 'e', 'â', 'ô', 'u', 'ç' },
    // Serbian/Croation/Slovenian (100%)
    { 0,  '#', 'Ë', 'È', 'Æ', '®', 'Ð', '©', 'ë', 'è', 'æ', '®', 'ð', '¹' },
    // Czech/Slovak (100%)
    { 0,  '#', 'ù', 'è', '»', '¾', 'ý', 'í', 'ø', 'é', 'á', 'ì', 'ú', '¹' },
    // Rumanian (95%)
    { 0,  '#', '¢', 'Þ', 'Â', 'ª', 'Ã', 'Î', 'i', 'þ', 'â', 'º', 'ã', 'î' },
};

/* Yankable latin charset :-)
     !"#$%&'()*+,-./0123456789:;<=>?
    @ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_
    `abcdefghijklmnopqrstuvwxyz{|}~
     ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿
    ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞß
    àáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ
*/



static struct mark { char *g0, *latin1, *latin2; } marks[16] =
{
    /* none */         { "#",
                         "¤",
                         "$"                                   },
    /* grave - ` */    { " aeiouAEIOU",
                         "`àèìòùÀÈÌÒÙ",
                         "`aeiouAEIOU"                         },
    /* acute - ' */    { " aceilnorsuyzACEILNORSUYZ",
                         "'ácéílnórsúýzÁCÉÍLNÓRSÚÝZ",
                         "'áæéíåñóà¶úý¼ÁÆÉÍÅÑÓÀ¦ÚÝ¬"           },
    /* cirumflex - ^ */        { " aeiouAEIOU",
                         "^âêîôûÂÊÎÔÛ",
                         "^âeîôuÂEÎÔU"                         },
    /* tilde - ~ */    { " anoANO",
                         "~ãñõÃÑÕ",
                         "~anoANO"                             },
    /* ??? - ¯ */      { "",
                         "",
                         ""                                    },
    /* breve - u */    { "aA",
                         "aA",
                         "ãÃ"                                  },
    /* abovedot - · */ { "zZ",
                         "zZ",
                         "¿¯"                                  },
    /* diaeresis ¨ */  { "aeiouAEIOU",
                         "äëïöüÄËÏÖÜ",
                         "äëiöüÄËIÖÜ"                          },
    /* ??? - . */      { "",
                         "",
                         ""                                    },
    /* ringabove - ° */        { " auAU",
                         "°åuÅU",
                         "°aùAÙ"                               },
    /* cedilla - ¸ */  { "cstCST",
                         "çstÇST",
                         "çºþÇªÞ"                              },
    /* ??? - _ */      { " ",
                         "_",
                         "_"                                   },
    /* dbl acute - " */        { " ouOU",
                         "\"ouOU",
                         "\"õûÕÛ"                              },
    /* ogonek - \, */  { "aeAE",
                         "aeAE",
                         "±ê¡Ê"                                },
    /* caron - v */    { "cdelnrstzCDELNRSTZ",
                         "cdelnrstzCDELNRSTZ",
                         "èïìµòø¹»¾ÈÏÌ¥ÒØ©«®"                  },
};

static unsigned char g2map_latin1[] =
   /*0123456789abcdef*/
    " ¡¢£$¥#§¤'\"«    "
    "°±²³×µ¶·÷'\"»¼½¾¿"
    " `´^~   ¨.°¸_\"  "
    "_¹®©            "
    " ÆÐªH ILLØ ºÞTNn"
    "Kædðhiillø ßþtn\x7f";

static unsigned char g2map_latin2[] =
   /*0123456789abcdef*/
    " icL$Y#§¤'\"<    "
    "°   ×u  ÷'\">    "
    " `´^~ ¢ÿ¨.°¸_½²·"
    "- RC            "
    "  ÐaH iL£O opTNn"
    "K ðdhiil³o ßptn\x7f";



void
lang_init(void)
{
    int i;

    memset(lang_char, 0, sizeof(lang_char));
    for (i = 1; i <= 13; i++)
       lang_char[lang_chars[0][i]] = i;
}


void
conv2latin(unsigned char *p, int n, int lang)
{
    int c, gfx = 0;

    while (n--)
    {
       if (lang_char[c = *p])
       {
           if (! gfx || (c & 0xa0) != 0x20)
               *p = lang_chars[lang + 1][lang_char[c]];
       }
       else if ((c & 0xe8) == 0)
           gfx = c & 0x10;
       p++;
    }
}



void
init_enhance(struct enhance *eh)
{
    eh->next_des = 0;
}

void
add_enhance(struct enhance *eh, int dcode, unsigned int *t)
{
    if (dcode == eh->next_des)
    {
       memcpy(eh->trip + dcode * 13, t, 13 * sizeof(*t));
       eh->next_des++;
    }
    else
       eh->next_des = -1;
}

void
enhance(struct enhance *eh, struct vt_page *vtp)
{
    int row = 0;
    unsigned int *p, *e;

    if (eh->next_des < 1)
       return;

    for (p = eh->trip, e = p + eh->next_des * 13; p < e; p++)
       if (*p % 2048 != 2047)
       {
           int adr = *p % 64;
           int mode = *p / 64 % 32;
           int data = *p / 2048 % 128;

           //printf("%2x,%d,%d ", mode, adr, data);
           if (adr < 40)
           {
               // col functions
               switch (mode)
               {
                   case 15: // char from G2 set
                       if (adr < VT_WIDTH && row < VT_HEIGHT)
                       {
                           if (latin1)
                               vtp->data[row][adr] = g2map_latin1[data-32];
                           else
                               vtp->data[row][adr] = g2map_latin2[data-32];
                       }
                       break;
                   case 16 ... 31: // char from G0 set with diacritical mark
                       if (adr < VT_WIDTH && row < VT_HEIGHT)
                       {
                           struct mark *mark = marks + (mode - 16);
                           char *x;

                           if ((x = strchr(mark->g0, data)))
                           {
                               if (latin1)
                                   data = mark->latin1[x - mark->g0];
                               else
                                   data = mark->latin2[x - mark->g0];
                           }
                           vtp->data[row][adr] = data;
                       }
                       break;
               }
           }
           else
           {
               // row functions
               if ((adr -= 40) == 0)
                   adr = 24;

               switch (mode)
               {
                   case 1: // full row color
                       row = adr;
                       break;
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
    //printf("\n");
}

