#
# Downloads and prepares the dependencies to compile SimpleMail
# using a cross compiler on an Unix system
#

.build-dependencies-done:
	mkdir -p build-dependencies
ifndef AMISSL_INCLUDE
	mkdir -p build-dependencies/amissl
	cd build-dependencies/amissl && wget -N http://www.heightanxiety.com/AmiSSL/AmiSSL-3.5-SDK.lha
	cd build-dependencies/amissl && lha xf AmiSSL-3.5-SDK.lha
endif
ifndef EXPAT_INCLUDE
	mkdir -p build-dependencies/expat
	cd build-dependencies/expat && wget -N http://www.os4depot.net/share/development/library/misc/expat.lha
	cd build-dependencies/expat && lha xf expat.lha && ln -sf libraries/expat.h expat/SDK/Include/include_h/expat.h
endif
ifndef MUI_INCLUDE
	# MUI
	mkdir -p build-dependencies/SDK
	cd build-dependencies/SDK && wget -c "http://www.hyperion-entertainment.biz/index.php?option=com_registration&amp;view=download&amp;format=raw&amp;file=38&amp;Itemid=63"
	cd build-dependencies/SDK && ln -sf "index.php?option=com_registration&amp;view=download&amp;format=raw&amp;file=38&amp;Itemid=63" SDK_53.20.lha 
	cd build-dependencies/SDK && lha xf SDK_53.20.lha && lha xf SDK_Install/MUI-3.9.lha
	cd build-dependencies/SDK && echo "struct MUI_ImageSpec; struct MUI_FrameSpec;" >/tmp/muimaster.h && grep -v muiprog.h MUI/C/Include/interfaces/muimaster.h >>/tmp/muimaster.h && cp /tmp/muimaster.h MUI/C/Include/interfaces/muimaster.h
	# NList
	cd build-dependencies/SDK && wget -c http://os4depot.net/share/library/mui/mcc_nlist.lha
	cd build-dependencies/SDK && lha xf mcc_nlist.lha
	cp -R build-dependencies/SDK/MCC_NList/Developer/C/include/mui build-dependencies/SDK/MUI/C/Include/mui
	# BetterString
	cd build-dependencies/SDK && wget -c http://os4depot.net/share/library/mui/mcc_betterstring.lha
	cd build-dependencies/SDK && lha xf mcc_betterstring.lha
	cp -R build-dependencies/SDK/MCC_BetterString/Developer/C/include/mui build-dependencies/SDK/MUI/C/Include
	# Texteditor
	cd build-dependencies/SDK && wget -c http://os4depot.net/share/library/mui/mcc_texteditor.lha
	cd build-dependencies/SDK && lha xf mcc_texteditor.lha
	cp -R build-dependencies/SDK/MCC_TextEditor/Developer/C/include/mui build-dependencies/SDK/MUI/C/Include
	# The bar
	cd build-dependencies/SDK && wget -c http://os4depot.net/share/library/mui/mcc_thebar.lha
	cd build-dependencies/SDK && lha xf mcc_thebar.lha
	cp -R build-dependencies/SDK/MCC_TheBar/Developer/C/include/mui build-dependencies/SDK/MUI/C/Include
	# Popplaceholder
	cd build-dependencies/SDK && wget -c http://aminet.net/dev/mui/MCC_Popph.lha
	cd build-dependencies/SDK && lha xf MCC_Popph.lha
	cp -R build-dependencies/SDK/MCC_Popph/Developer/C/include/mui build-dependencies/SDK/MUI/C/Include
endif
	# OpenURL
ifndef OPENURL_INCLUDE
	cd build-dependencies && wget -c http://os4depot.net/share/network/misc/openurl.lha
	cd build-dependencies && lha xf openurl.lha
endif
	touch $@

# Now set the variables

ifndef AMISSL_INCLUDE
AMISSL_INCLUDE=build-dependencies/amissl/include/include_h
endif

ifndef NET_INCLUDE
NET_INCLUDE=.
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
