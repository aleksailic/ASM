.data
test: .word 6548
.text
	.globl main
main:
	movw [r7][test]