// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Copyright 2008  Hector Martin  <marcan@marcansoft.com>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
#include "Python.h"
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;

/* ----------------------------------------------------- */

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

static void elt_add(u8 *d, const u8 *a, const u8 *b)
{
	u32 i;

	for (i = 0; i < 30; i++)
		d[i] = a[i] ^ b[i];
}

static void elt_mul_x(u8 *d, const u8 *a)
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

static void elt_mul(u8 *d, const u8 *a, const u8 *b)
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

static const u8 square[16] =
		"\x00\x01\x04\x05\x10\x11\x14\x15\x40\x41\x44\x45\x50\x51\x54\x55";

static void elt_square_to_wide(u8 *d, const u8 *a)
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

static void elt_square(u8 *d, const u8 *a)
{
	u8 wide[60];

	elt_square_to_wide(wide, a);
	wide_reduce(wide);

	elt_copy(d, wide + 30);
}

static void itoh_tsujii(u8 *d, const u8 *a, const u8 *b, u32 j)
{
	u8 t[30];

	elt_copy(t, a);
	while (j--) {
		elt_square(d, t);
		elt_copy(t, d);
	}

	elt_mul(d, t, b);
}

static void elt_inv(u8 *d, const u8 *a)
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

/* ----------------------------------------------------- */

static PyObject *
_ec_elt_mul(PyObject *self /* Not used */, PyObject *args)
{
	const char *a, *b;
	char d[30];
	int la, lb;
	if (!PyArg_ParseTuple(args, "s#s#", &a, &la, &b, &lb))
		return NULL;
	
	if(la != 30 || lb != 30) {
		PyErr_SetString(PyExc_ValueError, "ELT lengths must be 30");
		return NULL;
	}
	elt_mul(d,a,b);
	return Py_BuildValue("s#", d, 30);
}

static PyObject *
_ec_elt_inv(PyObject *self /* Not used */, PyObject *args)
{
	const char *a;
	char d[30];
	int la;

	if (!PyArg_ParseTuple(args, "s#", &a, &la))
		return NULL;
	
	if(la != 30) {
		PyErr_SetString(PyExc_ValueError, "ELT length must be 30");
		return NULL;
	}
	elt_inv(d,a);
	return Py_BuildValue("s#", d, 30);
}

static PyObject *
_ec_elt_square(PyObject *self /* Not used */, PyObject *args)
{
	const char *a;
	char d[30];
	int la;

	if (!PyArg_ParseTuple(args, "s#", &a, &la))
		return NULL;
	
	if(la != 30) {
		PyErr_SetString(PyExc_ValueError, "ELT length must be 30");
		return NULL;
	}
	elt_square(d,a);
	return Py_BuildValue("s#", d, 30);
}

/* List of methods defined in the module */

static struct PyMethodDef _ec_methods[] = {
	{"elt_mul",	(PyCFunction)_ec_elt_mul,	METH_VARARGS,	"Multiply two ELTs"},
	{"elt_inv",	(PyCFunction)_ec_elt_inv,	METH_VARARGS,	"Take the inverse of an ELT"},
	{"elt_square",	(PyCFunction)_ec_elt_square,	METH_VARARGS,	"Efficiently square an ELT"},
 
	{NULL,	 (PyCFunction)NULL, 0, NULL}		/* sentinel */
};


/* Initialization function for the module (*must* be called init_ec) */

static char _ec_module_documentation[] = 
"Faster C versions of some ELT functions"
;

void
init_ec()
{
	PyObject *m, *d;

	/* Create the module and add the functions */
	m = Py_InitModule4("_ec", _ec_methods,
		_ec_module_documentation,
		(PyObject*)NULL,PYTHON_API_VERSION);

	/* Check for errors */
	if (PyErr_Occurred())
		Py_FatalError("can't initialize module _ec");
}

