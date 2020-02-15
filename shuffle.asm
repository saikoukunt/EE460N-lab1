        .ORIG x3000
        LEA     R1, start
        LDW     R1, R1, #0 ; R1 <- 0x3050

loop    BRnz     done
        LDB     R2, R1, #0 
        LDB     R3, R1, #1 
        STB     R3, R1, #0
        STB     R2, R1, #1

        ADD     R1, R1, #2 ; increment address
        ADD     R0, R0, #-1; decrement remaining count
        BR      loop 

done    HALT

start   .FILL   x3050
        .END