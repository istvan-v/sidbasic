
        org     5ce0h

main:
        di
;        ld      bc, 7ffdh
;        xor     a
;        out     (c), a
        ld      sp, readBlock
        ld      hl,0c000h
        ld      a,(5b5ch)
        and     07h
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
        ld      hl, loaderCodeBegin
        ld      de, readBlock
        ld      bc, loaderCodeEnd - loaderCodeBegin
        ldir
        ld      hl, 0ba00h              ; SIDBASIC
        push    hl
        ld      de, 4253h
        jp      readBlock

loaderCodeBegin:
        phase   0b900h

; DE = block ID
; HL = start address

readBlock:
        di
        push    hl
        pop     ix
.l1:    ld      b, 8dh
        call    readPulse
        sla     b
        adc     hl, hl
        ld      a, l
        xor     0ch
        ld      c, a
        ld      a, h
        xor     0f3h
        or      c
        jr      nz, .l1
.l2:    call    readByte
        cp      0d2h
        jr      z, .l2
        cp      8bh
        jr      nz, .l1
        call    readByte
        cp      e
        jr      nz, .l1
        call    readByte
        cp      d
        jr      nz, .l1
        call    readByte
        ld      e, a
        call    readByte
        ld      d, a
        call    readByte
        ld      l, a
        call    readByte
        ld      h, a
        ld      bc, 9d01h
        jr      .l4
.l3:    ld      bc, 9b01h
.l4:    call    readBit
        ld      a, b                    ; update CRC
        and     80h
        xor     h
        ld      h, a
        add     hl, hl
        ld      a, 21h
        jr      nc, .l6
        xor     l
        ld      l, a
        ld      a, 10h
        xor     h
        ld      h, a
.l5:    sla     b
        rl      c
        ld      b, 9ch
        jr      nc, .l4
        dec     de
        ld      a, e
        ld      (ix), c
        inc     ix
        or      d
        jr      nz, .l3
        out     (0feh), a
        ld      a, l
        and     h
        inc     a
        ret
;        ret     z
;        rst     00h
.l6:    nop
        nop
        jp      .l5

readBit:
        call    readPulse
        ld      a, c
        and     02h
        out     (0feh), a

readPulse:
        ld      a, 0ffh
        in      a, (0feh)
        and     40h
        rra
        rra
        rra
        and     08h
        xor     0f8h                    ; F0h = RET P, F8h = RET M
        ld      (.l2), a
.l1:    ld      a, 0ffh
        in      a, (0feh)
        add     a, a
.l2:    ret     p                       ; *
        djnz    .l1
        ret                             ; bit error

readByte:
        ld      bc, 9c01h
.l1:    call    readBit
        sla     b
        rl      c
        ld      b, 9dh
        jr      nc, .l1
        ld      a, c
        ret

        dephase
loaderCodeEnd:

