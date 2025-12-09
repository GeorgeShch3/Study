boot:	
	mv t0 a0 
	mv t1 a1
	mv t2 a2
	mv t3 a3
	mv t4 a4
	li t5 0xffff0020
	mv t2 a0
	li s1 0xffff0018
	lw s1 (s1)
	add s1 s1 a4
	sw s1 (t5) #Интервал переключения между заданиями
	la t6 handler 
	csrw t6 utvec
	csrwi uie 0x10
	csrwi ustatus 1
	li t6 0
	addi t5 t1 -2
	slli t5 t5 2
	lw s10 (t0)
	jr s10
end:
	ret 
handler:
	li a7 1
	sub t3 t3 t4
	blez t3 end_
	addi t6 t6 1
	mv a0 t1

	li s0 0xffff0018
	li s1 0xffff0020
	lw s0 (s0)
	add s0 s0 t4
	sw s0 (s1)
	beq t6 t1 p
	
	addi s10 s10 4
    	csrw s10 uepc
	uret
p:
	li t6 0
	sub s10 s10 t5
	csrw s10 uepc
	uret
end_:
	j end
