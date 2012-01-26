// Inlay.h
/*
 * Copyright (c) 2009, Dan Heeks, Perttu Ahola
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#ifndef INLAY_CYCLE_CLASS_DEFINITION
#define INLAY_CYCLE_CLASS_DEFINITION


#include "DepthOp.h"
#include "HeeksCNCTypes.h"
#include "CTool.h"

#include <list>
#include <vector>
#include <map>
#include "CNCPoint.h"
#include <TopoDS_Wire.hxx>
#include <TopoDS_Edge.hxx>


class CInlay;

class CInlayParams{
public:
    typedef enum {
		eFemale,		// No mirroring and take depths from DepthOp settings.
		eMale,			// Reverse depth values (bottom up measurement) and mirror the sketches.
		eBoth
	} eInlayPass_t;

	typedef enum {
	    eXAxis = 0,
	    eYAxis
	} eAxis_t;

public:
	CInlayParams()
	{
		m_border_width = 25.4; // 1 inch.
		m_clearance_tool = 0;
		m_pass = eBoth;
		m_female_before_male_fixtures = true;
		m_min_cornering_angle = 30.0;   // degrees.
		m_cleanout_step_over = 0.3;   // for the cleanout of the gap between the chamfering bit and endmill areas (in the corners)
	}

	void set_initial_values();
	void write_values_to_config();
	void GetProperties(CInlay* parent, std::list<Property *> *list);
	void WriteXMLAttributes(TiXmlNode* pElem);
	void ReadParametersFromXMLElement(TiXmlElement* pElem);

	const wxString ConfigPrefix(void)const{return _T("Inlay");}

	double m_border_width;
	CTool::ToolNumber_t  m_clearance_tool;
	eInlayPass_t    m_pass;
	eAxis_t         m_mirror_axis;
	bool			m_female_before_male_fixtures;
	double          m_min_cornering_angle;
	double          m_cleanout_step_over;

	bool operator== ( const CInlayParams & rhs ) const;
	bool operator!= ( const CInlayParams & rhs ) const { return(! (*this == rhs)); }
};

/**
	The Inlay operation generates the tool paths so that a combination of chamfering bits and
	normal endmills can be used to machine away two mirrored patterns, presumably out of two
	different coloured woods.  These two mirrored patterns can then be glued together, one fitting
	into the other.  When the glue is dry, one half is, almost, machined away leaving the
	other half with its valleys filled in with the contrasting wood.

	i.e. draw a shape.  machine a valley inside that shape on one workpiece.  machine a mountain
	around a mirrored copy of that shape.  fit the two workpieces into each other.   machine away
	the backing that remains from the male half leaving the female half (the valleys) filled with
	the wood from the male half (the mountains).

	The other trick is to use a pointed chamfering bit to machine the walls at the edges of both the
	valleys and the mountains.  By using the same bit for both halves, the two patterns should fit
	into each other correctly.  Indeed, since it's a chamfering bit, the pressure applied when
	gluing them together should ensure a tight fit between the edges of the two halves.  The other
	feature of using a chamfering bit is to 'sharpen up' the concave corners of the shape by lifting
	the chamfering bit up into the corners.  i.e. as the bit is lifted, its effective diameter
	approaches zero.  This works around the need to have the shape modified to remove corners
	whose radius is less than the clearance tool's radius.

	This class supports the use of two private fixtures so that both the female and male halves of the
	machining can be done on different fixtures at the same time.  i.e. to minimize tool changes.  The
	idea is to fix a dark coloured wood as G54 and a light coloured wood as G55 and machine both halves
	of the design in the same machining session.

	NOTE: There currently exists a flaw in the logic.  The radius of the clearance bit (i.e. the endmill)
	means that it cannot fit tightly into concave corners of the sketch.  The chamfering bit, on the
	other hand can get right into the corners.  There is currently no mechanism to determine what wood
	remains unmachined by the two operations.  i.e. near the concave corners, the chamfering bit does the
	very corners and the clearance bit does most of the model but there is a small area between the
	two that is not machined.  One day it would be nice to figure out how to calculate these areas and
	have the chamfering bit machine them but I don't know how to do that yet.
 */

class CInlay: public CDepthOp {
private:
	class CDouble
	{
	public:
		CDouble(const double value)
		{
			m_value = value;
		}

		~CDouble() { }

		CDouble( const CDouble & rhs )
		{
			*this = rhs;
		}

		CDouble & operator= ( const CDouble & rhs )
		{
			if (this != &rhs)
			{
				m_value = rhs.m_value;
			}

			return(*this);
		}

		bool operator==( const CDouble & rhs ) const
		{
			if (fabs(m_value - rhs.m_value) < (2.0 * heeksCAD->GetTolerance())) return(true);
			return(false);
		}

		bool operator< (const CDouble & rhs ) const
		{
			if (*this == rhs) return(false);
			return(m_value < rhs.m_value);
		}

		bool operator<= (const CDouble & rhs ) const
		{
			if (*this == rhs) return(true);
			return(m_value < rhs.m_value);
		}

		bool operator> (const CDouble & rhs ) const
		{
			if (*this == rhs) return(false);
			return(m_value > rhs.m_value);
		}

	private:
		double	m_value;
	};

private:
    class Path
    {
    public:
        Path()
        {
            m_depth = 0.0;
            m_offset = 0.0;
        }

        ~Path() { }

        Path & operator= ( const Path & rhs )
        {
            if (this != &rhs)
            {
                m_depth = rhs.m_depth;
                m_offset = rhs.m_offset;
                m_wire = rhs.m_wire;
            }

            return(*this);
        }

        Path( const Path & rhs )
        {
            // Call the assignment operato
            *this = rhs;
        }

        bool operator== ( const Path & rhs ) const
        {
            double tolerance = heeksCAD->GetTolerance();
            if (fabs(m_depth - rhs.m_depth) > tolerance) return(false);
            if (fabs(m_offset - rhs.m_offset) > tolerance) return(false);
            return(true);
        }

        bool operator< ( const Path & rhs ) const
        {
            double tolerance = heeksCAD->GetTolerance();
            if (m_depth > rhs.m_depth) return(false);
            if (fabs(m_depth - rhs.m_depth) < tolerance) return(false);
            if (m_offset > rhs.m_offset) return(false);
            if (fabs(m_offset - rhs.m_offset) < tolerance) return(false);
            return(true);
        }

        double Depth() const { return(m_depth); }
        void Depth(const double value) { m_depth = value; }

        double Offset() const { return(m_offset); }
        void Offset(const double value) { m_offset = value; }

        TopoDS_Wire Wire() const { return(m_wire); }
        void Wire(const TopoDS_Wire wire) { m_wire = wire; }

    private:
        double m_depth;
        double m_offset;
        TopoDS_Wire m_wire;
    }; // End Path class definition

public:
	/**
		Define some data structures to hold references to CAD elements.
 	 */
	typedef int SymbolType_t;
	typedef unsigned int SymbolId_t;
	typedef std::pair< SymbolType_t, SymbolId_t > Symbol_t;
	typedef std::list< Symbol_t > Symbols_t;

	typedef std::list<Path> Valley_t;   // All for the same sketch.
	typedef std::list< Valley_t > Valleys_t;

	typedef std::map<CNCPoint, std::set<CNCVector> > Corners_t;

public:
	//	These are references to the CAD elements whose position indicate where the Drilling Cycle begins.
	//	If the m_params.m_sort_drilling_locations is false then the order of symbols in this list should
	//	be respected when generating GCode.  We will, eventually, allow a user to sort the sub-elements
	//	visually from within the main user interface.  When this occurs, the change in order should be
	//	reflected in the ordering of symbols in the m_symbols list.

	Symbols_t m_symbols;
	CInlayParams m_params;
	static double max_deviation_for_spline_to_arc;

	//	Constructors.
	CInlay():CDepthOp(GetTypeString(), 0, InlayType)
	{
		m_params.set_initial_values();
	}
	CInlay(	const Symbols_t &symbols,
			const int tool_number )
		: CDepthOp(GetTypeString(), NULL, tool_number, InlayType), m_symbols(symbols)
	{
		m_params.set_initial_values();
		ReloadPointers();
		if (CTool::Find(tool_number))
		{
		    CTool *pChamferingBit = CTool::Find(tool_number);
		    double theta = pChamferingBit->m_params.m_cutting_edge_angle / 360.0 * 2.0 * PI;
		    double radius = pChamferingBit->m_params.m_diameter / 2.0;
		    m_depth_op_params.m_step_down = radius / tan(theta);
		}

	}

	CInlay( const CInlay & rhs );
	CInlay & operator= ( const CInlay & rhs );

	bool operator==( const CInlay & rhs ) const;
	bool operator!= ( const CInlay & rhs ) const { return(! (*this == rhs)); }

	bool IsDifferent( HeeksObj *other ) { return(*this != (*(CInlay *)other)); }

	// HeeksObj's virtual functions
	int GetType()const{return InlayType;}
	const wxChar* GetTypeString(void)const{return _T("Inlay");}
	void glCommands(bool select, bool marked, bool no_color);

	const wxBitmap &GetIcon();
	void GetProperties(std::list<Property *> *list);
	HeeksObj *MakeACopy(void)const;
	void CopyFrom(const HeeksObj* object);
	void WriteXML(TiXmlNode *root);
	bool CanAddTo(HeeksObj* owner);
	bool CanAdd(HeeksObj *object);
	void GetTools(std::list<Tool*>* t_list, const wxPoint* p);

	// This is the method that gets called when the operator hits the 'Python' button.  It generates a Python
	// program whose job is to generate RS-274 GCode.
	Python AppendTextToProgram( CMachineState *pMachineState );

	static HeeksObj* ReadFromXMLElement(TiXmlElement* pElem);

	void AddSymbol( const SymbolType_t type, const SymbolId_t id ) { m_symbols.push_back( Symbol_t( type, id ) ); }

	std::list<wxString> DesignRulesAdjustment(const bool apply_changes);

	static wxString GeneratePathFromWire( 	const TopoDS_Wire & wire,
											CNCPoint & last_position,
											const CFixture fixture,
											const double clearance_height,
											const double rapid_down_to_height );

	static bool Clockwise( const gp_Circ & circle );
	void ReloadPointers();
	static void GetOptions(std::list<Property *> *list);

	static std::vector<TopoDS_Edge> SortEdges( const TopoDS_Wire & wire );
	static bool DirectionTowarardsNextEdge( const TopoDS_Edge &from, const TopoDS_Edge &to );
	static double FindMaxOffset( const double max_offset_required, TopoDS_Wire wire, const double tolerance );
	Python FormCorners( Valley_t & paths, CMachineState *pMachineState ) const;
	Corners_t FindSimilarCorners( const CNCPoint coordinate, Corners_t corners, const CTool *pChamferingBit ) const;
	double CornerAngle( const std::set<CNCVector> _vectors ) const;

	Valleys_t DefineValleys(CMachineState *pMachineState);
	Valleys_t DefineMountains(CMachineState *pMachineState);

	Python FormValleyWalls( Valleys_t valleys, CMachineState *pMachineState  );
	Python FormValleyPockets( Valleys_t valleys, CMachineState *pMachineState  );
	Python FormMountainWalls( Valleys_t mountains, CMachineState *pMachineState  );
	Python FormMountainPockets( Valleys_t mountains, CMachineState *pMachineState, const bool only_above_mountains  );
	Python FormValleyCrevices( Valleys_t valleys, CMachineState *pMachineState  );
	Python FormMountainCrevices( Valleys_t valleys, CMachineState *pMachineState  );
	bool CutterBoundary( TopoDS_Wire original, const double cutter_radius, TopoDS_Wire &result  ) const;
	std::vector<TopoDS_Wire> FaceMinusFace( const TopoDS_Face lhs, const TopoDS_Face rhs, const double z_height ) const;
	void AddShapeToBoundingBox( TopoDS_Shape shape, CBox & box ) const;
	CBox GetBoundingBoxForMountains(Valleys_t valleys, CMachineState *pMachineState);
	bool DistanceBetweenWires( const TopoDS_Wire lhs, const TopoDS_Wire rhs, double *pResult ) const;

	// Overloaded from COp class.
	virtual unsigned int MaxNumberOfPrivateFixtures() const { return(2); }

	Python SelectFixture( CMachineState *pMachineState, const bool female_half );
	bool CornerNeedsSharpenning(Corners_t::iterator itCorner) const;

	bool DeterminePocketArea(HeeksObj* sketch, CMachineState *pMachineState, TopoDS_Wire *pPocketArea);

public:
	static gp_Pnt GetStart(const TopoDS_Edge &edge);
    static gp_Pnt GetEnd(const TopoDS_Edge &edge);
};




#endif // INLAY_CYCLE_CLASS_DEFINITION


