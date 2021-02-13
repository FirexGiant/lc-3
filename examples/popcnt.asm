;; set r0 to the number of bits "on" in r1
	.ORIG	$3000
	LD	r1	value
pop:	AND	r0	r0	#0
	ADD	r1	r1	#0
	BRzp	skipf
	ADD	r0	r0	#1
skipf:	AND	r2	r2	#0
	ADD	r2	r2	#15
loop:	ADD	r1	r0	r1
	BRzp	skip
	ADD	r0	r0	#1
skip:	ADD	r2	r2	#-1
	BRp	loop
	HALT
value:	.FILL	#29
	.STRINGZ "This is a quote inside\" another string"
	.END
these are some values
extra lines that should
	ADD
