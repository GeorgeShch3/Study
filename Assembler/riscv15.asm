.globl main 
.text

.macro strlen %adr
	la t0 %adr
	li t1 '\0'
	li a0 0 
loop:
	lb t2 0(t0) 
	beq t2 t1 end
	addi a0 a0 1
	addi t0 t0 1
	j loop	
end:	
.end_macro

.macro strcpy %res %input
    	la t3 %input
    	la t4 %res
    	mv a1 t4
    	
   	strlen %input
    	mv t5 a0
    	addi a0 a0 -1
   	add t3 t3 a0
   	add t4 t4 a0
   	strlen %res
   	mv t0 a1
	mv a1 a0
	mv a0 a1
loop:
    	beq t5 zero end
    	lb t6 0(t3)
    	sb t6 0(t4)
    	addi t5 t5 -1
    	addi t3 t3 -1
    	addi t4 t4 -1
    	j loop
end:
.end_macro

.macro strcat %res %input
    	la t3 %input
    	la t4 %res
    	mv a1 t4
    	
   	strlen %input
    	mv t5 a0
    	addi a0 a0 -1
   	add t3 t3 a0
   	
   	strlen %res
   	add t4 t4 a0
   	mv t0 a1
	mv a1 a0
	mv a0 a1
	addi t4 t4 -1
loop:
    	beq t5 zero end
    	lb t6 0(t3)
    	sb t6 0(t4)
    	addi t5 t5 -1
    	addi t3 t3 -1
    	addi t4 t4 -1
    	j loop
end:
.end_macro

.data
src:    .asciz  "Source"
dst:    .asciz  "Destination"
fin:    .asciz  "Destination+Source"
.text
main:
        strcpy  fin src
        strcat  fin dst
        la      a0 fin
        li      a7 4
        ecall
        strlen  fin
        li      a7 1
        ecall   