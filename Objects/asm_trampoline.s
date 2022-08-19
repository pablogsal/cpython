    .text
    .globl	_Py_trampoline_func_start
_Py_trampoline_func_start:
    push   %rbp
    mov    %rsp,%rbp
    mov    %rdi,%rax
    mov    %rsi,%rdi
    mov    %rdx,%rsi
    mov    %ecx,%edx
    call   *%rax
    pop    %rbp
    ret
    .globl	_Py_trampoline_func_end
_Py_trampoline_func_end:
