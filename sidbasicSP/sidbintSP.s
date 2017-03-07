USE_ROM_LOADER  equ     1

start           equ     0ba00h
decodeTablesBegin   equ start+16
triangletable   equ     8b00h
sawtoothtable   equ     triangletable+0f00h
noisetable      equ     triangletable+1e00h
convertC64Img   equ     0e700h
input_buf       equ     5b00h
reoradd         equ     input_buf+300h
stack           equ     0c000h
; sidSynth loop time = 345 cycles
; sample rate = fSR = 17734475 / 5 / 345 = 10281 Hz
; frequency multiplier = 512 * (17734475 / 18 / 256) / fSR = 192
FREQ_MULT       equ     192
; frame duration should be 17734475 / 5 / (17734475 / 18 / 63 / 312) = 70762 Z80 cycles
SMPS_PER_FRAME  equ     204

        phase   start

startpr di
        ld      hl,5800h
        ld      de,5801h
        ld      bc,0ffh
        ld      (hl),3fh
        ldir
    if 0 ; USE_ROM_LOADER != 0
        ld      hl,5b00h
        ld      de,4500h
        ld      bc,200h
        ldir
    endif
        ld      hl,cod4200
        ld      de,4200h
        ld      bc,c4200ln
        ldir
        ld      hl,4010h
        ld      bc,0fffdh
        xor     a
        out     (c),a
        ld      b,0bfh
        out     (c),h
        ld      b,0ffh
        in      a,(c)
        cp      h
        jr      nz,.l1
        out     (c),l
        in      a,(c)
        inc     a
        jr      z,.l2
.l1:    ld      a,3eh                   ;=LD A,n
        ld      (cpYMtbl),a             ;no YM detected
.l2:    xor     a
        out     (c),a
        ld      b,0bfh
        out     (c),a
        ld      hl,escprsx
        push    hl
        ld      hl,speccycheck
        push    hl
        exx
        ld      hl,screen+1
        ld      e,80h
        exx
        ld      de,convertC64Img
        push    de
        jp      decompressData

        db      "Töki,Drazsé,Gombóc,Gyurmi,Bogyó,Lizi,Maszat,Gombóc.."
        defs    156 - ($-decodeTablesBegin)

        block   0baaeh - $, 00h

paglen  defw    0ffffh

videoIRQHandler
speed1  ld      b,SMPS_PER_FRAME
        exx
plypage ld      a,00h
        out     (0fdh),a
        ld      hl,paglen
        inc     (hl)
        jr      z,blkend
npgend  ld      hl,1000h
        pop     bc
        pop     de
        ld      a,c
        rlca
        rlca
        rl      h
        ld      a,b
        rlca
        rlca
        rl      h
        ld      a,e
        rlca
        rlca
        rl      h
        jp      (hl)
blkend  inc     l
        inc     (hl)
        jr      nz,npgend
pagenm  ld      hl,pages
        ld      a,(hl)
        ld      (plypage+1),a
        out     (0fdh),a
        inc     l
        ld      a,l
endpg1  cp      06h
        sbc     a,a
        and     l
        ld      (pagenm+1),a
        ld      hl,(0fffeh)
        ld      (paglen),hl
        ld      sp,0c000h       ;sid data start
        jp      npgend

        align   256
pages   db      00h,01h,03h,04h,06h,07h

escprsx ld      sp,stack
        call    filesel
        call    resetpage
        call    nextpage
        ld      hl,(0fffeh)
        ld      (paglen),hl
        ld      bc,0c0fdh
        exx
        ld      ix,0000h
        ld      iy,0000h
        ld      hl,0000h
speed2  ld      b,SMPS_PER_FRAME
        di
siddata ld      sp,0c000h
        jr      videoIRQHandler

; -----------------------------------------------------------------------------

reorder call    resetpage
conv16k call    nextpage
        ld      hl,0c000h
        ld      de,reoradd
        ld      bc,02a9h
        call    copyblk
        ld      hl,0e000h
        call    copyblk
        ld      hl,(0dffeh)
        ld      a,h
        or      l
        jr      z,blkcnt2
        ex      de,hl
        ld      hl,10000h-02aah
        add     hl,de
        jr      storlen
blkcnt2 ld      de,(0fffeh)
        add     hl,de
        ex      de,hl
        ld      hl,10000h-(02aah*2)
        add     hl,de
storlen ld      (0fffeh),hl     ;store song length on the page
        ld      hl,reoradd
        ld      ix,0c000h
convdat call    convchn
        ex      de,hl
        ld      hl,0010h
        add     hl,de
        ex      de,hl
        ld      a,d
        cp      high (reoradd+4000h)
        jr      nz,convdat
        ld      a,(pagenum+1)
endpage cp      00h
        jr      nz,conv16k
        ret

convchn call    divfreq
        ld      (ix+6),e
        ld      (ix+7),d
        call    getvol
        ld      (ix+0),a        ;vol
        ld      (ix+3),e        ;pwm
        call    divfreq
        ld      (ix+8),e
        ld      (ix+9),d
        call    getvol
        ld      (ix+1),a        ;vol
        ld      (ix+4),e        ;pwm
        call    divfreq
        ld      (ix+10),e
        ld      (ix+11),d
        call    getvol
        ld      (ix+2),a        ;vol
        ld      (ix+5),e        ;pwm
        ld      de,000ch
        add     ix,de
        ret

    macro BitMult n
        add     hl, hl
        rla
      if (FREQ_MULT & n) != 0
        add     hl, de
        adc     a, b
      endif
    endm

divfreq:
        ld      e, (hl)
        inc     l
        ld      d, (hl)                 ; frequency
        inc     l
        push    hl
        xor     a
        ld      b, a
    if (FREQ_MULT & 80h) == 0
        ld      l, a
        ld      h, a
    else
        ld      l, e
        ld      h, d
        add     hl, hl
        rla
    endif
    if (FREQ_MULT & 40h) != 0
        add     hl, de
        adc     a, b
    endif
        BitMult 20h
        BitMult 10h
        BitMult 08h
        BitMult 04h
        BitMult 02h
        BitMult 01h
        ld      l, h
        ld      h, a
        inc     hl
        srl     h
        rr      l
        ex      de, hl
        pop     hl
        ret

getvol  ld      e,(hl)
        inc     l
        ld      a,e
        and     60h
        jr      z,v1tri
        cp      40h
        jr      z,v1pul
        jr      c,v1saw
        ld      d,high (noisetable-8000h)-1     ;modify /2
        defb    0dah
v1saw   ld      d,high (sawtoothtable-8000h)-1  ;modify /2
v1cont  ld      a,e
        and     1eh             ;volume
        jr      z,v1pul
        rrca
        add     a,d
        or      40h
        jr      storreg
v1tri   ld      d,high (triangletable-8000h)-1  ;modify /2
        jr      v1cont
v1pul   ld      a,e
        and     1fh
        add     a,a
storreg ld      d,a
        ld      a,e
        and     80h
        or      d
        ld      e,(hl)
        inc     hl
        ret

; -----------------------------------------------------------------------------

SIZE_OPTIMIZED          equ     0
; total table size is 156 bytes, and should not cross a 256-byte page boundary
; decodeTablesBegin     equ     0ff4ch

nLengthSlots            equ     8
nOffs1Slots             equ     4
nOffs2Slots             equ     8
maxOffs3Slots           equ     32

lengthDecodeTable       equ     decodeTablesBegin
offs1DecodeTable        equ     lengthDecodeTable + (nLengthSlots * 3)
offs2DecodeTable        equ     offs1DecodeTable + (nOffs1Slots * 3)
offs3DecodeTable        equ     offs2DecodeTable + (nOffs2Slots * 3)
decodeTablesEnd         equ     offs3DecodeTable + (maxOffs3Slots * 3)

        assert  decodeTablesBegin >= ((decodeTablesEnd - 1) & 0ff00h)

; input/output parameters:
;     HL':            source address (forward decompression)
;     DE:             destination address (C000h-FFFFh)
;     E':             shift register (should be 80h before the first block)
;     A, Z flag:      A = 0, Z = 1 if there are more blocks remaining
; BC, HL, BC', D':    undefined
; AF', IX, IY:        not changed

decompressDataBlock:
        exx
        xor   a
        ld    c, a
        ld    b, a
        exx
        ld    bc, 1001h
        ld    h, a
        call  readBits16                ; read block size (HL)
        ld    c, 0                      ; set C = 0 for read2Bits/readBits
        call  read2Bits                 ; read flag bits
        srl   a
        push  af                        ; save last block flag (A=1, Z=0: yes)
        jr    nc, .l14                  ; uncompressed data ?
        call  read2Bits                 ; get prefix size for >= 3 byte matches
        push  de                        ; save decompressed data write address
        push  hl
        exx
        ld    b, a
        ld    a, 02h                    ; len >= 3 offset slots: 4, 8, 16, 32
        ld    d, 80h                    ; prefix size codes: 40h, 20h, 10h, 08h
        inc   b
.l2:    rlca
        srl   d                         ; D' = prefix size code for length >= 3
        djnz  .l2
        pop   bc                        ; store the block size in BC'
        exx
        add   a, nLengthSlots + nOffs1Slots + nOffs2Slots - 3
        ld    b, a                      ; store total table size - 3 in B
        ld    hl, decodeTablesBegin     ; initialize decode tables
.l3:    ld    de, 1
.l4:    ld    a, 10h                    ; NOTE: C is 0 here, as set above
        call  readBits
        ld    (hl), a                   ; store the number of bits to read
        inc   hl
        ld    (hl), e                   ; store base value LSB
        inc   hl
        ld    (hl), d                   ; store base value MSB
        inc   hl
        push  hl
        ld    hl, 1                     ; calculate 2 ^ nBits
        jr    z, .l6                    ; readBits sets Z = 1 if A = 0
.l5:    add   hl, hl
        dec   a
        jr    nz, .l5
.l6:    add   hl, de                    ; calculate new base value
        ex    de, hl
        pop   hl
        ld    a, l
        cp    low offs1DecodeTable
        jr    z, .l3                    ; end of length decode table ?
        cp    low offs2DecodeTable
        jr    z, .l3                    ; end of offset table for length = 1 ?
        cp    low offs3DecodeTable
        jr    z, .l3                    ; end of offset table for length = 2 ?
        djnz  .l4                       ; continue until all tables are read
        pop   de                        ; DE = decompressed data write address
        jr    .l9                       ; jump to main decompress loop
.l7:    exx
        ld    a, d
        or    high 0c000h
        ld    d, a
        pop   af                        ; return with last block flag in A, Z
        ret
.l8:    ld    a, (hl)                   ; copy literal byte
        inc   hl
        exx
        ld    (de), a
        inc   de
.l9:    exx
.l10:   ld    a, c                      ; check the data size remaining:
        or    b
        jr    z, .l7                    ; end of block ?
        dec   bc
        sla   e                         ; read flag bit
    if SIZE_OPTIMIZED == 0
        jr    nz, .l11
        ld    e, (hl)
        inc   hl
        rl    e
    else
        call  z, readCompressedByte
    endif
.l11:   jr    nc, .l8                   ; literal byte ?
        ld    a, 0f8h
.l12:   sla   e                         ; read length prefix bits
    if SIZE_OPTIMIZED == 0
        jr    nz, .l13
        ld    e, (hl)
        inc   hl
        rl    e
    else
        call  z, readCompressedByte
    endif
.l13:   jr    nc, copyLZMatch           ; LZ77 match ?
        inc   a
        jr    nz, .l12
        exx                             ; literal sequence:
        ld    bc, 0811h                 ; 0b1, 0b11111111, 0bxxxxxxxx
        ld    h, a
        call  readBits16                ; length is 8-bit value + 17
.l14:   ld    c, l                      ; copy literal sequence,
        ld    b, h                      ; or uncompressed block
        exx
        push  hl
        exx
        pop   hl
        ldir
        push  hl
        exx
        pop   hl
        jr    .l10                      ; return to main decompress loop

copyLZMatch:
        exx
        ld    b, low (lengthDecodeTable + 24)
        call  readEncodedValue          ; decode match length
        ld    c, 20h                    ; C = 20h: not readBits routine
        or    h                         ; if length <= 255, then A and H are 0
        jr    nz, .l8                   ; length >= 256 bytes ?
        ld    b, l
        djnz  .l7                       ; length > 1 byte ?
        ld    b, low offs1DecodeTable   ; no, read 2 prefix bits
.l1:    ld    a, 40h                    ; read2Bits routine if C is 0
.l2:    exx                             ; readBits routine if C is 0
.l3:    sla   e                         ; if C is FFh, read offset prefix bits
    if SIZE_OPTIMIZED == 0
        jp    nz, .l4
        ld    e, (hl)
        inc   hl
        rl    e
    else
        call  z, readCompressedByte
    endif
.l4:    rla
        jr    nc, .l3
        exx
        cp    c
        ret   nc
        push  hl
        call  readEncodedValue          ; decode match offset
        ld    a, e                      ; calculate LZ77 match read address
        sub   l
        ld    l, a
        ld    a, d
        sbc   a, h
        or    high 0c000h
        ld    h, a
        pop   bc
        ld    a, l
        add   a, c
        ld    a, h
        adc   a, b
        jr    c, .l5                    ; source sequence crosses page boundary?
        ldir                            ; copy match data
        jr    decompressDataBlock.l9    ; return to main decompress loop
.l5:    xor   a
.l6:    ldi
        cp    h
        jr    nz, .l6
        ld    a, c
        or    b
        jr    z, decompressDataBlock.l9
        ld    h, high 0c000h
        ldir
        jr    decompressDataBlock.l9
.l7:    djnz  .l8                       ; length > 2 bytes ?
        ld    a, c                      ; no, read 3 prefix bits (C = 20h)
        ld    b, low offs2DecodeTable
        jr    .l2
.l8:    exx                             ; length >= 3 bytes,
        ld    a, d                      ; variable prefix size
        exx
        ld    b, low offs3DecodeTable
        jr    .l2

; NOTE: C must be 0 when calling these
read2Bits       equ     copyLZMatch.l1
; read 1 to 8 bits to A for A = 80h, 40h, 20h, 10h, 08h, 04h, 02h, 01h
readBits        equ     copyLZMatch.l2

readEncodedValue:
        ld    l, a                      ; calculate table address L (3 * A + B)
        add   a, a
        add   a, l
        add   a, b
        ld    l, a
        ld    h, high decodeTablesBegin
        ld    b, (hl)                   ; B = number of prefix bits
        inc   l
        ld    c, (hl)                   ; AC = base value
        inc   l
        ld    h, (hl)
        xor   a

; read B bits to HL, and add HC to the result; A must be zero

readBits16:
        ld    l, c
        cp    b
        ret   z
        ld    c, a
.l1:    exx
        sla   e
    if SIZE_OPTIMIZED == 0
        jp    nz, .l2
        ld    e, (hl)
        inc   hl
        rl    e
    else
        call  z, readCompressedByte
    endif
.l2:    exx
        rl    c
        rla
        djnz  .l1
        ld    b, a
        add   hl, bc
        ret

    if SIZE_OPTIMIZED != 0
readCompressedByte:
        ld    e, (hl)
        inc   hl
        rl    e
        ret
    endif

decompressData:
.l1:    call  decompressDataBlock
        jr    z, .l1
        ret

        assert  ($ <= decodeTablesBegin) || (decompressDataBlock >= decodeTablesEnd)

; -----------------------------------------------------------------------------

copyblk ld      a,0ch
copynxt push    hl
copy1bl ldi
        inc     c
        add     hl,bc
        dec     a
        jr      nz,copy1bl
        pop     hl
        inc     hl
        ld      a,l
        cp      0aah
        jr      nz,copyblk
        ld      a,h
        and     02h
        cp      02h
        jr      nz,copyblk
        ret

copydat push    de
        ld      hl,0c000h
copydt1 ex      af,af'
        ld      a,07h
        ld      bc,07ffdh
        out     (c),a
        ld      de,decodeTablesBegin
        ld      bc,80h
        ldir
        ex      af,af'
        ld      bc,07ffdh
        out     (c),a
        ld      e,low decodeTablesBegin
        ld      bc,0ff80h
        add     hl,bc
        inc     b
        ex      de,hl
        ldir
        ex      de,hl
        jr      c,copydt1
        pop     de
        ret

nextpage
pagenum ld      hl,pages
;        ld      bc,7ffdh
        ld      a,(hl)
        ld      (plypage+1),a
;        out     (c),a
        out     (0fdh),a
        inc     l
        ld      a,l
        cp      06h
        jr      nz,nmemend
        xor     a
nmemend ld      (pagenum+1),a
        ret

unpack  call    resetpage
        exx
        ld      hl,input_buf + 11h
        ld      e,80h
        exx
        ld      de,0c000h
        xor     a
        ld      (pgval+1),a
unpack1 ld      a,07h
        ld      bc,7ffdh
        out     (c),a
        call    decompressDataBlock
        jr      nz,finunp
        call    decompressDataBlock
finunp  push    af
pgval   ld      a,00h
        cp      07h
        jr      z,unpfin
        call    copydat
        call    nextpage
        ld      a,(hl)
        and     07h
        ld      (pgval+1),a
        pop     af
        jr      z,unpack1
        ret

unpfin  dec     a
        ld      (pagenum+1),a
        pop     af
        ret

resetpage
        xor     a
        ld      (pagenum+1),a
        inc     a
        ld      (pagenm+1),a
        ret
;       0,   300,   447,   635,   925,  1351,  1851,  2991,
;     3695,  5782,  7705,  9829, 12460, 15014, 18528, 21845

; -----------------------------------------------------------------------------

;        defs    low -$
loaderCodeBegin:
;        phase   0bf00h

    if USE_ROM_LOADER == 0

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

    else

tapeload
        di
        call    relkey
        xor     a               ;load header
        ld      de,0011h
        call    loader
        ret     nz
        ld      de,(input_buf+11)
        ld      a,d
        or      e
        jr      nz,lengthok
        ld      de,5f10h
lengthok
        ld      a,0ffh          ;load data
        call    loader
        ret     nz
        ld      hl,(input_buf+0eh)
        ld      a,l
        or      h
        ret     nz
        ld      a,(input_buf+01h)
        xor     'O'
        ret

loader  scf
        ld      ix,input_buf
        inc     d
        ex      af,af'
        dec     d
        ld      a,0fh
        out     (0feh), a
        call    0562h
        ld      a,7fh
        in      a,(0feh)
        rra
        jr      nc,spacepr
        xor     a
        ret
spacepr
relkey  xor     a
        in      a,(0feh)
        cpl
        and     1fh
        jr      nz,relkey
        inc     a
        ret

    endif

;        dephase

; -----------------------------------------------------------------------------

filesel di
        xor     a
        ld      l,0bh
clray   ld      bc,0fffdh
        dec     l
        out     (c),l
        ld      b,0bfh
        out     (c),a
        jr      nz,clray
        ld      a,07h
        ld      b,0ffh
        out     (c),a
        ld      a,3fh
        ld      b,0bfh
        out     (c),a
        im      1
statusb xor     a
        out     (0feh),a
    if USE_ROM_LOADER == 0
        ld      hl, input_buf   ; M64 input file
        ld      de, 364dh
        call    readBlock
    else
        ld      a,04h
        ld      bc,1ffdh
        out     (c),a
        ld      a,10h
        ld      b,7fh
        out     (c),a
        call    tapeload
    endif
        jr      z,loadok
error   xor     a
        in      a,(0feh)
        cpl
        and     1fh
        jr      nz,statusb
        ld      a,r
        and     07h
        out     (0feh),a
        jr      error
loadok
        ld      hl,(input_buf+4)
        or      h
        jr      nz,error
        ld      a,l
        cp      41
        jr      c,error
; fIRQ = (17734475 / 18 / 63 / 312) * (fM64 / 50)
; fZ80 = 17734475 / 5
; Z80 cycles per frame = fZ80 / fIRQ
; Z80 cycles per sample = 345
; IRQ cost = ~481 cycles
; N = (fZ80 / fIRQ - 481) / 345 = 10255.295 / fM64 - 1.386
        ld      b, a
        ld      de, 1
        ld      hl, 0f4ch               ; 10255.295 * 256 = 280F4Ch
        ld      a, 28h
spdlp1  add     hl, hl
        rla
        sub     b
        jr      nc, spdlp2
        add     a, b
spdlp2  ccf
        rl      e
        rl      d
        jr      nc, spdlp1
        ld      hl, 10000h - 227        ; -1.386 * 256 + 128
        add     hl, de
        ld      a, h
        ld      (speed1+1),a
        ld      (speed2+1),a
        ld      a,07h
        out     (0feh),a
        call    unpack
        di
        ld      a,(pagenum+1)
        ld      (endpg1+1),a
        cp      06h
        jr      nz,pgvalok
        xor     a
pgvalok ld      (endpage+1),a
        call    reorder
        ld      a,02h
        ld      bc,7ffdh
        out     (c),a
        exx
        ld      hl,sidSynthPacked+1
        ld      e,80h
        exx
        ld      de,0c000h
        call    decompressData
        xor     a
        ld      bc,7ffdh
        out     (c),a
        ld      hl,8b00h        ;copy YM D/A table
        ld      de,8800h
        ld      bc,300h
cpYMtbl ldir
speccyplus
        call    specpls
        ld      de,triangletable

; DE = table start address

sidWaveTables:
.mod1   ld    c,04h             ; triangle / sawtooth
.l1:    ld    hl, 0080h
.l2:    ex    de, hl
        ld    b, h              ;write address high triangle
        ld    a, h              ;write address high triangle
        add   a, high 0f00h
        ld    h, a              ;write address high sawtooth
        ld    (hl), d           ; sawtooth
        ld    h, b              ;write address high triangle
        ld    b, l              ;write address low sawtooth
        ld    a, l
        rra                     ;get write address low triangle
        jr    nc, .l3
        cpl                     ;every odd byte to the beginning, even to the end
.l3:    ld    l, a              ;flip-flop address of triangle
        ld    (hl), d           ; triangle
        ld    l, b              ;back write address high triangle
        ex    de, hl
        ld    b, 0
        add   hl, bc
        inc   e
        jr    nz, .l2
        inc   d
.mod2   inc   c
        inc   c
        inc   c
        inc   c
        bit   6, c
        jr    z, .l1
        ld    de, triangletable + 1e00h
.l4:    ld    c, 12h                    ; noise (23 bit LFSR)
        ld    hl, 3456h
.l5:    ld    b, c
        ld    a, h
        srl   b
        rra
        srl   b
        rra
        ld    b, a
        ld    a, h
        add   a, a
        ld    a, c
        rla
        xor   b
        ld    c, h
        ld    h, l
        ld    l, a
        ld    a, d
        sub   high (triangletable + 1d00h)
        rla
        push  bc
        push  hl
        ld    c, l                      ; HL = A * L (A < 64)
        ld    b, 0
        add   a, a
        add   a, a
        add   a, a
        ld    h, a
        jr    c, .l6
        ld    l, b
.l6:    add   hl, hl
        jr    nc, .l7
        add   hl, bc
.l7:    add   hl, hl
        jr    nc, .l8
        add   hl, bc
.l8:    add   hl, hl
        jr    nc, .l9
        add   hl, bc
.l9:    add   hl, hl
        jr    nc, .l10
        add   hl, bc
.l10:   add   hl, hl
        jr    nc, .l11
        add   hl, bc
.l11:
        ld    a, h
        sla   l
        adc   a, b                      ; B = 0
        pop   hl
        pop   bc
        add     a,a
        ld    (de), a
        inc   e
        jr    nz, .l5
        inc   d
        ld    a, d
        cp    high (triangletable + 2d00h)
        jr    nz, .l5       ;.l4
        ret

;chkfile ld      hl,10100h - (input_buf+10h)
;        add     hl,de
;        push    de
;        ld      b,l
;        ld      c,h             ; CB = file size + 256
;        ld      de,10000h - (18 + 256)
;        add     hl,de
;        pop     hl
;        jr      nc,.l4          ;if file size<18
;        sbc     a,a
;        inc     b
;        ld      d,0ach
;        jr      .l3
;.l1:    dec     h
;.l2:    dec     l
;        xor     (hl)
;        rlca
;        add     a,d
;.l3:    djnz    .l2
;        dec     c
;        jr      nz,.l1
;        xor     80h
;        jr      nz,.l4
;        inc     hl
;        ld      a,(hl)
;        cp      high 2000h      ;error if 2nd byte >= 32
;        jr      nc,.l4
;        inc     hl
;        or      (hl)
;        ret     nz              ;if file was ok NZ
;.l4:    xor     a               ;if error then Z
;        ret

; -----------------------------------------------------------------------------

cod4200
        phase   4200h

sidSynthPacked:
        incbin  "sidsynth.bin"
speccycheck
        ld      hl,0d000h
        ld      a,(5b5ch)
        and     07h
        ld      e,a
        out     (0fdh),a
        ld      (hl),01h
        dec     a
        and     07h
        out     (0fdh),a        ;01
        ld      (hl),02h
        ld      a,e
        out     (0fdh),a
        ld      a,(hl)
        dec     a
        ret     z               ;128, +2, Pentagon128
        ld      hl,pages        ;+2a,+3, Scorpion
        ld      b,06h
convpag ld      a,(hl)
        or      50h
        ld      (hl),a
        inc     hl
        djnz    convpag
        ld      a,0cdh
        ld      (speccyplus),a
        ret
specpls ld      a,50h
        ld      (803dh+1),a
        ld      hl,813bh+1
        ld      (813bh+1),a
        ld      (823bh+1),a
        ld      (8338h+1),a
        ld      (843bh+1),a
        ld      (8538h+1),a
        ld      (8638h+1),a
        ld      (8735h+1),a
        ret
c4200ln equ     $-sidSynthPacked
        dephase

screen  incbin  "cod47scr.bin"

vege
        dephase
len     equ $-startpr

