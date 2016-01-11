:PACKAGE:		- uwin:license

$(ETCDIR)		:INSTALLDIR: traceit inetdconfig.sh stop_uwin group.add passwd.add rc profile mkman mkadmin restart_uwin
$(ETCDIR)/rc.d		:INSTALLDIR: at sshd XWin
$(MANDIR)1		:INSTALLDIR: doclist
$(LIBDIR)		:INSTALLDIR: tips mksymtab ld_r mkdeffile mkexec.c
$(LIBDIR)/msvcrt	:INSTALLDIR: msvcrt.tab
$(INSTALLROOT)/X11/bin	:INSTALLDIR: Xrun
.			:INSTALLDIR: webbrowser

ar::		ar.sh

adduser::	adduser.sh

env::		env.sh

groups::	groups.sh

clear::		clear.sh

nohup::		nohup.sh

dircmp::	dircmp.sh

true::		true.sh

false::		false.sh

ffind::		ffind.sh

command::	command.sh

line::		line.sh

inetdconfig.sh::inetdconfig.sh

cd::		cd.sh

chsh::		chsh.sh

getopts:LINK:	command

kill:LINK:	command 

ld_r::		ld_r.sh

logstat::	logstat.sh

lorder::	lorder.sh LICENSE=author=bl

man::		man.sh LICENSE=author=dgk+gsf

exception::	exception.sh LICENSE=author=dgk+gsf

mkadmin::	mkadmin.sh LICENSE=author=gsf

mkimplib::	mkimplib.sh

mkdeffile::	mkdeffile.sh

mkexec::	mkexec.sh

mkmsvcrtlib::	mkmsvcrtlib.sh LICENSE=author=gsf

mksymtab::	mksymtab.sh

mkman::		mkman.sh LICENSE=author=gsf

Xrun::		Xrun.sh

"print":LINK:	command 

printf:LINK:	command

pwd:LINK:	command

ranlib:LINK:	true 

"read":LINK:	command 

restart_uwin::	restart_uwin.sh LICENSE=since=2010,author=dgk

sleep:LINK:	command 

stdinstall::	stdinstall.sh LICENSE=author=gsf

stop_uwin::	stop_uwin.sh

strip:LINK:	true

test:LINK:	command

tipofday::	tipofday.sh tips

traceit::	traceit.sh

umask:LINK:	command 

vc::		vc.sh LICENSE=author=gsf

wait:LINK:	command 

which::		which.sh

$(BINDIR)/install :INSTALL: uwin_install

uwin_install::	install.sh

webbrowser::	webbrowser.sh LICENSE=author=dgk+kf

:: RELEASE
