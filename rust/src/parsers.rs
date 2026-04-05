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

/// Safe implementation of the 'ar' format numeric parser.
/// Faithfully reproduces the C version's behavior regarding whitespace and overflow.
pub fn common_atol(p: &[u8], base: u64) -> u64 {
    let mut it = p.iter();
    let mut char_cnt = p.len();

    // Skip leading whitespace
    let mut current = it.next();
    while let Some(&byte) = current {
        if byte == b' ' || byte == b'\t' {
            if char_cnt == 0 {
                return 0;
            }
            char_cnt -= 1;
            current = it.next();
        } else {
            break;
        }
    }

    if current.is_none() || char_cnt == 0 {
        return 0;
    }

    let mut l: u64 = 0;
    let limit = u64::MAX / base;
    let last_digit_limit = u64::MAX % base;

    while let Some(&byte) = current {
        if char_cnt == 0 {
            break;
        }

        if byte < b'0' {
            break;
        }
        let digit = (byte - b'0') as u64;
        if digit >= base {
            break;
        }

        // Check for overflow
        if l > limit || (l == limit && digit > last_digit_limit) {
            return u64::MAX;
        }

        l = (l * base) + digit;
        char_cnt -= 1;
        current = it.next();
    }

    l
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_atol8() {
        assert_eq!(common_atol(b"777", 8), 511);
        assert_eq!(common_atol(b"  10", 8), 8);
        assert_eq!(common_atol(b"9", 8), 0); // Invalid digit for base 8
    }

    #[test]
    fn test_atol10() {
        assert_eq!(common_atol(b"123", 10), 123);
        assert_eq!(common_atol(b"  456abc", 10), 456);
        assert_eq!(common_atol(b"18446744073709551615", 10), 18446744073709551615);
        assert_eq!(common_atol(b"18446744073709551616", 10), u64::MAX); // Overflow
    }
}
