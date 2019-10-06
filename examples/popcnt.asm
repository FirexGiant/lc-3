;; set r0 to the number of bits "on" in r1
	.ORIG	$3000
	LD	%1	value
pop:	AND	%0	%0	#0
	ADD	%1	%1	#0
	BRzp	skipf
	ADD	%0	%0	#1
skipf:	AND	%2	%2	#0
	ADD	%2	%2	#15
loop:	ADD	%1	%0	%1
	BRzp	skip
	ADD	%0	%0	#1
skip:	ADD	%2	%2	#-1
	BRp	loop
	HALT
value:	.FILL	#29
	.STRINGZ "This is a quote inside\" another string"
	.END
these are some values
extra lines that should
	ADD
