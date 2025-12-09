outnum:
	mv t5 zero
	mv s0 zero
	lui t6 0xffff0
	li t0 0xff
	blt a0 t0 good 
	li  t1 0x80
    	sb  t1 0x10(t6)
    	li  t2 0x80
    	sb  t2 0x11(t6)
	ret
good:	
	li t3 0xD7ED 
	li t4 1   
	andi t1 a0 0xf0
	srli t1 t1 4
	beqz t1 p1
	sll t4 t4 t1
	and t4 t4 t3
	beqz t4 p1
	addi t5 t5 0x1
p1: 
	andi t2 a0 0xf
	li t4 1  
	sll t4 t4 t2
	and t4 t4 t3
	beqz t4 e1
	addi s0 s0 0x1
e1:
	li t3 0x279F
	li t4 1   
	beqz t1 p2
	sll t4 t4 t1
	and t4 t4 t3
	beqz t4 p2
	addi t5 t5 0x2
p2: 
	andi t2 a0 0xf
	li t4 1  
	sll t4 t4 t2
	and t4 t4 t3
	beqz t4 e2
	addi s0 s0 0x2
e2:
	li t3 0x2FFB
	li t4 1   
	beqz t1 p3
	sll t4 t4 t1
	and t4 t4 t3
	beqz t4 p3
	addi t5 t5 0x4 
p3: 
	andi t2 a0 0xf
	li t4 1  
	sll t4 t4 t2
	and t4 t4 t3
	beqz t4 e3
	addi s0 s0 0x4
e3:
	li t3 0x7B6D
	li t4 1   
	beqz t1 p4
	sll t4 t4 t1
	and t4 t4 t3
	beqz t4 p4
	addi t5 t5 0x8 
p4: 
	andi t2 a0 0xf
	li t4 1  
	sll t4 t4 t2
	and t4 t4 t3
	beqz t4 e4
	addi s0 s0 0x8
e4:
	li t3 0xFD45
	li t4 1   
	beqz t1 p5
	sll t4 t4 t1
	and t4 t4 t3
	beqz t4 p5
	addi t5 t5 0x10 
p5: 
	andi t2 a0 0xf
	li t4 1  
	sll t4 t4 t2
	and t4 t4 t3
	beqz t4 e5
	addi s0 s0 0x10
e5:
	li t3 0xDF71 
	li t4 1   
	beqz t1 p6
	sll t4 t4 t1
	and t4 t4 t3
	beqz t4 p6
	addi t5 t5 0x20
p6: 
	andi t2 a0 0xf
	li t4 1  
	sll t4 t4 t2
	and t4 t4 t3
	beqz t4 e6
	addi s0 s0 0x20
e6:
	li t3 0xEF7C
	li t4 1   
	beqz t1 p7
	sll t4 t4 t1
	and t4 t4 t3
	beqz t4 p7
	addi t5 t5 0x40 
p7: 
	andi t2 a0 0xf
	li t4 1  
	sll t4 t4 t2
	and t4 t4 t3
	beqz t4 e7
	addi s0 s0 0x40
e7:
	sb t5 0x11(t6)
	sb  s0 0x10(t6)
	ret
