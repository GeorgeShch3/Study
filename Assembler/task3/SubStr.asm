.data                   
yes: .asciz "YES\n" 
no:  .asciz "NO\n"
str_1: .space 505  
str_2: .space 505 
.text
    la a0 str_1      
    li a1 505        
    li a7 8        
    ecall            
    
    mv s0 a0         
    
    la a0 str_2    
    li a1 505       
    li a7 8          
    ecall             

    mv s1 a0         

    li t4 10 
    li t0, 0    
    mv s2 s1   
    lb t2 0(s0)    
compare_loop:
    beq t2 t4 print_no
    mv s1 s2 
    lb t3 0(s1)  
    beq t2, t3, match_found
    addi s0 s0 1  
    lb t2 0(s0)  
    j compare_loop  

print_no:
    li a7 4
    la a0 no
    ecall
    j end
    
match_found:
    addi s0 s0 1 
    addi s1 s1 1
    lb t2 0(s0) 
    lb t3 0(s1)
    beq t3 t4 print_yes 
    bne t2 t3 compare_loop
    j match_found

print_yes: 
    li a7, 4
    la a0 yes
    ecall
    
end:
    li a7 10         
    ecall
