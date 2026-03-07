// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Minimal arm64 QEMU boot shim for Fuchsia.
//
// QEMU's -machine virt loads a Linux arm64 Image and passes:
//   x0 = FDT (device tree) physical address
//   x1 = 0
//   x2 = 0
//   x3 = 0
//
// Fuchsia's physboot (KERNEL_ARM64 ZBI item) expects:
//   x0 = pointer to the data ZBI (everything after the kernel item)
//
// This shim:
//   1. Follows the Linux arm64 Image header format so QEMU loads it
//   2. Parses the FDT to find the initrd (which is the full Fuchsia ZBI)
//   3. Finds the KERNEL_ARM64 item in the ZBI
//   4. Computes the data ZBI start (after the kernel item)
//   5. Jumps to the kernel entry point with x0 = data ZBI address
//
// The ZBI passed as -initrd contains:
//   [ZBI container header (64 bytes)]
//   [KERNEL_ARM64 item header (32 bytes) + kernel payload]
//   [data items...]

.section .text
.global _start

// --- Linux arm64 Image header (64 bytes) ---
// See: Documentation/arm64/booting.rst
_start:
    b       real_start          // code0: branch to real code
    .long   0                   // code1: unused
    .quad   0                   // text_offset (0 = any alignment)
    .quad   _end - _start       // image_size
    .quad   0x0a                // flags: LE, 4K pages, any placement
    .quad   0                   // res2
    .quad   0                   // res3
    .quad   0                   // res4
    .long   0x644d5241          // magic: "ARM\x64"
    .long   0                   // res5 (PE header offset, 0 = none)

// --- Constants ---
.equ FDT_MAGIC,        0xd00dfeed
.equ FDT_BEGIN_NODE,    0x00000001
.equ FDT_END_NODE,      0x00000002
.equ FDT_PROP,          0x00000003
.equ FDT_END,           0x00000009

// ZBI constants
.equ ZBI_CONTAINER_MAGIC, 0x868cf7e6
.equ ZBI_ITEM_MAGIC,      0xb5781729
.equ ZBI_TYPE_KERNEL_ARM64, 0x00415241  // "ARA\0" (ZBI_TYPE_KERNEL_PREFIX | ZBI_TYPE_KERNEL_ARM64)

real_start:
    // x0 = FDT address from QEMU
    // Save FDT address
    mov     x19, x0

    // --- Parse FDT to find initrd ---
    // FDT header (big-endian):
    //   +0x00: magic (0xd00dfeed)
    //   +0x04: totalsize
    //   +0x08: off_dt_struct
    //   +0x0C: off_dt_strings
    //   +0x28: size_dt_strings

    // Verify FDT magic
    ldr     w1, [x19]
    rev     w1, w1              // big-endian to little-endian
    mov     w2, #0xfeed
    movk    w2, #0xd00d, lsl #16
    cmp     w1, w2
    b.ne    halt

    // Get structure block offset
    ldr     w20, [x19, #0x08]
    rev     w20, w20
    add     x20, x19, x20      // x20 = dt_struct pointer

    // Get strings block offset
    ldr     w21, [x19, #0x0C]
    rev     w21, w21
    add     x21, x19, x21      // x21 = dt_strings pointer

    // Walk the FDT structure to find "linux,initrd-start" and "linux,initrd-end"
    mov     x22, #0             // initrd_start
    mov     x23, #0             // initrd_end

walk_fdt:
    ldr     w1, [x20]
    rev     w1, w1
    add     x20, x20, #4

    cmp     w1, #FDT_BEGIN_NODE
    b.eq    skip_node_name

    cmp     w1, #FDT_END_NODE
    b.eq    walk_fdt

    cmp     w1, #FDT_PROP
    b.eq    handle_prop

    cmp     w1, #FDT_END
    b.eq    fdt_done

    // NOP or unknown token — skip
    b       walk_fdt

skip_node_name:
    // Skip null-terminated node name, then align to 4 bytes
1:  ldrb    w1, [x20], #1
    cbnz    w1, 1b
    // Align x20 up to 4 bytes
    add     x20, x20, #3
    and     x20, x20, #~3
    b       walk_fdt

handle_prop:
    // Property: struct { u32 len; u32 nameoff; u8 data[len]; }
    ldr     w4, [x20]          // len (big-endian)
    rev     w4, w4
    ldr     w5, [x20, #4]     // nameoff (big-endian)
    rev     w5, w5
    add     x20, x20, #8      // skip len + nameoff

    // x6 = property name string
    add     x6, x21, x5

    // Check if name is "linux,initrd-start"
    adr     x7, str_initrd_start
    bl      strcmp_simple
    cbz     w0, found_initrd_start

    // Check if name is "linux,initrd-end"
    adr     x7, str_initrd_end
    bl      strcmp_simple
    cbz     w0, found_initrd_end

skip_prop_data:
    // Skip property data (len bytes), align to 4
    add     x20, x20, x4
    add     x20, x20, #3
    and     x20, x20, #~3
    b       walk_fdt

found_initrd_start:
    // Read value (big-endian, 4 or 8 bytes)
    cmp     w4, #8
    b.eq    2f
    ldr     w22, [x20]
    rev     w22, w22
    b       skip_prop_data
2:  ldr     x22, [x20]
    rev     x22, x22
    b       skip_prop_data

found_initrd_end:
    cmp     w4, #8
    b.eq    2f
    ldr     w23, [x20]
    rev     w23, w23
    b       skip_prop_data
2:  ldr     x23, [x20]
    rev     x23, x23
    b       skip_prop_data

fdt_done:
    // x22 = initrd_start, x23 = initrd_end
    // Verify we found initrd
    cbz     x22, halt
    cbz     x23, halt

    // --- Parse ZBI to find KERNEL_ARM64 and data ZBI ---
    // ZBI container header (32 bytes):
    //   +0x00: type (u32) = ZBI_TYPE_CONTAINER
    //   +0x04: length (u32) = total payload length
    //   +0x08: extra (u32) = ZBI_CONTAINER_MAGIC
    //   +0x0C: flags (u32)
    //   +0x10: reserved0 (u32)
    //   +0x14: reserved1 (u32)
    //   +0x18: magic (u32) = ZBI_ITEM_MAGIC
    //   +0x1C: crc32 (u32)
    //
    // Each item header (32 bytes):
    //   +0x00: type (u32)
    //   +0x04: length (u32)
    //   +0x08: extra (u32)
    //   +0x0C: flags (u32)
    //   +0x10: reserved0 (u32)
    //   +0x14: reserved1 (u32)
    //   +0x18: magic (u32) = ZBI_ITEM_MAGIC
    //   +0x1C: crc32 (u32)

    mov     x24, x22            // x24 = ZBI start (initrd)

    // Verify ZBI container magic
    ldr     w1, [x24, #0x08]   // extra field
    mov     w2, #0xf7e6
    movk    w2, #0x868c, lsl #16
    cmp     w1, w2
    b.ne    halt

    // Skip container header (32 bytes), now at first item
    add     x25, x24, #32      // x25 = first item

    // Check if first item is KERNEL_ARM64
    ldr     w1, [x25]          // type
    mov     w2, #0x524b
    movk    w2, #0x384e, lsl #16  // ZBI_TYPE_KERNEL_ARM64 = 0x384e524b
    cmp     w1, w2
    b.ne    halt                // First item must be kernel

    // Get kernel item payload length
    ldr     w26, [x25, #0x04]  // length of kernel payload

    // Kernel entry point is at start of payload (after 32-byte item header)
    add     x27, x25, #32      // x27 = kernel entry point (physboot)

    // Data ZBI = everything after the kernel item
    // data_zbi_start = kernel_item + 32 (header) + align_up(length, 8)
    add     x28, x26, #7
    and     x28, x28, #~7      // aligned length
    add     x28, x25, #32      // + header size
    add     x28, x28, x26      // + payload length = start of next item
    // But actually, we need to construct a valid ZBI container for the data items.
    // physboot expects x0 to point to a valid ZBI container.
    //
    // We'll modify the container header in-place: adjust the length to exclude
    // the kernel item, and copy the container header to just before the data items.

    // Calculate data items start and data length
    // data_start = kernel_item_start + 32 (item header) + aligned_payload_length
    add     x0, x26, #7
    and     x0, x0, #~7
    add     x0, x0, #32        // item header + aligned payload = total kernel item size
    add     x0, x25, x0        // x0 = start of data items

    // We need to put a ZBI container header before the data items.
    // Back up 32 bytes from the data items start to overwrite the end of
    // the kernel payload (which we no longer need).
    sub     x1, x0, #32        // x1 = where we'll write the new container header

    // Calculate data payload length = total container length - kernel item size
    ldr     w2, [x24, #0x04]   // original container payload length
    sub     x3, x0, x25        // kernel item total size (64-bit)
    sub     w2, w2, w3         // data payload length

    // Write new ZBI container header at x1
    mov     w4, #0x4f42         // ZBI_TYPE_CONTAINER = 0x45544f42
    movk    w4, #0x4554, lsl #16
    str     w4, [x1, #0x00]    // type
    str     w2, [x1, #0x04]    // length (data payload)
    mov     w4, #0xf7e6
    movk    w4, #0x868c, lsl #16
    str     w4, [x1, #0x08]    // extra = ZBI_CONTAINER_MAGIC
    mov     w4, #0x0001
    lsl     w4, w4, #16        // flags = 0x00010000 (ZBI_FLAGS_VERSION)
    str     w4, [x1, #0x0C]    // flags
    str     wzr, [x1, #0x10]   // reserved0
    str     wzr, [x1, #0x14]   // reserved1
    mov     w4, #0x1729
    movk    w4, #0xb578, lsl #16
    str     w4, [x1, #0x18]    // magic = ZBI_ITEM_MAGIC
    str     wzr, [x1, #0x1C]   // crc32

    // x1 = data ZBI container, x27 = kernel entry
    mov     x0, x1              // x0 = data ZBI for physboot

    // Clean up: zero other registers as physboot may expect
    mov     x1, #0
    mov     x2, #0
    mov     x3, #0

    // Jump to physboot!
    br      x27

halt:
    wfe
    b       halt

// --- Simple string compare (x6 vs x7), returns 0 if equal ---
strcmp_simple:
    stp     x8, x9, [sp, #-16]!
1:  ldrb    w8, [x6], #1
    ldrb    w9, [x7], #1
    cmp     w8, w9
    b.ne    2f
    cbz     w8, 3f             // both null = match
    b       1b
2:  mov     w0, #1             // not equal
    ldp     x8, x9, [sp], #16
    ret
3:  mov     w0, #0             // equal
    ldp     x8, x9, [sp], #16
    ret

// --- String constants ---
.balign 4
str_initrd_start:
    .asciz "linux,initrd-start"
.balign 4
str_initrd_end:
    .asciz "linux,initrd-end"

.balign 4
_end:
    .quad 0                     // end marker for image_size calculation
