.eqv    ALLSIZE 0x20000                 # размер экрана в ячейках
.eqv    BASE    0x10010000              # MMIO экрана (на куче)
.data   BASE
screen:
.text
        li a0 ALLSIZE              
        li a7 9
        ecall
	li a7 5 
	ecall
	li t3 127
	mv s8 t3
	li t4 256
	mv a1 t0
	la t0 screen
	mv s10 t0
	li a3 ALLSIZE 
	add t0 t0 a3
	mv a4 t4
	slli a4 a4 2
	sub t0 t0 a4
	mv t5 t3
	mv s9 a4
loop_y:
	mv t6 zero
	bltz t3 main_loop
loop_x:	 
	beq t6 t4 loop_py
	sw a0 (t0)
	addi t6 t6 1
	addi t0 t0 4
	j loop_x
loop_py:
	addi t3 t3 -1
	sub t0 t0 a4
	sub t0 t0 a4
	j loop_y 
main_loop:
	ecall 
	beqz a0 end
	mv s0 a0
	ecall 
	mv s1 a0
	ecall 
	mv s2 a0
	ecall
	mv s3 a0
	mv t3 s8 
	mv a1 t0
	mv t0 s10
	add t0 t0 a3
	sub a2 s2 s0
	bgez a2 good3
	mv a2 zero
good3:
	sub t3 t3 a2
	mul a2 a2 s9
	sub t0 t0 a2
	sub t0 t0 s9
	sub s11 s1 s0
	bgez s11 good
	mv s11 zero
good:
	slli s11 s11 2
	mv t5 s8
	add t2 s1 s0
	addi t2 t2 1
	ble t2 t4 loop__y
	mv t2 t4
	mv t1 s8
	sub t1 s0 s0
	ble t1 t3 gg
	mv t1 zero
gg:
	bgez t3 g
	mv t3 zero
g:
	ble t3 s8 loop__y
	mv t3 s8
loop__y:
	sub t6 s1 s0
	bgez t6 good1
	mv t6 zero
good1:
	add t0 t0 s11
	blt t3 t1 main_loop
loop__x:	 
	beq t6 t2 loop__py
	sub s4 t6 s1
	sub a4 t5 t3
	sub s5 a4 s2
	bgez s4 back1
	neg s4 s4
back1:	
	bgez s5 back2
	neg s5 s5
back2:
	add s4 s4 s5
	bgt s4 s0 not_romb
	sw s3 (t0)
not_romb:
	addi t6 t6 1
	addi t0 t0 4
	j loop__x
loop__py:
	addi t3 t3 -1
	sub t0 t0 s9
	slli t6 t6 2
	sub t0 t0 t6
	j loop__y 	
	j main_loop
end:
	li a7 10
	ecall
