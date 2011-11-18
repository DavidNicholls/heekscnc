import nc
import iso
import math

class Creator(iso.Creator):
	def init(self): 
		iso.Creator.init(self) 

	def SPACE(self): return(' ')
	# def TAP(self): return('G33.1')
	# def TAP_DEPTH(self, depth): return(self.SPACE() + 'K' + (self.fmt.string(depth)))
	def BORE_FEED_OUT(self): return('G85')
	def BORE_SPINDLE_STOP_RAPID_OUT(self): return('G86')
	def BORE_DWELL_FEED_OUT(self, format, dwell): return('G89') + self.SPACE() + (format % dwell)
	def FEEDRATE(self): return((self.SPACE() + ' F'))
	def COMMENT(self,comment):
		# Replace any embedded round brackets with curly braces so that the EMC2 GCode
		# interpreter will not have trouble with the nested comment format.
		_comment = comment.replace('(','{')
		_comment = _comment.replace(')','}')
		return( ('(%s)' % _comment ) )


	def program_begin(self, id, comment):
		self.write( ('(' + comment + ')' + '\n') )
		self.write_blocknum()
		self.write( self.DISABLE_TOOL_LENGTH_COMPENSATION() + '\t(Ensure tool length compensation is OFF)\n' )
		self.tool_length_compenstation_enabled = False
		self.write_blocknum()
		self.write((self.REMOVE_TEMPORARY_COORDINATE_SYSTEM() + '\t(Ensure no temporary coordinate systems are in effect)\n'))

 ############################################################################
    ##  Settings
    
	def imperial(self):
            self.write_blocknum()
            self.write( self.IMPERIAL() + '\t (Imperial Values)\n')
            self.fmt.number_of_decimal_places = 4
	    self.gcode_is_metric = False

	def metric(self):
            self.write_blocknum()
            self.fmt.number_of_decimal_places = 3
            self.write( self.METRIC() + '\t (Metric Values)\n' )
	    self.gcode_is_metric = True

	def absolute(self):
		self.write_blocknum()
		self.write( self.ABSOLUTE() + '\t (Absolute Coordinates)\n')

	def incremental(self):
		self.write_blocknum()
		self.write( self.INCREMENTAL() + '\t (Incremental Coordinates)\n' )

	def polar(self, on=True):
		if (on) :
			self.write_blocknum()
			self.write(self.POLAR_ON() + '\t (Polar ON)\n' )
		else : 
			self.write_blocknum()
			self.write(self.POLAR_OFF() + '\t (Polar OFF)\n' )

	def set_plane(self, plane):
		if (plane == 0) : 
			self.write_blocknum()
			self.write(self.PLANE_XY() + '\t (Select XY Plane)\n')
		elif (plane == 1) :
			self.write_blocknum()
			self.write(self.PLANE_XZ() + '\t (Select XZ Plane)\n')
		elif (plane == 2) : 
			self.write_blocknum()
			self.write(self.PLANE_YZ() + '\t (Select YZ Plane)\n')

	def comment(self, text):
		self.write_blocknum()
		self.write((self.COMMENT(text) + '\n'))

	# This is the coordinate system we're using.  G54->G59, G59.1, G59.2, G59.3
	# These are selected by values from 1 to 9 inclusive.
	def workplane(self, id):
		self.current_workplane = id
		if ((id >= 1) and (id <= 6)):
			self.write_blocknum()
			self.write( (self.WORKPLANE() % (id + self.WORKPLANE_BASE())) + '\t (Select Relative Coordinate System)\n')
		if ((id >= 7) and (id <= 9)):
			self.write_blocknum()
			self.write( ((self.WORKPLANE() % (6 + self.WORKPLANE_BASE())) + ('.%i' % (id - 6))) + '\t (Select Relative Coordinate System)\n')

	def report_probe_results(self, x1=None, y1=None, z1=None, x2=None, y2=None, z2=None, x3=None, y3=None, z3=None, x4=None, y4=None, z4=None, x5=None, y5=None, z5=None, x6=None, y6=None, z6=None, xml_file_name=None ):
		if (xml_file_name != None):
			self.comment('Generate an XML document describing the probed coordinates found');
			self.write_blocknum()
			self.write('(LOGOPEN,')
			self.write(xml_file_name)
			self.write(')\n')

		self.write_blocknum()
		self.write('(LOG,<POINTS>)\n')

		if ((x1 != None) or (y1 != None) or (z1 != None)):
			self.write_blocknum()
			self.write('(LOG,<POINT>)\n')

		if (x1 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + x1 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<X>#<_value></X>)\n')

		if (y1 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + y1 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Y>#<_value></Y>)\n')

		if (z1 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + z1 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Z>#<_value></Z>)\n')

		if ((x1 != None) or (y1 != None) or (z1 != None)):
			self.write_blocknum()
			self.write('(LOG,</POINT>)\n')

		if ((x2 != None) or (y2 != None) or (z2 != None)):
			self.write_blocknum()
			self.write('(LOG,<POINT>)\n')

		if (x2 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + x2 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<X>#<_value></X>)\n')

		if (y2 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + y2 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Y>#<_value></Y>)\n')

		if (z2 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + z2 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Z>#<_value></Z>)\n')

		if ((x2 != None) or (y2 != None) or (z2 != None)):
			self.write_blocknum()
			self.write('(LOG,</POINT>)\n')

		if ((x3 != None) or (y3 != None) or (z3 != None)):
			self.write_blocknum()
			self.write('(LOG,<POINT>)\n')

		if (x3 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + x3 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<X>#<_value></X>)\n')

		if (y3 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + y3 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Y>#<_value></Y>)\n')

		if (z3 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + z3 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Z>#<_value></Z>)\n')

		if ((x3 != None) or (y3 != None) or (z3 != None)):
			self.write_blocknum()
			self.write('(LOG,</POINT>)\n')

		if ((x4 != None) or (y4 != None) or (z4 != None)):
			self.write_blocknum()
			self.write('(LOG,<POINT>)\n')

		if (x4 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + x4 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<X>#<_value></X>)\n')

		if (y4 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + y4 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Y>#<_value></Y>)\n')

		if (z4 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + z4 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Z>#<_value></Z>)\n')

		if ((x4 != None) or (y4 != None) or (z4 != None)):
			self.write_blocknum()
			self.write('(LOG,</POINT>)\n')

		if ((x5 != None) or (y5 != None) or (z5 != None)):
			self.write_blocknum()
			self.write('(LOG,<POINT>)\n')

		if (x5 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + x5 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<X>#<_value></X>)\n')

		if (y5 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + y5 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Y>#<_value></Y>)\n')

		if (z5 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + z5 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Z>#<_value></Z>)\n')

		if ((x5 != None) or (y5 != None) or (z5 != None)):
			self.write_blocknum()
			self.write('(LOG,</POINT>)\n')

		if ((x6 != None) or (y6 != None) or (z6 != None)):
			self.write_blocknum()
			self.write('(LOG,<POINT>)\n')

		if (x6 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + x6 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<X>#<_value></X>)\n')

		if (y6 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + y6 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Y>#<_value></Y>)\n')

		if (z6 != None):
			self.write_blocknum()
			self.write('#<_value>=[' + z6 + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Z>#<_value></Z>)\n')

		if ((x6 != None) or (y6 != None) or (z6 != None)):
			self.write_blocknum()
			self.write('(LOG,</POINT>)\n')

		self.write_blocknum()
		self.write('(LOG,</POINTS>)\n')

		if (xml_file_name != None):
			self.write_blocknum()
			self.write('(LOGCLOSE)\n')
			
	def open_log_file(self, xml_file_name=None ):
		self.write_blocknum()
		self.write('(LOGOPEN,')
		self.write(xml_file_name)
		self.write(')\n')
			
	def close_log_file(self):
		self.write_blocknum()
		self.write('(LOGCLOSE)\n')
			
	def log_coordinate(self, x=None, y=None, z=None):
		if ((x != None) or (y != None) or (z != None)):
			self.write_blocknum()
			self.write('(LOG,<POINT>)\n')

		if (x != None):
			self.write_blocknum()
			self.write('#<_value>=[' + x + ']\n')
			self.write_blocknum()
			self.write('(LOG,<X>#<_value></X>)\n')

		if (y != None):
			self.write_blocknum()
			self.write('#<_value>=[' + y + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Y>#<_value></Y>)\n')

		if (z != None):
			self.write_blocknum()
			self.write('#<_value>=[' + z + ']\n')
			self.write_blocknum()
			self.write('(LOG,<Z>#<_value></Z>)\n')

		if ((x != None) or (y != None) or (z != None)):
			self.write_blocknum()
			self.write('(LOG,</POINT>)\n')

	def log_message(self, message=None ):
		self.write_blocknum()
		self.write('(LOG,' + message + ')\n')
		
	def message(self, text=None ):
		# Replace any embedded round brackets with curly braces so that the EMC2 GCode
		# interpreter will not have trouble with the nested comment format.
		_message = text.replace('(','{')
		_message = _message.replace(')','}')
		self.write_blocknum()
		self.write('(MSG,' + _message + ')\n')
		
	def start_CRC(self, left = True, radius = 0.0):
		if self.t == None:
			raise "No tool specified for start_CRC()"
		self.write_blocknum()
		if left:
			self.write(('G41' + self.SPACE() + 'D%i') % self.t  + '\t (start left cutter radius compensation)\n' )
		else:
			self.write(('G42' + self.SPACE() + 'D%i') % self.t  + '\t (start right cutter radius compensation)\n' )

	def end_CRC(self):
		self.g = 'G40'
		self.write_blocknum()
		self.write_preps()
		self.write_misc()
		self.write('\t (end cutter radius compensation)\n')
		
	
	# G33.1 tapping with EMC for now
	# unsynchronized (chuck) taps NIY (tap_mode = 1)
    
	def oldtap(self, x=None, y=None, z=None, zretract=None, depth=None, standoff=None, dwell_bottom=None, pitch=None, stoppos=None, spin_in=None, spin_out=None, tap_mode=None, direction=None):
		# mystery parameters: 
		# zretract=None, dwell_bottom=None,pitch=None, stoppos=None, spin_in=None, spin_out=None):
		# I dont see how to map these to EMC Gcode

		if (standoff == None):		
			# This is a bad thing.  All the drilling cycles need a retraction (and starting) height.		
			return
		if (z == None): 
			return	# We need a Z value as well.  This input parameter represents the top of the hole 
		if (pitch == None): 
			return	# We need a pitch value.
		if (direction == None): 
			return	# We need a direction value.

		if (tap_mode != 0):
			raise "only rigid tapping currently supported"

		self.write_preps()
		self.write_blocknum()				
		self.write_spindle()
		self.write('\n')

		# rapid to starting point; z first, then x,y iff given

		# Set the retraction point to the 'standoff' distance above the starting z height.		
		retract_height = z + standoff		

		# unsure if this is needed:
		if self.z != retract_height:
				self.rapid(z = retract_height)

		# then continue to x,y if given
		if (x != None) or (y != None):
				self.write_blocknum()				
				self.write(self.RAPID() )		   

				if (x != None):		
						self.write(self.X() + self.fmt.string(x))		
						self.x = x 

				if (y != None):		
						self.write(self.Y() + self.fmt.string(y))		
						self.y = y
				self.write('\n')
       
		self.write_blocknum()				
		self.write( self.TAP() )
		self.write( self.TAP_DEPTH(pitch) + self.SPACE() )			
		self.write(self.Z() + self.fmt.string(z - depth))	# This is the 'z' value for the bottom of the tap.
		self.write_misc()	
		self.write('\n')

		self.z = retract_height	# this cycle returns to the start position, so remember that as z value

	def tool_defn(self, id, name='', radius=None, length=None, gradient=None):
		pass

	def nurbs_begin_definition(self, id, degree=None, x=None, y=None, weight=None):
		self.write_blocknum()
		self.write( self.NURBS_BEGIN() )
		self.write( self.SPACE() + self.DEGREE() + (self.fmt.string(degree + 1) ) )
		self.write( self.SPACE() + self.X() + (self.fmt.string(x) ) )
		self.write( self.SPACE() + self.Y() + (self.fmt.string(y) ) )
		self.write( self.SPACE() + self.WEIGHT() + (self.fmt.string(weight) ) )
		self.write( '\n')

	def nurbs_add_pole(self, id, x=None, y=None, weight=None):
		self.write_blocknum()
		self.write( self.SPACE() + self.X() + (self.fmt.string(x) ) )
		self.write( self.SPACE() )
		self.write( self.SPACE() + self.Y() + (self.fmt.string(y) ) )
		self.write( self.SPACE() )
		self.write( self.SPACE() + self.WEIGHT() + (self.fmt.string(weight) ) )
		self.write( '\n')

	def nurbs_end_definition(self, id):
		self.write_blocknum()
		self.write( self.NURBS_END() )
		self.write( '\n')

	def measure_and_offset_tool(self, distance=None, switch_offset_variable_name=None, fixture_offset_variable_name=None, feed_rate=None ):
	        self.write_blocknum()
		self.write(self.DISABLE_TOOL_LENGTH_COMPENSATION() + ' (Turn OFF tool length compensation)\n');
		self.tool_length_compenstation_enabled = False

		self.write_blocknum()
		self.write(self.SPACE() + self.SET_TEMPORARY_COORDINATE_SYSTEM() + ' X 0 Y 0 Z 0\t(Temporarily make this the origin)\n')

		# Probe downwards until we hit the tool length measurement switch
		self.write_blocknum()
		self.write((self.PROBE_TOWARDS_WITH_SIGNAL() + self.SPACE() + (self.Z() + (self.fmt.string(-1.0 * float(distance))))) )
		self.write( self.SPACE() + self.FEEDRATE() + self.fmt.string(feed_rate) + '\t(Probe down to find the tool length switch)\n' )

		self.write_blocknum()
		self.write(self.ENABLE_TOOL_LENGTH_COMPENSATION() + self.SPACE() + self.Z() + '[#5063 - #' + switch_offset_variable_name + ']' + ' (Turn ON tool length compensation)\n');
		self.tool_length_compenstation_enabled = True

		self.write_blocknum()
		self.write((self.REMOVE_TEMPORARY_COORDINATE_SYSTEM() + '\t(Ensure no temporary coordinate systems are in effect)\n'))

nc.creator = Creator()

