#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <sys/dir.h>
#include <sys/stat.h>

#include <proto/exec.h>
#include <proto/dos.h>

/*#define MYDEBUG*/
#include "amigadebug.h"

#define FAST_SEEK

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *UtilityBase;
struct Library *IntuitionBase;
struct Library *LocaleBase;
struct Library *DataTypesBase;
struct Library *KeymapBase;
struct Library *IFFParseBase;
struct Library *IconBase;
struct Library *DiskfontBase;
struct Library *WorkbenchBase;
struct Library *AslBase;
struct Library *GfxBase;
struct Library *LayersBase;
struct Library *ExpatBase;

int main(int argc, char *argv[]);

static int start(struct WBStartup *wbs);

static int open_libs(void);
static void close_libs(void);
static int init_mem(void);
static void deinit_mem(void);
static int init_io(void);
static void deinit_io(void);

__saveds static int __startup(void)
{
	struct Process *pr;
	int rc;

	SysBase = *((struct ExecBase**)4);
	pr = (struct Process*)FindTask(NULL);

	if (!pr->pr_CLI)
	{
		struct WBStartup *wbs;

		WaitPort(&pr->pr_MsgPort);
		wbs = (struct WBStartup*)GetMsg(&pr->pr_MsgPort);

		rc = start(wbs);

		Forbid();
		ReplyMsg((struct Message *)wbs);
	}	else rc = start(NULL);
	return rc;
}

static int rc;

/* not static so the optimized doesn't inline it */
int __swap_and_start(struct StackSwapStruct *stk)
{
	StackSwap(stk);
	rc = main(0,NULL);
	StackSwap(stk);
	return rc;
}

static int start(struct WBStartup *wbs)
{
	int rc = 20;
	struct Process *pr = (struct Process*)FindTask(NULL);

	if ((DOSBase = (struct DosLibrary*)OpenLibrary("dos.library",37)))
	{
		BPTR out;
		BPTR oldout;

		if (wbs)
		{
			if ((out = Open("CON:10/10/320/80/SimpleMail/AUTO/CLOSE/WAIT",MODE_OLDFILE)))
				oldout = SelectOutput(out);
		}

		if (open_libs())
		{
			if (init_mem())
			{
				BPTR dirlock = DupLock(pr->pr_CurrentDir);
				if (!dirlock) dirlock = Lock("PROGDIR:",ACCESS_READ);
				if (dirlock)
				{
					BPTR odir = CurrentDir(dirlock);
					if (init_io())
					{
						struct StackSwapStruct stk;

						if ((stk.stk_Lower = (APTR)AllocVec(40000,MEMF_PUBLIC)))
						{
					    stk.stk_Upper = (ULONG)stk.stk_Lower + 40000;
					    stk.stk_Pointer = (APTR)stk.stk_Upper;
					    rc = __swap_and_start(&stk);
					    FreeVec(stk.stk_Lower);
					   }
						deinit_io();
					}
					deinit_mem();
					UnLock(CurrentDir(odir));
				}
			}
			close_libs();
		}

		if (wbs && out)
		{
			SelectOutput(oldout);
			Close(out);
		}

		CloseLibrary((struct Library*)DOSBase);
	}

	return rc;
}

static int open_libs(void)
{
	if ((IntuitionBase = OpenLibrary("intuition.library",37)))
	{
		if ((UtilityBase = OpenLibrary("utility.library",37)))
		{
			if ((LocaleBase = OpenLibrary("locale.library",37)))
			{
				if ((DataTypesBase = OpenLibrary("datatypes.library",39)))
				{
					if ((KeymapBase = OpenLibrary("keymap.library",36)))
					{
						if ((IFFParseBase = OpenLibrary("iffparse.library",37)))
						{
							if ((IconBase = OpenLibrary("icon.library",37)))
							{
								if ((DiskfontBase = OpenLibrary("diskfont.library",37)))
								{
									if ((WorkbenchBase = OpenLibrary("workbench.library",37)))
									{
										if ((AslBase = OpenLibrary("asl.library",38)))
										{
											if ((GfxBase = OpenLibrary("graphics.library",37)))
											{
												if ((LayersBase = OpenLibrary("layers.library",37)))
												{
													if (!(ExpatBase = OpenLibrary("expat.library",0)))
														ExpatBase = OpenLibrary("PROGDIR:libs/expat.library",0);

													if (ExpatBase)
													{
														return 1;
													} else PutStr("Couldn't open expat.library. Please download it from aminet or somewhere else.\n");
												}
											}
										} else PutStr("Couldn't open asl.library\n");
									} else PutStr("Couldn't open workbench.library\n");
								} else PutStr("Couldn't open diskfont.library\n");
							} else PutStr("Couldn't open icon.library\n");
						} else PutStr("Couldn't open iffparse.library\n");
					}
				} else PutStr("Couldn't open datatypes.library\n");
			} else PutStr("Couldn't open locale.library\n");
		}
	}
	close_libs();
	return 0;
}

static void close_libs(void)
{
	if (ExpatBase) CloseLibrary(ExpatBase);
	if (LayersBase) CloseLibrary(LayersBase);
	if (GfxBase) CloseLibrary(GfxBase);
	if (AslBase) CloseLibrary(AslBase);
	if (WorkbenchBase) CloseLibrary(WorkbenchBase);
	if (DiskfontBase) CloseLibrary(DiskfontBase);
	if (IconBase) CloseLibrary(IconBase);
	if (IFFParseBase) CloseLibrary(IFFParseBase);
	if (KeymapBase) CloseLibrary(KeymapBase);
	if (DataTypesBase) CloseLibrary(DataTypesBase);
	if (LocaleBase) CloseLibrary(LocaleBase);
	if (UtilityBase) CloseLibrary(UtilityBase);
	if (IntuitionBase) CloseLibrary(IntuitionBase);
}

/*****************************************
 Memory stuff (thread safe)
******************************************/

APTR pool;
struct SignalSemaphore pool_sem;

static int init_mem(void)
{
	/* Stuff can be used by multiple tasks */
	if ((pool = CreatePool(MEMF_PUBLIC,16384,8192)))
	{
		InitSemaphore(&pool_sem);
		return 1;
	}
	return 0;
}

static void deinit_mem(void)
{
	if (pool) DeletePool(pool);
}

void *malloc(size_t size)
{
	ULONG *mem;
	ObtainSemaphore(&pool_sem);
	mem = AllocPooled(pool,4+size);
	ReleaseSemaphore(&pool_sem);
	if (!mem) return NULL;
	mem[0] = size;
	return mem+1;
}

void free(void *m)
{
	ULONG *mem = (ULONG*)m;
	if (!m) return;
	mem--;
	ObtainSemaphore(&pool_sem);
	FreePooled(pool,mem,mem[0]+4);
	ReleaseSemaphore(&pool_sem);
}

void *realloc(void *om, size_t size)
{
	ULONG *oldmem;

	if (!om) return malloc(size);

	oldmem = (ULONG*)om;
	oldmem--;
	if (size > oldmem[0])
	{
		void *mem = malloc(size);
		if (!mem) return NULL;
		memcpy(mem,om,oldmem[0]);
		free(om);
		return mem;
	}

	if (size < oldmem[0])
	{
		void *mem = malloc(size);
		if (!mem) return NULL;
		memcpy(mem,om,size);
		free(om);
		return mem;
	}
	return om;
}


/***************************************************
 IO Stuff.
 This is a very simple and primitive functions of
 the ANSI C File IOs. Only the SimpleMail neeeded
 functionality is implemented. The functions are
 thread safe (but the file intstances mustn't be
 shared between the threads)
****************************************************/

__stdargs int vsnprintf(char *buffer, int buffersize, const char *fmt0, va_list ap);

#define MAX_FILES 100

static struct SignalSemaphore files_sem;
static BPTR files[MAX_FILES];
static char filesbuf[4096];
static int tmpno;

static int init_io(void)
{
	InitSemaphore(&files_sem);
	return 1;
}

static void deinit_io(void)
{
	int i;
	for (i=0;i<MAX_FILES;i++)
		if (files[i]) Close(files[i]);
}

int errno;

FILE *fopen(const char *filename, const char *mode)
{
	struct __iobuf *file = NULL;
	int _file;

	LONG amiga_mode;
	if (*mode == 'w') amiga_mode = MODE_NEWFILE;
	else if (*mode == 'r') amiga_mode = MODE_OLDFILE;
	else if (*mode == 'a') amiga_mode = MODE_READWRITE;
	else return NULL;

	ObtainSemaphore(&files_sem);
	/* Look if we can still open file left */
	for (_file=0;_file < MAX_FILES && files[_file];_file++);
	if (_file == MAX_FILES) goto fail;
	
	file = malloc(sizeof(*file));
	if (!file) goto fail;
	memset(file,0,sizeof(*file));

	if (!(files[_file] = Open((STRPTR)filename,amiga_mode))) goto fail;
	file->_file = _file;

	if (amiga_mode == MODE_READWRITE)
	{
		Seek(files[_file],0,OFFSET_END);
		file->_rcnt = Seek(files[_file],0,OFFSET_CURRENT);
	}

	ReleaseSemaphore(&files_sem);
	D(bug("0x%lx opened %s\n",file,filename));
	return file;

fail:
	if (_file < MAX_FILES) files[_file] = NULL;
	free(file);
	ReleaseSemaphore(&files_sem);
	return NULL;
}

int fclose(FILE *file)
{
	int error;
	char tempname[200];

	if (!file) return NULL;
	ObtainSemaphore(&files_sem);

	if (file->_flag & 0x10000)
	{
		if (!NameFromFH(files[file->_file],tempname,sizeof(tempname)))
			tempname[0] = 0;
	} else tempname[0] = 0;

	error = !Close(files[file->_file]);
	if (!error)
	{
		files[file->_file] = NULL;
		free(file);
		if (tempname[0]) DeleteFile(tempname);
	}
	ReleaseSemaphore(&files_sem);
	return error;
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE *file)
{
	BPTR fh = files[file->_file];
	size_t rc = (size_t)FWrite(fh,(void*)buffer,size,count);
	file->_rcnt += rc * size;
	return rc;
}

size_t fread(void *buffer, size_t size, size_t count, FILE *file)
{
	size_t len;
	BPTR fh = files[file->_file];
	len = (size_t)FRead(fh,buffer,size,count);
	if (!len && size && count) file->_flag |= _IOEOF;
	D(bug("0x%lx reading %ld bytes\n",file,len * size));
	file->_rcnt += len * size;
	return len;
}

int fputs(const char *string, FILE *file)
{
	BPTR fh = files[file->_file];
	int rc = FPuts(fh,(char*)string);
	if (!rc) /* DOSFALSE is true here */
		file->_rcnt += strlen(string);
	return rc;
}

int fputc(int character, FILE *file)
{
	BPTR fh = files[file->_file];
	int rc = FPutC(fh,character);
	if (rc != -1) file->_rcnt++;
	return rc;
}

int fseek(FILE *file, long offset, int origin)
{
	BPTR fh = files[file->_file];
	ULONG amiga_seek;

	if (origin == SEEK_SET) amiga_seek = OFFSET_BEGINNING;
	else if (origin == SEEK_CUR)
	{
		/* Optimize trival cases (used heavily when loading indexfiles) */
		amiga_seek = OFFSET_CURRENT;
#ifdef FAST_SEEK
		if (!offset) return 0;
		if (offset == 1)
		{
			fgetc(file);
			return 0;
		}
#endif
	}
	else amiga_seek = OFFSET_END;

	if (!Flush(fh)) return -1;
	if (Seek(fh,offset,amiga_seek)==-1) return -1;
	file->_rcnt = Seek(fh,0,OFFSET_CURRENT);
	return 0;
}

long ftell(FILE *file)
{
	BPTR fh = files[file->_file];
	D(bug("0x%lx ftell() = %ld\n",file,Seek(fh,0,OFFSET_CURRENT)));
#ifdef FAST_SEEK
	return file->_rcnt;
#else
	return Seek(fh,0,OFFSET_CURRENT);
#endif
}

int fflush(FILE *file)
{
	BPTR fh = files[file->_file];
	if (Flush(fh)) return 0;
	return -1;

}

char *fgets(char *string, int n, FILE *file)
{
	BPTR fh = files[file->_file];
	char *rc =  (char*)FGets(fh,string,n);
	if (!rc && !IoErr()) file->_flag |= _IOEOF;
	else if (rc) file->_rcnt += strlen(rc);
	return rc;
}

int fgetc(FILE *file)
{
	BPTR fh = files[file->_file];
	int rc = FGetC(fh);
	if (rc != -1) file->_rcnt++;
	return rc;
}

FILE *tmpfile(void)
{
	char buf[40];
	FILE *file;
	ObtainSemaphore(&files_sem);
	sprintf(buf,"T:%lx%lx.tmp",FindTask(NULL),tmpno++);
	file = fopen(buf,"w");
	if (file) file->_flag |= 0x10000;
	ReleaseSemaphore(&files_sem);
	return file;
}

char *tmpnam(char *name)
{
	static char default_buf[L_tmpnam];
	ObtainSemaphore(&files_sem);
	if (!name) name = default_buf;
	vsnprintf(name,sizeof(default_buf),"T:sm%05lx",&tmpno);
	tmpno++;
	ReleaseSemaphore(&files_sem);
	return name;
}

int remove(const char *filename)
{
	if (DeleteFile((char*)filename)) return 0;
	return -1;
}

int stat(const char *filename, struct stat *stat)
{
	BPTR lock;
	int rc = -1;

	if ((lock = Lock((STRPTR)filename,ACCESS_READ)))
	{
		struct FileInfoBlock *fib = (struct FileInfoBlock*)AllocDosObject(DOS_FIB,NULL);
		if (fib)
		{
			if (Examine(lock,fib))
			{
				memset(stat,0,sizeof(*stat));
				if (fib->fib_DirEntryType > 0) stat->st_mode |= S_IFDIR;
				rc = 0;
			}
			FreeDosObject(DOS_FIB,fib);
		}
		UnLock(lock);
	}
	return rc;
}

int fprintf(FILE *file, const char *fmt,...)
{
	int rc;
	int size;

  va_list ap;

	ObtainSemaphore(&files_sem);
  va_start(ap, fmt);
  size = vsnprintf(filesbuf, sizeof(filesbuf), fmt, ap);
  va_end(ap);

	if (size >= 0)
	{
		rc = fwrite(filesbuf,1,size,file);
	} else rc = -1;
	
	ReleaseSemaphore(&files_sem);

	return rc;
}


#undef printf

int printf(const char *fmt,...)
{
	int rc;
	int size;

  va_list ap;

	ObtainSemaphore(&files_sem);
  va_start(ap, fmt);
  size = vsnprintf(filesbuf, sizeof(filesbuf), fmt, ap);
  va_end(ap);

	if (size >= 0)
	{
		PutStr(filesbuf);
	} else rc = -1;
	
	ReleaseSemaphore(&files_sem);

	return size;
}

int puts(const char *string)
{
	return PutStr((STRPTR)string);
}

int sprintf(char *buf, const char *fmt, ...)
{
  int r;
  va_list ap;
  
  va_start(ap, fmt);
  r = vsnprintf(buf, 0x7fff, fmt, ap);
  va_end(ap);
  return r;
}

DIR *opendir(const char *name)
{
	BPTR dh;
	struct FileInfoBlock *fib;
	DIR *dir = malloc(sizeof(*dir) + sizeof(struct dirent));
	if (!dir) return NULL;

	if (!(dh = Lock((STRPTR)name,ACCESS_READ)))
	{
		free(dir);
		return NULL;
	}

	if (!(fib = AllocDosObject(DOS_FIB,NULL)))
	{
		UnLock(dh);
		free(dir);
		return NULL;
	}

	if (!(Examine(dh,fib)))
	{
		UnLock(dh);
		free(dir);
		return NULL;
	}

	dir->dd_fd = ((unsigned long)fib);
	dir->dd_loc = ((unsigned long)dh);
	dir->dd_buf = (char*)(dir+1);

	D(bug("Opendir %s\n",name));

	return dir;
}

void closedir(DIR *dir)
{
	FreeDosObject(DOS_FIB,(APTR)dir->dd_fd);
	UnLock((BPTR)dir->dd_loc);
	free(dir);
}

struct dirent *readdir(DIR *dir)
{
	struct dirent *dirent = (struct dirent*)dir->dd_buf;
	struct FileInfoBlock *fib = (struct FileInfoBlock*)dir->dd_fd;
	if (ExNext((BPTR)dir->dd_loc,fib))
	{
		strncpy(dirent->d_name,fib->fib_FileName,107);
		dirent->d_name[107] = 0;

		D(bug("Readdir %s\n",dirent->d_name));

		return dirent;
	}
	return NULL;
}

int chdir(const char *dir)
{
	BPTR lock = Lock((STRPTR)dir,ACCESS_READ);
	BPTR odir;
	if (!lock) return -1;
	odir = CurrentDir(lock);
	UnLock(odir);
	return 0;
}

char *getcwd(char *buf, int size)
{
	struct Process *pr = (struct Process*)FindTask(NULL);
	if (!(NameFromLock(pr->pr_CurrentDir, buf, size))) return NULL;
	return buf;
}

char *getenv(const char *name)
{
	return NULL;
}
/*
struct tm *localtime(const time_t *timeptr)
{
}
*/
int rename(const char *oldname, const char *newname)
{
	if (!(Rename((STRPTR)oldname,(STRPTR)newname)))
		return -1;
	return 0;
}

void _CXFERR(void)
{
    D(bug("CFXERR\n"));
}
