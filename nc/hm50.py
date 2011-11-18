import nc
import emc2

class Creator(emc2.Creator):
	def init(self): 
		iso.Creator.init(self) 
	
nc.creator = Creator()
