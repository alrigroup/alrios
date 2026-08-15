// Go Implementation with Linear Memory Slice
package main

var memoryBuffer = make([]uint32, 262144) // 1MB Memory Slice

func compute(iters uint32) uint32 {
    var acc uint32 = 0x12345678
    for i := uint32(0); i < iters; i++ {
        idx := i & 0x3FFFF
        val := memoryBuffer[idx]
        acc = ((acc ^ (i + 0x9E3779B9) ^ val) * 1664525) + 1013904223
        memoryBuffer[idx] = acc
    }
    return acc
}

func main() {}
