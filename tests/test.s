.equ a, 76
.equ b, 3378

.section .text

.extern printf

movw r3, b
call $printf
call $printf