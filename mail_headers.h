/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf mail_headers.gperf  */
/* Computed positions: -k'1,3,10' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 6 "mail_headers.gperf"
struct header_entry
{
  int name;
  int id;
};

enum
{
	HEADER_FROM,
	HEADER_TO,
	HEADER_CC,
	HEADER_SUBJECT,
	HEADER_DATE,
	HEADER_REPLY_TO,
	HEADER_RECEIVED,
	HEADER_MIME_VERSION,
	HEADER_CONTENT_DISPOSITION,
	HEADER_CONTENT_TYPE,
	HEADER_CONTENT_ID,
	HEADER_MESSAGE_ID,
	HEADER_IN_REPLY_TO,
	HEADER_CONTENT_TRANSFER_ENCODING,
	HEADER_CONTENT_DESCRIPTION,
	HEADER_IMPORTANCE,
	HEADER_X_PRIORITY,
	HEADER_X_SIMPLEMAIL_POP3,
	HEADER_X_SIMPLEMAIL_PARTIAL
};

#define TOTAL_KEYWORDS 19
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 25
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 35
/* maximum key range = 34, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36,  0, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36,  5,  5,  5,
       5,  5,  0, 36, 36,  0, 36, 36, 36,  0,
       0,  0,  0, 36,  0, 10,  0, 36, 36, 36,
       0,  0, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
      36, 36, 36, 36, 36, 36
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[9]];
      /*FALLTHROUGH*/
      case 9:
      case 8:
      case 7:
      case 6:
      case 5:
      case 4:
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

struct stringpool_t
  {
    char stringpool_str2[sizeof("to")];
    char stringpool_str4[sizeof("from")];
    char stringpool_str7[sizeof("cc")];
    char stringpool_str8[sizeof("reply-to")];
    char stringpool_str9[sizeof("date")];
    char stringpool_str10[sizeof("x-priority")];
    char stringpool_str11[sizeof("in-reply-to")];
    char stringpool_str12[sizeof("mime-version")];
    char stringpool_str13[sizeof("received")];
    char stringpool_str15[sizeof("importance")];
    char stringpool_str17[sizeof("content-type")];
    char stringpool_str20[sizeof("content-id")];
    char stringpool_str22[sizeof("subject")];
    char stringpool_str24[sizeof("content-disposition")];
    char stringpool_str25[sizeof("message-id")];
    char stringpool_str29[sizeof("content-description")];
    char stringpool_str30[sizeof("content-transfer-encoding")];
    char stringpool_str32[sizeof("x-simplemail-pop3")];
    char stringpool_str35[sizeof("x-simplemail-partial")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "to",
    "from",
    "cc",
    "reply-to",
    "date",
    "x-priority",
    "in-reply-to",
    "mime-version",
    "received",
    "importance",
    "content-type",
    "content-id",
    "subject",
    "content-disposition",
    "message-id",
    "content-description",
    "content-transfer-encoding",
    "x-simplemail-pop3",
    "x-simplemail-partial"
  };
#define stringpool ((const char *) &stringpool_contents)
const struct header_entry *
mail_lookup_header (register const char *str, register size_t len)
{
  static const struct header_entry wordlist[] =
    {
      {-1}, {-1},
#line 36 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str2,							HEADER_TO},
      {-1},
#line 35 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str4,						HEADER_FROM},
      {-1}, {-1},
#line 37 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str7,							HEADER_CC},
#line 40 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str8,					HEADER_REPLY_TO},
#line 39 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str9,						HEADER_DATE},
#line 51 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str10,				HEADER_X_PRIORITY},
#line 47 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str11,				HEADER_IN_REPLY_TO},
#line 42 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str12,				HEADER_MIME_VERSION},
#line 41 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str13,					HEADER_RECEIVED},
      {-1},
#line 50 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str15,					HEADER_IMPORTANCE},
      {-1},
#line 44 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str17,				HEADER_CONTENT_TYPE},
      {-1}, {-1},
#line 45 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str20,					HEADER_CONTENT_ID},
      {-1},
#line 38 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str22,					HEADER_SUBJECT},
      {-1},
#line 43 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str24,		HEADER_CONTENT_DISPOSITION},
#line 46 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str25,					HEADER_MESSAGE_ID},
      {-1}, {-1}, {-1},
#line 49 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str29,		HEADER_CONTENT_DESCRIPTION},
#line 48 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str30,	HEADER_CONTENT_TRANSFER_ENCODING},
      {-1},
#line 52 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str32,			HEADER_X_SIMPLEMAIL_POP3},
      {-1}, {-1},
#line 53 "mail_headers.gperf"
      {(int)(size_t)&((struct stringpool_t *)0)->stringpool_str35,		HEADER_X_SIMPLEMAIL_PARTIAL}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register int o = wordlist[key].name;
          if (o >= 0)
            {
              register const char *s = o + stringpool;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &wordlist[key];
            }
        }
    }
  return 0;
}
#line 54 "mail_headers.gperf"

