BROKEN_SPOOLER_FLAGS = 0
GHOSTSCRIPT = gs

default.res = 240
default.cpi = 10
default.lpi = 6

default.desc : .USE font/$$(<:/.*\.\(.*\)\..*/dev\1)/DESC.proto ($$(<:/.*\.\(.*\)\..*/\1).generator)
	{
	sed -e "s/^res .*/res $($(<:/.*\.\(.*\)\..*/\1).res)/" \
	    -e "s/^hor .*/hor $($(<:/.*\.\(.*\)\..*/\1).hor)/" \
	    -e "s/^vert .*/vert $($(<:/.*\.\(.*\)\..*/\1).vert)/" \
	    -e "s/^fonts .*/fonts $($(<:/.*\.\(.*\)\..*/\1).fonts:O) $($(<:/.*\.\(.*\)\..*/\1).fonts)/" \
	    $(*)
	[[ "$($(<:/.*\.\(.*\)\..*/\1).generator)" ]] && echo "image_generator $($(<:/.*\.\(.*\)\..*/\1).generator)"
	} > 1.$(tmp).$(<:/.*\.//)
	if	cmp -s 1.$(tmp).$(<:/.*\.//) $(<)
	then	rm 1.$(tmp).$(<:/.*\.//)
	else	mv 1.$(tmp).$(<:/.*\.//) $(<)
	fi

default.proto : .USE font/$$(<:/.*\.\(.*\)\..*/dev\1)/R.proto
	sed -e "s/^name [A-Z]*$/name $(<:/.*\.//)/" \
	    -e "s/^\\([^	]*\\)	[0-9]+	/\\1	$($(<:/.*\.\(.*\)\..*/\1).hor)	/" \
	    -e "s/^spacewidth [0-9]+$/spacewidth $($(<:/.*\.\(.*\)\..*/\1).hor)/" \
	    -e "s/^internalname .*/internalname $(<:/.*\.//)/" \
	    -e "/^internalname/s/CR/4/" \
	    -e "/^internalname/s/BI/3/" \
	    -e "/^internalname/s/B/2/" \
	    -e "/^internalname/s/I/1/" \
	    -e "/^internalname .*[^ 0-9]/d" \
	    $(*) > 1.$(tmp).$(<:/.*\.//)
	if	cmp -s 1.$(tmp).$(<:/.*\.//) $(<)
	then	rm 1.$(tmp).$(<:/.*\.//)
	else	mv 1.$(tmp).$(<:/.*\.//) $(<)
	fi

AFMTODIT =	afmtodit
AFMTODITFLAGS =	-c
AFM_E =		-e $(*:N=*.enc)
AFM_I =		-i 50
AFM_N =		-n
AFM_R =		-i 0 -m
AFM_S =		-s

afm.AR =	AvantGarde-Book			avangbk		textmap		$(AFM_E) $(AFM_R)
afm.AI =	AvantGarde-BookOblique		avangbko	textmap		$(AFM_E) $(AFM_I)
afm.AB =	AvantGarde-Demi			avangd		textmap		$(AFM_E) $(AFM_R)
afm.ABI =	AvantGarde-DemiOblique		avangdo		textmap		$(AFM_E) $(AFM_I)
afm.BMB =	Bookman-Demi			bookmd		textmap		$(AFM_E) $(AFM_R)
afm.BMBI =	Bookman-DemiItalic		bookmdi		textmap		$(AFM_E) $(AFM_I)
afm.BMR =	Bookman-Light			bookml		textmap		$(AFM_E) $(AFM_R)
afm.BMI =	Bookman-LightItalic		bookmli		textmap		$(AFM_E) $(AFM_I)
afm.CR =	Courier				couri		textmap		$(AFM_E) $(AFM_R) $(AFM_N)
afm.CB =	Courier-Bold			courib		textmap		$(AFM_E) $(AFM_R) $(AFM_N)
afm.CBI =	Courier-BoldOblique		couribo		textmap		$(AFM_E) $(AFM_I) $(AFM_N)
afm.CI =	Courier-Oblique			courio		textmap		$(AFM_E) $(AFM_I) $(AFM_N)
afm.EURO =	Euro				freeeuro	symbol		$(AFM_R)
afm.HR =	Helvetica			helve		textmap		$(AFM_E) $(AFM_R)
afm.HB =	Helvetica-Bold			helveb		textmap		$(AFM_E) $(AFM_R)
afm.HBI =	Helvetica-BoldOblique		helvebo		textmap		$(AFM_E) $(AFM_I)
afm.HNR =	Helvetica-Narrow		helven		textmap		$(AFM_E) $(AFM_R)
afm.HNB =	Helvetica-Narrow-Bold		helvenb		textmap		$(AFM_E) $(AFM_R)
afm.HNBI =	Helvetica-Narrow-BoldOblique	helvenbo	textmap		$(AFM_E) $(AFM_I)
afm.HNI =	Helvetica-Narrow-Oblique	helveno		textmap		$(AFM_E) $(AFM_I)
afm.HI =	Helvetica-Oblique		helveo		textmap		$(AFM_E) $(AFM_I)
afm.NB =	NewCenturySchlbk-Bold		newcsb		textmap		$(AFM_E) $(AFM_R)
afm.NBI =	NewCenturySchlbk-BoldItalic	newcsbi		textmap		$(AFM_E) $(AFM_I)
afm.NI =	NewCenturySchlbk-Italic		newcsi		textmap		$(AFM_E) $(AFM_I)
afm.NR =	NewCenturySchlbk-Roman		newcsr		textmap		$(AFM_E) $(AFM_R)
afm.PB =	Palatino-Bold			palatb		textmap		$(AFM_E) $(AFM_R)
afm.PBI =	Palatino-BoldItalic		palatbi		textmap		$(AFM_E) $(AFM_I)
afm.PI =	Palatino-Italic			palati		textmap		$(AFM_E) $(AFM_I)
afm.PR =	Palatino-Roman			palatr		textmap		$(AFM_E) $(AFM_R)
afm.S =		Symbol				symbol		lgreekmap	$(AFM_S) $(AFM_I)
afm.SS =	Symbol				symbolsl	lgreekmap	$(AFM_S) $(AFM_I)
afm.TB =	Times-Bold			timesb		textmap		$(AFM_E) $(AFM_R)
afm.TBI =	Times-BoldItalic		timesbi		textmap		$(AFM_E) $(AFM_I)
afm.TI =	Times-Italic			timesi		textmap		$(AFM_E) $(AFM_I) -a 7
afm.TR =	Times-Roman			timesr		textmap		$(AFM_E) $(AFM_R)
afm.ZCMI =	ZapfChancery-MediumItalic	zapfcmi		textmap		$(AFM_E) $(AFM_I)
afm.ZD =	ZapfDingbats			zapfd		dingbats.map	$(AFM_S) $(AFM_R)
afm.ZDR =	ZapfDingbats-Roman		zapfdr		dingbats.rmap	$(AFM_S) $(AFM_R)

font.dvi.DESC : font/devdvi/DESC.in
	{
	cat $(*)
	echo "papersize $(PAGE|"letter":F=%(lower)s)"
	$(DVIPRINT:+echo print "$(DVIPRINT)")
	} > $(<)

font.lbp.DESC : font/devlbp/DESC.in
	{
	cat $(*)
	echo "papersize $(PAGE|"letter":F=%(lower)s)"
	$(LBPPRINT:+echo print "$(LBPPRINT)")
	} > $(<)

.SOURCE.%.afm : font/devps/afm font/devps/generate font/devps
.SOURCE.%map : font/devps/generate font/devps

font.ps.DESC : font/devps/DESC.in
	{
	cat $(*)
	echo broken $(BROKEN_SPOOLER_FLAGS)
	if	[[ "$(PAGE)" == A4 ]]
	then	echo "papersize a4"
	else	echo "papersize letter"
	fi
	if	[[ "$(PSPRINT)" ]]
	then	echo print '$(PSPRINT)'
	fi
	} > $(<)

ps.prereqs : .FUNCTION
	local F R I
	F := $(<<:/.*\.//)
	local ( N A M O ... ) $(afm.$(F))
	I = $(N).afm
	if ! ( I = "$(I:T=F)" )
		I = $(A).afm
	end
	return font.ps.DESC $(I) $(M)

ps.proto : .USE .DONTCARE $$(ps.prereqs)
	F=$(<:/.*\.//)
	set -- $(afm.$(<:/.*\.//))
	shift 3
	$(AFMTODIT) $(AFMTODITFLAGS) $* -d $(*) 1.$(tmp).$F
	if	cmp -s 1.$(tmp).$F $(<)
	then	rm 1.$(tmp).$F
	else	mv 1.$(tmp).$F $(<)
	fi

LJ4RES =	1200
LJ4PRINT =	$(PSPRINT)

font.lj4.DESC : font/devlj4/DESC.in
	{
	echo "res $(LJ4RES)"
	echo "unitwidth $$(( 7620000 / $(LJ4RES) ))"
	cat $(*)
	echo "papersize $(PAGE|"letter":F=%(lower)s)"
	$(LJ4PRINT:+echo print "$(LJ4PRINT)")
	} > $(<)

symbolmap : font/devps/textmap font/devps/symbolchars
	{
	echo '#'
	echo '# This is a list of all predefined groff symbols.'
	echo '#'
	cat $(*)
	} > $(<)

generate = ascii cp1047 html latin1 utf8

html.generator = $(GHOSTSCRIPT)

":FONT:" : .MAKE .OPERATOR
	local dev=$(*:O=1) DEV=dev$(*:O=1) I D
	if "$(generate:N=$(dev))"
		if ! $(dev).res
			$(dev).res := $(default.res)
		end
		if ! $(dev).cpi
			$(dev).cpi := $(default.cpi)
		end
		if ! $(dev).lpi
			$(dev).lpi := $(default.lpi)
		end
		let $(dev).hor = $(dev).res / $(dev).cpi
		let $(dev).vert = $(dev).res / $(dev).lpi
		$(dev).fonts := $(*:O>1:N=+([[:upper:]]))
		if ! "$($(dev).DESC:A=.TARGET)"
			if "$(@$(dev).desc:V)"
				font.$(dev).DESC : $(dev).desc
			else
				font.$(dev).DESC : default.desc
			end
		end
		$(LIBDIR)/$(ID)/font/$(DEV)/DESC :INSTALL: font.$(dev).DESC
		if "$(@$(dev).proto:V)"
			$(*:O>1:/.*/font.$(dev).&/) : $(dev).proto
		else
			$(*:O>1:/.*/font.$(dev).&/) : default.proto
		end
		for I $($(dev).fonts)
			$(LIBDIR)/$(ID)/font/$(DEV)/$(I) :INSTALL: font.$(dev).$(I)
		end
	else
		I := font.$(dev).DESC
		if ! "$(I:A=.TARGET)"
			I = font/$(DEV)/DESC
		end
		$(LIBDIR)/$(ID)/font/$(DEV) :INSTALLDIR: $(*:O>1:D=font/$(DEV):B:S)
		$(LIBDIR)/$(ID)/font/$(DEV)/DESC :INSTALL: $(I)
	end
