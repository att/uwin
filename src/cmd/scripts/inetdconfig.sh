#!/bin/sh
########################################################################
#                                                                      #
#              This software is part of the uwin package               #
#          Copyright (c) 1996-2011 AT&T Intellectual Property          #
#                      and is licensed under the                       #
#                 Eclipse Public License, Version 1.0                  #
#                    by AT&T Intellectual Property                     #
#                                                                      #
#                A copy of the License is available at                 #
#          http://www.eclipse.org/org/documents/epl-v10.html           #
#         (with md5 checksum b35adb5213ca9657e911e9befb180842)         #
#                                                                      #
#              Information and Software Systems Research               #
#                            AT&T Research                             #
#                           Florham Park NJ                            #
#                                                                      #
#                  David Korn <dgk@research.att.com>                   #
#                 Glenn Fowler <gsf@research.att.com>                  #
#                                                                      #
########################################################################
scriptfile=/tmp/script$$
trap 'rm -f /tmp/script$$' EXIT

cat > $scriptfile << '+++EOF+++'
#! /usr/bin/ksh
FTP_DISABLE=
RSH_DISABLE=
TELNET_DISABLE=
RLOGIN_DISABLE=
integer FTP_PORT=21
integer TELNET_PORT=23
integer RLOGIN_PORT=513
integer RSH_PORT=514
. uwin_keys
rel=$(<"$REG/.../Release")
if	[[ $rel && -f $REG/$rel/InetConfig ]]
then	. "$REG/$rel/InetConfig"
fi
+++EOF+++

print -- 'cat > /tmp/services.new << +++EOF+++' >> $scriptfile
sed -e '
/^ftp .*tcp/s/[0-9]*\/tcp/\${FTP_PORT-21}\/tcp/
/^telnet .*tcp/s/[0-9]*\/tcp/\${TELNET_PORT-23}\/tcp/
/^login .*tcp/s/[0-9]*\/tcp/\${RLOGIN_PORT-513}\/tcp/
/^shell .*tcp/ s/[0-9]*\/tcp/\${RSH_PORT-514}\/tcp/
' /etc/services >> $scriptfile

print -- '+++EOF+++' >> $scriptfile

print -- 'cat > /etc/inetd.conf.new << +++EOF+++' >> $scriptfile
sed < /etc/inetd.conf >> $scriptfile \
	-e 's/^ftp/\${FTP_DISABLE:+#}ftp/' -e 's/^#ftp/\${FTP_DISABLE:+#}ftp/' \
	-e 's/^telnet/\${TELNET_DISABLE:+#}telnet/' -e 's/^#telnet/\${TELNET_DISABLE:+#}telnet/' \
	-e 's/^shell/\${RSH_DISABLE:+#}shell/' -e 's/^#shell/\${RSH_DISABLE:+#}shell/' \
	-e 's/^login/\${RLOGIN_DISABLE:+#}login/' -e 's/^#login/\${RLOGIN_DISABLE:+#}login/'
print -- '+++EOF+++' >> $scriptfile
cat >> $scriptfile <<- '+++EOF+++'
	mv /etc/inetd.conf /etc/inetd.conf.old
	mv /etc/inetd.conf.new /etc/inetd.conf
	cp /etc/services /etc/services.old
	sed $'s/\r*$/\r/' /tmp/services.new > /etc/services
	rm -f /tmp/services.new
	set -- $(ps -eo pid+8,command | fgrep inetd.exe | cut -c1-8)
	if      [[ $1 ]]
	then    kill -s HUP "$@"
	fi
+++EOF+++
ksh $scriptfile
