.text
.extern a,b,c
.global f
HALT
PUSH *1233
POP f
CALL a
PUSH *90
ADD b,c
PUSH r1[c]
PUSH r1[f]
PUSH 23
JMP pc[k]
JMP pc[f]
k: .skip 2, 2
.data
f:
.word 6
.skip 4,8