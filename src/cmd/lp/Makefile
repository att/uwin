:PACKAGE: ast

FAKE_PRINTER == 
NO_OPTIONS == 1

lp :: lp.c get_default_printer.c slashback.c -lwinspool

cancel :: cancel.c enum_printers.c slashback.c -lwinspool

lpstat :: lpstat.c get_default_printer.c enum_printers.c slashback.c -lwinspool

options.c : gen_options.sh options.tab
	$(*:P=A) -c > $(<)

options_help.c : gen_options.sh options.tab
	$(*:P=A) -h > $(<)
