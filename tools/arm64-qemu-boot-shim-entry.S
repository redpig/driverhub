// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Entry point for the arm64 QEMU boot shim.
// Provides the Linux arm64 Image header and sets up a stack.

.section .text.header, "ax"
.global _start

// --- Linux arm64 Image header (64 bytes) ---
_start:
    b       entry               // code0: branch to real code
    .long   0                   // code1
    .quad   0                   // text_offset (0 = position independent)
    .quad   _image_end - _start // image_size
    .quad   0x0a                // flags: LE, 4K pages, any placement
    .quad   0                   // res2
    .quad   0                   // res3
    .quad   0                   // res4
    .long   0x644d5241          // magic: "ARM\x64"
    .long   0                   // PE offset (0 = none)

.section .text, "ax"
.global entry
entry:
    // x0 = FDT address (from QEMU)

    // Set up a 16KB stack just below our load address.
    // QEMU loads at 0x40200000 on virt, so stack at 0x40200000 - 16.
    adr     x1, _start
    mov     sp, x1

    // Call C main
    bl      shim_main

    // Should not return, but halt if it does.
1:  wfe
    b       1b

.section .note, "a"
    // Empty section to mark end
.section .data
.global _image_end
_image_end:
    .quad 0
