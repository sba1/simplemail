/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

#ifndef SM__SMINTL_H
#define SM__SMINTL_H

#if defined(_AMIGA) || defined(__MORPHOS__) || defined(__AMIGAOS4__)
#define ENABLE_NLS
#define PACKAGE ""
#define LOCALEDIR "PROGDIR:locale"
#endif

#if 0
 #ifdef HAVE_CONFIG_H
 #ifndef FC_CONFIG_H		/* this should be defined in config.h */
 #error Files including fcintl.h should also include config.h directly
 #endif
 #endif
#endif

#ifdef ENABLE_NLS
#include <libintl.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#define _(String) gettext(String)
#define N_(String) String
#define Q_(String) skip_intl_qualifier_prefix(gettext(String))

#else

#define _(String) (String)
#define N_(String) String
#define Q_(String) skip_intl_qualifier_prefix(String)

#define textdomain(Domain)
#define bindtextdomain(Package, Directory)

#endif

char *skip_intl_qualifier_prefix(const char *str);

#endif  /* FC__FCINTL_H */
