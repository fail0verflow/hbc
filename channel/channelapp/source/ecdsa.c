// Copyright 2007-2009  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include <string.h>

#include <ogcsys.h>

#include "sha1.h"
#include "ecdsa.h"

#include "../config.h"

#ifdef ENABLE_UPDATES

// order of the addition group of points
static u8 ec_N[30] =
	"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x13\xe9\x74\xe7\x2f\x8a\x69\x22\x03\x1d\x26\x03\xcf\xe0\xd7";

// base point
static u8 ec_G[60] =
	"\x00\xfa\xc9\xdf\xcb\xac\x83\x13\xbb\x21\x39\xf1\xbb\x75\x5f"
	"\xef\x65\xbc\x39\x1f\x8b\x36\xf8\xf8\xeb\x73\x71\xfd\x55\x8b"
	"\x01\x00\x6a\x08\xa4\x19\x03\x35\x06\x78\xe5\x85\x28\xbe\xbf"
	"\x8a\x0b\xef\xf8\x67\xa7\xca\x36\x71\x6f\x7e\x01\xf8\x10\x52";

static const u8 square[16] =
	"\x00\x01\x04\x05\x10\x11\x14\x15\x40\x41\x44\x45\x50\x51\x54\x55";

static void bn_zero(u8 *d, u32 n)
{
	memset(d, 0, n);
}

static void bn_copy(u8 *d, const u8 *a, u32 n)
{
	memcpy(d, a, n);
}

static int bn_compare(const u8 *a, const u8 *b, u32 n)
{
	u32 i;

	for (i = 0; i < n; i++) {
		if (a[i] < b[i])
			return -1;
		if (a[i] > b[i])
			return 1;
	}

	return 0;
}

static void bn_sub_modulus(u8 *a, const u8 *N, u32 n)
{
	u32 i;
	u32 dig;
	u8 c;

	c = 0;
	for (i = n - 1; i < n; i--) {
		dig = N[i] + c;
		c = (a[i] < dig);
		a[i] -= dig;
	}
}

static void bn_add(u8 *d, const u8 *a, const u8 *b, const u8 *N, u32 n)
{
	u32 i;
	u32 dig;
	u8 c;

	c = 0;
	for (i = n - 1; i < n; i--) {
		dig = a[i] + b[i] + c;
		c = (dig >= 0x100);
		d[i] = dig;
	}

	if (c)
		bn_sub_modulus(d, N, n);

	if (bn_compare(d, N, n) >= 0)
		bn_sub_modulus(d, N, n);
}

static void bn_mul(u8 *d, const u8 *a, const u8 *b, const u8 *N, u32 n)
{
	u32 i;
	u8 mask;

	bn_zero(d, n);

	for (i = 0; i < n; i++)
		for (mask = 0x80; mask != 0; mask >>= 1) {
			bn_add(d, d, d, N, n);
			if ((a[i] & mask) != 0)
				bn_add(d, d, b, N, n);
		}
}

static void bn_exp(u8 *d, const u8 *a, const u8 *N, u32 n, const u8 *e, u32 en)
{
	u8 t[512];
	u32 i;
	u8 mask;

	bn_zero(d, n);
	d[n-1] = 1;
	for (i = 0; i < en; i++)
		for (mask = 0x80; mask != 0; mask >>= 1) {
			bn_mul(t, d, d, N, n);
			if ((e[i] & mask) != 0)
				bn_mul(d, t, a, N, n);
			else
				bn_copy(d, t, n);
		}
}

// only for prime N -- stupid but lazy, see if I care
static void bn_inv(u8 *d, const u8 *a, const u8 *N, u32 n)
{
	u8 t[512], s[512];

	bn_copy(t, N, n);
	bn_zero(s, n);
	s[n-1] = 2;
	bn_sub_modulus(t, s, n);
	bn_exp(d, a, N, n, t, n);
}

static void elt_copy(u8 *d, const u8 *a)
{
	memcpy(d, a, 30);
}

static void elt_zero(u8 *d)
{
	memset(d, 0, 30);
}

static int elt_is_zero(const u8 *d)
{
	u32 i;

	for (i = 0; i < 30; i++)
		if (d[i] != 0)
			return 0;

	return 1;
}

static void elt_add(u8 *d, u8 *a, const u8 *b)
{
	u32 i;

	for (i = 0; i < 30; i++)
		d[i] = a[i] ^ b[i];
}

static void elt_mul_x(u8 *d, u8 *a)
{
	u8 carry, x, y;
	u32 i;

	carry = a[0] & 1;

	x = 0;
	for (i = 0; i < 29; i++) {
		y = a[i + 1];
		d[i] = x ^ (y >> 7);
		x = y << 1;
	}
	d[29] = x ^ carry;

	d[20] ^= carry << 2;
}

static void elt_mul(u8 *d, u8 *a, u8 *b)
{
	u32 i, n;
	u8 mask;

	elt_zero(d);

	i = 0;
	mask = 1;
	for (n = 0; n < 233; n++) {
		elt_mul_x(d, d);

		if ((a[i] & mask) != 0)
			elt_add(d, d, b);

		mask >>= 1;
		if (mask == 0) {
			mask = 0x80;
			i++;
		}
	}
}

static void elt_square_to_wide(u8 *d, u8 *a)
{
	u32 i;

	for (i = 0; i < 30; i++) {
		d[2*i] = square[a[i] >> 4];
		d[2*i + 1] = square[a[i] & 15];
	}
}

static void wide_reduce(u8 *d)
{
	u32 i;
	u8 x;

	for (i = 0; i < 30; i++) {
		x = d[i];

		d[i + 19] ^= x >> 7;
		d[i + 20] ^= x << 1;

		d[i + 29] ^= x >> 1;
		d[i + 30] ^= x << 7;
	}

	x = d[30] & ~1;

	d[49] ^= x >> 7;
	d[50] ^= x << 1;

	d[59] ^= x >> 1;

	d[30] &= 1;
}

static void elt_square(u8 *d, u8 *a)
{
	u8 wide[60];

	elt_square_to_wide(wide, a);
	wide_reduce(wide);

	elt_copy(d, wide + 30);
}

static void itoh_tsujii(u8 *d, u8 *a, u8 *b, u32 j)
{
	u8 t[30];

	elt_copy(t, a);
	while (j--) {
		elt_square(d, t);
		elt_copy(t, d);
	}

	elt_mul(d, t, b);
}

static void elt_inv(u8 *d, u8 *a)
{
	u8 t[30];
	u8 s[30];

	itoh_tsujii(t, a, a, 1);
	itoh_tsujii(s, t, a, 1);
	itoh_tsujii(t, s, s, 3);
	itoh_tsujii(s, t, a, 1);
	itoh_tsujii(t, s, s, 7);
	itoh_tsujii(s, t, t, 14);
	itoh_tsujii(t, s, a, 1);
	itoh_tsujii(s, t, t, 29);
	itoh_tsujii(t, s, s, 58);
	itoh_tsujii(s, t, t, 116);
	elt_square(d, s);
}

static int point_is_zero(const u8 *p)
{
	return elt_is_zero(p) && elt_is_zero(p + 30);
}

static void point_double(u8 *r, u8 *p)
{
	u8 s[30], t[30];
	u8 *px, *py, *rx, *ry;

	px = p;
	py = p + 30;
	rx = r;
	ry = r + 30;

	if (elt_is_zero(px)) {
		elt_zero(rx);
		elt_zero(ry);

		return;
	}

	elt_inv(t, px);
	elt_mul(s, py, t);
	elt_add(s, s, px);

	elt_square(t, px);

	elt_square(rx, s);
	elt_add(rx, rx, s);
	rx[29] ^= 1;

	elt_mul(ry, s, rx);
	elt_add(ry, ry, rx);
	elt_add(ry, ry, t);
}

static void point_add(u8 *r, u8 *p, const u8 *q)
{
	u8 s[30], t[30], u[30];
	u8 *px, *py, *rx, *ry;
	const u8 *qx, *qy;

	px = p;
	py = p + 30;
	qx = q;
	qy = q + 30;
	rx = r;
	ry = r + 30;

	if (point_is_zero(p)) {
		elt_copy(rx, qx);
		elt_copy(ry, qy);
		return;
	}

	if (point_is_zero(q)) {
		elt_copy(rx, px);
		elt_copy(ry, py);
		return;
	}

	elt_add(u, px, qx);

	if (elt_is_zero(u)) {
		elt_add(u, py, qy);
		if (elt_is_zero(u))
			point_double(r, p);
		else {
			elt_zero(rx);
			elt_zero(ry);
		}

		return;
	}

	elt_inv(t, u);
	elt_add(u, py, qy);
	elt_mul(s, t, u);

	elt_square(t, s);
	elt_add(t, t, s);
	elt_add(t, t, qx);
	t[29] ^= 1;

	elt_mul(u, s, t);
	elt_add(s, u, py);
	elt_add(rx, t, px);
	elt_add(ry, s, rx);
}

static void point_mul(u8 *d, u8 *a, const u8 *b)	// a is bignum
{
	u32 i;
	u8 mask;

	elt_zero(d);
	elt_zero(d + 30);

	for (i = 0; i < 30; i++)
		for (mask = 0x80; mask != 0; mask >>= 1) {
			point_double(d, d);
			if ((a[i] & mask) != 0)
				point_add(d, d, b);
		}
}

int check_ecdsa(const u8 *Q, u8 *R, u8 *S, u8 *hash)
{
	u8 Sinv[30];
	u8 e[30];
	u8 w1[30], w2[30];
	u8 r1[60], r2[60];

	bn_inv(Sinv, S, ec_N, 30);

	elt_zero(e);
	memcpy(e + 10, hash, 20);

	bn_mul(w1, e, Sinv, ec_N, 30);
	bn_mul(w2, R, Sinv, ec_N, 30);

	point_mul(r1, w1, ec_G);
	point_mul(r2, w2, Q);

	point_add(r1, r1, r2);

	if (bn_compare(r1, ec_N, 30) >= 0)
		bn_sub_modulus(r1, ec_N, 30);

	return (bn_compare(r1, R, 30) == 0);
}

#endif
