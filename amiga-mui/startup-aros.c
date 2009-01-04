#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* what an ugly hack
   needed for the stdio clone */
#undef __amigaos__
#include <dirent.h>
#define __amigaos__

// #include <sys/dir.h>
#include <sys/stat.h>

#include <dos/stdio.h>
#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>

//#define MYDEBUG
#include "amigadebug.h"

#define FAST_SEEK
//#define BIG_BUFFER

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
//struct Library *ExpatBase;

static int start(struct WBStartup *wbs);

static int open_libs(void);
static void close_libs(void);
static int init_mem(void);
static void deinit_mem(void);
static int init_io(void);
static void deinit_io(void);

#define MINSTACK 60000

int main(int argc, char **argv)
{
	struct WBStartup *wbs = NULL;

	if (argc == 0)
	{
		wbs = (struct WBStartup*)argv;
	}
	return start(wbs);
}

static int rc;

static int start(struct WBStartup *wbs)
{
	int rc = 20;
	struct Process *pr = (struct Process*)FindTask(NULL);

	if ((DOSBase = (struct DosLibrary*)OpenLibrary("dos.library",37)))
	{
		BPTR out = out;
		BPTR oldout = oldout;

		if (wbs)
		{
			if ((out = Open("CON:10/10/320/80/SimpleMail/AUTO/CLOSE/WAIT", MODE_OLDFILE)))
			{
				oldout = SelectOutput(out);
			}
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
						rc = simplemail_main();
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
	if ((IntuitionBase = OpenLibrary("intuition.library", 37)))
	{
		if ((UtilityBase = OpenLibrary("utility.library", 37)))
		{
			if ((LocaleBase = OpenLibrary("locale.library", 37)))
			{
				if ((DataTypesBase = OpenLibrary("datatypes.library", 39)))
				{
					if ((KeymapBase = OpenLibrary("keymap.library", 36)))
					{
						if ((IFFParseBase = OpenLibrary("iffparse.library", 37)))
						{
							if ((IconBase = OpenLibrary("icon.library", 37)))
							{
								if ((DiskfontBase = OpenLibrary("diskfont.library", 37)))
								{
									if ((WorkbenchBase = OpenLibrary("workbench.library", 37)))
									{
										if ((AslBase = OpenLibrary("asl.library", 38)))
										{
											if ((GfxBase = OpenLibrary("graphics.library", 37)))
											{
												if ((LayersBase = OpenLibrary("layers.library", 37)))
												{
//													if (!(ExpatBase = OpenLibrary("expat.library", 0)))
//													{
//														ExpatBase = OpenLibrary("PROGDIR:libs/expat.library", 0);
//													}

//													if (ExpatBase)
//													{
														return 1;
//													}
//													else PutStr("Couldn't open expat.library. Please download it from aminet or somewhere else.\n");
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
//	CloseLibrary(ExpatBase);
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

	mem = AllocPooled(pool, 4 + size);
	if (mem)
	{
		mem[0] = size;
		return mem + 1;
	}

	return NULL;
}

void free(void *m)
{
	ULONG *mem = (ULONG*)m;

	if (mem)
	{
		mem--;
		FreePooled(pool, mem, mem[0] + 4);
	}
}

void *realloc(void *om, size_t size)
{
	if (om)
	{
		ULONG *oldmem = (ULONG*)om;

		oldmem--;
		if (size > oldmem[0])
		{
			void *mem;

			mem = malloc(size);
			if (mem)
			{
				memcpy(mem, om, oldmem[0]);
				free(om);
			}

			return mem;
		}

		if (size < oldmem[0])
		{
			void *mem;
		
			mem = malloc(size);
			if (mem)
			{
				memcpy(mem, om, size);
				free(om);
			}

			return mem;
		}

		return om;
	}

	return malloc(size);
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

	for (i = 0; i < MAX_FILES; i++)
	{
		if (files[i])
		{
			Close(files[i]);
		}
	}
}

char *tmpnam(char *name)
{
	static char default_buf[L_tmpnam];

	ObtainSemaphore(&files_sem);
	if (!name)
	{
		name = default_buf;
	}
	snprintf(name, sizeof(default_buf), "T:sm%05lx", tmpno);
	tmpno++;
	ReleaseSemaphore(&files_sem);

	return name;
}

int remove(const char *filename)
{
	if (DeleteFile((char*)filename))
	{
		return 0;
	}

	return -1;
}

int stat(const char *filename, struct stat *stat)
{
	BPTR lock;
	int rc = -1;

	if ((lock = Lock(filename,ACCESS_READ)))
	{
		struct FileInfoBlock *fib;

		fib = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL);
		if (fib)
		{
			if (Examine(lock, fib))
			{
				memset(stat, 0, sizeof(*stat));

				if (fib->fib_DirEntryType > 0)
				{
					stat->st_mode |= S_IFDIR;
				}

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
	}
	else
	{	
		rc = -1;
	}

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
	}
	else
	{
		rc = -1;
	}

	ReleaseSemaphore(&files_sem);

	return size;
}

int puts(const char *string)
{
	return PutStr(string);
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

int chdir(const char *dir);
int chdir(const char *dir)
{
	BPTR lock;
	BPTR odir;

	lock = Lock(dir, ACCESS_READ);
	if (lock)
	{
		odir = CurrentDir(lock);
		UnLock(odir);

		return 0;
	}

	return -1;
}

char *getcwd(char *buf, int size);
char *getcwd(char *buf, int size)
{
	struct Process *pr = (struct Process*)FindTask(NULL);

	if (NameFromLock(pr->pr_CurrentDir, buf, size))
	{
		return buf;
	}

	return NULL;
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
	{
		return -1;
	}

	return 0;
}
