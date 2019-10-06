;; set r0 to 10*r1
	.ORIG $3000
mul10:	ADD	%0	%1	%1
	ADD	%0	%0	%0
	ADD	%0	%0	%1
	ADD	%0	%0	%0
	HALT
