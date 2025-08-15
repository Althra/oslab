    AREA MyCode, CODE, READONLY
    EXPORT Mode_Change
    EXPORT Pri_Thread_Change

; Mode_Change
Mode_Change PROC
    MRS R0, CONTROL     ; 读取CONTROL特殊寄存器
    ORR R0, R0, #1      ; 将第0位置1
    MSR CONTROL, R0     ; 写入CONTROL，切换到用户级线程模式
    
    SVC #0              ; 触发软中断，进入Handler模式
    
    BX LR
    ENDP

; Pri_Thread_Change
Pri_Thread_Change PROC
    MRS R0, CONTROL     ; 读取CONTROL寄存器
    BIC R0, R0, #1      ; 将第0位清0
    MSR CONTROL, R0     ; 写入CONTROL，准备在中断返回后进入特权级
    
    BX LR
    ENDP

END
