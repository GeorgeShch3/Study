perf:
	mv s10 ra
	li s0 0xffff0018
	mv s1 zero #кол-во вызовов
	li s2 0xffff0020
	lw s0 (s0) 
	add s0 s0 a0
        sw s0 (s2)
        la s3 handler       
        csrw s3 utvec        
        csrwi uie 0x10        
        csrwi ustatus 1     
        fcvt.s.w fa1 a0 
loop:
	jalr a1
	addi s1 s1 1
	j loop
after_loop:
	fcvt.s.w fa0 s1 	 
	fdiv.s fa0 fa0 fa1
	mv ra s10
	ret
handler:
    	la s5 after_loop  
    	csrw s5 uepc
	csrr s4 utval
	
	uret
