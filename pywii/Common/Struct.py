import struct, sys

class StructType(tuple):
	def __getitem__(self, value):
		return [self] * value
	def __call__(self, value, endian='<'):
		if isinstance(value, bytes):
			return struct.unpack(endian + tuple.__getitem__(self, 0), value[:tuple.__getitem__(self, 1)])[0]
		else:
			return struct.pack(endian + tuple.__getitem__(self, 0), value)

class StructException(Exception):
	pass

class Struct:
	__slots__ = ('__attrs__', '__baked__', '__defs__', '__next__', '__sizes__', '__values__')
	int8 = StructType(('b', 1))
	uint8 = StructType(('B', 1))
	
	int16 = StructType(('h', 2))
	uint16 = StructType(('H', 2))
	
	int32 = StructType(('l', 4))
	uint32 = StructType(('L', 4))
	
	int64 = StructType(('q', 8))
	uint64 = StructType(('Q', 8))
	
	float = StructType(('f', 4))
	
	@classmethod
	def string(cls, len, offset=0, encoding=None, stripNulls=False, value=''):
		return StructType(('string', (len, offset, encoding, stripNulls, value)))
	
	LE = '<'
	BE = '>'
	__endian__ = '<'
	
	def __init__(self, func=None, unpack=None, **kwargs):
		self.__defs__ = []
		self.__sizes__ = []
		self.__attrs__ = []
		self.__values__ = {}
		self.__next__ = True
		self.__baked__ = False
		
		if func == None:
			self.__format__()
		else:
			sys.settrace(self.__trace__)
			func()
			for name in func.__code__.co_varnames:
				value = self.__frame__.f_locals[name]
				self.__setattr__(name, value)
		
		self.__baked__ = True
		
		if unpack != None:
			if isinstance(unpack, tuple):
				self.unpack(*unpack)
			else:
				self.unpack(unpack)
		
		if len(kwargs):
			for name in kwargs:
				self.__values__[name] = kwargs[name]
	
	def __trace__(self, frame, event, arg):
		self.__frame__ = frame
		sys.settrace(None)
	
	def __setattr__(self, name, value):
		if name in self.__slots__:
			return object.__setattr__(self, name, value)
		
		if self.__baked__ == False:
			if not isinstance(value, list):
				value = [value]
				attrname = name
			else:
				attrname = '*' + name
			
			self.__values__[name] = None
			
			for sub in value:
				if isinstance(sub, Struct):
					sub = sub.__class__
				try:
					if issubclass(sub, Struct):
						sub = ('struct', sub)
				except TypeError:
					pass
				type_, size = tuple(sub)
				if type_ == 'string':
					self.__defs__.append(Struct.string)
					self.__sizes__.append(size)
					self.__attrs__.append(attrname)
					self.__next__ = True
					
					if attrname[0] != '*':
						self.__values__[name] = size[3]
					elif self.__values__[name] == None:
						self.__values__[name] = [size[3] for val in value]
				elif type_ == 'struct':
					self.__defs__.append(Struct)
					self.__sizes__.append(size)
					self.__attrs__.append(attrname)
					self.__next__ = True
					
					if attrname[0] != '*':
						self.__values__[name] = size()
					elif self.__values__[name] == None:
						self.__values__[name] = [size() for val in value]
				else:
					if self.__next__:
						self.__defs__.append('')
						self.__sizes__.append(0)
						self.__attrs__.append([])
						self.__next__ = False
					
					self.__defs__[-1] += type_
					self.__sizes__[-1] += size
					self.__attrs__[-1].append(attrname)
					
					if attrname[0] != '*':
						self.__values__[name] = 0
					elif self.__values__[name] == None:
						self.__values__[name] = [0 for val in value]
		else:
			try:
				self.__values__[name] = value
			except KeyError:
				raise AttributeError(name)
	
	def __getattr__(self, name):
		if self.__baked__ == False:
			return name
		else:
			try:
				return self.__values__[name]
			except KeyError:
				raise AttributeError(name)
	
	def __len__(self):
		ret = 0
		arraypos, arrayname = None, None
		
		for i in range(len(self.__defs__)):
			sdef, size, attrs = self.__defs__[i], self.__sizes__[i], self.__attrs__[i]
			
			if sdef == Struct.string:
				size, offset, encoding, stripNulls, value = size
				if isinstance(size, str):
					size = self.__values__[size] + offset
			elif sdef == Struct:
				if attrs[0] == '*':
					if arrayname != attrs:
						arrayname = attrs
						arraypos = 0
					size = len(self.__values__[attrs[1:]][arraypos])
				size = len(self.__values__[attrs])
			
			ret += size
		
		return ret
	
	def unpack(self, data, pos=0):
		for name in self.__values__:
			if not isinstance(self.__values__[name], Struct):
				self.__values__[name] = None
			elif self.__values__[name].__class__ == list and len(self.__values__[name]) != 0:
				if not isinstance(self.__values__[name][0], Struct):
					self.__values__[name] = None
		
		arraypos, arrayname = None, None
		
		for i in range(len(self.__defs__)):
			sdef, size, attrs = self.__defs__[i], self.__sizes__[i], self.__attrs__[i]
			
			if sdef == Struct.string:
				size, offset, encoding, stripNulls, value = size
				if isinstance(size, str):
					size = self.__values__[size] + offset
				
				temp = data[pos:pos+size]
				if len(temp) != size:
					raise StructException('Expected %i byte string, got %i' % (size, len(temp)))
				
				if stripNulls:
					temp = temp.rstrip(b'\0')
				
				if encoding != None:
					temp = temp.decode(encoding)
				else:
					temp = temp.decode("ascii")
				
				if attrs[0] == '*':
					name = attrs[1:]
					if self.__values__[name] == None:
						self.__values__[name] = []
					self.__values__[name].append(temp)
				else:
					self.__values__[attrs] = temp
				pos += size
			elif sdef == Struct:
				if attrs[0] == '*':
					if arrayname != attrs:
						arrayname = attrs
						arraypos = 0
					name = attrs[1:]
					self.__values__[attrs][arraypos].unpack(data, pos)
					pos += len(self.__values__[attrs][arraypos])
					arraypos += 1
				else:
					self.__values__[attrs].unpack(data, pos)
					pos += len(self.__values__[attrs])
			else:
				values = struct.unpack(self.__endian__+sdef, data[pos:pos+size])
				pos += size
				j = 0
				for name in attrs:
					if name[0] == '*':
						name = name[1:]
						if self.__values__[name] == None:
							self.__values__[name] = []
						self.__values__[name].append(values[j])
					else:
						self.__values__[name] = values[j]
					j += 1
		
		return self
	
	def pack(self):
		arraypos, arrayname = None, None
		
		ret = b''
		for i in range(len(self.__defs__)):
			sdef, size, attrs = self.__defs__[i], self.__sizes__[i], self.__attrs__[i]
			
			if sdef == Struct.string:
				size, offset, encoding, stripNulls, value = size
				if isinstance(size, str):
					size = self.__values__[size]+offset
				
				if attrs[0] == '*':
					if arrayname != attrs:
						arraypos = 0
						arrayname = attrs
					temp = self.__values__[attrs[1:]][arraypos]
					arraypos += 1
				else:
					temp = self.__values__[attrs]
				
				if encoding != None:
					temp = temp.encode(encoding)
				elif isinstance(temp, str):
					temp = temp.encode("ascii")
				
				temp = temp[:size]
				ret += temp + (b'\0' * (size - len(temp)))
			elif sdef == Struct:
				if attrs[0] == '*':
					if arrayname != attrs:
						arraypos = 0
						arrayname = attrs
					ret += self.__values__[attrs[1:]][arraypos].pack()
					arraypos += 1
				else:
					ret += self.__values__[attrs].pack()
			else:
				values = []
				for name in attrs:
					if name[0] == '*':
						if arrayname != name:
							arraypos = 0
							arrayname = name
						values.append(self.__values__[name[1:]][arraypos])
						arraypos += 1
					else:
						values.append(self.__values__[name])
				
				ret += struct.pack(self.__endian__+sdef, *values)
		return ret
	
	def __getitem__(self, value):
		return [('struct', self.__class__)] * value

if __name__=='__main__':
	class TestStruct(Struct):
		__endian__ = Struct.LE
		def __format__(self):
			self.foo, self.bar = Struct.uint32, Struct.float
			self.baz = Struct.string(8)
			
			self.omg = Struct.uint32
			self.wtf = Struct.string(self.omg)
			
			class HaxStruct(Struct):
				__endian__ = Struct.LE
				def __format__(self):
					self.thing1 = Struct.uint32
					self.thing2 = Struct.uint32
			self.hax = HaxStruct
	
	test = TestStruct()
	test.unpack(b'\xEF\xBE\xAD\xDE\x00\x00\x80\x3Fdeadbeef\x04\x00\x00\x00test\xCA\xFE\xBA\xBE\xBE\xBA\xFE\xCA')
	assert test.foo == 0xDEADBEEF
	assert test.bar == 1.0
	assert test.baz == b'deadbeef'
	assert test.omg == 4
	assert test.wtf == b'test'
	assert test.hax.thing1 == 0xBEBAFECA
	assert test.hax.thing2 == 0xCAFEBABE
	
	print('Tests successful')
	
	"""
	@Struct.LE
	def TestStruct():
		foo, bar = Struct.uint32, Struct.float
		baz = Struct.string(8)
		
		omg = Struct.uint32
		wtf = Struct.string(omg)
		
		@Struct.LE
		def HaxStruct():
			thing1 = Struct.uint32
			thing2 = Struct.uint32
		hax = HaxStruct()
	
	test = TestStruct()
	test.foo = 0xCAFEBABE
	test.bar = 0.0
	thing = test.hax.thing1
	test.hax.thing1 = test.hax.thing2
	test.hax.thing2 = thing
	assert test.pack() == '\xBE\xBA\xFE\xCA\0\0\0\0deadbeef\x04\x00\x00\x00test\xBE\xBA\xFE\xCA\xCA\xFE\xBA\xBE'
	"""
