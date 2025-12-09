.macro POLY %A %n %reg
	la a0 %A
	mv t1 %n
	ble t1 zero zero_end
	fmv.d ft2 %reg
	li t2 8
	mul t2 t2 t1
	add a0 a0 t2
	fld  ft1 0(a0)
	addi a0 a0 -8
	fmul.d %reg %reg ft1
	addi t1 t1 -1

loop: 
	ble t1 zero end
	fld ft1 0(a0)
	addi a0 a0 -8
	fadd.d %reg %reg ft1
	fmul.d %reg %reg ft2
	addi t1 t1 -1
	j loop	
end:	
	fld ft1 0(a0)
	fadd.d %reg %reg ft1
	j end_all
zero_end:
	fld ft1 0(a0)
	fmv.d %reg ft1
end_all:
.end_macro
