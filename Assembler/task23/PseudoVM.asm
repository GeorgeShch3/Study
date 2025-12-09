.data
table: .word 0:128
.text
handler:
	csrrw sp uscratch sp
	addi sp sp -20
	sw t3 16(sp)
	sw a0 12(sp)
	sw a1 8(sp)
	sw t2 4(sp)
	sw t1 0(sp)
   	csrr t1 ucause         
    	li a0 5   
   	beq t1 a0 load  
 	li a0 7
 	beq t1 a0 store
    	j fatal_exception    
load:
     	la t1 table
     	li a1 16
     	csrr t2 uepc           
    	addi t2 t2 4
    	csrw t2 uepc 
    	csrr t2 utval
loop_:
	beqz a1 end_load
	lw t3 0(t1)
	beq t3 t2 pre_end
	addi t1 t1 8
	addi a1 a1 -1
	j loop_
store:
     	la t1 table
     	li a1 16
     	csrr t2 uepc           
    	addi t2 t2 4
    	csrw t2 uepc 
    	csrr t2 utval
loop__:
	beqz a1 end
	lw t3 0(t1)
	beq t3 t2 write
	beqz t3 write
	addi t1 t1 8
	addi a1 a1 -1	
	j loop__
write:
	sw t2 0(t1)
	addi t1 t1 4
	sw t0 0(t1)
	j end
	
pre_end:
	addi t1 t1 4
	lw t0 (t1)
	j end
end_load:
	mv t0 zero
	j end

end:
        lw t1 0(sp)
	lw t2 4(sp)
	lw a1 8(sp)
	lw a0 12(sp)
	lw t3 16(sp)
	addi sp sp 20 
	csrrw sp uscratch sp 
    	uret
fatal_exception:
    	li a7 1             
    	mv a0 t1             
    	ecall
    	li a7 10             
    	ecall
