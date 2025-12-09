.data
eps: .double 1.0e-10
.text
	li a7 5
	ecall
	mv a2 a0
	ecall
	mv a3 a0
	fld ft6 eps t0 
	li a1 -250
	fcvt.d.w ft7 a1 #a
	li a1 250 
	fcvt.d.w ft8 a1 #b
	li a1 2
	fcvt.d.w ft10 a1 #2.0
	fcvt.d.w ft2 a2 #ft2 - c, ft3 - d, ft4 - result, ft1 - x
	fcvt.d.w ft3 a3
	j start
function: 
	fcvt.d.w ft4 zero
	fcvt.d.w ft5 zero
	fmul.d ft4 ft1 ft1
	fmul.d ft4 ft4 ft1 
	fmul.d ft5 ft1 ft2
	fadd.d f4 f4 f5
	fadd.d f4 f4 f3
	fmv.d f0 ft4
	ret
start:
	fsub.d ft1 ft8 ft7 #(b-a)
	fdiv.d ft1 ft1 ft10 #(b-a)/2
	flt.d t0 ft1 ft6  # t0 = 1, если ft1 < ft6, иначе t0 = 0
	bne t0 zero end 
	fadd.d ft1 ft8 ft7 
	fdiv.d ft1 ft1 ft10   
	jal function 
	fmv.d ft11 f0 
	fmv.d fa2 ft1
	fmv.d ft1 ft7
	jal function
	fmv.d ft1 fa2
	fmul.d ft11 ft11 f0
	fcvt.d.w f0 zero
	flt.d t0 f0 ft11 
	bne t0, zero bb
	fmv.d ft8 ft1
	j start
bb:		
	fmv.d ft7 ft1
	j start 
end:
	fmv.d fa0 ft8
	li a7 3
	ecall
	li a7 10
	ecall
