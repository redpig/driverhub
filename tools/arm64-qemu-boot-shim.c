// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Minimal arm64 QEMU boot shim for Fuchsia.
//
// Bridges the Linux arm64 boot protocol (FDT in x0) to Fuchsia's physboot
// (data ZBI in x0). See arm64-qemu-boot-shim.S for the detailed explanation.
//
// Build: clang --target=aarch64-none-elf -ffreestanding -nostdlib -O2 ...

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef uint64_t uintptr_t;

// PL011 UART on QEMU virt is at 0x09000000
#define UART_BASE ((volatile uint32_t*)0x09000000UL)
#define UART_DR   0        // Data register (offset 0x00)
#define UART_FR   (0x18/4) // Flag register (offset 0x18)
#define UART_IBRD (0x24/4) // Integer baud rate (offset 0x24)
#define UART_FBRD (0x28/4) // Fractional baud rate (offset 0x28)
#define UART_LCR  (0x2C/4) // Line control (offset 0x2C)
#define UART_CR   (0x30/4) // Control register (offset 0x30)
#define UART_IMSC (0x38/4) // Interrupt mask (offset 0x38)
#define UART_ICR  (0x44/4) // Interrupt clear (offset 0x44)

static void uart_init(void) {
    // Disable UART first
    UART_BASE[UART_CR] = 0;
    // Clear all interrupts
    UART_BASE[UART_ICR] = 0x7FF;
    // Set baud rate (doesn't matter for QEMU but required for proper init)
    UART_BASE[UART_IBRD] = 1;
    UART_BASE[UART_FBRD] = 0;
    // 8 bits, no parity, 1 stop bit, FIFO enabled
    UART_BASE[UART_LCR] = (3 << 5) | (1 << 4);  // WLEN=8bit, FEN=1
    // Disable all interrupts
    UART_BASE[UART_IMSC] = 0;
    // Enable UART, TX, RX
    UART_BASE[UART_CR] = (1 << 0) | (1 << 8) | (1 << 9);  // UARTEN | TXE | RXE
}

static void uart_putc(char c) {
    // Wait for TX FIFO not full (bit 5 of FR = TXFF)
    while (UART_BASE[UART_FR] & (1 << 5))
        ;
    UART_BASE[UART_DR] = (uint32_t)c;
}

static void uart_puts(const char* s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static void uart_hex(uint64_t val) {
    const char hex[] = "0123456789abcdef";
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uart_putc(hex[(val >> i) & 0xf]);
    }
}

// Big-endian to host (little-endian)
static inline uint32_t be32(const void* p) {
    const uint8_t* b = (const uint8_t*)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}

static inline uint64_t be64(const void* p) {
    return ((uint64_t)be32(p) << 32) | be32((const uint8_t*)p + 4);
}

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return *a == *b;
}

// FDT constants
#define FDT_MAGIC       0xd00dfeed
#define FDT_BEGIN_NODE  1
#define FDT_END_NODE    2
#define FDT_PROP        3
#define FDT_NOP         4
#define FDT_END         9

// ZBI constants
#define ZBI_TYPE_CONTAINER    0x544f4f42u
#define ZBI_TYPE_KERNEL_ARM64 0x384e524bu
#define ZBI_CONTAINER_MAGIC   0x868cf7e6u
#define ZBI_ITEM_MAGIC        0xb5781729u

typedef struct {
    uint32_t type;
    uint32_t length;
    uint32_t extra;
    uint32_t flags;
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t magic;
    uint32_t crc32;
} zbi_header_t;

typedef void (*physboot_entry_t)(const void* data_zbi);

// Parse FDT to find chosen/linux,initrd-start and linux,initrd-end
static void find_initrd(const void* fdt, uint64_t* start, uint64_t* end) {
    *start = 0;
    *end = 0;

    if (be32(fdt) != FDT_MAGIC) {
        uart_puts("boot-shim: bad FDT magic\n");
        return;
    }

    uint32_t off_struct = be32((const uint8_t*)fdt + 0x08);
    uint32_t off_strings = be32((const uint8_t*)fdt + 0x0c);

    uart_puts("boot-shim: FDT struct_off=");
    uart_hex(off_struct);
    uart_puts(" strings_off=");
    uart_hex(off_strings);
    uart_puts("\n");

    const uint8_t* p = (const uint8_t*)fdt + off_struct;
    const char* strings = (const char*)fdt + off_strings;
    int in_chosen = 0;
    int node_count = 0;

    while (1) {
        uint32_t token = be32(p);
        p += 4;

        switch (token) {
        case FDT_BEGIN_NODE: {
            const char* name = (const char*)p;
            node_count++;
            if (node_count <= 20) {
                uart_puts("boot-shim: node: ");
                uart_puts(name);
                uart_puts("\n");
            }
            if (streq(name, "chosen")) in_chosen = 1;
            // Skip name + null + align to 4
            while (*p) p++;
            p++;
            p = (const uint8_t*)(((uintptr_t)p + 3) & ~3UL);
            break;
        }
        case FDT_END_NODE:
            in_chosen = 0;
            break;
        case FDT_PROP: {
            uint32_t len = be32(p);
            uint32_t nameoff = be32(p + 4);
            p += 8;
            const char* prop_name = strings + nameoff;

            if (in_chosen) {
                if (streq(prop_name, "linux,initrd-start")) {
                    *start = (len == 8) ? be64(p) : be32(p);
                } else if (streq(prop_name, "linux,initrd-end")) {
                    *end = (len == 8) ? be64(p) : be32(p);
                }
            }
            p += len;
            p = (const uint8_t*)(((uintptr_t)p + 3) & ~3UL);
            break;
        }
        case FDT_NOP:
            break;
        case FDT_END:
            return;
        default:
            return;
        }
    }
}

void shim_main(void* fdt) {
    uart_init();
    uart_puts("\nboot-shim: DriverHub arm64 QEMU boot shim\n");
    uart_puts("boot-shim: FDT at ");
    uart_hex((uint64_t)fdt);
    uart_puts("\n");

    // Find initrd (ZBI) from FDT
    uart_puts("boot-shim: parsing FDT...\n");

    // Debug: check stack pointer
    uint64_t sp_val;
    __asm__ volatile("mov %0, sp" : "=r"(sp_val));
    uart_puts("boot-shim: SP=");
    uart_hex(sp_val);
    uart_puts("\n");

    // Quick test: read FDT byte
    uart_puts("boot-shim: reading FDT[0]...\n");
    volatile uint8_t first_byte = *(volatile uint8_t*)fdt;
    uart_puts("boot-shim: FDT[0]=");
    uart_hex(first_byte);
    uart_puts("\n");

    // Read magic manually
    const uint8_t* fb = (const uint8_t*)fdt;
    uart_puts("boot-shim: FDT bytes: ");
    uart_hex(fb[0]); uart_puts(" ");
    uart_hex(fb[1]); uart_puts(" ");
    uart_hex(fb[2]); uart_puts(" ");
    uart_hex(fb[3]); uart_puts("\n");

    uint32_t fdt_magic = ((uint32_t)fb[0] << 24) | ((uint32_t)fb[1] << 16) |
                         ((uint32_t)fb[2] << 8)  | (uint32_t)fb[3];
    uart_puts("boot-shim: FDT magic=");
    uart_hex(fdt_magic);
    uart_puts("\n");

    uint64_t initrd_start = 0, initrd_end = 0;
    find_initrd(fdt, &initrd_start, &initrd_end);

    if (!initrd_start || !initrd_end) {
        uart_puts("boot-shim: ERROR: initrd not found in FDT\n");
        goto halt;
    }

    uart_puts("boot-shim: initrd at ");
    uart_hex(initrd_start);
    uart_puts(" - ");
    uart_hex(initrd_end);
    uart_puts("\n");

    // Verify ZBI container
    {
        const zbi_header_t* container = (const zbi_header_t*)initrd_start;
        if (container->type != ZBI_TYPE_CONTAINER ||
            container->extra != ZBI_CONTAINER_MAGIC) {
            uart_puts("boot-shim: ERROR: initrd is not a valid ZBI\n");
            goto halt;
        }

        uart_puts("boot-shim: ZBI container length=");
        uart_hex(container->length);
        uart_puts("\n");

        // First item should be KERNEL_ARM64
        const zbi_header_t* kernel_item =
            (const zbi_header_t*)(initrd_start + sizeof(zbi_header_t));

        if (kernel_item->type != ZBI_TYPE_KERNEL_ARM64) {
            uart_puts("boot-shim: ERROR: first ZBI item is not KERNEL_ARM64 (type=");
            uart_hex(kernel_item->type);
            uart_puts(")\n");
            goto halt;
        }

        uint32_t kernel_payload_len = kernel_item->length;
        uart_puts("boot-shim: kernel payload length=");
        uart_hex(kernel_payload_len);
        uart_puts("\n");

        uart_puts("boot-shim: computing addresses...\n");

        // The kernel payload starts with zbi_kernel_t:
        //   uint64_t entry;                // offset from ZBI container start
        //   uint64_t reserve_memory_size;
        const uint8_t* kernel_payload =
            (const uint8_t*)kernel_item + sizeof(zbi_header_t);

        // Read entry offset (little-endian uint64_t)
        uint64_t entry_offset = *(const uint64_t*)kernel_payload;
        uint64_t reserve_size = *(const uint64_t*)(kernel_payload + 8);

        // Entry point is offset from the ZBI container start
        const uint8_t* kernel_entry =
            (const uint8_t*)container + entry_offset;

        // DEBUG: Also try the _start at kernel payload offset 0x3ae18
        // (where mrs x9, CurrentEL is found)
        const uint8_t* alt_entry =
            (const uint8_t*)kernel_payload + 0x3ae18;

        uart_puts("boot-shim: entry_offset=");
        uart_hex(entry_offset);
        uart_puts(" reserve=");
        uart_hex(reserve_size);
        uart_puts("\n");
        uart_puts("boot-shim: entry=");
        uart_hex((uint64_t)kernel_entry);
        uart_puts("\n");
        uart_puts("boot-shim: alt_entry=");
        uart_hex((uint64_t)alt_entry);
        uart_puts("\n");

        // Data items start after kernel item (header + aligned payload)
        uint32_t aligned_len = (kernel_payload_len + 7) & ~7u;
        uint8_t* data_items =
            (uint8_t*)kernel_item + sizeof(zbi_header_t) + aligned_len;

        uart_puts("boot-shim: data_items=");
        uart_hex((uint64_t)data_items);
        uart_puts("\n");

        // Calculate data payload length
        uint32_t total_payload = container->length;
        uint32_t kernel_item_size = sizeof(zbi_header_t) + aligned_len;
        uint32_t data_payload_len = total_payload - kernel_item_size;

        uart_puts("boot-shim: data_payload_len=");
        uart_hex(data_payload_len);
        uart_puts("\n");

        // Write a new ZBI container header just before the data items.
        // We overwrite the last 32 bytes of the kernel payload (which we
        // no longer need since we've already located the entry point).
        zbi_header_t* data_container = (zbi_header_t*)(data_items - sizeof(zbi_header_t));

        // Check current exception level
        uint64_t current_el;
        __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
        uart_puts("boot-shim: CurrentEL=");
        uart_hex((current_el >> 2) & 3);
        uart_puts("\n");

        // Strategy: pass the FULL ZBI (with kernel item) as x0.
        // Physboot may need to reference the ZBI container and kernel item.
        const uint8_t* full_zbi = (const uint8_t*)container;

        uart_puts("boot-shim: using alt_entry (real _start) at ");
        uart_hex((uint64_t)alt_entry);
        uart_puts("\n");
        uart_puts("boot-shim: passing full ZBI at ");
        uart_hex((uint64_t)full_zbi);
        uart_puts("\n");

        // Verify alt_entry code
        const uint32_t* acode = (const uint32_t*)alt_entry;
        uart_puts("boot-shim: alt_entry code:\n");
        for (int i = 0; i < 4; i++) {
            uart_puts("  "); uart_hex(acode[i]); uart_puts("\n");
        }

        // DSB + ISB to ensure memory coherence
        __asm__ volatile("dsb sy\n" "isb\n" ::: "memory");

        // Re-enable UART before jumping — physboot may assume it's enabled
        uart_init();

        // Try BOTH entry points: first try official entry_offset with data ZBI,
        // then try alt_entry with full ZBI.
        // Use kernel_entry (the official zbi_kernel_t.entry offset)
        // and construct a proper in-place data ZBI.
        uint8_t* data_zbi = data_items - sizeof(zbi_header_t);
        volatile uint32_t* hdr32 = (volatile uint32_t*)data_zbi;
        hdr32[0] = ZBI_TYPE_CONTAINER;
        hdr32[1] = data_payload_len;
        hdr32[2] = ZBI_CONTAINER_MAGIC;
        hdr32[3] = 0x00010000;
        hdr32[4] = 0;
        hdr32[5] = 0;
        hdr32[6] = ZBI_ITEM_MAGIC;
        hdr32[7] = 0;

        uart_puts("boot-shim: data ZBI at ");
        uart_hex((uint64_t)data_zbi);
        uart_puts("\n");

        // Use alt_entry (real _start with CurrentEL check) with full ZBI.
        // The earlier test showed 1885 UART writes from this entry point.
        uart_puts("boot-shim: JUMPING to alt_entry=");
        uart_hex((uint64_t)alt_entry);
        uart_puts(" x0=full_zbi\n");
        __asm__ volatile(
            "mov x0, %0\n"
            "mov x1, xzr\n"
            "mov x2, xzr\n"
            "mov x3, xzr\n"
            "br %1\n"
            : : "r"(full_zbi), "r"(alt_entry)
            : "x0", "x1", "x2", "x3", "memory"
        );
    }

halt:
    uart_puts("boot-shim: halted\n");
    while (1) {
        __asm__ volatile("wfe");
    }
}
