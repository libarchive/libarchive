/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The libarchive-rust Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

pub mod parsers;

/// FFI Bridge for libarchive.
/// Exposes safe Rust implementations of numeric parsers to the C library.

#[no_mangle]
pub unsafe extern "C" fn rust_ar_atol8(p_raw: *const u8, char_cnt: usize) -> u64 {
    // SAFETY: The length 'char_cnt' is verified by the C caller to be greater than 0.
    // The memory is owned by the C stack and remains valid for the duration of this
    // synchronous call. We wrap it in a slice immediately to enforce bounds checking
    // in Rust, eliminating the raw pointer arithmetic present in the C implementation.
    let p = std::slice::from_raw_parts(p_raw, char_cnt);
    parsers::common_atol(p, 8)
}

#[no_mangle]
pub unsafe extern "C" fn rust_ar_atol10(p_raw: *const u8, char_cnt: usize) -> u64 {
    // SAFETY: The length 'char_cnt' is verified by the C caller to be greater than 0.
    // The memory is owned by the C stack and remains valid for the duration of this
    // synchronous call. We wrap it in a slice immediately to enforce bounds checking
    // in Rust, eliminating the raw pointer arithmetic present in the C implementation.
    let p = std::slice::from_raw_parts(p_raw, char_cnt);
    parsers::common_atol(p, 10)
}
