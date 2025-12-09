.text
	li a7 7
	ecall
	li a7 5
	ecall
	
	li t0 1
	fcvt.d.w ft0 t0 #res
	fcvt.d.w fa7 t0
	fcvt.d.w fa2 zero #counter
	fcvt.d.w  fa3 t0
	li t0 -1
	fcvt.d.w fa5 t0
	li t0 100
	fcvt.d.w fa6 t0
	fmv.d fa1 ft0
	fmv.d ft5 fa0 
loop_: 	
	fadd.d fa2 fa2 fa3 #counter++
	fmul.d fa0 fa0 ft5 #x^n 
	fmul.d fa1 fa1 fa2 #factorial
	fadd.d fa2 fa2 fa3 #counter++
	fmul.d fa1 fa1 fa2 #factorial		
	fdiv.d fa4 fa0 fa1 #x^n/n!
	fmul.d fa7 fa7 fa5 #sign 
	fmul.d fa4 fa4 fa7 #local result (-1)^n*x^n/n!
	fadd.d ft0 ft0 fa4 #result +=local result
	feq.d t0 fa6 fa2
	fmul.d fa0 fa0 ft5
	bne t0 zero round_call 
	j loop_
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
	
round_call:
	fmv.d fa0 ft0
	jal round
	li a7 3
	ecall
	li a7 10
	ecall