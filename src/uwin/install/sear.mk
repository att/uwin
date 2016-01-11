/*
 * uwin installation self extracting archive support
 */

CC =		ncc
START =		instsear install.sh
MKSEAR =	VPATH=$(VPATH) mksear
MKSEARDEFAULT:=	--ancestor=3
MKSEARFLAGS =	--icon=$(MKSEARICO:T=F)
MKSEARICO =	uwin-sear$(sh uname -i:C,/.*,,).ico

SPACE ==	30Mib
VARIANTS ==	W7/VI/XP/2K/NT

set		select=P=S

:INSTALL:

.SOURCE :	../doc $(INCLUDEDIR)/uwin

(MKSEAR) :	$$(MKSEAR)

SEARS =

EXE =

.SEAR.REL.VER =

":sear:" : .MAKE .OPERATOR
	local ADD INI SRC TXT REL VER
	if "$(<:O=2:N!=-)"
		$(<:O=2) : $(<:O=1)$(EXE)
	end
	SEARS += $(<:O=1)
	if EXE
	$(<:O=1) : $$(<:O=1)$(EXE)
	end
	if "$(>:N=--start=*)"
		ADD += $(>:N=--start=*:/--start=//)
	else
		ADD += $(START)
		INI := --start="$(START)"
	end
	if ! "$(>:N=--release=*)"
		if ! RELEASE
			if ! .SEAR.REL.VER
				.SEAR.REL.VER := $(sh if [[ -f $INSTALLROOT/sys/posix.dll ]]; then cd $INSTALLROOT/sys; [[ -x uname ]] || cp /bin/uname .; ./uname -rv; else uname -rv; fi)
			end
			RELEASE := $(.SEAR.REL.VER:O=1:/\/.*//)
		end
		REL := --release=$(RELEASE)
	end
	if ! "$(>:N=--version=*)"
		if ! VERSION
			if ! .SEAR.REL.VER
				.SEAR.REL.VER := $(sh if [[ -f $INSTALLROOT/sys/posix.dll ]]; then cd $INSTALLROOT/sys; [[ -x uname ]] || cp /bin/uname .; ./uname -rv; else uname -rv; fi)
			end
			VERSION := $(.SEAR.REL.VER:O=2:/\/.*//)
		end
		VER := --version=$(VERSION)
	end
	if SRC = "$(>:N!=--*:A!=.TARGET)"
		:: $(SRC)
	end
	if TXT = "$(<:O>2)"
		TXT := --description=$(TXT)
	else
		TXT := --description=$(<:O=1:/./&/U)
	end
	eval
	$$(<:O=1) : (MKSEAR) (MKSEARFLAGS) $$(>:N=--warn:/^/MKSEARFLAGS+=/) $$(<:O=1).files $$(>:N!=--*) $$(ADD)
		$$(MKSEAR) $$(MKSEARDEFAULT) $$(MKSEARFLAGS) $(REL:V) $(VER:V) $(INI) $(TXT) $(>:N=--*) $$(*)
	end
	$(>:N=*.files:B) : # mark these not standalone
	all : $(<:O=1)$(EXE)

install:

sears: $$(~.ARGS|SEARS:B)
	s=../../lib/libposix
	f=""
	for i in $(*)
	do	mv $i uwin-$i.$(VERSION).$(HOSTTYPE).exe
		f="$f uwin-$i.$(VERSION).$(HOSTTYPE).exe"
	done
	cd $s
	$(MAKE) $(-) tgz
	cd ~-
	cp $s/libposix.tgz uwin-posix.$(VERSION).tgz
	f="$f uwin-posix.$(VERSION).tgz"
	cp $INSTALLROOT/sys/posix.dll uwin-posix.$(VERSION).$(HOSTTYPE).dll
	f="$f uwin-posix.$(VERSION).$(HOSTTYPE).dll"
	for i in $f
	do	o=${i%.@(dll|exe|tgz)}.md5
		md5sum < $i > $o
		f="$f $o"
	done
	$(PAX) -wvf $(<).pax $f

/*
 * the power and the hackery
 */

%-update : %.files (MKSEAR) (MKSEARFLAGS) $$(<:/-[^-]*//).official $$(~$$(<:/-[^-]*//)$(EXE):N=*.files)
	$(MKSEAR) $(MKSEARDEFAULT) $(MKSEARFLAGS) --update=$(*:N=*.official:@Q) --start=$(START) --release=$(RELEASE) --description=$(<:/-/ + /:/./&/U:@/+ Update$/update/:@Q) $(*:N!=?(*/)$(%)?(.official))

% : %.files (MKSEAR) (MKSEARFLAGS) $$(<:N=*-*:+$$(~$$(<:/-[^-]*//)$(EXE)):N!=$$(<:/-[^-]*//).files)
	$(<:N=*-*:?$$(@$$(<:/-[^-]*//)$(EXE):C%\<$$(:/-[^-]*//)\.files\>%$$(>:O=1)%:/^--description="*\(.*\)"*/--description="\1 + $$(<:/.*-//:/./&/U)"/)?$$(MKSEAR) $$(MKSEARDEFAULT) $$(MKSEARFLAGS) --start=$$(START) --release=$$(RELEASE) --description=$$(<:/-/ + /:/./&/U:@Q) $$(*)?)
