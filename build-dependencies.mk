#
# Downloads and prepares the dependencies to compile SimpleMail
# using a cross compiler on an Unix system
#


ifdef USE_OPENSSL
OPENSSL_DEPENDENCY=.openssl-done
else
OPENSSL_DEPENDENCY=
endif

.build-dependencies-done: .amissl-done .expat-done .mui-done .openurl-done .netinclude-done $(OPENSSL_DEPENDENCY)
	touch $@

LHA=$(shell pwd)/build-dependencies/lha/lha-1.14i.orig/src/lha

#
# Download and compile lha
#

$(LHA):
	mkdir -p build-dependencies/lha
	cd build-dependencies/lha && wget -N http://archive.debian.org/debian-archive/debian/pool/non-free/l/lha/lha_1.14i.orig.tar.gz
	cd build-dependencies/lha && wget -N http://archive.debian.org/debian-archive/debian/pool/non-free/l/lha/lha_1.14i-10.3.diff.gz
	cd build-dependencies/lha && tar -xzf lha_1.14i.orig.tar.gz
	cd build-dependencies/lha && zcat lha_1.14i-10.3.diff.gz | patch -p0
	cd build-dependencies/lha/lha-1.14i.orig/ && make

.lha-done: $(LHA)
	touch $@

#
# Download and extract AmiSSL includes
#
.amissl-done: $(LHA)
ifndef AMISSL_INCLUDE
	mkdir -p build-dependencies/amissl
	cd build-dependencies/amissl && wget -N http://www.heightanxiety.com/AmiSSL/AmiSSL-3.5-SDK.lha
	cd build-dependencies/amissl && $(LHA) xf AmiSSL-3.5-SDK.lha
endif
	touch $@

.amissl4-done: $(LHA)
ifndef AMISSL4_INCLUDE
	mkdir -p build-dependencies/amissl4
	cd build-dependencies/amissl4 && wget -N https://github.com/jens-maus/amissl/releases/download/4.0/AmiSSL-4.0.lha
	cd build-dependencies/amissl4 && $(LHA) xf AmiSSL-4.0.lha
endif
	touch $@

#
# Compile OpenSSL
#
.openssl-done:
	(cd build-dependencies/interim-openssl && git pull) || \
	    (mkdir -p build-dependencies && cd build-dependencies && git clone https://github.com/sba1/interim-openssl.git)
	$(MAKE) -C build-dependencies/interim-openssl build-clib2-no-read-pw
	touch $@

#
# Download and extract expat includes
#
.expat-done: $(LHA)
ifndef EXPAT_INCLUDE
	mkdir -p build-dependencies/expat
	cd build-dependencies/expat && wget -N http://www.os4depot.net/share/development/library/misc/expat.lha
	cd build-dependencies/expat && $(LHA) xf expat.lha && ln -sf libraries/expat.h expat/SDK/Include/include_h/expat.h
endif
	touch $@

build-dependencies/SDK/SDK_53.20.lha:
	mkdir -p build-dependencies/SDK
	cd build-dependencies/SDK && wget -c "http://www.hyperion-entertainment.biz/index.php?option=com_registration&amp;view=download&amp;format=raw&amp;file=38&amp;Itemid=63" -O SDK_53.20.lha

# Extract netinclude
ifdef NET_INCLUDE
.netinclude-done:
else
.netinclude-done: build-dependencies/SDK/SDK_53.20.lha $(LHA)
	cd build-dependencies/SDK && $(LHA) xf SDK_53.20.lha && lha xf SDK_Install/base.lha
endif
	touch $@

#
# Download and extract MUI includes
#
ifdef MUI_INCLUDE
.mui-done:
else
.mui-done: build-dependencies/SDK/SDK_53.20.lha $(LHA)
	# MUI
	mkdir -p build-dependencies/SDK
	cd build-dependencies/SDK && $(LHA) xf SDK_53.20.lha && lha xf SDK_Install/MUI-3.9.lha
	cd build-dependencies/SDK && echo "struct MUI_ImageSpec; struct MUI_FrameSpec;" >/tmp/muimaster.h && grep -v muiprog.h MUI/C/Include/interfaces/muimaster.h >>/tmp/muimaster.h && cp /tmp/muimaster.h MUI/C/Include/interfaces/muimaster.h
	# NList
	cd build-dependencies/SDK && wget -c http://os4depot.net/share/library/mui/mcc_nlist.lha
	cd build-dependencies/SDK && $(LHA) xf mcc_nlist.lha
	cp -R build-dependencies/SDK/MCC_NList/Developer/C/include/mui build-dependencies/SDK/MUI/C/Include/mui
	# BetterString
	cd build-dependencies/SDK && wget -c http://os4depot.net/share/library/mui/mcc_betterstring.lha
	cd build-dependencies/SDK && $(LHA) xf mcc_betterstring.lha
	cp -R build-dependencies/SDK/MCC_BetterString/Developer/C/include/mui build-dependencies/SDK/MUI/C/Include
	# Texteditor
	cd build-dependencies/SDK && wget -c http://os4depot.net/share/library/mui/mcc_texteditor.lha
	cd build-dependencies/SDK && $(LHA) xf mcc_texteditor.lha
	cp -R build-dependencies/SDK/MCC_TextEditor/Developer/C/include/mui build-dependencies/SDK/MUI/C/Include
	# The bar
	cd build-dependencies/SDK && wget -c http://os4depot.net/share/library/mui/mcc_thebar.lha
	cd build-dependencies/SDK && $(LHA) xf mcc_thebar.lha
	cp -R build-dependencies/SDK/MCC_TheBar/Developer/C/include/mui build-dependencies/SDK/MUI/C/Include
	# Popplaceholder
	cd build-dependencies/SDK && wget -c http://aminet.net/dev/mui/MCC_Popph.lha
	cd build-dependencies/SDK && $(LHA) xf MCC_Popph.lha
	cp -R build-dependencies/SDK/MCC_Popph/Developer/C/include/mui build-dependencies/SDK/MUI/C/Include
endif
	touch $@

#
# Download and extract OpenURL includes
#
.openurl-done: $(LHA)
	# OpenURL
ifndef OPENURL_INCLUDE
	cd build-dependencies && wget -c http://os4depot.net/share/network/misc/openurl.lha
	cd build-dependencies && $(LHA) xf openurl.lha
endif
	touch $@

# Now set the variables

ifndef AMISSL_INCLUDE
AMISSL_INCLUDE=build-dependencies/amissl/include/include_h
endif

ifndef NET_INCLUDE
NET_INCLUDE=build-dependencies/SDK/Include/netinclude
endif

ifndef EXPAT_INCLUDE
EXPAT_INCLUDE=build-dependencies/expat/expat/SDK/Include/include_h
endif

ifndef EXPAT_LIB
EXPAT_LIB=build-dependencies/expat/expat/SDK/Local/clib2/lib/
endif

ifndef MUI_INCLUDE
MUI_INCLUDE=./build-dependencies/SDK/MUI/C/Include/
endif

ifndef SDK_INCLUDE
SDK_INCLUDE=.
endif

ifndef OPENURL_INCLUDE
OPENURL_INCLUDE=./build-dependencies/OpenURL/Developer/C/include
endif
