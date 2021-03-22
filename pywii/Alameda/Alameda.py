have_pyglet = False

try:
	from pyglet import clock, window
	from pyglet.gl import *
	from pyglet.image import *
	have_pyglet = True
except:
	pass

import sys, os
sys.path.append(os.path.realpath(os.path.dirname(sys.argv[0]))+"/../Common")
from Struct import Struct
import struct
import pywii as wii

def LZ77Decompress(data):
	inp = 0
	dlen = len(data)
	data += b'\0' * 16
	
	ret = []
	
	while inp < dlen:
		bitmask = data[inp]
		inp += 1
		
		for i in range(8):
			if bitmask & 0x80:
				rep = data[inp]
				repLength = (rep >> 4) + 3
				inp += 1
				repOff = data[inp] | ((rep & 0x0F) << 8)
				inp += 1
				
				assert repOff <= len(ret)
				
				while repLength > 0:
					ret.append(ret[-repOff - 1])
					repLength -= 1
			else:
				ret.append(data[inp])
				inp += 1
			
			bitmask <<= 1
	
	return bytes(ret)

class U8(object):
	class U8Header(Struct):
		__endian__ = Struct.BE
		def __format__(self):
			self.Tag = Struct.uint32
			self.RootNode = Struct.uint32
			self.HeaderSize = Struct.uint32
			self.DataOffset = Struct.uint32
			self.Zeroes = Struct.uint8[0x10]
	
	class U8Node(Struct):
		__endian__ = Struct.BE
		def __format__(self):
			self.Type = Struct.uint16
			self.NameOffset = Struct.uint16
			self.DataOffset = Struct.uint32
			self.Size = Struct.uint32
	
	def __init__(self, data=None):
		self.Files = {}
		
		if data != None:
			self.Unpack(data)
	
	def Unpack(self, data):
		pos = 0
		u8 = self.U8Header()
		u8.unpack(data[pos:pos+len(u8)])
		pos += len(u8)
		
		print(hex(u8.Tag))
		assert u8.Tag == 0x55aa382d
		pos += u8.RootNode - 0x20
		
		root = self.U8Node()
		root.unpack(data[pos:pos+len(root)])
		pos += len(root)
		
		children = []
		for i in range(root.Size - 1):
			child = self.U8Node()
			child.unpack(data[pos:pos+len(child)])
			pos += len(child)
			children.append(child)
		
		stringTable = data[pos:pos + u8.DataOffset - len(u8) - root.Size * len(root)]
		pos += len(stringTable)
		
		currentOffset = u8.DataOffset
		
		path = ['.']
		pathDepth = [root.Size - 1]
		for offset,child in enumerate(children):
			name = stringTable[child.NameOffset:].split(b'\0', 1)[0].decode("ascii")
			if child.Type == 0x0100:
				path.append(name)
				pathDepth.append(child.Size-offset-1)
			elif child.Type == 0:
				name = '/'.join(path + [name])
				
				if currentOffset < child.DataOffset:
					diff = child.DataOffset - currentOffset
					assert diff <= 32
					
					pos += diff
					currentOffset += diff
				
				currentOffset += child.Size
				
				self.Files[name.lower()] = data[pos:pos+child.Size]
				pos += child.Size
			
			pathDepth[-1] -= 1
			if pathDepth[-1] == 0:
				path = path[:-1]
				pathDepth = pathDepth[:-1]

class IMD5Header(Struct):
	__endian__ = Struct.BE
	def __format__(self):
		self.Tag = Struct.string(4)
		self.Size = Struct.uint32
		self.Zeroes = Struct.uint8[8]
		self.MD5 = Struct.uint8[0x10]

def IMD5(data):
	imd5 = IMD5Header()
	imd5.unpack(data[:len(imd5)])
	assert imd5.Tag == 'IMD5'
	pos = len(imd5)
	
	if data[pos:pos+4] == b'LZ77':
		return LZ77Decompress(data[pos+8:])
	else:
		return data[pos:]

class TPL(object):
	class TPLHeader(Struct):
		__endian__ = Struct.BE
		def __format__(self):
			self.Magic = Struct.uint32
			self.Count = Struct.uint32
			self.Size = Struct.uint32
	
	class TexOffsets(Struct):
		__endian__ = Struct.BE
		def __format__(self):
			self.HeaderOff = Struct.uint32
			self.PaletteOff = Struct.uint32
	
	class TexHeader(Struct):
		__endian__ = Struct.BE
		def __format__(self):
			self.Size = Struct.uint16[2]
			self.Format = Struct.uint32
			self.DataOff = Struct.uint32
			self.Wrap = Struct.uint32[2]
			self.Filter = Struct.uint32[2]
			self.LODBias = Struct.float
			self.EdgeLOD = Struct.uint8
			self.LOD = Struct.uint8[2]
			self.Unpacked = Struct.uint8
	
	class PalHeader(Struct):
		__endian__ = Struct.BE
		def __format__(self):
			self.Count = Struct.uint16
			self.Unpacked = Struct.uint8
			self.Pad = Struct.uint8
			self.Format = Struct.uint32
			self.DataOff = Struct.uint32
	
	def __init__(self, data=None):
		self.Textures = []
		
		if data != None:
			self.Unpack(data)
	
	def Unpack(self, data):
		header = self.TPLHeader()
		header.unpack(data[:len(header)])
		pos = len(header)
		
		assert header.Magic == 0x0020AF30
		assert header.Size == 0xc
		
		for i in range(header.Count):
			offs = self.TexOffsets()
			offs.unpack(data[pos:pos+len(offs)])
			pos += len(offs)
			
			self.Textures.append(self.ParseTexture(data, offs))
	
	def ParseTexture(self, data, offs):
		texHeader = self.TexHeader()
		texHeader.unpack(data[offs.HeaderOff:offs.HeaderOff+len(texHeader)])
		format = texHeader.Format
		self.format = format
		rgba = None
		if format == 0:
			rgba = self.I4(data[texHeader.DataOff:], texHeader.Size)
		elif format == 1:
			rgba = self.I8(data[texHeader.DataOff:], texHeader.Size)
		elif format == 2:
			rgba = self.IA4(data[texHeader.DataOff:], texHeader.Size)
		elif format == 3:
			rgba = self.IA8(data[texHeader.DataOff:], texHeader.Size)
		elif format == 4:
			rgba = self.RGB565(data[texHeader.DataOff:], texHeader.Size)
		elif format == 5:
			rgba = self.RGB5A3(data[texHeader.DataOff:], texHeader.Size)
		elif format == 6:
			rgba = self.RGBA8(data[texHeader.DataOff:], texHeader.Size)
		elif format == 14:
			rgba = self.S3TC(data[texHeader.DataOff:], texHeader.Size)
		else:
			print('Unknown texture format', format)
		
		if rgba == None:
			rgba = b'\0\0\0\0' * texHeader.Size[0] * texHeader.Size[1]
		
		image = ImageData(texHeader.Size[1], texHeader.Size[0], 'RGBA', rgba)
		print(format)
		return image
	
	def I4(self, data, xxx_todo_changeme):
		(y, x) = xxx_todo_changeme
		out = [0 for i in range(x * y)]
		outp = 0
		inp = 0
		for i in range(0, y, 8):
			for j in range(0, x, 8):
				ofs = 0
				for k in range(8):
					off = min(x - j, 8)
					for sub in range(0, off, 2):
						texel = data[inp]
						high, low = texel >> 4, texel & 0xF
						if (outp + ofs + sub) < (x*y):
							out[outp + ofs + sub] = (high << 4) | (high << 20) | (high << 12) | 0xFF<<24
						
						if sub + 1 < off:
							if (outp + ofs + sub + 1) < (x*y):
								out[outp + ofs + sub + 1] = (low << 4) | (low << 20) | (low << 12) | 0xFF<<24
						
						inp += 1
					
					ofs += x
					inp += (8 - off) // 2
				outp += off
			outp += x * 7
		
		return b''.join(Struct.uint32(p) for p in out)
	
	def I8(self, data, xxx_todo_changeme1):
		(y, x) = xxx_todo_changeme1
		out = [0 for i in range(x * y*2)]
		outp = 0
		inp = 0
		for i in range(0, y, 4):
			for j in range(0, x, 4):
				ofs = 0
				for k in range(4):
					off = min(x - j, 4)
					for sub in range(off):
						texel = data[inp]
						out[outp + ofs + sub] = (texel << 24) | (texel << 16) | (texel << 8) | 0xFF
						inp += 1
					
					ofs += x
					inp += 4 - off * 2
				outp += off
			outp += x * 3
		
		return b''.join(Struct.uint32(p) for p in out)
	
	def IA4(self, data, xxx_todo_changeme2):
		(y, x) = xxx_todo_changeme2
		out = [0 for i in range(x * y)]
		outp = 0
		inp = 0
		for i in range(0, y, 4):
			for j in range(0, x, 8):
				ofs = 0
				for k in range(4):
					off = min(x - j, 8)
					for sub in range(off):
						texel = data[inp]
						alpha, inte = texel >> 4, texel & 0xF
						if (outp + ofs + sub) < (x*y):
							out[outp + ofs + sub] = (inte << 4) | (inte << 12) | (inte << 20) | (alpha << 28)
						inp += 1
					
					ofs += x
					inp += 8 - off
				outp += off
			outp += x * 3
		
		return b''.join(Struct.uint32(p) for p in out)
	
	def IA8(self, data, xxx_todo_changeme3):
		(y, x) = xxx_todo_changeme3
		out = [0 for i in range(x * y)]
		outp = 0
		inp = 0
		for i in range(0, y, 4):
			for j in range(0, x, 4):
				ofs = 0
				for k in range(4):
					off = min(x - j, 4)
					for sub in range(off):
						if (outp + ofs + sub) < (x*y):
							texel = Struct.uint16(data[inp:inp + 2], endian='>')
							p  = (texel & 0xFF)
							p |= (texel & 0xFF) << 8
							p |= (texel & 0xFF) << 16
							p |= (texel >> 8)   << 24
							out[outp + ofs + sub] = p
						inp += 2
					
					ofs += x
					inp += (4 - off) * 2
				outp += off
			outp += x * 3
		
		return b''.join(Struct.uint32(p) for p in out)
	
	def RGB565(self, data, xxx_todo_changeme4):
		(y, x) = xxx_todo_changeme4
		out = [0 for i in range(x * y)]
		outp = 0
		inp = 0
		for i in range(0, y, 4):
			for j in range(0, x, 4):
				ofs = 0
				for k in range(4):
					off = min(x - j, 4)
					for sub in range(off):
						if (outp + ofs + sub) < (x*y):
							texel = Struct.uint16(data[inp:inp + 2], endian='>')
							p  = ((texel >> 11) & 0x1F) << 3
							p |= ((texel >> 5) & 0x3F) << 10
							p |= ( texel       & 0x1F) << 19
							p |= 0xFF<<24
							out[outp + ofs + sub] = p
						inp += 2
					
					ofs += x
					inp += (4 - off) * 2
				outp += off
			outp += x * 3
		
		return b''.join(Struct.uint32(p) for p in out)

	def RGB5A3(self, data, xxx_todo_changeme5):
		(y, x) = xxx_todo_changeme5
		out = [0 for i in range(x * y)]
		outp = 0
		inp = 0
		for i in range(0, y, 4):
			for j in range(0, x, 4):
				ofs = 0
				for k in range(4):
					off = min(x - j, 4)
					for sub in range(off):
						texel = Struct.uint16(data[inp:inp + 2], endian='>')
						if texel & 0x8000:
							p  = ((texel >> 10) & 0x1F) << 3
							p |= ((texel >> 5) & 0x1F) << 11
							p |= ( texel       & 0x1F) << 19
							p |= 0xFF<<24
						else:
							p  = ((texel >> 12) & 0x07) << 29
							p |= ((texel >>  8) & 0x0F) << 4
							p |= ((texel >>  4) & 0x0F) << 12
							p |=  (texel        & 0x0F) << 20
						if (outp + ofs + sub) < (x*y):
							out[outp + ofs + sub] = p
						inp += 2
					
					ofs += x
					inp += (4 - off) * 2
				outp += off
			outp += x * 3
		
		return b''.join(Struct.uint32(p) for p in out)

	def RGBA8(self, data, xxx_todo_changeme6):
		(y, x) = xxx_todo_changeme6
		out = [0 for i in range(x * y)]
		outp = 0
		inp = 0
		for i in range(0, y, 4):
			for j in range(0, x, 4):
				ofs = 0
				for k in range(4):
					off = min(x - j, 4)
					for sub in range(off):
						texel = Struct.uint16(data[inp:inp + 2], endian='>')<<16
						texel |= Struct.uint16(data[inp+32:inp + 34], endian='>')
						if (outp + ofs + sub) < (x*y):
							out[outp + ofs + sub] = texel&0xff000000 | ((texel>>16)&0xff) | (texel&0xff00) | ((texel<<16)&0xff0000)
						inp += 2
					
					ofs += x
					inp += (4 - off) * 2
				outp += off
				inp += 32
			outp += x * 3
		
		return b''.join(Struct.uint32(p) for p in out)

	def unpack_rgb565(self,texel):
		b = (texel&0x1f)<<3
		g = ((texel>>5)&0x3f)<<2
		r = ((texel>>11)&0x1f)<<3
		b |= b>>5
		g |= g>>6
		r |= r>>5
		return (0xff<<24) | (b<<16) | (g<<8) | r
	def icolor(self,a,b,fa,fb,fc):
		c = 0
		for i in range(0,32,8):
			xa = (a>>i)&0xff
			xb = (b>>i)&0xff
			xc = min(255,max(0,int((xa*fa + xb*fb)/fc)))
			c |= xc<<i
		return c
	
	def S3TC(self, data, xxx_todo_changeme7):
		(y, x) = xxx_todo_changeme7
		out = [0 for i in range(x * y)]
		TILE_WIDTH = 8
		TILE_HEIGHT = 8
		inp = 0
		outp = 0
		for i in range(0, y, TILE_HEIGHT):
			for j in range(0, x, TILE_WIDTH):
				maxw = min(x - j,TILE_WIDTH)
				for k in range(2):
					for l in range(2):
						rgb = [0,0,0,0]
						texel1 = Struct.uint16(data[inp:inp + 2], endian='>')
						texel2 = Struct.uint16(data[inp + 2:inp + 4], endian='>')
						rgb[0] = self.unpack_rgb565(texel1)
						rgb[1] = self.unpack_rgb565(texel2)
						
						if texel1 > texel2:
							rgb[2] = self.icolor (rgb[0], rgb[1], 2, 1, 3) | 0xff000000
							rgb[3] = self.icolor (rgb[1], rgb[0], 2, 1, 3) | 0xff000000
						else:
							rgb[2] = self.icolor (rgb[0], rgb[1], 0.5, 0.5, 1) | 0xff000000
							rgb[3] = 0
						
						# color selection (00, 01, 10, 11)
						cm = list(data[inp+4:inp+8])
						ofs = l*4
						for n in range(4):
							if (ofs + outp)<(x*y):
								# one row (4 texels)
								if maxw > (0 + l*4):
									out[ofs + 0 + outp] = rgb[(cm[n] & 0xc0) >> 6];
								if maxw > (1 + l*4):
									out[ofs + 1 + outp] = rgb[(cm[n] & 0x30) >> 4];
								if maxw > (2 + l*4):
									out[ofs + 2 + outp] = rgb[(cm[n] & 0x0c) >> 2];
								if maxw > (3 + l*4):
									out[ofs + 3 + outp] = rgb[(cm[n] & 0x03) >> 0];
							ofs += x
						inp += 8
					outp += x * 4
				outp += maxw - x * 8
			outp += x * (TILE_HEIGHT - 1)
		
		return b''.join(Struct.uint32(p) for p in out)

class Object(object):
	def __init__(self, name):
		self.Frame = 0
		self.Name = name
		self.Alpha = 255
		self.Animations = 0
	def __str__(self):
		return self.Name
	
class Pane(Object):
	def __init__(self, name, flags=0x104, alpha=255, coords=[0,0,0,0,0,0,1,1,0,0]):
		Object.__init__(self, name)
		self.Flags = flags
		self.Alpha = alpha
		self.Coords = list(coords)
		self.Children = []
		self.Enabled = True
		
	def getX(self): return self.Coords[0]
	def setX(self,v): self.Coords[0] = v
	def getY(self): return self.Coords[1]
	def setY(self,v): self.Coords[1] = v
	def getMagX(self): return self.Coords[6]
	def setMagX(self,v): self.Coords[6] = v
	def getMagY(self): return self.Coords[7]
	def setMagY(self,v): self.Coords[7] = v
	def getW(self): return self.Coords[8]
	def setW(self,v): self.Coords[8] = v
	def getH(self): return self.Coords[9]
	def setH(self,v): self.Coords[9] = v
	def getRot(self): return self.Coords[5]
	def setRot(self,v): self.Coords[5] = v
	
	X = property(getX,setX)
	Y = property(getY,setY)
	MagX = property(getMagX,setMagX)
	MagY = property(getMagY,setMagY)
	Width = property(getW,setW)
	Height = property(getH,setH)
	Rot = property(getRot,setRot)
	
	def Add(self, child):
		self.Children.append(child)
	
	def AnimDone(self):
		if not Object.AnimDone(self):
			return False
		
		for child in self.Children:
			if not child.AnimDone():
				return False
		
		return True

class Picture(Pane):
	def __init__(self, name, flags=0x504, alpha=255, coords=[0,0,0,0,0,0,1,1,0,0], unk=[0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff], mat=0, flags2=0x100, matcoord=[(0,0),(1,0),(0,1),(1,1)]):
		Pane.__init__(self, name, flags, alpha, coords)
		self.Unk = unk
		self.Material = mat
		self.Flags2 = flags2
		self.MaterialCoords = list(matcoord)
		
class ItemList(object):
	HDRLEN=4
	LSIZE=4
	OFFSET=0
	IS_ATOM=True
	def __init__(self, data=None):
		self.Items = []
		if data is not None:
			self.unpack(data)
	
	def add(self,n):
		self.Items.append(n)
	
	def unpack(self, data):
		pos = 0
		if self.IS_ATOM:
			pos = 8
		count = self.__unpkcnt__(data[pos:])
		pos = self.__getlistoff__(data[pos:])
		for i in range(count):
			off = Struct.uint32(data[pos:pos+4], endian='>') + self.OFFSET
			if i == (count-1):
				next = len(data)
			else:
				next = Struct.uint32(data[pos+self.LSIZE:pos+self.LSIZE+4], endian='>') + self.OFFSET
			pos += self.LSIZE
			self.unpack_item(i, data[off:next])
		# sanity check
		#dp = self.pack()
		#assert dp == data
	
	def pack(self,extra=None):
		
		extradata = b""
		if extra is not None:
			extradata = extra.pack()
		
		data = b""
		
		offsets = []
		
		for n,i in enumerate(self.Items):
			offsets.append(len(data))
			data += self.pack_item(n,i)
		
		listlen = len(self.Items) * self.LSIZE
		
		head = self.__mkheader__()
		outdata = b""
		if self.IS_ATOM:
			outdata += self.FOURCC.encode("ascii") + Struct.uint32((len(extradata) + listlen + len(data) + 8 + len(head) + 3) &(~3), endian='>')
		outdata += head
		
		dataoff = len(outdata) + listlen - self.OFFSET
		assert dataoff >= 0
		
		for n in offsets:
			outdata += Struct.uint32(n + dataoff, endian='>')
			if self.LSIZE > 4:
				outdata += (self.LSIZE-4)*b"\x00"
		
		outdata += data
		outdata += extradata
		
		if len(outdata)%4 != 0:
			outdata += (4-len(outdata)%4)*b"\x00"
		
		return outdata
	
	def __mkheader__(self):
		return Struct.uint16(len(self.Items), endian='>') + b"\x00\x00"
	
	def __unpkcnt__(self, data):
		return Struct.uint16(data[0:2], endian='>')
	
	def __getlistoff__(self, data):
		off=self.HDRLEN
		if self.IS_ATOM:
			off += 8
		return off
	
	def __getitem__(self, n):
		return self.Items[n]

	def __len__(self):
		return len(self.Items)

class StdAtom(Struct):
	__endian__ = Struct.BE
	def __format__(self):
		self.FourCC = Struct.string(4)
		self.Size = Struct.uint32
	def pack(self):
		self.FourCC = self.FOURCC
		self.Size = len(self)
		return Struct.pack(self)

class Brlyt(object):
	class BrlytHeader(Struct):
		__endian__ = Struct.BE
		def __format__(self):
			self.Magic = Struct.string(4)
			self.Unk = Struct.uint32
			self.Size = Struct.uint32
			self.UnkCount = Struct.uint16
			self.AtomCount = Struct.uint16
	
	class BrlytLYT1(StdAtom):
		FOURCC = "lyt1"
		def __format__(self):
			StdAtom.__format__(self)
			self.Flag = Struct.uint8
			self.pad = Struct.uint8[3]
			self.Width = Struct.float
			self.Height = Struct.float
	
	class BrlytTexture(object):
		def __init__(self, name, texture=None):
			self.Name = name
			self.Texture = texture
			self.GLTexture = None
		def create_texture(self):
			self.GLTexture = self.Texture.create_texture(Texture)
			
	class BrlytTXL1(ItemList):
		LSIZE = 8
		OFFSET = 12
		FOURCC = "txl1"
		def __init__(self, archive=None, data=None):
			self.Archive = archive
			ItemList.__init__(self, data)
		
		def unpack_item(self, i, data):
			fn = data.split(b'\0', 1)[0].decode("ascii")
			print(fn)
			tex = TPL(self.Archive.Files['./arc/timg/' + fn.lower()]).Textures[0]
			self.Items.append(Brlyt.BrlytTexture(fn, tex))
		def pack_item(self, i, item):
			return item.Name.encode("ascii") + b"\0"
	
	class BrlytMAT1(ItemList):
		LSIZE = 4
		OFFSET = 0
		FOURCC = "mat1"
		def __init__(self, data=None):
			ItemList.__init__(self, data)
		
		def unpack_item(self, num, data):
			mtl = Brlyt.BrlytMaterial()
			mtl.unpack(data)
			self.Items.append(mtl)

		def pack_item(self, i, item):
			return item.pack()
	
	class BrlytMaterialFlags(object):
		def __init__(self,value=None):
			if value is not None:
				self.unpack(value)
		def unpack(self,value):
			self.value = value
			self.NumTextures = value & 15
			self.NumCoords = (value>>4) & 15
			self.m_2 = (value>>8) & 15
			self.m_4 = bool((value>>12) & 1)
			self.m_6 = (value>>13) & 3
			self.m_5 = (value>>15) & 7
			self.m_3 = (value>>18) & 31
			self.m_9 = bool((value>>23) & 1)
			self.m_10 = bool((value>>24) & 1)
			self.m_7 = bool((value>>25) & 1)
			self.m_8 = bool((value>>27) & 1)
		
		def show(self):
			print("Flags: %08x"%self.value)
			print("ReserveGXMem(")
			print(" r4        =",self.NumTextures)
			print(" r5        =",self.NumCoords)
			print(" r6        =",self.m_2)
			print(" r7        =",self.m_3)
			print(" r8        =",self.m_4)
			print(" r9        =",self.m_5)
			print(" r10       =",self.m_6)
			print(" 0x8(%sp)  =",self.m_7)
			print(" 0xC(%sp)  =",self.m_8)
			print(" 0x10(%sp) =",self.m_9)
			print(" 0x14(%sp) =",self.m_10)
			print(")")
		
		def pack(self):
			val = 0
			val |= self.NumTextures
			val |= self.NumCoords << 4
			val |= self.m_2 << 8
			val |= self.m_4 << 12
			val |= self.m_6 << 13
			val |= self.m_5 << 15
			val |= self.m_3 << 18
			val |= self.m_9 << 23
			val |= self.m_10 << 24
			val |= self.m_7 << 25
			val |= self.m_8 << 27
			return val
	
	class BrlytMatHeader(Struct):
		__endian__ = Struct.BE
		
		def __format__(self):
			self.Name = Struct.string(0x14, stripNulls=True)
			self.Color1 = Struct.uint16[4]
			self.Color2 = Struct.uint16[4]
			self.Color3 = Struct.uint16[4]
			self.Blah = Struct.uint32[4]
			self.Flags = Struct.uint32
		
			# 0-3 num 0-8
			# 4-7 num 0-10
			# 8-11 num 0-8
			# 12 bool
			# 13-14 num 0-3
			# 15-17 num 0-4
			# 18-22 num 0-16
			# 23 bool
			# 24 bool
			# 25 bool a
			# 27 bool
			
			
			# if 25, xtra word
			# if 27, xtra word
			# if 12, xtra word
			# 13-14 * 0x14
			# 15-17 * 4
			# 18-22 * 16
			# if 23, xtra word
			# if 24, xtra word
	
	class BrlytMaterial(object):
		
		def __init__(self, name=None):
			if name is not None:
				self.Name = name
			self.Color1 = [0,0,0,0]
			self.Color2 = [255,255,255,255]
			self.Color3 = [255,255,255,255]
			self.Blah = [0xffffffff,0xffffffff,0xffffffff,0xffffffff]
			self.Textures = []
			self.TextureCoords = []
			self.SthB = []
			self.SthI = None
			self.SthJ = None
			self.SthC = None
			self.SthD = []
			self.SthE = []
			self.SthF = []
			self.SthG = 0x77000000
			self.SthH = None

		def unpack(self, data):
			wii.chexdump(data)
			hdr = Brlyt.BrlytMatHeader()
			hdr.unpack(data)
			self.FlagData = Brlyt.BrlytMaterialFlags(hdr.Flags)
			self.FlagData.show()
			
			self.Name = hdr.Name.split("\0",1)[0]
			self.Color1 = hdr.Color1
			self.Color2 = hdr.Color2
			self.Color3 = hdr.Color3
			self.Blah = hdr.Blah
			
			ptr = 0x40
			
			self.Textures = []
			self.TextureCoords = []
			self.SthB = []
			self.SthI = None
			self.SthJ = None
			self.SthC = None
			self.SthD = []
			self.SthE = []
			self.SthF = []
			self.SthG = None
			self.SthH = None
			
			for i in range(self.FlagData.NumTextures):
				texid = Struct.uint16(data[ptr:ptr+2], endian='>')
				texcs = data[ptr+2]
				texct = data[ptr+3]
				print("  * Texture: %04x %d %d"%(texid,texcs,texct))
				self.Textures.append((texid,texcs,texct))
				ptr += 4
			for i in range(self.FlagData.NumCoords):
				dat = []
				for j in range(5):
					dat.append(Struct.float(data[ptr+j*4:ptr+j*4+4], endian='>'))
				print("  * Coords: [",', '.join(["%f"%x for x in dat]),"]")
				ptr += 0x14
				self.TextureCoords.append(dat)
			for i in range(self.FlagData.m_2):
				dat = Struct.uint32(data[ptr:ptr+4], endian='>')
				self.SthB.append(dat)
				print("  * SthB: %08x"%dat)
				ptr += 0x04
			if self.FlagData.m_7:
				self.SthI = Struct.uint32(data[ptr:ptr+4], endian='>')
				print("  SthI: %08x"%self.SthI)
				ptr += 0x04
			if self.FlagData.m_8:
				self.SthJ = Struct.uint32(data[ptr:ptr+4], endian='>')
				print("  SthJ: %08x"%self.SthJ)
				ptr += 0x04
			if self.FlagData.m_4:
				self.SthC = Struct.uint32(data[ptr:ptr+4], endian='>')
				print("  SthC: %08x"%self.SthC)
				ptr += 0x04
			for i in range(self.FlagData.m_6):
				dat = []
				for j in range(5):
					dat.append(Struct.float(data[ptr+j*4:ptr+j*4+4], endian='>'))
				self.SthD.append(dat)
				print("  * SthD: [",', '.join(["%f"%x for x in dat]),"]")
				ptr += 0x14
			for i in range(self.FlagData.m_5):
				dat = Struct.uint32(data[ptr:ptr+4], endian='>')
				self.SthE.append(dat)
				print("  * SthE: %08x"%dat)
				ptr += 0x04
			for i in range(self.FlagData.m_3):
				dat = []
				for j in range(4):
					dat.append(Struct.uint32(data[ptr+j*4:ptr+j*4+4], endian='>'))
				self.SthF.append(dat)
				print("  * SthF: [",', '.join(["%08x"%x for x in dat]),"]")
				ptr += 0x10
			if self.FlagData.m_9:
				dat = Struct.uint32(data[ptr:ptr+4], endian='>')
				self.SthG = dat
				print("  SthG: %08x"%dat)
				ptr += 0x04
			if self.FlagData.m_10:
				dat = Struct.uint32(data[ptr:ptr+4], endian='>')
				self.SthH = dat
				print("  SthH: %08x"%dat)
				ptr += 0x04
			
			#assert ptr == len(data)
			
		def pack(self):
			
			self.FlagData = Brlyt.BrlytMaterialFlags()
			
			self.FlagData.NumTextures = len(self.Textures)
			self.FlagData.NumCoords = len(self.TextureCoords)
			self.FlagData.m_2 = len(self.SthB)
			self.FlagData.m_7 = self.SthI is not None
			self.FlagData.m_8 = self.SthJ is not None
			self.FlagData.m_4 = self.SthC is not None
			self.FlagData.m_6 = len(self.SthD)
			self.FlagData.m_5 = len(self.SthE)
			self.FlagData.m_3 = len(self.SthF)
			self.FlagData.m_9 = self.SthG is not None
			self.FlagData.m_10 = self.SthH is not None
			
			self.Flags = self.FlagData.pack()

			hdr = Brlyt.BrlytMatHeader()

			hdr.Name = self.Name.encode("ascii") + b"\x00"*(0x14-len(self.Name))
			hdr.Color1 = self.Color1
			hdr.Color2 = self.Color2
			hdr.Color3 = self.Color3
			hdr.Blah = self.Blah
			hdr.Flags = self.Flags

			data = hdr.pack()
			
			for i in self.Textures:
				data += Struct.uint16(i[0], endian='>')
				data += Struct.uint8(i[1], endian='>')
				data += Struct.uint8(i[2], endian='>')
			for i in self.TextureCoords:
				for j in i:
					data += Struct.float(j, endian='>')
			for i in self.SthB:
				data += Struct.uint32(i, endian='>')
			if self.SthI is not None:
				data += Struct.uint32(self.SthI, endian='>')
			if self.SthJ is not None:
				data += Struct.uint32(self.SthJ, endian='>')
			if self.SthC is not None:
				data += Struct.uint32(self.SthC, endian='>')
			for i in self.SthD:
				for j in i:
					data += Struct.float(j, endian='>')
			for i in self.SthE:
				data += Struct.uint32(i, endian='>')
			for i in self.SthF:
				for j in i:
					data += Struct.uint32(j, endian='>')
			if self.SthG is not None:
				data += Struct.uint32(self.SthG, endian='>')
			if self.SthH is not None:
				data += Struct.uint32(self.SthH, endian='>')
			return data
		
	class BrlytPAN1(StdAtom):
		FOURCC = "pan1"
		def __format__(self):
			StdAtom.__format__(self)
			self.Flags = Struct.uint16
			self.Alpha = Struct.uint16
			self.Name = Struct.string(0x18, stripNulls=True)
			self.Coords = Struct.float[10]

	class BrlytPIC1v1(BrlytPAN1):
		FOURCC = "pic1"
		def __format__(self):
			Brlyt.BrlytPAN1.__format__(self)
			self.unk = Struct.uint8[16]
			self.Material = Struct.uint16
			self.Flags2 = Struct.uint16

	class BrlytPIC1v2(BrlytPIC1v1):
		FOURCC = "pic1"
		def __format__(self):
			Brlyt.BrlytPIC1v1.__format__(self)
			self.MaterialCoords = Struct.float[8]

	def __init__(self, archive, data, renderer):
		self.Archive = archive
		self.Textures = Brlyt.BrlytTXL1()
		self.Materials = Brlyt.BrlytMAT1()
		self.LastPic = None
		self.RootPane = None
		self.RootPaneName = None
		self.Objects = {}
		self.PanePath = []
		self.PaneId = 0
		self.Language = b"ENG"
		self.Renderer = renderer
		
		if data != None:
			self.Unpack(data)
	
	def Unpack(self, data):
		pos = 0
		header = self.BrlytHeader()
		header.unpack(data[:len(header)])
		print("BRLYT header:")
		wii.chexdump(data[:len(header)])
		print(" unk1: %08x"%header.Unk)
		print(" unkc: %08x"%header.UnkCount)
		pos += len(header)
		
		print(" %d atoms"%header.AtomCount)
		
		assert header.Magic == 'RLYT'
		
		for i in range(header.AtomCount):
			atom = StdAtom()
			atom.unpack(data[pos:pos+len(atom)])
			
			atomdata = data[pos:pos+atom.Size]
			if atom.FourCC == 'txl1':
				self.TXL1(atomdata)
			elif atom.FourCC == 'lyt1':
				self.LYT1(atomdata)
			elif atom.FourCC == 'mat1':
				self.MAT1(atomdata)
			elif atom.FourCC == 'pan1':
				self.PAN1(atomdata)
			elif atom.FourCC == 'pas1':
				self.PAS1(atomdata)
			elif atom.FourCC == 'pae1':
				self.PAE1(atomdata)
			elif atom.FourCC == 'pic1':
				self.PIC1(atomdata)
			elif atom.FourCC == "grp1":
				self.GRP1(atomdata)
			else:
				print("Unknown FOURCC:",atom.FourCC)
				wii.chexdump(atomdata)
			
			pos += atom.Size
	
	def _PackObject(self, object):
		atoms = 0
		if isinstance(object,Pane):
			atoms += 1
			if isinstance(object,Picture):
				atom = Brlyt.BrlytPIC1v2()
			else:
				atom = Brlyt.BrlytPAN1()
			atom.Name = object.Name.encode("ascii") + b"\x00"*(0x18-len(object.Name))
			atom.Alpha = int(object.Alpha * 256)
			atom.Flags = object.Flags
			atom.Coords = object.Coords
			if isinstance(object,Picture):
				atom.Flags2 = object.Flags2
				atom.Material = object.Material
				atom.unk = object.Unk
				atom.MaterialCoords = sum(object.MaterialCoords,[])
			data = atom.pack()
			
			if len(object.Children) > 0:
				atoms += 2
				data += b"pas1\x00\x00\x00\x08"
				for child in object.Children:
					ac, dc = self._PackObject(child)
					data += dc
					atoms += ac
				data += b"pae1\x00\x00\x00\x08"
			return atoms, data
		else:
			raise ValueError("Unknown object: "+repr(object))
	
	def Pack(self, extra=None):
		
		header = self.BrlytHeader()
		header.Unk = 0xfeff0008
		header.UnkCount = 0x10
		header.Magic = "RLYT"
		header.AtomCount = 0
		
		data = b""
		data += self.mkLYT1()
		header.AtomCount+=1
		
		data += self.mkTXL1()
		header.AtomCount+=1
		
		data += self.mkMAT1(extra)
		header.AtomCount+=1
		
		atoms, ndata = self._PackObject(self.RootPane)
		data += ndata
		header.AtomCount += atoms
		
		#uuugly. TODO: fix this crap.
		data += b"grp1\x00\x00\x00\x1cRootGroup\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
		header.AtomCount += 1
		
		header.Size = len(data) + len(header)
		data = header.pack() + data
		
		return data
	
	def mkLYT1(self):
		lyt1 = self.BrlytLYT1()
		lyt1.Width = self.Width
		lyt1.Height = self.Height
		lyt1.Flag = 1
		for n in range(3):
			lyt1.pad[n] = 0
		return lyt1.pack()
	
	def mkTXL1(self):
		return self.Textures.pack()
	
	def mkMAT1(self, extra=None):
		return self.Materials.pack(extra)
	
	def LYT1(self, data):
		lyt1 = self.BrlytLYT1()
		lyt1.unpack(data)
		self.Width = lyt1.Width
		self.Height = lyt1.Height
		print("LYT1: %f x %f, flag %d"%(self.Width, self.Height, lyt1.Flag))
		self.Renderer.Create(int(self.Width), int(self.Height))

	def TXL1(self, data):
		self.Textures = self.BrlytTXL1(self.Archive, data)
		for i in self.Textures:
			i.create_texture()
	
	def ApplyMask(self, image, mask):
		print("Making mask:",image,mask)
		print(image.width,image.height,mask.width,mask.height)
		if image.height != mask.height or image.width != mask.width:
			raise ValueError("Mask dimensions must be equal to mask dimensions")
		newdata = [0 for x in range(image.height * image.width * 4)]
		
		for pix in range(image.height * image.width):
			newdata[pix*4 + 0] = image.data[pix*4 + 0]
			newdata[pix*4 + 1] = image.data[pix*4 + 1]
			newdata[pix*4 + 2] = image.data[pix*4 + 2]
			newdata[pix*4 + 3] = mask.data[pix*4 + 0]
		
		return  ImageData(image.width, image.height, 'RGBA', ''.join(newdata))
	
	def MAT1(self, data):
		self.Materials = self.BrlytMAT1(data)
	
	def _addpane(self, p):
		self.CurPane = p
		self.Objects[p.Name] = p
		if self.RootPane is None:
			self.RootPane = p
		else:
			self.PanePath[-1].Add(p)
	
	def PAN1(self, data):
		wii.chexdump(data)
		pane = Brlyt.BrlytPAN1()
		pane.unpack(data)
		p = Pane(pane.Name, pane.Flags, pane.Alpha/256.0, pane.Coords)
		print('Pane %s (flags %04x, alpha %f): ' % (p.Name, pane.Flags, pane.Alpha),pane.Coords)
		self._addpane(p)
	
	def PAS1(self, data):
		wii.chexdump(data)
		if self.CurPane is None:
			raise ValueError("No current pane!")
		self.PanePath.append(self.CurPane)
		print("Pane start:",'.'.join(map(str,self.PanePath)))
		self.CurPane = None
	
	def PAE1(self, data):
		print("Pane end:",'.'.join(map(str,self.PanePath)))
		self.PanePath = self.PanePath[:-1]
	
	def PIC1(self, data):
		wii.chexdump(data)
		if len(data) == len(Brlyt.BrlytPIC1v1()):
			pic = Brlyt.BrlytPIC1v1()
		else:
			pic = Brlyt.BrlytPIC1v2()
		pic.unpack(data)

		kw = {}
		if not isinstance(pic, Brlyt.BrlytPIC1v1):
			mc = []
			for i in range(4):
				mc.append(pic.MaterialCoords[i*2:i*2+2])
			kw["matcoord"] = mc

		p = Picture(pic.Name, pic.Flags, pic.Alpha/256.0, pic.Coords, pic.unk, pic.Material, pic.Flags2, *kw)
		print(repr(p.Name))
		mat = self.Materials[pic.Material]
		if mat is not None:
			self._addpane(p)
		else:
			print('Picture %s with null material!')
		
	def GRP1(self, data):
		wii.chexdump(data)
		if len(data) < 0x1c:
			pass
		lang = data[0x8:0x18].split(b'\0', 1)[0]
		nitems = Struct.uint16(data[0x18:0x1a], endian='>')
		p = 0x1c
		items = []
		for i in range(nitems):
			items.append(data[p:p+0x10].split(b'\0', 1)[0].decode("ascii"))
			p += 0x10
		for i in items:
			if i not in self.Objects:
				print("Missing object:", i)
				continue
			if lang != self.Language:
				self.Objects[i].Enabled = False
			else:
				self.Objects[i].Enabled = True

class Brlan(object):
	A_COORD = "RLPA"
	A_PARM = "RLVC"
	C_X = 0
	C_Y = 1
	C_ANGLE = 5
	C_MAGX = 6
	C_MAGY = 7
	C_WIDTH = 8
	C_HEIGHT = 9
	P_ALPHA = 0x10
	class BrlanHeader(Struct):
		__endian__ = Struct.BE
		def __format__(self):
			self.Magic = Struct.string(4)
			self.Unk = Struct.uint32
			self.Size = Struct.uint32
			self.UnkCount = Struct.uint16
			self.AtomCount = Struct.uint16
	
	def __init__(self, data=None):
		self.Anim = None
		if data != None:
			self.Unpack(data)
		else:
			self.Anim = Brlan.BrlanPAI1()
	
	def Unpack(self, data):
		header = self.BrlanHeader()
		header.unpack(data[:len(header)])
		pos = len(header)
		
		assert header.Magic == 'RLAN'
		
		for i in range(header.AtomCount):
			atom = StdAtom()
			atom.unpack(data[pos:pos+len(atom)])
			atomdata = data[pos:pos+atom.Size]
			pos += atom.Size
			
			if atom.FourCC == 'pai1':
				self.PAI1(atomdata)
			else:
				print("Unknown animation atom: %s"%atom.FourCC)
	
	class BrlanPAI1(ItemList):
		LSIZE=4
		OFFSET=0
		FOURCC="pai1"
		HDRLEN=12
		def unpack(self, data):
			self.FrameCount = Struct.uint16(data[8:10], endian='>')
			print(self.FrameCount)
			ItemList.unpack(self, data)
		def __mkheader__(self):
			hdr = Struct.uint16(self.FrameCount, endian='>')
			hdr += Struct.uint16(0x100, endian='>')
			hdr += Struct.uint32(len(self.Items), endian='>')
			hdr += Struct.uint32(0x14, endian='>')
			return hdr
		def __unpkcnt__(self, data):
			return Struct.uint32(data[4:8], endian='>')
		def __getlistoff__(self, data):
			return Struct.uint32(data[8:12], endian='>')
		def unpack_item	(self, num, data):
			self.Items.append(Brlan.BrlanAnimSet(data))
		def pack(self, offset, count):
			self.FrameCount = count
			self.Offset = offset
			return ItemList.pack(self)
		def pack_item (self, num, item):
			return item.pack(self.Offset)
		def __getitem__(self, n):
			if isinstance(n,str):
				for i in self.Items:
					if i.Name == n:
						return i
				raise KeyError("Key %s not found"%n)
			else:
				return ItemList.__getitem__(self, n)
					
		def __contains__(self, n):
			if isinstance(n,str):
				for i in self.Items:
					if i.Name == n:
						return True
				return False
			else:
				return n in self.Items
				
			
	
	class BrlanAnimSet(ItemList):
		LSIZE=4
		OFFSET=0
		HDRLEN=0x18
		IS_ATOM=False
		def __init__(self, data=None):
			if data is not None and len(data)<0x14:
				self.Name = data
				ItemList.__init__(self, None)
			else:
				ItemList.__init__(self, data)
		def unpack(self, data):
			self.Name = data[:0x14].split(b"\0",1)[0].decode("ascii")
			print(self.Name)
			ItemList.unpack(self, data)
		def __mkheader__(self):
			hdr = self.Name.encode("ascii") + b"\x00" * (0x14-len(self.Name))
			hdr += Struct.uint8(len(self.Items), endian='>')
			hdr += b"\x00\x00\x00"
			return hdr
		def __unpkcnt__(self, data):
			return Struct.uint8(data[0x14:0x15], endian='>')
		def unpack_item	(self, num, data):
			self.Items.append(Brlan.BrlanAnimClass(data))
		def pack_item (self, num, item):
			return item.pack(self.Offset)
		def pack(self, offset):
			self.Offset = offset
			return ItemList.pack(self)
		def __getitem__(self, n):
			if isinstance(n,str):
				for i in self.Items:
					if i.Type == n:
						return i
				raise KeyError("Key %s not found"%n)
			else:
				return ItemList.__getitem__(self, n)
			
	class BrlanAnimClass(ItemList):
		LSIZE=4
		OFFSET=0
		HDRLEN=8
		IS_ATOM=False
		
		class BrlanAnimClassIterator(object):
			def __init__(self, cl):
				self.cl = cl
				self.item = 0
			def __iter__(self):
				return self
			def __next__(self):
				if self.item == len(self.cl.Items):
					raise StopIteration()
				else:
					self.item += 1
					return self.cl.Items[self.item-1]
		
		def __init__(self, data=None):
			if data is not None and len(data) == 4:
				self.Type = data
				ItemList.__init__(self, None)
			else:
				ItemList.__init__(self, data)
		def unpack(self, data):
			self.Type = data[0:4].decode("ascii")
			print(" ",self.Type)
			ItemList.unpack(self, data)
		def __mkheader__(self):
			hdr = self.Type.encode("ascii")
			hdr += Struct.uint8(len(self.Items), endian='>')
			hdr += b"\x00\x00\x00"
			return hdr
		def __unpkcnt__(self, data):
			return Struct.uint8(data[4:5], endian='>')
		def unpack_item (self, num, data):
			self.Items.append(Brlan.BrlanAnim(data))
		def pack(self, offset):
			self.Offset = offset
			return ItemList.pack(self)
		def pack_item (self, num, item):
			return item.pack(self.Offset)
		def __getitem__(self, n):
			for i in self.Items:
				if i.Type == n:
					return i
			raise KeyError("Key %d not found"%n)
		def __iter__(self):
			return self.BrlanAnimClassIterator(self)
			
	class BrlanAnim(object):
		def __init__(self, data=None):
			self.Triplets = []
			self.Unk = 0x200
			self.Type = 0
			if data is not None and isinstance(data,int):
				self.Type = data
			else:
				self.unpack(data)
		def unpack(self, data):
			self.Type = Struct.uint16(data[0:2], endian='>')
			self.Unk = Struct.uint16(data[2:4], endian='>')
			count = Struct.uint16(data[4:6], endian='>')
			pos = Struct.uint32(data[8:12], endian='>')
			print("  ",self.Type)
			if self.Unk == 0x200:
				print("      Triplets:")
				for i in range(count):
					F = Struct.float(data[pos+0:pos+4], endian='>')
					P = Struct.float(data[pos+4:pos+8], endian='>')
					D = Struct.float(data[pos+8:pos+12], endian='>')
					print("         %11f %11f %11f"%(F,P,D))
					self.Triplets.append((F,P,D))
					pos += 12
			else:
				print("      Unknown format: %04x"%self.Unk)
		def pack(self, offset):
			self.Triplets.sort(key=lambda x: x[0])
			t = self.Triplets
			self.Triplets = []
			lt = (None, None, None)
			for i in t:
				if i == lt:
					pass
				elif i[0] == lt[0]:
					raise ValueError("Duplicate triplet frame numbers with different data")
				else:
					self.Triplets.append(i)
				
			out = Struct.uint16(self.Type, endian='>')
			out += Struct.uint16(self.Unk, endian='>')
			out += Struct.uint16(len(self.Triplets), endian='>')
			out += b"\x00\x00"
			out += Struct.uint32(0xc, endian='>')
			for F,P,D in self.Triplets:
				out += Struct.float(F-offset, endian='>')
				out += Struct.float(P, endian='>')
				out += Struct.float(D, endian='>')
			return out
		def add(self, F, P=None, D=None):
			if isinstance(F,tuple):
				self.Triplets.append(F)
			else:
				self.Triplets.append((F,P,D))
		def rep(self, start, end, times):
			replist = []
			for i in self.Triplets:
				if i[0] >= start and i[0] <= end:
					replist.append(i)
			if len(replist) == 0:
				return
			off = end-start
			for i in range(times-1):
				for a,b,c in replist:
					self.Triplets.append((a+off,b,c))
				off += end-start
			a,b,c = replist[0]
			if a == start:
				self.Triplets.append((a+off,b,c))
		def repsimple(self, start, end, times, sp, sd, ep, ed):
			step = float(end-start) / times
			self.Triplets.append((start, sp, sd))
			self.Triplets.append((start+step/2.0, ep, ed))
			self.rep(start, start+step, times)
		def calc(self, frame):
			for tn in range(len(self.Triplets)):
				if self.Triplets[tn][0] > frame:
					if tn == 0:
						return self.Triplets[0][1]
					else:
						start, value1, coef1 = self.Triplets[tn-1]
						end, value2, coef2 = self.Triplets[tn]
						t = (frame - start) / (end - start)
						nf = end - start
						return \
							coef1  * 1 * nf * (t + t**3 - 2*t**2) + \
							coef2  * 1 * nf * (t**3 - t**2) + \
							value1 * (1 + (2*t**3 - 3*t**2)) + \
							value2 * (-2*t**3 + 3*t**2)
			else:
				return self.Triplets[-1][1]
		def __getitem__(self, n):
			return self.Triplets[n]
	def PAI1(self, data):
		assert self.Anim is None
		self.Anim = self.BrlanPAI1(data)
		return
	def Pack(self, a=None, b=None):
		
		if a is None and b is None:
			frames = self.FrameCount
			offset = 0
		elif a is not None and b is None:
			frames = a
			offset = 0
		elif a is not None and b is not None:
			frames = b-a
			offset = a
		else:
			raise ValueError("WTF are you doing")
		
		header = self.BrlanHeader()
		header.Unk = 0xfeff0008
		header.UnkCount = 0x10
		header.Magic = "RLAN"
		header.AtomCount = 1
		
		data = self.Anim.pack(offset, frames)
		
		header.Size = len(data) + len(header)
		data = header.pack() + data
		
		return data

if have_pyglet:
	class BannerWindow(window.Window):
		def on_resize(self, width, height):
			glViewport(0, 0, width, height)
			glMatrixMode(GL_PROJECTION)
			glLoadIdentity()
			glOrtho(-width/2, width/2, -height/2, height/2, -1, 1)
			glMatrixMode(GL_MODELVIEW)


class Renderer(object):
	def __init__(self):
		self.Brlyt = None
		self.Brlan = None
	
	def Create(self, width, height):
		self.Width = width
		self.Height = height
		print("Render: %f x %f"%(self.Width,self.Height))
		self.Window = BannerWindow(self.Width, self.Height)
		self.Window.set_exclusive_mouse(False)
		
		glClearColor(0.0, 0.0, 0.0, 0.0)
		glClearDepth(1.0)
		glDepthFunc(GL_LEQUAL)
		glEnable(GL_DEPTH_TEST)
		
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA)
		glEnable(GL_BLEND)
		glEnable(GL_TEXTURE_2D)
		#clock.set_fps_limit(60)
		
	
	def Render(self, item, wireframe=False):
		if not item.Enabled:
			return
		if isinstance(item, Picture):

			mat = self.Brlyt.Materials[item.Material]
			if not mat.Textures:
				return

			texture = self.Brlyt.Textures[mat.Textures[0][0]].GLTexture
			mtc = mat.TextureCoords[0]
			x, y, a, b, c, rot, xsc, ysc, xs, ys = item.Coords[:10]
			
			xs *= xsc
			ys *= ysc
			#print item.Name,x,y,xs,ys
			xc, yc = x-(xs/2),y-(ys/2)
			
			glPushMatrix()
			glMatrixMode( GL_MODELVIEW )
			glTranslatef( x, y, 0)
			glRotatef(rot,0,0,1)
			glScalef(xs,ys,0)
			
			if not wireframe:
				col = [x/255.0 for x in list(mat.Color2)]
				col[3] = col[3] * item.Alpha / 255.0
				
				tw = texture.tex_coords[6]
				th = texture.tex_coords[7]
				
				mc = item.MaterialCoords
				
				glMatrixMode( GL_TEXTURE )
				glPushMatrix()
				glTranslatef(0.5,0.5,0)
				glTranslatef(mtc[0],mtc[1],0)
				glScalef(mtc[3],mtc[4],0)
				glTranslatef(-0.5,-0.5,0)
				array = (GLfloat * 32)(
					mc[0][0] * tw,  mc[0][1] * th,  0,  1.,
					-0.5,      0.5,      0,     1.,
					mc[1][0] * tw,  mc[1][1] * th,  0,  1.,
						0.5,      0.5,      0,     1.,
					mc[3][0] * tw,  mc[3][1] * th,  0,  1.,
						0.5,     -0.5, 0,     1.,
					mc[2][0] * tw,  mc[2][1] * th,  0,  1.,
					-0.5,     -0.5, 0,     1.)
		
				glColor4f(*col)
				glPushAttrib(GL_ENABLE_BIT)
				glEnable(texture.target)
				glBindTexture(texture.target, texture.id)
				if mat.Textures[0][1] == 0x0:
					glTexParameteri(texture.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
				elif mat.Textures[0][1] == 0x1:
					glTexParameteri(texture.target, GL_TEXTURE_WRAP_S, GL_REPEAT)
				elif mat.Textures[0][1] == 0x2:
					glTexParameteri(texture.target, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT)
				else:
					glTexParameteri(texture.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
				if mat.Textures[0][2] == 0x0:
					glTexParameteri(texture.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
				elif mat.Textures[0][2] == 0x1:
					glTexParameteri(texture.target, GL_TEXTURE_WRAP_T, GL_REPEAT)
				elif mat.Textures[0][2] == 0x2:
					glTexParameteri(texture.target, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT)
				else:
					glTexParameteri(texture.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
					
				glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT)
				glInterleavedArrays(GL_T4F_V4F, 0, array)
				glDrawArrays(GL_QUADS, 0, 4)
				glPopClientAttrib()
				glPopAttrib()
				
				glBindTexture(texture.target, 0)
				glPopMatrix()
				glMatrixMode( GL_MODELVIEW )

			else:
				
				glColor3f(1,0,0)
				glBegin(GL_LINE_STRIP)
				glVertex2f(-0.5,-0.5)
				glVertex2f(0.5,-0.5)
				glVertex2f(0.5,0.5)
				glVertex2f(-0.5,0.5)
				glVertex2f(-0.5,-0.5)
				glColor3f(1,1,1)
				glEnd()
				pass
	
			glPopMatrix()
		if isinstance(item, Pane):
			if item.Coords is not None:
				x, y, a, b, c, rot, xsc, ysc, xs, ys = item.Coords[:10]
			else:
				x = y = 0
				xs = ys = 0
				xsc = 1
				ysc = 1
			glPushMatrix()
			glTranslatef(x, y, 0)
			glRotatef(rot,0,0,1)
			glScalef(xsc,ysc,0)
			#glScalef(xs,ys,1)
			for child in item.Children:
				self.Render(child,wireframe)
			glPopMatrix()

	def Animate(self, frame):
		for set in self.Brlan.Anim:
			for clss in set:
				for anim in clss:
					if set.Name not in self.Brlyt.Objects:
						print("Missing object for animation: %s (%s)" % (set.Name, clss.Type))
						continue
					#print(set.Name, clss.Type, anim.Type, anim.calc(frame), frame)
					if clss.Type == Brlan.A_COORD:
						self.Brlyt.Objects[set.Name].Coords[anim.Type] = anim.calc(frame)
					elif clss.Type == Brlan.A_PARM:
						if anim.Type == Brlan.P_ALPHA:
							self.Brlyt.Objects[set.Name].Alpha = anim.calc(frame)

	def MainLoop(self, loop):
		frame = 0
		print("Starting mainloop: loop =",loop)
		if self.Brlan:
			print("Length in frames:",self.Brlan.Anim.FrameCount)	
		while not self.Window.has_exit:
			self.Window.dispatch_events()
			self.Window.clear()
			
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
			glLoadIdentity()
			glColor4f(1.0, 1.0, 1.0, 1.0)
			
			self.Render(self.Brlyt.RootPane,False)
			#self.Render(self.Brlyt.RootPane,True)
			
			clock.tick()
			
			self.Window.flip()
			if self.Brlan is not None:
				self.Animate(frame)
				if frame >= self.Brlan.Anim.FrameCount:
					if not loop:
						print("Animation done!")
						return True
					else:
						print("Looping...")
						frame = -1
			frame += 1

class Alameda(object):
	class IMETHeader(Struct):
		__endian__ = Struct.BE
		def __format__(self):
			self.Zeroes = Struct.uint8[0x40]
			self.IMET = Struct.string(4)
			self.Fixed = Struct.uint8[8]
			self.Sizes = Struct.uint32[3]
			self.Flag1 = Struct.uint32
			self.Names = Struct.string(0x2A<<1, encoding='utf-16-be', stripNulls=True)[7]
			self.Zeroes = Struct.uint8[0x348]
			self.MD5 = Struct.uint8[0x10]
	
	def __init__(self, fn, type='icon'):
		renderer = Renderer()
		
		fp = open(fn, 'rb')
		
		imet = self.IMETHeader()
		try:
			imet.unpack(fp.read(len(imet)))
		except:
			imet = None
		if imet is None or imet.IMET != 'IMET':
			imet = self.IMETHeader()
			fp.seek(0x40)
			imet.unpack(fp.read(len(imet)))
			assert imet.IMET == 'IMET'
		
		print('English title: %s' % imet.Names[1])
		
		
		root = U8(fp.read())
		if type == 'icon':
			banner = U8(IMD5(root.Files['./meta/icon.bin']))
			renderer.Brlyt = Brlyt(banner, banner.Files['./arc/blyt/icon.brlyt'], renderer)
			fd = None
			try:
				fd = banner.Files['./arc/anim/icon.brlan']
			except:
				pass
			if fd is not None:
				renderer.Brlan = Brlan(fd)

			loop = True
		else:
			banner = U8(IMD5(root.Files['./meta/banner.bin']))
			renderer.Brlyt = Brlyt(banner, banner.Files['./arc/blyt/banner.brlyt'], renderer)
			loop = './arc/anim/banner_start.brlan' not in banner.Files
			loop_anim = None

			if not loop:
				renderer.Brlan = Brlan(banner.Files['./arc/anim/banner_start.brlan'])
				if './arc/anim/banner_loop.brlan' in banner.Files:
					loop_anim = Brlan(banner.Files['./arc/anim/banner_loop.brlan'])
			else:
				renderer.Brlan = Brlan(banner.Files['./arc/anim/banner.brlan'])
				if './arc/anim/banner.brlan' in banner.Files:
					renderer.Brlan = Brlan(banner.Files['./arc/anim/banner.brlan'])
				else:
					renderer.Brlan = None
				
		if renderer.MainLoop(loop) and type == 'banner' and not loop:
			renderer.Brlan = loop_anim
			renderer.MainLoop(True)

if __name__=='__main__':
	Alameda(*sys.argv[1:])
