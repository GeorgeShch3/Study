.data
space: .asciz " "
.text
	li a7 5
	ecall 
	mv a1 a0	
loop:
	ecall 
	beq a0 zero end_programm
	jal digit
	li a7 1 
	ecall
	li a7 4 
	la a0 space
	ecall
	li a7 5
	j loop
digit:
	li t0 10
	mv t1 zero
	mv t2 zero
	mv t3 zero
loop_1:
	rem t1 a0 t0
	div a0 a0 t0
	addi sp sp -4
	sw t1 (sp)
	addi t2 t2 1
	beq a0 zero revers
	j loop_1
revers:
	blt t2 a1 bad
	mv t3 zero 
loop_2:
	beq t3 a1 end
	lw a0 (sp)
	addi sp sp 4
	addi t3 t3 1
	j loop_2
bad:
	li a0 -1 
end:
	blt  t2 t3 end
	addi sp sp 4
	addi t3 t3 1
	ret
	
end_programm:	
	li a7 1
	mv a0 zero 
	ecall