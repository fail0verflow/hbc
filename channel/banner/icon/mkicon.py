import sys, os, struct, random

sys.path.append(os.environ['ALAMEDA']+"/../Common")
sys.path.append(os.environ['ALAMEDA'])

from Alameda import *

brlyt = Brlyt(None, None, None)
brlyt.Width = 170
brlyt.Height = 96

brlyt.Textures.add(Brlyt.BrlytTexture("icon_title.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("white.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("icon_wavea.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("icon_waveb.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("icon_wave1a.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("icon_wave1b.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("icon_shape2.tpl"))
brlyt.Textures.add(Brlyt.BrlytTexture("icon_fade.tpl"))

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
brlyt.Materials[2].TextureCoords.append([0,2,0,4,6])
brlyt.Materials[2].SthB.append(0x01041e00)

brlyt.Materials.add(Brlyt.BrlytMaterial("waveb"))
brlyt.Materials[3].Textures.append((3,1,0))
brlyt.Materials[3].TextureCoords.append([0,2,0,4,6])
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

bkg = Picture("background")
bkg.Material = 1
bkg.X, bkg.Y, bkg.Width, bkg.Height = 0,0,170,96

tit = Picture("title")
tit.Material = 0
tit.X, tit.Y, tit.Width, tit.Height = 0,-7,110,49

wavea = Picture("wavea")
wavea.Material = 2
wavea.X, wavea.Y, wavea.Width, wavea.Height = -171,-5,1024,96

waveb = Picture("waveb")
waveb.Material = 3
waveb.X, waveb.Y, waveb.Width, waveb.Height = -171,-3,1024,96

wave1a = Picture("wave1a")
wave1a.Material = 4
wave1a.X, wave1a.Y, wave1a.Width, wave1a.Height = -75,27,94,8

wave1b = Picture("wave1b")
wave1b.Material = 5
wave1b.X, wave1b.Y, wave1b.Width, wave1b.Height = 65,27,130,9

shadow = Picture("shadow")
shadow.Material = 6
shadow.X, shadow.Y, shadow.Width, shadow.Height = -45,21,159,7

fade = Picture("fade")
fade.Material = 7
fade.X, fade.Y, fade.Width, fade.Height = 0,-16,170,80


waterpane.Add(wavea)
waterpane.Add(waveb)
waterpane.Add(wave1a)
waterpane.Add(wave1b)
waterpane.Add(shadow)
waterpane.Add(fade)

brlyt.RootPane.Add(bkg)
brlyt.RootPane.Add(waterpane)
brlyt.RootPane.Add(tit)

brldata = brlyt.Pack()

open(sys.argv[1],"wb").write(brldata)


brlan = Brlan()
brlan.Anim.add(Brlan.BrlanAnimSet("water"))
brlan.Anim.add(Brlan.BrlanAnimSet("wavea"))
brlan.Anim.add(Brlan.BrlanAnimSet("waveb"))
brlan.Anim.add(Brlan.BrlanAnimSet("wave1a"))
brlan.Anim.add(Brlan.BrlanAnimSet("wave1b"))
brlan.Anim.add(Brlan.BrlanAnimSet("shadow"))
brlan.Anim.add(Brlan.BrlanAnimSet("title"))

brlan.Anim['title'].add(Brlan.BrlanAnimClass(Brlan.A_COORD))
brlan.Anim['title'][Brlan.A_COORD].add(Brlan.BrlanAnim(Brlan.C_Y))
brlan.Anim['title'][Brlan.A_COORD][Brlan.C_Y].repsimple(0, 960, 8, -7, 0, -11, 0)

for i in ['wavea', 'waveb', 'wave1a', 'wave1b', 'shadow']:
	brlan.Anim[i].add(Brlan.BrlanAnimClass(Brlan.A_COORD))
	brlan.Anim[i][Brlan.A_COORD].add(Brlan.BrlanAnim(Brlan.C_X))
	brlan.Anim[i][Brlan.A_COORD].add(Brlan.BrlanAnim(Brlan.C_Y))

brlan.Anim['wavea'][Brlan.A_COORD][Brlan.C_X].repsimple(0, 960, 2, -130, 0, 130, 0)
brlan.Anim['waveb'][Brlan.A_COORD][Brlan.C_X].repsimple(0, 960, 2, -130, 2.0, 130, 2.0)
brlan.Anim['wavea'][Brlan.A_COORD][Brlan.C_Y].repsimple(0, 960, 3, -5, 0, 1, 0)
brlan.Anim['waveb'][Brlan.A_COORD][Brlan.C_Y].repsimple(0, 960, 4, -3, 0, 3, 0)

brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_X].Triplets.append((0,  -75,   0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((0,   27,   0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_X].Triplets.append((100,  0,   0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((100,  29,  0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_X].Triplets.append((200,  50,  0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((200,  27,  0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_X].Triplets.append((400,  -75, 0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((400,  27,  0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_X].Triplets.append((500,  -40, 0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((500,  30,  0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_X].Triplets.append((600,  -40, 0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((600,  30,  0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_X].Triplets.append((750,  15,  0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((750,  28,  0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_X].Triplets.append((960,  -75, 0.2))
brlan.Anim['wave1a'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((960,  27,  0.2))

brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_X].Triplets.append((0,  65,    0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((0,   27,   0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_X].Triplets.append((120,  10,  0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((120,  29,  0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_X].Triplets.append((190,  65,  0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((190,  27,  0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_X].Triplets.append((430,  20,  0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((430,  27,  0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_X].Triplets.append((510,  -20, 0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((510,  30,  0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_X].Triplets.append((670,  -40, 0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((670,  30,  0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_X].Triplets.append((710,  0,   0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((710,  28,  0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_X].Triplets.append((960,  65,  0.2))
brlan.Anim['wave1b'][Brlan.A_COORD][Brlan.C_Y].Triplets.append((960,  27,  0.2))

brlan.Anim['shadow'][Brlan.A_COORD][Brlan.C_Y].repsimple(0, 960, 4, 21, -0.1, 25, -0.1)
brlan.Anim['shadow'][Brlan.A_COORD][Brlan.C_X].repsimple(0, 960, 2, -45, -0.1, 45, -0.1)


bradata = brlan.Pack(60*16)
for a,b,c in brlan.Anim['waveb'][Brlan.A_COORD][Brlan.C_X].Triplets:
	print(a,b,c)

open(sys.argv[2],"wb").write(bradata)
