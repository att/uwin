/*
 * uwin package sears
 */

LICENSEFILE =	uwin.lic
LICENSE =	since=1996,author=dgk

BASE = \
	--debug=2 \
	--install --start=phase1.sh instgui \
	uninstall uninstall.bat uninstall.sh \
	release release.sh \
	base-gpl.files gpllic.txt \
	relnotes.txt license.txt error.txt \
	phase1.sh phase2.sh

base		sys			:sear: $(BASE)
dev		sys "Development"	:sear: dev.finish

groff		aux			:sear:
perl		xaux			:sear:
terminfo	aux "Terminfo DB" 	:sear:
xbase		aux "X11 Base"		:sear: --warn
xdev		aux "X11 Development" 	:sear:
xfonts		aux "X11 Fonts"		:sear:

if GRAPHVIZ
graphviz	att "GraphViz"		:sear:
end

save:		sys
		package write source lcl base uwin version=$(version:-$(VERSION))
		package write source base uwin version=$(version:-$(VERSION))

mksear::	mksear.sh LICENSE=author=dgk+gsf,since=1996

instgui::	instgui.c instgui.rc DESCRIPTION="'at&t uwin installer'" \
			-lgdi32 -luser32  -lcomdlg32 \
			-lshell32 -lgdi32 -lcomctl32 -lsetupapi -ladvapi32  \
			LICENSE=author=dgk+ek,since=1996

instsear::	instsear.c DESCRIPTION="'AT&T Research uwin sear installer'" -ladvapi32 -luser32

uninstall::	uninstall.c unin.rc CCFLAGS+=--mt-administrator DESCRIPTION="'AT&T Research uwin uninstaller'" -ladvapi32

test_uwin_keys::test_uwin_keys.c -ladvapi32 -luser32

relnotes::	relnotes.sh

relnotes.txt:	relnotes
		$(*) > $(<)

LICENSETEXT = $(.FIND. lib/package/LICENSES - $(LICENSECLASS:B))

license.txt:	$$(LICENSETEXT)
		sed '1,/^$/d' $(*) | fmt -w60 | cat -D > $(<)

gpllic.txt:	gnu_license.txt
		cat -D $(*) > $(<)

install:	.INSTALL

$(ETCDIR):INSTALLDIR: uwin-base

uwin-base:COPY:	phase1.sh

phase1.sh::	phase1.sh.sh \
		LICENSE==since=1996,author=dgk+gsf \
		LICENSE_domain==$(LICENSEINFO:P=W=query=domain) \
		LICENSE_id==$(LICENSEINFO:P=W=query=${id-${type}}) \
		LICENSE_type==$(LICENSEINFO:P=W=query=type) \
		LICENSE_version==$(LICENSEINFO:P=W=query=version)

$(ETCDIR):INSTALLDIR: uwin-sear

uwin-sear:	install.sh
		$(SED) 's/^\[-1p2/[-1p0/' < $(*) > $(<)
		chmod +x $(<)

install.sh::	install.sh.sh LICENSE=since=1996,author=dgk+gsf

MKMAN = $INSTALLROOT/etc/mkman
MKMANFLAGS =

man:
		[[ -d man ]] || $(MKDIR) man
		$(MKMAN) $(MKMANFLAGS) --verbose --directory=man --log=man.log --uwin
		cd man
		$(PAX) -wf ../man.tgz -x tgz .

lcl:
		package only write source base lcl uwin

:: README RELEASE.txt
