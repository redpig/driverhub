// Copyright 2024 The DriverHub Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Memory allocator for loaded module sections.
//!
//! On host Linux, uses `mmap` with RWX permissions. On Fuchsia, this would
//! be replaced with VMO-backed allocations.

use std::ptr;

/// Function pointer type for external memory allocation (C++ callback).
/// Takes size in bytes, returns a pointer to RWX memory or null on failure.
pub type AllocFn = unsafe extern "C" fn(size: usize) -> *mut u8;

/// Function pointer type for external memory deallocation (C++ callback).
/// Takes the pointer and size returned by the matching AllocFn.
pub type DeallocFn = unsafe extern "C" fn(ptr: *mut u8, size: usize);

/// A block of executable memory allocated for module sections.
pub struct MemoryAllocation {
    pub base: *mut u8,
    pub size: usize,
    /// If set, use the external deallocator instead of munmap.
    dealloc_fn: Option<DeallocFn>,
}

impl MemoryAllocation {
    /// Release the allocation. After this call, `base` is null.
    pub fn release(&mut self) {
        if !self.base.is_null() && self.size > 0 {
            if let Some(dealloc) = self.dealloc_fn {
                unsafe { dealloc(self.base, self.size) };
            } else {
                unsafe {
                    libc::munmap(self.base as *mut libc::c_void, self.size);
                }
            }
            self.base = ptr::null_mut();
            self.size = 0;
        }
    }
}

impl Drop for MemoryAllocation {
    fn drop(&mut self) {
        self.release();
    }
}

// The allocation holds a raw pointer to mmap'd memory. It's only accessed
// by the loader which runs single-threaded per module load.
unsafe impl Send for MemoryAllocation {}

/// Allocate RWX memory for module sections using `mmap`.
///
/// On x86_64, uses `MAP_32BIT` to keep allocations in the low 2GB
/// for `R_X86_64_32` relocations.
pub fn mmap_allocate(size: usize) -> Option<MemoryAllocation> {
    let mut flags = libc::MAP_PRIVATE | libc::MAP_ANONYMOUS;

    #[cfg(target_arch = "x86_64")]
    {
        flags |= libc::MAP_32BIT;
    }

    let ptr = unsafe {
        libc::mmap(
            ptr::null_mut(),
            size,
            libc::PROT_READ | libc::PROT_WRITE | libc::PROT_EXEC,
            flags,
            -1,
            0,
        )
    };

    if ptr == libc::MAP_FAILED {
        eprintln!("driverhub: mmap failed for {} bytes", size);
        None
    } else {
        Some(MemoryAllocation {
            base: ptr as *mut u8,
            size,
            dealloc_fn: None,
        })
    }
}

/// Allocate memory using an external C++ allocator callback.
pub fn external_allocate(
    size: usize,
    alloc_fn: AllocFn,
    dealloc_fn: DeallocFn,
) -> Option<MemoryAllocation> {
    let ptr = unsafe { alloc_fn(size) };
    if ptr.is_null() {
        eprintln!("driverhub: external allocator failed for {} bytes", size);
        None
    } else {
        Some(MemoryAllocation {
            base: ptr,
            size,
            dealloc_fn: Some(dealloc_fn),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn allocate_and_release() {
        let mut alloc = mmap_allocate(4096).expect("mmap failed");
        assert!(!alloc.base.is_null());
        assert_eq!(alloc.size, 4096);

        // Write to the allocation to verify it's accessible.
        unsafe {
            ptr::write(alloc.base, 0x42u8);
            assert_eq!(ptr::read(alloc.base), 0x42u8);
        }

        alloc.release();
        assert!(alloc.base.is_null());
        assert_eq!(alloc.size, 0);
    }

    #[test]
    fn drop_releases() {
        let alloc = mmap_allocate(4096).expect("mmap failed");
        let _base = alloc.base; // Just verify it doesn't leak on drop.
        drop(alloc);
    }
}
