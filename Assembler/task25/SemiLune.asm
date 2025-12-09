.eqv    ALLSIZE 0x20000                 # размер экрана в ячейках
.eqv    BASE    0x10010000              # MMIO экрана (на куче)
.data   BASE
screen:
.text
        li a0 ALLSIZE              # «Закажем» видеопамять в куче
        li a7 9
        li a1 2
        ecall
	li a7 5
	ecall
	mv s0 a0
	ecall
	mv s1 a0
	ecall
	mul a0 a0 a0
	mv s2 a0
	ecall
	mv s3 a0
	ecall
	mv s4 a0
	ecall
	mul a0 a0 a0
	mv s5 a0
	ecall
	mv s6 a0
	ecall
	mv s7 a0
	
	mv a1 zero
	li t3 127
	li t4 256
	mv a1 t0
	la t0 screen
	li a0 ALLSIZE 
	add t0 t0 a0
	li a0 256
	slli a0 a0 2
	sub t0 t0 a0
	mv t5 t3
loop_y:
	mv t6 zero
	bltz t3 end
loop_x:	 
	beq t6 t4 loop_py
	sub a2 t6 s0 #это новый x
	sub a4 t5 t3
	sub a3 a4 s1 #это новый y
	mul a2 a2 a2
	mul a3 a3 a3
	add a2 a2 a3
	bgt a2 s2 not_moon
	sub a2 t6 s3 #это новый x
	sub a4 t5 t3
	sub a3 a4 s4 #это новый y
	mul a2 a2 a2
	mul a3 a3 a3
	add a2 a2 a3
	ble a2 s5 not_moon
	sw s6 (t0)
	j apc
not_moon:	
	sw s7 (t0)
apc:
	addi t6 t6 1
	addi t0 t0 4
	j loop_x
loop_py:
	addi t3 t3 -1
	sub t0 t0 a0
	sub t0 t0 a0
	j loop_y 
end:
	li a7 10
	ecall
