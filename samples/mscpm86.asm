bdosint      macro  function 
            mov    cl,function   
            int    0E0h
            endm

program     segment
            assume cs:program, ds:program
            org 100h
_start:     
            lea    dx,[_message]
            bdosint 09h
_exit:
            bdosint 00h 
            
_message    db "Hello world, I'm 16-bit CPM/86 Assembly !!!", 13, 10, "$"
program     ends
            end _start
