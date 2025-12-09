.data
msg: .asciz "Exception "  
buffer: .space 258
.text
handler:
	csrrw sp uscratch sp
	addi sp sp -32
	sw a7 28(sp)
	sw t0 24(sp)
	sw t1 20(sp)
	sw t2 16(sp)  
	sw t3 12(sp)
	sw t4 8(sp)
	sw t5 4(sp)
	sw t6 0(sp)
	csrr t1 ucause  
	li a0 8
	bne t1 a0 fatal_exception
	li a0 71
	bne a7 a0 fatal_exception	
	la a0 buffer
	li a1 258
	li a7 8
	ecall
	li t0 ' '
	li t2 '-'
	li t5 10
	mv t6 zero
	mv t4 zero
	lb t1 0(a0)
	li a1 9
loop:	
	beqz t1 end
	lb t1 0(a0)
	addi a0 a0 1
	beq t1 t0 loop
	beq t1 t2 minus
loop2:
	beq a1 t4 end
	li t0 47
	li t2 58
	li a7 '0'
	ble t1 t0 end
	bge t1 t2 end
	sub t1 t1 a7
	mul t3 t3 t5 
	add t3 t3 t1
	lb t1 0(a0)
	addi t4 t4 1  
	beqz t1 end
	addi a0 a0 1
	j loop2 
minus:
	li t6 -1
	j loop
end:
	bnez t4 not_zero
	mv a0 zero
	mv a1 zero
	j stack	
not_zero:
	mv a0 t3
	mv a1 t4
	beqz t6 stack
	mul a0 a0 t6
stack:
	csrr t0 uepc
    	addi t0 t0 4          
    	csrw t0 uepc   
    	lw t6 0(sp)
    	lw t5 4(sp)
    	lw t4 8(sp)
    	lw t3 12(sp)
    	lw t2 16(sp)
    	lw t1 20(sp)
    	lw t0 24(sp)
    	lw a7 28(sp)
	addi sp sp 32
	csrrw sp uscratch sp
	uret
fatal_exception:
    	li a7 4              
    	la a0 msg   
    	ecall
    	li a7 1             
    	mv a0 t1             
    	ecall
    	li a7 10             
    	ecall
