.data 
.text
	li a7 5
	ecall
	mv a1 a0
	ecall 
	mv s0 a0
	mv s1 a1
	jal nod
	mul s0 s0 s1
	div a0 s0 a0
	j end
nod:
	bne a1 zero one_more
	lw      ra (sp)      
        addi    sp sp 4
	ret
one_more:
	addi    sp sp -4
	sw      ra (sp)
	rem a0 a0 a1  
	mv a2 a0 
	mv a0 a1 
	mv a1 a2     
	jal nod
	lw      ra (sp)      
        addi    sp sp 4
        ret
 	
end:
	li a7 1 
	ecall
   	li a7 10         
   	ecall
