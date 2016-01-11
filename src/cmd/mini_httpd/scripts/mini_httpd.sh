#!/bin/sh
#
# mini_httpd.sh - startup script for mini_httpd on FreeBSD
#
# This goes in /usr/local/etc/rc.d and gets run at boot-time.

case "$1" in
 
    start)
    if [ -x /usr/local/sbin/mini_httpd_wrapper ] ; then
	echo -n " mini_httpd"
	/usr/local/sbin/mini_httpd_wrapper &
    fi
    ;;

    stop)
    kill -USR1 `cat /var/run/mini_httpd.pid`
    ;;
	    
    *)
    echo "usage: $0 { start | stop }" >&2
    exit 1
    ;;
			     
esac
