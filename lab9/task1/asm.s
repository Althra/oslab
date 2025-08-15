    AREA MyCode, CODE, READONLY
    EXPORT Assembly_ADD
    IMPORT C_transfer

Assembly_ADD
    ; INIT
    MOV R2, #5
    LDR R3,=0x00001000
    STR R2, [R3]
    
    PUSH {R4, LR}
    MOV R4, R0
    BL C_transfer ; R0 = 8
    CMP R1, #1
    BEQ Find_data1 ; 立即数寻址
    CMP R1, #2
    BEQ Find_data2 ; 寄存器寻址
    CMP R1, #3
    BEQ Find_data3 ; 寄存器间址
    
Find_data1
    MOV R3, #5
    B ADD_Continue
    
Find_data2
    MOV R3, R2
    B ADD_Continue
    
Find_data3
    LDR R3, [R3]
    B ADD_Continue
    
ADD_Continue
    ADD R0, R4, R0
    ADD R0, R0, R3 // 10 + 5 + 8
    POP {R4, PC}

END
