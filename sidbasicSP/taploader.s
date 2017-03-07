
        org     5ce0h

main:
        di
;        ld      bc, 7ffdh
;        xor     a
;        out     (c), a
        ld      sp, 0b800h
        ld      hl,0c000h
        ld      a,(5b5ch)
        and     07h
        or      10h
        ld      e,a
;        ld      e,07h
        ld      bc,7ffdh
        out     (c),e
        ld      (hl),01h
        dec     a
        and     07h
        out     (c),a
        ld      (hl),02h
        out     (c),e
        ld      a,(hl)
        dec     a
        jp      nz,0000h
        ld      a,0ffh          ;load data
        ld      de,3298
        ld      ix,0ba00h       ; SIDBASIC
        push    ix
        scf
        jp      0556h
