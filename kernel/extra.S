	/* memset(void* dest, int value, size_t n) */
        .weak memset
memset:
        mov 4(%esp),%eax       # dest
        mov 8(%esp),%edx       # value
        mov 12(%esp),%ecx      # n
1:
        add $-1,%ecx
        jl 1f
        movb %dl,(%eax)
        add $1,%eax
        jmp 1b
1:
        ret

	/* memcpy(void* dest, void* src, size_t n) */
        .weak memcpy
memcpy:
        mov 4(%esp),%eax       # dest
        mov 8(%esp),%edx       # src
        mov 12(%esp),%ecx      # n
        push %ebx
1:
        add $-1,%ecx
        jl 1f
        movb (%edx),%bl
        movb %bl,(%eax)
        add $1,%edx
        add $1,%eax
        jmp 1b
1:
        pop %ebx
        ret

     /* bzero(void* dest, size_t n) */
    .weak bzero
bzero:
    mov 4(%esp),%eax       # dest
    mov 8(%esp),%ecx      # n
1:
    add $-1,%ecx
    jl 1f
    movb $0,(%eax)
    add $1,%eax
    jmp 1b
1:
    ret


