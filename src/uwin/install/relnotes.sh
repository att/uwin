cat -D << EOF
1.  UWIN ${RELEASE} requires at least ${SPACE} of disk space. 

2.  UWIN runs best on NTFS (Windows ${VARIANTS}), but will run
    in a degraded mode on FAT file systems.

3.  If you are installing UWIN on Windows ${VARIANTS} you will
    need Administrator's privileges in order to install the UWIN
    Services.  These services provide setuid and telnet capabilities.

4.  After installation, please read the UWIN Overview in the UWIN
    Program Group.

5.  The UWIN Program Group has a shortcut to the UWIN configuration
    applet that manages the UWIN Master Service (ums), inetd managed
    servers, UWIN resource limits, and UWIN releases.
EOF
