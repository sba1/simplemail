#include <signal.h>

#include <dos/stdio.h>
#include <workbench/startup.h>

#include <proto/exec.h>
#include <proto/dos.h>

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

#define MIN68KSTACK 40000
#define MINSTACK 60000

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
													if (!(ExpatBase = OpenLibrary("expat.library", 0)))
													{
														ExpatBase = OpenLibrary("PROGDIR:libs/expat.library", 0);
													}

													if (ExpatBase)
													{
														return 1;
													}
													else PutStr("Couldn't open expat.library. Please download it from aminet or somewhere else.\n");
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

static int __swap_and_start(void)
{
	ULONG MySize;
	struct Task *MyTask = FindTask(NULL);
	int rc = 20;

	if (!NewGetTaskAttrsA(MyTask, &MySize, sizeof(MySize), TASKINFOTYPE_STACKSIZE, NULL))
	{
		MySize = (ULONG)MyTask->tc_ETask->PPCSPUpper - (ULONG)MyTask->tc_ETask->PPCSPLower;
	}

	if (MySize < MINSTACK)
	{
		struct StackSwapStruct MySSS;
		UBYTE *MyStack;

		MyStack = AllocVec(MINSTACK, MEMF_PUBLIC);
		if (MyStack)
		{
			MySSS.stk_Lower   = (void *)MyStack;
			MySSS.stk_Upper   = (ULONG) &MyStack[MINSTACK];
			MySSS.stk_Pointer = (void *)MySSS.stk_Upper;
			rc = NewPPCStackSwap(&MySSS, &simplemail_main, NULL);
			FreeVec(MyStack);
		}
	}
	else
	{
		rc = simplemail_main();
	}

	return rc;
}

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
			BPTR dirlock = DupLock(pr->pr_CurrentDir);
			if (!dirlock) dirlock = Lock("PROGDIR:", ACCESS_READ);
			if (dirlock)
			{
				BPTR odir = CurrentDir(dirlock);
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
				UnLock(CurrentDir(odir));
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

int main(int argc, char **argv)
{
	struct WBStartup *wbs = NULL;

	if (argc == 0)
	{
		wbs = (struct WBStartup*)argv;
	}
	
	/* Avoid libnix calls raise()/exit() on CTRL-C */
	signal(SIGINT, SIG_IGN);

	return start(wbs);
}


/*****************************************************
 * bsdsocket                                         *
 * Notes: in order to not use the global SocketBase, *
 *        OpenSSL must be built using _NO_PPCINLINE. *
 *****************************************************/

#include "socket-stubs.h"


LONG socket(LONG domain, LONG type, LONG protocol)
{
	return stub_socket(domain, type, protocol);
}

LONG bind(LONG s, const void *name, LONG namelen)
{
	return stub_bind(s, name, namelen);
}

LONG listen(LONG s, LONG backlog)
{
	return stub_listen(s, backlog);
}

LONG accept(LONG s, void *addr, LONG *addrlen)
{
	return stub_accept(s, addr, addrlen);
}

LONG connect(LONG s, const void *name, LONG namelen)
{
	return stub_connect(s, name, namelen);
}

LONG send(LONG s, const UBYTE *msg, LONG len, LONG flags)
{
	return stub_send(s, msg, len, flags);
}

LONG sendto(LONG s, const UBYTE *msg, LONG len, LONG flags, const void *to, LONG tolen)
{
	return stub_sendto(s, msg, len, flags, to, tolen);
}

LONG sendmsg(LONG s, void *msg, LONG flags)
{
	return stub_sendmsg(s, msg, flags);
}

LONG recv(LONG s, UBYTE *buf, LONG len, LONG flags)
{
	return stub_recv(s, buf, len, flags);
}

LONG recvfrom(LONG s, UBYTE *buf, LONG len, LONG flags, void *from, LONG *fromlen)
{
	return stub_recvfrom(s, buf, len, flags, from, fromlen);
}

LONG recvmsg(LONG s, void *msg, LONG flags)
{
	return stub_recvmsg(s, msg, flags);
}

LONG shutdown(LONG s, LONG how)
{
	return stub_shutdown(s, how);
}

LONG setsockopt(LONG s, LONG level, LONG optname, const void *optval, LONG optlen)
{
	return stub_setsockopt(s, level, optname, optval, optlen);
}

LONG getsockopt(LONG s, LONG level, LONG optname, void *optval, LONG *optlen)
{
	return stub_getsockopt(s, level, optname, optval, optlen);
}

LONG getsockname(LONG s, void *name, LONG *namelen)
{
	return stub_getsockname(s, name, namelen);
}

LONG getpeername(LONG s, void *name, LONG *namelen)
{
	return stub_getpeername(s, name, namelen);
}

LONG IoctlSocket(LONG d, ULONG request, char *argp)
{
	return stub_IoctlSocket(d, request, argp);
}

LONG CloseSocket(LONG d)
{
	return stub_CloseSocket(d);
}

LONG WaitSelect(LONG nfds, void *readfds, void *writefds, void *exeptfds, void *timeout, ULONG *maskp)
{
	return stub_WaitSelect(nfds, readfds, writefds, exeptfds, timeout, maskp);
}

LONG Dup2Socket(LONG fd1, LONG fd2)
{
	return stub_Dup2Socket(fd1, fd2);
}

LONG getdtablesize(void)
{
	return stub_getdtablesize();
}

void SetSocketSignals(ULONG _SIGINTR, ULONG _SIGIO, ULONG _SIGURG)
{
	return stub_SetSocketSignals(_SIGINTR, _SIGIO, _SIGURG);
}

LONG SetErrnoPtr(void *errno_p, LONG size)
{
	return stub_SetErrnoPtr(errno_p, size);
}

LONG SocketBaseTagList(void *tagList)
{
	return stub_SocketBaseTagList(tagList);
}

LONG GetSocketEvents(ULONG *eventmaskp)
{
	return stub_GetSocketEvents(eventmaskp);
}

LONG Errno(void)
{
	return stub_Errno();
}

LONG gethostname(STRPTR hostname, LONG size)
{
	return stub_gethostname(hostname, size);
}

ULONG gethostid(void)
{
	return stub_gethostid();
}

LONG ObtainSocket(LONG id, LONG domain, LONG type, LONG protocol)
{
	return stub_ObtainSocket(id, domain, type, protocol);
}

LONG ReleaseSocket(LONG fd, LONG id)
{
	return stub_ReleaseSocket(fd, id);
}

LONG ReleaseCopyOfSocket(LONG fd, LONG id)
{
	return stub_ReleaseCopyOfSocket(fd, id);
}

ULONG inet_addr(const UBYTE *p)
{
	return stub_inet_addr(p);
}

ULONG inet_network(const UBYTE *p)
{
	return stub_inet_network(p);
}

char *Inet_NtoA(ULONG s_addr)
{
	return stub_Inet_NtoA(s_addr);
}

ULONG Inet_MakeAddr(ULONG net, ULONG lna)
{
	return stub_Inet_MakeAddr(net, lna);
}

ULONG Inet_LnaOf(LONG s_addr)
{
	return stub_Inet_LnaOf(s_addr);
}

ULONG Inet_NetOf(LONG s_addr)
{
	return stub_Inet_NetOf(s_addr);
}

void *gethostbyname(const UBYTE *name)
{
	return stub_gethostbyname(name);
}

void *gethostbyaddr(const UBYTE *addr, LONG len, LONG type)
{
	return stub_gethostbyaddr(addr, len, type);
}

void *getnetbyname(const UBYTE *name)
{
	return stub_getnetbyname(name);
}

void *getnetbyaddr(LONG net, LONG type)
{
	return stub_getnetbyaddr(net, type);
}

void *getservbyname(const UBYTE *name, const UBYTE *proto)
{
	return stub_getservbyname(name, proto);
}

void *getservbyport(LONG port, const UBYTE *proto)
{
	return stub_getservbyport(port, proto);
}

void *getprotobyname(const UBYTE *name)
{
	return stub_getprotobyname(name);
}

void *getprotobynumber(LONG proto)
{
	return stub_getprotobynumber(proto);
}


/******************
 * MUI Dispatcher *
 ******************/

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


/**********************
 * expat.library hack *
 **********************/

#include <emul/emulregs.h>

#include "expatinc.h"

void xml_start_tag(void *, const char *, const char **);

static void xml_start_tag_gate(void)
{
	void *data        = (void *)       ((ULONG *)REG_A7)[1];
	char *el          = (char *)       ((ULONG *)REG_A7)[2];
	const char **attr = (const char **)((ULONG *)REG_A7)[3];

	xml_start_tag(data, el, attr);
}

struct EmulLibEntry xml_start_tag_trap =
{
	TRAP_LIB, 0, (void (*)(void)) xml_start_tag_gate
};

void xml_end_tag(void *, const char *);

static void xml_end_tag_gate(void)
{
	void *data = (void *)((ULONG *)REG_A7)[1];
	char *el   = (char *)((ULONG *)REG_A7)[2];

	xml_end_tag(data, el);
}

struct EmulLibEntry xml_end_tag_trap =
{
	TRAP_LIB, 0, (void (*)(void)) xml_end_tag_gate
};

void xml_char_data(void *, const XML_Char *, int);

static void xml_char_data_gate(void)
{
	void *data  = (void *)    ((ULONG *)REG_A7)[1];
	XML_Char *s = (XML_Char *)((ULONG *)REG_A7)[2];
	int len     = (int)       ((ULONG *)REG_A7)[3];

	xml_char_data(data, s, len);
}

struct EmulLibEntry xml_char_data_trap =
{
	TRAP_LIB, 0, (void (*)(void)) xml_char_data_gate
};
