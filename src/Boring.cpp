// Boring.cpp
/*
 * Copyright (c) 2009, Dan Heeks, Perttu Ahola
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include "stdafx.h"
#include "Boring.h"
#include "CNCConfig.h"
#include "ProgramCanvas.h"
#include "interface/HeeksObj.h"
#include "interface/PropertyInt.h"
#include "interface/PropertyDouble.h"
#include "interface/PropertyLength.h"
#include "interface/PropertyChoice.h"
#include "interface/PropertyString.h"
#include "tinyxml/tinyxml.h"
#include "Operations.h"
#include "CTool.h"
#include "Profile.h"
#include "Fixture.h"
#include "Fixtures.h"
#include "CNCPoint.h"
#include "MachineState.h"
#include "Program.h"
#include "CounterBore.h"
#include "Tools.h"

#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>

extern CHeeksCADInterface* heeksCAD;


void CBoringParams::set_initial_values( const double depth, const int tool_number )
{
	CNCConfig config(ConfigScope());

	config.Read(_T("m_standoff"), &m_standoff, (25.4 / 4));	// Quarter of an inch
	config.Read(_T("m_dwell"), &m_dwell, 1);
	config.Read(_T("m_depth"), &m_depth, 25.4);		// One inch
	config.Read(_T("m_sort_boring_locations"), &m_sort_boring_locations, 1);
	config.Read(_T("m_retract_mode"), (int *) &m_retract_mode, int(eRapidRetract));
	config.Read(_T("m_spindle_mode"), (int *) &m_spindle_mode, int(eSpindleStop));
	config.Read(_T("m_clearance_height"), &m_clearance_height, 25.4);		// One inch
	config.Read(_T("m_diameter"), &m_diameter, 30.0);

	if (depth > 0)
	{
		// We've found the depth we want used.  Assign it now.
		m_depth = depth;
	} // End if - then
}

void CBoringParams::write_values_to_config()
{
	// We always want to store the parameters in mm and convert them back later on.

	CNCConfig config(ConfigScope());

	// These values are in mm.
	config.Write(_T("m_standoff"), m_standoff);
	config.Write(_T("m_dwell"), m_dwell);
	config.Write(_T("m_depth"), m_depth);
	config.Write(_T("m_sort_boring_locations"), m_sort_boring_locations);
	config.Write(_T("m_retract_mode"), m_retract_mode);
	config.Write(_T("m_spindle_mode"), m_spindle_mode);
	config.Write(_T("m_clearance_height"), m_clearance_height);
	config.Write(_T("m_diameter"), m_diameter);

}


static void on_set_spindle_mode(int value, HeeksObj* object)
{
	((CBoring*)object)->m_params.m_spindle_mode = CBoringParams::eSpindleMode_t(value);
	((CBoring*)object)->m_params.write_values_to_config();
}

static void on_set_retract_mode(int value, HeeksObj* object)
{
	((CBoring*)object)->m_params.m_retract_mode = CBoringParams::eRetractMode_t(value);
	((CBoring*)object)->m_params.write_values_to_config();
}

static void on_set_standoff(double value, HeeksObj* object)
{
	((CBoring*)object)->m_params.m_standoff = value;
	((CBoring*)object)->m_params.write_values_to_config();
}

static void on_set_dwell(double value, HeeksObj* object)
{
	((CBoring*)object)->m_params.m_dwell = value;
	((CBoring*)object)->m_params.write_values_to_config();
}

static void on_set_diameter(double value, HeeksObj* object)
{
	((CBoring*)object)->m_params.m_diameter = value;
	((CBoring*)object)->m_params.write_values_to_config();
}

static void on_set_depth(double value, HeeksObj* object)
{
    if (value <= 0)
    {
        wxMessageBox(_("Depth must be a positive number as it is relative to the starting point's position"));
    }
    else
    {
        ((CBoring*)object)->m_params.m_depth = value;
        ((CBoring*)object)->m_params.write_values_to_config();
    }
}

static void on_set_sort_boring_locations(int value, HeeksObj* object)
{
	((CBoring*)object)->m_params.m_sort_boring_locations = value;
	((CBoring*)object)->m_params.write_values_to_config();
}

static void on_set_clearance_height(double value, HeeksObj* object)
{
	((CBoring*)object)->m_params.ClearanceHeight( value );
	((CBoring*)object)->m_params.write_values_to_config();
}

void CBoringParams::GetProperties(CBoring* parent, std::list<Property *> *list)
{
	list->push_back(new PropertyLength(_("standoff"), m_standoff, parent, on_set_standoff));

	switch(theApp.m_program->m_clearance_source)
	{
	case CProgram::eClearanceDefinedByFixture:
		list->push_back(new PropertyString(_("clearance height"), _("Defined in fixture definition"), NULL, NULL));
		break;

	case CProgram::eClearanceDefinedByMachine:
		list->push_back(new PropertyString(_("clearance height"), _("Defined in Program properties for whole machine"), NULL, NULL));
		break;

	case CProgram::eClearanceDefinedByOperation:
	default:
		list->push_back(new PropertyLength(_("clearance height"), m_clearance_height, parent, on_set_clearance_height));
	} // End switch

	list->push_back(new PropertyDouble(_("dwell"), m_dwell, parent, on_set_dwell));
	list->push_back(new PropertyLength(_("depth"), m_depth, parent, on_set_depth));
	list->push_back(new PropertyLength(_("diameter"), m_diameter, parent, on_set_diameter));
	{ // Begin choice scope
		std::list< wxString > choices;

		choices.push_back(_("Rapid retract"));	// Must be 'false' (0)
		choices.push_back(_("Feed retract"));	// Must be 'true' (non-zero)

		int choice = int(m_retract_mode);
		list->push_back(new PropertyChoice(_("retract_mode"), choices, choice, parent, on_set_retract_mode));
	} // End choice scope
	{ // Begin choice scope
		std::list< wxString > choices;

		choices.push_back(_("Keep running"));	// Must be 'false' (0)
		choices.push_back(_("Stop at bottom"));	// Must be 'true' (non-zero)

		int choice = int(m_spindle_mode);
		list->push_back(new PropertyChoice(_("spindle_mode"), choices, choice, parent, on_set_spindle_mode));
	} // End choice scope
	{ // Begin choice scope
		std::list< wxString > choices;

		choices.push_back(_("Respect existing order"));	// Must be 'false' (0)
		choices.push_back(_("True"));			// Must be 'true' (non-zero)

		int choice = int(m_sort_boring_locations);
		list->push_back(new PropertyChoice(_("sort_boring_locations"), choices, choice, parent, on_set_sort_boring_locations));
	} // End choice scope

}

void CBoringParams::WriteXMLAttributes(TiXmlNode *root)
{
	TiXmlElement * element;
	element = heeksCAD->NewXMLElement( "params" );
	heeksCAD->LinkXMLEndChild( root,  element );

	element->SetDoubleAttribute( "standoff", m_standoff);
	element->SetDoubleAttribute( "dwell", m_dwell);
	element->SetDoubleAttribute( "depth", m_depth);
	element->SetDoubleAttribute( "diameter", m_diameter);

	element->SetAttribute( "sort_boring_locations", m_sort_boring_locations);
	element->SetAttribute( "retract_mode", m_retract_mode);
	element->SetAttribute( "spindle_mode", m_spindle_mode);
	element->SetAttribute( "clearance_height", m_clearance_height);
}

void CBoringParams::ReadParametersFromXMLElement(TiXmlElement* pElem)
{
	if (pElem->Attribute("standoff")) pElem->Attribute("standoff", &m_standoff);
	m_clearance_height = m_standoff;  // Default if the clearance_height parameter is not found.
	if (pElem->Attribute("dwell")) pElem->Attribute("dwell", &m_dwell);
	if (pElem->Attribute("depth")) pElem->Attribute("depth", &m_depth);
	if (pElem->Attribute("diameter")) pElem->Attribute("diameter", &m_diameter);
	if (pElem->Attribute("sort_boring_locations")) pElem->Attribute("sort_boring_locations", &m_sort_boring_locations);
	if (pElem->Attribute("retract_mode")) pElem->Attribute("retract_mode", (int *) &m_retract_mode);
	if (pElem->Attribute("spindle_mode")) pElem->Attribute("spindle_mode", (int *) &m_spindle_mode);
	if (pElem->Attribute("clearance_height")) pElem->Attribute("clearance_height", &m_clearance_height);
}

const wxBitmap &CBoring::GetIcon()
{
	static wxBitmap* icon = NULL;
	if(icon == NULL)icon = new wxBitmap(wxImage(theApp.GetResFolder() + _T("/icons/boring.png")));
	return *icon;
}


/**
	This method is called when the CAD operator presses the Python button.  This method generates
	Python source code whose job will be to generate RS-274 GCode.  It's done in two steps so that
	the Python code can be configured to generate GCode suitable for various CNC interpreters.
 */
Python CBoring::AppendTextToProgram( CMachineState *pMachineState )
{
	Python python;

	python << CSpeedOp::AppendTextToProgram( pMachineState );   // Set any private fixtures and change tools (if necessary)

	std::vector<CNCPoint> locations = CDrilling::FindAllLocations(this, pMachineState->Location(), m_params.m_sort_boring_locations != 0, NULL);
	for (std::vector<CNCPoint>::const_iterator l_itLocation = locations.begin(); l_itLocation != locations.end(); l_itLocation++)
	{
		gp_Pnt point = pMachineState->Fixture().Adjustment( *l_itLocation );

		python << _T("boring(")
			<< _T("x=") << point.X()/theApp.m_program->m_units << _T(", ")
			<< _T("y=") << point.Y()/theApp.m_program->m_units << _T(", ")
			<< _T("z=") << point.Z()/theApp.m_program->m_units << _T(", ")
			<< _T("depth=") << m_params.m_depth/theApp.m_program->m_units << _T(", ")
			<< _T("standoff=") << m_params.m_standoff/theApp.m_program->m_units << _T(", ")
			<< _T("dwell=") << m_params.m_dwell << _T(", ")
			<< _T("retract_mode=") << int(m_params.m_retract_mode) << _T(", ")
			<< _T("spindle_mode=") << int(m_params.m_spindle_mode) << _T(", ")
			<< _T("clearance_height=") << m_params.ClearanceHeight()
			<< _T(")\n");
        pMachineState->Location(point); // Remember where we are.
	} // End for

	python << _T("end_canned_cycle()\n");

	return(python);
}


/**
	This routine generates a list of coordinates around the circumference of a circle.  It's just used
	to generate data suitable for OpenGL calls to paint a circle.  This graphics is transient but will
	help represent what the GCode will be doing when it's generated.
 */
std::list< CNCPoint > CBoring::PointsAround(
		const CNCPoint & origin,
		const double radius,
		const unsigned int numPoints ) const
{
	std::list<CNCPoint> results;

	double alpha = 3.1415926 * 2 / numPoints;

	unsigned int i = 0;
	while( i++ < numPoints )
	{
		double theta = alpha * i;
		CNCPoint pointOnCircle( cos( theta ) * radius, sin( theta ) * radius, 0 );
		pointOnCircle += origin;
		results.push_back(pointOnCircle);
	} // End while

	return(results);

} // End PointsAround() routine


/**
	Generate a list of vertices that represent the hole that will be drilled.  Let it be a circle at the top, a
	spiral down the length and a countersunk base.

	This method is only called by the glCommands() method.  This means that the graphics is transient.

	TODO: Handle boring in any rotational angle. At the moment it only handles boring 'down' along the 'z' axis
 */

std::list< CNCPoint > CBoring::DrillBitVertices( const CNCPoint & origin, const double radius, const double length ) const
{
	std::list<CNCPoint> top, spiral, bottom, countersink, result;

	double flutePitch = 5.0;	// 5mm of depth per spiral of the drill bit's flute.
	double countersinkDepth = -1 * radius * tan(31.0); // this is the depth of the countersink cone at the end of the drill bit. (for a typical 118 degree bevel)
	unsigned int numPoints = 20;	// number of points in one circle (360 degrees) i.e. how smooth do we want the graphics
	const double pi = 3.1415926;
	double alpha = 2 * pi / numPoints;

	// Get a circle at the top of the dill bit's path
	top = PointsAround( origin, radius, numPoints );
	top.push_back( *(top.begin()) );	// Close the circle

	double depthPerItteration;
	countersinkDepth = -1 * radius * tan(31.0);	// For a typical (118 degree bevel on the drill bit tip)

	unsigned int l_iNumItterations = numPoints * (length / flutePitch);
	depthPerItteration = length / l_iNumItterations;

	// Now generate the spirals.

	unsigned int i = 0;
	while( i++ < l_iNumItterations )
	{
		double theta = alpha * i;
		CNCPoint pointOnCircle( cos( theta ) * radius, sin( theta ) * radius, 0 );
		pointOnCircle += origin;

		// And spiral down as we go.
		pointOnCircle.SetZ( pointOnCircle.Z() - (depthPerItteration * i) );

		spiral.push_back(pointOnCircle);
	} // End while

    CNCPoint bottom_point(origin);
    bottom_point.SetZ(bottom_point.Z() - length);
    bottom = PointsAround( bottom_point, radius, numPoints );
	bottom.push_back( *(bottom.begin()) );	// Close the circle

	std::copy( top.begin(), top.end(), std::inserter( result, result.begin() ) );
	std::copy( spiral.begin(), spiral.end(), std::inserter( result, result.end() ) );
	std::copy( bottom.begin(), bottom.end(), std::inserter( result, result.end() ) );

	return(result);

} // End DrillBitVertices() routine


/**
	This is the Graphics Library Commands (from the OpenGL set).  This method calls the OpenGL
	routines to paint the drill action in the graphics window.  The graphics is transient.

	Part of its job is to re-paint the elements that this CBoring object refers to so that
	we know what CAD objects this CNC operation is referring to.
 */
void CBoring::glCommands(bool select, bool marked, bool no_color)
{
    std::list<CFixture> fixtures = PrivateFixtures();
	if (fixtures.size() == 0)
	{
	    fixtures = theApp.m_program->Fixtures()->PublicFixtures();
	}

    for (std::list<CFixture>::iterator itFixture = fixtures.begin(); itFixture != fixtures.end(); itFixture++)
    {
        if (m_params.m_depth < 0)
        {
            m_params.m_depth *= -1.0;
        }

        CSpeedOp::glCommands(select, marked, no_color);

        if(marked && !no_color)
        {
			double l_dHoleDiameter = m_params.m_diameter;

            std::vector<CNCPoint> locations = CDrilling::FindAllLocations(this);

            for (std::vector<CNCPoint>::const_iterator l_itLocation = locations.begin(); l_itLocation != locations.end(); l_itLocation++)
            {
                GLdouble start[3], end[3];

				CNCPoint from( itFixture->Adjustment(*l_itLocation) );
				CNCPoint to( itFixture->Adjustment(*l_itLocation) );

				from.ToDoubleArray(start);
				to.ToDoubleArray(end);

                end[2] -= m_params.m_depth;

				{
					gp_Pnt from = itFixture->ReverseAdjustment( gp_Pnt(start[0], start[1], start[2]) );
					gp_Pnt to = itFixture->ReverseAdjustment( gp_Pnt(end[0], end[1], end[2]) );

					glBegin(GL_LINE_STRIP);
					glVertex3d( from.X(), from.Y(), from.Z() );
					glVertex3d( to.X(), to.Y(), to.Z() );
					glEnd();
				}


				CNCPoint point( itFixture->Adjustment(*l_itLocation) );
                std::list< CNCPoint > pointsAroundCircle = DrillBitVertices( 	point,
                                                    l_dHoleDiameter / 2,
                                                    m_params.m_depth);

                glBegin(GL_LINE_STRIP);
                CNCPoint previous = *(pointsAroundCircle.begin());

                previous = itFixture->ReverseAdjustment( previous );

                for (std::list< CNCPoint >::const_iterator l_itPoint = pointsAroundCircle.begin();
                    l_itPoint != pointsAroundCircle.end();
                    l_itPoint++)
                {


                    gp_Pnt point = itFixture->ReverseAdjustment( *l_itPoint );

                    glBegin(GL_LINE_STRIP);
                    glVertex3d( previous.X(), previous.Y(), previous.Z() );
                    glVertex3d( point.X(), point.Y(), point.Z() );
                    glEnd();
                    previous = point;
                }
                glEnd();
            } // End for
        } // End if - then
    } // End for
}


void CBoring::GetProperties(std::list<Property *> *list)
{
	m_params.GetProperties(this, list);
	CSpeedOp::GetProperties(list);
}

HeeksObj *CBoring::MakeACopy(void)const
{
	return new CBoring(*this);
}

void CBoring::CopyFrom(const HeeksObj* object)
{
	if (object->GetType() == GetType())
	{
		operator=(*((CBoring*)object));
	}
}

CBoring::CBoring(	std::list<HeeksObj *> reference_objects,
        const int tool_number,
        const double depth )
    : CSpeedOp(GetTypeString(), tool_number, BoringType)
{
    m_params.set_initial_values(depth, tool_number);
	for (std::list<HeeksObj *>::iterator itObject = reference_objects.begin(); itObject != reference_objects.end(); itObject++)
	{
		HeeksObj *obj = *itObject;
        if (obj != NULL)
        {
            Add(obj, NULL);
        }
    } // End for
}


CBoring::CBoring( const CBoring & rhs ) : CSpeedOp( rhs )
{
    m_params = rhs.m_params;
}

CBoring & CBoring::operator= ( const CBoring & rhs )
{
	if (this != &rhs)
	{
		CSpeedOp::operator=(rhs);
		m_params = rhs.m_params;
	}

	return(*this);
}

bool CBoring::CanAddTo(HeeksObj* owner)
{
    if (owner == NULL) return(false);

	int type = owner->GetType();

	if (type == OperationsType) return(true);
	if (type == CounterBoreType) return(true);

	return(false);
}

bool CBoring::CanAdd(HeeksObj* object)
{
	return(CBoring::ValidType(object->GetType()));
}

void CBoring::WriteXML(TiXmlNode *root)
{
	TiXmlElement * element = heeksCAD->NewXMLElement( "Boring" );
	heeksCAD->LinkXMLEndChild( root,  element );
	m_params.WriteXMLAttributes(element);

	WriteBaseXML(element);
}

// static member function
HeeksObj* CBoring::ReadFromXMLElement(TiXmlElement* element)
{
	CBoring* new_object = new CBoring;

	// read point and circle ids
	for(TiXmlElement* pElem = heeksCAD->FirstXMLChildElement( element ) ; pElem; pElem = pElem->NextSiblingElement())
	{
		std::string name(pElem->Value());
		if(name == "params"){
			new_object->m_params.ReadParametersFromXMLElement(pElem);
		}
	}

	new_object->ReadBaseXML(element);

	return new_object;
}


/**
	The old version of the CBoring object stored references to graphics as type/id pairs
	that get read into the m_symbols list.  The new version stores these graphics references
	as child elements (based on ObjList).  If we read in an old-format file then the m_symbols
	list will have data in it for which we don't have children.  This routine converts
	these type/id pairs into the HeeksObj pointers as children.
 */
void CBoring::ReloadPointers()
{
	CSpeedOp::ReloadPointers();
}




/**
	This method adjusts any parameters that don't make sense.  It should report a list
	of changes in the list of strings.
 */
std::list<wxString> CBoring::DesignRulesAdjustment(const bool apply_changes)
{
	std::list<wxString> changes;

	if (m_tool_number > 0)
	{
		// Make sure the hole depth isn't greater than the tool's cutting depth.
		CTool *pTool = (CTool *) CTool::Find( m_tool_number );
		if (pTool->m_params.m_type != CToolParams::eBoringHead)
		{
			wxString change;
			change << DesignRulesPreamble() << _("is using a ") << pTool->m_params.m_type;
			changes.push_back(change);
		}

		if ((pTool != NULL) && (pTool->m_params.m_cutting_edge_height < m_params.m_depth))
		{
			// The drill bit we've chosen can't cut as deep as we've setup to go.

			if (apply_changes)
			{
				wxString change;

				change << DesignRulesPreamble() << _("Adjusting depth of boring cycle from '")
					<< m_params.m_depth / theApp.m_program->m_units << _(" to ")
					<< pTool->m_params.m_cutting_edge_height / theApp.m_program->m_units;
				changes.push_back(change);

				m_params.m_depth = pTool->m_params.m_cutting_edge_height;
			} // End if - then
			else
			{
				wxString change;
				change << DesignRulesPreamble() << _("Can't bore hole ") << m_params.m_depth / theApp.m_program->m_units << _(" when the boring head bit's cutting length is only ") << pTool->m_params.m_cutting_edge_height << _(" long\n");
				changes.push_back(change);
			} // End if - else
		} // End if - then
	} // End if - then


	std::list<wxString> extra_changes = CSpeedOp::DesignRulesAdjustment(apply_changes);
	std::copy( extra_changes.begin(), extra_changes.end(), std::inserter( changes, changes.end() ));

	return(changes);

} // End DesignRulesAdjustment() method


/**
    This method returns TRUE if the type of symbol is suitable for reference as a source of location
 */
/* static */ bool CBoring::ValidType( const int object_type )
{
	return(CDrilling::ValidType(object_type));
}


void CBoring::GetTools(std::list<Tool*>* t_list, const wxPoint* p)
{
    CSpeedOp::GetTools( t_list, p );
}


bool CBoringParams::operator==( const CBoringParams & rhs) const
{
	if (m_standoff != rhs.m_standoff) return(false);
	if (m_dwell != rhs.m_dwell) return(false);
	if (m_depth != rhs.m_depth) return(false);
	if (m_diameter != rhs.m_diameter) return(false);
	if (m_sort_boring_locations != rhs.m_sort_boring_locations) return(false);
	if (m_retract_mode != rhs.m_retract_mode) return(false);
	if (m_spindle_mode != rhs.m_spindle_mode) return(false);
	if (m_clearance_height != rhs.m_clearance_height) return(false);

	return(true);
}


bool CBoring::operator==( const CBoring & rhs ) const
{
	if (m_params != rhs.m_params) return(false);

	return(CSpeedOp::operator==(rhs));
}

double CBoringParams::ClearanceHeight() const
{
	switch (theApp.m_program->m_clearance_source)
	{
	case CProgram::eClearanceDefinedByMachine:
		return(theApp.m_program->m_machine.m_clearance_height);

	case CProgram::eClearanceDefinedByFixture:
		// We need to figure out which is the 'active' fixture and return
		// the clearance height from that fixture.

		if (theApp.m_program->m_active_machine_state != NULL)
		{
			return(theApp.m_program->m_active_machine_state->Fixture().m_params.m_clearance_height);
		}
		else
		{
			// This should not occur.  In any case, use the clearance value from the individual operation.
			return(m_clearance_height);
		}

	case CProgram::eClearanceDefinedByOperation:
	default:
		return(m_clearance_height);
	} // End switch
}


/**
	This routine is called when the COp::m_tool_number value is changed.
 */
/* virtual */ void CBoring::OnSetTool(const COp::ToolNumber_t new_tool_number)
{
	CTool *pTool = CTool::Find(new_tool_number);
	if (pTool)
	{
	}
}

/* virtual */ void CBoring::ResetSpeeds(const int tool_number, const double bored_hole_diameter)
{
	// Reset the speed based on this operation's hole diameter rather than the
	// tool's diameter.

	CSpeedOp::ResetSpeeds(tool_number, m_params.m_diameter);
}

/* virtual */ void CBoring::ResetFeeds(const int tool_number)
{
	CSpeedOp::ResetFeeds(tool_number);
}


