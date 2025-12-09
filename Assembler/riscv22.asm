
#t0,t1,a0,a7
handler:
	csrrw sp uscratch sp
	addi sp sp -16
	sw a0 12(sp)
	sw a1 8(sp)
	sw t0 4(sp)
	sw t1 0(sp)
   	csrr t1 ucause         
    	li a0 2    
   	beq t1 a0 fault  
    	j fatal_exception     

fault:
    	csrr t0 uepc           
    	addi t0 t0 4          
    	csrw t0 uepc      
        lw t1 0(sp)
	lw t0 4(sp)
	lw a1 8(sp)
	lw a0 12(sp)
	addi sp sp 16 
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
.data
msg:
    	.asciz "Exception "        