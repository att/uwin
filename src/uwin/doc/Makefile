:PACKAGE: - uwin

DOCDIR = $(INSTALLROOT)/doc

all :

$(DOCDIR) :INSTALLDIR: credits gnu_license.txt ucb_license.txt \
	license.txt unin.ico \
	uwin.ico uwin32.ico uwin64.ico \
	uwin-sear32.ico uwin-sear64.ico \
	uwin-sear-i32.ico uwin-sear-i64.ico \
	uwin-sear-rt32.ico uwin-sear-rt64.ico \
	config.guess config.sub

$(DOCDIR) :INSTALLDIR: applet.html debugging.html \
	dlls.html iffe.html links.html nmake.html problems.html reg.html \
	tape.html whatis.html telnet.html telnet.gif  \
	map.html compilers.html xbase.html

$(DOCDIR) :INSTALLDIR: devset.html dlls.html iffe.html links.html \
	nmake.html reg.html whatis.html seattle.pdf talk.pdf winnt.pdf \
	winnt3.pdf index.html

$(DOCDIR) :INSTALLDIR: uwin.html uwin.faq.html faq.dev.html faq.general.html \
	faq.installation.html faq.usage.html faq.x11.html

$(DOCDIR) :INSTALLDIR: socket.html utilities.html xopen.html

$(DOCDIR)/images :INSTALLDIR: images/uwin.gif images/uwinback.gif	

LICENSETEXT = $(.FIND. lib/package/LICENSES - $(LICENSECLASS:B))

license.txt :COPY: $$(LICENSETEXT)

%.html : %.mm 
	$(MM2HTML) $(MM2HTMLFLAGS) < $(>) > $(<) 

%.html : %.tab gentab
	$(*:O>1) $(>) > $(<) 
