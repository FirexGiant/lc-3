	.ORIG	$3000
cint:	LD	%3	b15
	LD	%1	a
	LD	%2	b
	AND	%3 	%3	%1
	ADD	%3	%3	%2
	BRzp	cmp

retA:	ST	%1	r
	BR	leave

cmp:	NOT	%2	%2
	ADD	%2	%2	#1
	ADD	%2	%1	%2
	ST	%2	r

leave:	HALT

b15:	.FILL	$8000
a:	.FILL	#-20000
b:	.FILL	#20000
r:	.BLKW	#1
	.END
