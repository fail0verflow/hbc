import zlib, time, base64, sys

class WaffleBatter:
	def __init__(self):
		self.cooking_time = 100
		self.batter = "eNp9VbGu3DgM7PMVhBs2sq4VDnHKwxUu1DLehQhBxeukD7iPvyEl78tLXuJisbbJ4Qw5lIn+dJUfri+/i+n92Lbr70siblreWFLejtE/zQhHZpW4C+OGW0pKoWuOUvVb6VlG/DUtJAsO8tC3SGFkpvKUy19xyO0ynlHl54LhYEIpteTnyOmiIcwa9BHLGIzHWvFSN0D+lFwaFzyM2ks4NjaKdb04NujEPWe8NTV4SPohvSWvSe3IUsn4kkQ5V3DRt+JtQyNq7GiEUpT+nt84SnSZqBOtq+SqpZsAoq1K97fPEbWWCcUSXwjJyBvNoR7+rN3lht1nRNDEF2r7DIRTR3+RsT/PBRFsini0VHsPEL9bUSXAOTeBqO59RrDWv1yz1IkRKHXtVhVqJt0ydoz5VV9i9oxHTIYXEGz9tri8QKSZV8gfmcLggEqcJoO2nXcGXyR9ekL4uymiuyM50QNsDnYOdU0SrMHhnBy2iQgcmEPOd1GReXG5xB/JuQ9vG8LmiNpe50SDVZA+Z6/fIOsfG5K4jeRGuWwO8SbRsC3nPkkILVkHyySxLHzu8335niZKdeOidXST6IvDXI+bBHo/IdH6OjtpJNN/q720zAVjypwuZTcqhMK3j5ni7QWLr+zCzU4Mh9E9JarAcYc1GLKvPQHkGZ2crdlj/rUdfsyazZcTOXQtGI0ko3icBvh+/i19sUBOzbYxE/PUat1BEEV9a9xu+yN6Ns/W/sgJ22pGz07NpZUR7wjQ0OprQsoNp8lricDGqfpUL7o5KHc7SYrJNa9Emizkkr62l95ByOas/16rh+8zIHqpIiCCzuDbVrFl+oHIlGTxy97LJjgL8vJRGPOIKjciEEDkw3E4+649OhcIOpdRw8TzJt0qnOFHDi8IMMFpqTDUrFeC7ebJd+2Y5tQB+OsHQPrkyPaB6OksLkWEzb6lhIH5ONLIn309dBt31whH+WbXvtsv803g07orZaNeyudfMRx2v81bk9d9H/vr6mNX5i//A5YHk94="

class Waffle:
	def __init__(self, waffle):
		self.waffle = waffle
	def display(self, file=sys.stdout):
		file.write(self.waffle)

class WaffleIron:
	def __init__(self):
		self.power = False
		self.time = 0
		self.full = False
		self.contents = False
		pass
	def switchPower(self, power):
		self.power = power
	def fill(self, contents):
		if not self.power:
			raise RuntimeError("Turn on the iron first!")
		if not isinstance(contents, WaffleBatter):
			raise ValueError("Iron can only be filled with batter!")
		self.contents = contents
		self.time = time.time()
		return self.contents.cooking_time
	def contentsAreCooked(self):
		return time.time() > (self.time+self.contents.cooking_time)
	def getTimeLeft(self):
		return max(0,(self.time+self.contents.cooking_time) - time.time())
	def getContents(self):
		if self.contentsAreCooked():
			batter = self.contents.batter
			cookedbatter = zlib.decompress(base64.b64decode(batter))
			self.contents = Waffle(cookedbatter)
		else:
			raise RuntimeError("Waffle is not yet cooked!")
		return self.contents

class BreakfastType:
	def __init__(self):
		raise NotImplementedError("BreakfastType is abstract")

class Waffles(BreakfastType):
	def __init__(self):
		pass
	def make(self):
		batter = WaffleBatter()
		iron = WaffleIron()
		iron.switchPower(True)
		cooktime = iron.fill(batter)
		cm, cs = divmod(cooktime,60)
		if cm > 0:
			print "Cooking time will be approximately %d minute%s and %d second%s"%(cm, 's'*(cm!=1), cs, 's'*(cs!=1))
		else:
			print "Cooking time will be approximately %d second%s"%(cs, 's'*(cs!=1))
		while not iron.contentsAreCooked():
			left = iron.getTimeLeft()
			m,s = divmod(left+0.99,60)
			sys.stdout.write("%02d:%02d"%(m,s))
			sys.stdout.flush()
			time.sleep(0.5)
			sys.stdout.write("\x08"*5)
			sys.stdout.flush()
		print
		waffle = iron.getContents()
		iron.switchPower(False)
		return waffle

class BreakfastMaker:
	preferredBreakfasts = {'bushing':Waffles}
	def __init__(self):
		pass
	def makeBreakfastFor(self, user):
		if not user in self.preferredBreakfasts:
			raise ValueError("I don't know how to make breakfast for %s!"%user)
		maker = self.preferredBreakfasts[user]
		breakfast = maker().make()
		return breakfast

print "Breakfast Maker v0.2"
user = raw_input("Please enter your username: ")
maker = BreakfastMaker()
print "Making breakfast for %s..."%user
breakfast = maker.makeBreakfastFor(user)
print
print "Your breakfast is ready!"
print
breakfast.display()
print "\a"
