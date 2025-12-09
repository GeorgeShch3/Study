.data
array: .space 40000      
newline: .asciz "\n"     

.text
.globl main

main:
    li t0 0             
    la t1 array          
    la s1 array
    
input_loop:
    li a7 5              
    ecall
    mv t2 a0            

    beq t2 zero process_numbers  

    sw t2 0(t1)         
    addi t1 t1 4        
    addi t0 t0 1        
    j input_loop  
    
process_numbers:
    li t3 0              
    la t1 array         

even_loop:
    bge t3 t0 odd_loop

    lw t2 0(t1)          

    andi t4 t2 1       
    beq t4 zero print 

    addi t1 t1 4        
    addi t3 t3 1        
    j even_loop      

print:
    addi t1 t1 4        
    addi t3 t3 1
    mv a0 t2             
    li a7 1             
    ecall
 
odd_loop:
    bge s2 t0 cheak_end 

    lw s3 0(s1)          
    andi s4 s3 1        
    bne s4 zero print_all 

    addi s1 s1 4        
    addi s2 s2 1        
    j odd_loop   

print_all:   
    mv a0 s3             
    li a7 1             
    ecall
    li a7 4              
    la a0 newline
    ecall 
    addi s1 s1 4   
    addi s2 s2 1  
    j even_loop
   
cheak_end:
    li a7 4              
    la a0 newline
    ecall
    bge t3 t0 end
    j even_loop	  

end:    
    li a7 10             
    ecall
