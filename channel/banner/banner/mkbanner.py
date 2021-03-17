import sys, os, random

sys.path.append(os.environ['ALAMEDA']+"/../Common")
sys.path.append(os.environ['ALAMEDA'])

from Alameda import *

brlyt = Brlyt(None, None, None)
brlyt.Width = 810
brlyt.Height = 456

brlyt.Textures.add(Brlyt.BrlytTexture("banner_title.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("white.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("banner_wavea.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("banner_waveb.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("banner_wave1a.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("banner_wave1b.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("banner_shape2.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("banner_fade.tpl"))

brlyt.Materials.add(Brlyt.BrlytMaterial("title"))
brlyt.Materials[0].Textures.append((0,0,0))
brlyt.Materials[0].TextureCoords.append([0,0,0,1,1])
brlyt.Materials[0].SthB.append(0x01041e00)

brlyt.Materials.add(Brlyt.BrlytMaterial("white"))
brlyt.Materials[1].Textures.append((1,1,1))
brlyt.Materials[1].TextureCoords.append([0,0,0,1,1])
brlyt.Materials[1].SthB.append(0x01041e00)

brlyt.Materials.add(Brlyt.BrlytMaterial("wavea"))
brlyt.Materials[2].Textures.append((2,1,0))
brlyt.Materials[2].TextureCoords.append([0,2,0,3,6])
brlyt.Materials[2].SthB.append(0x01041e00)

brlyt.Materials.add(Brlyt.BrlytMaterial("waveb"))
brlyt.Materials[3].Textures.append((3,1,0))
brlyt.Materials[3].TextureCoords.append([0,2,0,3,6])
brlyt.Materials[3].SthB.append(0x01041e00)

brlyt.Materials.add(Brlyt.BrlytMaterial("wave1a"))
brlyt.Materials[4].Textures.append((4,0,0))
brlyt.Materials[4].TextureCoords.append([0,0,0,1,1])
brlyt.Materials[4].SthB.append(0x01041e00)

brlyt.Materials.add(Brlyt.BrlytMaterial("wave1b"))
brlyt.Materials[5].Textures.append((5,0,0))
brlyt.Materials[5].TextureCoords.append([0,0,0,1,1])
brlyt.Materials[5].SthB.append(0x01041e00)

brlyt.Materials.add(Brlyt.BrlytMaterial("shape2"))
brlyt.Materials[6].Textures.append((6,0,0))
brlyt.Materials[6].TextureCoords.append([0,0,0,1,1])
brlyt.Materials[6].SthB.append(0x01041e00)

brlyt.Materials.add(Brlyt.BrlytMaterial("fade"))
brlyt.Materials[7].Textures.append((7,0,0))
brlyt.Materials[7].TextureCoords.append([0,0,0,1,1])
brlyt.Materials[7].SthB.append(0x01041e00)

brlyt.RootPane = Pane("RootPane")
brlyt.RootPane.Width = brlyt.Width
brlyt.RootPane.Height = brlyt.Height

waterpane = Pane("water")
bubblepane = Pane("bubbles")

bkg = Picture("background")
bkg.Material = 1
bkg.X, bkg.Y, bkg.Width, bkg.Height = 0,0,brlyt.Width,brlyt.Height

tit = Picture("title")
tit.Material = 0
tit.X, tit.Y, tit.Width, tit.Height = 0,32,400,180

wavea = Picture("wavea")
wavea.Material = 2
wavea.X, wavea.Y, wavea.Width, wavea.Height = -300,35,3072,384

waveb = Picture("waveb")
waveb.Material = 3
waveb.X, waveb.Y, waveb.Width, waveb.Height = -300,32,3072,384

wave1a = Picture("wave1a")
wave1a.Material = 4
wave1a.X, wave1a.Y, wave1a.Width, wave1a.Height = -200,160,382,32

wave1b1 = Picture("wave1b1")
wave1b1.Material = 5
wave1b1.X, wave1b1.Y, wave1b1.Width, wave1b1.Height = 200,170,527,37

wave1b2 = Picture("wave1b2")
wave1b2.Material = 5
wave1b2.X, wave1b2.Y, wave1b2.Width, wave1b2.Height = -380,170,527,37

shadow = Picture("shadow")
shadow.Material = 6
shadow.X, shadow.Y, shadow.Width, shadow.Height = -180,150,644,28

fade = Picture("fade")
fade.Material = 7
fade.X, fade.Y, fade.Width, fade.Height = 0,8,brlyt.Width,256

boom = Picture("boom")
boom.Material = 1
boom.Alpha = 0
boom.X, boom.Y, boom.Width, boom.Height = 0,0,brlyt.Width,brlyt.Height

waterpane.Add(wavea)
waterpane.Add(waveb)
waterpane.Add(wave1a)
waterpane.Add(wave1b1)
waterpane.Add(wave1b2)
waterpane.Add(shadow)
waterpane.Add(fade)
waterpane.Add(bubblepane)

brlyt.RootPane.Add(bkg)
brlyt.RootPane.Add(waterpane)
brlyt.RootPane.Add(tit)
brlyt.RootPane.Add(boom)

fakeStart = -60 * 10
loopStart = 60 * 6
loopEnd = 60 * 22

brlan = Brlan()
brlan.Anim.add(Brlan.BrlanAnimSet("water"))
brlan.Anim.add(Brlan.BrlanAnimSet("wavea"))
brlan.Anim.add(Brlan.BrlanAnimSet("waveb"))
brlan.Anim.add(Brlan.BrlanAnimSet("wave1a"))
brlan.Anim.add(Brlan.BrlanAnimSet("wave1b1"))
brlan.Anim.add(Brlan.BrlanAnimSet("wave1b2"))
brlan.Anim.add(Brlan.BrlanAnimSet("shadow"))
brlan.Anim.add(Brlan.BrlanAnimSet("title"))
brlan.Anim.add(Brlan.BrlanAnimSet("boom"))

brlan.Anim['title'].add(Brlan.BrlanAnimClass(Brlan.A_COORD))
brlan.Anim['title'][Brlan.A_COORD].add(Brlan.BrlanAnim(Brlan.C_Y))
brlan.Anim['title'][Brlan.A_COORD][Brlan.C_Y].repsimple(fakeStart, loopEnd, 12, 32, 0, 20, 0)

brlan.Anim['title'].add(Brlan.BrlanAnimClass(Brlan.A_PARM))
brlan.Anim['title'][Brlan.A_PARM].add(Brlan.BrlanAnim(Brlan.P_ALPHA))
brlan.Anim['title'][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((0, 0, 0))
brlan.Anim['title'][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((243, 0, 0))
brlan.Anim['title'][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((244, 255, 0))

brlan.Anim['boom'].add(Brlan.BrlanAnimClass(Brlan.A_PARM))
brlan.Anim['boom'][Brlan.A_PARM].add(Brlan.BrlanAnim(Brlan.P_ALPHA))
brlan.Anim['boom'][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((0, 0, 0))
brlan.Anim['boom'][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((243, 0, 0))
brlan.Anim['boom'][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((244, 255, 0))
brlan.Anim['boom'][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((246, 255, 0))
brlan.Anim['boom'][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((256, 0, 0))


for i in ['wavea', 'waveb', 'wave1a', 'wave1b1', 'wave1b2', 'shadow']:
	brlan.Anim[i].add(Brlan.BrlanAnimClass(Brlan.A_COORD))
	brlan.Anim[i][Brlan.A_COORD].add(Brlan.BrlanAnim(Brlan.C_X))
	brlan.Anim[i][Brlan.A_COORD].add(Brlan.BrlanAnim(Brlan.C_Y))

brlan.Anim['wavea'][Brlan.A_COORD][Brlan.C_X].repsimple(fakeStart, loopEnd, 4, -300, 0, 300, 0)
brlan.Anim['waveb'][Brlan.A_COORD][Brlan.C_X].repsimple(fakeStart, loopEnd, 4, -300, 2.0, 300, 2.0)
brlan.Anim['wavea'][Brlan.A_COORD][Brlan.C_Y].repsimple(fakeStart, loopEnd, 6, 35, 0, 55, 0)
brlan.Anim['waveb'][Brlan.A_COORD][Brlan.C_Y].repsimple(fakeStart, loopEnd, 8, 45, 0, 55, 0)

brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_X].repsimple(fakeStart, loopEnd, 4, -200, 4.0, 200, 4.0)
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_Y].repsimple(fakeStart, loopEnd, 6, 160, 0.2, 180, 0.2)
brlan.Anim['wave1b1'][Brlan.A_COORD][Brlan.C_X].repsimple(fakeStart, loopEnd, 4, 200, 3.2, 400, 3.2)
brlan.Anim['wave1b1'][Brlan.A_COORD][Brlan.C_Y].repsimple(fakeStart, loopEnd, 6, 170, 0.2, 183, 0.2)
brlan.Anim['wave1b2'][Brlan.A_COORD][Brlan.C_X].repsimple(fakeStart, loopEnd, 4, -400, 3.7, -200, 3.7)
brlan.Anim['wave1b2'][Brlan.A_COORD][Brlan.C_Y].repsimple(fakeStart, loopEnd, 6, 165, 0.2, 185, 0.2)

brlan.Anim['shadow'][Brlan.A_COORD][Brlan.C_X].repsimple(fakeStart, loopEnd, 4, -180, 1.4, 100, 1.4)
brlan.Anim['shadow'][Brlan.A_COORD][Brlan.C_Y].repsimple(fakeStart, loopEnd, 6, 150, 0.2, 155, 0.2)

brlan.Anim.add(Brlan.BrlanAnimSet("water"))
brlan.Anim['water'].add(Brlan.BrlanAnimClass(Brlan.A_COORD))
brlan.Anim['water'][Brlan.A_COORD].add(Brlan.BrlanAnim(Brlan.C_Y))
brlan.Anim['water'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((0, -320, 0))
brlan.Anim['water'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((160, 0, 0))

##### Bubble generation #####

random.seed(1)

class BubbleType:
	def __init__(self, name, width, height):
		self.Name = name
		self.TextureName = name + ".tpl"
		self.Width = width
		self.Height = height
		self.PicCtr = 0
	def makemat(self, brlyt):
		self.Brlyt = brlyt
		self.Texture = len(brlyt.Textures)
		brlyt.Textures.add(Brlyt.BrlytTexture(self.TextureName))
		self.Material = len(brlyt.Materials)
		brlyt.Materials.add(Brlyt.BrlytMaterial(self.Name))
		brlyt.Materials[self.Material].Textures.append((self.Texture,0,0))
		brlyt.Materials[self.Material].TextureCoords.append([0,0,0,1,1])
		brlyt.Materials[self.Material].SthB.append(0x01041e00)
	def makepic(self):
		name = "%s_%d"%(self.Name,self.PicCtr)
		pic = Picture(name)
		pic.Material = self.Material
		pic.X, pic.Y, pic.Width, pic.Height = 0,-600,self.Width,self.Height
		self.PicCtr += 1
		return pic

class BubbleInstance:
	YSTART = -150
	YEND = 170
	FADESTART = 0.7
	def __init__(self, start, length=None, x=None, xp=None):
		if x is None:
			x = random.uniform(-brlyt.Width/2-64, brlyt.Width/2+64)
		if xp is None:
			xp = random.uniform(0, 32)
		if length is None:
			length = random.uniform(50, 170)
		self.X = x
		self.XP = xp
		self.Start = start
		self.Length = length
		self.End = self.Start + self.Length
		self.TypeID = None
	def render(self, brlan):
		if self.Picture.Name not in brlan.Anim:
			#print "Adding animation set for %s"%self.Picture.Name
			brlan.Anim.add(Brlan.BrlanAnimSet(self.Picture.Name))
			brlan.Anim[self.Picture.Name].add(Brlan.BrlanAnimClass(Brlan.A_COORD))
			brlan.Anim[self.Picture.Name].add(Brlan.BrlanAnimClass(Brlan.A_PARM))
			brlan.Anim[self.Picture.Name][Brlan.A_COORD].add(Brlan.BrlanAnim(Brlan.C_X))
			brlan.Anim[self.Picture.Name][Brlan.A_COORD].add(Brlan.BrlanAnim(Brlan.C_Y))
			brlan.Anim[self.Picture.Name][Brlan.A_PARM].add(Brlan.BrlanAnim(Brlan.P_ALPHA))
		
		tps = [
			brlan.Anim[self.Picture.Name][Brlan.A_COORD][Brlan.C_X],
			brlan.Anim[self.Picture.Name][Brlan.A_COORD][Brlan.C_Y],
			brlan.Anim[self.Picture.Name][Brlan.A_PARM][Brlan.P_ALPHA]
		]
		
		for i in tps:
			if len(i.Triplets) > 0:
				if i.Triplets[-1][0] >= self.Start:
					print("WTF at %s: %f >= %f"%(self.Picture.Name,i.Triplets[-1][0],self.Start))
					raise RuntimeError("We Have A Problem")
		
		brlan.Anim[self.Picture.Name][Brlan.A_COORD][Brlan.C_X].Triplets.append((self.Start, self.X, 0))
		brlan.Anim[self.Picture.Name][Brlan.A_COORD][Brlan.C_Y].Triplets.append((self.Start, self.YSTART, 1))
		brlan.Anim[self.Picture.Name][Brlan.A_COORD][Brlan.C_X].Triplets.append((self.End, self.X, 0))
		brlan.Anim[self.Picture.Name][Brlan.A_COORD][Brlan.C_Y].Triplets.append((self.End, self.YEND, 1))
		
		fadepoint = self.Start + self.Length * self.FADESTART
		
		brlan.Anim[self.Picture.Name][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((self.Start, 0, 0))
		brlan.Anim[self.Picture.Name][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((self.Start, 255, 0))
		brlan.Anim[self.Picture.Name][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((fadepoint, 255, 0))
		brlan.Anim[self.Picture.Name][Brlan.A_PARM][Brlan.P_ALPHA].Triplets.append((self.End, 0, 0))
	def clone(self):
		new = BubbleInstance(self.Start, self.Length, self.X, self.XP)
		new.TypeID = self.TypeID
		return new

class BubbleCollection:
	def __init__(self, brlyt, brlan, pane):
		self.Brlyt = brlyt
		self.Brlan = brlan
		self.Pane = pane
		self.BubbleTypes = []
		self.Instances = []
	def addtype(self, t, chance):
		self.BubbleTypes.append((t, chance))
	def choosetype(self):
		sumchances = sum([c for t,c in self.BubbleTypes])
		opt = random.uniform(0,sumchances)
		for i,bt in enumerate(self.BubbleTypes):
			t,c = bt
			if c > opt:
				return i
			opt -= c
		return i
	def addinstance(self, i):
		if i.TypeID is None:
			i.TypeID = self.choosetype()
		self.Instances.append(i)
	def cleanInstances(self, time):
		for tis in self.TypeInstances:
			for i, ti in enumerate(tis):
				pic, user = ti
				if user is not None and user.End < time:
					#print "Freeing instance: [%f-%f]"%(user.Start,user.End)
					tis[i] = pic, None
	def printinstances(self):
		print("Type Instances:")
		for tid, tis in enumerate(self.TypeInstances):
			print(" Type Instances for type %d (%s):"%(tid, self.BubbleTypes[tid][0].Name))
			for i, ti in enumerate(tis):
				pic, user = ti
				if user is None:
					print("  %d: Picture %s, free"%(i,pic.Name))
				else:
					print("  %d: Picture %s, user: %s [%f-%f]"%(i,pic.Name,repr(user),user.Start,user.End))
	def render(self):
		for t,c in self.BubbleTypes:
			t.makemat(self.Brlyt)
		self.TypeInstances = []
		for i in range(len(self.BubbleTypes)):
			self.TypeInstances.append([])
		self.Instances.sort(key=lambda x: x.Start)
		for n,i in enumerate(self.Instances):
			#print "Processing instance %d of type %d (%f-%f)"%(n,i.TypeID,i.Start,i.End)
			time = i.Start
			self.cleanInstances(time)
			tis = self.TypeInstances[i.TypeID]
			for nti,ti in enumerate(tis):
				pic, user = ti
				if user is None:
					tis[nti] = (pic,i)
					i.Picture = pic
					#print "Found space"
					break
			else:
				#print "No space for type %d"%i.TypeID
				pic = self.BubbleTypes[i.TypeID][0].makepic()
				self.Pane.Add(pic)
				tis.append((pic, i))
				i.Picture = pic
			i.render(self.Brlan)
		#self.printinstances()

print("Fake Start",fakeStart)
print("Loop Start",loopStart)
print("Loop End",loopEnd)

col = BubbleCollection(brlyt, brlan, bubblepane)
col.addtype(BubbleType("abubble1", 48, 48),1)
col.addtype(BubbleType("abubble2", 32, 32),1)
col.addtype(BubbleType("abubble3", 16, 16),1)
col.addtype(BubbleType("abubble4", 24, 24),1)
col.addtype(BubbleType("abubble5", 32, 32),1)
col.addtype(BubbleType("abubble6", 16, 16),1)
col.addtype(BubbleType("bbubble1", 48, 48),1)
col.addtype(BubbleType("cbubble1", 64, 64),1)
col.addtype(BubbleType("cbubble2", 16, 16),1)

bubbleBoom = 190

# bubble mania!
for i in range(100):
	col.addinstance(BubbleInstance(bubbleBoom))

# now fill things
for i in range(280):
	start = random.uniform(bubbleBoom, loopEnd)
	col.addinstance(BubbleInstance(start))

# clear the ones that overrun the loop
for i in col.Instances:
	if i.End > loopEnd:
		col.Instances.remove(i)

# and copy the loop start bubbles to the end
for i in col.Instances:
	if i.Start < loopStart and i.End > loopStart:
		new = i.clone()
		new.Start -= loopStart
		new.Start += loopEnd
		new.End -= loopStart
		new.End += loopEnd
		col.Instances.append(new)

col.render()

brldata = brlyt.Pack()
open(sys.argv[1],"wb").write(brldata)

bradata = brlan.Pack(loopStart)
open(sys.argv[2],"wb").write(bradata)

bradata = brlan.Pack(loopStart, loopEnd)
open(sys.argv[3],"wb").write(bradata)
