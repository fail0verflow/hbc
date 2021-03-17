#!/usr/bin/python3
# Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
# Copyright 2008  Hector Martin  <marcan@marcansoft.com>
# Licensed under the terms of the GNU GPL, version 2
# http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

from array import array
from struct import pack, unpack
try:
    from Cryptodome.Util.number import bytes_to_long, long_to_bytes
except ImportError:
    from Crypto.Util.number import bytes_to_long, long_to_bytes

# y**2 + x*y = x**3 + x + b
ec_b = (b"\x00\x66\x64\x7e\xde\x6c\x33\x2c\x7f\x8c\x09\x23\xbb\x58\x21"+
	    b"\x3b\x33\x3b\x20\xe9\xce\x42\x81\xfe\x11\x5f\x7d\x8f\x90\xad")

def hexdump(s,sep=""):
	return sep.join(["%02x"%ord(x) for x in s])

def bhex(s,sep=""):
	return hexdump(long_to_bytes(s,30),sep)

fastelt = False
try:
	import _ec
	fastelt = True
except ImportError:
	#print "C Elliptic Curve functions not available. EC certificate checking will be much slower."
	pass


class ByteArray(array):
	def __new__(cls, initializer=None):
		return super(ByteArray, cls)	.__new__(cls,'B',initializer)
	def __init__(self,initializer=None):
		array.__init__(self)
	def __setitem__(self,item,value):
		if isinstance(item, slice):
			array.__setitem__(self, item, [x & 0xFF for x in value])
		else:
			array.__setitem__(self, item, value & 0xFF)
	def __long__(self):
		return bytes_to_long(self.tobytes())
	def __str__(self):
		return ''.join(["%02x"%x for x in self.tobytes()])
	def __repr__(self):
		return "ByteArray('%s')"%''.join(["\\x%02x"%x for x in self.tobytes()])

class ELT_PY:
	SIZEBITS=233
	SIZE=(SIZEBITS+7)/8
	square = ByteArray(b"\x00\x01\x04\x05\x10\x11\x14\x15\x40\x41\x44\x45\x50\x51\x54\x55")
	def __init__(self, initializer=None):
		if isinstance(initializer, int) or isinstance(initializer, int):
			self.d = ByteArray(long_to_bytes(initializer,self.SIZE))
		elif isinstance(initializer, bytes):
			self.d = ByteArray(initializer)
		elif isinstance(initializer, ByteArray):
			self.d = ByteArray(initializer)
		elif isinstance(initializer, array):
			self.d = ByteArray(initializer)
		elif isinstance(initializer, ELT):
			self.d = ByteArray(initializer.d)
		elif initializer is None:
			self.d = ByteArray([0]*self.SIZE)
		else:
			raise TypeError("Invalid initializer type")
		if len(self.d) != self.SIZE:
			raise ValueError("ELT size must be 30")
		
	def __cmp__(self, other):
		if other == 0: #exception
			if self:
				return 1
			else:
				return 0
		if not isinstance(other,ELT):
			return NotImplemented
		return cmp(self.d,other.d)

	def __long__(self):
		return int(self.d)
	def __repr__(self):
		return repr(self.d).replace("ByteArray","ELT")
	def __bytes__(self):
		return bytes(self.d)
	def __bool__(self):
		for x in self.d:
			if x != 0:
				return True
		return False
	def __len__(self):
		return self.SIZE
	def __add__(self,other):
		if not isinstance(other,ELT):
			return NotImplemented
		new = ELT(self)
		for x in range(self.SIZE):
			new[x] ^= other[x]
		return new
	def _mul_x(self):
		carry = self[0]&1
		x = 0
		d = ELT()
		for i in range(self.SIZE-1):
			y = self[i + 1]
			d[i] = x ^ (y >> 7)
			x = y << 1
		d[29] = x ^ carry
		d[20] ^= carry << 2
		return d
	def __mul__(self,other):
		if not isinstance(other,ELT):
			return NotImplemented
		d = ELT()
		i = 0
		mask = 1
		for n in range(self.SIZEBITS):
			d = d._mul_x()
			if (self[i] & mask) != 0:
				d += other
			mask >>= 1
			if mask == 0:
				mask = 0x80
				i+=1
		return d
	def __pow__(self,other):
		if other == -1:
			return 1/self
		if other < 1:
			return NotImplemented
		if other % 2 == 0:
			return self._square()**(other/2)
		x = self
		for i in range(other-1):
			x *= self
		return x
	def _square(self):
		wide = ByteArray([0]*self.SIZE*2)
		for i in range(self.SIZE):
			wide[2*i] = self.square[self[i] >> 4]
			wide[2*i + 1] = self.square[self[i] & 0xf]
		for i in range(self.SIZE):
			x = wide[i]
	
			wide[i + 19] ^= x >> 7;
			wide[i + 20] ^= x << 1;
	
			wide[i + 29] ^= x >> 1;
			wide[i + 30] ^= x << 7;
		x = wide[30] & 0xFE;
	
		wide[49] ^= x >> 7;
		wide[50] ^= x << 1;
	
		wide[59] ^= x >> 1;
	
		wide[30] &= 1;
		return ELT(wide[self.SIZE:])
	def _itoh_tsujii(self,b,j):
		t = ELT(self)
		return t**(2**j) * b
	def __rdiv__(self,other):
		if isinstance(other,ELT):
			return 1/self * other
		elif other == 1:
			t = self._itoh_tsujii(self, 1)
			s = t._itoh_tsujii(self, 1)
			t = s._itoh_tsujii(s, 3)
			s = t._itoh_tsujii(self, 1)
			t = s._itoh_tsujii(s, 7)
			s = t._itoh_tsujii(t, 14)
			t = s._itoh_tsujii(self, 1)
			s = t._itoh_tsujii(t, 29)
			t = s._itoh_tsujii(s, 58)
			s = t._itoh_tsujii(t, 116)
			return s**2
		else:
			return NotImplemented

	def __getitem__(self,item):
		return self.d[item]
	def __setitem__(self,item,value):
		self.d[item] = value
	def tobignum(self):
		return bytes_to_long(self.d.tobytes())
	def tobytes(self):
		return self.d.tobytes()

class ELT_C(ELT_PY):
	def __mul__(self,other):
		if not isinstance(other,ELT):
			return NotImplemented
		return ELT(_ec.elt_mul(self.d.tobytes(),other.d.tobytes()))
	def __rdiv__(self,other):
		if other != 1:
			return ELT_PY.__rdiv__(self,other)
		return ELT(_ec.elt_inv(self.d.tobytes()))
	def _square(self):
		return ELT(_ec.elt_square(self.d.tobytes()))

if fastelt:
	ELT = ELT_C
else:
	ELT = ELT_PY

class Point:
	def __init__(self,x,y=None):
		if isinstance(x,bytes) and (y is None) and (len(x) == 60):
			self.x = ELT(x[:30])
			self.y = ELT(x[30:])
		elif isinstance(x,Point):
			self.x = ELT(x.x)
			self.y = ELT(x.y)
		else:
			self.x = ELT(x)
			self.y = ELT(y)
	def on_curve(self):
		return (self.x**3 + self.x**2 + self.y**2 + self.x*self.y + ELT(ec_b)) == 0
	def __cmp__(self, other):
		if other == 0:
			if self.x or self.y:
				return 1
			else:
				return 0
		elif isinstance(other, Point):
			ca = cmp(self.x,other.x)
			if ca != 0:
				return ca
			return cmp(self.y,other.y)
		return NotImplemented
	def _double(self):
		if self.x == 0:
			return Point(0,0)
		
		s = self.y/self.x + self.x
		rx = s**2 + s
		rx[29] ^= 1;
		ry = s * rx + rx + self.x**2
		return Point(rx,ry)
	def __add__(self, other):
		if not isinstance(other,Point):
			return NotImplemented
		if self == 0:
			return Point(other)
		if other == 0:
			return Point(self)
		u = self.x + other.x
		if u == 0:
			u = self.y + other.y
			if u == 0:
				return self._double()
			else:
				return Point(0,0)
	
		s = (self.y + other.y) / u
		t = s**2 + s + other.x
		t[29] ^= 1
	
		rx = t+self.x
		ry = s*t+self.y+rx
		return Point(rx,ry)
		
	def __mul__(self, other):
		bts = long_to_bytes(other,30)
		d = Point(0,0)
		for i in range(30):
			mask = 0x80
			while mask != 0:
				d = d._double()
				if ((ord(bts[i]) & mask) != 0):
					d += self
				mask >>=1
		return d
	#def __mul__(self, other):
		#if not (isinstance(other,long) or isinstance(other,int)):
			#return NotImplemented
		
		#d = Point(0,0)
		#s = Point(self)
		
		#while other != 0:
			#if other & 1:
				#d += s
			#s = s._double()
			#other >>= 1
		#return d
	def __rmul__(self, other):
		return self * other
	def __str__(self):
		return "(%s,%s)"%(str(self.x),str(self.y))
	def __repr__(self):
		return "Point"+str(self)
	def __bool__(self):
		return self.x or self.y
	def tobytes(self):
		return self.x.tobytes() + self.y.tobytes()

#only for prime N
#segher, your math makes my head hurt. But it works.
def bn_inv(a,N):
	return pow(a,N-2,N)


# order of the addition group of points
ec_N = bytes_to_long(
			b"\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"+
			b"\x13\xe9\x74\xe7\x2f\x8a\x69\x22\x03\x1d\x26\x03\xcf\xe0\xd7")

# base point
ec_G = Point(
	b"\x00\xfa\xc9\xdf\xcb\xac\x83\x13\xbb\x21\x39\xf1\xbb\x75\x5f"+
	b"\xef\x65\xbc\x39\x1f\x8b\x36\xf8\xf8\xeb\x73\x71\xfd\x55\x8b"+
	b"\x01\x00\x6a\x08\xa4\x19\x03\x35\x06\x78\xe5\x85\x28\xbe\xbf"+
	b"\x8a\x0b\xef\xf8\x67\xa7\xca\x36\x71\x6f\x7e\x01\xf8\x10\x52")

def generate_ecdsa(k, sha):
	k = bytes_to_long(k)

	if k >= ec_N:
		raise Exception("Invalid private key")

	e = bytes_to_long(sha)

	m = open("/dev/random","rb").read(30)
	if len(m) != 30:
		raise Exception("Failed to get random data")
	m = bytes_to_long(m) % ec_N

	r = (m * ec_G).x.tobignum() % ec_N

	kk = ((r*k)+e)%ec_N
	s = (bn_inv(m,ec_N) * kk)%ec_N

	r = long_to_bytes(r,30)
	s = long_to_bytes(s,30)
	return r,s

def check_ecdsa(q,r,s,sha):

	q = Point(q)
	r = bytes_to_long(r)
	s = bytes_to_long(s)
	e = bytes_to_long(sha)

	s_inv = bn_inv(s,ec_N)

	w1 = (e*s_inv)%ec_N
	w2 = (r*s_inv)%ec_N

	r1 = w1 * ec_G + w2 * q

	rx = r1.x.tobignum()%ec_N

	return rx == r

def priv_to_pub(k):
	k = bytes_to_long(k)
	q = k * ec_G
	return q.tobytes()

def gen_priv_key():
	k = open("/dev/random","rb").read(30)
	if len(k) != 30:
		raise Exception("Failed to get random data")

	k = bytes_to_long(k)
	k = k % ec_N
	return long_to_bytes(k,30)
