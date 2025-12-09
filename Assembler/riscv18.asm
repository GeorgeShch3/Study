.macro strlen %adr
	la t0 %adr
	li t1 '\0'
	li a0 0 
loop:
	lb t2 0(t0) 
	beq t2 t1 end
	addi a0 a0 1
	addi t0 t0 1
	j loop	
end:	
.end_macro

.macro strcpy %res %input
    	la t3 %input
    	la t4 %res
    	mv a1 t4
    	
   	strlen %input
    	mv t5 a0
   	add t3 t3 a0
   	add t4 t4 a0
   	strlen %res
   	mv t0 a1
	mv a1 a0
	mv a0 a1
loop:
    	bltz t5 end
    	lb t6 0(t3)
    	sb t6 0(t4)
    	addi t5 t5 -1
    	addi t3 t3 -1
    	addi t4 t4 -1
    	j loop
end:
.end_macro

.macro strcmp %first %second
	strlen %first
	mv t3 a0
	strlen %second
	mv t4 a0
	slt t0 t3 t4
	bne t0 zero small
	j pre_loop
small:
	mv t3 t4
pre_loop:
	la t0 %first
	la t1 %second
	li a0 0
loop:
	bltz t3 end
	lb t4 0(t0)
	lb t5 0(t1)
	
	slt t2 t4 t5
	slt t6 t5 t4
	
	beq t2 zero check_equal
	j less
check_equal:
	beq t6 zero equal
	j more
less:
	li a0 -1
	j end
more:
	li a0 1
	j end
equal:
	addi t0 t0 1
	addi t1 t1 1
	addi t3 t3 -1
	j loop
end:
.end_macro

.macro strchr %str %byte
	strlen %str
	mv t0 a0
	la t1 %byte
	lb t1 0(t1)
	la t2 %str
	li a0 0
loop:
	bltz t0 end
	lb t3 0(t2)
	bne t1 t3 neq	
	mv a0 t2
	j end
neq:
	addi t2 t2 1
	addi t0 t0 -1
	j loop
end:
.end_macro

.macro strget 
.data 
buffer: .space 101
.text 
	la a0 buffer
	li a1 102
	li a7 8
	mv t0 a0
	ecall
	li t5 '\n'
	mv t1 t0
	li t4 0
loop:
	lb t3 0(t1)
	beq t5 t3 fix
	addi t4 t4 1
	addi t1 t1 1
	j loop
fix:
	sb zero 0(t1)
# t4-length, t0 - address	
	mv a0 t4 
	mv a1 a0 
	addi a0 a0 1
	li a7 9
	ecall
	mv t2 a0
	addi t4 t4 1
loop_:
	bltz t4 end
	lb t1 0(t0)
	sb t1 0(t2)
	addi t0 t0 1
	addi t2 t2 1
	addi t4 t4 -1
	j loop_
end:
.end_macro
.globl  main
main:
	li s1 0
loop: 
	strget
	beq a1 zero end
    	slt  t0 a1 s1    
    	beq  t0 zero print
	j loop
print:
	li a7 4
	ecall
	li a7 11
	li a0 '\n'
	ecall 
	mv s1 a1
	j loop	
end:
	li a7 10
	ecall 