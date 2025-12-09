
handler:
	csrrw sp uscratch sp
	addi sp sp -16
	sw t0 12(sp)
	sw t1 8(sp)
	sw t2 4(sp)
	sw t3 0(sp)
	li t3 0x80000000 #если знаковый бит не нулевой то это прерывание
    	csrr t0 ucause      
    	and t3 t3 t0
    	beqz t3 exeption 
    	csrr a0 utval
    	li a7 34
    	ecall  
    	li a7 11
    	li a0 10
    	ecall
	j end
 exeption:   
    	csrr t2 uepc
	addi t2 t2 4
	csrw t2 uepc  
    	mv a0 t0     
    	li a7 34
    	ecall  
    	li a7 11
    	li a0 10
    	ecall
end:
	lw t3 0(sp)
        lw t2 4(sp)
        lw t1 8(sp)
        lw t0 12(sp)        
        addi sp sp 16         
        csrrw sp uscratch sp 
    	uret                       
