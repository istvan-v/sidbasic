
        org     0e700h

convertC64Img:
        ld      hl, 0f800h
        ld      de, 5800h
        ld      bc, 0300h
        ldir
        ld      hl, 0e800h
        ld      de, 4800h
        ld      bc, 1000h
.l1:    ldi
        inc     d
        dec     de
        ldi
        inc     d
        dec     de
        ldi
        inc     d
        dec     de
        ldi
        inc     d
        dec     de
        ldi
        inc     d
        dec     de
        ldi
        inc     d
        dec     de
        ldi
        inc     d
        dec     de
        ldi
        ret     po
        ld      a, d
        and     0f8h
        ld      d, a
        jp      .l1

        block   0e800h - $, 00h
        incbin  "sidc64.scr"

