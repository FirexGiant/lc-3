	.orig $3000
	lea r0, hello ; end of line comment
	puts
	HALT
hello:	.STRINGZ "Hello,World" ; TODO handle spaces
	.END
