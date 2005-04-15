#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* what an ugly hack
   needed for the stdio clone */
#undef __amigaos__
#include <dirent.h>
#define __amigaos__

#include <sys/dir.h>
#include <sys/stat.h>

#include <dos/stdio.h>
#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>

//#define MYDEBUG
#include "amigadebug.h"

#define FAST_SEEK
#define BIG_BUFFER

#include "simplemail.h"

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

#define MIN68KSTACK 8192 /* MUI requirement legacy */
#define MINSTACK 60000

ULONG __abox__ = 1;

int __startup(void)
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

static int __swap_and_start(void)
{
	ULONG MySize;
	struct Task *MyTask = FindTask(NULL);

	if (!NewGetTaskAttrsA(MyTask, &MySize, sizeof(MySize), TASKINFOTYPE_STACKSIZE, NULL))
	{
		MySize = (ULONG)MyTask->tc_ETask->PPCSPUpper - (ULONG)MyTask->tc_ETask->PPCSPLower;
	}

	if (MySize < MINSTACK)
	{
		struct StackSwapStruct MySSS;
		struct PPCStackSwapArgs MyArgs;
		UBYTE *MyStack;

		MyStack = AllocVec(MINSTACK, MEMF_PUBLIC);
		if (MyStack)
		{
			MySSS.stk_Lower   = (void *)MyStack;
			MySSS.stk_Upper   = (ULONG) &MyStack[MINSTACK];
			MySSS.stk_Pointer = (void *)MySSS.stk_Upper;
			MyArgs.Args[0] = 0;
			MyArgs.Args[1] = 0;
			rc = NewPPCStackSwap(&MySSS, &main, &MyArgs);
			FreeVec(MyStack);
		}
	}
	else
	{
		rc = main(0, NULL);
	}

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
			if ((out = Open("CON:10/10/320/80/SimpleMail/AUTO/CLOSE/WAIT", MODE_OLDFILE)))
				oldout = SelectOutput(out);
		}

		if (open_libs())
		{
			if (init_mem())
			{
				BPTR dirlock = DupLock(pr->pr_CurrentDir);
				if (!dirlock) dirlock = Lock("PROGDIR:", ACCESS_READ);
				if (dirlock)
				{
					BPTR odir = CurrentDir(dirlock);

					if (init_io())
					{
						ULONG MySize;
						struct Task *MyTask = &pr->pr_Task;

						if (!NewGetTaskAttrsA(MyTask, &MySize, sizeof(MySize), TASKINFOTYPE_STACKSIZE_M68K, NULL))
						{
							MySize = (ULONG)MyTask->tc_SPUpper - (ULONG)MyTask->tc_SPLower;
						}

						if (MySize < MIN68KSTACK)
						{
							struct StackSwapStruct *sss;

							sss = AllocMem(sizeof(*sss) + MIN68KSTACK, MEMF_PUBLIC);
							if (sss)
							{
								sss->stk_Lower   = sss + 1;
								sss->stk_Upper   = (ULONG) (((UBYTE *) (sss + 1)) + MIN68KSTACK);
								sss->stk_Pointer = (APTR) sss->stk_Upper;
								StackSwap(sss);
								rc = __swap_and_start();
								StackSwap(sss);
								FreeMem(sss, sizeof(*sss) + MIN68KSTACK);
							}
						}
						else
						{
							rc = __swap_and_start();
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
	CloseLibrary(ExpatBase);
	CloseLibrary(LayersBase);
	CloseLibrary(GfxBase);
	CloseLibrary(AslBase);
	CloseLibrary(WorkbenchBase);
	CloseLibrary(DiskfontBase);
	CloseLibrary(IconBase);
	CloseLibrary(IFFParseBase);
	CloseLibrary(KeymapBase);
	CloseLibrary(DataTypesBase);
	CloseLibrary(LocaleBase);
	CloseLibrary(UtilityBase);
	CloseLibrary(IntuitionBase);
}


/*****************************************
 Memory stuff (thread safe)
******************************************/

static APTR pool;

static int init_mem(void)
{
	/* Stuff can be used by multiple tasks */
	if ((pool = CreatePool(MEMF_PUBLIC | MEMF_SEM_PROTECTED, 16384, 8192)))
	{
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
	mem = AllocPooled(pool,4+size);
	if (!mem) return NULL;
	mem[0] = size;
	return mem+1;
}

void free(void *m)
{
	ULONG *mem = (ULONG*)m;
	if (!m) return;
	mem--;
	FreePooled(pool,mem,mem[0]+4);
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
 thread safe (but the file instances must not be
 shared between the threads)
****************************************************/

#define MAX_FILES 100

static struct SignalSemaphore files_sem;
static BPTR files[MAX_FILES];
static char filesbuf[4096];
static unsigned long tmpno;

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

FILE *fopen(const char *filename, const char *mode)
{
	FILE *file = NULL;
	short _file;

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

#ifdef BIG_BUFFER
	file->_bf._size = 16*1024;
	file->_bf._base = malloc(file->_bf._size);
	if(!file->_bf._base) goto fail;
#endif

	if (!(files[_file] = Open(filename,amiga_mode))) goto fail;
	file->_file = _file;

#ifdef BIG_BUFFER
	if(SetVBuf(files[_file], file->_bf._base, BUF_FULL, file->_bf._size) != 0)
	{
		Close(files[_file]);
		goto fail;
	}
#endif

	if (amiga_mode == MODE_READWRITE)
	{
		Seek(files[_file],0,OFFSET_END);
		file->_offset = Seek(files[_file],0,OFFSET_CURRENT);
	}

	ReleaseSemaphore(&files_sem);
	D(bug("0x%lx opened %s\n",file,filename));
	return file;

fail:
	if (_file < MAX_FILES) files[_file] = NULL;
#ifdef BIG_BUFFER
	if(file)
	{
		free(file->_bf._base);
	}
#endif
	free(file);
	ReleaseSemaphore(&files_sem);
	return NULL;
}

int fclose(FILE *file)
{
	int error;
	if (!file) return NULL;
	ObtainSemaphore(&files_sem);
	error = !Close(files[file->_file]);
	if (!error)
	{
		files[file->_file] = NULL;
#ifdef BIG_BUFFER
		if(file)
		{
			free(file->_bf._base);
		}
#endif
		if(file->_cookie)
		{
			DeleteFile(file->_cookie);
			free(file->_cookie);
		}
		free(file);
	}
	ReleaseSemaphore(&files_sem);
	return error;
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE *file)
{
	BPTR fh = files[file->_file];
	size_t rc = (size_t)FWrite(fh,(void*)buffer,size,count);
	file->_offset += rc * size;
	return rc;
}

size_t fread(void *buffer, size_t size, size_t count, FILE *file)
{
	size_t len;
	BPTR fh = files[file->_file];
	len = (size_t)FRead(fh,buffer,size,count);
	if (!len && size && count) file->_flags |= __SEOF;
	D(bug("0x%lx reading %ld bytes\n",file,len * size));
	file->_offset += len * size;
	return len;
}

int fputs(const char *string, FILE *file)
{
	BPTR fh = files[file->_file];
	int rc = FPuts(fh,(char*)string);
	if (!rc) /* DOSFALSE is true here */
		file->_offset += strlen(string);
	return rc;
}

int fputc(int character, FILE *file)
{
	BPTR fh = files[file->_file];
	int rc = FPutC(fh,character);
	if (rc != -1) file->_offset++;
	return rc;
}

int fseek(FILE *file, long offset, int origin)
{
	BPTR fh = files[file->_file];
	ULONG amiga_seek;

	if (origin == SEEK_SET) amiga_seek = OFFSET_BEGINNING;
	else if (origin == SEEK_CUR)
	{
		/* Optimize trivial cases (used heavily when loading indexfiles) */
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
	file->_offset = Seek(fh,0,OFFSET_CURRENT);
	return 0;
}

long ftell(FILE *file)
{
	BPTR fh = files[file->_file];
	D(bug("0x%lx ftell() = %ld\n",file,Seek(fh,0,OFFSET_CURRENT)));
#ifdef FAST_SEEK
	return file->_offset;
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
	if (!rc && !IoErr()) file->_flags |= __SEOF;
	else if (rc) file->_offset += strlen(rc);
	return rc;
}

int fgetc(FILE *file)
{
	BPTR fh = files[file->_file];
	int rc = FGetC(fh);
	if (rc != -1) file->_offset++;
	return rc;
}

FILE *tmpfile(void)
{
	char *buf;
	FILE *file = NULL;
	buf = malloc(40);
	if(buf)
	{
		ObtainSemaphore(&files_sem);
		sprintf(buf,"T:%p%lx.tmp",FindTask(NULL),tmpno++);
		file = fopen(buf,"w");
		ReleaseSemaphore(&files_sem);
		if(file)
		{
			file->_cookie = buf; /* hack to delete tmp files at fclose() */
		}
		else
		{
			free(buf);
		}
	}
	return file;
}

char *tmpnam(char *name)
{
	static char default_buf[L_tmpnam];
	ObtainSemaphore(&files_sem);
	if (!name) name = default_buf;
	snprintf(name,sizeof(default_buf),"T:sm%05lx",tmpno);
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

	if ((lock = Lock(filename,ACCESS_READ)))
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
	return PutStr(string);
}

DIR *opendir(const char *name)
{
	BPTR dh;
	struct FileInfoBlock *fib;
	DIR *dir = malloc(sizeof(*dir) + sizeof(struct dirent));
	if (!dir) return NULL;

	if (!(dh = Lock(name,ACCESS_READ)))
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

int closedir(DIR *dir)
{
	FreeDosObject(DOS_FIB,(APTR)dir->dd_fd);
	UnLock((BPTR)dir->dd_loc);
	free(dir);

	return 0;
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
	BPTR lock = Lock(dir,ACCESS_READ);
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
	if (!(Rename(oldname,newname)))
		return -1;
	return 0;
}


/**************
 MUI Dispatcher
***************/

#include <intuition/classes.h>
#include <libraries/mui.h>

#include <emul/emulregs.h>

static ULONG muiDispatcherGate(void)
{
	ULONG (*dispatcher)(struct IClass *, Object *, Msg);

	struct IClass *cl  = (struct IClass *)REG_A0;
	Object        *obj = (Object *)       REG_A2;
	Msg           msg  = (Msg)            REG_A1;

	dispatcher = (ULONG(*)(struct IClass *, Object *, Msg))cl->cl_UserData;

	return dispatcher(cl, obj, msg);
}

struct EmulLibEntry muiDispatcherEntry =
{
	TRAP_LIB, 0, (void (*)(void)) muiDispatcherGate
};


/********************
 expat.library hack
*********************/

#include <emul/emulregs.h>
#include "expatinc.h"

void xml_start_tag(void *, const char *, const char **);

void xml_start_tag_gate(void)
{
	void *data        = ((void **)       REG_A7)[1];
	char *el          = ((char **)       REG_A7)[2];
	const char **attr = ((const char ***)REG_A7)[3];

	xml_start_tag(data, el, attr);
}

struct EmulLibEntry xml_start_tag_trap =
{
	TRAP_LIB, 0, (void (*)(void)) xml_start_tag_gate
};

void xml_end_tag(void *, const char *);

void xml_end_tag_gate(void)
{
	void *data = ((void **)REG_A7)[1];
	char *el   = ((char **)REG_A7)[2];

	xml_end_tag(data, el);
}

struct EmulLibEntry xml_end_tag_trap =
{
	TRAP_LIB, 0, (void (*)(void)) xml_end_tag_gate
};

void xml_char_data(void *, const XML_Char *, int);

void xml_char_data_gate(void)
{
	void *data  = ((void **)    REG_A7)[1];
	XML_Char *s = ((XML_Char **)REG_A7)[2];
	int len     = ((int *)      REG_A7)[3];

	xml_char_data(data, s, len);
}

struct EmulLibEntry xml_char_data_trap =
{
	TRAP_LIB, 0, (void (*)(void)) xml_char_data_gate
};

