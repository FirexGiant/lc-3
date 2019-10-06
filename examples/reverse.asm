;; reverse a string
	.ORIG	$3000
rev:	LEA	%0	FILE
	ADD	%1	%0	#-1
LOOP1:	LDR	%3	%1	#1
	BRz	DONE1
	ADD	%1	%1	#1
	BR	LOOP1

DONE1:	NOT	%2	%0
	ADD	%2	%2	%1

LOOP2:	ADD	%2	%2	#0
	BRn	DONE2
	LDR	%3	%0	#0
	LDR	%4	%1	#0
	STR	%4	%0	#0
	STR	%3	%1	#0
	ADD	%0	%0	#1
	ADD	%1	%1	#-1
	ADD	%2	%2	#-2
DONE2:	HALT
FILE:	.STRINGZ "This is so much fun!"
	.END
