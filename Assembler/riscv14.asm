.macro input %msg %reg
.data
msg:	.asciz %msg
.text
	li a7 4
	la a0 msg
	ecall
	li a7 5
	ecall
	mv %reg a0
	
.end_macro

.macro print %msg %reg
.data 
msg: 	.asciz %msg
.text
	li a7 4
	la a0 msg
	ecall
	li a7 1
	mv a0 %reg
	ecall
	li a0 '\n'
	li a7 11
	ecall
.end_macro

