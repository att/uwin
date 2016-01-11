:PACKAGE: - uwin ast:20100309

:ALL:

.no.local : .BEFORE .VIRTUAL .FORCE
	$(RM) $(RMFLAGS) cc

cc :: .no.local cc.c -lcmd -ladvapi32

c89 ld ncc nld CC^1 :LINK: cc
