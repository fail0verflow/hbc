#!/usr/bin/env python3
# Copyright 2008  Hector Martin  <marcan@marcansoft.com>
# Licensed under the terms of the GNU GPL, version 2
# http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

from struct import unpack, pack

import os, os.path
import sys

try:
    from Cryptodome.Cipher import AES
    from Cryptodome.Hash import SHA
    from Cryptodome.PublicKey import RSA
    from Cryptodome.Util.number import bytes_to_long, long_to_bytes
    from Cryptodome.Signature import pkcs1_15
except ImportError:
    from Crypto.Cipher import AES
    from Crypto.Hash import SHA
    from Crypto.PublicKey import RSA
    from Crypto.Util.number import bytes_to_long, long_to_bytes
    from Crypto.Signature import pkcs1_15

from . import ec

WII_RSA4096 = 0
WII_RSA2048 = 1
WII_ECDSA = 2

sigtypes = [ "RSA-4096", "RSA-2048", "EC-DSA" ]

def load_rsa_key(issuer):
    print("Loading private key for %s" % issuer)
    path = os.path.join(os.environ["HOME"], ".wii", "dpki", issuer + ".pem")
    return RSA.importKey(open(path, "r").read())

signkeyfuncs = [ load_rsa_key, load_rsa_key, None ]

NULL_IV = b"\x00"*16

keylist = [
    "common-key",
    "sd-key",
    "sd-iv",
    "md5-blanker",
    "root-key",
    "korean-key",
]

keys = {}

known_titles = {
    0x0000000100000001 : [ "boot2", "boot2" ],
    0x0000000100000002 : [ "System Menu", "SystemMenu" ],
    0x0000000100000100 : [ "BC", "BC" ],
    0x0000000100000101 : [ "MIOS", "MIOS" ],
    0x0001000148415858 : [ "The Homebrew Channel (HAXX)", "HBC1" ],
    0x000100014a4f4449 : [ "The Homebrew Channel (JODI)", "HBC2" ],
    0x00010001af1bf516 : [ "The Homebrew Channel (new)", "HBC3" ],
    0x000100014c554c5a : [ "The Homebrew Channel (LULZ)", "HBC4" ],
    0x0001000148424330 : [ "The Homebrew Channel OSS release (OHBC)", "OpenHBC" ],
    0x0001000844564458 : [ "DVDX (DVDX)", "DVDX1" ],
    0x0001000844495343 : [ "DVDX (DISC)", "DVDX2" ]
}

known_titles_noregion = {
    0x0001000148414400 : [ "Internet Channel", "Internet" ],
    0x0001000148414a00 : [ "Everybody Votes Channel", "Votes" ],
    0x0001000148415000 : [ "Check Mii Out Channel", "CheckMii" ],
    0x0001000148415400 : [ "Nintendo Channel", "Nintendo" ],
    0x0001000148415600 : [ "Today and Tomorrow Channel", "Today" ],
    0x0001000148415700 : [ "Metroid Prime 3 Preview", "Metroid" ],
    0x0001000148434400 : [ "Digicam Print Channel", "Digicam" ],
    0x0001000148434600 : [ "Wii Speak Channel", "Speak", ],

    0x0001000248414100 : [ "Photo Channel", "Photo" ],
    0x0001000248414200 : [ "Wii Shop Channel", "Shop" ],
    0x0001000248414300 : [ "Mii Channel", "Mii" ],
    0x0001000248414600 : [ "Weather Channel", "Weather" ],
    0x0001000248414700 : [ "News Channel", "News" ],
    0x0001000248415900 : [ "Photo Channel (1.1)", "Photo2" ],

    0x0001000848414b00 : [ "EULA hidden channel", "EULA" ],
    0x0001000848414c00 : [ "Region Selection hidden channel", "Region" ],
    0x0001000848434300 : [ "Set Personal Data hidden channel", "Personal" ]
}

def hexdump(s,sep=" "):
    return sep.join(["%02x"%x for x in s])

def strcmp(s1,s2):
    clen = min(len(s1),len(s2))

    for i in range(clen):
        if s1[i] == 0 and s2[i] == 0:
            return True
        if s1[i] != s2[i]:
            return False
    return False

def ascii(s):
    s2 = ""
    for c in s:
        if c<0x20 or c>0x7e:
            s2 += "."
        else:
            s2 += chr(c)
    return s2

def pad(s,c,l):
    if len(s)<l:
        s += c * (l-len(s))
    return s

def chexdump(s):
    for i in range(0,len(s),16):
        print("%08x  %s  %s  |%s|"%(i,pad(hexdump(s[i:i+8],' ')," ",23),pad(hexdump(s[i+8:i+16],' ')," ",23),pad(ascii(s[i:i+16])," ",16)))

def getcstring(s):
    return s.split(b"\x00")[0].decode("ascii")

def align(n,a):
    if a == 0:
        return n
    if (n % a) != 0:
        n += a - (n % a)
    return n

def rangel(n,s):
    return list(range(n,n+s))

def xrangel(n,s):
    return range(n,n+s)

def falign(f,a):
    f.seek(align(f.tell(),a))

def toblocks(start, size, blocksize):
    if start < 0:
        raise ValueError("start must be >= 0")
    if size < 0:
        raise ValueError("size must be >= 0")
    if blocksize <= 0:
        raise ValueError("blocksize must be > 0")
    header = 0
    hdroffset = start % blocksize
    startblock = start / blocksize
    if hdroffset != 0:
        startblock+=1
        header = blocksize - hdroffset
        if size < header:
            return (startblock, 0, hdroffset, size, 0)
        start += header
        size -= header
    nblocks = size / blocksize
    footer = size % blocksize
    return (startblock, nblocks, hdroffset, header, footer)

def get_readable_title(titleid, shortname = False):
    if titleid in known_titles:
        if shortname:
            return known_titles[titleid][1]
        return known_titles[titleid][0]

    if (titleid & ~0xFF) in known_titles_noregion:
        if shortname:
            return known_titles_noregion[titleid & ~0xFF][1]
        return known_titles_noregion[titleid & ~0xFF][0]

    type = titleid >> 32
    id = titleid & 0xFFFFFFFF
    if type == 0x00000001: # IOS
        return "IOS%d" % id
    
    s = "%c%c%c%c" % (id >> 24, (id >> 16) & 0xFF,
                    (id >> 8) & 0xFF, id & 0xFF)

    if shortname:
        return s

    if type == 0x00010002: # channel
        return "Channel '%s'" % s

    if type == 0x00010008: # channel
        return "System Channel '%s'" % s

    return "Unknown title type"

def loadkeys(path = None):
    keys.clear()
    if path is None:
        path = os.path.join(os.environ["HOME"], ".wii")

    for key in keylist:
        try:
            keys[key] = open(path + os.sep + key, "rb").read()
        except:
            print("Warning: failed to load key %s"%key)

def loadkeys_dpki(path = None):
    if path is None:
        path = os.path.join(os.environ["HOME"], ".wii", "dpki")
    loadkeys(path)

def parse_certs(blob):
    certs = {}
    certlist = []
    while blob != b"":
        cert = WiiCert(blob)
        certs[cert.name] = cert
        certlist.append(cert)
        blob = blob[align(len(cert.data),0x40):]

    return (certs, certlist)


class HashError(RuntimeError):
    def __init__(self, msg, expected, got):
        RuntimeError.__init__(self, msg)
        self.msg = msg
        self.expected = expected
        self.got = got
    def __str__(self):
        return "%s: expected %s, got %s"%(self.msg, hexdump(self.expected,''), hexdump(self.got,''))

class WiiPKAlgo:
    def __init__(self,key):
        self.key = key

    def get_digest(self, signature):
        raise NotImplementedError("WiiPKAlgo is abstract")

    def check_digest(self, signature, expected):
        return self.get_digest(signature) == expected

    def sign(self, data, key):
        raise NotImplementedError("WiiPKAlgo is abstract")

    def bruteforce(self, signature, bytes, match):
        codes = {1: "B", 2:"H", 4:"I", 8:"Q"}

        if bytes not in codes:
            raise ValueError("Bytes must be 1,2,4, or 8")

        code = ">"+codes[bytes]
        matchlen = len(match)

        for pad in range(0, 256**bytes):
            pad += 0x4612512415125316
            pad %= 256**bytes
            padsig = signature + pack(code, pad)
            dig = self.get_digest(padsig)
            if dig[:matchlen] == match:
                return padsig
        raise RuntimeError("Bruteforce failed")

class WiiRSA(WiiPKAlgo):
    def __init__(self,key):
        WiiPKAlgo.__init__(self, key)
        self.n = bytes_to_long(self.key[:-4])
        self.e = bytes_to_long(self.key[-4:])
        self.rsa = RSA.construct((self.n, self.e))
        self.can_get_digest = True

    def get_digest(self, signature):
        lsig = bytes_to_long(signature)
        if lsig >= self.n:
            print("Warning: signature larger than modulus, using sig%modulus as signature")
        ldec = pow(lsig, self.e, self.n)
        dec = long_to_bytes(ldec)
        pad = len(signature) - len(dec)
        dec = b"\x00"*pad+dec
        return dec[-20:]

    def sign(self, data, key):
        # Wrapper class to make PyCryptodome happy
        class H(object):
            oid = "1.3.14.3.2.26"
        h = H()
        h.digest = lambda: data
        return pkcs1_15.new(key).sign(h)

class WiiECDSA(WiiPKAlgo):
    def __init__(self,key):
        WiiPKAlgo.__init__(self, key)
        self.can_get_digest = False

    def get_digest(self, signature):
        raise NotImplementedError("EC-DSA cannot read message digest")

    def check_digest(self, signature, expected):
        return ec.check_ecdsa(self.key,signature[0:30],signature[30:60],expected)

    def sign(self, data, key):
        raise NotImplementedError("EC-DSA signing not implemented")

class WiiDisc:
    def __init__(self, iso, readonly=False):
        self.fname = iso
        self.readonly = readonly
        self.f = open(self.fname, "rb")

        self.partitions = None

        self.read_header()

    def read_header(self):
        self.f.seek(0)
        self.gamecode = self.f.read(4)
        self.makercode = self.f.read(2)
        self.f.seek(0x18)
        self.magic = unpack(">I",self.f.read(4))[0]
        if self.magic != 0x5d1c9ea3:
            raise RuntimeError("Not a Wii ISO!")
        self.f.seek(0x20)
        self.gamename = getcstring(self.f.read(0x60))

    def read_partitions(self):
        if self.partitions is not None:
            return self.partitions
        self.f.seek(0x40000)
        pt_num, pt_start, p2_num, p2_start = unpack(">IIII",self.f.read(16))
        self.f.seek(pt_start<<2)
        partitions = []
        for i in range(pt_num):
            p_off, p_type = unpack(">II",self.f.read(8))
            partitions.append((p_off<<2, p_type))
        if p2_num > 0:
            self.f.seek(p2_start<<2)
            for i in range(p2_num):
                p_off, p_type = unpack(">II",self.f.read(8))
                partitions.append((p_off<<2, p_type))

        self.partitions = partitions
        return self.partitions

    def showinfo(self):
        print("Game %s, maker %s, magic %08x: %s"%(self.gamecode, self.makercode, self.magic, self.gamename))
        self.read_partitions()
        print("%d partitions in ISO:"%len(self.partitions))
        for p_num,p_dat in enumerate(self.partitions):
            print(" [%2d] 0x%010x (%08x)"%(p_num,p_dat[0],p_dat[1]))

class WiiSigned:
    sigsizes = [512, 256, 60]
    sigblocks = [0x240, 0x140, 0x80]

    def __init__(self, data):
        self.type = "UNKNOWN"
        if len(data) < 4:
            raise ValueError("Data too short")
        self.sigtype = unpack(">I",data[:4])[0] - 0x10000

        if 0 <= self.sigtype < len(sigtypes):
            self.signature = data[4:4+self.sigsizes[self.sigtype]]
            self.body_offset = self.sigblocks[self.sigtype]
        else:
            raise ValueError("Unknown signature type %08x"%self.sigtype)
        self.data = data
        self.body = self.data[self.body_offset:]
        self.data = data[:self._getbodysize() + self.body_offset]
        self.body = self.body[:self._getbodysize()]
        self.issuer = getcstring(self.body[:0x40]).split("-")
        self.fillshort = None

    def update(self):
        self.data = pack(">I",self.sigtype+0x10000) + self.signature
        self.data += b"\x00" * (self.body_offset - len(self.data))
        self.data += self.body

    def parse(self):
        pass

    def _getbodysize(self):
        return len(self.body)

    def get_hash(self):
        return SHA.new(self.body).digest()

    def get_signature_hash(self,certs,):
        cert = certs[self.issuer[-1]]
        if cert.key_type != self.sigtype:
            raise ValueError("Signature type %s does not match certificate type %s!"%(sigtypes[self.sigtype],sigtypes[cert.key_type]))
        return cert.pkalgo.get_digest(self.sigtype, self.signature)

    def brute_sha(self, match = b"\x00", fillshort = None):
        l = len(match)

        if fillshort is None:
            fillshort = self.fillshort
        if fillshort is None:
            raise ValueError("No fill short specified!")

        for cnt in range(65536):
            self.body = self.body[:fillshort] + pack(">H",cnt) + self.body[fillshort+2:]
            if self.get_hash()[:l] == match:
                WiiSigned.update(self)
                self.parse()
                return
        raise RuntimeError("SHA Bruteforce failed")

    def update_issuer(self, issuer):
        if len(issuer) > 39:
            raise ValueError("issuer name too long!")
        self.issuer = issuer.split("-")
        self.body = issuer + b"\x00" * (0x40 - len(issuer)) + self.body[0x40:]
        self.update()

    def update_signature(self, sig):
        self.signature = sig
        self.data = pack(">I",self.sigtype+0x10000) + self.signature + self.data[len(sig) + 4:]

    def null_signature(self):
        self.signature = b"\x00"*len(self.signature)
        self.data = pack(">I",self.sigtype+0x10000) + self.signature + self.data[len(self.signature) + 4:]

    def sign(self,certs):
        keyfunc = signkeyfuncs[self.sigtype]
        if keyfunc is None:
            raise ValueError("signing with %s not available" % sigtypes[self.sigtype])
        key = keyfunc(self.issuer[-1])
        if key is None:
            raise ValueError("no key for %s available" % self.issuer[-1])
        cert = certs[self.issuer[-1]]
        sig = cert.pkalgo.sign(self.get_hash(), key)
        self.update_signature(sig)
        return self.signcheck(certs)

    def signcheck(self,certs):
        myhash = self.get_hash()
        cert = certs[self.issuer[-1]]
        return cert.pkalgo.check_digest(self.signature,myhash)

    def findcert(self, certs):
        if self.issuer[-1] == "Root":
            cert = WiiRootCert(keys['root-key'])
        else:
            cert = certs[self.issuer[-1]]
        return cert

    def showsig(self,certs,it=""):
        myhash = self.get_hash()
        try:
            if self.issuer[-1] == "Root":
                cert = WiiRootCert(keys['root-key'])
            else:
                cert = certs[self.issuer[-1]]
            if cert.pkalgo.can_get_digest:
                signhash = cert.pkalgo.get_digest(self.signature)
                if myhash == signhash:
                    print(it+"%s signed by %s using %s: %s [OK]"%(self.type, "-".join(self.issuer), sigtypes[self.sigtype], hexdump(myhash)))
                elif strcmp(myhash, signhash):
                    print(it+"%s signed by %s using %s: %s [BUG]"%(self.type, "-".join(self.issuer), sigtypes[self.sigtype], hexdump(myhash)))
                    print(it+"   Signature hash: %s"%hexdump(signhash))
                else:
                    print(it+"%s signed by %s using %s: %s [FAIL]"%(self.type, "-".join(self.issuer), sigtypes[self.sigtype], hexdump(myhash)))
                    print(it+"   Signature hash: %s"%hexdump(signhash))
            else:
                sigok = cert.pkalgo.check_digest(self.signature,myhash)
                if sigok:
                    print(it+"%s signed by %s using %s: %s [OK]"%(self.type, "-".join(self.issuer), sigtypes[self.sigtype], hexdump(myhash)))
                else:
                    print(it+"%s signed by %s using %s: %s [FAIL]"%(self.type, "-".join(self.issuer), sigtypes[self.sigtype], hexdump(myhash)))

        except KeyError:
            print(it+"%s signed by %s using %s: %s [ISSUER NOT FOUND]"%(self.type, "-".join(self.issuer), sigtypes[self.sigtype], hexdump(myhash)))

class WiiTik(WiiSigned):
    def __init__(self, data):
        WiiSigned.__init__(self, data)
        self.type = "ETicket"
        self.fillshort = 0xb2
        self.parse()

    def parse(self):
        self.title_key_enc = self.body[0x7f:0x8f]
        self.title_id = self.body[0x9c:0xa4]
        self.title_key_iv = self.title_id + b"\x00"*8
        self.common_key_index = ord(self.body[0xb1:0xb2])

        try:
            if self.common_key_index == 0:
                key = keys["common-key"]
            elif self.common_key_index == 1:
                key = keys["korean-key"]
            else:
                print("WARNING: OLD FAKESIGNED TICKET WITH BAD KEY OFFSET, ASSUMING NORMAL COMMON KEY")
                key = keys["common-key"]
            aes = AES.new(key, AES.MODE_CBC, self.title_key_iv)
            self.title_key = aes.decrypt(self.title_key_enc)
        except:
            self.title_key = None

    def update(self):
        self.body = self.body[:0x9c] + self.title_id + self.body[0xa4:]
        self.parse()
        WiiSigned.update(self)

    def _getbodysize(self):
        return 0x164

    def showinfo(self, it=""):
        print(it+"ETicket: ")
        print(it+" Title ID: "+repr(self.title_id))
        print(it+" Title key IV: "+hexdump(self.title_key_iv))
        print(it+" Title key (encrypted): "+hexdump(self.title_key_enc))
        print(it+" Common key index: %d" % self.common_key_index)
        if self.title_key is not None:
            print(it+" Title key (decrypted): "+hexdump(self.title_key))

class WiiPartitionOffsets:
    def __init__(self, data):
        self.data = data
        self.parse()

    def parse(self):
        self.tmd_size = unpack(">I",self.data[0x0:0x4])[0]
        self.tmd_offset = unpack(">I",self.data[0x4:0x8])[0]<<2
        self.cert_size = unpack(">I",self.data[0x8:0xc])[0]
        self.cert_offset = unpack(">I",self.data[0xc:0x10])[0]<<2
        self.h3_offset = unpack(">I",self.data[0x10:0x14])[0]<<2
        self.data_offset = unpack(">I",self.data[0x14:0x18])[0]<<2
        self.data_size = unpack(">I",self.data[0x18:0x1c])[0]<<2

    def showinfo(self, it=""):
        print(it+"TMD @ 0x%x [0x%x], Certs @ 0x%x [0x%x], H3 @ 0x%x, Data @ 0x%x [0x%x]"%(
            self.tmd_offset, self.tmd_size, self.cert_offset, self.cert_size,
            self.h3_offset, self.data_offset, self.data_size))

    def update(self):
        self.data = pack(">II",self.tmd_size, self.tmd_offset>>2)
        self.data += pack(">II",self.cert_size, self.cert_offset>>2)
        self.data += pack(">I",self.h3_offset>>2)
        self.data += pack(">II",self.data_offset>>2,self.data_size>>2)

class WiiTmdContentRecord:
    def __init__(self, data):
        self.cid, self.index, self.ftype, self.size, self.sha = unpack(">IHHQ20s",data)
        self.data = data
        self.shared = (self.ftype & 0x8000) != 0

    def update(self):
        self.data = pack(">IHHQ20s", self.cid, self.index, self.ftype, self.size, self.sha)

class WiiTmd(WiiSigned):
    def __init__(self, data):
        WiiSigned.__init__(self, data)
        self.type = "TMD"
        self.fillshort = 0x70
        self.parse()

    def parse(self):
        self.version, self.ca_crl_version, self.signer_crl_version, \
        self.fill2, self.sys_version, self.title_id, self.title_type, \
        self.group_id, self.reserved, self.access_rights, \
        self.title_version, self.num_contents, self.boot_index = \
        unpack(">BBBBQ8sI2s62sIHHH",self.body[0x40:0xa2])

    def _getbodysize(self):
        self.parse()
        return 0xa4+self.num_contents*36

    def update(self):
        hdr = pack(">BBBBQ8sI2s62sIHHH", \
            self.version, self.ca_crl_version, self.signer_crl_version, \
            self.fill2, self.sys_version, self.title_id, self.title_type, \
            self.group_id, self.reserved, self.access_rights, \
            self.title_version, self.num_contents, self.boot_index \
        )
        self.body = self.body[:0x40] + hdr + self.body[0xa2:]
        WiiSigned.update(self)

    def get_content_records(self):
        cts = []
        for i in range(self.num_contents):
            ctr = WiiTmdContentRecord(self.body[0xa4+36*i:0xa4+36*(i+1)])
            cts.append(ctr)
        return cts

    def find_cr_by_index(self, index):
        for i,ct in enumerate(self.get_content_records()):
            if ct.index == index:
                return i
        raise ValueError("Index %d not found"%index)

    def find_cr_by_cid(self, cid):
        for i,ct in enumerate(self.get_content_records()):
            if ct.cid == cid:
                return i
        raise ValueError("CID %08X not found"%cid)

    def update_content_record(self, num, cr):
        cr.update()
        self.body = self.body[:0xa4+36*num] + cr.data +  self.body[0xa4+36*(num+1):]
        self.update()

    def showinfo(self,it=""):
        print(it+"TMD: ")
        print(it+" Versions: %d, CA CRL %d, Signer CRL %d, System %d-%d"%(
            self.version,self.ca_crl_version,self.signer_crl_version,self.sys_version>>32,self.sys_version&0xffffffff))
        print(it+" Title ID: %s-%s (%s-%s)"%(hexdump(self.title_id[:4],''),hexdump(self.title_id[4:],''),repr(self.title_id[:4]),repr(self.title_id[4:])))
        print(it+" Title Type: %d"%self.title_type)
        print(it+" Group ID: %s"%repr(self.group_id))
        print(it+" Access Rights: 0x%08x"%self.access_rights)
        print(it+" Title Version: 0x%x"%self.title_version)
        print(it+" Boot Index: %d"%self.boot_index)
        print(it+" Contents:")
        print(it+"  ID       Index Type    Size         Hash")
        for ct in self.get_content_records():

            print(it+"  %08X %-5d 0x%-5x %-12s %s"%(ct.cid, ct.index, ct.ftype, "0x%x"%ct.size,hexdump(ct.sha)))

class WiiCert(WiiSigned):
    key_sizes = [516, 260, 60]
    pk_types = [WiiRSA, WiiRSA, WiiECDSA]

    def __init__(self, data):
        WiiSigned.__init__(self, data)
        self.type = "Certificate"
        self.parse()

    def _getbodysize(self):
        keytype = unpack(">I",self.body[0x40:0x44])[0]
        keysize = self.key_sizes[keytype]
        return align(keysize + 0x8c,64)

    def parse(self):
        self.key_type = unpack(">I",self.body[0x40:0x44])[0]
        self.key_size = self.key_sizes[self.key_type]
        self.name = getcstring(self.body[0x44:0x84])
        self.unk2 = self.body[0x84:0x88]
        self.key = self.body[0x88:0x88 + self.key_size]
        self.pkalgo = self.pk_types[self.key_type](self.key)

    def showinfo(self,it=""):
        print(it+"%s (%s)"%(self.name,sigtypes[self.key_type]))

class WiiRootCert:
    def __init__(self, data):
        self.type = "Certificate"
        self.key = data
        self.pkalgo = WiiRSA(self.key)
        self.name = "Root"
        self.key_size = 516
        self.key_type = 0

    def showinfo(self,it=""):
        print(it+"%s (%s)"%(self.name,sigtypes[self.key_type]))

class WiiPartition:
    BLOCKS_PER_SUBGROUP = 8
    SUBGROUPS_PER_GROUP = 8
    BLOCKS_PER_GROUP = BLOCKS_PER_SUBGROUP * SUBGROUPS_PER_GROUP
    CIPHER_BLOCK_SIZE = 0x8000
    CIPHER_SUBGROUP_SIZE = BLOCKS_PER_SUBGROUP*CIPHER_BLOCK_SIZE
    CIPHER_GROUP_SIZE = SUBGROUPS_PER_GROUP*CIPHER_SUBGROUP_SIZE
    SHA_SIZE = 0x400
    DATA_SIZE = CIPHER_BLOCK_SIZE - SHA_SIZE
    DATA_CHUNK_SIZE = 0x400
    DATA_CHUNKS_PER_BLOCK = DATA_SIZE / DATA_CHUNK_SIZE
    PLAIN_BLOCK_SIZE = DATA_SIZE
    PLAIN_SUBGROUP_SIZE = BLOCKS_PER_SUBGROUP*PLAIN_BLOCK_SIZE
    PLAIN_GROUP_SIZE = SUBGROUPS_PER_GROUP*PLAIN_SUBGROUP_SIZE
    NUM_H3,TAIL_H3 = divmod(0x18000,20) #the division has a remainder, so there is some dead space

    def __init__(self, disc, number, checkhash = True):
        if disc.readonly:
            self.f = open(disc.fname,"rb")
        else:
            self.f = open(disc.fname,"r+b")
        self.offset = disc.read_partitions()[number][0]

        self.read_headers()

        self.checkhash = checkhash

        if checkhash and not self.checkh4hash():
            raise HashError("H4 hash check failed",self.tmd.get_content_records()[0].sha, self.geth4hash())

    def _seek(self,n):
        self.f.seek(self.offset + n)

    def read_headers(self):

        self._seek(0)
        self.tik = WiiTik(self.f.read(0x2a4))
        self.offsets = WiiPartitionOffsets(self.f.read(0x1c))
        self._seek(self.offsets.tmd_offset)
        self.tmd = WiiTmd(self.f.read(self.offsets.tmd_size))
        self.certs = {}
        self._seek(self.offsets.cert_offset)
        certdata = self.f.read(self.offsets.cert_size)
        self.certlist = []
        while certdata != "":
            cert = WiiCert(certdata)
            self.certs[cert.name] = cert
            self.certlist.append(cert)
            certdata = certdata[align(len(cert.data),0x40):]
        self._seek(self.offsets.h3_offset)
        h3data = self.f.read(0x18000)
        self.h3 = []
        for i in range(self.NUM_H3):
            self.h3.append(h3data[20*i:20*(i+1)])
        self.data_offset = self.offsets.data_offset + self.offset
        self.data_blocks = self.offsets.data_size / self.CIPHER_BLOCK_SIZE
        if self.offsets.data_size % self.CIPHER_BLOCK_SIZE != 0:
            raise ValueError("Data size (0x%x) not a multiple of block size (0x%x)"%(
                self.offsets.data_size, self.CIPHER_BLOCK_SIZE))
        self.data_bytes = self.data_blocks * self.PLAIN_BLOCK_SIZE
        self.data_subgroups = self.data_blocks / self.BLOCKS_PER_SUBGROUP
        self.data_groups = self.data_subgroups / self.SUBGROUPS_PER_GROUP
        self.extra_subgroup_blocks = self.data_blocks % self.BLOCKS_PER_SUBGROUP
        self.extra_group_blocks = self.data_blocks % self.BLOCKS_PER_GROUP
        self.partition_end = self.data_offset + self.data_blocks * self.CIPHER_BLOCK_SIZE

    def updatetmd(self):
        self._seek(self.offsets.tmd_offset)
        self.f.write(self.tmd.data)

    def updatetik(self):
        self._seek(0)
        self.f.write(self.tik.data)

    def updateoffsets(self):
        self.offsets.update()
        self._seek(0x2a4)
        self.f.write(self.offsets.data)

    def showinfo(self,it=""):
        print(it+"Wii Partition at 0x%010x:"%(self.offset))
        self.offsets.showinfo(" ")
        self.tik.showinfo(it+" ")
        self.tik.showsig(self.certs,it+" ")
        self.tmd.showinfo(it+" ")
        self.tmd.showsig(self.certs,it+" ")
        if self.checkh4hash():
            print(it+" H4 hash check passed")
        else:
            print(it+" H4 check failed: SHA1(H3) = "+hexdump(self.geth4hash()))
        print(it+" Data:")
        print(it+"  Blocks:    %d"%self.data_blocks)
        print(it+"  Subgroups: %d (plus %d blocks)"%(self.data_subgroups,self.extra_subgroup_blocks))
        print(it+"  Groups:    %d (plus %d blocks)"%(self.data_groups,self.extra_group_blocks))
        self.showcerts(it+" ")

    def showcerts(self,it=""):
        print(it+"Certificates: ")
        for cert in self.certlist:
            cert.showinfo(it+" - ")
            cert.showsig(self.certs,it+"    ")

    def geth4hash(self):
        return SHA.new(b''.join(self.h3) + b"\x00"*self.TAIL_H3).digest()

    def checkh4hash(self):
        return self.geth4hash() == self.tmd.get_content_records()[0].sha

    def updateh3(self):
        self._seek(self.offsets.h3_offset)
        self.f.write(b''.join(self.h3))

    def updateh4(self):
        cr = self.tmd.get_content_records()[0]
        cr.sha = self.geth4hash()
        self.tmd.update_content_record(0,cr)
        self.updatetmd()

    def readc(self, start, length):
        '''Read raw data from the partition'''
        if (start + length) > self.offsets.data_size:
            raise ValueError("Attempted to read past the end of the partition data")
        if start < 0:
            raise ValueError("start must be >= 0")
        self.f.seek(self.data_offset + start)
        return self.f.read(length)

    def readblockc(self, block):
        '''Read a raw block (0x8000 bytes) from the partition'''
        if block >= self.data_blocks:
            raise ValueError("Attempted to read block past the end of the partition data")
        if block < 0:
            raise ValueError("block must be >= 0")
        self.f.seek(self.data_offset + block * self.CIPHER_BLOCK_SIZE)
        return self.f.read(self.CIPHER_BLOCK_SIZE)

    def readsubgroupc(self, subgroup):
        '''Read a raw subgroup (0x40000 bytes) from the partition'''
        if subgroup >= self.data_subgroups:
            raise ValueError("Attempted to read subgroup past the end of the partition data")
        if subgroup < 0:
            raise ValueError("block must be >= 0")
        self.f.seek(self.data_offset + subgroup * self.CIPHER_SUBGROUP_SIZE)
        return self.f.read(self.CIPHER_SUBGROUP_SIZE)

    def readgroupc(self, group):
        '''Read a raw group (0x200000 bytes) from the partition'''
        if group >= self.data_groups:
            raise ValueError("Attempted to read group past the end of the partition data")
        if group < 0:
            raise ValueError("block must be >= 0")
        self.f.seek(self.data_offset + group * self.CIPHER_GROUP_SIZE)
        return self.f.read(self.CIPHER_GROUP_SIZE)

    def read(self, start, length):
        '''Read data from an arbitrary offset into the partition'''
        if (start+length) > (self.data_blocks*self.PLAIN_BLOCK_SIZE):
            raise ValueError("Attempted to read past the end of the partition data (0x%x + 0x%x)"%(start,length))
        if start < 0:
            raise ValueError("start must be >= 0")

        bstart, bnum, hdroff, header, footer = toblocks(start, length, self.PLAIN_BLOCK_SIZE)

        if header:
            data = self.readblock(bstart - 1)[hdroff:hdroff+header]
        else:
            data = ""
        for block in xrangel(bstart, bnum):
            data += self.readblock(block)
        if footer:
            data += self.readblock(bstart + bnum)[:footer]
        return data

    def _check_hash_chain(self, blocknum, data, sha):
        h0 = sha[:0x26c]
        h1 = sha[0x280:0x320]
        h2 = sha[0x340:0x3e0]
        for i in range(self.DATA_CHUNKS_PER_BLOCK):
            h0i = SHA.new(data[i*self.DATA_CHUNK_SIZE:(i+1)*self.DATA_CHUNK_SIZE]).digest()
            if h0i != h0[i*20:(i+1)*20]:
                raise HashError("Failed to verify data chunk %d against H0"%i,h0[i*20:(i+1)*20],h0i)
        h1i = SHA.new(h0).digest()
        rem,block = divmod(blocknum, self.BLOCKS_PER_SUBGROUP)
        if h1i != h1[block*20:(block+1)*20]:
            raise HashError("Failed to verify H0 %d against H1"%block,h1[block*20:(block+1)*20],h1i)
        h2i = SHA.new(h1).digest()
        group,subgroup = divmod(rem, self.SUBGROUPS_PER_GROUP)
        if h2i != h2[subgroup*20:(subgroup+1)*20]:
            raise HashError("Failed to verify H1 %d against H2"%subgroup,h2[subgroup*20:(subgroup+1)*20],h2i)
        h3i = SHA.new(h2).digest()
        if h3i != self.h3[group]:
            raise HashError("Failed to verify H2 %d against H3"%group,self.h3[group],h3i)

    def _decrypt_block(self, block, blocknum):
        if self.tik.title_key is None:
            raise RuntimeError("Title key is missing (you probably don't have the common key)")
        data_iv = block[0x3d0:0x3e0]
        aes = AES.new(self.tik.title_key, AES.MODE_CBC, data_iv)
        data = aes.decrypt(block[self.SHA_SIZE:])
        if self.checkhash:
            aes = AES.new(self.tik.title_key, AES.MODE_CBC, NULL_IV)
            shablock = aes.decrypt(block[:self.SHA_SIZE])
            self._check_hash_chain(blocknum, data, shablock)
        return data

    def readblock(self, blocknum):
        '''Read one block of data (0x7C00 bytes) from the partition'''
        if blocknum >= self.data_blocks:
            raise ValueError("Attempted to read block past the end of the partition data")
        if blocknum < 0:
            raise ValueError("blocknum must be >= 0")

        rawblock = self.readblockc(blocknum)
        return self._decrypt_block(rawblock, blocknum)

    def readsubgroup(self, subgroupnum):
        '''Read one subgroup of data (0x3E000 bytes) from the partition'''
        if subgroupnum > self.data_subgroups	:
            raise ValueError("Attempted to read subgroup past the end of the partition data")
        if subgroupnum < 0:
            raise ValueError("subgroupnum must be >= 0")

        nblocks = self.BLOCKS_PER_SUBGROUP
        blockoff = subgroupnum * self.BLOCKS_PER_SUBGROUP
        if subgroupnum == self.data_subgroups:
            if self.extra_subgroup_blocks != 0:
                nblocks = self.extra_subgroup_blocks
                blocknum = subgroupnum * self.BLOCKS_PER_SUBGROUP
            else:
                raise ValueError("Attempted to read subgroup past the end of the partition data")

        data = ""
        for i in range(nblocks):
            data += self.readblock(blockoff+i)
        return data

    def readgroup(self, groupnum):
        if groupnum > self.data_groups:
            raise ValueError("Attempted to read group past the end of the partition data")
        if groupnum < 0:
            raise ValueError("groupnum must be >= 0")

        nblocks = self.BLOCKS_PER_GROUP
        blockoff = groupnum * self.BLOCKS_PER_GROUP
        if groupnum == self.data_groups:
            if self.extra_group_blocks != 0:
                nblocks = self.extra_group_blocks
                blockoff = groupnum * self.BLOCKS_PER_GROUP
            else:
                raise ValueError("Attempted to read group past the end of the partition data")

        data = b""
        for i in range(nblocks):
            data += self.readblock(blockoff+i)
        return data

    def writegroup(self, groupnum, data):
        '''Write a group of data (0x1F0000 bytes) to the partition'''

        if self.tik.title_key is None:
            raise RuntimeError("Title key is missing (you probably don't have the common key)")

        if groupnum >= self.data_groups:
            # tolerate writing last incomplete group if data is of the right size
            if groupnum == self.data_groups and self.extra_group_blocks > 0 and len(data) == (self.extra_group_blocks * self.PLAIN_BLOCK_SIZE):
                blocks = self.extra_group_blocks
                writesize = blocks * self.CIPHER_BLOCK_SIZE
                data += b"\x00" * (self.PLAIN_BLOCK_SIZE * self.BLOCKS_PER_GROUP - blocks)
            else:
                raise ValueError("Attempted to write group past the end of the partition data")
        else:
            blocks = self.BLOCKS_PER_GROUP
            writesize = self.CIPHER_GROUP_SIZE
            if len(data) != self.PLAIN_GROUP_SIZE:
                raise ValueError("Data size must be equal to group size")

        h0 = []
        h1 = []
        h2 = b""
        for subgroup in range(self.SUBGROUPS_PER_GROUP):
            bh1 = b""
            sh0 = []
            for block in range(self.BLOCKS_PER_SUBGROUP):
                bh0 = b""
                for chunk in range(self.DATA_CHUNKS_PER_BLOCK):
                    offset = subgroup * self.PLAIN_SUBGROUP_SIZE + block * self.PLAIN_BLOCK_SIZE + chunk * self.DATA_CHUNK_SIZE
                    bh0 += SHA.new(data[offset:offset+self.DATA_CHUNK_SIZE]).digest()
                sh0.append(bh0)
                bh1 += SHA.new(bh0).digest()
            h0.append(sh0)
            h1.append(bh1)
            h2 += SHA.new(bh1).digest()
        h3 = SHA.new(h2).digest()

        data_out = b""
        for subgroup in range(self.SUBGROUPS_PER_GROUP):
            for block in range(self.BLOCKS_PER_SUBGROUP):
                shablock = ""
                shablock += h0[subgroup][block]
                shablock += b"\x00"*20
                shablock += h1[subgroup]
                shablock += b"\x00"*32
                shablock += h2
                shablock += b"\x00"*32
                assert len(shablock) == self.SHA_SIZE, "sha block size messed up"
                aes = AES.new(self.tik.title_key, AES.MODE_CBC, NULL_IV)
                shablock = aes.encrypt(shablock)
                data_out += shablock
                aes = AES.new(self.tik.title_key, AES.MODE_CBC, shablock[0x3d0:0x3e0])
                offset = subgroup * self.PLAIN_SUBGROUP_SIZE + block * self.PLAIN_BLOCK_SIZE
                data_out += aes.encrypt(data[offset:offset+self.PLAIN_BLOCK_SIZE])

        assert len(data_out) == self.CIPHER_GROUP_SIZE, "group size messed up"

        self.f.seek(self.data_offset + groupnum * self.CIPHER_GROUP_SIZE)
        self.f.write(data_out[:writesize])

        self.h3[groupnum] = h3

    def write(self, start, data):
        if (start+len(data)) > self.data_blocks*self.PLAIN_BLOCK_SIZE:
            raise ValueError("Attempted to write past the end of the partition data")
        if start < 0:
            raise ValueError("start must be >= 0")

        gstart, gnum, hdroff, header, footer = toblocks(start, len(data), self.PLAIN_GROUP_SIZE)

        if header != 0:
            g = self.readgroup(gstart-1)
            g2 = g[:hdroff] + data[:header] + g[hdroff+header:]
            assert len(g) == len(g2), "group length mismatch in header"
            self.writegroup(gstart-1,g2)
        for group in xrangel(gstart,gnum):
            self.writegroup(group,data[header+self.PLAIN_GROUP_SIZE*group:header+self.PLAIN_GROUP_SIZE*(group+1)])
        if footer != 0:
            g = self.readgroup(gstart+gnum)
            g2 = data[header+self.PLAIN_GROUP_SIZE*gnum:] + g[footer:]
            assert len(g) == len(g2), "group length mismatch in footer"
            self.writegroup(gstart+gnum,g2)

    def update(self):
        self.updateh3()
        self.updateh4()
        self.updatetmd()
        self.updatetik()

class CacheEntry:
    def __init__(self, data, lastuse, dirty=False):
        self.data = data
        self.lastuse = lastuse
        self.dirty = dirty

    def __repr__(self):
        return "CacheEntry([0x%x bytes], %d, %s)"%(len(self.data),self.lastuse,repr(self.dirty))
    def __str__(self):
        return repr(self)

class WiiCachedPartition(WiiPartition):
    def __init__(self, disc, number, checkhash = True, cachesize=16, debug=False):
        WiiPartition.__init__(self, disc, number, checkhash)
        self.cachesectors = cachesize * 32
        self.cache = {}
        self.opnum = 0
        self.debug = debug

    def _dprint(self, s, *args):
        if self.debug:
            print(s%tuple(args))

    def _readblock(self, blocknum):
        self._dprint("_readblock(0x%x)",blocknum)
        d=WiiPartition.readblock(self, blocknum)
        self._dprint(" -> [0x%x]",len(d))
        return d

    def _flushgroup(self, groupno):
        self._dprint("_flushgroup(0x%x)",groupno)
        groupdata = ""
        startblock = groupno * self.BLOCKS_PER_GROUP
        if groupno == self.data_groups and self.extra_group_blocks > 0:
            self._dprint(" Last group")
            numblocks = self.extra_group_blocks
        else:
            numblocks = self.BLOCKS_PER_GROUP
        self._dprint(" numblocks = 0x%x",numblocks)
        for block in range(numblocks):
            self._dprint(" block 0x%x",block+startblock)
            if (block+startblock) in self.cache:
                self._dprint("  in cache: %s",repr(self.cache[block+startblock]))
                groupdata += self.cache[block+startblock].data
            else:
                self._dprint("  not in cache")
                groupdata += self._readblock(block+startblock)

        self._dprint(" writegroup(0x%x, [0x%x])",groupno,len(groupdata))
        WiiPartition.writegroup(self, groupno, groupdata)

        for block in range(self.BLOCKS_PER_GROUP):
            if (block+startblock) in self.cache:
                self._dprint(" cache undirty: 0x%x",block+startblock)
                self.cache[block+startblock].dirty = False

    def _freecache(self, num):
        self._dprint("_freecache(%d)",num)
        while (len(self.cache)+num) > self.cachesectors:
            self._dprint(" want %d, have %d, freeing",num,self.cachesectors-len(self.cache))
            # dirty group lastops
            groups = {}
            for key in self.cache:
                if self.cache[key].dirty:
                    grnum = key/self.BLOCKS_PER_GROUP
                    if grnum in groups:
                        groups[grnum] = max(groups[grnum], self.cache[key].lastuse)
                    else:
                        groups[grnum] = self.cache[key].lastuse

            lruop = self.opnum
            lru = -1
            lrugroup = None
            for key in self.cache:
                # if dirty, return the lastop of the entire group
                if self.cache[key].dirty:
                    groupno = key/self.BLOCKS_PER_GROUP
                    if groups[groupno] < lruop:
                        lru = key
                        lruop = groups[groupno]
                        lrugroup = groupno
                # otherwise return the lastop of the sector only
                else:
                    if self.cache[key].lastuse < lruop:
                        lrugroup = None
                        lru = key
                        lruop = self.cache[key].lastuse
            self._dprint(" LRU cache entry: [0x%x, %d]: %s",lru,lruop,self.cache[lru])
            if lrugroup is not None:
                self._dprint(" dirty, in group 0x%x",lrugroup)
            # if not dirty, delete it and continue
            if lrugroup is None:
                del self.cache[lru]
                continue
            # otherwise flush entire group and try again
            else:
                self._flushgroup(lrugroup)

    def readblock(self, blocknum):
        '''Read one block of data (0x7C00 bytes) from the partition'''
        self._dprint("readblock(0x%x)",blocknum)
        if blocknum >= self.data_blocks:
            raise ValueError("Attempted to read block past the end of the partition data")
        if blocknum < 0:
            raise ValueError("blocknum must be >= 0")

        self.opnum += 1
        if blocknum in self.cache:
            self._dprint(" cached: %s",self.cache[blocknum])
            self.cache[blocknum].lastuse = self.opnum
            return self.cache[blocknum].data
        else:
            self._dprint(" not cached")
            self._freecache(1)
            decblock = self._readblock(blocknum)
            self.cache[blocknum] = CacheEntry(decblock, self.opnum, False)
            return decblock

    def writeblock(self, blocknum, data):
        self._dprint("writeblock(0x%x,[0x%x])",blocknum, len(data))
        if self.tik.title_key is None:
            raise RuntimeError("Title key is missing (you probably don't have the common key)")

        if blocknum >= self.data_blocks:
            raise ValueError("Attempted to write block past the end of the partition data")
        if blocknum < 0:
            raise ValueError("blocknum must be >= 0")
        if len(data) != self.PLAIN_BLOCK_SIZE:
            raise ValueError("Data size must be equal to block size")

        self.opnum += 1

        if blocknum not in self.cache:
            self._dprint(" not in cache, freeing")
            self._freecache(1)
        self.cache[blocknum]=CacheEntry(data,self.opnum,True)

    # writegroup is not cached, since we already have the entire block
    # but we update the cache, for stuff already in cache
    def writegroup(self, groupnum, data):
        self._dprint("writegroup(0x%x,[0x%x])",groupnum, len(data))
        self.opnum += 1
        WiiPartition.writegroup(self, groupnum, data)

        if groupnum == self.data_groups:
            nblocks = self.extra_group_blocks
        else:
            nblocks = self.BLOCKS_PER_GROUP

        blockoff = groupnum * self.BLOCKS_PER_GROUP
        self._dprint(" nblocks=0x%x",nblocks)
        self._dprint(" blockoff=0x%x",blockoff)
        for blockno in range(nblocks):
            if (blockno+blockoff) in self.cache:
                self._dprint(" updating cache entry for block 0x%x",blockno+blockoff)
                self.cache[blockno+blockoff].data = data[blockno*self.PLAIN_BLOCK_SIZE:(1+blockno)*self.PLAIN_BLOCK_SIZE:]
                self.cache[blockno+blockoff].lastuse = self.opnum
                self.cache[blockno+blockoff].dirty = False
                self._dprint(" --> %s",self.cache[blockno+blockoff])

    def write(self, start, data):
        '''Write data to an arbitrary offset into the partition'''
        self._dprint("write(0x%x,[0x%x])",start, len(data))
        if (start+len(data)) > self.data_blocks*self.PLAIN_BLOCK_SIZE:
            raise ValueError("Attempted to write past the end of the partition data")
        if start < 0:
            raise ValueError("start must be >= 0")

        bstart, bnum, hdroff, header, footer = toblocks(start, len(data), self.PLAIN_BLOCK_SIZE)

        self._dprint(" bstart=0x%x bnum=0x%x hdroff=0x%x header=0x%x footer=0x%x",bstart,bnum,hdroff,header,footer)

        if header:
            hb = self.readblock(bstart - 1)
            hb = hb[:hdroff] + data[:header] + hb[hdroff+header:]
            self.writeblock(bstart - 1, hb)
        for block in range(bnum):
            self.writeblock(block+bstart, data[header+block*self.PLAIN_BLOCK_SIZE:header+(block+1)*self.PLAIN_BLOCK_SIZE])
        if footer:
            fb = self.readblock(bstart+bnum)
            df = data[header+bnum*self.PLAIN_BLOCK_SIZE:]
            self._dprint(" df: [0x%x]",len(df))
            fb = df + fb[footer:]
            self.writeblock(bstart + bnum, fb)

    def flush(self):
        self._dprint("flush()")
        # dirty groups
        groups = []
        for key in self.cache:
            if self.cache[key].dirty:
                grnum = key/self.BLOCKS_PER_GROUP
                if grnum not in groups:
                    groups.append(grnum)
        for group in groups:
            self._flushgroup(group)

class WiiPartitionFile:
    def __init__(self, part, offset=0, size=None):
        self.part = part
        self.pointer = 0
        self.offset = offset
        if size is None:
            self.size = part.data_bytes - offset
        else:
            self.size = size
    def read(self, n=None):
        leftn = self.size - self.pointer
        if n is None:
            n = leftn
        if n > leftn:
            n = leftn
        if n < 0:
            return ""
        d = self.part.read(self.pointer + self.offset, n)
        self.pointer += len(d)
        return d

    def write(self, d):
        self.part.write(self.pointer + self.offset,d)
        self.pointer += len(d)

    def tell(self):
        return self.pointer

    def seek(self, where, whence=0):
        if whence == 1:
            where += self.pointer
        elif whence == 2:
            where += self.size
        elif whence == 0:
            pass
        else:
            raise ValueError("Invalid value for whence: %d"%whence)
        self.pointer = where

    def close(self):
        self.part = None

class WiiApploader:
    def __init__(self, data):
        self.date = getcstring(data[:0x10])
        self.entry, self.textsize, self.trailersize = unpack(">III",data[0x10:0x1c])
        self.text = data[0x20:0x20+self.textsize]
        self.trailer = data[0x20+self.textsize:0x20+self.textsize+self.trailersize]
        self.extrafooter = data[0x20+self.textsize+self.trailersize:]

    def showinfo(self, it=""):
        print(it+"Apploader:")
        print(it+" Date: %s"%self.date)
        print(it+" Entrypoint: 0x%08x"%self.entry)
        print(it+" Text size: 0x%x"%self.textsize)
        print(it+" Trailer size: 0x%x"%self.trailersize)

class WiiPartitionData:
    def __init__(self, partition):
        self.part = partition
        self.hdr = self.part.read(0, 0x440)
        self.bi2 = self.part.read(0x440, 0x20)

        self.gamecode, self.maker, self.diskid, self.version, self.streaming, self.strbufsize = unpack(">4s2sBBBB",self.hdr[:10])
        self.magic = unpack(">I",self.hdr[0x18:0x1c])[0]
        if self.magic != 0x5d1c9ea3:
            raise RuntimeError("Not a Wii partition!")

        self.gamename = getcstring(self.part.read(0x20,0x60))

        self.dbgoff, self.dbgaddr, self.rsvd1, self.doloff, self.fstoff, self.fstsize, self.fstmax, self.usrpos, self.usrlen, self.unk1, self.rsvd2 = unpack(">II24sIIIIIIII", self.hdr[0x400:0x440])
        self.fstoff *= 4
        self.fstsize *= 4
        self.doloff *= 4
        self.dbgoff *= 4

        apphdr = self.part.read(0x2440,32)
        appsiz, apptr = unpack(">II",apphdr[0x14:0x1c])
        self.apploadersize = align(appsiz + apptr,32) + 32
        self.apploader = WiiApploader(self.part.read(0x2440, self.apploadersize))

        self.dolsize = 0x100
        dolhdr = self.part.read(self.doloff, 0x100)
        for i in range(0,0x48,4):
            self.dolsize += unpack(">I",dolhdr[0x90+i:0x94+i])[0]
        self.dol = self.part.read(self.doloff, self.dolsize)

        fstd = self.part.read(self.fstoff,self.fstsize)
        self.fst = WiiFST(fstd, wiigcm=True)

    def replacedol(self, dol):
        if len(dol) > self.dolsize:
            raise ValueError("dol must be smaller than preexisting dol")
        self.dol = dol
        self.part.write(self.doloff, dol)
    def showinfo(self,it=""):
        print(it+"Partition data:")
        print(it+" Game Name: %s"%self.gamename)
        print(it+" Offsets: DOL @ 0x%x [0x%x], Apploader @ 0x%x [0x%x], FST @ 0x%x [0x%x]"%(self.doloff, self.dolsize, 0x2440, self.apploadersize, self.fstoff, self.fstsize))
        self.apploader.showinfo(it+" ")
        print(it+"FST:")
        self.fst.show(it+" ")

class FakeFile:
    def __init__(self, data):
        self.data = data
        self.offset = 0
    def close(self):
        pass
    def flush(self):
        pass
    def write(self, data):
        raise NotImplementedError("Writes not implemented")
    def read(self, size=None):
        if size is None:
            return self.data[self.offset:]
        r =  self.data[self.offset:self.offset+size]
        self.offset=min(len(self.data),self.offset+size)
    def tell(self):
        return self.offset
    def seek(self, offset, whence=0):
        if whence == 0:
            pass 
        elif whence == 1:
            offset += self.offset
        elif whence == 2:
            offset += len(self.data)
        else:
            raise ValueError("Invalid whence")
        self.offset=max(0,min(len(self.data),offset))

class WiiWad:
    def __init__(self, file, readonly = True):
        self.ALIGNMENT = 0x40
        if readonly:
            self.f = open(file, "rb")
        else:
            self.f = open(file, "r+b")
        self.read_headers()

    def read_hdr(self):

        self.f.seek(0)

        hdr = unpack(">I2sHIIIIII",self.f.read(0x20))
        if hdr[0] != 0x20:
            raise ValueError("Invalid header length!")

        self.hdr_len = hdr[0]
        self.wadtype = hdr[1]
        self.cert_len = hdr[3]
        self.tik_len = hdr[5]
        self.tmd_len = hdr[6]
        self.data_len = hdr[7]
        self.footer_len = hdr[8]

        falign(self.f, self.ALIGNMENT)

    def read_boot2hdr(self):

        self.ALIGNMENT = 0
        self.f.seek(0)

        hdr = unpack(">IHHIIIIII",self.f.read(0x20))
        if hdr[0] != 0x20:
            raise ValueError("Invalid header length!")

        self.hdr_len = hdr[0]
        self.wadtype = hdr[1]
        self.cert_len = hdr[3]
        self.tik_len = hdr[4]
        self.tmd_len = hdr[5]

        self.f.seek(0,os.SEEK_END)
        size = self.f.tell()
        self.f.seek(0x20)

        self.data_len = size - 0x20 - self.tmd_len - self.tik_len - self.cert_len
        self.data_off = align(0x20 + self.tmd_len + self.tik_len + self.cert_len, 0x40)
        self.footer_len = 0

    def read_headers(self):
        self.data_off = None

        self.f.seek(4)
        wt = self.f.read(2)
        if wt == b"\x00\x00":
            self.read_boot2hdr()
        else:
            self.read_hdr()

        certdata = self.f.read(self.cert_len)
        self.certlist = []
        self.certs = {}
        while certdata != b"":
            cert = WiiCert(certdata)
            self.certs[cert.name] = cert
            self.certlist.append(cert)
            certdata = certdata[align(len(cert.data),0x40):]

        falign(self.f, self.ALIGNMENT)
        self.tik_offset = self.f.tell()
        self.tik = WiiTik(self.f.read(self.tik_len))
        falign(self.f, self.ALIGNMENT)
        self.tmd_offset = self.f.tell()
        self.tmd = WiiTmd(self.f.read(self.tmd_len))
        falign(self.f, 0x40)
        self.data_off = self.f.tell()
        self.f.seek(self.data_len,1)
        falign(self.f, self.ALIGNMENT)
        self.footer = self.f.read(self.footer_len)

    def updatetmd(self):
        self.f.seek(self.tmd_offset)
        self.f.write(self.tmd.data)
    def updatetik(self):
        self.f.seek(self.tik_offset)
        self.f.write(self.tik.data)

    def showinfo(self,it=""):
        print(it+"Wii Wad:")
        print(it+" Header 0x%x Type %s Certs 0x%x Tik 0x%x TMD 0x%x Data 0x%x @ 0x%x Footer 0x%x"%(self.hdr_len, repr(self.wadtype), self.cert_len, self.tik_len, self.tmd_len, self.data_len, self.data_off, self.footer_len))
        self.tik.showinfo(it+" ")
        self.tik.showsig(self.certs,it+" ")
        self.tmd.showinfo(it+" ")
        self.tmd.showsig(self.certs,it+" ")
        allok=True
        for ct in self.tmd.get_content_records():
            d = self.getcontent(ct.index)
            sha = SHA.new(d).digest()
            if sha != ct.sha:
                print(it+" SHA-1 for content %08x is invalid:"%ct.cid, hexdump(sha))
                print(it+"  Expected:",hexdump(ct.sha))
                allok=False
        if allok:
            print(it+" All content SHA-1 hashes are valid")
        self.showcerts(it+" ")

    def showcerts(self,it=""):
        print(it+"Certificates: ")
        for cert in self.certlist:
            cert.showinfo(it+" - ")
            cert.showsig(self.certs,it+"    ")

    def getcontent(self, index, encrypted=False):
        self.f.seek(self.data_off)
        for ct in self.tmd.get_content_records():
            if ct.index == index:
                data = self.f.read(align(ct.size,0x10))
                if encrypted:
                    return data

                iv = pack(">H",index)+b"\x00"*14
                aes = AES.new(self.tik.title_key, AES.MODE_CBC, iv)
                return aes.decrypt(data)[:ct.size]

            self.f.seek(ct.size,1)
            falign(self.f, 0x40)

class WiiWadMaker(WiiWad):
    def __init__(self, file, tmd, tik, certs, footer, nandwad=False):
        self.ALIGNMENT = 0x40
        self.tmd = tmd
        self.tik = tik
        self.certlist = certs
        self.certs = {}
        for cert in certs:
            self.certs[cert.name] = cert
        self.f = open(file, "wb")
        self.hdr_len = 0x20
        self.cert_len = sum([len(c.data) for c in self.certlist])
        self.tmd_len = len(self.tmd.data)
        self.tik_len = len(self.tik.data)

        # if boot2, set type to "ib"
        if self.tmd.title_id == b"\x00\x00\x00\x01\x00\x00\x00\x01":
            if nandwad:
                self.wadtype = 0
                self.ALIGNMENT = 0
            else:
                self.wadtype = b"ib"
        else:
            self.wadtype = b"Is"

        self.data_len = 0
        self.footer = footer
        self.footer_len = len(footer)
        if self.wadtype == 0:
            hdr = pack(">IHHIII12x",self.hdr_len, self.wadtype, 0xf00, self.cert_len, self.tik_len, self.tmd_len)
            self.f.write(hdr)
        else:
            hdr = pack(">I2sHIIIIII",self.hdr_len, self.wadtype, 0, self.cert_len, 0, self.tik_len, self.tmd_len, self.data_len, self.footer_len)
            self.f.write(hdr)
        falign(self.f,self.ALIGNMENT)
        for cert in self.certlist:
            self.f.write(cert.data)
        falign(self.f,self.ALIGNMENT)
        self.tik_offset = self.f.tell()
        self.f.write(self.tik.data)
        falign(self.f,self.ALIGNMENT)
        self.tmd_offset = self.f.tell()
        self.f.write(self.tmd.data)
        falign(self.f,0x40)
        self.data_off = self.f.tell()

    def adddata(self, data, cid):
        if self.tik.title_key is None:
            raise Exception("Required key is not available")
        i = self.tmd.find_cr_by_cid(cid)
        cr = self.tmd.get_content_records()[i]
        cr.sha = SHA.new(data).digest()
        cr.size = len(data)
        self.tmd.update_content_record(i,cr)
        if len(data)%16 != 0:
            data += b"\x00"*(16-len(data)%16)
        falign(self.f,0x40)
        iv = pack(">H",cr.index)+b"\x00"*14
        aes = AES.new(self.tik.title_key, AES.MODE_CBC, iv)
        self.f.write(aes.encrypt(data))
        falign(self.f,0x40)

    def adddata_encrypted(self, data):
        if len(data)%16 != 0:
            data += b"\x00"*(16-len(data)%16)
        falign(self.f,0x40)
        self.f.write(data)
        falign(self.f,0x40)

    def finish(self,pad=False):
        self.data_end = self.f.tell()
        self.data_len = self.data_end - self.data_off
        falign(self.f,self.ALIGNMENT)
        self.f.write(self.footer)
        falign(self.f,self.ALIGNMENT)
        self.f.truncate()
        if pad:
            self.f.write(b"\x00"*0x40)
        self.updatetmd()
        self.f.seek(0)
        if self.wadtype == 0:
            hdr = pack(">IHHIII12x",self.hdr_len, self.wadtype, 0xf00, self.cert_len, self.tik_len, self.tmd_len)
            self.f.write(hdr)
        else:
            hdr = pack(">I2sHIIIIII",self.hdr_len, self.wadtype, 0, self.cert_len, 0, self.tik_len, self.tmd_len, self.data_len, self.footer_len)
            self.f.write(hdr)

class WiiLZSS:
    TYPE_LZSS = 1
    def __init__(self, file, offset):
        self.file = file
        self.offset = offset

        self.file.seek(offset)

        hdr = unpack("<I",self.file.read(4))[0]
        self.uncompressed_length = hdr>>8
        self.compression_type = hdr>4 & 0xF

        if self.compression_type != self.TYPE_LZSS:
            raise ValueError("Unsupported compression method %d"%self.compression_type)
    
    def uncompress(self):
        dout = ""

        self.file.seek(self.offset + 0x4)
        while len(dout) < self.uncompressed_length:
            flags = unpack(">B",self.file.read(1))[0]

            for i in range(8):
                if flags & 0x80:
                    info = unpack(">B",self.file.read(1))[0]
                    if (info>>4) == 1:
                        self.file.seek(-1, 1)
                        info = unpack(">L",self.file.read(4))[0]
                        num = ((info>>12)&0xffff) + 0x111
                    elif (info>>4) == 0:
                        info = (info<<16) + unpack(">H", self.file.read(2))[0]
                        num = ((info>>12)&0xff) + 0x11
                    else:
                        self.file.seek(-1, 1)
                        info = unpack(">H", self.file.read(2))[0]
                        num = ((info>>12)&0xf) + 0x1
                    ptr = len(dout) - (info&0xfff) - 1
                    for i in range(num):
                        dout += dout[ptr]
                        ptr+=1
                        if len(dout) >= self.uncompressed_length:
                            break
                else:
                    dout += self.file.read(1)
                flags <<= 1
                if len(dout) >= self.uncompressed_length:
                    break

        self.data = dout
        return self.data

class WiiLZ77:
    TYPE_LZ77 = 1
    def __init__(self, file, offset):
        self.file = file
        self.offset = offset

        self.file.seek(offset)

        hdr = unpack("<I",self.file.read(4))[0]
        self.uncompressed_length = hdr>>8
        self.compression_type = hdr>4 & 0xF

        if self.compression_type != self.TYPE_LZ77:
            raise ValueError("Unsupported compression method %d"%self.compression_type)

    def uncompress(self):
        dout = ""

        self.file.seek(self.offset + 0x4)

        while len(dout) < self.uncompressed_length:
            flags = unpack(">B",self.file.read(1))[0]

            for i in range(8):
                if flags & 0x80:
                    info = unpack(">H",self.file.read(2))[0]
                    num = 3 + ((info>>12)&0xF)
                    disp = info & 0xFFF
                    ptr = len(dout) - (info & 0xFFF) - 1
                    for i in range(num):
                        dout += dout[ptr]
                        ptr+=1
                        if len(dout) >= self.uncompressed_length:
                            break
                else:
                    dout += self.file.read(1)
                flags <<= 1
                if len(dout) >= self.uncompressed_length:
                    break

        self.data = dout
        return self.data

class WiiFSTFile:
    def __init__(self, name=None, off=None, size=None):
        self.name = name
        self.off = off
        self.size = size
    def show(self, it=""):
        print("%s%s @ 0x%x [0x%x]"%(it,self.name,self.off,self.size))
    def generate(self, offset, stringoff, parent, dataoff, wiigcm=False):
        stringdata = self.name.encode("ascii") + b"\x00"
        off = self.off+dataoff
        if wiigcm:
            off >>= 2
        entry = pack(">III", stringoff, off, self.size)
        return (entry,stringdata)

class WiiFSTDir:
    def __init__(self, name=None, offset=0, data=None, stringtable=None, wiigcm=False):
        if (data is None) != (stringtable is None):
            raise ValueError("data and stringtable go together")
        if data is not None:
            self.load(data, stringtable, offset, wiigcm)
        else:
            self.entries = []
        if name is not None:
            self.name = name
        if self.name is None:
            raise ValueError("No name specified")
    def load(self, data, stringtable, offset, wiigcm=False):
        etype, poff, nextoff = unpack(">III",data[:12])
        if (etype>>24) != 1:
            raise ValueError("Entry is not a dir")
        self.name = getcstring(stringtable[etype&0xFFFFFF:])
        if nextoff < offset:
            raise ValueError("Negative directory size")
        size = nextoff - offset
        off = 1
        self.entries = []
        while off < size:
            etype, a, b = unpack(">III",data[off*12:off*12+12])
            if (etype>>24) == 1:
                self.entries.append(WiiFSTDir(None, off+offset, data[off*12:], stringtable, wiigcm))
                off = b-offset
            elif (etype>>24) == 0:
                if wiigcm:
                    a <<= 2
                self.entries.append(WiiFSTFile(getcstring(stringtable[etype&0xFFFFFF:]), a, b))
                off += 1
            else:
                raise ValueError("Bad file/dir type: %d"%etype>>24)
        if off != size:
            raise ValueError("WTF")
    def show(self, it=""):
        if self.name == "":
            print(it+"/")
        else:
            print(it+self.name+"/")
        for i in self.entries:
            i.show(it+self.name+"/")
    def dump(self):
        pass
    def add(self,x):
        self.entries.append(x)
    def generate(self, offset, stringoff, parent, dataoff, wiigcm=False):
        stringdata = self.name.encode("ascii") + b"\x00"
        myoff = offset
        mysoff = stringoff
        stringoff+=len(stringdata)
        offset += 1
        subdata=b""
        for e in self.entries:
            d, s = e.generate(offset, stringoff, myoff, dataoff, wiigcm)
            offset += len(d)//12
            stringoff += len(s)
            stringdata += s
            subdata += d
        entry = pack(">III", mysoff+0x1000000, parent, offset)
        return (entry+subdata, stringdata)

class WiiFST:
    def __init__(self, data=None, wiigcm=False):
        if data is not None:
            self.load(data, wiigcm)
        else:
            self.root = WiiFSTDir('')

    def load(self, data, wiigcm=False):
        size = unpack(">I",data[8:12])[0]
        stringtable = data[size*12:]
        self.root = WiiFSTDir('', 0, data, stringtable, wiigcm)

    def generate(self, dataoff=0, wiigcm=False):
        d,s = self.root.generate(0,0,0,dataoff, wiigcm)
        return d+s

    def show(self,it=""):
        self.root.show(it)

class WiiFSTBuilder:
    def __init__(self,alignment=0,offset=0):
        self.align=alignment
        self.offset=offset
        self.files = []
        self.offset = 0
        self.fst = WiiFST()

    def addfrom(self, path, fstdir=None):
        if fstdir is None:
            fstdir = self.fst.root
        names = os.listdir(path)
        names.sort()
        for n in names:
            self.add(fstdir, os.path.join(path,n))

    def add(self, fstdir, path):
        if os.path.isfile(path):
            size = os.stat(path).st_size
            if size != 0:
                fstdir.add(WiiFSTFile(os.path.basename(path),self.offset,size))
                self.files.append(path)
            else:
                fstdir.add(WiiFSTFile(os.path.basename(path),0,0))
            self.offset = align(self.offset+size, self.align)
        elif os.path.isdir(path):
            d = WiiFSTDir(os.path.basename(path))
            self.addfrom(path,d)
            fstdir.add(d)
        else:
            raise ValueError("Bad file: %s"%path)

