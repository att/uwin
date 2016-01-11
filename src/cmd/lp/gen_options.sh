#!/bin/ksh

gen=code

for i
do
	case $i in
	-h)	gen=help ;;
	-c)	gen=code ;;
	esac
done

exec 3<${1:-options.tab}

while read -u3
do
	case "$REPLY" in
	"{") 	while read -u3
		do
			[[ "$REPLY" == "}" ]] && break
			[[ "$gen" == code ]] && print -n "{ PRN_OPT_VALUE, "
			if [[ "$REPLY" == "*" ]]; then
				if [[ "$gen" == code ]]; then
					print "\"*\", { 0 } },"
				else
					print "\"[+?][+?Any other integer \\\\avalue\\\\a is also accepted.]\""
				fi
			else
				set -- $REPLY
				typeset -l value=${1#*_}
				if [[ "$gen" == code ]]; then
					print "\"$value\", { $1 } },"
				else
					shift 1
					print "\"[+$value?$@]\""
				fi
			fi
		done
		noval=
		[[ "$gen" != code ]] && print "\"}[+?]\""
		continue ;;
	short*)	[[ "$gen" == code ]] && print -n "{ PRN_OPT_SHORT, " ;;
	DWORD*)	[[ "$gen" == code ]] && print -n "{ PRN_OPT_LONG, " ;;
	'')	continue ;;
	*)	print " error: $REPLY " ; exit 1 ;;
	esac

	set -- $REPLY
	shift 1
	typeset -l name="$1"
	typeset -u bit=DM_"$1"
	if [[ "$gen" == code ]]; then
		print -n "\"$name\", "
		print "{ { offsetof(DEVMODE, dm$1), $bit } } },"
	else
		shift 1
		[[ -n "$noval" ]] && print "\"[+?An integer \\\\avalue\\\\a is accepted.]}[+?]\""
		print "\"[+$name?$@]{\""
		noval=1
	fi
done
