.data
array_0: .space 128
array_1: .space 64
array_2: .space 64
newline: .asciz "\n"
.text
	li a3 16
	la t0 array_1
	la t1 array_2
	mv a1 t0
	mv a2 t1
	mv s0 zero
	li a7 5
loop_1:
	ecall 
	sw a0 (t0)
	addi t0 t0 4
	addi s0 s0 1
	bltu s0 a3 loop_1
	
	mv s0 zero 
	

loop_2:
	ecall 
	sw a0 (t1)
	addi t1 t1 4
	addi s0 s0 1
	bltu s0 a3 loop_2
	
	la a0 array_0
	jal join
	j print 
join:
	mv t5 a0
	li t1 0
	li t2 0
loop:
	bge t1 a3 first
	bge t2 a3 second
	lw t3 (a1)
	lw t4 (a2)
	blt t3 t4 save
	sw t4 (a0)
	addi a0 a0 4
	addi a2 a2 4
	addi t2 t2 1
	j loop 
save: 
	sw t3 (a0) 
	addi a0 a0 4
	addi a1 a1 4
	addi t1 t1 1
	j loop
first: 
	lw t4 (a2)
	bge t2 a3 end
	sw t4 (a0)
	addi a0 a0 4
	addi a2 a2 4
	addi t2 t2 1
	lw t4 (a2)
	j first 
second: 
	lw t3 (a1)
	bge t1 a3 end
	sw t3 (a0)
	addi a0 a0 4
	addi a1 a1 4
	addi t1 t1 1
	lw t3 (a1)
	j second
end:	
	mv a0 t5
	ret

print: 
	mv t0 a0 
	mv t1 zero 
	li t2 31 
print_loop:
	li a7 1
	bltu t2 t1 end_programm
	lw a0 (t0)
	ecall 
	addi t0 t0 4
	addi t1 t1 1
	li a7 4             
        la a0 newline        
        ecall  
	j print_loop
end_programm:
	li a7 10
	ecall
