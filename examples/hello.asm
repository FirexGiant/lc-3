	.orig $3000
	lea r5 hello
	lea r6, hello
	lea r7, hello ; end of line comment
	puts
	HALT
hello:	.STRINGZ "Hello, World"
	.END
