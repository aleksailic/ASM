.data
	.skip 4
test:	.word 6548
.section ".rodata"
msg:
	.byte 5,6
.text
	.global main
main:
	push	msg
        movw	bp, sp
	call	getchar
	call	getchar
	cmp 	ax, 'A'
	jne	$skip
	push	bp
	call	$printf
	add	sp, 4
skip:	mov	ax, 0
	mov	sp, bp
	pop	bp
	ret