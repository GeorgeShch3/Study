.data
YES: .asciz "YES"
NO: .asciz "NO"
array: .word 80000
.text
	li a7 5
	ecall
	li a1 16
	la a2 array
	mv t1 a2
        bge a0 zero loop

    	xori a0 a0 32   
    	addi a0 a0 1  
loop:
	rem a3 a0 a1
	div a0 a0 a1 
	sw a3 (t1)
	beq a0 zero revers
	addi t1 t1 4
	j loop
revers: 
	beq a2 t1 yes
	lw t0 (a2)
	lw t2 (t1)
	bne t0 t2 no
	addi a2 a2 4
	addi t1 t1 -4

yes:
	li a7 4
	la a0 YES
	ecall
	j end
	
no:
	li a7 4
	la a0 NO
	ecall
end:
	li a7 10
	ecall	