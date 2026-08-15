// Rust Implementation with Zero-Cost Linear Memory Buffer
static mut MEMORY_BUFFER: [u32; 262144] = [0; 262144]; // 1MB Memory Array

#[no_mangle]
pub extern "C" fn compute(iters: u32) -> u32 {
    let mut acc: u32 = 0x12345678;
    unsafe {
        for i in 0..iters {
            let idx = (i & 0x3FFFF) as usize;
            let val = *MEMORY_BUFFER.get_unchecked(idx);
            let term = (acc ^ i.wrapping_add(0x9E3779B9) ^ val).wrapping_mul(1664525);
            acc = term.wrapping_add(1013904223);
            *MEMORY_BUFFER.get_unchecked_mut(idx) = acc;
        }
    }
    acc
}
