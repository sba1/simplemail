/*  $VER: maketls.rexx 1.5 (27.8.09)
 *
 *  Helps to add localisation support for ARexx programs easier
 *  by building a translation file to be used with them.
 *
 *  If a translation file already exists, it will be used and
 *  updated.
 *
 *  Usage: rx maketls.rexx <source script> <language> [<destination dir>]
 *     OR: rx maketls.rexx CHECK <source script> [<destination dir>]
 *
 *  Where:
 *
 *  - <source script> is the absolute path and name of the
 *    script to be scanned for translations.
 *    This argument (SRCFILE) is required.
 *
 *  - <language> is the name of the language for which
 *    you want to translate the source script into.
 *    This argument (LANGUAGE) is required in the normal
 *    usage, but forbidden in CHECK mode (see below).
 *    NOTE: Since OS4.0, the language names to use are
 *    in English.  That is "french" for French instead
 *    of "français".
 *
 *  - <destination dir> (DSTDIR) is optional (default is
 *    the directory of the source script).  It is the
 *    absolute path to where you want the output results to be
 *    written:
 *    - in normal usage, the translation file created or
 *      updated will be:
 *      "Locale/Catalogs/<language>/<source script>.catalog"
 *    - in CHECK mode, the check file created will be:
 *      "<source script>.check"
 *
 *  The CHECK mode (second usage; the keyword is required and
 *  MUST be the first given argument) is intended to help the
 *  authors of ARexx scripts to review the output strings
 *  available for translation (eg. to avoid creating many
 *  translations for very similar strings).  The strings found
 *  are sorted alphabetically (reversed order, case insensistive)
 *  and prepended by the line number they were found in the
 *  <source script>.
 *
 *  Script by: AmigaPhil  <AmigaPhil scarlet be>
 *  Distribution: Freeware (See: maketls.readme)
 *
 */

template = 'SRCFILE/A,LANGUAGE/A,DSTDIR'
template2 = 'SRCFILE/A,DSTDIR'

/* Localise this script ? */
scriptLang = ''
IF EXISTS('ENV:Language') THEN DO
   IF OPEN('el', 'ENV:Language', 'R') THEN DO
      scriptLang = READLN('el')
      CALL CLOSE('el')
   END
END
IF (scriptLang ~= '') & (scriptLang ~= 'english') THEN CALL gettlstext(scriptLang)

/* Set default destination directory to the current directory */
dstDir = PRAGMA('D')

PARSE ARG argstring
IF (argstring = '?') | (argstring = '') | (UPPER(argstring) = 'HELP') THEN DO
   SAY 'Usage1:     rx maketls.rexx <arguments>'
   SAY 'Template1: ' || template
   SAY 'Usage2:     rx maketls.rexx CHECK <arguments>'
   SAY 'Template2: ' || template2
   SAY tls('(See script or doc for more.)')
   EXIT
END

/* CHECK mode ? */
check = 0
IF UPPER(LEFT(argstring, 5)) = 'CHECK' THEN DO
   check = 1
   argstring = STRIP(SUBSTR(argstring, 6))
END

/* Get source file from arguments */
srcfile = ''
IF UPPER(LEFT(argstring, 7)) = 'SRCFILE' THEN DO
   argstring = STRIP(SUBSTR(argstring, 8))
   IF LEFT(argstring, 1) = '=' THEN argstring = STRIP(SUBSTR(argstring, 2))
END
IF (LEFT(argstring, 1) = '"') | (LEFT(argstring, 1) = "'") THEN DO
   endchar = LEFT(argstring, 1)
   argpos  = 2
   endpos  = POS(endchar, argstring, argpos)
   IF endpos > argpos THEN srcfile = SUBSTR(argstring, argpos, endpos - argpos)
END
ELSE DO
   srcfile = WORD(argstring, 1)
   endpos  = LENGTH(srcfile)
END
IF srcfile = '' THEN DO
   SAY tls("ERROR: Missing SRCFILE argument.")
   SAY tls("Type 'rx maketls.rexx HELP' for usage.)"
   EXIT 10
END
IF POS(':', srcfile) = 0 THEN DO
   SAY tls("ERROR: Absolute path required for SRCFILE.")
   SAY tls("Type 'rx maketls.rexx HELP' for usage.")
   EXIT 10
END
IF ~EXISTS(srcfile) THEN DO
   SAY tls("ERROR: Can't find source file: '") || srcfile || "'"
   SAY tls("Type 'rx maketls.rexx HELP' for usage.")
   EXIT 10
END
argstring = STRIP(DELSTR(argstring, 1, endpos))

/* Extract source file name */
posprt = LASTPOS('/',srcfile)
IF posprt = 0 THEN posprt = LASTPOS(':',srcfile)
IF posprt > 0 THEN srcfilename = SUBSTR(srcfile,posprt + 1)
ELSE srcfilename = srcfile
/* Set new default destination directory */
parpos = LASTPOS('/', srcfile)
IF parpos = 0 THEN parpos = POS(':', srcfile)
IF parpos > 0 THEN dstDir = LEFT(srcfile, parpos)

/* Get language from arguments */
language = ''
IF ~check THEN DO
   IF UPPER(LEFT(argstring, 8)) = 'LANGUAGE' THEN DO
      argstring = STRIP(SUBSTR(argstring, 9))
      IF LEFT(argstring, 1) = '=' THEN argstring = STRIP(SUBSTR(argstring, 2))
   END
   IF (LEFT(argstring, 1) = '"') | (LEFT(argstring, 1) = "'") THEN DO
      endchar = LEFT(argstring, 1)
      argpos  = 2
      endpos  = POS(endchar, argstring, argpos)
      IF endpos > argpos THEN language = SUBSTR(argstring, argpos, endpos - argpos)
   END
   ELSE DO
      language = WORD(argstring, 1)
      endpos   = LENGTH(language)
   END
   IF language = '' THEN DO
      SAY tls("ERROR: Missing LANGUAGE argument.")
      SAY tls("Type 'rx maketls.rexx HELP' for usage.")
      EXIT 10
   END
   argstring = STRIP(DELSTR(argstring, 1, endpos))
END

/* Check if destination dir is given as argument */
IF UPPER(LEFT(argstring, 6)) = 'DSTDIR' THEN DO
   argstring = STRIP(SUBSTR(argstring, 7))
   IF LEFT(argstring, 1) = '=' THEN argstring = STRIP(SUBSTR(argstring, 2))
END
IF (LEFT(argstring, 1) = '"') | (LEFT(argstring, 1) = "'") THEN DO
   endchar = LEFT(argstring, 1)
   argpos  = 2
   endpos  = POS(endchar, argstring, argpos)
   IF endpos > argpos THEN DO
      dstDir = SUBSTR(argstring, argpos, endpos - argpos)
      argstring = STRIP(DELSTR(argstring, 1, endpos))
   END
END
ELSE DO
   IF LENGTH(argstring) > 0 THEN DO
      dstDir    = WORD(argstring, 1)
      argstring = DELWORD(argstring, 1)
   END
END
IF (LENGTH(dstDir) > 0) & (POS(':', dstDir) = 0) THEN DO
   SAY tls("ERROR: Absolute path required for DSTDIR.")
   SAY tls("Type 'rx maketls.rexx HELP' for usage.")
   EXIT 10
END

/* Is there anything unrecognised in arguments ? */
IF argstring ~= '' THEN DO
   SAY tls("ERROR: Unrecognised argument: '") || argstring || "'"
   SAY tls("Type 'rx maketls.rexx HELP' for usage.")
   EXIT 10
END


ntlstext. = ''
writemode = tls('Creating ')
oldcount  = 0

/* Build .check or .catalog file name */
IF check THEN dstfilename = srcfilename || '.check'
ELSE dstfilename = srcfilename || '.catalog'

/* Build destination path */
IF (dstDir ~= '') & (RIGHT(dstDir, 1) ~= ":") & (RIGHT(dstDir, 1) ~= "/") THEN dstDir = dstDir || '/'
IF ~check THEN DO
   dstDir = dstDir || 'Locale'
   IF ~EXISTS(dstDir) THEN ADDRESS COMMAND 'MakeDir >NIL: "'dstDir'"'
   dstDir = dstDir || '/Catalogs'
   IF ~EXISTS(dstDir) THEN ADDRESS COMMAND 'MakeDir >NIL: "'dstDir'"'
   dstDir = dstDir || '/' || language
   IF ~EXISTS(dstDir) THEN ADDRESS COMMAND 'MakeDir >NIL: "'dstDir'"'
   dstDir = dstDir || '/'
END

/* Construct new destination file (path and name) */
dstfile = dstDir || dstfilename

IF EXISTS(dstfile) THEN writemode = tls('Updating ')

/* Get the string to translate from the source file */
IF ~OPEN('sr',srcfile,'R') THEN DO
   SAY tls("Can't open the source file ") || srcfilename
   EXIT 10
END
IF check THEN SAY writemode || dstfilename || " ..."
ELSE SAY writemode || 'Locale/catalogs/' || language || '/' || dstfilename || " ..."
newcount = 0
slinecnt = 0
DO WHILE ~EOF('sr')
  instring = READLN('sr')
  slinecnt = slinecnt + 1
  DO WHILE POS('tls(',instring) > 0

     instring = STRIP(SUBSTR(instring,POS('tls(',instring) + 4))
     quot = LEFT(instring,1)
     IF (quot ~= "'") & (quot ~= '"') THEN ITERATE
     strend = POS(quot || ')',instring,2)
     IF strend < 3 THEN ITERATE

     newstr = SUBSTR(instring,2,strend - 2)
     IF (quot = "'") & (VERIFY(newstr,quot,'MATCH') > 0) THEN quot = '"'
     IF (quot = '"') & (VERIFY(newstr,quot,'MATCH') > 0) THEN newstr = COMPRESS(newstr,'"')

     isnewer = 1
     IF newcount > 0 THEN DO ck=1 TO newcount
        IF ntlstext.ck.SRC == newstr THEN DO
           isnewer = 0
           LEAVE ck
        END
     END
     IF isnewer THEN DO
        newcount = newcount + 1
        ntlstext.newcount.SRC      = newstr
        ntlstext.newcount.SRCQUOTE = quot
        ntlstext.newcount.DSTQUOTE = quot
        ntlstext.newcount.ISNEW    = 1
        ntlstext.newcount.SRCLINE  = slinecnt
     END

  END
END
CALL CLOSE('sr')

/* Get any already translated strings from the language file */
IF ~check & EXISTS(dstfile) THEN DO
   IF ~OPEN('lg',dstfile,'R') THEN DO
      SAY tls("Can't open the translation file ") || dstfile
      EXIT 10
   END
   tlssrc   = ''
   tlsdst   = ''
   oldcount = 0
   oldidx   = 0
   DO WHILE ~EOF('lg')
     instring = STRIP(READLN('lg'))
     IF (LENGTH(instring) > 0) & (LEFT(instring,2) ~= '/*') & (LEFT(instring,2) ~= '//') THEN INTERPRET instring
     IF tlssrc ~= '' THEN DO
        oldcount = oldcount + 1
        oldidx   = 0
        IF newcount > 1 THEN DO ck=1 TO newcount
           IF ntlstext.ck.SRC == tlssrc THEN DO
              ntlstext.ck.ISNEW = 0
              oldidx = ck
              LEAVE ck
           END
        END
        tlssrc = ''
        tlsdst = ''
     END
     IF (tlsdst ~= '') & (oldidx ~= 0) THEN DO
        quot = ntlstext.oldidx.SRCQUOTE
        squotpos = POS("'",instring)
        dquotpos = POS('"',instring)
        IF squotpos > 0 THEN quot = "'"
        IF (dquotpos > 0) & (dquotpos < squotpos) THEN quot = '"'
        IF (quot = "'") & (VERIFY(tlsdst,quot,'MATCH') > 0) THEN quot = '"'
        IF (quot = '"') & (VERIFY(tlsdst,quot,'MATCH') > 0) THEN tlsdst = COMPRESS(tlsdst,'"')
        ntlstext.oldidx.DSTQUOTE = quot
        ntlstext.oldidx.DST = tlsdst
        tlssrc = ''
        tlsdst = ''
        oldidx = 0
     END
   END
   CALL CLOSE('lg')
END

/* Create/update the language file (or the check file) */
IF ~OPEN('lg',dstfile,'W') THEN DO
   SAY tls("Can't create the file ") || dstfilename
   EXIT 10
END

newstrcount = 0
IF check THEN DO
   CALL WRITELN('lg','maketls.rexx check on ' || srcfilename || ' (' || DATE() || ' ' || TIME() || ')')
   CALL WRITELN('lg','')
   IF newcount = 0 THEN DO
      CALL WRITELN('lg',tls('No string available for translation !'))
   END
   ELSE DO
      IF newcount = 1 THEN DO
         CALL WRITELN('lg',tls('One string available for translation.'))
         CALL WRITELN('lg',tls('Line (first column) is the line number from the source code'))
         CALL WRITELN('lg',tls('where the string was found.'))
         CALL WRITELN('lg','')
         CALL WRITELN('lg',tls('L: String'))
         CALL WRITELN('lg','')
      END
      ELSE DO
         CALL WRITELN('lg',newcount || tls(' strings available for translation.'))
         CALL WRITELN('lg',tls('(Sorted in reverted alphanumeric order, case insensitive)'))
         CALL WRITELN('lg',tls('Line (first column) is the line number from the source code'))
         CALL WRITELN('lg',tls('where the string was found.'))
         CALL WRITELN('lg','')
         CALL WRITELN('lg',LEFT(tls('Line'),LENGTH(slinecnt)) || tls(': String'))
         CALL WRITELN('lg','')
         notempty = 1
         lowlim   = 1
         higlim   = newcount
         DO WHILE notempty
            updidx   = 0
            firstone = higlim
            lastone  = lowlim
            strcntrl = ''
            DO i=lowlim TO higlim
               IF ntlstext.i.SRC ~= '' THEN DO
                  IF i < firstone THEN firstone = i
                  IF i > lastone THEN lastone = i
                  IF UPPER('1' || ntlstext.i.SRC) >= UPPER('1' || strcntrl) THEN DO
                     updidx = i
                     strcntrl = ntlstext.i.SRC
                  END
               END
            END
            IF updidx = 0 THEN notempty = 0
            ELSE DO
              quot = '"'
              IF ntlstext.updidx.SRCQUOTE ~= '' THEN quot = ntlstext.updidx.SRCQUOTE
              CALL WRITELN('lg',RIGHT(ntlstext.updidx.SRCLINE,LENGTH(slinecnt)) || ': ' || quot || ntlstext.updidx.SRC || quot)
              ntlstext.updidx.SRC = ''
              IF firstone > lowlim THEN lowlim = firstone
              IF lastone > higlim THEN higlim = lastone
            END
         END
      END
   END
END
ELSE DO
   CALL WRITELN('lg','/* Translation file for ' || srcfilename || ' */')
   CALL WRITELN('lg','/* Language: ' || language || '  (Last updated on ' || DATE() || ' ' || TIME() || ') */')
   CALL WRITELN('lg','')
   CALL WRITELN('lg','/* Do not touch the tlssrc lines.           */')
   CALL WRITELN('lg','/* Add your translation string in tlsdst.   */')
   CALL WRITELN('lg','/* If a string does not need a translation, */')
   CALL WRITELN('lg','/* leave the translated string EMPTY.       */')
   CALL WRITELN('lg','')
   CALL WRITELN('lg','/* TAKE CARE OF ' || "'" || ' AND " WHEN ADDING NEW STRINGS ! */')
   DO updidx = 1 TO newcount
      CALL WRITELN('lg','')
      quot = '"'
      IF ntlstext.updidx.SRCQUOTE ~= '' THEN quot = ntlstext.updidx.SRCQUOTE
      addNEW = ''
      IF ntlstext.updidx.ISNEW THEN DO
         addNEW = '  /*** NEW ***/'
         newstrcount = newstrcount + 1
      END
      CALL WRITELN('lg','tlssrc = ' || quot || ntlstext.updidx.SRC || quot || addNEW)
      IF ntlstext.updidx.DSTQUOTE ~= '' THEN quot = ntlstext.updidx.DSTQUOTE
      CALL WRITELN('lg','tlsdst = ' || quot || ntlstext.updidx.DST || quot)
   END
END

CALL CLOSE('lg')

IF check THEN SAY tls('Number of strings found: ') || newcount
ELSE DO
   IF newstrcount > 0 THEN DO
      SAY dstfilename || tls(' is ready for ') || newstrcount || tls(' new translation(s) !')
   END
   ELSE DO
      SAY tls('No new string to translate.')
   END
   remcount = (oldcount + newstrcount) - newcount
   IF remcount > 0 THEN DO
      SAY remcount || tls(' unused string(s) removed.')
   END
END

EXIT



/* This is the function which translates the output strings */
/* to the user's prefered language                          */

tls: PROCEDURE EXPOSE tlstext.

  PARSE ARG instring

  IF instring = '' THEN RETURN instring
  IF tlstext.instring == 'TLSTEXT.' || instring THEN RETURN instring
  IF tlstext.instring = '' THEN RETURN instring

  RETURN tlstext.instring


/* This function loads the translation file (.catalog) */

gettlstext: PROCEDURE EXPOSE tlstext.

  PARSE ARG language
  IF language = '' THEN RETURN

  currentDir = PRAGMA('D')
  scriptDir  = ''
  PARSE SOURCE scriptPath
  scriptPath = SUBWORD(scriptPath,4,WORDS(scriptPath)-5)
  parpos = LASTPOS('/', scriptPath)
  IF parpos = 0 THEN parpos = POS(':', scriptPath)
  IF parpos > 0 THEN scriptDir = LEFT(scriptPath, parpos)
  scriptName = SUBSTR(scriptPath, parpos + 1)
  langFile = "Locale/Catalogs/" || language || "/" || scriptName || ".catalog"
  IF ~EXISTS(langFile) THEN CALL PRAGMA('D',scriptDir)
  IF ~EXISTS(langFile) THEN CALL PRAGMA('D',"SYS:")

  IF OPEN('lf',langFile, 'R') THEN DO
    tlssrc = ''
    tlsdst = ''
    DO WHILE ~EOF('lf')
      instring = STRIP(READLN('lf'))
      IF (LENGTH(instring) > 0) & (LEFT(instring,2) ~= '/*') & (LEFT(instring,2) ~= '//') THEN INTERPRET instring
      IF tlssrc ~= '' THEN DO
        IF tlsdst ~= '' THEN DO
          tlstext.tlssrc = tlsdst
          tlssrc = ''
          tlsdst = ''
        END
      END
    END
    CALL CLOSE('lf')
  END

  CALL PRAGMA('D',currentDir)

  RETURN

