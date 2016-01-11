/*
 * generate uwin object map tarballs with line number info
 */

.MAP.ID. = "@(#)$Id: map (at&t Research) 2009-10-15 $"

set --readstate=1

EXCEPTION = exception
EXCEPTIONFLAGS =

MAPLIB = $(.MAIN.TARGET.)
MAPDLL = $(*$(MAPLIB):N=*$(CC.SUFFIX.DYNAMIC))
MAPMAP = $(MAPDLL:D:B:S=.map)
MAPID = $(.sh. objstamp $(MAPDLL:T=F))

for .X. $(.SUFFIX.c) $(.SUFFIX.C)
	%.s : %$(.X.) "$$(%:B:S=$$(CC.SUFFIX.OBJECT):A=.NOOPTIMIZE$(.X.):+.NOOPTIMIZE$(.X.))" (CC) (CCFLAGS)
		$(CC) $(CCFLAGS) -S $(>)
		$(EXCEPTION) $(EXCEPTIONFLAGS) --asm $(<)
end

.MAP.INIT : .MAKE .VIRTUAL .FORCE .AFTER
	.ATTRIBUTE.$(IFFEGENDIR)/% : .CLEAR .TERMINAL
	$(...:N=*.[ch]:N!=.*:A=.TARGET) : .CLEAR .TERMINAL

.MAKEINIT : .MAP.INIT

.MAP.AGAIN =

.MAP : .MAKE .VIRTUAL .FORCE .ONOBJECT .REPEAT .PROBE.INIT
	local A S T U X
	for S $(.SUFFIX.c) $(.SUFFIX.C)
		T += $(.SOURCES.:G=%$(S):B:S=$(CC.SUFFIX.OBJECT))
	end
	if T
		.SOURCE.c : $(IFFESRCDIR)
		for U $(T)
			X =
			for S $(.SUFFIX.c) $(.SUFFIX.C)
				X += $(*$(U):G=%$(S))
			end
			if ( X = "$(X:A!=.LCL.INCLUDE|.STD.INCLUDE)" )
				$(U:B:S=.s) : .IMPLICIT $(X) $(~$(U):A=.STATEVAR)
				A += $(U:B:S=.s)
			end
		end
		$(MAPID) : id addr $(MAPMAP) $(A)
			$(PAX) -wf $(<) -x tgz -s ',.*\.map,map,' $(*)
			touch -t '#'$(<) $(<)
			$(RM) -f current
			$(LN) $(<) current
		$(MAPDLL) : .CLEAR .TERMINAL
		addr : $(MAPDLL)
			$(EXCEPTION) $(EXCEPTIONFLAGS) --disasm $(*) > $(<)
		id : (MAPID)
			print $(MAPID) > $(<)
		.MAKE : $(MAPID)
	elif .MAP.AGAIN
		error 3 map: no source to generate map database
	else
		.MAP.AGAIN = 1
		.ARGS : .INSERT $(<)
		.MAKE : .ALL
	end
