":YACCPLUSPLUS:" : .MAKE .OPERATOR

.y.h.c := $(@%.y>%.c:V)

%.c %.h : %.y .CLEAR

%.cpp %.h : %.y .YACC.SEMAPHORE (YACC) (YACCFLAGS)
	$(.y.h.c)
	mv $(%).c $(%).cpp
