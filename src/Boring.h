
#ifndef BORING_CYCLE_CLASS_DEFINITION
#define BORING_CYCLE_CLASS_DEFINITION

// Boring.h
/*
 * Copyright (c) 2009, Dan Heeks, Perttu Ahola
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include "SpeedOp.h"
#include "HeeksCNCTypes.h"
#include <list>
#include <vector>
#include "CNCPoint.h"

class CBoring;

class CBoringParams
{
public:
	typedef enum
	{
		eRapidRetract = 0,
		eFeedRetract
	} eRetractMode_t;

	friend wxString & operator << ( wxString & ss, const eRetractMode_t & retract_mode )
	{
		switch (retract_mode)
		{
		case eFeedRetract:		ss << _("Feed Retraction");
			break;

		case eRapidRetract:	ss << _("Rapid Retraction");
			break;
		} // End switch()

		return(ss);
	}

	friend Python & operator << ( Python & ss, const eRetractMode_t & retract_mode )
	{
		wxString value;
		value << retract_mode;
		ss << value;

		return(ss);
	}

	typedef enum
	{
		eSpindleStop = 0,
		eSpindleRun
	} eSpindleMode_t;

	friend wxString & operator << ( wxString & ss, const eSpindleMode_t & spindle_mode )
	{
		switch (spindle_mode)
		{
		case eSpindleStop:		ss << _("Spindle Stop Mode");
			break;

		case eSpindleRun:	ss << _("Spindle Run Mode");
			break;
		} // End switch()

		return(ss);
	}

	friend Python & operator << ( Python & ss, const eSpindleMode_t & spindle_mode )
	{
		wxString value;
		value << spindle_mode;
		ss << value;

		return(ss);
	}

public:
	double m_standoff;		// This is the height above the staring Z position that forms the Z retract height (R word)
	double m_dwell;			// If dwell_bottom is non-zero then we're using the G82 drill cycle rather than G83 peck drill cycle.  This is the 'P' word
	double m_depth;			// Incremental length down from 'z' value at which the bottom of the hole can be found
	double m_diameter;
	int    m_sort_boring_locations;	// Perform a location-based sort before generating GCode?
	eRetractMode_t    m_retract_mode;	// boring - 0 - rapid retract, 1 - feed retract
	eSpindleMode_t    m_spindle_mode;	// boring - if true, stop spindle at bottom

private:
	double m_clearance_height; // The tool moves to this height between drill locations and then rapidly moves down to the m_standoff height.

public:
	double ClearanceHeight() const;
	void   ClearanceHeight(const double value) { m_clearance_height = value; }

	// The following line is the prototype setup in the Python routines for the drill sequence.
	// depending on parameter combinations the backend should emit proper bore cycles (EMC2:  G85, G86, G89)
	//
	// def drill(x=None, y=None, z=None, depth=None, standoff=None, dwell=None, peck_depth=None, retract_mode=None, spindle_mode=None):


	void set_initial_values( const double depth, const int tool_number );
	void write_values_to_config();
	void GetProperties(CBoring* parent, std::list<Property *> *list);
	void WriteXMLAttributes(TiXmlNode* pElem);
	void ReadParametersFromXMLElement(TiXmlElement* pElem);

	const wxString ConfigScope(void)const{return _T("Boring");}

	bool operator== ( const CBoringParams & rhs ) const;
	bool operator!= ( const CBoringParams & rhs ) const { return(! (*this == rhs)); }
};


class CBoring: public CSpeedOp {
public:
	/**
		The following two methods are just to draw pretty lines on the screen to represent boring
		cycle activity when the operator selects the Boring Cycle operation in the data list.
	 */
	std::list< CNCPoint > PointsAround( const CNCPoint & origin, const double radius, const unsigned int numPoints ) const;
	std::list< CNCPoint > DrillBitVertices( const CNCPoint & origin, const double radius, const double length ) const;


public:
	CBoringParams m_params;

	//	Constructors.
	CBoring():CSpeedOp(GetTypeString(), 0){}
	CBoring(	std::list<HeeksObj *> reference_objects,
			const int tool_number,
			const double depth );

	CBoring( const CBoring & rhs );
	CBoring & operator= ( const CBoring & rhs );

	// HeeksObj's virtual functions
	virtual int GetType() const {return BoringType;}
	const wxChar* GetTypeString(void)const{return _T("Boring");}
	void glCommands(bool select, bool marked, bool no_color);

	const wxBitmap &GetIcon();
	void GetProperties(std::list<Property *> *list);
	HeeksObj *MakeACopy(void)const;
	void CopyFrom(const HeeksObj* object);
	void WriteXML(TiXmlNode *root);
	bool CanAddTo(HeeksObj* owner);
	bool CanAdd(HeeksObj* object);
	void GetTools(std::list<Tool*>* t_list, const wxPoint* p);
	void ReloadPointers();

	// This is the method that gets called when the operator hits the 'Python' button.  It generates a Python
	// program whose job is to generate RS-274 GCode.
	Python AppendTextToProgram( CMachineState *pMachineState );

	static HeeksObj* ReadFromXMLElement(TiXmlElement* pElem);

	std::list<wxString> DesignRulesAdjustment(const bool apply_changes);
	static bool ValidType( const int object_type );

	bool operator==( const CBoring & rhs ) const;
	bool operator!=( const CBoring & rhs ) const { return(! (*this == rhs)); }
	bool IsDifferent( HeeksObj *other ) { return( *this != (*(CBoring *)other) ); }

	/* virtual */ void OnSetTool(const COp::ToolNumber_t new_tool_number); // from COp class.

	// virtual functions from CSpeedOp class
	/* virtual */ void ResetSpeeds(const int tool_number, const double bored_hole_diameter);
	/* virtual */ void ResetFeeds(const int tool_number);
};




#endif // BORING_CYCLE_CLASS_DEFINITION
