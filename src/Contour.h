
#ifndef CONTOUR_CYCLE_CLASS_DEFINITION
#define CONTOUR_CYCLE_CLASS_DEFINITION

// Contour.h
/*
 * Copyright (c) 2009, Dan Heeks, Perttu Ahola
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include "DepthOp.h"
#include "HeeksCNCTypes.h"
#include <list>
#include <vector>
#include "CNCPoint.h"
#include <TopoDS_Wire.hxx>
#include <TopoDS_Edge.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <Bnd_Box.hxx>
#include "CDouble.h"

class CContour;

class CContourParams{
public:
	typedef enum {
		eRightOrInside = -1,
		eOn = 0,
		eLeftOrOutside = +1
	}eSide;
	eSide m_tool_on_side;

	typedef enum
	{
	    ePlunge = 0,
	    eRamp
    } EntryMove_t;
    EntryMove_t m_entry_move_type;

public:
	CContourParams()
	{
		m_tool_on_side = eOn;
		m_entry_move_type = ePlunge;
		ReadDefaultValues();
	}

	void WriteDefaultValues();
    void ReadDefaultValues();
	void GetProperties(CContour* parent, std::list<Property *> *list);
	void WriteXMLAttributes(TiXmlNode* pElem);
	void ReadParametersFromXMLElement(TiXmlElement* pElem);

	const wxString ConfigPrefix(void)const{return _T("Contour");}

	bool operator== ( const CContourParams & rhs ) const
	{
		return(m_tool_on_side == rhs.m_tool_on_side);
	}

	bool operator!= ( const CContourParams & rhs ) const
	{
		return(! (*this == rhs));	// Call the equivalence operator.
	}

    friend wxString & operator<< ( wxString & str, CContourParams::EntryMove_t & move )
    {
        switch(move)
        {
            case CContourParams::ePlunge:
                str << _("Plunge");
                break;

            case CContourParams::eRamp:
                str << _("Ramp");
                break;
        }

        return(str);
    }
};


/**
	The CContour class is suspiciously similar to the CProfile class.  The main difference is that the NC path
	is generated from this class itself (in C++) rather than by using the kurve Pythin library.  It also
	has depth values that are RELATIVE to the sketch coordinate rather than assuming a horozontal sketch.  The idea
	of this is to allow for a rotation of the XZ or YZ planes.

	Finally, it is hoped that the CContour class will support 'bridging tabs' during a cutout so that the
	workpiece is held in place until the very last moment.

	This class uses the offet functionality for TopoDS_Wire objects to handle the path generation. This is DIFFERENT
	to that used by the Profile class in that it doesn't handle the case where the user wants to machine inside a
	sketch but where the diamter of the  tool makes that impossible.  With the Profile class, this would be
	possible for a converging sketch shape such that the tool would penetrate as far as it could without gouging
	the sketch but would not cut out the whole sketch shape.  This class allows the FAILURE to occur rather than
	allowing half the sketch to be machined.  At the initial time of writing, I consider this to be a GOOD thing.
	I wish to do some 'inlay' work and I want to know whether the  tools will COMPLETELY cut out the sketch
	shapes.  Perhaps we will add a flag to enable/disable this behaviour later.
 */

class CContour: public CDepthOp {
public:
	/**
		Define some data structures to hold references to CAD elements.
 	 */
	typedef int SymbolType_t;
	typedef unsigned int SymbolId_t;
	typedef std::pair< SymbolType_t, SymbolId_t > Symbol_t;
	typedef std::list< Symbol_t > Symbols_t;

public:


	/**
		Container for a single TopoDS_Edge along with all its various parameters.
		NOTE: The OpenCascade library uses the terms FirstParameter() and LastParameter()
		to define a value that is representative of the first end of the edge and the last
		end of the edge.  This class uses the terms StartParameter() and StartPoint() etc. to
		indicate the starting point in terms of movement along the edge.  This distinction
		is the based on the m_is_forwards boolean.  We may want to 'reverse' this edge
		in terms of tool path.  This is simply a reversal of the m_is_forwards boolean
		which allows us to select either the FirstParameter or the LastParameter as the
		StartParameter as appropriate.  I know the terminology is a little confusing but
		part of it comes from OpenCascade and the other part just makes sense in terms
		of tool path progression.
	 */
	class Path
	{
	public:
		Path(const TopoDS_Edge edge): m_edge(edge), m_curve(m_edge), m_is_forwards(true)
		{
			m_length = GCPnts_AbscissaPoint::Length( m_curve );
			m_tolerance = heeksCAD->GetTolerance();
		}

		Path & operator= ( const Path & rhs )
		{
			if (this != &rhs)
			{
				m_edge = rhs.m_edge;
				m_curve = BRepAdaptor_Curve(m_edge);
				m_length = rhs.m_length;
				m_is_forwards = rhs.m_is_forwards;
				m_tolerance = rhs.m_tolerance;
			}

			return(*this);
		}

		Path( const Path & rhs )
		{
			*this = rhs;	// call the assignment operator.
		}

		TopoDS_Edge Edge() const { return(m_edge); }

		Standard_Real StartParameter() const;
		Standard_Real EndParameter() const;

		Standard_Real Proportion( const double proportion ) const;

		CNCPoint PointAt(const Standard_Real U) const
		{
			gp_Pnt point;
			m_curve.D0(U,point);
			return(point);
		}

		CNCPoint StartPoint() const
		{
			gp_Pnt point;
			m_curve.D0(StartParameter(),point);
			return(point);
		}

		CNCPoint EndPoint() const
		{
			gp_Pnt point;
			m_curve.D0(EndParameter(),point);
			return(point);
		}

		gp_Vec StartVector() const
		{
			gp_Vec vec;
			gp_Pnt point;
			m_curve.D1(StartParameter(), point, vec);
			if (! m_is_forwards) vec.Reverse();
			return(vec);
		}

		gp_Vec EndVector() const
		{
			gp_Vec vec;
			gp_Pnt point;
			m_curve.D1(EndParameter(), point, vec);
			if (! m_is_forwards) vec.Reverse();
			return(vec);
		}

		Standard_Real Length() const
		{
			return(m_length);
        }

		void Reverse() 
		{
			m_is_forwards = ! m_is_forwards;
			m_edge.Reverse();
			m_curve = BRepAdaptor_Curve(m_edge);
		}

		Python GCode( CMachineState *pMachineState, const double end_z );
		Python GCode( Standard_Real start_u, Standard_Real end_u, CMachineState *pMachineState, const double end_z );
		HeeksObj *Sketch();
		std::list<CNCPoint> Intersect( const Path & rhs ) const;
		Standard_Real Tolerance() const { return(m_tolerance); }

	private:
		TopoDS_Edge m_edge;
		BRepAdaptor_Curve m_curve;
		bool		m_is_forwards;
		Standard_Real m_length;
		Standard_Real m_tolerance;

	}; // End Path class definition

	/**
		This is a container of multiple Path objects that are both ordered and contiguous (connected)
		It is similar to the Paths (plural) class except that the Paths class can hold a list of
		ContiguousPath objects.  It can also merge ContiguousPath objects if the addition of an edge
		marries two ContiguousPath objects together.
	 */
	class ContiguousPath
	{
	public:
		ContiguousPath() { m_is_forwards = true; }
		bool Add(Path path);
		Standard_Real Length() const;
		void Reverse();
        bool Periodic() const;
		std::vector<Path>::iterator Next(std::vector<Path>::iterator itPath);
		CNCPoint StartPoint() const;
		CNCPoint EndPoint() const;
		bool IsInside( const CNCPoint point ) const;
		bool IsInside( const ContiguousPath & rhs ) const;
		TopoDS_Wire Wire() const;

		ContiguousPath & operator+=( ContiguousPath &rhs );

		Python GCode(	CMachineState *pMachineState,       // for both fixture and last_position.
						const double clearance_height,
						const double rapid_down_to_height,
						const double start_depth,
						const CContourParams::EntryMove_t entry_move_type );

		bool SetStartPoint( CNCPoint location );

		Python Ramp( std::vector<Path>::iterator itPath, CMachineState *pMachineState, const double end_z );
		HeeksObj *Sketch();

		Bnd_Box BoundingBox() const;
		bool IsClockwise();

	private:
		bool m_is_forwards;
		std::vector<Path> m_paths;
		int m_concentricity;	// more positive the value indicates an outer (encompasing) polygon.
	}; // End of ContiguousPath class definition.

	class Paths
	{
	public:
		Paths() { }

		void Reverse()
		{
			m_is_forwards = ! m_is_forwards;
			for (std::vector<ContiguousPath>::iterator itPath = m_contiguous_paths.begin(); itPath != m_contiguous_paths.end(); itPath++)
			{
				itPath->Reverse();
			}
		}

		::size_t Number() const { return(m_contiguous_paths.size()); }
		Path Next() const;
		void Add(const Path path);
		void Add(const TopoDS_Edge edge);
		void Add(const TopoDS_Wire wire);
		bool Join( std::vector<ContiguousPath>::iterator lhs, std::vector<ContiguousPath>::iterator rhs );
		Python GCode(	CMachineState *pMachineState,       // for both fixture and last_position.
						const double clearance_height,
						const double rapid_down_to_height,
						const double start_depth,
						const CContourParams::EntryMove_t entry_move_type );

        bool GenerateSketches();

	private:
		std::vector<ContiguousPath> m_contiguous_paths;
		bool		m_is_forwards;
	}; // End Paths class definition.

public:
	//	These are references to the CAD elements whose position indicate where the Drilling Cycle begins.
	//	If the m_params.m_sort_drilling_locations is false then the order of symbols in this list should
	//	be respected when generating GCode.  We will, eventually, allow a user to sort the sub-elements
	//	visually from within the main user interface.  When this occurs, the change in order should be
	//	reflected in the ordering of symbols in the m_symbols list.

	Symbols_t m_symbols;
	CContourParams m_params;
	static double max_deviation_for_spline_to_arc;

	//	Constructors.
	CContour():CDepthOp(GetTypeString(), 0, ContourType)
	{
	    ReadDefaultValues();
	}

	CContour(	const Symbols_t &symbols,
			const int tool_number )
		: CDepthOp(GetTypeString(), NULL, tool_number, ContourType), m_symbols(symbols)
	{
	    ReadDefaultValues();
		ReloadPointers();
	}

	CContour( const CContour & rhs );
	CContour & operator= ( const CContour & rhs );

	// HeeksObj's virtual functions
	int GetType()const{return ContourType;}
	const wxChar* GetTypeString(void)const{return _T("Contour");}
	void glCommands(bool select, bool marked, bool no_color);

	const wxBitmap &GetIcon();
	void GetProperties(std::list<Property *> *list);
	HeeksObj *MakeACopy(void)const;
	void CopyFrom(const HeeksObj* object);
	void WriteXML(TiXmlNode *root);
	bool CanAddTo(HeeksObj* owner);
	bool CanAdd(HeeksObj *object);
	void GetTools(std::list<Tool*>* t_list, const wxPoint* p);

	void WriteDefaultValues();
    void ReadDefaultValues();

	bool operator== ( const CContour & rhs ) const;
	bool operator!= ( const CContour & rhs ) const { return(! (*this == rhs)); }
	bool IsDifferent(HeeksObj* other);

	// This is the method that gets called when the operator hits the 'Python' button.  It generates a Python
	// program whose job is to generate RS-274 GCode.
	Python AppendTextToProgram( CMachineState *pMachineState );

	static HeeksObj* ReadFromXMLElement(TiXmlElement* pElem);

	void AddSymbol( const SymbolType_t type, const SymbolId_t id ) { m_symbols.push_back( Symbol_t( type, id ) ); }

	std::list<wxString> DesignRulesAdjustment(const bool apply_changes);

	static Python GCode( 	const TopoDS_Wire & wire,
							CMachineState *pMachineState,
							const double clearance_height,
							const double rapid_down_to_height,
							const double start_depth,
							const CContourParams::EntryMove_t entry_move_type );

	static bool Clockwise( const gp_Circ & circle );
	void ReloadPointers();
	static void GetOptions(std::list<Property *> *list);
};


#endif

