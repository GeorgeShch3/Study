.macro VectorOp %A %B %len %op
	la t0 %A
	la t1 %B
	li t2 %len 
	la t3 %op
loop: 
	beq t2 zero end
	lw a0 0(t0)
	lw a1 0(t1)
	jal %op
	sw a0 0(t0)
	addi t0 t0 4
	addi t1 t1 4	
	addi t2 t2 -1
	j loop	
end:	
.end_macro
