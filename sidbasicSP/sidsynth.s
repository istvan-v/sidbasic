
videoIRQHandler equ     0bab0h
escprsx         equ     0bb06h

        org     8000h

    macro SidSynth_ pwm1, pwm2, pwm3

        align   256

    if (pwm1 & pwm2 & pwm3) == 0
        ld      hl, 80c0h
    else
        ld      h, 80h
    endif
        ld      a, c
        and     h
        ld      (.l2 + 1), a            ; rm other1
        xor     c
    if pwm1 == 0
        xor     l
        ld      (.l3 + 1), a            ; wavetable1
    else
        ld      (.l3 + 4), a            ; vol1
    endif
        ld      a, b
        and     h
        ld      (.l5 + 1), a            ; rm other2
        xor     b
    if pwm2 == 0
        xor     l
        ld      (.l6 + 1), a            ; wavetable2
    else
        ld      (.l6 + 4), a            ; vol2
    endif
        ld      a, e
        and     h
        ld      (.l8 + 1), a            ; rm other3
        xor     e
    if pwm3 == 0
        xor     l
        ld      (.l9 + 1), a            ; wavetable3
    else
        ld      (.l9 + 4), a            ; vol3
    endif
        pop     hl                      ; pwm32
    if pwm1 != 0
        ld      a, d
        ld      (.l3 + 1), a            ; pw1
    endif
    if pwm2 != 0
        ld      a, l
        ld      (.l6 + 1), a            ; pw2
    endif
    if pwm3 != 0
        ld      a, h
        ld      (.l9 + 1), a            ; pw3
    endif
        pop     hl
        ld      (.l1 + 1), hl           ; freq1
        pop     hl
        ld      (.l4 + 1), hl           ; freq2
        pop     hl
        ld      (.l7 + 1), hl           ; freq3
        ld      bc, 0c0fdh
        ld      de, 090ah
        exx
.pagchn ld      a,00h
        out     (0fdh), a

.sidSynth:
.l1:    ld      de, 0000h       ; * frequency
        add     ix, de
        ld      a, h
.l2:    and     00h             ; * ring mod
        xor     ixh
.l3:
    if pwm1
        cp      00h             ; * pw
        sbc     a, a
        and     00h             ; * volume
    else
        ld      d, 00h          ; * wavetable
        ld      e, a
        ld      a, (de)
    endif
        ld      c, a
.l4:    ld      de, 0000h       ; * frequency
        add     iy, de
        ld      a, ixh
.l5:    and     00h             ; * ring mod
        xor     iyh
.l6:
    if pwm2
        cp      00h             ; * pw
        sbc     a, a
        and     00h             ; * volume
    else
        ld      d, 00h          ; * wavetable
        ld      e, a
        ld      a, (de)
    endif
        add     a, c
        ld      c, a
.l7:    ld      de, 0000h       ; * frequency
        add     hl, de
        ld      a, iyh
.l8:    and     00h             ; * ring mod
        xor     h
.l9:
    if pwm3
        cp      00h             ; * pw
        sbc     a, a
        and     00h             ; * volume
    else
        ld      d, 00h          ; * wavetable
        ld      e, a
        ld      a, (de)
    endif
        add     a, c
        exx
        ld      l, a
        ld      h, high dacTable
        ld      a, 8
        out     (c), a
        outi
        inc     b
        inc     h
        out     (c), d
        outd
        inc     b
        inc     h
        out     (c), e
        outi
        inc     b
        exx
        djnz    .sidSynth
        in      a, (0feh)
        cpl
        and     1fh
        jp      z, videoIRQHandler
        jp      escprsx

        endm

        SidSynth_   1, 1, 1
        SidSynth_   1, 1, 0
        SidSynth_   1, 0, 1
        SidSynth_   1, 0, 0
        SidSynth_   0, 1, 1
        SidSynth_   0, 1, 0
        SidSynth_   0, 0, 1
        SidSynth_   0, 0, 0

        block   8800h - $, 00h
        include "dactable.s"
        module  DacTableYM
        include "dactableYM.s"
        endmod

