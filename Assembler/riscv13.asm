.data
NX: .asciz "NX\n"
UF: .asciz "UF\n"
OF: .asciz "OF\n"
DZ: .asciz "DZ\n"
.text
	li a7 6
	ecall
	fmv.s fa1 fa0 #A
	ecall
	fmv.s fa2 fa0 #B
	ecall
	li a7 4
	fmul.s fa2 fa2 fa0
	frflags t0
	li t5 1
	
	andi t1 t0 1          
	bne t1 zero print1 
b1:
	srl t2 t0 t5          
	andi t2 t2 1 
	addi t5 t5 1        
	bne t2 zero print2 
b2:
	srl t3 t0  t5           
	andi t3 t3 1   
	addi t5 t5 1       
	bne t3 zero print3 
b3:
	srl t4 t0 t5          
	andi t4 t4 1
	addi t5 t5 1
	bne t4 zero print4 
b4:	
	li t0 0
	csrrw t0 fcsr t0
	fdiv.s fa1 fa1 fa2
	li t0 0
	li t1 0
	li t2 0
	li t3 0
	li t4 0
	li t5 0
	
	frflags t6
	li t5 1
	
	andi t1 t6 1          
	bne t1 zero print11
b11:
	srl t2 t6 t5          
	andi t2 t2 1 
	addi t5 t5 1        
	bne t2 zero print22 
b22:
	srl t3 t6  t5           
	andi t3 t3 1   
	addi t5 t5 1       
	bne t3 zero print33 
b33:
	srl t4 t6 t5          
	andi t4 t4 1
	addi t5 t5 1
	bne t4 zero print44 
b44:
	fmv.s fa0 fa1
	li a7 2
	ecall
	li a7 10
	ecall		
print1:
	la a0 NX
	ecall
	j b1

print2:
	la a0 UF
	ecall
	j b2

print3:
	la a0 OF
	ecall
	j b3
	
print4:
	la a0 DZ
	ecall
	j b4
	
print11:
	la a0 NX
	ecall
	j b11

print22:
	la a0 UF
	ecall
	j b22

print33:
	la a0 OF
	ecall
	j b33
	
print44:
	la a0 DZ
	ecall
	j b44