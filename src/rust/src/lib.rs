#![no_std]

use core::panic::PanicInfo;

unsafe extern "C" {
    fn avlos_arch_halt() -> !;
}

/// FNV-1a 64-bit checksum exposed through the C ABI.
///
/// The caller must pass a valid byte range of length bytes. A null pointer
/// is accepted only when length is zero.
#[no_mangle]
pub unsafe extern "C" fn avlos_rust_fnv1a64(data: *const u8, length: usize) -> u64 {
    if length == 0 {
        return 0xcbf29ce484222325;
    }
    if data.is_null() {
        return 0;
    }

    let mut hash = 0xcbf29ce484222325_u64;
    let mut index = 0;
    while index < length {
        let byte = unsafe { *data.add(index) };
        hash ^= u64::from(byte);
        hash = hash.wrapping_mul(0x100000001b3);
        index += 1;
    }
    hash
}

#[panic_handler]
fn panic(_info: &PanicInfo<'_>) -> ! {
    unsafe { avlos_arch_halt() }
}

