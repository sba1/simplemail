#include "SimpleMail_rev.h"

#ifndef SIMPLEMAIL_DATE
#include "date.h"
#endif

#define VERSTRING 

#undef VSTRING
#undef VERSTAG

#define VSTRING		VERS " (" SIMPLEMAIL_DATE ")\n"
#define VERSTAG		"$VER: " VERS " (" SIMPLEMAIL_DATE ")"
