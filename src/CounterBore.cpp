// CounterBore.cpp
/*
 * Copyright (c) 2009, Dan Heeks, Perttu Ahola
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include "stdafx.h"

#include "CounterBore.h"
#include "CNCConfig.h"
#include "ProgramCanvas.h"
#include "interface/HeeksObj.h"
#include "interface/PropertyInt.h"
#include "interface/PropertyDouble.h"
#include "interface/PropertyLength.h"
#include "interface/PropertyChoice.h"
#include "tinyxml/tinyxml.h"
#include "CTool.h"
#include "Drilling.h"
#include "CNCPoint.h"
#include "PythonStuff.h"
#include "MachineState.h"
#include "Program.h"
#include "Fixtures.h"
#include "Contour.h"
#include "CDouble.h"

#include <sstream>
#include <algorithm>
#include <vector>

extern CHeeksCADInterface* heeksCAD;


void CCounterBoreParams::set_initial_values( const int tool_number )
{
	m_finishing_pass = 0.005 * 25.4;

	CNCConfig config(ConfigScope());

	config.Read(_T("m_diameter"), &m_diameter, (25.4 / 10));	// One tenth of an inch
	config.Read(_T("m_finishing_pass"), &m_finishing_pass, 0.005 * 25.4);	// Five thousands of an inch

	config.Read(_T("m_sort_locations"), &m_sort_locations, 1);

	if (tool_number > 0)
	{
		CTool *pTool = CTool::Find( tool_number );
		if (pTool != NULL)
		{
			std::pair< double, double > depth_and_diameter = CCounterBore::SelectSizeForHead( pTool->m_params.m_diameter );
			m_diameter = depth_and_diameter.second;
		} // End if - then
	} // End if - then
}

void CCounterBoreParams::write_values_to_config()
{
	// We always want to store the parameters in mm and convert them back later on.

	CNCConfig config(ConfigScope());
	config.Write(_T("m_diameter"), m_diameter);
	config.Write(_T("m_finishing_pass"), m_finishing_pass);
	config.Write(_T("m_sort_locations"), m_sort_locations);
}


static void on_set_diameter(double value, HeeksObj*  object)
{
	((CCounterBore*)object)->m_params.m_diameter = value;
	((CCounterBore*)object)->m_params.write_values_to_config();
}

static void on_set_finishing_pass(double value, HeeksObj*  object)
{
	((CCounterBore*)object)->m_params.m_finishing_pass = value;
	((CCounterBore*)object)->m_params.write_values_to_config();
}

static void on_set_sort_locations(int value, HeeksObj*  object)
{
	((CCounterBore*)object)->m_params.m_sort_locations = value;
	((CCounterBore*)object)->m_params.write_values_to_config();
}


void CCounterBoreParams::GetProperties(CCounterBore* parent, std::list<Property *> *list)
{
	list->push_back(new PropertyLength(_("diameter"), m_diameter, parent, on_set_diameter));
	list->push_back(new PropertyLength(_("finishing pass"), m_finishing_pass, parent, on_set_finishing_pass));

	{ // Begin choice scope
		std::list< wxString > choices;

		choices.push_back(_("Respect existing order"));	// Must be 'false' (0)
		choices.push_back(_("True"));			// Must be 'true' (non-zero)

		int choice = int(m_sort_locations);
		list->push_back(new PropertyChoice(_("Sort Locations"), choices, choice, parent, on_set_sort_locations));
	} // End choice scope
}

void CCounterBoreParams::WriteXMLAttributes(TiXmlNode *root)
{
	TiXmlElement * element;
	element = heeksCAD->NewXMLElement( "params" );
	heeksCAD->LinkXMLEndChild( root,  element );
	element->SetDoubleAttribute( "diameter", m_diameter);
	element->SetDoubleAttribute( "finishing_pass", m_finishing_pass);
	element->SetAttribute( "sort_locations", m_sort_locations);
}

void CCounterBoreParams::ReadParametersFromXMLElement(TiXmlElement* pElem)
{
	if (pElem->Attribute("diameter")) pElem->Attribute("diameter", &m_diameter);
	if (pElem->Attribute("finishing_pass")) pElem->Attribute("finishing_pass", &m_finishing_pass);
	if (pElem->Attribute("sort_locations")) pElem->Attribute("sort_locations", &m_sort_locations);
}


static double drawing_units( const double value )
{
	return(value / theApp.m_program->m_units);
}

const wxBitmap &CCounterBore::GetIcon()
{
	static wxBitmap* icon = NULL;
	if(icon == NULL)icon = new wxBitmap(wxImage(theApp.GetResFolder() + _T("/icons/drilling.png")));
	return *icon;
}


Python CCounterBore::OneRevolution(
   CMachineState *pMachineState,
   CNCPoint centre,
   const double toolpath_radius,
   const double gradient ) const
{
	Python python;
	double circumference = 2.0 * PI * toolpath_radius;

	CNCPoint point(centre);
	// First quadrant (12 O'Clock to 9 O'Clock)
	point.SetX( centre.X(false) - toolpath_radius );
	point.SetY( centre.Y(false) );
	point.SetZ( pMachineState->Location().Z() + (gradient * (circumference * (1.0 / 4.0))) );
	python << _T("arc_ccw( x=") << point.X(true) << _T(", ") <<
				_T("y=") << point.Y(true) << _T(", ") <<
				_T("z=") << point.Z(true) << _T(", ") <<
				_T("i=") << centre.X(true) << _T(", ") <<
				_T("j=") << centre.Y(true) << _T(")\n");
	pMachineState->Location(point);

	// Second quadrant (9 O'Clock to 6 O'Clock)
	point.SetX( centre.X(false) );
	point.SetY( centre.Y(false) - toolpath_radius );
	point.SetZ( pMachineState->Location().Z() + (gradient * (circumference * (1.0 / 4.0))) );
	python << _T("arc_ccw( x=") << point.X(true) << _T(", ") <<
				_T("y=") << point.Y(true) << _T(", ") <<
				_T("z=") << point.Z(true) << _T(", ") <<
				_T("i=") << centre.X(true) << _T(", ") <<
				_T("j=") << centre.Y(true) << _T(")\n");
	pMachineState->Location(point);

	// Third quadrant (6 O'Clock to 3 O'Clock)
	point.SetX( centre.X(false) + toolpath_radius );
	point.SetY( centre.Y(false) );
	point.SetZ( pMachineState->Location().Z() + (gradient * (circumference * (1.0 / 4.0))) );
	python << _T("arc_ccw( x=") << point.X(true) << _T(", ") <<
				_T("y=") << point.Y(true) << _T(", ") <<
				_T("z=") << point.Z(true) << _T(", ") <<
				_T("i=") << centre.X(true) << _T(", ") <<
				_T("j=") << centre.Y(true) << _T(")\n");
	pMachineState->Location(point);

	// Fourth quadrant (3 O'Clock to 12 O'Clock)
	point.SetX( centre.X(false) );
	point.SetY( centre.Y(false) + toolpath_radius );
	point.SetZ( pMachineState->Location().Z() + (gradient * (circumference * (1.0 / 4.0))) );
	python << _T("arc_ccw( x=") << point.X(true) << _T(", ") <<
				_T("y=") << point.Y(true) << _T(", ") <<
				_T("z=") << point.Z(true) << _T(", ") <<
				_T("i=") << centre.X(true) << _T(", ") <<
				_T("j=") << centre.Y(true) << _T(")\n");
	pMachineState->Location(point);

	return(python);
}

Python CCounterBore::HelicalToolpath(
	const CNCPoint & location,
	const CTool *pTool,
	const double toolpath_radius,
	CMachineState *pMachineState,
	const double start_depth,
	const double final_depth,
	const double requested_gradient,
	const double requested_step_down ) const
{
	Python python;

	double tolerance = heeksCAD->GetTolerance();

	double gradient(requested_gradient);
	double step_down(requested_step_down);

	if (gradient > 0) gradient *= -1.0;
	if (pTool->m_params.m_gradient < gradient)
	{
		// The tool can't handle this steep a descent.
		gradient = pTool->m_params.m_gradient;
	}

	if (CDouble(gradient) == CDouble(0.0))
	{
		return(python);	// empty
	}

	if (pMachineState->Location().Z() < final_depth)
	{
	    wxMessageBox(_T("ERROR in Counterbore data.  Start depth < Final Depth"));
	    return(python);
	}

	if (step_down <= 0)
	{
        wxMessageBox(_("Step down must be a positive number"));
        return(python);
	}

	CNCPoint centre( location );
	CNCPoint north( location ); north.SetY(location.Y() + toolpath_radius);

	python << _T("comment('Helical toolpath down to ") << final_depth << _("')\n");

	CNCPoint point(north);
	if (pMachineState->Location().XYDistance(north) > tolerance)
	{
		// Rapid to above the starting point (up at clearance height)
		point.SetZ( m_depth_op_params.ClearanceHeight() );
		python << _T("rapid( z=") << point.Z(true) << _T(")\n");
		python << _T("rapid( x=") << point.X(true) << _T(", y=") << point.Y(true) << _T(")\n");

		// Rapid down to the 'rapid_safety_space'
		point.SetZ( m_depth_op_params.m_rapid_safety_space );
		python << _T("rapid( x=") << point.X(true) << _T(", y=") << point.Y(true) << _T(", z=") << point.Z(true) << _T(")\n");

		// Feed (slowly) to the starting point at the centre of the material
		point.SetZ(start_depth);
		python << _T("feed( x=") << point.X(true) << _T(", y=") << point.Y(true) << _T(", z=") << point.Z(true) << _T(")\n");
		pMachineState->Location(point);
	}
	else
	{
		if ((pMachineState->Location().Z() - start_depth) > tolerance)
		{
			point.SetZ(start_depth);
			python << _T("feed(z=") << point.Z(true) << _T(")\n");
			pMachineState->Location(point);
		}
	}

	// At this point we're at the north point at start_depth.


	if ((CDouble(pMachineState->Location().Z() - step_down)) < CDouble(final_depth))
	{
		step_down = pMachineState->Location().Z() - final_depth;	// last pass
	}

	double gradient_for_this_pass = gradient;
	// Adjust the gradient if necessary so that we end up at one step-down depth
	// when we arrive back at the north point.

	double circumference = 2.0 * PI * toolpath_radius;
	int num_revolutions = 0;
	do {
		num_revolutions++;
		gradient_for_this_pass = (step_down / (double(num_revolutions) * circumference)) * -1.0;;
	} while (CDouble(fabs(gradient_for_this_pass)) > CDouble(fabs(gradient)));

	while (CDouble(pMachineState->Location().Z()) > (CDouble(final_depth)))
	{
		python << OneRevolution( pMachineState, centre, toolpath_radius, gradient_for_this_pass );
	} // End while

	return(python);

} // End HelicalToolpath() method




/**
	Before we call this routine, we're sure that the tool's diameter is <= counterbore's diameter.  To
	this end, we need to spiral down to each successive cutting depth and then spiral out to the
	outside diameter.  Repeat these operations until we've cut the full depth and the full width.
 */
Python CCounterBore::GenerateGCodeForOneLocation( const CNCPoint & location, const CTool *pTool, CMachineState *pMachineState ) const
{
	Python python;

	double cutting_depth = m_depth_op_params.m_start_depth;
	double final_depth   = m_depth_op_params.m_final_depth;
	double tool_overlap_proportion = 0.95;

	if (cutting_depth < final_depth)
	{
	    wxMessageBox(_T("ERROR in Counterbore data.  Start depth < Final Depth"));
	    return(python);
	}

	if (m_depth_op_params.m_step_down <= 0)
	{
        wxMessageBox(_("Step down must be a positive number"));
        return(python);
	}

	CNCPoint point( location );
	CNCPoint centre( point );

	// Rapid to above the starting point (up at clearance height)
	point.SetZ( m_depth_op_params.ClearanceHeight() );
	python << _T("rapid( x=") << point.X(true) << _T(", y=") << point.Y(true) << _T(", z=") << point.Z(true) << _T(")\n");

	// Rapid down to the 'rapid_safety_space'
	point.SetZ( m_depth_op_params.m_rapid_safety_space );
	python << _T("rapid( x=") << point.X(true) << _T(", y=") << point.Y(true) << _T(", z=") << point.Z(true) << _T(")\n");

	// Feed (slowly) to the starting point at the centre of the material
	point.SetZ(m_depth_op_params.m_start_depth);
	python << _T("feed( x=") << point.X(true) << _T(", y=") << point.Y(true) << _T(", z=") << point.Z(true) << _T(")\n");

	double tolerance = heeksCAD->GetTolerance();
	double max_radius_of_spiral = (m_params.m_diameter / 2.0) - pTool->CuttingRadius(false) - m_params.m_finishing_pass;

	while (CDouble(cutting_depth) > CDouble(final_depth))
	{
		python << _T("comment('Spiral down until we get to the cutting depth')\n");

		double step_down = m_depth_op_params.m_step_down;
		if (CDouble(cutting_depth - step_down) < CDouble(final_depth))
		{
			step_down = cutting_depth - final_depth;	// last pass
		}

		// We want to spiral down to the next cutting depth over a half circle distance.
		// The width of the spiral will depend on the diameter of the tool and the diameter of
		// the hole.  We don't want it to be more than the tool's radius but we also don't
		// want it to be wider than the hole.

		double radius_of_spiral = pTool->CuttingRadius(false) * tool_overlap_proportion;
		if (radius_of_spiral > max_radius_of_spiral)
		{
			// Reduce the radius of the spiral so that we don't run outside the hole.

			radius_of_spiral = max_radius_of_spiral;
		} // End if - then

		// Now spiral down using the width_of_spiral to the cutting_depth.
		// Move to 12 O'Clock.
		python << _T("feed( x=") << centre.X(true) << _T(", ")
					_T("y=") << drawing_units(centre.Y() + radius_of_spiral) << _T(", ")
					_T("z=") << drawing_units(cutting_depth) << _T(")\n");
		point.SetX( centre.X(false) );
		point.SetY( centre.Y(false) + radius_of_spiral );
		point.SetZ( cutting_depth );
		pMachineState->Location(point);

		python << HelicalToolpath( centre,
									pTool,
									radius_of_spiral,
									pMachineState,
									cutting_depth,
									cutting_depth - step_down,
									pTool->m_params.m_gradient,
									step_down );

		if ((max_radius_of_spiral - radius_of_spiral) > tolerance) {
			python << _T("comment('Now spiral outwards to the counterbore perimeter')\n");
		} // End if - then

		while ((max_radius_of_spiral - radius_of_spiral) > tolerance) {
			radius_of_spiral += (pTool->CuttingRadius(false) * tool_overlap_proportion);
			if (radius_of_spiral > max_radius_of_spiral)
			{
				// Reduce the radius of the spiral so that we don't run outside the hole.
				radius_of_spiral = max_radius_of_spiral;
			} // End if - then

			// Move to 12 O'Clock.
			python << _T("feed( x=") << centre.X(true) << _T(", ")
						_T("y=") << drawing_units(centre.Y(false) + radius_of_spiral) << _T(", ")
						_T("z=") << drawing_units(cutting_depth - (1.0 * step_down)) << _T(")\n");
			point.SetX( centre.X(false) );
			point.SetY( centre.Y(false) + radius_of_spiral );

			python << OneRevolution( pMachineState, centre, radius_of_spiral, 0.0 );
		}

		if (((cutting_depth - final_depth) < step_down) && ((cutting_depth - final_depth) > tolerance))
		{
			// Last pass at this depth.
			cutting_depth = final_depth;
		} // End if - then
		else
		{
			cutting_depth -= step_down;
		} // End if - else
	} // End while

	if (m_params.m_finishing_pass > 0.0)
	{
		// Rapid back to the centre of the hole (at the bottom) so we don't hit the
		// sides on the way up.
		python << _T("comment('Finishing pass')\n");
		point = centre;
		python << _T("rapid( x=") << centre.X(true) << _T(", y=") << centre.Y(true) << _T(")\n");

		// Rapid to above the starting point (up at clearance height)
		point.SetZ( m_depth_op_params.m_start_depth );
		python << _T("rapid( z=") << point.Z(true) << _T(")\n");
		pMachineState->Location(point);

		// Move to 12 O'Clock.
		double radius_of_spiral = (m_params.m_diameter / 2.0) - pTool->CuttingRadius(false);

		cutting_depth = m_depth_op_params.m_start_depth;
		python << _T("feed( x=") << centre.X(true) << _T(", ")
					_T("y=") << drawing_units(centre.Y() + radius_of_spiral) << _T(", ")
					_T("z=") << drawing_units(cutting_depth) << _T(")\n");
		point.SetX( centre.X(false) );
		point.SetY( centre.Y(false) + radius_of_spiral );
		point.SetZ( cutting_depth );
		pMachineState->Location(point);

		python << HelicalToolpath( centre,
									pTool,
									radius_of_spiral,
									pMachineState,
									m_depth_op_params.m_start_depth,
									m_depth_op_params.m_final_depth,
									pTool->m_params.m_gradient,
									m_depth_op_params.m_step_down );

		python << OneRevolution( pMachineState, centre, radius_of_spiral, 0.0 );
	}

	// Rapid back to the centre of the hole (at the bottom) so we don't hit the
	// sides on the way up.
	python << _T("rapid( x=") << centre.X(true) << _T(", y=") << centre.Y(true) << _T(", z=") << drawing_units(final_depth) << _T(")\n");

	// Rapid to above the starting point (up at clearance height)
	point.SetZ( m_depth_op_params.ClearanceHeight() );
	python << _T("rapid( x=") << centre.X(true) << _T(", y=") << centre.Y(true) << _T(", z=") << point.Z(true) << _T(")\n");

	return(python);

} // End GenerateGCodeForOneLocation() method



/**
	This method is called when the CAD operator presses the Python button.  This method generates
	Python source code whose job will be to generate RS-274 GCode.  It's done in two steps so that
	the Python code can be configured to generate GCode suitable for various CNC interpreters.
 */
Python CCounterBore::AppendTextToProgram(CMachineState *pMachineState)
{
	Python python;

	python << CDepthOp::AppendTextToProgram(pMachineState);

	if (m_tool_number > 0)
	{
		CTool *pTool = CTool::Find( m_tool_number );
		if (pTool != NULL)
		{
			if ((pTool->CuttingRadius() * 2.0) >= m_params.m_diameter)
			{
				wxString message;
				message << _("Error: Tool diameter (") << pTool->m_params.m_diameter << _T(") ")
					 << _(">= hole diameter (") << m_params.m_diameter << _T(") ")
					 << _("in counter bore operation.  ")
					 << _("Skipping this counter bore operation (ID=") << m_id << _T(")");
				wxMessageBox(message);
				return(python);
			} // End if - then

			std::vector<CNCPoint> locations = CDrilling::FindAllLocations( this, pMachineState->Location(), m_params.m_sort_locations != 0, NULL );
			for (std::vector<CNCPoint>::const_iterator l_itLocation = locations.begin(); l_itLocation != locations.end(); l_itLocation++)
			{
				CNCPoint point( pMachineState->Fixture().Adjustment(*l_itLocation) );
				python << GenerateGCodeForOneLocation( point, pTool, pMachineState );
				pMachineState->Location(*l_itLocation); // Remember where we are.
			} // End for
		} // End if - then
		else
		{
			wxString message;
			message << _("Warning: Counter bore refers to a tool ")
				 << _("that can't be found in the model.  ")
				 << _("Skipping this counter bore operation (ID=") << m_id << _T(")");
			wxMessageBox(message);
		} // End if - else
	} // End if - then
	else
	{
		wxString message;
		message << _("Warning: Counter bore operations MUST refer to a tool.  ")
			 << _("Skipping this counter bore operation (ID=") << m_id << _T(")");
		wxMessageBox(message);
	} // End if - else

	return(python);
}


/**
	This routine generates a list of coordinates around the circumference of a circle.  It's just used
	to generate data suitable for OpenGL calls to paint a circle.  This graphics is transient but will
	help represent what the GCode will be doing when it's generated.
 */
std::list< CNCPoint > CCounterBore::PointsAround( const CNCPoint & origin, const double radius, const unsigned int numPoints ) const
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
	This is the Graphics Library Commands (from the OpenGL set).  This method calls the OpenGL
	routines to paint the drill action in the graphics window.  The graphics is transient.

	Part of its job is to re-paint the elements that this CCounterBore object refers to so that
	we know what CAD objects this CNC operation is referring to.
 */
void CCounterBore::glCommands(bool select, bool marked, bool no_color)
{
	CDepthOp::glCommands(select,marked,no_color);

	if(marked && !no_color)
	{
	    std::list<CFixture> fixtures = PrivateFixtures();
        if (fixtures.size() == 0)
        {
            fixtures = theApp.m_program->Fixtures()->PublicFixtures();
        }

        for (std::list<CFixture>::iterator itFixture = fixtures.begin(); itFixture != fixtures.end(); itFixture++)
        {
            // For all coordinates that relate to these reference objects, draw the graphics that represents
            // both a drilling hole and a counterbore.

            std::vector<CNCPoint> locations = CDrilling::FindAllLocations( this );
            for (std::vector<CNCPoint>::const_iterator l_itLocation = locations.begin(); l_itLocation != locations.end(); l_itLocation++)
            {
                std::list< CNCPoint > circle = PointsAround( *l_itLocation, m_params.m_diameter / 2, 10 );

                glBegin(GL_LINE_STRIP);
                // Once around the top circle.
                for (std::list<CNCPoint>::const_iterator l_itPoint = circle.begin(); l_itPoint != circle.end(); l_itPoint++)
                {
                    gp_Pnt point = itFixture->Adjustment(*l_itPoint);
                    GLdouble start[3], end[3];

                    start[0] = point.X();
                    start[1] = point.Y();
                    start[2] = m_depth_op_params.m_start_depth;

                    l_itPoint++;

                    if (l_itPoint != circle.end())
                    {
                        gp_Pnt point = itFixture->Adjustment(*l_itPoint);

                        end[0] = point.X();
                        end[1] = point.Y();
                        end[2] = m_depth_op_params.m_start_depth;

                        gp_Pnt from = itFixture->ReverseAdjustment( gp_Pnt(start[0], start[1], start[2]) );
                        gp_Pnt to = itFixture->ReverseAdjustment( gp_Pnt(end[0], end[1], end[2]) );

                        glVertex3d( from.X(), from.Y(), from.Z() );
                        glVertex3d( to.X(), to.Y(), to.Z() );
                    } // End if - then
                } // End for

                // Once around the bottom circle.
                for (std::list<CNCPoint>::const_iterator l_itPoint = circle.begin(); l_itPoint != circle.end(); l_itPoint++)
                {
                    gp_Pnt point = itFixture->Adjustment(*l_itPoint);

                    GLdouble start[3], end[3];

                    start[0] = point.X();
                    start[1] = point.Y();
                    start[2] = m_depth_op_params.m_final_depth;

                    l_itPoint++;

                    if (l_itPoint != circle.end())
                    {
                        gp_Pnt point = itFixture->Adjustment(*l_itPoint);

                        end[0] = point.X();
                        end[1] = point.Y();
                        end[2] = m_depth_op_params.m_final_depth;

                        gp_Pnt from = itFixture->ReverseAdjustment( gp_Pnt(start[0], start[1], start[2]) );
                        gp_Pnt to = itFixture->ReverseAdjustment( gp_Pnt(end[0], end[1], end[2]) );

                        glVertex3d( from.X(), from.Y(), from.Z() );
                        glVertex3d( to.X(), to.Y(), to.Z() );
                    } // End if - then
                } // End for

                // And once to join the two circles together.
                for (std::list<CNCPoint>::const_iterator l_itPoint = circle.begin(); l_itPoint != circle.end(); l_itPoint++)
                {
                    gp_Pnt point = itFixture->Adjustment(*l_itPoint);

                    GLdouble start[3], end[3];

                    start[0] = point.X();
                    start[1] = point.Y();
                    start[2] = m_depth_op_params.m_start_depth;

                    end[0] = point.X();
                    end[1] = point.Y();
                    end[2] = m_depth_op_params.m_final_depth;

                    gp_Pnt from = itFixture->ReverseAdjustment( gp_Pnt(start[0], start[1], start[2]) );
                    gp_Pnt to = itFixture->ReverseAdjustment( gp_Pnt(end[0], end[1], end[2]) );

                    glVertex3d( from.X(), from.Y(), from.Z() );
                    glVertex3d( to.X(), to.Y(), to.Z() );
                    glVertex3d( from.X(), from.Y(), from.Z() );
                } // End for
                glEnd();
            } // End for
		} // End for
	} // End if - then
}
CCounterBore::CCounterBore( const CCounterBore & rhs ) : CDepthOp(rhs)
{
	std::copy( rhs.m_symbols.begin(), rhs.m_symbols.end(), std::inserter( m_symbols, m_symbols.begin() ) );
    m_params = rhs.m_params;
}

CCounterBore & CCounterBore::operator= ( const CCounterBore & rhs )
{
	if (this != &rhs)
	{
		CDepthOp::operator=(rhs);
		m_symbols.clear();
		std::copy( rhs.m_symbols.begin(), rhs.m_symbols.end(), std::inserter( m_symbols, m_symbols.begin() ) );

		m_params = rhs.m_params;
	}

	return(*this);
}


void CCounterBore::GetProperties(std::list<Property *> *list)
{
	m_params.GetProperties(this, list);
	CDepthOp::GetProperties(list);
}

HeeksObj* CCounterBore::MakeACopy(void)const
{
	return new CCounterBore(*this);
}

void CCounterBore::CopyFrom(const HeeksObj*  object)
{
	if (object->GetType() == GetType())
	{
		operator=(*((CCounterBore*)object));
	}
}

bool CCounterBore::CanAddTo(HeeksObj*  owner)
{
	return ((owner != NULL) && (owner->GetType() == OperationsType));
}

void CCounterBore::WriteXML(TiXmlNode *root)
{
	TiXmlElement * element = heeksCAD->NewXMLElement( "CounterBore" );
	heeksCAD->LinkXMLEndChild( root,  element );
	m_params.WriteXMLAttributes(element);

	TiXmlElement * symbols;
	symbols = heeksCAD->NewXMLElement( "symbols" );
	heeksCAD->LinkXMLEndChild( element, symbols );

	for (Symbols_t::const_iterator l_itSymbol = m_symbols.begin(); l_itSymbol != m_symbols.end(); l_itSymbol++)
	{
		TiXmlElement * symbol = heeksCAD->NewXMLElement( "symbol" );
		symbols->LinkEndChild( symbol );
		symbol->SetAttribute("type", l_itSymbol->first );
		symbol->SetAttribute("id", l_itSymbol->second );
	} // End for

	WriteBaseXML(element);
}

// static member function
HeeksObj*  CCounterBore::ReadFromXMLElement(TiXmlElement* element)
{
	CCounterBore* new_object = new CCounterBore;

	std::list<TiXmlElement *> elements_to_remove;

	// read point and circle ids
	for(TiXmlElement* pElem = heeksCAD->FirstXMLChildElement( element ) ; pElem; pElem = pElem->NextSiblingElement())
	{
		std::string name(pElem->Value());
		if(name == "params"){
			new_object->m_params.ReadParametersFromXMLElement(pElem);
			elements_to_remove.push_back(pElem);
		}
		else if(name == "symbols"){
			for(TiXmlElement* child = heeksCAD->FirstXMLChildElement( pElem ) ; child; child = child->NextSiblingElement())
			{
				if (child->Attribute("type") && child->Attribute("id"))
				{
					new_object->AddSymbol( atoi(child->Attribute("type")), atoi(child->Attribute("id")) );
				}
			} // End for

			elements_to_remove.push_back(pElem);
		} // End if
	}

	for (std::list<TiXmlElement*>::iterator itElem = elements_to_remove.begin(); itElem != elements_to_remove.end(); itElem++)
	{
		heeksCAD->RemoveXMLChild( element, *itElem);
	}

	new_object->ReadBaseXML(element);

	return new_object;
}

/**
	The old version of the CDrilling object stored references to graphics as type/id pairs
	that get read into the m_symbols list.  The new version stores these graphics references
	as child elements (based on ObjList).  If we read in an old-format file then the m_symbols
	list will have data in it for which we don't have children.  This routine converts
	these type/id pairs into the HeeksObj pointers as children.
 */
void CCounterBore::ReloadPointers()
{
	for (Symbols_t::iterator symbol = m_symbols.begin(); symbol != m_symbols.end(); symbol++)
	{
		HeeksObj* object = heeksCAD->GetIDObject( symbol->first, symbol->second );
		if (object != NULL)
		{
			Add( object, NULL );
		}
	}

	m_symbols.clear();	// We don't want to convert them twice.

	CDepthOp::ReloadPointers();
}





// Return depth and diameter in that order.
std::pair< double, double > CCounterBore::SelectSizeForHead( const double drill_hole_diameter )
{
	// Just bluf it for now.  We will implement a lookup table eventually.

	return( std::make_pair( drill_hole_diameter * 1, drill_hole_diameter * 1.7 )  );

} // End SelectSizeForHead() method


bool CCounterBore::CanAdd(HeeksObj*  object)
{
	return((object != NULL) && (CCounterBore::ValidType(object->GetType())));
}

/**
    This method returns TRUE if the type of symbol is suitable for reference as a source of location

    NOTE: Since we're using the CDrilling::FindAllLocations() routine, we should just
    allow all the same child objects as the CDrilling operation allows.
 */
/* static */ bool CCounterBore::ValidType( const int object_type )
{
    return(CDrilling::ValidType(object_type));
}

void CCounterBore::GetTools(std::list<Tool*>* t_list, const wxPoint* p)
{
    CDepthOp::GetTools( t_list, p );
}


CCounterBore::CCounterBore(	const Symbols_t &symbols,
			const int tool_number )
		: CDepthOp(GetTypeString(), NULL, tool_number, CounterBoreType), m_symbols(symbols)
{
    for (Symbols_t::iterator symbol = m_symbols.begin(); symbol != m_symbols.end(); symbol++)
    {
        HeeksObj* object = heeksCAD->GetIDObject( symbol->first, symbol->second );
        if (object != NULL)
        {
            Add(object,NULL);
        } // End if - then
    } // End for
    m_symbols.clear();

    m_params.set_initial_values( tool_number );

    std::list<int> drillbits;
    std::vector<CNCPoint> locations = CDrilling::FindAllLocations( this, CNCPoint(0.0, 0.0, 0.0), false, &drillbits );
    if (drillbits.size() > 0)
    {
        // We found some drilling objects amongst the symbols. Use the diameter of
        // any tools they used to help decide what size cap screw we're trying to cater for.

        for (std::list<int>::const_iterator drillbit = drillbits.begin(); drillbit != drillbits.end(); drillbit++)
        {
            HeeksObj* object = heeksCAD->GetIDObject( ToolType, *drillbit );
            if (object != NULL)
            {
                std::pair< double, double > screw_size = SelectSizeForHead( ((CTool *) object)->m_params.m_diameter );
                m_depth_op_params.m_final_depth = screw_size.first;
                m_params.m_diameter = screw_size.second;
            } // End if - then
        } // End for
    } // End if - then
    else
    {
        // If there are no drilling operations as children, look to see how big the bounding
        // box is for each of the child objects.  The scenario we're looking for is where a
        // circular sketch is used as a child element.  In this case, get the counterbore
        // diameter from the sketch bounding box size.

        for (HeeksObj *child = GetFirstChild(); child != NULL; child = GetNextChild())
        {
            if (child->GetType() == SketchType)
            {
                CBox box;
                child->GetBox(box);

                if (box.Width() > 0)
                {
                    m_depth_op_params.m_final_depth = m_depth_op_params.m_start_depth - (box.Width() / 5.0);
                    m_params.m_diameter = box.Width();
                }
            }
        }
    }
}

bool CCounterBoreParams::operator== ( const CCounterBoreParams & rhs ) const
{
	if (m_diameter != rhs.m_diameter) return(false);
	if (m_finishing_pass != rhs.m_finishing_pass) return(false);
	if (m_sort_locations != rhs.m_sort_locations) return(false);

	return(true);
}


bool CCounterBore::operator== ( const CCounterBore & rhs ) const
{
	if (m_params != rhs.m_params) return(false);

	return(CDepthOp::operator==(rhs));
}



/**
	This method adjusts any parameters that don't make sense.  It should report a list
	of changes in the list of strings.
 */
std::list<wxString> CCounterBore::DesignRulesAdjustment(const bool apply_changes)
{
	std::list<wxString> changes;

	if (m_tool_number > 0)
	{
		// Make sure the hole depth isn't greater than the tool's cutting depth.
		CTool *pDrill = (CTool *) CTool::Find( m_tool_number );
		if (pDrill)
		{
			if ((pDrill->m_params.m_type != CToolParams::eEndmill) &&
				(pDrill->m_params.m_type != CToolParams::eSlotCutter) &&
				(pDrill->m_params.m_type != CToolParams::eBallEndMill))
			{
				wxString change;
				change << DesignRulesPreamble() << _("is using a ") << pDrill->m_params.m_type;
				changes.push_back(change);
			}

			if ((pDrill->CuttingRadius() * 2.0) > m_params.m_diameter)
			{
				wxString change;
				change << DesignRulesPreamble() << _("Tool diameter (") << (pDrill->CuttingRadius() * 2.0) / theApp.m_program->m_units << _(") is larger than the counterbore's diameter (") << m_params.m_diameter / theApp.m_program->m_units << _T(")");
				changes.push_back(change);
			}
		}
	} // End if - then

	std::list<wxString> extra_changes = CDepthOp::DesignRulesAdjustment(apply_changes);
	std::copy( extra_changes.begin(), extra_changes.end(), std::inserter( changes, changes.end() ));

	return(changes);

} // End DesignRulesAdjustment() method


