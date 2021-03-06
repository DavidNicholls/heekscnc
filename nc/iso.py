################################################################################
# iso.py
#
# Simple ISO NC code creator
#
# Hirutso Enni, 2009-01-13

import nc
import math
from format import Format
from format import *

################################################################################
class Creator(nc.Creator):

    def __init__(self):
        nc.Creator.__init__(self)

        self.a = 0
        self.b = 0
        self.c = 0
        self.f = Address('F', fmt = Format(number_of_decimal_places = 2))
        self.fh = None
        self.fv = None
        self.fhv = False
        self.g_plane = Address('G', fmt = Format(number_of_decimal_places = 0))
        self.g_list = []
        self.i = 0
        self.j = 0
        self.k = 0
        self.m = []
        self.n = 10
        self.r = 0
        self.s = AddressPlusMinus('S', fmt = Format(number_of_decimal_places = 2), modal = False)
        self.t = None
        self.x = 0
        self.y = 0
        self.z = 500
        self.g0123_modal = False
        self.drill_modal = False
        self.prev_f = ''
        self.prev_g0123 = ''
        self.prev_drill = ''
        self.prev_retract = ''
        self.prev_z = ''
        self.useCrc = False
        self.useCrcCenterline = False
        self.gCRC = ''
        self.fmt = Format()
        self.absolute_flag = True
        self.ffmt = Format(number_of_decimal_places = 2)
        self.sfmt = Format(number_of_decimal_places = 1)
        self.arc_centre_absolute = False
        self.arc_centre_positive = False
        self.in_arc_splitting = False
        self.machine_coordinates = False
	self.tool_length_compenstation_enabled = False
	self.gcode_is_metric = True
	self.machine_is_metric = False	# Needed when looking at machine variables such as #5063
	self.current_workplane = 1;	# G54 by default.

        
    ############################################################################
    ##  Codes

    def SPACE(self): return(' ')
    def FORMAT_FEEDRATE(self): return('%.2f') 
    def FORMAT_ANG(self): return('%.1f')
    def FORMAT_TIME(self): return('%.2f')
    def FORMAT_DWELL(self): return('P%f')

    def BLOCK(self): return('N%i')
    def COMMENT(self,comment): return( ('(%s)' % comment ) )
    def VARIABLE(self): return( '#%i')
    def VARIABLE_SET(self): return( '=%.3f')

    def PROGRAM(self): return( 'O%i')
    def PROGRAM_END(self): return( 'M02')

    def SUBPROG_CALL(self): return( 'M98' + self.SPACE() + 'P%i')
    def SUBPROG_END(self): return( 'M99')

    def STOP_OPTIONAL(self): return('M01')
    def STOP(self): return('M00')

    def IMPERIAL(self): return('G20')
    def METRIC(self): return('G21')
    def ABSOLUTE(self): return('G90')
    def INCREMENTAL(self): return('G91')
    def SET_TEMPORARY_COORDINATE_SYSTEM(self): return('G92')
    def REMOVE_TEMPORARY_COORDINATE_SYSTEM(self): return('G92.1')
    def POLAR_ON(self): return('G16')
    def POLAR_OFF(self): return('G15')
    def PLANE_XY(self): return('G17')
    def PLANE_XZ(self): return('G18')
    def PLANE_YZ(self): return('G19')

    def TOOL(self): return('T%i' + self.SPACE() + 'M06')
    def TOOL_DEFINITION(self): return('G10' + self.SPACE() + 'L1')

    def WORKPLANE(self): return('G%i')
    def WORKPLANE_BASE(self): return(53)

    def SPINDLE_CW(self): return('M03')
    def SPINDLE_CCW(self): return('M04')
    def COOLANT_OFF(self): return('M09')
    def COOLANT_MIST(self): return('M07')
    def COOLANT_FLOOD(self): return('M08')
    def GEAR_OFF(self): return('?')
    def GEAR(self): return('M%i')
    def GEAR_BASE(self): return(37)

    def RAPID(self): return('G00')
    def FEED(self): return('G01')
    def ARC_CW(self): return('G02')
    def ARC_CCW(self): return('G03')
    def NURBS_BEGIN(self): return('G5.2')
    def NURBS_END(self): return('G5.3')
    def WEIGHT(self): return('P')
    def DEGREE(self): return('L')
    def DWELL(self): return('G04')
    def DRILL(self): return('G81')
    def DRILL_WITH_DWELL(self, format, dwell): return('G82' + self.SPACE() + (format.string(dwell)))
    def PECK_DRILL(self): return('G83')
    def PECK_DEPTH(self, format, depth): return(self.SPACE() + 'Q' + (format.string(depth)))
    def RETRACT(self, format, height): return(self.SPACE() + 'R' + (format.string(height)))
    def END_CANNED_CYCLE(self): return('G80')
    def TAP(self): return('G84')
    def TAP_DEPTH(self, format, depth): return(self.SPACE() + 'K' + (format.string(depth)))

    def X(self): return('X')
    def Y(self): return('Y')
    def Z(self): return('Z')
    def A(self): return('A')
    def B(self): return('B')
    def C(self): return('C')
    def CENTRE_X(self): return('I')
    def CENTRE_Y(self): return('J')
    def CENTRE_Z(self): return('K')
    def RADIUS(self): return('R')
    def TIME(self): return('P')

    def PROBE_TOWARDS_WITH_SIGNAL(self): return('G38.2')
    def PROBE_TOWARDS_WITHOUT_SIGNAL(self): return('G38.3')
    def PROBE_AWAY_WITH_SIGNAL(self): return('G38.4')
    def PROBE_AWAY_WITHOUT_SIGNAL(self): return('G38.5')

    def MACHINE_COORDINATES(self): return('G53')

    def EXACT_PATH_MODE(self): return('G61')
    def EXACT_STOP_MODE(self): return('G61.1')
        
    def ENABLE_TOOL_LENGTH_COMPENSATION(self): return('G43.1')
    def DISABLE_TOOL_LENGTH_COMPENSATION(self): return('G49')

    ############################################################################
    ##  Internals

    def write_feedrate(self):
        self.write(self.f)

    def write_preps(self):
        self.g_plane.write(self)
        for g in self.g_list:
            self.write(self.SPACE() + 'G' + g)
        self.g_list = []

    def write_misc(self):
        if (len(self.m)) : self.write(self.m.pop())

    def write_blocknum(self):
        self.write((self.BLOCK() % self.n) + self.SPACE())
        self.n += 10
        
    def write_spindle(self):
        self.s.write(self)

    ############################################################################
    ##  Programs

    def program_begin(self, id, name=''):
        self.write((self.PROGRAM() % id) + self.SPACE() + (self.COMMENT(name)))
        self.write('\n')

    def program_stop(self, optional=False):
        self.write_blocknum()
        if (optional) : 
            self.write(self.SPACE() + self.STOP_OPTIONAL() + '\n')
            self.prev_g0123 = ''
        else : 
            self.write(self.STOP() + '\n')
            self.prev_g0123 = ''


    def program_end(self):
	if self.tool_length_compenstation_enabled:
           self.write_blocknum()
           self.write( self.DISABLE_TOOL_LENGTH_COMPENSATION() + '\n' )
	   self.tool_length_compenstation_enabled = False

        self.write_blocknum()
        self.write(self.SPACE() + self.PROGRAM_END() + '\n')


    def flush_nc(self):
        if len(self.g_list) == 0 and len(self.m) == 0: return
        self.write_blocknum()
        self.write_preps()
        self.write_misc()
        self.write('\n')

    ############################################################################
    ##  Subprograms
    
    def sub_begin(self, id, name=''):
        self.write((self.PROGRAM() % id) + self.SPACE() + (self.COMMENT(name)))
        self.write('\n')

    def sub_call(self, id):
        self.write_blocknum()
        self.write(self.SPACE() + (self.SUBPROG_CALL() % id) + '\n')

    def sub_end(self):
        self.write_blocknum()
        self.write(self.SPACE() + self.SUBPROG_END() + '\n')

    ############################################################################
    ##  Settings
    
    def imperial(self):
        self.g_list.append(self.IMPERIAL())
        self.fmt.number_of_decimal_places = 4
	self.gcode_is_metric = False

    def metric(self):
        self.g_list.append(self.METRIC())
        self.fmt.number_of_decimal_places = 3
	self.gcode_is_metric = True

    def machine_units_metric(self, is_metric):
	self.machine_is_metric = is_metric

    def absolute(self):
        self.g_list.append(self.ABSOLUTE())
        self.absolute_flag = True

    def incremental(self):
        self.g_list.append(self.INCREMENTAL())
        self.absolute_flag = False

    def polar(self, on=True):
        if (on) : self.g_list.append(self.POLAR_ON())
        else : self.g_list.append(self.POLAR_OFF())

    def set_plane(self, plane):
        if (plane == 0) : self.g_plane.set(self.PLANE_XY())
        elif (plane == 1) : self.g_plane.set(self.PLANE_XZ())
        elif (plane == 2) : self.g_plane.set(self.PLANE_YZ())

    def set_temporary_origin(self, x=None, y=None, z=None, a=None, b=None, c=None):
        self.write_blocknum()
        self.write(self.SPACE() + (self.SET_TEMPORARY_COORDINATE_SYSTEM()))
        if (x != None): self.write( self.SPACE() + 'X ' + (self.fmt.string(x)) )
        if (y != None): self.write( self.SPACE() + 'Y ' + (self.fmt.string(y)) )
        if (z != None): self.write( self.SPACE() + 'Z ' + (self.fmt.string(z)) )
        if (a != None): self.write( self.SPACE() + 'A ' + (self.fmt.string(a)) )
        if (b != None): self.write( self.SPACE() + 'B ' + (self.fmt.string(b)) )
        if (c != None): self.write( self.SPACE() + 'C ' + (self.fmt.string(c)) )
        self.write('\n')

    def remove_temporary_origin(self):
        self.write_blocknum()
        self.write(self.SPACE() + (self.REMOVE_TEMPORARY_COORDINATE_SYSTEM()))
        self.write('\n')

    ############################################################################
    ##  Tools

    def tool_change(self, id, description=None):
	if self.tool_length_compenstation_enabled:
           self.write_blocknum()
           self.write( self.DISABLE_TOOL_LENGTH_COMPENSATION() + '\n' )
	   self.tool_length_compenstation_enabled = False

	if description != None:
		self.message(description)

        self.write_blocknum()
        self.write(self.SPACE() + (self.TOOL() % id) + '\n')
        self.t = id

    def predefined_position(self, type):
	"""Move to a predefined position (G28 or G30)"""
        self.write_blocknum()
        self.write(self.SPACE() + type + self.SPACE() + '(Move to the predefined position)\n')

    def tool_defn(self, id, name='', radius=None, length=None, gradient=None):
        self.write_blocknum()
        self.write(self.SPACE() + self.TOOL_DEFINITION())
        self.write(self.SPACE() + ('P%i' % id) + ' ')

        if (radius != None):
            self.write(self.SPACE() + ('R%.3f' % radius))

        if (length != None):
            self.write(self.SPACE() + 'Z%.3f' % length)

        self.write('\n')

    def offset_radius(self, id, radius=None):
        pass

    def offset_length(self, id, length=None):
        pass

    def measure_and_offset_tool(self, distance=None, switch_offset_variable_name=None, fixture_offset_variable_name=None, feed_rate=None ):
	"""Only implemented within EMC2 at the moment"""
        pass


    ############################################################################
    ##  Datums
    
    def datum_shift(self, x=None, y=None, z=None, a=None, b=None, c=None):
        pass

    def datum_set(self, x=None, y=None, z=None, a=None, b=None, c=None):
        pass

    # This is the coordinate system we're using.  G54->G59, G59.1, G59.2, G59.3
    # These are selected by values from 1 to 9 inclusive.
    def workplane(self, id):
	self.current_workplane = id;
        if ((id >= 1) and (id <= 6)):
            self.write_blocknum()
            self.write( (self.WORKPLANE() % (id + self.WORKPLANE_BASE())) + '\t (Select Relative Coordinate System)\n')
        if ((id >= 7) and (id <= 9)):
            self.write_blocknum()
            self.write( ((self.WORKPLANE() % (6 + self.WORKPLANE_BASE())) + ('.%i' % (id - 6))) + '\t (Select Relative Coordinate System)\n')
        

    ############################################################################
    ##  Rates + Modes

    def feedrate(self, f):
        self.f = self.SPACE() + self.FEEDRATE() + self.ffmt.string(f)
        self.fhv = False

    def feedrate_hv(self, fh, fv):
        self.fh = fh
        self.fv = fv
        self.fhv = True
	self.calc_feedrate_hv( fh, fv )

    def calc_feedrate_hv(self, h, v):
        if math.fabs(v) > math.fabs(h * 2):
            # some horizontal, so it should be fine to use the horizontal feed rate
            self.f = self.SPACE() + self.FEEDRATE() + self.ffmt.string(self.fv)
        else:
            # not much, if any horizontal component, so use the vertical feed rate
            self.f = self.SPACE() + self.FEEDRATE() + self.ffmt.string(self.fh)

    def spindle(self, s, clockwise):
        if clockwise == True:
            self.s.set(s, self.SPACE() + self.SPINDLE_CW(), self.SPACE() + self.SPINDLE_CCW())
        else:
            self.s.set(s, self.SPACE() + self.SPINDLE_CCW(), self.SPACE() + self.SPINDLE_CW())

    def coolant(self, mode=0):
        if (mode <= 0) : self.m.append(self.SPACE() + self.COOLANT_OFF())
        elif (mode == 1) : self.m.append(self.SPACE() + self.COOLANT_MIST())
        elif (mode == 2) : self.m.append(self.SPACE() + self.COOLANT_FLOOD())

    def gearrange(self, gear=0):
        if (gear <= 0) : self.m.append(self.SPACE() + self.GEAR_OFF())
        elif (gear <= 4) : self.m.append(self.SPACE() + self.GEAR() % (gear + GEAR_BASE()))

    ############################################################################
    ##  Moves

    def rapid(self, x=None, y=None, z=None, a=None, b=None, c=None, machine_coordinates=None ):
        self.write_blocknum()

        if self.machine_coordinates != False or (machine_coordinates != None and machine_coordinates == True):
            self.write( self.MACHINE_COORDINATES() + self.SPACE() )

        if self.g0123_modal:
            if self.prev_g0123 != self.RAPID():
                self.write(self.RAPID())
                self.prev_g0123 = self.RAPID()
        else:
            self.write(self.RAPID())
        self.write_preps()
        if (x != None):
            dx = x - self.x
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.X() + (self.fmt.string(x)))
            else:
                self.write(self.SPACE() + self.X() + (self.fmt.string(dx)))
            self.x = x
        if (y != None):
            dy = y - self.y
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.Y() + (self.fmt.string(y)))
            else:
                self.write(self.SPACE() + self.Y() + (self.fmt.string(dy)))

            self.y = y
        if (z != None):
            dz = z - self.z
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.Z() + (self.fmt.string(z)))
            else:
                self.write(self.SPACE() + self.Z() + (self.fmt.string(dz)))

            self.z = z

        if (a != None):
            da = a - self.a
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.A() + (self.fmt.string(a)))
            else:
                self.write(self.SPACE() + self.A() + (self.fmt.string(da)))
            self.a = a

        if (b != None):
            db = b - self.b
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.B() + (self.fmt.string(b)))
            else:
                self.write(self.SPACE() + self.B() + (self.fmt.string(db)))
            self.b = b

        if (c != None):
            dc = c - self.c
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.C() + (self.fmt.string(c)))
            else:
                self.write(self.SPACE() + self.C() + (self.fmt.string(dc)))
            self.c = c
        self.write_spindle()
        self.write_misc()
        self.write('\n')

    def feed(self, x=None, y=None, z=None):
        if self.same_xyz(x, y, z): return
        self.write_blocknum()
        if self.g0123_modal:
            if self.prev_g0123 != self.FEED():
                self.write(self.FEED())
                self.prev_g0123 = self.FEED()
        else:
            self.write(self.FEED())
        self.write_preps()
        dx = dy = dz = 0
        if (x != None):
            dx = x - self.x
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.X() + (self.fmt.string(x)))
            else:
                self.write(self.SPACE() + self.X() + (self.fmt.string(dx)))
            self.x = x
        if (y != None):
            dy = y - self.y
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.Y() + (self.fmt.string(y)))
            else:
                self.write(self.SPACE() + self.Y() + (self.fmt.string(dy)))

            self.y = y
        if (z != None):
            dz = z - self.z
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.Z() + (self.fmt.string(z)))
            else:
                self.write(self.SPACE() + self.Z() + (self.fmt.string(dz)))

            self.z = z
        if (self.fhv) : self.calc_feedrate_hv(math.sqrt(dx*dx+dy*dy), math.fabs(dz))
        self.write_feedrate()
        self.write_spindle()
        self.write_misc()
        self.write('\n')

    def nurbs_begin_definition(self, id, degree=None, x=None, y=None, weight=None):
	""" This routine has been implemented within emc2.py """
        pass

    def nurbs_add_pole(self, id, x=None, y=None, weight=None):
	""" This routine has been implemented within emc2.py """
        pass

    def nurbs_end_definition(self, id):
	""" This routine has been implemented within emc2.py """
        pass

    def same_xyz(self, x=None, y=None, z=None):
        if (x != None):
            if (self.fmt.string(x)) != (self.fmt.string(self.x)):
                return False
        if (y != None):
            if (self.fmt.string(y)) != (self.fmt.string(self.y)):
                return False
        if (z != None):
            if (self.fmt.string(z)) != (self.fmt.string(self.z)):
                return False
            
        return True
    
    def get_quadrant(self, dx, dy):
        if dx < 0:
            if dy < 0:
                return 2
            else:
                return 1
        else:
            if dy < 0:
                return 3
            else:
                return 0
    
    def quadrant_start(self, q, i, j, rad):
        while q > 3: q = q - 4
        if q == 0:
            return i + rad, j
        if q == 1:
            return i, j + rad
        if q == 2:
            return i - rad, j
        return i, j - rad

    def quadrant_end(self, q, i, j, rad):
        return self.quadrant_start(q + 1, i, j, rad)

    def arc(self, cw, x=None, y=None, z=None, i=None, j=None, k=None, r=None):
        if self.arc_centre_positive == True and self.in_arc_splitting == False:
            # split in to quadrant arcs
            self.in_arc_splitting = True
            
            if x == None: x = self.x
            if y == None: y = self.y
            sdx = self.x - i
            sdy = self.y - j
            edx = x - i
            edy = y - j
            
            qs = self.get_quadrant(sdx, sdy)
            qe = self.get_quadrant(edx, edy)
            
            if qs == qe:
                self.arc(cw, x, y, z, i, j, k, r)
            else:
                rad = math.sqrt(sdx * sdx + sdy * sdy)
                if cw:
                    if qs < qe: qs = qs + 4
                else:
                    if qe < qs: qe = qe + 4
                    
                q = qs
                while 1:
                    x1 = x
                    y1 = y
                    if q != qe:
                        if cw:
                            x1, y1 = self.quadrant_start(q, i, j, rad)
                        else:
                            x1, y1 = self.quadrant_end(q, i, j, rad)
                    if (self.fmt.string(x1) != self.fmt.string(self.x)) or (self.fmt.string(y1) != self.fmt.string(self.y)): self.arc(cw, x1, y1, z, i, j, k, r)
                    if q == qe:
                        break
                    if cw:
                        q = q - 1
                    else:
                        q = q + 1                        
                    
            self.in_arc_splitting = False
            return
            
        #if self.same_xyz(x, y, z): return
        self.write_blocknum()
        arc_g_code = ''
        if cw: arc_g_code = self.ARC_CW()
        else: arc_g_code = self.ARC_CCW()
        if self.g0123_modal:
            if self.prev_g0123 != arc_g_code:
                self.write(arc_g_code)
                self.prev_g0123 = arc_g_code
        else:
            self.write(arc_g_code)
        self.write_preps()
        if (x != None):
            dx = x - self.x
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.X() + (self.fmt.string(x)))
            else:
                self.write(self.SPACE() + self.X() + (self.fmt.string(dx)))
        if (y != None):
            dy = y - self.y
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.Y() + (self.fmt.string(y)))
            else:
                self.write(self.SPACE() + self.Y() + (self.fmt.string(dy)))
        if (z != None):
            dz = z - self.z
            if (self.absolute_flag ):
                self.write(self.SPACE() + self.Z() + (self.fmt.string(z)))
            else:
                self.write(self.SPACE() + self.Z() + (self.fmt.string(dz)))
        if (i != None):
            if self.arc_centre_absolute == False:
                i = i - self.x
            s = self.fmt.string(i)
            if self.arc_centre_positive == True:
                if s[0] == '-':
                    s = s[1:]
            self.write(self.SPACE() + self.CENTRE_X() + s)
        if (j != None):
            if self.arc_centre_absolute == False:
                j = j - self.y
            s = self.fmt.string(j)
            if self.arc_centre_positive == True:
                if s[0] == '-':
                    s = s[1:]
            self.write(self.SPACE() + self.CENTRE_Y() + s)
        if (k != None):
            if self.arc_centre_absolute == False:
                k = k - self.z
            s = self.fmt.string(k)
            if self.arc_centre_positive == True:
                if s[0] == '-':
                    s = s[1:]
            self.write(self.SPACE() + self.CENTRE_Z() + s)
        if (r != None):
            s = self.fmt.string(r)
            if self.arc_centre_positive == True:
                if s[0] == '-':
                    s = s[1:]
            self.write(self.SPACE() + self.RADIUS() + s)
#       use horizontal feed rate
        if (self.fhv) : self.calc_feedrate_hv(1, 0)
        self.write_feedrate()
        self.write_spindle()
        self.write_misc()
        self.write('\n')
        if (x != None):
            self.x = x
        if (y != None):
            self.y = y
        if (z != None):
            self.z = z

    def arc_cw(self, x=None, y=None, z=None, i=None, j=None, k=None, r=None):
        self.arc(True, x, y, z, i, j, k, r)

    def arc_ccw(self, x=None, y=None, z=None, i=None, j=None, k=None, r=None):
        self.arc(False, x, y, z, i, j, k, r)

    def dwell(self, t):
        self.write_blocknum()
        self.write_preps()
        self.write(self.DWELL() + (self.TIME() % t))
        self.write_misc()
        self.write('\n')

    def rapid_home(self, x=None, y=None, z=None, a=None, b=None, c=None):
        pass

    def rapid_unhome(self):
        pass

    def set_machine_coordinates(self):
        self.write(self.SPACE() + self.MACHINE_COORDINATES())
        self.prev_g0123 = ''

    ############################################################################
    ##  CRC
    
    def use_CRC(self):
        return self.useCrc

    def CRC_nominal_path(self):
        return self.useCrcCenterline

    def start_CRC(self, left = True, radius = 0.0):
        # set up prep code, to be output on next line
        if self.t == None:
            raise "No tool specified for start_CRC()"
        self.write_blocknum()
        if left:
            self.write(self.SPACE() + 'G41')
        else:
            self.write(self.SPACE() + 'G42')
        self.write((self.SPACE() + 'D%i\n') % self.t)

    def end_CRC(self):
        self.write_blocknum()
        self.write(self.SPACE() + 'G40\n')

    ############################################################################
    ##  Cycles

    def pattern(self):
        pass

    def pocket(self):
        pass

    def profile(self):
        pass

    def boring(self, x=None, y=None, z=None, depth=None, standoff=None, dwell=None, retract_mode=None, spindle_mode=None, clearance_height=None):
    	"""
	The boring routine supports
		G85 - Boring, no dwell, feed out.
		G86 - Boring, spindle stop, rapid out.
		G89 - Boring, with dwell, feed out
    
	The x,y,z values are INITIAL locations (above the hole to be made.  This is in contrast to
	the Z value used in the G8[1-3] cycles where the Z value is that of the BOTTOM of the hole.
	Instead, this routine combines the Z value and the depth value to determine the bottom of
	the hole.
	The standoff value is the distance up from the 'z' value (normally just above the surface) where the bit retracts
	to in order to clear the swarf.  This combines with 'z' to form the 'R' value in the G8[1-3] cycles.
    
	The peck_depth value is the incremental depth (Q value) that tells the peck drilling
	cycle how deep to go on each peck until the full depth is achieved.
   
	NOTE: This routine forces the mode to absolute mode so that the values  passed into
	the G8[1-3] cycles make sense.  I don't know how to find the mode to revert it so I won't
	revert it.  I must set the mode so that I can be sure the values I'm passing in make
	sense to the end-machine.
	"""

	if (standoff == None):        
		# This is a bad thing.  All the boring cycles need a retraction (and starting) height.        
		return

	if (clearance_height == None):
		clearance_height = standoff
           
	if (z == None): 
		return    # We need a Z value as well.  This input parameter represents the top of the hole

	# Set the retraction point to the 'standoff' distance above the starting z height.        
	retract_height = z + standoff

	self.rapid(x=x,y=y)
	self.rapid(z=standoff)
	
	self.write_preps()
	self.write_blocknum()
        
	if (dwell == None) or (dwell == 0):
		# No dwell.

		if (spindle_mode == 0):
			# spindle stop
			if (retract_mode == 0):
				# rapid retraction
				self.write('G86' + self.SPACE())
			else:
				# feed retract
				self.write('G85' + self.SPACE())
		else:
			# spindle run mode.
			if (retract_mode == 0):
				# rapid retraction
				self.write('G85' + self.SPACE())
			else:
				# feed retract
				self.write('G85' + self.SPACE())

	else:
		# With dwell.
		if (spindle_mode == 0):
			# spindle stop

			if (retract_mode == 0):
				# rapid retraction
				self.write('G86' + self.SPACE())
			else:
				# feed retract
				self.write('G89' + self.SPACE())
		else:
			# spindle run mode.

			if (retract_mode == 0):
				# rapid retraction
				self.write('G85' + self.SPACE())
			else:
				# feed retract
				self.write('G89' + self.SPACE())

                self.write( 'P' + (self.fmt.string(dwell)) + self.SPACE())

        if (x != None):        
            dx = x - self.x        
            self.write(self.SPACE() + self.X() + (self.fmt.string(x)))        
            self.x = x 
       
        if (y != None):        
            dy = y - self.y        
            self.write(self.SPACE() + self.Y() + (self.fmt.string(y)))        
            self.y = y
                      
        dz = (z + standoff) - self.z # In the end, we will be standoff distance above the z value passed in.

        if self.drill_modal:
            if z != self.prev_z:
                self.write(self.SPACE() + self.Z() + (self.fmt.string(z - depth)))
                self.prev_z=z
        else:             
            self.write(self.SPACE() + self.Z() + (self.fmt.string(z - depth)))    # This is the 'z' value for the bottom of the hole.
            self.z = (z + standoff)            # We want to remember where z is at the end (at the top of the hole)

        if self.drill_modal:
            if self.prev_retract  != self.RETRACT(self.fmt, retract_height) :
                self.write(self.SPACE() + self.RETRACT(self.fmt, retract_height))               
                self.prev_retract = self.RETRACT(self.fmt, retract_height)
        else:              
            self.write(self.SPACE() + self.RETRACT(self.fmt, retract_height))
           
        self.write_feedrate()
        self.write_spindle()            
        self.write_misc()    
        self.write('\n')
       
	self.rapid(z=clearance_height)



    # The drill routine supports drilling (G81), drilling with dwell (G82) and peck drilling (G83).
    # The x,y,z values are INITIAL locations (above the hole to be made.  This is in contrast to
    # the Z value used in the G8[1-3] cycles where the Z value is that of the BOTTOM of the hole.
    # Instead, this routine combines the Z value and the depth value to determine the bottom of
    # the hole.
    #
    # The standoff value is the distance up from the 'z' value (normally just above the surface) where the bit retracts
    # to in order to clear the swarf.  This combines with 'z' to form the 'R' value in the G8[1-3] cycles.
    #
    # The peck_depth value is the incremental depth (Q value) that tells the peck drilling
    # cycle how deep to go on each peck until the full depth is achieved.
    #
    # NOTE: This routine forces the mode to absolute mode so that the values  passed into
    # the G8[1-3] cycles make sense.  I don't know how to find the mode to revert it so I won't
    # revert it.  I must set the mode so that I can be sure the values I'm passing in make
    # sense to the end-machine.
    #
    def drill(self, x=None, y=None, z=None, depth=None, standoff=None, dwell=None, peck_depth=None, retract_mode=None, spindle_mode=None, clearance_height=None):
        if (standoff == None):        
        # This is a bad thing.  All the drilling cycles need a retraction (and starting) height.        
            return

	if (clearance_height == None):
		clearance_height = standoff
           
        if (z == None): 
            return    # We need a Z value as well.  This input parameter represents the top of the hole

	self.rapid(x=x,y=y)
	self.rapid(z=standoff)
	
        self.write_preps()
        self.write_blocknum()
        
        if (peck_depth != 0):        
            # We're pecking.  Let's find a tree. 
            self.write(self.SPACE() + self.PECK_DRILL() + self.SPACE() + self.PECK_DEPTH(self.fmt, peck_depth))  
        else:        
            # We're either just drilling or drilling with dwell.        
            if (dwell == 0):        
                # We're just drilling. 
                self.write(self.SPACE() + self.DRILL())  
            else:        
                # We're drilling with dwell.

                if self.drill_modal:       
                   self.write(self.SPACE() + self.DRILL_WITH_DWELL(self.FORMAT_DWELL(),dwell))  
    
    # Set the retraction point to the 'standoff' distance above the starting z height.        
        retract_height = z + standoff        
        if (x != None):        
            dx = x - self.x        
            self.write(self.SPACE() + self.X() + (self.fmt.string(x)))        
            self.x = x 
       
        if (y != None):        
            dy = y - self.y        
            self.write(self.SPACE() + self.Y() + (self.fmt.string(y)))        
            self.y = y
                      
        dz = (z + standoff) - self.z # In the end, we will be standoff distance above the z value passed in.

        if self.drill_modal:
            if z != self.prev_z:
                self.write(self.SPACE() + self.Z() + (self.fmt.string(z - depth)))
                self.prev_z=z
        else:             
            self.write(self.SPACE() + self.Z() + (self.fmt.string(z - depth)))    # This is the 'z' value for the bottom of the hole.
            self.z = (z + standoff)            # We want to remember where z is at the end (at the top of the hole)

        if self.drill_modal:
            if self.prev_retract  != self.RETRACT(self.fmt, retract_height) :
                self.write(self.SPACE() + self.RETRACT(self.fmt, retract_height))               
                self.prev_retract = self.RETRACT(self.fmt, retract_height)
        else:              
            self.write(self.SPACE() + self.RETRACT(self.fmt, retract_height))
           
        self.write_feedrate()
        self.write_spindle()            
        self.write_misc()    
        self.write('\n')
       
	self.rapid(z=clearance_height)


 
    # G33.1 tapping with EMC for now
    # unsynchronized (chuck) taps NIY (tap_mode = 1)
    
    def tap(self, x=None, y=None, z=None, zretract=None, depth=None, standoff=None, dwell_bottom=None, pitch=None, stoppos=None, spin_in=None, spin_out=None, tap_mode=None, direction=None, clearance_height=None):
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

	if (clearance_height == None):
		clearance_height = standoff

        self.write_preps()
        self.write_blocknum()				
        self.write_spindle()
        self.write('\n')

        # rapid to starting point; z first, then x,y iff given

        # Set the retraction point to the 'standoff' distance above the starting z height.		
        retract_height = z + standoff

        # unsure if this is needed:
	self.rapid(z = clearance_height)

        # then continue to x,y if given
        if (x != None) or (y != None):
                        self.write_blocknum()				
                        self.write(self.RAPID() )		   

                        if (x != None):		
                                        self.write(self.X() + self.fmt.string(x) + self.SPACE())
                                        self.x = x 

                        if (y != None):		
                                        self.write(self.Y() + self.fmt.string(y) + self.SPACE())		
                                        self.y = y
                        self.write('\n')

	self.rapid(z = retract_height)

        self.write_blocknum()				
        self.write( self.TAP() )
        self.write( self.TAP_DEPTH(self.ffmt,pitch) + self.SPACE() )			
        self.write(self.Z() + self.fmt.string(z - depth))	# This is the 'z' value for the bottom of the tap.
        self.write(self.RETRACT( self.ffmt, retract_height))	# This is the 'z' value at the top of the tap.
        self.write_misc()	
        self.write('\n')

        self.z = retract_height	# this cycle returns to the start position, so remember that as z value
        
    def bore(self, x=None, y=None, z=None, zretract=None, depth=None, standoff=None, dwell_bottom=None, feed_in=None, feed_out=None, stoppos=None, shift_back=None, shift_right=None, backbore=False, stop=False):
        pass

    def end_canned_cycle(self):
        self.write_blocknum()
        self.write(self.SPACE() + self.END_CANNED_CYCLE() + '\n')
        self.prev_drill = ''
        self.prev_g0123 = ''
        self.prev_z = ''   
        self.prev_f = '' 
        self.prev_retract = ''    
    ############################################################################
    ##  Misc

    def comment(self, text):
        self.write((self.COMMENT(text) + '\n'))

    def insert(self, text):
        pass

    def block_delete(self, on=False):        
        pass

    def variable(self, id):
        return (self.VARIABLE() % id)

    def variable_set(self, id, value):
        self.write_blocknum()
        self.write(self.SPACE() + (self.VARIABLE() % id) + self.SPACE() + (self.VARIABLE_SET() % value) + '\n')

    # This routine uses the G92 coordinate system offsets to establish a temporary coordinate
    # system at the machine's current position.  It can then use absolute coordinates relative
    # to this position which makes coding easy.  It then moves to the 'point along edge' which
    # should be above the workpiece but still on one edge.  It then backs off from the edge
    # to the 'retracted point'.  It then plunges down by the depth value specified.  It then
    # probes back towards the 'destination point'.  The probed X,Y location are stored
    # into the 'intersection variable' variables.  Finally the machine moves back to the
    # original location.  This is important so that the results of multiple calls to this
    # routine may be compared meaningfully.
    def probe_single_point(self, \
				point_along_edge_x=None, point_along_edge_y=None, depth=None, \
				retracted_point_x=None, retracted_point_y=None, \
				destination_point_x=None, destination_point_y=None, \
				intersection_variable_x=None, intersection_variable_y=None, \
				probe_offset_x_component=None, probe_offset_y_component=None ):
        self.write_blocknum()
        self.write(self.SPACE() + (self.SET_TEMPORARY_COORDINATE_SYSTEM() + (' X 0 Y 0 Z 0') + ('\t(Temporarily make this the origin)\n')))

        if (self.fhv) : self.calc_feedrate_hv(1, 0)
        self.write_blocknum()
        self.write_feedrate()
        self.write('\t(Set the feed rate for probing)\n')

        self.rapid(point_along_edge_x,point_along_edge_y)
        self.rapid(retracted_point_x,retracted_point_y)
        self.feed(z=depth)

        self.write_blocknum()
        self.write((self.PROBE_TOWARDS_WITH_SIGNAL() + (' X ' + (self.fmt.string(destination_point_x)) + ' Y ' + (self.fmt.string(destination_point_y)) ) + ('\t(Probe towards our destination point)\n')))
        
        self.comment('Back off the workpiece and re-probe more slowly')
        self.write_blocknum();
        self.write(self.RAPID())
        self.write(self.SPACE() + ' X [#5061 - [ 0.5 * [' + probe_offset_x_component + ']]] ')
        self.write(self.SPACE() + ' Y [#5062 - [ 0.5 * [' + probe_offset_y_component + ']]]\n')

        self.write_blocknum()
        self.write(self.FEEDRATE() + self.ffmt.string(self.fh / 2.0) + '\n')

        self.write_blocknum()
        self.write((self.PROBE_TOWARDS_WITH_SIGNAL() + (' X ' + (self.fmt.string(destination_point_x)) + ' Y ' + (self.fmt.string(destination_point_y)) ) + ('\t(Probe towards our destination point)\n')))

        self.write_blocknum()
        self.write(('#' + intersection_variable_x + ' = [ [' + probe_offset_x_component + '] + #5061]\n'))
        self.write_blocknum()
        self.write(('#' + intersection_variable_y + ' = [ [' + probe_offset_y_component + '] + #5062]\n'))

        self.comment('Now move back to the original location')
        self.rapid(retracted_point_x,retracted_point_y)
        self.rapid(z=0)
        self.rapid(point_along_edge_x,point_along_edge_y)
        self.rapid(x=0, y=0)

        self.write_blocknum()
        self.write((self.REMOVE_TEMPORARY_COORDINATE_SYSTEM() + ('\t(Restore the previous coordinate system)\n')))

    def probe_grid(self, x_increment, x_count, y_increment, y_count, z_safety, z_probe, feed_rate, filename):
	pass

    def probe_downward_point(self, depth=None, intersection_variable_z=None, touch_off_as_z=None, rapid_down_to_height=None):
        """
	This routine starts at the machine's current location and probes down (in Z) for the depth specified.
	It stores the RELATIVE coordinate in the variable name specified (as a GCode variable such as #<G54_offset>).
	i.e. since we're using the relative coordinate system (G92), the Z value stored in the #5063 variable
	(in emc2 at least) is in those relative coordinates as opposed to machine or G54 coordinates.

	If the touch_off_as_z variable is given then the current coordinate system (G54, G55 etc.) has its Z offset set
	to the probed point's location as the 'touch_off_as_z' value.  eg: 0.0
	This requires that the rapid_down_to_height value is also given.  We don't really want to rapid to the probed location
	because we don't want to trip the probe during this movement.  As an alternative, we rapid down to this height
	above the probed point and use the G10 L20 command to tell it that we are now at the 'rapid_down_to_height' above
	the zero point for the specified coordinat system.
        """

        self.write_blocknum()
        self.write((self.SET_TEMPORARY_COORDINATE_SYSTEM() + (' X 0 Y 0 Z 0') + ('\t(Temporarily make this the origin)\n')))

        if (self.fhv) : self.calc_feedrate_hv(0.0, math.fabs(self.fv / 5.0))
        self.write_blocknum()
        self.write_feedrate()
        self.write_spindle()
        self.write_misc()
        self.write('\n')

        self.write_blocknum()
        self.write((self.PROBE_TOWARDS_WITH_SIGNAL() + ' Z ' + (self.fmt.string(depth)) + ('\t(Probe towards our destination point)\n')))

        self.comment('Store the probed location somewhere we can get it again later')
        self.write_blocknum()
        self.write(('#' + intersection_variable_z + '= #5063\n'))

        if touch_off_as_z != None and rapid_down_to_height != None:
            self.write_blocknum()
            self.write(self.SPACE() + self.FEED() + self.SPACE() + self.Z() + '[ #' + intersection_variable_z + ' + ' + self.fmt.string(rapid_down_to_height) + ' ]\n')
            self.write_blocknum()
            self.write((self.REMOVE_TEMPORARY_COORDINATE_SYSTEM() + ('\t(Restore the previous coordinate system)\n')))

            self.write_blocknum()
            self.write(self.SPACE() + 'G10 L20 P' + str(self.current_workplane) + self.SPACE() + self.Z() + self.SPACE() + self.fmt.string(rapid_down_to_height + touch_off_as_z) + '\n')
	else:
            self.write_blocknum()
            self.write((self.REMOVE_TEMPORARY_COORDINATE_SYSTEM() + ('\t(Restore the previous coordinate system)\n')))



    def report_probe_results(self, x1=None, y1=None, z1=None, x2=None, y2=None, z2=None, x3=None, y3=None, z3=None, x4=None, y4=None, z4=None, x5=None, y5=None, z5=None, x6=None, y6=None, z6=None, xml_file_name=None ):
        pass

    def open_log_file(self, xml_file_name=None ):
        pass

    def log_coordinate(self, x=None, y=None, z=None):
        pass

    def message(self, text=None):
        pass

    def debug_message(self, message=None):
        pass

    def log_message(self, message=None):
        pass

    def close_log_file(self):
        pass

    # Rapid movement to the midpoint between the two points specified.
    # NOTE: The points are specified either as strings representing numbers or as strings
    # representing variable names.  This allows the HeeksCNC module to determine which
    # variable names are used in these various routines.
    def rapid_to_midpoint(self, x1=None, y1=None, z1=None, x2=None, y2=None, z2=None):
        self.write_blocknum()
        self.write(self.RAPID())
        if ((x1 != None) and (x2 != None)):
            self.write((' X ' + '[[[' + x1 + ' - ' + x2 + '] / 2.0] + ' + x2 + ']'))

        if ((y1 != None) and (y2 != None)):
            self.write((' Y ' + '[[[' + y1 + ' - ' + y2 + '] / 2.0] + ' + y2 + ']'))

        if ((z1 != None) and (z2 != None)):
            self.write((' Z ' + '[[[' + z1 + ' - ' + z2 + '] / 2.0] + ' + z2 + ']'))

        self.write('\n')

    # Rapid movement to the intersection of two lines (in the XY plane only). This routine
    # is based on information found in http://local.wasp.uwa.edu.au/~pbourke/geometry/lineline2d/
    # written by Paul Bourke.  The ua_numerator, ua_denominator, ua and ub parameters
    # represent variable names (with the preceding '#' included in them) for use as temporary
    # variables.  They're specified here simply so that HeeksCNC can manage which variables
    # are used in which GCode calculations.
    #
    # As per the notes on the web page, the ua_denominator and ub_denominator formulae are
    # the same so we don't repeat this.  If the two lines are coincident or parallel then
    # no movement occurs.
    #
    # NOTE: The points are specified either as strings representing numbers or as strings
    # representing variable names.  This allows the HeeksCNC module to determine which
    # variable names are used in these various routines.
    def rapid_to_intersection(self, x1, y1, x2, y2, x3, y3, x4, y4, intersection_x, intersection_y, ua_numerator, ua_denominator, ua, ub_numerator, ub):
        self.comment('Find the intersection of the two lines made up by the four probed points')
        self.write_blocknum();
        self.write(ua_numerator + '=[[[' + x4 + ' - ' + x3 + '] * [' + y1 + ' - ' + y3 + ']] - [[' + y4 + ' - ' + y3 + '] * [' + x1 + ' - ' + x3 + ']]]\n')
        self.write_blocknum();
        self.write(ua_denominator + '=[[[' + y4 + ' - ' + y3 + '] * [' + x2 + ' - ' + x1 + ']] - [[' + x4 + ' - ' + x3 + '] * [' + y2 + ' - ' + y1 + ']]]\n')
        self.write_blocknum();
        self.write(ub_numerator + '=[[[' + x2 + ' - ' + x1 + '] * [' + y1 + ' - ' + y3 + ']] - [[' + y2 + ' - ' + y1 + '] * [' + x1 + ' - ' + x3 + ']]]\n')

        self.comment('If they are not parallel')
        self.write('O900 IF [' + ua_denominator + ' NE 0]\n')
        self.comment('And if they are not coincident')
        self.write('O901    IF [' + ua_numerator + ' NE 0 ]\n')

        self.write_blocknum();
        self.write('       ' + ua + '=[' + ua_numerator + ' / ' + ua_denominator + ']\n')
        self.write_blocknum();
        self.write('       ' + ub + '=[' + ub_numerator + ' / ' + ua_denominator + ']\n') # NOTE: ub denominator is the same as ua denominator
        self.write_blocknum();
        self.write('       ' + intersection_x + '=[' + x1 + ' + [[' + ua + ' * [' + x2 + ' - ' + x1 + ']]]]\n')
        self.write_blocknum();
        self.write('       ' + intersection_y + '=[' + y1 + ' + [[' + ua + ' * [' + y2 + ' - ' + y1 + ']]]]\n')
        self.write_blocknum();
        self.write('       ' + self.RAPID())
        self.write(' X ' + intersection_x + ' Y ' + intersection_y + '\n')

        self.write('O901    ENDIF\n')
        self.write('O900 ENDIF\n')

    # We need to calculate the rotation angle based on the line formed by the
    # x1,y1 and x2,y2 coordinate pair.  With that angle, we need to move
    # x_offset and y_offset distance from the current (0,0,0) position.
    #
    # The x1,y1,x2 and y2 parameters are all variable names that contain the actual
    # values.
    # The x_offset and y_offset are both numeric (floating point) values
    def rapid_to_rotated_coordinate(self, x1, y1, x2, y2, ref_x, ref_y, x_current, y_current, x_final, y_final):
        self.comment('Rapid to rotated coordinate')
        self.write_blocknum();
        self.write( '#1 = [atan[' + y2 + ' - ' + y1 + ']/[' + x2 +' - ' + x1 + ']] (nominal_angle)\n')
        self.write_blocknum();
        self.write( '#2 = [atan[' + ref_y + ']/[' + ref_x + ']] (reference angle)\n')
        self.write_blocknum();
        self.write( '#3 = [#1 - #2] (angle)\n' )
        self.write_blocknum();
        self.write( '#4 = [[[' + (self.fmt.string(0)) + ' - ' + (self.fmt.string(x_current)) + '] * COS[ #3 ]] - [[' + (self.fmt.string(0)) + ' - ' + (self.fmt.string(y_current)) + '] * SIN[ #3 ]]]\n' )
        self.write_blocknum();
        self.write( '#5 = [[[' + (self.fmt.string(0)) + ' - ' + (self.fmt.string(x_current)) + '] * SIN[ #3 ]] + [[' + (self.fmt.string(0)) + ' - ' + (self.fmt.string(y_current)) + '] * COS[ #3 ]]]\n' )

        self.write_blocknum();
        self.write( '#6 = [[' + (self.fmt.string(x_final)) + ' * COS[ #3 ]] - [' + (self.fmt.string(y_final)) + ' * SIN[ #3 ]]]\n' )
        self.write_blocknum();
        self.write( '#7 = [[' + (self.fmt.string(y_final)) + ' * SIN[ #3 ]] + [' + (self.fmt.string(y_final)) + ' * COS[ #3 ]]]\n' )

        self.write_blocknum();
        self.write( self.RAPID() + ' X [ #4 + #6 ] Y [ #5 + #7 ]\n' )

    def BEST_POSSIBLE_SPEED(self, motion_blending_tolerance, naive_cam_tolerance): 
	    statement = 'G64'

	    if (motion_blending_tolerance > 0):
		    statement += ' P ' + str(motion_blending_tolerance)

	    if (naive_cam_tolerance > 0):
		    statement += ' Q ' + str(naive_cam_tolerance)

	    return(statement)
            
    def set_path_control_mode(self, mode, motion_blending_tolerance, naive_cam_tolerance ):
        self.write_blocknum()
        if (mode == 0):
            self.write( self.EXACT_PATH_MODE() + '\n' )
        if (mode == 1):
            self.write( self.EXACT_STOP_MODE() + '\n' )
        if (mode == 2):
            self.write( self.BEST_POSSIBLE_SPEED( motion_blending_tolerance, naive_cam_tolerance ) + '\n' )
        

################################################################################

nc.creator = Creator()
