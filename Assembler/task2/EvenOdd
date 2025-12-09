.data
array: .space 40000
arrend:
newline: .asciz "\n"
.text
	li a7 5
	ecall
	li s0 0
	la t0 array
	la t1 arrend
	mv t2 t0
	mv s1 a0
loop:
	ecall 
	sw a0 (t0)
	addi t0 t0 4
	addi s0 s0 1
	bltu s0 s1 loop

	mv t0 t2
	mv s0 zero
loop_1:
	lw t3 (t0)
	andi t4 t3 1
	beq t4 zero print_1 
back_1:
	addi t0 t0 4
	addi s0 s0 1
	bltu s0 s1 loop_1

	mv t0 t2
	mv s0 zero
loop_2:
	lw t3 (t0)
	andi t4 t3 1
	bne t4 zero print_2 
back_2:
	addi t0 t0 4
	addi s0 s0 1
	bltu s0 s1 loop_2

	li a7 10 
	ecall
print_2:
	li a7 1
	mv a0 t3
	ecall
	li a7 4             
        la a0 newline        
        ecall 
	j back_2
print_1:
	li a7 1
	mv a0 t3
	ecall
	li a7 4             
        la a0, newline        
        ecall  
	j back_1	
