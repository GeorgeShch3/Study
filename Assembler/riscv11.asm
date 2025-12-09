.data
.text
	li a7 7
	ecall
	li a7 5
	ecall
	
	jal round 
	li a7 3
	ecall
	li a7 10
	ecall
round:
	li t0 10
	li t1 1 
	li t3 5
loop:
	beq a0 zero next_step
	mul t1 t1 t0
	addi a0 a0 -1
	j loop
next_step:
	fcvt.d.w ft1 t1 
	fmul.d fa0 fa0 ft1 
	fmv.d ft2 fa0
	fcvt.w.d t0 fa0 rtz 
	fcvt.d.w fa0 t0 
end:	
	fdiv.d fa0 fa0 ft1
	ret 