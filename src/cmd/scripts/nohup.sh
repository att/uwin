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
# k-shell version of nohup command
case $(getopts '[-]' opt --???man 2>&1) in
version=[0-9]*)
    usage=$'[-?@(#)nohup (AT&T Labs Research) 1999-05-20\n]
	'$USAGE_LICENSE$'
	[+NAME?nohup - invoke a utility immune to hangups]
	[+DESCRIPTION?\bnohup\b invokes the utility named by \acommand\a
		with arguments supplied as \aarg\a.  At the time that
		\acommand\a is invoked, the SIGHUP signal will be set
		to ignored.]
	[+?If the standard output is a terminal, standard output for
		\acommand\a will be appended to a file named \bnohup.out\b 
		in the current directory.  If \bnohup.out\b cannot
		be opened for appending or created, standard output
		will be appended to the \bnohup.out\b file in the directory
		specified by the \bHOME\b environment variable.  If this
		file cannot be opened for appending or created, \bnohup\b
		will fail.]
	[+?If standard error is a terminal, the standard error for
		\acommand\a will be redirected to the same file descriptor
		as standard output.]
	[+?If a file is created, the permission bits will be set to
		S_IRUSR|S_IWUSR.]
	[+?\bnohup\b does not run \acommand\a in the background.  You must
		do that explicitly, by ending the command line with an \b&\b.]

        command [arg ...]

	[+EXIT STATUS?If successful, the exit status of \bnohup\b will
		be that of \acommand\a.  Otherwise, it will be one of
		the following:]{
	        [+126?\acommand\a was found but could not be invoked.]
	        [+127?An error occurred in \bnohup\b or \acommand\a could
			not be found.]
	}
	[+SEE ALSO?disown(1)]
    '
    ;;
*)
    usage=' command [arg ...]'
    ;;
esac

err=
while	getopts "$usage" opt
do	[[ $opt == '?' ]] && err=1
done
[[ $err ]] && exit 1
shift $((OPTIND-1))
trap '' HUP		# ignore hangup
command=$(whence "$1")
oldmask=$(umask)
umask u=rw,og=
exec 0< /dev/null	# disconnect input
if	[[ -t 1 ]]	# redirect output if necessary
then	if	[[ -w . ]]
	then	print 'Sending output to nohup.out'
		exec >> nohup.out
	else	print "Sending output to $HOME/nohup.out"
		exec >> $HOME/nohup.out
	fi
fi
umask "$oldmask"
if	[[ -t 2 ]]	# direct unit 2 to a file
then	exec 2>&1
fi
# run the command
case $command in
*/*)	exec "$@"
	;;
time)	eval "$@"
	;;
*)	"$@"
	;;
esac
