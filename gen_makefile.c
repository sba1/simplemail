/*
** makesmakefile.c - generates a smakefile for SimpleMail
**
** Creation Date: 5.10.2000 (very bad coding style!)
*/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <dos.h>

#include <proto/dos.h>

#include "gen_makefile.h"


/* Global data */
static char *target; /* name of the target */
static char *objsdir; /* path to the root of the object directory */
static char *objs; /* name of the macro to store all object names */
static char *cflags; /* flags used for compiling c files */
static int nolink; /* should linking the target be suppressed? */

/**************************************************************************
 Display an error text
**************************************************************************/
void panic(char *text)
{
  if (text)
    puts(text);
  exit(20);
}

/**************************************************************************
 Allocate memory and copy a string to it 
**************************************************************************/
char *scopy(char *s)
{
  char *t;
  t = malloc(strlen(s) + 1);
  if (!t)
    panic("No Memory!\n");
  strcpy(t, s);
  return t;
}

/**************************************************************************
 Return the descriptor for the next file in the FileList
**************************************************************************/
struct FileDesc *NextFile(FileList * fl)
{
  if (fl->cur == NULL)
    return fl->cur = fl->head;
  else
    return fl->cur = fl->cur->next;
}

/**************************************************************************
 Find a file in a FileList
**************************************************************************/
struct FileDesc *FindFile(FileList * fl, char *file)
{
  struct FileDesc *fd;
  for (fd = fl->head; fd && stricmp(file, fd->name); fd = fd->next);
  return fd;
}

/**************************************************************************
 Add a single file (not a pattern) to a FileList
 File argument is duplicated
**************************************************************************/
struct FileDesc *AddFile(FileList * fl, char *file)
{
  struct FileDesc *fd;

  if (!(file = scopy(file)))
    panic("No memory!\n");

  /* Check to make sure it's not already on the list */
  if (fd = FindFile(fl, file))
    return fd;

  if (!(fd = malloc(sizeof(*fd))))
    panic("No memory!\n");

  memset(fd, 0, sizeof(*fd));

  /* Link the new FileDesc structure into the FileList */
  fd->prev = fl->tail;
  if (fl->tail)
    fl->tail->next = fd;
  else
    fl->head = fd;
  fl->tail = fd;

  fd->name = file;

  return fd;
}

/**************************************************************************
 Add all files matching the pattern "pattern" to the FileList "fl"
**************************************************************************/
void AddPattern(FileList * fl, char *pattern)
{
  struct FileInfoBlock __aligned fib;

  if (dfind(&fib, pattern, 0))
    return;

  do
  {
    AddFile(fl, fib.fib_FileName);
  }
  while (!dnext(&fib));
}


char buf[1024];

/**************************************************************************
 Add a dependancy to a file
**************************************************************************/
void AddDep(struct FileDesc *fd, struct FileDesc *newfd)
{
  int i;

  if (fd == newfd)
    return;			// Trivial circular dependancy

  for (i = 0; i < newfd->ndirect; i++)
  {
    if (newfd->Direct[i] == fd)
    {
      printf("Circular dependancy: \"%s\" and \"%s\"\n",
	     fd->name, newfd->name);
      return;			/* Circular dependancy */
    }
  }

  for (i = 0; i < fd->ndirect; i++)
  {
    if (fd->Direct[i] == newfd)
    {
      printf("Double dependancy: \"%s\"\n", fd->name);
      return;			/* already depends */
    }
  }

  if (fd->ndirect >= fd->adirect)
  {
    fd->adirect += 20;
    fd->Direct = realloc(fd->Direct, fd->adirect * sizeof(struct FileDesc *));
    if (!fd->Direct)
      panic("No Memory!\n");
  }
  fd->Direct[fd->ndirect++] = newfd;
}

char **include_array; /* an array of includes */

/**************************************************************************
 Get dependancies for a single file
**************************************************************************/
void DoFile(struct FileList *fl, struct FileDesc *fd)
{
  BPTR fp;
  char *s, *t;

  printf("Processing \"%s\"\n", fd->name);

  if (!(fp = Open(fd->name, MODE_OLDFILE)))
  {
    printf("Can't open file \"%s\"!\n", fd->name);
    return;
  }

  while (FGets(fp, buf, sizeof(buf)))
  {
    if (!memcmp(buf, "#include ", sizeof("#include")))
    {
      BPTR lock;
      struct FileDesc *newfd;

      for (t = buf + sizeof("#include"); *t && *t == ' '; t++);
      if (*t != '"')
	continue;
      s = ++t;
      for (; *t && *t != '"'; t++);
      *t = 0;

      /* try the name directly */
      if (lock = Lock(s,ACCESS_READ))
      {
	newfd = AddFile(fl, s);
	AddDep(fd, newfd);
	UnLock(lock);
      } else
      {
      	int rc = 0;

      	/* try the same location as the source file is in */
      	static char buf[256];
      	char *path = PathPart(fd->name);
      	strncpy(buf,fd->name,path - fd->name);
      	buf[path - fd->name] = 0;
      	AddPart(buf,s,256);

	if (lock = Lock(buf,ACCESS_READ))
	{
	  newfd = AddFile(fl, buf);
	  AddDep(fd, newfd);
	  UnLock(lock);
	  rc = 1;
        } else
        {
          /* try the directories specified in the include_array */
          if (include_array)
          {
            int i = 0;
            while (include_array[i])
            {
              strcpy(buf,include_array[i]);
              AddPart(buf,s,256);
              if (lock = Lock(buf,ACCESS_READ))
              {
              	newfd = AddFile(fl,buf);
              	AddDep(fd,newfd);
              	UnLock(lock);
              	rc = 1;
              	break;
              }
	      i++;
            }
          }
	}
	if (rc == 0) printf("Couldn't find file: %s\n",s);
      }
    }
  }

  Close(fp);
}

/**************************************************************************
 Return the suffix (portion after the '.') for a source file
 Note that no memory is allocated, the return pointer points
 =into= the argument string.
**************************************************************************/
char *suffix(char *s)
{
  char *p;
  for (p = s + strlen(s); p > s && *p != '.'; p--);
  return p;
}

#define MAXLINELEN 77		/* Maximum line size (not counting backslash) */

/* File types */
#define FT_UNK -1		/* Unknown */
#define FT_C    0		/* C source file */
#define FT_CXX  1		/* C++ source file */
#define FT_ASM  2		/* Assembler source file */
#define FT_H    3		/* Header file */


static const struct SUFFIXES
{
  char suffix[5];
  char val;
}
suffixes[] =
{
  {".a", FT_ASM},
  {".asm", FT_ASM},
  {".c", FT_C},
  {".cc", FT_CXX},
  {".cpp", FT_CXX},
  {".cxx", FT_CXX},
  {".h", FT_H},
  {".s", FT_ASM},
};

#define NUMSUFFIXES (sizeof(suffixes)/sizeof(struct SUFFIXES))

/**************************************************************************
 return the file type (as defined above) for a given source file
**************************************************************************/
int filetype(char *name)
{
  int i, j;
  name = suffix(name);

  for (i = 0; i < NUMSUFFIXES; i++)
  {
    j = stricmp(name, (char *) suffixes[i].suffix);
    if (j == 0)
      return suffixes[i].val;
  }
  return FT_UNK;
}

/**************************************************************************
 Return the prefix (portion before the '.') for a source file.
 This function DOES allocate memory to copy the prefix into.
**************************************************************************/
char *prefix(char *s)
{
  char c;
  char *p = suffix(s);
  if (p != s)
  {
    c = *p;
    *p = 0;
    s = scopy(s);
    *p = c;
    return s;
  }
  else
    return scopy("");
}

/**************************************************************************
 Take a filename as input, and return the object filename
 that would correspond to it.  I.E. take "foo.c" as input
 and put "foo.o" out.  Note that this functions DOES
 allocate a new copy of the string.
 it also adds objsdir before the string (sb)
**************************************************************************/
char *objout(char *s)
{
  char *p;
  char *t;
  long l;

  p = suffix(s);
  if (*p != '.')
    return NULL;
  if (!(t = malloc((unsigned long) p - (unsigned long) s + 3 + strlen(objsdir) + 1)))
    panic("No Memory!");

  l = (unsigned long) p - (unsigned long) s;
  strcpy(t, objsdir);
  memcpy(t + strlen(objsdir), s, l);
  strcpy(t + l + strlen(objsdir), ".o");
  return t;
}

/**************************************************************************
 Expand file dependancies
 We copy the dependancies of all our children into ourself.
 If the dependancy is already there, nothing will be added.
 Otherwise, it gets added onto the end and fd->ndirect gets
 bumped.  When we get there, that dependancy will also be
 expanded.  The result is that all dependancies, no matter
 how far removed, end up on the list for fd.
**************************************************************************/
void ExpandDeps(struct FileDesc *fd)
{
  int i, j;
  for (i = 0; i < fd->ndirect; i++)
  {
    for (j = 0; j < fd->Direct[i]->ndirect; j++)
    {
      AddDep(fd, fd->Direct[i]->Direct[j]);
    }
  }
}

/**************************************************************************
 Write out the smakefile to the given file with the given mode
**************************************************************************/
void WriteSMakefile(FileList * fl, char *destfile, int append)
{
  struct FileDesc *fd;
  char *p;
  int i, j, ft;
  int tmplen, curlen, indent;

  FILE *fh = fopen(destfile, append ? "a" : "w");
  if (!fh)
    return;

  fprintf(fh, "%s=",objs);
  curlen = indent = 5;

  /* Put out the list of object files */
  for (fd = fl->head; fd; fd = fd->next)
  {
    ft = filetype(fd->name);
    if (ft == FT_C || ft == FT_CXX || ft == FT_ASM)
    {
      if (p = objout(fd->name))
      {
	tmplen = strlen(p) + 1;
	if (curlen + tmplen > MAXLINELEN)
	{
	  /* Over MAXLINELEN bytes, continue on the next line */
	  fprintf(fh, " \\\n");
	  for (j = 0; j < indent; j++)
	    fputc(' ', fh);
	  curlen = indent + tmplen;
	}
	else
	  curlen += tmplen;
	fprintf(fh, " %s", p);

	free(p);
      }
    }
    else if (ft == FT_UNK)
    {
      /* It's not a file that produces an object file. */
      /* we don't really know what it is, but add it   */
      /* to the dependancy list anyway.  It might be   */
      /* a library or .o file, for example.            */
      fprintf(fh, " %s", fd->name);
    }
  }
  fprintf(fh, "\n\n");

  /* Put out the linker command */
  if (!nolink)
	  fprintf(fh, "%s: $(%s)\n  sc $(LFLAGS) link to $@ with <<\n$(%s)\n<\n\n", target, objs, objs);

  /* Put out the individual file dependancy lists */
  for (fd = fl->head; fd; fd = fd->next)
  {
    ft = filetype(fd->name);
    if (ft == FT_C || ft == FT_CXX || ft == FT_ASM)
    {
      if (!(p = objout(fd->name)))
	break;

      fprintf(fh, "%s: %s", p, fd->name);
      indent = strlen(p) + 1;
      curlen = indent + strlen(fd->name) + 1;
      free(p);

      /* Expand the dependancies of the current file.  This will  */
      /* copy all second, third, .... nth level dependancies into */
      /* the current file descriptor.                             */
      ExpandDeps(fd);

      for (i = 0; i < fd->ndirect; i++)
      {
	tmplen = strlen(fd->Direct[i]->name) + 1;
	if (curlen + tmplen > MAXLINELEN)
	{
	  /* Over MAXLINELEN bytes, continue on a new line */
	  fprintf(fh, " \\\n");
	  for (j = 0; j < indent; j++)
	    fputc(' ', fh);
	  curlen = indent + tmplen;
	}
	else
	  curlen += tmplen;
	fprintf(fh, " %s", fd->Direct[i]->name);
      }
      if (ft == FT_ASM)
      {
	fprintf(fh, "\n  phxass %s TO $@\n\n", fd->name);
      }
      else
      {
	fprintf(fh, "\n  sc %s %s OBJNAME=$@\n\n", fd->name,cflags);
      }
    }
  }

  printf("Done\n");

  fclose(fh);
}

int main(void)
{
  struct FileList fl;
  struct FileDesc *file;
  struct RDArgs *rdargs;
  struct
  {
    char *pattern;
    char *filelist;
    char *destfile;
    char **idir;
    ULONG append;
    char *target;
    char *objs;
    char *objsdir;
    char *cflags;
    ULONG nolink;
  } opts;

  memset(&fl, 0, sizeof(fl));
  memset(&opts, 0, sizeof(opts));

  if (!(rdargs = ReadArgs("PATTERN/K,FILELIST/K,DESTFILE/A/K,IDIR/K/M,APPEND/S,TARGET/K,OBJS/K,OBJSDIR/K,CFLAGS/K,NOLINK/S", (LONG *) & opts, NULL)))
  {
    /* rdargs failed for some reason.  Use PrintFault to print the
       reason, then quit. */
    PrintFault(IoErr(), NULL);
    panic(NULL);
  }

  if (opts.pattern)
    AddPattern(&fl, opts.pattern);

  if (opts.filelist)
  {
    FILE *fh = fopen(opts.filelist, "rb");
    if (fh)
    {
      char buf[256];
      while (fgets(buf, 256, fh))
      {
	int len = strlen(buf);
	if (len > 1 && *buf != ';')
	{
	  buf[len - 1] = 0;	/* cut the line feed */
	  AddFile(&fl, buf);
	}
      }
      fclose(fh);
    }
  }

  if (!(objsdir = opts.objsdir))
	  objsdir = "$(OBJSDIR)";
  if (!(objs = opts.objs))
	  objs = "OBJS";
  if (!(target = opts.target))
	  target = "$(PROGRAMMNAME)";
  if (!(cflags = opts.cflags))
	  cflags = "$(CFLAGS)";
  nolink = !!opts.nolink;

  /* set the include array */
  include_array = opts.idir;

  /* Walk the file list, processing each file.  Note that during file processing, it is
     possible that other files will be added to the end of the list.  (For example, if a
     .c file on the list includes a .h file, we add the .h file to the end of the FileList).
     Since they are added to the end, we stay in this loop until ALL files have been
     processed, even the ones that weren't on the list when the loop started. */
  for (file = NextFile(&fl); file; file = NextFile(&fl))
    DoFile(&fl, file);

  WriteSMakefile(&fl, opts.destfile, opts.append);

  FreeArgs(rdargs);

  return 0;
}
