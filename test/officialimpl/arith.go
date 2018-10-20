// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// This file provides Go implementations of elementary multi-precision
// arithmetic operations on word vectors. Needed for platforms without
// assembly implementations of these routines.

package big

import "math/bits"

// A Word represents a single digit of a multi-precision unsigned integer.
type Word uint

const (
	_S = _W / 8 // word size in bytes

	_W = bits.UintSize // word size in bits
	_B = 1 << _W       // digit base
	_M = _B - 1        // digit mask

	_W2 = _W / 2   // half word size in bits
	_B2 = 1 << _W2 // half digit base
	_M2 = _B2 - 1  // half digit mask
)

// ----------------------------------------------------------------------------
// Elementary operations on words
//
// These operations are used by the vector operations below.

// z1<<_W + z0 = x+y+c, with c == 0 or 1
func addWW_g(x, y, c Word) (z1, z0 Word) {
	yc := y + c
	z0 = x + yc
	if z0 < x || yc < y {
		z1 = 1
	}
	return
}

// z1<<_W + z0 = x-y-c, with c == 0 or 1
func subWW_g(x, y, c Word) (z1, z0 Word) {
	yc := y + c
	z0 = x - yc
	if z0 > x || yc < y {
		z1 = 1
	}
	return
}

// z1<<_W + z0 = x*y
// Adapted from Warren, Hacker's Delight, p. 132.
func mulWW_g(x, y Word) (z1, z0 Word) {
	x0 := x & _M2
	x1 := x >> _W2
	y0 := y & _M2
	y1 := y >> _W2
	w0 := x0 * y0
	t := x1*y0 + w0>>_W2
	w1 := t & _M2
	w2 := t >> _W2
	w1 += x0 * y1
	z1 = x1*y1 + w2 + w1>>_W2
	z0 = x * y
	return
}

// z1<<_W + z0 = x*y + c
func mulAddWWW_g(x, y, c Word) (z1, z0 Word) {
	z1, zz0 := mulWW_g(x, y)
	if z0 = zz0 + c; z0 < zz0 {
		z1++
	}
	return
}

// nlz returns the number of leading zeros in x.
// Wraps bits.LeadingZeros call for convenience.
func nlz(x Word) uint {
	return uint(bits.LeadingZeros(uint(x)))
}

// q = (u1<<_W + u0 - r)/y
// Adapted from Warren, Hacker's Delight, p. 152.
func divWW_g(u1, u0, v Word) (q, r Word) {
	if u1 >= v {
		return 1<<_W - 1, 1<<_W - 1
	}

	s := nlz(v)
	v <<= s

	vn1 := v >> _W2
	vn0 := v & _M2
	un32 := u1<<s | u0>>(_W-s)
	un10 := u0 << s
	un1 := un10 >> _W2
	un0 := un10 & _M2
	q1 := un32 / vn1
	rhat := un32 - q1*vn1

	for q1 >= _B2 || q1*vn0 > _B2*rhat+un1 {
		q1--
		rhat += vn1
		if rhat >= _B2 {
			break
		}
	}

	un21 := un32*_B2 + un1 - q1*v
	q0 := un21 / vn1
	rhat = un21 - q0*vn1

	for q0 >= _B2 || q0*vn0 > _B2*rhat+un0 {
		q0--
		rhat += vn1
		if rhat >= _B2 {
			break
		}
	}

	return q1*_B2 + q0, (un21*_B2 + un0 - q0*v) >> s
}

// Keep for performance debugging.
// Using addWW_g is likely slower.
const use_addWW_g = false

// The resulting carry c is either 0 or 1.
func addVV_g(z, x, y []Word) (c Word) {
	if use_addWW_g {
		for i := range z {
			c, z[i] = addWW_g(x[i], y[i], c)
		}
		return
	}

	for i, xi := range x[:len(z)] {
		yi := y[i]
		zi := xi + yi + c
		z[i] = zi
		// see "Hacker's Delight", section 2-12 (overflow detection)
		c = (xi&yi | (xi|yi)&^zi) >> (_W - 1)
	}
	return
}

// The resulting carry c is either 0 or 1.
func subVV_g(z, x, y []Word) (c Word) {
	if use_addWW_g {
		for i := range z {
			c, z[i] = subWW_g(x[i], y[i], c)
		}
		return
	}

	for i, xi := range x[:len(z)] {
		yi := y[i]
		zi := xi - yi - c
		z[i] = zi
		// see "Hacker's Delight", section 2-12 (overflow detection)
		c = (yi&^xi | (yi|^xi)&zi) >> (_W - 1)
	}
	return
}

// The resulting carry c is either 0 or 1.
func addVW_g(z, x []Word, y Word) (c Word) {
	if use_addWW_g {
		c = y
		for i := range z {
			c, z[i] = addWW_g(x[i], c, 0)
		}
		return
	}

	c = y
	for i, xi := range x[:len(z)] {
		zi := xi + c
		z[i] = zi
		c = xi &^ zi >> (_W - 1)
	}
	return
}

func subVW_g(z, x []Word, y Word) (c Word) {
	if use_addWW_g {
		c = y
		for i := range z {
			c, z[i] = subWW_g(x[i], c, 0)
		}
		return
	}

	c = y
	for i, xi := range x[:len(z)] {
		zi := xi - c
		z[i] = zi
		c = (zi &^ xi) >> (_W - 1)
	}
	return
}

func mulAddVWW_g(z, x []Word, y, r Word) (c Word) {
	c = r
	for i := range z {
		c, z[i] = mulAddWWW_g(x[i], y, c)
	}
	return
}

// TODO(gri) Remove use of addWW_g here and then we can remove addWW_g and subWW_g.
func addMulVVW_g(z, x []Word, y Word) (c Word) {
	for i := range z {
		z1, z0 := mulAddWWW_g(x[i], y, z[i])
		c, z[i] = addWW_g(z0, c, 0)
		c += z1
	}
	return
}

func divWVW_g(z []Word, xn Word, x []Word, y Word) (r Word) {
	r = xn
	for i := len(z) - 1; i >= 0; i-- {
		z[i], r = divWW_g(r, x[i], y)
	}
	return
}
