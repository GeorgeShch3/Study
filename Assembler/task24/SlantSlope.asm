.eqv    ALLSIZE 0x20000                 # размер экрана в ячейках
.eqv    BASE    0x10010000              # MMIO экрана (на куче)
.data   BASE
screen:
.text
        li a0 ALLSIZE              # «Закажем» видеопамять в куче
        li a7 9
        ecall
        li a7 6
	ecall
	fmv.s  ft0 fa0
	ecall
	fmv.s ft1 fa0
	ecall
	fmv.s ft2 fa0
	li a7 5
	ecall
	mv t1 a0
	ecall
	mv t2 a0
	mv a1 zero
	li t3 127
	li t4 256
	mv a1 t0
	la s0 screen
	li a0 ALLSIZE 
	add s0 s0 a0
	li a0 256
	slli a0 a0 2
	sub s0 s0 a0
	mv t5 t3
loop_y:
	mv t6 zero
	bltz t3 end
loop_x:	 
	beq t6 t4 loop_py
	fcvt.s.w ft3 t6
	fmul.s ft4 ft0 ft3
	sub s5 t5 t3
	fcvt.s.w ft3 s5
	fmul.s ft5 ft1 ft3
	fadd.s ft4 ft4 ft5
	fadd.s ft4 ft4 ft2
	fmv.s.x ft5 zero
	fgt.s s2 ft4 ft5
	beq s2 zero pc  
	sw t1 (s0)
	j apc
pc:	sw t2 (s0)
apc:
	addi t6 t6 1
	addi s0 s0 4
	j loop_x
loop_py:
	addi t3 t3 -1
	sub s0 s0 a0
	sub s0 s0 a0
	j loop_y 
end:
	li a7 10
	ecall
