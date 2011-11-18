// Contour.cpp
/*
 * Copyright (c) 2009, Dan Heeks, Perttu Ahola
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include "stdafx.h"

#include "Contour.h"
#include "CNCConfig.h"
#include "ProgramCanvas.h"
#include "interface/HeeksObj.h"
#include "interface/PropertyInt.h"
#include "interface/PropertyDouble.h"
#include "interface/PropertyLength.h"
#include "interface/PropertyChoice.h"
#include "tinyxml/tinyxml.h"
#include "Operations.h"
#include "CTool.h"
#include "Profile.h"
#include "Fixture.h"
#include "Fixtures.h"
#include "CNCPoint.h"
#include "PythonStuff.h"
#include "MachineState.h"
#include "Program.h"
#include "interface/HeeksColor.h"

#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <functional>

#include <BRepOffsetAPI_MakeOffset.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Shape.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <gp_Circ.hxx>
#include <ShapeFix_Wire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRepMesh.hxx>
#include <Poly_Polygon3D.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <Handle_BRepAdaptor_HCurve.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Adaptor3d_HCurve.hxx>
#include <Adaptor3d_Curve.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <BRepOffsetAPI_NormalProjection.hxx>
#include <BRepProj_Projection.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>

extern CHeeksCADInterface* heeksCAD;

using namespace std;

/* static */ double CContour::max_deviation_for_spline_to_arc = 0.1;


static void on_set_tool_on_side(int value, HeeksObj* object){
	switch(value)
	{
	case 0:
		((CContour*)object)->m_params.m_tool_on_side = CContourParams::eLeftOrOutside;
		break;
	case 1:
		((CContour*)object)->m_params.m_tool_on_side = CContourParams::eRightOrInside;
		break;
	default:
		((CContour*)object)->m_params.m_tool_on_side = CContourParams::eOn;
		break;
	}
	((CContour*)object)->WriteDefaultValues();
}

static void on_set_entry_move_type(int value, HeeksObj *object)
{
    ((CContour*)object)->m_params.m_entry_move_type = CContourParams::EntryMove_t(value);
    ((CContour*)object)->WriteDefaultValues();
}

void CContourParams::GetProperties(CContour* parent, std::list<Property *> *list)
{
    {
        std::list< wxString > choices;

        SketchOrderType order = SketchOrderTypeUnknown;

        if(parent->GetNumChildren() > 0)
        {
            HeeksObj* sketch = NULL;

            for (HeeksObj *object = parent->GetFirstChild(); ((sketch == NULL) && (object != NULL)); object = parent->GetNextChild())
            {
                if (object->GetType() == SketchType)
                {
                    sketch = object;
                }
            }

            if(sketch != NULL)
            {
                order = heeksCAD->GetSketchOrder(sketch);
                switch(order)
                {
                case SketchOrderTypeOpen:
                    choices.push_back(_("Left"));
                    choices.push_back(_("Right"));
                    break;

                case SketchOrderTypeCloseCW:
                case SketchOrderTypeCloseCCW:
                    choices.push_back(_("Outside"));
                    choices.push_back(_("Inside"));
                    break;

                default:
                    choices.push_back(_("Outside or Left"));
                    choices.push_back(_("Inside or Right"));
                    break;
                }
            }

            choices.push_back(_("On"));

            int choice = int(CContourParams::eOn);
            switch (parent->m_params.m_tool_on_side)
            {
                case CContourParams::eRightOrInside:	choice = 1;
                        break;

                case CContourParams::eOn:	choice = 2;
                        break;

                case CContourParams::eLeftOrOutside:	choice = 0;
                        break;
            } // End switch

            list->push_back(new PropertyChoice(_("tool on side"), choices, choice, parent, on_set_tool_on_side));
        }
    }

    {
        int choice = int(parent->m_params.m_entry_move_type);
        std::list<wxString> choices;
        for (CContourParams::EntryMove_t move = CContourParams::ePlunge; move <= CContourParams::eRamp; move = CContourParams::EntryMove_t(int(move) + 1))
        {
            wxString description;
            description << move;
            choices.push_back( description );
        }

        list->push_back(new PropertyChoice(_("entry move type"), choices, choice, parent, on_set_entry_move_type));
    }

}

void CContourParams::WriteDefaultValues()
{
	CNCConfig config(ConfigPrefix());
	config.Write(_T("ToolOnSide"), (int) m_tool_on_side);
	config.Write(_T("EntryMoveType"), (int) m_entry_move_type);
}

void CContourParams::ReadDefaultValues()
{
	CNCConfig config(ConfigPrefix());
	config.Read(_T("ToolOnSide"), (int *) &m_tool_on_side, (int) eOn);
	config.Read(_T("EntryMoveType"), (int *) &m_entry_move_type, (int) ePlunge);
}

void CContour::WriteDefaultValues()
{
    m_params.WriteDefaultValues();
	CDepthOp::WriteDefaultValues();
}

void CContour::ReadDefaultValues()
{
    m_params.ReadDefaultValues();
	CDepthOp::ReadDefaultValues();
}

void CContourParams::WriteXMLAttributes(TiXmlNode *root)
{
	TiXmlElement * element;
	element = heeksCAD->NewXMLElement( "params" );
	heeksCAD->LinkXMLEndChild( root,  element );

	element->SetAttribute( "side", m_tool_on_side);
	element->SetAttribute( "entry_move_type", int(m_entry_move_type));
}

void CContourParams::ReadParametersFromXMLElement(TiXmlElement* pElem)
{
	if(pElem->Attribute("side")) pElem->Attribute("side", (int *) &m_tool_on_side);
	if (pElem->Attribute("entry_move_type")) pElem->Attribute("entry_move_type", (int *) &m_entry_move_type);
}


const wxBitmap &CContour::GetIcon()
{
	if(!m_active)return GetInactiveIcon();
	static wxBitmap* icon = NULL;
	if(icon == NULL)icon = new wxBitmap(wxImage(theApp.GetResFolder() + _T("/icons/drilling.png")));
	return *icon;
}

/* static */ bool CContour::Clockwise( const gp_Circ & circle )
{
	return(circle.Axis().Direction().Z() <= 0);
}




/**
	Return the U value that is this proportion of the way between the StartParameter()
	and the EndParameter().  The U value is some floating point value that represents
	how far along the Path one wants.  For a line, it's a distance but for an arc it's
	the number of radians.  We can't infer anything from the value but we can use it
	to move a proportion of the way along a Path object, no matter what type of object
	it is.
 */
Standard_Real CContour::Path::Proportion( const double proportion ) const
{
    if (StartParameter() < EndParameter())
    {
        return(((EndParameter() - StartParameter()) * proportion) + StartParameter());
    }
    else
    {
        return(((StartParameter() - EndParameter()) * proportion) + EndParameter());
    }
}


/**
	Generate a Python program that follows this ContiguousPath backwards and forwards but slopes down (in Z) at
	the tool's configured gradient until the 'end_z' depth is reached.

	We run along the Paths until we are half way between the current depth and the end_z depth in one direction (assuming
	the length of the ContiguousPath allows it) and then we reverse direction and come back.  By the time we reach our
	starting position (in X,Y) we should be at the end_z depth.  This sets us up to run around the whole ContiguousPath
	at this depth to make the real cut.

	If the length of the sketch and the tool's gradient mean that we can't get to our goal depths in a single pair of
	forwards/backwards passes then we just need to change direction as often as necessary.  i.e. if we have only a
	couple of short lines in our ContiguousPath then we just need to run backwards and forwards along those lines
	until we reach our required depth.
 */
Python CContour::ContiguousPath::Ramp( std::vector<CContour::Path>::iterator itPath, CMachineState *pMachineState, const double end_z )
{
	Python python;

	// We want to remember our original direction so we can run around the ContiguousPath following this Ramp() method.
    bool original_direction = m_is_forwards;
    double tolerance = heeksCAD->GetTolerance();
	std::vector<CContour::Path>::iterator itStart;

	// Get the gradient from the  tool's definition.
    CTool *pTool = CTool::Find( pMachineState->Tool() );
    if (pTool == NULL)
    {
        return(python); // empty.
    }

    double gradient = pTool->Gradient();
	if (gradient > 0) gradient *= -1.0;	// It MUST be negative to slope downwards.
	if (fabs(gradient) < tolerance)
	{
		// The tool definition is not setup correctly.
		return(python);	// empty.
	}

	if (CDouble(pMachineState->Location().Z()) <= CDouble(end_z))
	{
		// The machine's depth is already either at or lower than our destination depth.
		// There is no need to ramp down further.
		return(python);	// empty.
	}

	if (pMachineState->Location().XYDistance(itPath->StartPoint()) > tolerance)
	{
		// NOTE: We're assuming that we're above the workpiece before we get into this routine.
		python << _T("rapid(") << _T("x=") << itPath->StartPoint().X(true)
								<< _T(",y=") << itPath->StartPoint().Y(true)
								<< _T(")\n");
	}

	// Ramp to half way down the step-down distance.
	double goal_z = ((pMachineState->Location().Z() - end_z) / 2.0) + end_z;
    double initial_tool_depth = pMachineState->Location().Z();

    CNCPoint end_point(pMachineState->Location());
    end_point.SetZ(end_z); // This is where we want to be when we've finished.

    python << _T("comment('Ramp down to ") << end_z << _T(" at a gradient of ") << gradient << _T("')\n");

    // Ideally we would ramp down to half the step-down height in one direction and then come
    // back the other direction.  That way we would be back at the starting point by the
    // time we're at the step-down height.
    double half_way_down = ((initial_tool_depth - end_z) / 2.0) + end_z;
	goal_z = half_way_down;

    while (pMachineState->Location() != end_point)
    {
        double distance_remaining = pMachineState->Location().Z() - goal_z;

        // We need to go distance_remaining over the edge_length at no more
        // than the gradient.  See if we can do that within this edge's length.
        // If so, figure out what point marks the necessary depth.
        double depth_possible_with_this_edge = gradient * itPath->Length() * -1.0;   // positive number representing a depth (distance)
        if (CDouble(distance_remaining) < CDouble(depth_possible_with_this_edge))
        {
            // We don't need to traverse this whole edge before we're at depth.  Find
            // the point along this edge at which we will be at depth.
            // The FirstParameter() and the LastParameter() are not always lengths but they
            // do form a numeric representation of a distance along the element.  For lines
            // they are lengths and for arcs they are radians.  In any case, we can
            // use the proportion of the length to come up with a 'U' parameter for
            // this edge that indicates the point along the edge.

            double proportion = distance_remaining / depth_possible_with_this_edge;
            double U = itPath->Proportion(proportion);

            // The point_at_full_depth indicates where we will be when we are at depth.  Run
            // along the edge down to this point
            python << itPath->GCode( itPath->StartParameter(), U, pMachineState, goal_z );

            if (CDouble(proportion) < CDouble(1.0))
            {
                // Now reverse back to the start of this edge.
                Reverse();

                if (CDouble(pMachineState->Location().Z()) > CDouble(end_z))
                {
                    python << itPath->GCode( U, itPath->EndParameter(), pMachineState, goal_z - distance_remaining );
                }
                else
                {
                    python << itPath->GCode( U, itPath->EndParameter(), pMachineState, end_z);
                }
            }

            goal_z = end_z;   // Changed goal.
        }
        else
        {
            // This edge is not long enough for us to reach our goal depth.  Run all the way along this edge.
            double z_for_this_run = pMachineState->Location().Z() - depth_possible_with_this_edge;
            python << itPath->GCode( itPath->StartParameter(), itPath->EndParameter(), pMachineState, z_for_this_run );
        }

        if (pMachineState->Location() != end_point)
        {
            itPath = Next(itPath);
        }
    } // End while

    if (m_is_forwards != original_direction)
    {
        Reverse();  // Head back in the 'forwards' direction ready to machine the part.
    }

    python << _T("comment('end ramp')\n");
	return(python);
}




/**
	This is the main entry point for this class's GCode generation.  It's static so that
	other operations, like Inlay, can generate its own TopoDS_Wire objects and then
	use this code to generate GCode to follow it.
 */
/* static */ Python CContour::GCode(
	const TopoDS_Wire & wire,
	CMachineState *pMachineState,       // for both fixture and last_position.
	const double clearance_height,
	const double rapid_down_to_height,
	const double start_depth,
	const CContourParams::EntryMove_t entry_move_type )
{
	Python python;

	Paths paths;
	paths.Add(wire);

	// paths.GenerateSketches();

	python << paths.GCode( pMachineState, clearance_height, rapid_down_to_height, start_depth, entry_move_type );

	return(python);
}


/**
	Generate the Python program that will, in turn, generate GCode for all of our ContiguousPath objects.
 */
Python CContour::Paths::GCode(
	CMachineState *pMachineState,       // for both fixture and last_position.
	const double clearance_height,
	const double rapid_down_to_height,
	const double start_depth,
	const CContourParams::EntryMove_t entry_move_type )
{
	Python python;

	for (std::vector<ContiguousPath>::iterator itPath = m_contiguous_paths.begin(); itPath != m_contiguous_paths.end(); itPath++)
	{
		python << itPath->GCode( pMachineState, clearance_height, rapid_down_to_height, start_depth, entry_move_type );
	}

	return(python);
}


/**
	Adjust the vector of path objects so that the start point matches the X,Y values
	of the location specified.  If this is half way through the vector of path objects
	then the vector needs to be rearranged so that it's direction is still correct
	but the first item in the vector matches this location.  We normally do this so that
	the outline starts machining from the CNC machine's current location.
 */
bool CContour::ContiguousPath::SetStartPoint( CNCPoint location )
{
    double tolerance = heeksCAD->GetTolerance();

    // This will only work if the path is periodic (i.e. a closed shape)
    if (m_paths.begin()->StartPoint() != m_paths.rbegin()->EndPoint())
    {
        // We can't rearrange the list. See if we're already in position.
        if (m_paths.begin()->StartPoint().XYDistance(location) < tolerance)
        {
            return(true);
        }
        else if (m_paths.rbegin()->EndPoint().XYDistance(location) < tolerance)
        {
            Reverse();
            return(true);
        }
        else
        {
            // It's not periodic and the location doesn't line up
            // with either of the endpoints.

            return(false);
        }
    }
    // Look for this location in the start/end points of this path.
    for (std::vector<Path>::iterator itPath = m_paths.begin(); itPath != m_paths.end(); itPath++)
    {
        if (itPath->StartPoint().XYDistance(location) < tolerance)
        {
            // Rearrange the m_paths list so that this path is the first.
            std::vector<Path> new_paths;
            std::copy(itPath, m_paths.end(), std::inserter(new_paths, new_paths.begin()));
            std::copy(m_paths.begin(), itPath, std::inserter(new_paths, new_paths.end()));
            m_paths.clear();
            std::copy(new_paths.begin(), new_paths.end(), std::inserter(m_paths, m_paths.begin()));
            return(true);
        }
    }
	return(false);	// It must have been periodic but the location was not at any of the starting points.
}



/**
	Generate the Python program that will, in turn, generate GCode for this ContiguousPath.  We use
	the pMachineState to keep track of which fixture, which tool and the machine's current location.
	This all allows us to know whether or not we can get straight into machining this ContiguousPath
	or whether we need to raise up to clearance height, move into position and then back down before
	starting to machine this ContiguousPath.
 */
Python CContour::ContiguousPath::GCode(
	CMachineState *pMachineState,       // for both fixture and last_position.
	const double clearance_height,
	const double rapid_down_to_height,
	const double start_depth,
	const CContourParams::EntryMove_t entry_move_type )
{
	Python python;
	double tolerance = heeksCAD->GetTolerance();

    // Try to rearrange the list of paths so that the list starts at the machine's current
    // position.
	SetStartPoint( pMachineState->Location() );

	for (std::vector<Path>::iterator itPath = m_paths.begin(); itPath != m_paths.end(); itPath++)
	{
		if (pMachineState->Location() != itPath->StartPoint()) // in X,Y and Z
		{
		    if (pMachineState->Location().XYDistance(itPath->StartPoint()) > tolerance) // in X and Y only.
		    {
                // We need to move to the start BEFORE machining this line.
                // Move up above workpiece to relocate to the start of the next edge.
                python << _T("rapid(z=") << clearance_height / theApp.m_program->m_units << _T(")\n");

				// Look at the locations that have been machined earlier in this program.  If we can lower
				// the bit lower than the start_depth then we want to do so.
                CNCPoint previous_location;
                if (pMachineState->NearestLocation(pMachineState->Fixture(), itPath->StartPoint(), &previous_location))
                {
                    python << _T("rapid(x=") << itPath->StartPoint().X(true) << _T(", y=") << itPath->StartPoint().Y(true) << _T(")\n");
                    python << _T("rapid(z=") << rapid_down_to_height / theApp.m_program->m_units << _T(")\n");
                    python << _T("feed(z=") << previous_location.Z(true) << _T(")\n");
                    pMachineState->Location(previous_location);
                }
                else
                {
                    python << _T("rapid(x=") << itPath->StartPoint().X(true) << _T(", y=") << itPath->StartPoint().Y(true) << _T(")\n");
                    python << _T("rapid(z=") << rapid_down_to_height / theApp.m_program->m_units << _T(")\n");
                    python << _T("feed(z=") << start_depth / theApp.m_program->m_units << _T(")\n");
                    CNCPoint where(itPath->StartPoint());
                    where.SetZ(start_depth / theApp.m_program->m_units);
                    pMachineState->Location(where);
                }
		    }

            switch (entry_move_type)
            {
                case CContourParams::ePlunge:
                    python << _T("feed(z=") << itPath->StartPoint().Z(true) << _T(")\n");
                    pMachineState->Location().SetZ( itPath->StartPoint().Z() );;
                    break;

                case CContourParams::eRamp:
                {
                    python << Ramp( itPath, pMachineState, itPath->StartPoint().Z() );
                }
                    break;
            } // End switch
		}

        python << itPath->GCode(pMachineState, itPath->EndPoint().Z());
	}

	return(python);
}





/**
	Machine the whole edge.
 */
Python CContour::Path::GCode( CMachineState *pMachineState, const double end_z )
{
    return(GCode( StartParameter(), EndParameter(), pMachineState, end_z ));
}

/**
	Machine part of the edge (i.e. from start_u to end_u) and down to end_z depth.
 */
Python CContour::Path::GCode( Standard_Real start_u, Standard_Real end_u, CMachineState *pMachineState, const double end_z )
{
	Python python;

	double tolerance = heeksCAD->GetTolerance();

	BRepAdaptor_Curve curve(m_edge);
	GeomAbs_CurveType curve_type = curve.GetType();

	CNCPoint start;
    gp_Vec vector_start;
    curve.D1(start_u, start, vector_start);

    CNCPoint end;
    gp_Vec vector_end;
    curve.D1(end_u, end, vector_end);

	start.SetZ( pMachineState->Location().Z() );
	end.SetZ(end_z);


    if (pMachineState->Location() == end)
    {
        // We are already at the end point.  Just return.
        return python;
    }

    if (pMachineState->Location() != start)
    {
        python << _T("comment('WARNING: Asked to move machine from start point but it is not there')\n");
    }

  	switch(curve_type)
	{
		case GeomAbs_Line:
			// make a line
		{
			std::list<CNCPoint> points;
			points.push_back(start);
			points.push_back(end);

			for (std::list<CNCPoint>::iterator itPoint = points.begin(); itPoint != points.end(); itPoint++)
            {
                if (itPoint->Distance(pMachineState->Location()) > tolerance)
                {
                    python << _T("feed(x=") << itPoint->X(true) << _T(", y=") << itPoint->Y(true) << _T(", z=") << itPoint->Z(true) << _T(")\n");
                    pMachineState->Location(*itPoint);
                }
            } // End for
		}
		break;

		case GeomAbs_Circle:
		if ((pMachineState->Fixture().m_params.m_xz_plane == 0.0) && (pMachineState->Fixture().m_params.m_yz_plane == 0.0))
		{
			gp_Circ circle = curve.Circle();
            CNCPoint point(end);
            CNCPoint centre( circle.Location() );
            bool l_bClockwise = Clockwise(circle);
            if (m_is_forwards == false) l_bClockwise = ! l_bClockwise;

            std::list<CNCPoint> points;
            double period = curve.Period();
            double u = start_u;
			if (start_u < end_u)
			{
				double step_down = (pMachineState->Location().Z() - end_z) / ((end_u - start_u) / (period/4.0));
				double z = pMachineState->Location().Z();

				for (u = start_u; u <= end_u; u += (period/4.0))
				{
					gp_Pnt p;
					gp_Vec v;
					curve.D1(u, p, v);
					p.SetZ(z);
					z -= step_down;
					points.push_back( p );
				}
			}
			else
			{
				double step_down = (pMachineState->Location().Z() - end_z) / ((start_u - end_u) / (period/4.0));
				double z = pMachineState->Location().Z();

				for (u = start_u; u >= end_u; u -= (period/4.0))
				{
					gp_Pnt p;
					gp_Vec v;
					curve.D1(u, p, v);
					p.SetZ(z);
					z -= step_down;
					points.push_back( p );
				}
			}

            if ((points.size() > 0) && (*points.rbegin() != EndPoint()))
            {
                CNCPoint point(EndPoint());
                point.SetZ(end_z);
                points.push_back( point );
            }

            for (std::list<CNCPoint>::iterator itPoint = points.begin(); itPoint != points.end(); itPoint++)
            {
                if (itPoint->Distance(pMachineState->Location()) > tolerance)
                {
                    python << (l_bClockwise?_T("arc_cw("):_T("arc_ccw(")) << _T("x=") << itPoint->X(true) << _T(", y=") << itPoint->Y(true) << _T(", z=") << itPoint->Z(true) << _T(", ")
                        << _T("i=") << centre.X(true) << _T(", j=") << centre.Y(true);
                    python << _T(")\n");
                    pMachineState->Location(*itPoint);
                }
            } // End for

            break; // Allow the circle option to fall through to the 'default' option if
                    // there is any rotation in the YZ and/or XZ planes.
        }

		case GeomAbs_BSplineCurve:
			if ((curve.BSpline() != NULL) &&		// The curve adaptor can interpret this as a BSpline.
				(curve.BSpline()->IsRational()) &&  // the 'R' in NURBS stands for 'Rational'
				(theApp.m_program->m_machine.m_nurbs_supported) && // The controller can handle NURBS tool paths.
				(fabs(pMachineState->Location().Z() - end_z) < tolerance))	// We can't slope down using a BSPline definition.
			{
				// It's a (hopefully non-uniform) Rational BSpline Curve (NURBS curve).  We can handle this with EMC2 without having to
				// convert it into lines or arcs first.  i.e. we can represent it to EMC2 using G5.2 and G5.3 along with X, Y, P and L
				// arguments.  If we do it that way, the EMC2 routine will interpolate at the machine's best resolution rather than
				// anythin we configure here.

				TColStd_Array1OfReal weights(1, curve.BSpline()->NbPoles());
				curve.BSpline()->Weights(weights);

				if (weights.Length() > 0)
				{
					static int id=0;

					for (Standard_Integer i=1; i <= weights.Length(); i++)
					{
						// Standard_Real weight = weights.Value(i);
						gp_Pnt pole = curve.BSpline()->Pole(i);

						if (i == 1)
						{
							python << _T("nurbs_begin_definition(id=") << ++id << _T(", degree=") << curve.BSpline()->Degree() << _T(",")
									<< _T("x=") << pole.X() / theApp.m_program->m_units << _T(",")
									<< _T("y=") << pole.Y() / theApp.m_program->m_units << _T(",")
									<< _T("weight=") << weights.Value(i) << _T(")\n");
						}
						else
						{
							python << _T("nurbs_add_pole(id=") << id << _T(", ")
								<< _T("x=") << pole.X() / theApp.m_program->m_units << _T(",")
								<< _T("y=") << pole.Y() / theApp.m_program->m_units << _T(",")
								<< _T("weight=") << weights.Value(i) << _T(")\n");
						}
					}
					python << _T("nurbs_end_definition(id=") << id << _T(")\n");
				}

				break;
			}
			// If it is not rational or NURBS are not supported by this machine's controller
			// then let it fall through to the 'default' option to be 'stroked' into lines.
			// i.e. do NOT add a 'break' statement here.  Fall through instead.

		default:
		{
			// make lots of small lines
			BRepBuilderAPI_MakeEdge edge_builder(curve.Curve().Curve(), start_u, end_u);
			TopoDS_Edge edge(edge_builder.Edge());

			BRepTools::Clean(edge);
			BRepMesh::Mesh(edge, max_deviation_for_spline_to_arc);

			TopLoc_Location L;
			Handle(Poly_Polygon3D) Polyg = BRep_Tool::Polygon3D(edge, L);
			if (!Polyg.IsNull()) {
				const TColgp_Array1OfPnt& Points = Polyg->Nodes();
				Standard_Integer po;
				int i = 0;
				std::list<CNCPoint> interpolated_points;
				for (po = Points.Lower(); po <= Points.Upper(); po++, i++) {
					CNCPoint p = (Points.Value(po)).Transformed(L);
					interpolated_points.push_back(p);
				} // End for

				// See if we should go from the start to the end or the end to the start.
				if (start_u > end_u)
				{
					// We need to go from the end to the start.  Reverse the point locations to
					// make this easier.

					interpolated_points.reverse();
				} // End if - then


				double step_down = (pMachineState->Location().Z() - end_z) / interpolated_points.size();
				double z = pMachineState->Location().Z();
				for (std::list<CNCPoint>::iterator itPoint = interpolated_points.begin(); itPoint != interpolated_points.end(); itPoint++)
				{
					itPoint->SetZ( z );
					z -= step_down;

					if (*itPoint != pMachineState->Location())
					{
						python << _T("feed(x=") << itPoint->X(true) << _T(", y=") << itPoint->Y(true) << _T(", z=") << itPoint->Z(true) << _T(")\n");
						pMachineState->Location(*itPoint);
					} // End if - then
				} // End for
			} // End if - then
		}
		break;
	} // End switch

	return(python);
}





/**
	This method is called when the CAD operator presses the Python button.  This method generates
	Python source code whose job will be to generate RS-274 GCode.  It's done in two steps so that
	the Python code can be configured to generate GCode suitable for various CNC interpreters.
 */
Python CContour::AppendTextToProgram( CMachineState *pMachineState )
{
	Python python;

	ReloadPointers();

	python << CDepthOp::AppendTextToProgram( pMachineState );

	unsigned int number_of_bad_sketches = 0;
	double tolerance = heeksCAD->GetTolerance();

	CTool *pTool = CTool::Find( m_tool_number );
	if (! pTool)
	{
		return(python);
	}

	std::list<HeeksObj *> solids_to_avoid;

	for (HeeksObj *object = GetFirstChild(); object != NULL; object = GetNextChild())
    {
		if ((object->GetType() == StlSolidType) || (object->GetType() == SolidType))
		{
			solids_to_avoid.push_back(object);
		}
	}


    for (HeeksObj *object = GetFirstChild(); object != NULL; object = GetNextChild())
    {
		if (object->GetType() != SketchType)
		{
			continue;
		}

        std::list<TopoDS_Shape> wires;
        if (! heeksCAD->ConvertSketchToFaceOrWire( object, wires, false))
        {
            number_of_bad_sketches++;
        } // End if - then
        else
        {
            // The wire(s) represent the sketch objects for a tool path.
            if (object->GetShortString() != NULL)
            {
                python << _T("comment(") << PythonString(object->GetShortString()).c_str() << _T(")\n");
            }

            try {
                for(std::list<TopoDS_Shape>::iterator It2 = wires.begin(); It2 != wires.end(); It2++)
                {
                    TopoDS_Shape& wire_to_fix = *It2;
                    ShapeFix_Wire fix;
                    fix.Load( TopoDS::Wire(wire_to_fix) );
                    fix.FixReorder();

                    TopoDS_Shape wire = fix.Wire();

					// Rotate and translate the wire to align with the fixture (if necessary)
                    pMachineState->Fixture().Adjustment(wire);

                    BRepOffsetAPI_MakeOffset offset_wire(TopoDS::Wire(wire));

                    // Now generate a toolpath along this wire.
                    std::list<double> depths = GetDepths();

                    for (std::list<double>::iterator itDepth = depths.begin(); itDepth != depths.end(); itDepth++)
                    {
                        double radius = pTool->CuttingRadius(false,m_depth_op_params.m_start_depth - *itDepth);

                        if (m_params.m_tool_on_side == CContourParams::eLeftOrOutside) radius *= +1.0;
                        if (m_params.m_tool_on_side == CContourParams::eRightOrInside) radius *= -1.0;
                        if (m_params.m_tool_on_side == CContourParams::eOn) radius = 0.0;

                        TopoDS_Wire tool_path_wire(TopoDS::Wire(wire));

                        double offset = radius;
                        if (offset < 0) offset *= -1.0;

                        if (offset > tolerance)
                        {
                            offset_wire.Perform(radius);
                            if (! offset_wire.IsDone())
                            {
                                break;
                            }
                            tool_path_wire = TopoDS::Wire(offset_wire.Shape());
                        }

                        if ((m_params.m_tool_on_side == CContourParams::eOn) || (offset > tolerance))
                        {
                            gp_Trsf matrix;

                            matrix.SetTranslation( gp_Vec( gp_Pnt(0,0,0), gp_Pnt( 0,0,*itDepth)));
                            BRepBuilderAPI_Transform transform(matrix);
                            transform.Perform(tool_path_wire, false); // notice false as second parameter
                            tool_path_wire = TopoDS::Wire(transform.Shape());

/*
							HeeksObj *tool_path_sketch = heeksCAD->NewSketch();
							if (heeksCAD->ConvertWireToSketch(tool_path_wire, tool_path_sketch, 0.001))
							{
								tool_path_sketch->OnEditString(_T("tool path sketch"));
								heeksCAD->Add(tool_path_sketch, NULL);
							}
*/
                            python << GCode(	tool_path_wire,
                                                pMachineState,
                                                m_depth_op_params.ClearanceHeight(),
                                                m_depth_op_params.m_rapid_safety_space,
                                                m_depth_op_params.m_start_depth,
                                                m_params.m_entry_move_type );
                        } // End if - then
                    } // End for
                } // End for
            } // End try
            catch (Standard_Failure & error) {
                (void) error;	// Avoid the compiler warning.
                Handle_Standard_Failure e = Standard_Failure::Caught();
				wxMessageBox(Ctt(e->GetMessageString()));
                number_of_bad_sketches++;
            } // End catch
        } // End if - else
    } // End for

    if (pMachineState->Location().Z() < (m_depth_op_params.ClearanceHeight() / theApp.m_program->m_units))
    {
        // Move up above workpiece to relocate to the start of the next operation.
        python << _T("rapid(z=") << m_depth_op_params.ClearanceHeight() / theApp.m_program->m_units << _T(")\n");

        CNCPoint where(pMachineState->Location());
        where.SetZ(m_depth_op_params.ClearanceHeight() / theApp.m_program->m_units);
        pMachineState->Location(where);
    }

    if (number_of_bad_sketches > 0)
    {
        wxString message;
        message << _("Failed to create contours around ") << number_of_bad_sketches << _(" sketches");
        wxMessageBox(message);
    }

	return(python);
}




/**
	This is the Graphics Library Commands (from the OpenGL set).  This method calls the OpenGL
	routines to paint the drill action in the graphics window.  The graphics is transient.

	Part of its job is to re-paint the elements that this CContour object refers to so that
	we know what CAD objects this CNC operation is referring to.
 */
void CContour::glCommands(bool select, bool marked, bool no_color)
{
	CDepthOp::glCommands( select, marked, no_color );
}




void CContour::GetProperties(std::list<Property *> *list)
{
	m_params.GetProperties(this, list);
	CDepthOp::GetProperties(list);
}

HeeksObj *CContour::MakeACopy(void)const
{
	return new CContour(*this);
}

void CContour::CopyFrom(const HeeksObj* object)
{
	operator=(*((CContour*)object));
}

bool CContour::CanAddTo(HeeksObj* owner)
{
	return ((owner != NULL) && (owner->GetType() == OperationsType));
}

void CContour::WriteXML(TiXmlNode *root)
{
	TiXmlElement * element = heeksCAD->NewXMLElement( "Contour" );
	heeksCAD->LinkXMLEndChild( root,  element );
	m_params.WriteXMLAttributes(element);

    if (m_symbols.size() > 0)
    {
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
    } // End if - then

	WriteBaseXML(element);
}

// static member function
HeeksObj* CContour::ReadFromXMLElement(TiXmlElement* element)
{
	CContour* new_object = new CContour;
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
	This method adjusts any parameters that don't make sense.  It should report a list
	of changes in the list of strings.
 */
std::list<wxString> CContour::DesignRulesAdjustment(const bool apply_changes)
{
	std::list<wxString> changes;

	// Make some special checks if we're using a chamfering bit.
	if (m_tool_number > 0)
	{
		CTool *pTool = (CTool *) CTool::Find( m_tool_number );
		if (pTool != NULL)
		{
			if ((pTool->m_params.m_type != CToolParams::eEndmill) &&
				(pTool->m_params.m_type != CToolParams::eBallEndMill) &&
				(pTool->m_params.m_type != CToolParams::eChamfer) &&
				(pTool->m_params.m_type != CToolParams::eExtrusion) &&
				(pTool->m_params.m_type != CToolParams::eEngravingTool) &&
				(pTool->m_params.m_type != CToolParams::eSlotCutter))
			{
				wxString change;
				change << DesignRulesPreamble() << _("found with ") << pTool->m_params.m_type;
				changes.push_back(change);
			}

			if ((m_params.m_entry_move_type == CContourParams::eRamp) && (pTool->m_params.m_gradient >= 0.0))
			{
				wxString change;
				change << DesignRulesPreamble() << _("Entry move type RAMP selected but ") << pTool->m_title << _(" gradient >= 0.0");
				changes.push_back(change);
			}
		} // End if - then
	} // End if - then

	std::list<wxString> extra_changes = CDepthOp::DesignRulesAdjustment(apply_changes);
	std::copy( extra_changes.begin(), extra_changes.end(), std::inserter( changes, changes.end() ));

	return(changes);

} // End DesignRulesAdjustment() method


/**
    This method returns TRUE if the type of symbol is suitable for reference as a source of location
 */
bool CContour::CanAdd( HeeksObj *object )
{
    if (object == NULL) return(false);

    switch (object->GetType())
    {
        case CircleType:
        case SketchType:
		case LineType:
		case SplineType:
		case FixtureType:
		case StlSolidType:
		case SolidType:
            return(true);

        default:
            return(false);
    }
}


void CContour::GetTools(std::list<Tool*>* t_list, const wxPoint* p)
{
    CDepthOp::GetTools( t_list, p );
}

static void on_set_spline_deviation(double value, HeeksObj* object)
{
	CContour::max_deviation_for_spline_to_arc = value;
	CContour::WriteToConfig();
}

// static
void CContour::GetOptions(std::list<Property *> *list)
{
	list->push_back ( new PropertyDouble ( _("Contour spline deviation"), max_deviation_for_spline_to_arc, NULL, on_set_spline_deviation ) );
}


void CContour::ReloadPointers()
{
	for (Symbols_t::iterator l_itSymbol = m_symbols.begin(); l_itSymbol != m_symbols.end(); l_itSymbol++)
	{
		HeeksObj *object = heeksCAD->GetIDObject( l_itSymbol->first, l_itSymbol->second );
		if (object != NULL)
		{
			Add( object, NULL );
		}
	}

	m_symbols.clear();
}


CContour::CContour( const CContour & rhs ) : CDepthOp( rhs )
{
    m_params = rhs.m_params;
    m_symbols.clear();
	std::copy( rhs.m_symbols.begin(), rhs.m_symbols.end(), std::inserter( m_symbols, m_symbols.begin() ) );
}

CContour & CContour::operator= ( const CContour & rhs )
{
	if (this != &rhs)
	{
		m_params = rhs.m_params;
		m_symbols.clear();
		std::copy( rhs.m_symbols.begin(), rhs.m_symbols.end(), std::inserter( m_symbols, m_symbols.begin() ) );

		CDepthOp::operator=( rhs );
	}

	return(*this);
}

bool CContour::operator==( const CContour & rhs ) const
{
	if (m_params != rhs.m_params) return(false);

	return(CDepthOp::operator==(rhs));
}

bool CContour::IsDifferent(HeeksObj* other)
{
	return(! (*this == (*((CContour *) other))));
}

/**
	Try to add this path to the contiguous list of paths.  If it doesn't
	connect with the existing list of paths then it returns failure.
 */
bool CContour::ContiguousPath::Add(CContour::Path path)
{
	if (m_paths.size() == 0)
	{
		m_paths.push_back(path);
		return(true);
	}
	else if (m_paths.begin()->StartPoint() == path.StartPoint())
	{
		path.Reverse();
		m_paths.insert(m_paths.begin(), path);
		return(true);
	}
	else if (m_paths.begin()->StartPoint() == path.EndPoint())
	{
		m_paths.insert(m_paths.begin(), path);
		return(true);
	}
	else if (m_paths.rbegin()->EndPoint() == path.StartPoint())
	{
		m_paths.push_back(path);
		return(true);
	}
	else if (m_paths.rbegin()->EndPoint() == path.EndPoint())
	{
		path.Reverse();
		m_paths.push_back(path);
		return(true);
	}
	else
	{
		return(false);
	}
}

void CContour::Paths::Add(const TopoDS_Edge edge)
{
	Add(Path(edge));
}

void CContour::Paths::Add(const TopoDS_Wire wire)
{
	for(BRepTools_WireExplorer expEdge(wire); expEdge.More(); expEdge.Next())
	{
		Add(Path(expEdge.Current()));
	}
}


/**
	Add a single Path (really a TopoDS_Edge) into either an existing ContiguousPath
	object or a new ContiguousPath object.  If the addition of this Path into an
	existing one means that two ContiguousPath objects can now be joined, then join
	them to form a single, longer, ContiguousPath object.  We always want our list
	of ContiguousPath objects to be connected if possible.
 */
void CContour::Paths::Add(const Path path)
{
	bool added = false;

	// See if we can add it to any of the existing contiguous paths.
	for (std::vector<ContiguousPath>::iterator itContiguousPath = m_contiguous_paths.begin(); (! added) && (itContiguousPath != m_contiguous_paths.end()); itContiguousPath++)
	{
		if (itContiguousPath->Add(path))
		{
			added = true;
		}
	}

	if (! added)
	{
		ContiguousPath new_path;
		new_path.Add(path);
		m_contiguous_paths.push_back(new_path);
		return;
	}
	else
	{
		// It was added to one of the existing paths.  We need to check to see if this is the
		// missing link joining any of the existing paths together.

		bool join_found = false;
		do {
			join_found = false;

			for (std::vector<ContiguousPath>::iterator lhs = m_contiguous_paths.begin();
				 (join_found == false) && (lhs != m_contiguous_paths.end()); lhs++)
			{
				for (std::vector<ContiguousPath>::iterator rhs = m_contiguous_paths.begin();
				 (join_found == false) && (rhs != m_contiguous_paths.end()); rhs++)
				{
					if (lhs == rhs) continue;

					if (lhs->StartPoint() == rhs->EndPoint())
					{
						join_found = true;
						*rhs += *lhs;
						lhs = m_contiguous_paths.erase(lhs);
						break;
					}
					else if (lhs->EndPoint() == rhs->StartPoint())
					{
						join_found = true;
						*lhs += *rhs;
						rhs = m_contiguous_paths.erase(rhs);
						break;
					}
					else if (lhs->StartPoint() == rhs->StartPoint())
					{
						join_found = true;
						rhs->Reverse();
						*lhs += *rhs;
						rhs = m_contiguous_paths.erase(rhs);
						break;
					}
					else if (lhs->EndPoint() == rhs->EndPoint())
					{
						join_found = true;
						rhs->Reverse();
						*lhs += *rhs;
						rhs = m_contiguous_paths.erase(rhs);
						break;
					}
				} // End for
			} // End for
		} while (join_found);
	}
}


/**
	Append all Path objects within the rhs contiguous path into our own contiguous path.
	We use the Add() routine to add the Path objects at either the beginning or the end of this
	contiguous path as necessary.
 */
CContour::ContiguousPath & CContour::ContiguousPath::operator+=( ContiguousPath &rhs )
{
	for (std::vector<Path>::iterator itPath = rhs.m_paths.begin(); itPath != rhs.m_paths.end(); itPath++)
	{
		Add(*itPath);
	}

	return(*this);
}


/**
	Generate a Sketch object representing all the ContiguousPath objects we contain.
 */
bool CContour::Paths::GenerateSketches() const
{
    for (std::vector<ContiguousPath>::const_iterator itPath = m_contiguous_paths.begin(); itPath != m_contiguous_paths.end(); itPath++)
    {
        HeeksObj *object = itPath->Sketch();
        if (object) heeksCAD->Add( object, NULL );
    }
    return(true);
}


/**
	Generate a Sketch representing all the Path objects we contain.
 */
HeeksObj *CContour::ContiguousPath::Sketch() const
{
    HeeksObj *sketch = heeksCAD->NewSketch();
    for (std::vector<Path>::const_iterator itPath = m_paths.begin(); itPath != m_paths.end(); itPath++)
    {
        HeeksObj *object = itPath->Sketch();
        if (object) sketch->Add( object, NULL );
    }
    return(sketch);
}


/**
	Generate a Sketch representing this Path.
 */
HeeksObj *CContour::Path::Sketch() const
{
	Python python;

	double tolerance = heeksCAD->GetTolerance();

	BRepAdaptor_Curve curve(m_edge);
	GeomAbs_CurveType curve_type = curve.GetType();

	CNCPoint start;
    gp_Vec vector_start;
    curve.D1(StartParameter(), start, vector_start);

    CNCPoint end;
    gp_Vec vector_end;
    curve.D1(EndParameter(), end, vector_end);

  	switch(curve_type)
	{
		case GeomAbs_Line:
			// make a line
		{
		    double s[3], e[3];
		    start.ToDoubleArray(s);
		    end.ToDoubleArray(e);

		    return( heeksCAD->NewLine( s, e ) );
		}
		break;

		case GeomAbs_Circle:
		{
			gp_Circ circle = curve.Circle();
            CNCPoint centre( circle.Location() );
            bool l_bClockwise = Clockwise(circle);
            if (m_is_forwards == false) l_bClockwise = ! l_bClockwise;

            if (curve.IsPeriodic())
			{
				// It's a full circle.
				// gp_Circ circ(gp_Ax2(Start(),gp_Dir(0,0,-1)), (m_aperture.OutsideDiameter()/2.0));
				// return( new HCircle( circ, &wxGetApp().current_color ) );
			}
			else
			{
				// It's an arc
                double start[3]; StartPoint().ToDoubleArray(start);
                double end[3];   EndPoint().ToDoubleArray(end);
                double centre[3]; CNCPoint(circle.Location()).ToDoubleArray(centre);
                double up[3];

                up[0] = 0;
                up[1] = 0;
                up[2] = (Clockwise(circle)?-1:+1);

				return(heeksCAD->NewArc(start, end, centre, up ));
			}
		}
        break;
	} // End switch

	return(NULL);

}


/**
	Aggregate the lengths of all Path objects we contain.
 */
Standard_Real CContour::ContiguousPath::Length() const
{
	Standard_Real length = 0.0;
	for (std::vector<Path>::const_iterator itPath = m_paths.begin(); itPath != m_paths.end(); itPath++)
	{
		length += itPath->Length();
	}

	return(length);
}

/**
	This routine reverses the direction of each path object as well as sets a boolean
	indicating the direction to use when iterating through the Path objects.  The order
	of the actual list of Path objects is not changed.  Once this is done, the Ramp()
	method is always able to move from StartPoint() to EndPoint() no matter whether it's
	going forwards or backwards through the list of Path objects.
 */
void CContour::ContiguousPath::Reverse()
{
	for (std::vector<Path>::iterator itPath = m_paths.begin(); itPath != m_paths.end(); itPath++)
	{
		itPath->Reverse();
	}
	m_is_forwards = ! m_is_forwards;
}


bool CContour::ContiguousPath::Periodic() const
{
    if (StartPoint() == EndPoint()) return(true);
    return(false);
}

/**
	If we're iterating through the ContiguousPath, this routine figures out what the next
	Path in the chain is.  It handles going forwards and backwards through the list of
	Path objects.  It also handles whether the ContiguousPath is Periodic (i.e. a closed
	shape) or not.  The Ramp() method uses this method to iterate backwards and forwards
	through the Path objects moving slowly downwards in Z until the necessary depths are
	achieved.
 */
std::vector<CContour::Path>::iterator CContour::ContiguousPath::Next(std::vector<CContour::Path>::iterator itPath)
{
    if (m_paths.size() == 1)
    {
        Reverse();
        return(itPath);
    }

    if (m_is_forwards)
    {
        if (itPath == m_paths.end())
        {
            if (Periodic())
            {
                itPath = m_paths.begin();
            }
            else
            {
                Reverse();
            }
        }
        else
        {
            itPath++;
            if (itPath == m_paths.end())
            {
                if (Periodic())
                {
                    itPath = m_paths.begin();
                }
                else
                {
                    Reverse();
                    itPath = m_paths.end() - 1;
                }
            }
        }
        return(itPath);
    }
    else
    {
        // Moving backwards.
        if (itPath == m_paths.begin())
        {
            if (Periodic())
            {
                itPath = m_paths.end() - 1;
            }
            else
            {
                Reverse();
            }
        }
        else
        {
            itPath--;
        }

        return(itPath);
    }
}


CNCPoint CContour::ContiguousPath::StartPoint() const
{
	if (m_paths.size() == 0) return(CNCPoint(0.0,0.0,0.0));
	if (m_is_forwards)
	{
	    return(m_paths.begin()->StartPoint());
	}
	else
	{
	    return((m_paths.end()-1)->StartPoint());
	}
}

CNCPoint CContour::ContiguousPath::EndPoint() const
{
	if (m_paths.size() == 0) return(CNCPoint(0.0,0.0,0.0));
	if (m_is_forwards)
	{
	    return((m_paths.end()-1)->EndPoint());
	}
	else
	{
	    return(m_paths.begin()->EndPoint());
	}
}


Standard_Real CContour::Path::StartParameter() const
{
	if (m_is_forwards)
	{
		return(m_curve.FirstParameter());
	}
	else
	{
		return(m_curve.LastParameter());
	}
}

Standard_Real CContour::Path::EndParameter() const
{
	if (m_is_forwards)
	{
		return(m_curve.LastParameter());
	}
	else
	{
		return(m_curve.FirstParameter());
	}
}
