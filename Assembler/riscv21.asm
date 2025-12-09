.macro strlen %adr
	mv t0 %adr
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

.macro strcmp %first %second
	mv s6 %first
	mv s7 %second
	#strlen s6
	#mv t3 a0
	#strlen s7
	#mv t4 a0
	#slt t0 t3 t4
	#bnez t0 small
	#j pre_loop
#small:
#	mv t3 t4
pre_loop:
	mv t0 s6
	mv t1 s7
	mv a0 zero
loop:
	lb t4 0(t0)
	beqz t4 end
	lb t5 0(t1)
	beqz t5 end
	slt t2 t4 t5
	slt t6 t5 t4
	
	beqz t2 check_equal
	j less
check_equal:
	beqz t6 equal
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
	#addi t3 t3 -1
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
.data 
m: .space 485
.text
main:
	li a7 5
	ecall
	mv a3 a0
	li t0 4
	mul a0 a0 t0 
	mv s3 zero 
	li a7 9
	ecall
	la a4 m 
	mv s4 a4
loop: 
	beq s3 a3 sort
	strget	
	sw a0 0(s4)
	addi s4 s4 4
	addi s3 s3 1
	j loop
sort:    
    addi a3 a3 -1
    mv s3 zero
    mv s4 a4
loop1:
    blt a3 s3 end
    sub s9 a3 s3
    addi s9 s9 -1
    addi s4 s4 4 
    addi s3 s3 1
    mv s5 a4
    mv s8 zero
    lw a0 0(s5)
loop2:
    blt s9 s8 loop1 
    addi s8 s8 1
    addi s5 s5 4
    lw a1 0(s5)
    mv a2 a0
    strcmp a0 a1
    bgtz a0 swap
    beq a0 zero del
    mv a0 a1
    j loop2
swap:
    mv a0 a1
    sw a2 0(s5)
    sw a0 -4(s5)
    mv a0 a2
    j loop2
del:
    mv t0 zero
    mv t1 s5
del_loop:
    blt a3 t0 new
    addi t0 t0 1
    lw a0 0(t1)
    sw a2 0(t1)
    sw a0 -4(t1)
    addi t1 t1 4
    j del_loop
new:
    addi a3 a3 -1
    addi s3 s3 -1
    j loop1
end:
	mv t0 zero
print_loop:
	li a7 4
	blt a3 t0 end_prog
	lw a0 0(a4)
	addi a4 a4 4
	addi t0 t0 1
	ecall 
	li a0 10
	li a7 11
	ecall 
	j print_loop
end_prog:
	li a7 10
	ecall
	
