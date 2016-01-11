# UWIN registry and default directory paths #

if	[[ $(uname -i) == 32/64 ]]
then	_WoW_=/64
else	_WoW_=
fi

REG="/reg/local_machine/SOFTWARE/AT&T Labs/UWIN"
SVCREG="$_WoW_/reg/local_machine/SYSTEM/CurrentControlSet/Services"
SYSREG="$_WoW_/reg/local_machine/SOFTWARE/Microsoft/Windows/CurrentVersion"
PRGREG="$SYSREG/Explorer/Shell Folders"

DEFAULT_home="/C/home"
DEFAULT_root="/C/Program Files/UWIN"

typeset -a SYSFILES=(posix.dll ast54.dll uwin.cpl)
typeset -a BINFILES=(ksh pax ast54.dll cmd12.dll dll10.dll shell11.dll uwin_keys)
