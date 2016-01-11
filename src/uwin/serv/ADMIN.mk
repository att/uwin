":ADMIN:" : .MAKE .OPERATOR
	$(<) : $(>) (MKADMIN) (MKADMINFLAGS)
		$(MKADMIN) $(MKADMINFLAGS) -o $(<) $(*)
	$(>) : .TARGET
	: $(.INSTALL.COMMON. $$(BINDIR) $(<) - $(>)) :
