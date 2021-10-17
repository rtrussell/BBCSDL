/*
 * Copyright (c) 2020 Graham Sanderson
 *
 * Memotech-Bill  9/ 4/21 Additional commenting
 * Memotech-Bill  7/ 9/21 Extended versions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
 
#include "hardware/regs/sio.h"
#include "hardware/regs/addressmap.h"

.cpu cortex-m0plus
.thumb

.section .time_critical.pal16

#define INTERP_OFFSET0(x) (x - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP_OFFSET1(x) (INTERP_OFFSET0(x) + SIO_INTERP1_ACCUM0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)

    // Convert a line of framebuffer to 16-bit pixel values
    // Each byte becomes 2 pixel values (4 bytes)
// setup: note palette is 256 entries of 32bits; i.e. all 2 pixel combinations
//
// interp_config c = interp_default_config();
// interp_config_set_shift(&c, 22);
// interp_config_set_mask(&c, 2, 9);
// interp_set_config(interp0, 0, &c);
// interp_config_set_shift(&c, 14);
// interp_config_set_cross_input(&c, true);
// interp_set_config(interp0, 1, &c);
// interp_set_base(interp0, 0, (uintptr_t)palette256);
// interp_set_base(interp0, 1, (uintptr_t)palette256);

    // Convert a line of framebuffer to 16-bit pixel values
    // Each byte becomes 2 pixel values (4 bytes)
// dest - pixel buffer 16bpp
// src - source pixel buffer 4bpp (must be 32 bit aligned)
// count - is number of bytes to convert (must be a multiple of 4)
// extern void convert_from_pal16(uint32_t *dest, uint8_t *src, uint count);
    // r0 = &dest
    // r1 = &src
    // r2 = count

.global convert_from_pal16
.type convert_from_pal16,%function
.thumb_func
convert_from_pal16:
    push {r4-r7, lr}                                                // Save registers
    add r2, r1                                                      // r2 = &src + count
    ldr r7, =(SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET)                 // Address of SIO_INTERP0 registers

1:
    ldmia r1!, {r3}                                                 // r3 = *(src++)
    str r3, [r7, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]        // SIO_INTERP0_ACCUM0 = r3
    ldr r6, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]    // r6 = SIO_INTERP0_PEEK_LANE0
    ldr r5, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE1_OFFSET)]    // r5 = SIO_INTERP0_PEEK_LANE1
    ldr r6, [r6]                                                    // r6 = *(r6) = palette256[r3>>24]
    ldr r5, [r5]                                                    // r5 = *(r5) = palette256[(r3>>16)&0xFF]
    lsl r3, #16                                                     // r3 <<= 16
    str r3, [r7, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]        // SIO_INTERP0_ACCUM0 = r3
    ldr r4, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]    // r4 = SIO_INTERP0_PEEK_LANE0
    ldr r3, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE1_OFFSET)]    // r3 = SIO_INTERP0_PEEK_LANE1
    ldr r4, [r4]                                                    // r4 = *(r4) = palette256[r3>>24]
    ldr r3, [r3]                                                    // r3 = *(r3) = palette256[(r3>>16)&0xFF]
    stmia r0!, {r3, r4, r5, r6}     // *(dest++) = r3, *(dest++) = r4, *(dest++) = r5, *(dest++) = r6
    cmp r1, r2                                                      // Compare (r1 - r2)
    blo 1b                                                          // Repeat if r1 < r2 (= &src[count])

    pop {r4-r7, pc}                                                 // Restore registers and exit

    // Convert a line of framebuffer to 16-bit pixel values
    // Each byte becomes 8 pixel values (16 bytes)
// setup: note palette is 256 entries of 8 pixels (16 bytes); i.e. all pixel combinations for 1 byte
//
// interp_config c = interp_default_config();
// interp_config_set_shift(&c, 20);
// interp_config_set_mask(&c, 4, 11);
// interp_set_config(interp0, 0, &c);
// interp_config_set_shift(&c, 12);
// interp_config_set_cross_input(&c, true);
// interp_set_config(interp0, 1, &c);
// interp_set_base(interp0, 0, (uintptr_t)palette256);
// interp_set_base(interp0, 1, (uintptr_t)palette256);

// dest - pixel buffer 16bpp
// src - source pixel buffer 1-4bpp (must be 32 bit aligned)
// count - is number of bytes to convert (must be a multiple of 4)
// extern void convert_from_pal16_8(uint32_t *dest, uint8_t *src, uint count);
    // r0 = &dest
    // r1 = &src
    // r2 = count

.global convert_from_pal16_8
.type convert_from_pal16_8,%function
.thumb_func
convert_from_pal16_8:
    push {r4-r7, lr}                                                // Save registers
    add r2, r1                                                      // r2 = &src + count
    ldr r7, =(SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET)                 // Address of SIO_INTERP0 registers

2:
    ldmia r1!, {r3}                                                 // r3 = *(src++)
    str r3, [r7, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]        // SIO_INTERP0_ACCUM0 = r3
    ldr r6, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]    // r6 = &palette256[8*(r3>>24)]
    ldr r5, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE1_OFFSET)]    // r5 = &palette256[8*((r3>>16)&0xFF)]
    lsl r3, #16                                                     // r3 <<= 16
    str r3, [r7, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]        // SIO_INTERP0_ACCUM0 = r3
    ldr r4, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE1_OFFSET)]    // r3 = &palette256[8*((r3>>16)&0xFF)]
    ldr r3, [r4]                                                    // Copy 8 pixels from *r4 to *r0
    str r3, [r0]
    ldr r3, [r4, #4]
    str r3, [r0, #4]
    ldr r3, [r4, #8]
    str r3, [r0, #8]
    ldr r3, [r4, #12]
    str r3, [r0, #12]
    ldr r4, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]    // r4 = &palette256[8*(r3>>24)]
    ldr r3, [r4]                                                    // Copy 8 pixels from *r4 to *(r0+16)
    str r3, [r0, #16]
    ldr r3, [r4, #4]
    str r3, [r0, #20]
    ldr r3, [r4, #8]
    str r3, [r0, #24]
    ldr r3, [r4, #12]
    str r3, [r0, #28]
    ldr r3, [r5]                                                    // Copy 8 pixels from *r5 to *(r0+32)
    str r3, [r0, #32]
    ldr r3, [r5, #4]
    str r3, [r0, #36]
    ldr r3, [r5, #8]
    str r3, [r0, #40]
    ldr r3, [r5, #12]
    str r3, [r0, #44]
    ldr r3, [r6]                                                    // Copy 8 pixels from *r6 to *(r0+48)
    str r3, [r0, #48]
    ldr r3, [r6, #4]
    str r3, [r0, #52]
    ldr r3, [r6, #8]
    str r3, [r0, #56]
    ldr r3, [r6, #12]
    str r3, [r0, #60]
    add r0, #64                                                     // Increment destination by 32 pixels
    cmp r1, r2                                                      // Compare (r1 - r2)
    blo 2b                                                          // Repeat if r1 < r2 (= &src[count])

    pop {r4-r7, pc}                                                 // Restore registers and exit

    // Convert a line of framebuffer to 16-bit pixel values
    // Each byte becomes 4 pixel values (8 bytes)
// setup: note palette is 256 entries of 4 pixels (8 bytes); i.e. all pixel combinations for 1 byte
//
// interp_config c = interp_default_config();
// interp_config_set_shift(&c, 21);
// interp_config_set_mask(&c, 3, 10);
// interp_set_config(interp0, 0, &c);
// interp_config_set_shift(&c, 13);
// interp_config_set_cross_input(&c, true);
// interp_set_config(interp0, 1, &c);
// interp_set_base(interp0, 0, (uintptr_t)palette256);
// interp_set_base(interp0, 1, (uintptr_t)palette256);

// dest - pixel buffer 16bpp
// src - source pixel buffer 1-4bpp (must be 32 bit aligned)
// count - is number of bytes to convert (must be a multiple of 4)
// extern void convert_from_pal16_4(uint32_t *dest, uint8_t *src, uint count);
    // r0 = &dest
    // r1 = &src
    // r2 = count

.global convert_from_pal16_4
.type convert_from_pal16_4,%function
.thumb_func
convert_from_pal16_4:
    push {r4-r7, lr}                                                // Save registers
    add r2, r1                                                      // r2 = &src + count
    ldr r7, =(SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET)                 // Address of SIO_INTERP0 registers

3:
    ldmia r1!, {r3}                                                 // r3 = *(src++)
    str r3, [r7, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]        // SIO_INTERP0_ACCUM0 = r3
    ldr r6, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]    // r6 = &palette256[8*(r3>>24)]
    ldr r5, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE1_OFFSET)]    // r5 = &palette256[8*((r3>>16)&0xFF)]
    lsl r3, #16                                                     // r3 <<= 16
    str r3, [r7, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]        // SIO_INTERP0_ACCUM0 = r3
    ldr r4, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE1_OFFSET)]    // r4 = &palette256[8*((r4>>16)&0xFF)]
    ldr r3, [r4]                                                    // Copy 4 pixels from *r4 to *r0
    str r3, [r0]
    ldr r3, [r4, #4]
    str r3, [r0, #4]
    ldr r4, [r7, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]    // r4 = &palette256[8*(r3>>24)]
    ldr r3, [r4]                                                    // Copy 4 pixels from *r4 to *(r0+16)
    str r3, [r0, #8]
    ldr r3, [r4, #4]
    str r3, [r0, #12]
    ldr r3, [r5]                                                    // Copy 4 pixels from *r5 to *(r0+32)
    str r3, [r0, #16]
    ldr r3, [r5, #4]
    str r3, [r0, #20]
    ldr r3, [r6]                                                    // Copy 4 pixels from *r6 to *(r0+48)
    str r3, [r0, #24]
    ldr r3, [r6, #4]
    str r3, [r0, #28]
    add r0, #32                                                     // Increment destination by 32 pixels
    cmp r1, r2                                                      // Compare (r1 - r2)
    blo 3b                                                          // Repeat if r1 < r2 (= &src[count])

    pop {r4-r7, pc}                                                 // Restore registers and exit
