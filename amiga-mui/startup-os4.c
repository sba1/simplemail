#undef __USE_INLINE__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <dirent.h>
#include <sys/stat.h>

#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#define MIN(a,b) (((a)<(b))?(a):(b))

#define MEMWALL_FRONT_SIZE 1024  /* multiple of 4 */
#define MEMWALL_BACK_SIZE  1024  /* multiple of 4 */

#define FAST_SEEK

/*#define MEMWALL*/
/*#define MYDEBUG*/
#include "amigadebug.h"

struct Library *SysBase;
struct Library *DOSBase;
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
// struct Library *ExpatBase;

struct ExecIFace *IExec;
struct DOSIFace *IDOS;
struct Interface *IUtility;
struct Interface *IIntuition;
struct LocaleIFace *ILocale;
struct Interface *IDataTypes;
struct Interface *IKeymap;
struct Interface *IIFFParse;
struct Interface *IIcon;
struct Interface *IDiskfont;
struct Interface *IWorkbench;
struct Interface *IAsl;
struct Interface *IGraphics;
struct Interface *ILayers;
struct Interface *IExpat;

struct Locale *DefaultLocale;

int main(int argc, char *argv[]);

static int start(struct WBStartup *wbs);

static int open_libs(void);
static void close_libs(void);
static int init_mem(void);
static void deinit_mem(void);
static int init_io(void);
static void deinit_io(void);

static const char stack[] = "$STACK: 60000";

int _start(void)
{
	struct Process *pr;
	int rc;

	SysBase = *((struct Library**)4);
	IExec = (struct ExecIFace*)((struct ExecBase*)SysBase)->MainInterface;
	pr = (struct Process*)IExec->FindTask(NULL);

	if (!pr->pr_CLI)
	{
		struct WBStartup *wbs;

		IExec->WaitPort(&pr->pr_MsgPort);
		wbs = (struct WBStartup*)IExec->GetMsg(&pr->pr_MsgPort);

		rc = start(wbs);

		IExec->Forbid();
		IExec->ReplyMsg((struct Message *)wbs);
	}	else rc = start(NULL);
	return rc;
}

struct Library *OpenLibraryInterface(STRPTR name, int version, void *interface_ptr)
{
	struct Library *lib = IExec->OpenLibrary(name,version);
	struct Interface *iface;
	if (!lib) return NULL;

	iface = IExec->GetInterface(lib,"main",1,NULL);
	if (!iface)
	{
		IExec->CloseLibrary(lib);
		return NULL;
	}
	*((struct Interface**)interface_ptr) = iface;
	return lib;
}

void CloseLibraryInterface(struct Library *lib, void *interface)
{
	IExec->DropInterface(interface);
	IExec->CloseLibrary(lib);
}

static int start(struct WBStartup *wbs)
{
	int rc = 20;
	struct Process *pr = (struct Process*)IExec->FindTask(NULL);

	if ((DOSBase = OpenLibraryInterface("dos.library",37,&IDOS)))
	{
		BPTR out = ZERO;
		BPTR oldout = ZERO;

		if (wbs)
		{
			if ((out = IDOS->Open("CON:10/10/320/80/SimpleMail/AUTO/CLOSE/WAIT",MODE_OLDFILE)))
				oldout = IDOS->SelectOutput(out);
		}

		if (open_libs())
		{
			DefaultLocale = ILocale->OpenLocale(NULL);

			if (init_mem())
			{
				BPTR dirlock = IDOS->DupLock(pr->pr_CurrentDir);
				if (!dirlock) dirlock = IDOS->Lock("PROGDIR:",ACCESS_READ);
				if (dirlock)
				{
					BPTR odir = IDOS->CurrentDir(dirlock);
					if (init_io())
					{
						rc = main(0,NULL);
						deinit_io();
					}
					deinit_mem();
					IDOS->UnLock(IDOS->CurrentDir(odir));
				}
			}
			if (DefaultLocale) ILocale->CloseLocale(DefaultLocale);
			close_libs();
		}

		if (wbs && out)
		{
			IDOS->SelectOutput(oldout);
			IDOS->Close(out);
		}

		CloseLibraryInterface((struct Library*)DOSBase,IDOS);
	}

	return rc;
}

static int open_libs(void)
{
	if ((IntuitionBase = OpenLibraryInterface("intuition.library",37,&IIntuition)))
	{
		if ((UtilityBase = OpenLibraryInterface("utility.library",37,&IUtility)))
		{
			if ((LocaleBase = OpenLibraryInterface("locale.library",37,&ILocale)))
			{
				if ((DataTypesBase = OpenLibraryInterface("datatypes.library",39,&IDataTypes)))
				{
					if ((KeymapBase = OpenLibraryInterface("keymap.library",36,&IKeymap)))
					{
						if ((IFFParseBase = OpenLibraryInterface("iffparse.library",37,&IIFFParse)))
						{
							if ((IconBase = OpenLibraryInterface("icon.library",37,&IIcon)))
							{
								if ((DiskfontBase = OpenLibraryInterface("diskfont.library",37,&IDiskfont)))
								{
									if ((WorkbenchBase = OpenLibraryInterface("workbench.library",37,&IWorkbench)))
									{
										if ((AslBase = OpenLibraryInterface("asl.library",38,&IAsl)))
										{
											if ((GfxBase = OpenLibraryInterface("graphics.library",37,&IGraphics)))
											{
												if ((LayersBase = OpenLibraryInterface("layers.library",37,&ILayers)))
												{
//													if (!(ExpatBase = OpenLibraryInterface("expat.library",0,&IExpat)))
//														ExpatBase = OpenLibraryInterface("PROGDIR:libs/expat.library",0,&IExpat);

//													if (ExpatBase)
													{
														return 1;
													} //else IDOS->PutStr("Couldn't open expat.library. Please download it from aminet or somewhere else.\n");
												}
											}
										} else IDOS->PutStr("Couldn't open asl.library\n");
									} else IDOS->PutStr("Couldn't open workbench.library\n");
								} else IDOS->PutStr("Couldn't open diskfont.library\n");
							} else IDOS->PutStr("Couldn't open icon.library\n");
						} else IDOS->PutStr("Couldn't open iffparse.library\n");
					} else IDOS->PutStr("Couldn't open keymap.library\n");
				} else IDOS->PutStr("Couldn't open datatypes.library\n");
			} else IDOS->PutStr("Couldn't open locale.library\n");
		}
	}
	close_libs();
	return 0;
}

static void close_libs(void)
{
//	CloseLibraryInterface(ExpatBase,IExpat);
	CloseLibraryInterface(LayersBase,ILayers);
	CloseLibraryInterface(GfxBase,IGraphics);
	CloseLibraryInterface(AslBase,IAsl);
	CloseLibraryInterface(WorkbenchBase,IWorkbench);
	CloseLibraryInterface(DiskfontBase,IDiskfont);
	CloseLibraryInterface(IconBase,IIcon);
	CloseLibraryInterface(IFFParseBase,IIFFParse);
	CloseLibraryInterface(KeymapBase,IKeymap);
	CloseLibraryInterface(DataTypesBase,IDataTypes);
	CloseLibraryInterface(LocaleBase,ILocale);
	CloseLibraryInterface(UtilityBase,IUtility);
	CloseLibraryInterface(IntuitionBase,IIntuition);
}

/*****************************************
 Memory stuff (thread safe)
******************************************/

APTR pool;
struct SignalSemaphore pool_sem;

static int init_mem(void)
{
	/* Stuff can be used by multiple tasks */
	if ((pool = IExec->CreatePool(MEMF_SHARED,16384,8192)))
	{
		IExec->InitSemaphore(&pool_sem);
		return 1;
	}
	return 0;
}

static void deinit_mem(void)
{
	if (pool) IExec->DeletePool(pool);
}

#ifdef MEMWALL
#define MEM_INCREASED_SIZE (4 + MEMWALL_FRONT_SIZE + MEMWALL_BACK_SIZE)
#else
#define MEM_INCREASED_SIZE 4
#endif

void *malloc (size_t size)
{
	ULONG *mem;
	ULONG memsize;

	memsize = size + MEM_INCREASED_SIZE;

	IExec->ObtainSemaphore(&pool_sem);
	mem = IExec->AllocPooled(pool,memsize);
	IExec->ReleaseSemaphore(&pool_sem);
	if (!mem) return NULL;
	mem[0] = memsize;

#ifdef MEMWALL
	memset(((UBYTE*)mem) + 4, 0xaa, MEMWALL_FRONT_SIZE);
	memset(((UBYTE*)mem) + 4 + MEMWALL_FRONT_SIZE + size, 0xaa, MEMWALL_BACK_SIZE);
	return mem+MEMWALL_FRONT_SIZE/4+1;
#else
	return mem+1;
#endif
}

void free(void *m)
{
	ULONG *mem = (ULONG*)m;
	ULONG memsize;
	if (!m) return;

#ifdef MEMWALL
	mem -= 1 + MEMWALL_FRONT_SIZE/4;
	{
		unsigned char *buf;
		int i;

		buf = (unsigned char*)(mem + 1);
		for (i=0;i<MEMWALL_FRONT_SIZE;i++)
		{
			if (buf[i] != 0xaa)
			{
				D(bug("Front wall stomped!\n"));
				break;
			}
		}

		buf = (unsigned char*)mem;
		buf += mem[0] - MEMWALL_BACK_SIZE;
		for (i=0;i<MEMWALL_BACK_SIZE;i++)
		{
			if (buf[i] != 0xaa)
			{
				D(bug("Back wall stomped!\n"));
				break;
			}
		}
	}
#else
	mem--;
#endif

	memsize = mem[0];
	memset(mem,0xcc,memsize);
	IExec->ObtainSemaphore(&pool_sem);
	IExec->FreePooled(pool,mem,memsize);
	IExec->ReleaseSemaphore(&pool_sem);
}

void *realloc(void *om, size_t size)
{
	void *nm;
	ULONG *oldmem;
	size_t oldsize;

	if (!om) return malloc(size);

	oldmem = (ULONG*)om;
	oldmem--;
	oldsize = oldmem[0] - MEM_INCREASED_SIZE;

	if (size == oldsize) return om;
	if (!(nm = malloc(size))) return NULL;
	memcpy(nm,om,MIN(size,oldsize));
	free(om);
	return nm;
}

/***************************************************
 IO Stuff.
 This is a very simple and primitive functions of
 the ANSI C File IOs. Only the SimpleMail neeeded
 functionality is implemented. The functions are
 thread safe (but the file intstances mustn't be
 shared between the threads)
****************************************************/

int vsnprintf(char *buffer, size_t buffersize, const char *fmt0, va_list ap);

#define MAX_FILES 100

static struct SignalSemaphore files_sem;
static BPTR files[MAX_FILES];
static char filesbuf[4096];
static unsigned long tmpno;

static int init_io(void)
{
	IExec->InitSemaphore(&files_sem);
	return 1;
}

static void deinit_io(void)
{
	int i;
	for (i=0;i<MAX_FILES;i++)
		if (files[i]) IDOS->Close(files[i]);
}

int errno;

struct myfile
{
	int _file;
	int _eof;
#ifdef FAST_SEEK
	int _rcnt;
#endif
};

static LONG internal_seek(BPTR fh, LONG pos, LONG mode)
{
	LONG rc;

	if (DOSBase->lib_Version < 51) rc = IDOS->Seek(fh,pos,mode);
	else rc = IDOS->FSeek(fh,pos,mode);

	return rc;
}

FILE *fopen(const char *filename, const char *mode)
{
	struct myfile *file = NULL;
	int _file;

	LONG amiga_mode;
	if (*mode == 'w') amiga_mode = MODE_NEWFILE;
	else if (*mode == 'r') amiga_mode = MODE_OLDFILE;
	else if (*mode == 'a') amiga_mode = MODE_READWRITE;
	else return NULL;

	IExec->ObtainSemaphore(&files_sem);
	/* Look if we can still open file left */
	for (_file=0;_file < MAX_FILES && files[_file];_file++);
	if (_file == MAX_FILES) goto fail;

	file = malloc(sizeof(struct myfile));
	if (!file) goto fail;
	memset(file,0,sizeof(struct myfile));

	if (DOSBase->lib_Version < 51) files[_file] = IDOS->Open((STRPTR)filename,amiga_mode);
	else files[_file] = IDOS->FOpen((STRPTR)filename,amiga_mode,8192);

	if (!files[_file]) goto fail;

	file->_file = _file;

	if (amiga_mode == MODE_READWRITE)
	{
		internal_seek(files[_file],0,OFFSET_END);
		file->_rcnt = internal_seek(files[_file],0,OFFSET_CURRENT);
	}

	IExec->ReleaseSemaphore(&files_sem);
	D(bug("fopen: opened 0x%lx(%ld) %s\n",file,file->_file,filename));
	return (FILE*)file;

fail:
	if (_file < MAX_FILES) files[_file] = ZERO;
	free(file);
	IExec->ReleaseSemaphore(&files_sem);
	return NULL;
}

int fclose(FILE *f)
{
	struct myfile *file = (struct myfile*)f;
	int error;

	D(bug("fclose: close 0x%lx(%ld)\n",file,file->_file));

	if (!file) return 0;
	IExec->ObtainSemaphore(&files_sem);
	
	if (DOSBase->lib_Version < 51) 	error = !(IDOS->Close(files[file->_file]));
	else error = !(IDOS->FClose(files[file->_file]));

	if (!error)
	{
		files[file->_file] = ZERO;
		free(file);
	} else D(bug("Failed closeing 0x%lx",file));
	IExec->ReleaseSemaphore(&files_sem);
	return error;
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE *f)
{
	struct myfile *file = (struct myfile*)f;
	BPTR fh = files[file->_file];
	size_t bytes = (size_t)IDOS->FWrite(fh,(void*)buffer,size,count);
#ifdef FAST_SEEK
	file->_rcnt += bytes;
#endif
	return bytes;
}

size_t fread(void *buffer, size_t size, size_t count, FILE *f)
{
	struct myfile *file;
	size_t len;
	BPTR fh;

	file = (struct myfile*)f;
	fh = files[file->_file];
	len = (size_t)IDOS->FRead(fh,buffer,size,count);

	if (!len && size && count) file->_eof = 1;
#ifdef FAST_SEEK
	file->_rcnt += len;
#endif
	return len;
}

int fputs(const char *string, FILE *f)
{
	struct myfile *file;
	BPTR fh;
	int rc;

	file = (struct myfile*)f;
	fh = files[file->_file];
	rc = IDOS->FPuts(fh,(char*)string);

#ifdef FAST_SEEK
	if (!rc) /* DOSFALSE is true here */
		file->_rcnt += strlen(string);
#endif

	return rc;
}

int fputc(int character, FILE *f)
{
	struct myfile *file;
	BPTR fh;
	int rc;

	file = (struct myfile*)f;
	fh = files[file->_file];
	rc = IDOS->FPutC(fh,character);
#ifdef FAST_SEEK
	if (rc != -1) file->_rcnt++;
#endif
	return rc;
}

int fseek(FILE *f, long offset, int origin)
{
	struct myfile *file = (struct myfile*)f;
	BPTR fh = files[file->_file];
	ULONG amiga_seek;

	if (origin == SEEK_SET) amiga_seek = OFFSET_BEGINNING;
	else if (origin == SEEK_CUR)
	{
		amiga_seek = OFFSET_CURRENT;
		if (!offset) return 0;
		if (offset == 1)
		{
			fgetc(f);
			return 0;
		}
	}
	else amiga_seek = OFFSET_END;

	if (!IDOS->FFlush(fh)) return -1;
	if (internal_seek(fh,offset,amiga_seek)==-1) return -1;
#ifdef FAST_SEEK
	file->_rcnt = internal_seek(fh,0,OFFSET_CURRENT);
#endif
	return 0;
}

long ftell(FILE *f)
{
	struct myfile *file = (struct myfile*)f;
	BPTR fh = files[file->_file];
#ifdef FAST_SEEK
	D(bug("0x%lx ftell() = %ld, seek=%ld\n",file, file->_rcnt,internal_seek(fh,0,OFFSET_CURRENT)));
	return file->_rcnt;
#else
	return internal_seek(fh,0,OFFSET_CURRENT);
#endif
}

int fflush(FILE *f)
{
	struct myfile *file = (struct myfile*)f;
	BPTR fh = files[file->_file];
	if (IDOS->FFlush(fh)) return 0;
	return -1;
}

char *fgets(char *string, int n, FILE *f)
{
	struct myfile *file;
	BPTR fh;
	char *rc;

	file = (struct myfile*)f;
	fh = files[file->_file];

	rc =  (char*)IDOS->FGets(fh,string,n);
	if (!rc && !IDOS->IoErr()) file->_eof = 1;
#ifdef FAST_SEEK
	else file->_rcnt += strlen(rc);
#endif
	return rc;
}

int fgetc(FILE *f)
{
	struct myfile *file;
	BPTR fh;
	int rc;

	file = (struct myfile*)f;
	fh = files[file->_file];

	rc = IDOS->FGetC(fh);
#ifdef FAST_SEEK
	if (rc != -1) file->_rcnt++;
#endif
	return rc;
}

int feof(FILE *f)
{
	struct myfile *file = (struct myfile*)f;
	return file->_eof;
}

FILE *tmpfile(void)
{
	char buf[40];
	FILE *file;
	IExec->ObtainSemaphore(&files_sem);
	sprintf(buf,"T:%p%lx.tmp",IExec->FindTask(NULL),tmpno++);
	file = fopen(buf,"w");
	IExec->ReleaseSemaphore(&files_sem);
	return file;
}

char *tmpnam(char *name)
{
	static char default_buf[L_tmpnam];
	IExec->ObtainSemaphore(&files_sem);
	if (!name) name = default_buf;
	snprintf(name,sizeof(default_buf),"T:sm%05lx",tmpno);
	tmpno++;
	IExec->ReleaseSemaphore(&files_sem);
	return name;
}

int remove(const char *filename)
{
	if (IDOS->DeleteFile((char*)filename)) return 0;
	return -1;
}

int stat(const char *filename, struct stat *stat)
{
	BPTR lock;
	int rc = -1;

	if ((lock = IDOS->Lock((STRPTR)filename,ACCESS_READ)))
	{
		struct FileInfoBlock *fib = (struct FileInfoBlock*)IDOS->AllocDosObject(DOS_FIB,NULL);
		if (fib)
		{
			if (IDOS->Examine(lock,fib))
			{
				memset(stat,0,sizeof(*stat));
				if (fib->fib_DirEntryType > 0) stat->st_mode |= S_IFDIR;
				rc = 0;
			}
			IDOS->FreeDosObject(DOS_FIB,fib);
		}
		IDOS->UnLock(lock);
	}
	return rc;
}

int fprintf(FILE *file, const char *fmt,...)
{
	int rc;
	int size;

	va_list ap;

	IExec->ObtainSemaphore(&files_sem);
	va_start(ap, fmt);
	size = vsnprintf(filesbuf, sizeof(filesbuf), fmt, ap);
	va_end(ap);

	if (size >= 0)
	{
		rc = fwrite(filesbuf,1,size,file);
	} else rc = -1;

	IExec->ReleaseSemaphore(&files_sem);

	return rc;
}


#undef printf

int printf(const char *fmt,...)
{
	int rc;
	int size;

	va_list ap;

	IExec->ObtainSemaphore(&files_sem);
	va_start(ap, fmt);
	size = vsnprintf(filesbuf, sizeof(filesbuf), fmt, ap);
	va_end(ap);

	if (size >= 0)
	{
		IDOS->PutStr(filesbuf);
	} else rc = -1;

	IExec->ReleaseSemaphore(&files_sem);

	return size;
}

int puts(const char *string)
{
	return IDOS->PutStr((STRPTR)string);
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

struct mydir
{
	unsigned long dd_fd;
	unsigned long dd_loc;
	char *dd_buf;
};

DIR *opendir(const char *name)
{
	BPTR dh;
	struct FileInfoBlock *fib;
	struct mydir *dir = malloc(sizeof(*dir) + sizeof(struct dirent));
	if (!dir) return NULL;

	if (!(dh = IDOS->Lock((STRPTR)name,ACCESS_READ)))
	{
		free(dir);
		return NULL;
	}

	if (!(fib = IDOS->AllocDosObject(DOS_FIB,NULL)))
	{
		IDOS->UnLock(dh);
		free(dir);
		return NULL;
	}

	if (!(IDOS->Examine(dh,fib)))
	{
		IDOS->UnLock(dh);
		free(dir);
		return NULL;
	}

	dir->dd_fd = ((unsigned long)fib);
	dir->dd_loc = ((unsigned long)dh);
	dir->dd_buf = (char*)(dir+1);

	D(bug("Opendir %s\n",name));

	return (DIR*)dir;
}

int closedir(DIR *d)
{
	struct mydir *dir = (struct mydir*)d;
	IDOS->FreeDosObject(DOS_FIB,(APTR)dir->dd_fd);
	IDOS->UnLock((BPTR)dir->dd_loc);
	free(dir);
	return 1;
}

struct dirent *readdir(DIR *d)
{
	struct mydir *dir = (struct mydir*)d;
	struct dirent *dirent = (struct dirent*)dir->dd_buf;
	struct FileInfoBlock *fib = (struct FileInfoBlock*)dir->dd_fd;
	if (IDOS->ExNext((BPTR)dir->dd_loc,fib))
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
	BPTR lock = IDOS->Lock((STRPTR)dir,ACCESS_READ);
	BPTR odir;
	if (!lock) return -1;
	odir = IDOS->CurrentDir(lock);
	IDOS->UnLock(odir);
	return 0;
}

char *getcwd(char *buf, int size)
{
	struct Process *pr = (struct Process*)IExec->FindTask(NULL);
	if (!(IDOS->NameFromLock(pr->pr_CurrentDir, buf, size))) return NULL;
	return buf;
}

char *getenv(const char *name)
{
	return NULL;
}

/*struct tm *localtime(const time_t *timeptr)
{
}*/

int rename(const char *oldname, const char *newname)
{
	if (!(IDOS->Rename((STRPTR)oldname,(STRPTR)newname)))
		return -1;
	return 0;
}

int isspace(int c)
{
	return (c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f' || c == ' ');
}

int isalpha(int c)
{
	return (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'));
}

int isdigit(int c)
{
	return ('0' <= c && c <= '9');
}

int tolower(int c)
{
	return ('A' <= c && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

int toupper(int c)
{
	return ('a' <= c && c <= 'z') ? (c - ('a' - 'A')) : c;
}

int iscntrl(int c)
{
	return (('\0' <= c && c < ' ') || (c == 127));
}
