// Flags for file descriptor 'flags' field
#define FD_CSRC   0x80000000    // C source file
#define FD_HDR    0x40000000    // Header file
#define FD_CXXSRC 0x20000000    // C++ source file
#define FD_ASM    0x10000000    // Assembler file
#define FD_OPTS   0x08000000    // Options file
#define FD_MAKE   0x04000000    // Makefile

// File descriptor structure.  Contains information on the file like filename, file type,
// direct dependancies (files directly #included by this file), as well as linkage
// information to allow FileDesc structures to be linked into a list.
typedef struct FileDesc
{
   struct FileDesc *next;
   struct FileDesc *prev;
   char *name;                 // Name of file
   unsigned long flags;        // See above
   int ndirect;                // Number of direct dependancies
   int adirect;                // Available slots for dd's
   struct FileDesc **Direct;   // Direct dependancies
} FileDesc;

// The FileList structure is basically just a header to allow lists of FileDesc structures
// to be created.
typedef struct FileList
{
   struct FileDesc *head;
   struct FileDesc *tail;
   struct FileDesc *cur;
} FileList;

