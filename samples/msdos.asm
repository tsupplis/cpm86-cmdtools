dosint      macro  function 
            mov    ah,function   
            int    21h
            endm

program     segment
            assume cs:program, ds:program
            org 100h
_start:     
            lea    dx,[_message]
            dosint 09h
_exit:
            mov    al,0
            dosint 4ch 
            
_message    db "Hello world, I'm 16-bit DOS COM Assembly !!!", 13, 10, "$"
program     ends
            end _start
