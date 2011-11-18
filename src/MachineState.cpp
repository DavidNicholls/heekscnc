// MachineState.cpp
/*
 * Copyright (c) 2009, Dan Heeks
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include <stdafx.h>
#include "MachineState.h"
#include "CTool.h"
#include "CNCPoint.h"
#include "Program.h"
#include "AttachOp.h"

class CFixture;

#ifdef HEEKSCNC
#define PROGRAM theApp.m_program
#else
#define PROGRAM heeksCNC->GetProgram()
#endif

CMachineState::CMachineState(CMachine *pMachine, CFixture fixture) : m_fixture(fixture), m_pMachine(pMachine)
{
        m_location = CNCPoint(0.0, 0.0, 0.0);
		m_location_is_known = false;
        m_tool_number = 0;  // No tool assigned.
        m_fixture_has_been_set = true;
		m_attached_to_surface = NULL;
		PROGRAM->m_active_machine_state = this;
}

CMachineState::~CMachineState()
{
	PROGRAM->m_active_machine_state = NULL;
}

CMachineState::CMachineState(CMachineState & rhs) : m_fixture(rhs.Fixture())
{
    *this = rhs;  // Call the assignment operator
	PROGRAM->m_active_machine_state = this;
}

CMachineState & CMachineState::operator= ( CMachineState & rhs )
{
    if (this != &rhs)
    {
        m_location = rhs.Location();
        m_fixture = rhs.Fixture();
        m_tool_number = rhs.Tool();
        m_fixture_has_been_set = rhs.m_fixture_has_been_set;
		m_attached_to_surface = rhs.m_attached_to_surface;
		m_previous_locations.clear();
		std::copy( rhs.m_previous_locations.begin(), rhs.m_previous_locations.end(),
			std::inserter( m_previous_locations, m_previous_locations.begin() ));
    }

    return(*this);
}

bool CMachineState::operator== ( const CMachineState & rhs ) const
{
    if (m_fixture != rhs.Fixture()) return(false);
    if (m_tool_number != rhs.m_tool_number) return(false);
    if(m_attached_to_surface != rhs.m_attached_to_surface) return false;

    // Don't include the location in the state check.  Moving around the machine is nothing to reset ourselves
    // over.

    // Don't worry about m_fixture_has_been_set

    return(true);
}

/**
    The machine's tool has changed.  Issue the appropriate GCode if necessary.
 */
Python CMachineState::Tool( const int new_tool )
{
    Python python;

    if (m_tool_number != new_tool)
    {
        m_tool_number = new_tool;

        // Select the right tool.
        CTool *pTool = (CTool *) CTool::Find(new_tool);
        if (pTool != NULL)
        {
			if ((PROGRAM->m_machine.m_safety_height_defined) && (pTool->m_params.m_type != CToolParams::eTouchProbe) && (pTool->m_params.m_type != CToolParams::eToolLengthSwitch))
			{
				switch (PROGRAM->m_machine.m_tool_change_movement)
				{
				case CMachine::ToolChangeMovement_G28:
				case CMachine::ToolChangeMovement_G28_and_probe:
					python << _T("comment('Move to safe Z height and then to G28 location')\n");
					python << _T("rapid(z=") << PROGRAM->m_machine.m_safety_height / PROGRAM->m_units << _T(", machine_coordinates=True)\n");
					python << _T("predefined_position(type='G28')\n");
					break;

				case CMachine::ToolChangeMovement_G30:
				case CMachine::ToolChangeMovement_G30_and_probe:
					python << _T("comment('Move to safe Z height and then to G30 location')\n");
					python << _T("rapid(z=") << PROGRAM->m_machine.m_safety_height / PROGRAM->m_units << _T(", machine_coordinates=True)\n");
					python << _T("predefined_position(type='G30')\n");
					break;

				case CMachine::ToolChangeMovement_ExplicitlyDefined:
				case CMachine::ToolChangeMovement_ExplicitlyDefined_and_probe:
					python << _T("comment('Move to safe Z height')\n");
					python << _T("rapid(z=") << PROGRAM->m_machine.m_safety_height / PROGRAM->m_units << _T(", machine_coordinates=True)\n");
					python << _T("rapid(z=") << PROGRAM->m_machine.m_explicit_tool_change_position[2] << _T(", machine_coordinates=True)\n");
					python << _T("rapid(x=") << PROGRAM->m_machine.m_explicit_tool_change_position[0] << _T(",y=") << PROGRAM->m_machine.m_explicit_tool_change_position[1] << _T(", machine_coordinates=True)\n");
					break;

				default:
					// Don't do any special movements before changing tools.
					break;
				} // End switch
			} // End if - then

            python << _T("comment(") << PythonString(_T("tool change to ") + pTool->m_title) << _T(")\n");
            python << _T("tool_change( id=") << new_tool << _T(")\n");
			if(m_attached_to_surface)
			{
				python << _T("nc.nc.creator.cutter = ") << pTool->OCLDefinition(m_attached_to_surface) << _T("\n");
			}


			if ((PROGRAM->m_machine.m_safety_height_defined) &&
				(pTool->m_params.m_type != CToolParams::eTouchProbe) &&
				(pTool->m_params.m_type != CToolParams::eToolLengthSwitch) &&
				(this->Fixture().m_params.m_touch_off_point_defined))
			{
				switch (PROGRAM->m_machine.m_tool_change_movement)
				{
				case CMachine::ToolChangeMovement_G28_and_probe:
				case CMachine::ToolChangeMovement_G30_and_probe:
				case CMachine::ToolChangeMovement_ExplicitlyDefined_and_probe:
					{
                        wxString fixture_name;
                        fixture_name << m_fixture.m_coordinate_system_number;

                        python << _T("rapid(z=") << PROGRAM->m_machine.m_safety_height / PROGRAM->m_units << _T(", machine_coordinates=True)\n");
						python << _T("measure_and_offset_tool(distance=") << PROGRAM->m_machine.m_tool_change_probe_depth / PROGRAM->m_units << _T(", ")
                                << _T("switch_offset_variable_name='<tool_length_switch_offset>', ")
                                << _T("fixture_offset_variable_name='<") << fixture_name << _T("_fixture_height_offset>', ")
                                << _T("feed_rate=") << PROGRAM->m_machine.m_tool_change_probe_feed_rate / PROGRAM->m_units << _T(")\n");

						// If we have a safe height defined, move there now.  We don't want to break this new
						// bit when we move into position for machining.

                        python << _T("comment('Move back up to safety height before continuing with program')\n");
                        python << _T("rapid(z=") << (PROGRAM->m_machine.m_safety_height / PROGRAM->m_units) << _T(", machine_coordinates=True)\n");
						python << this->Fixture().AppendTextToProgram();	// Reinstate the current coordinate system.

						if (PROGRAM->m_machine.m_pause_after_tool_change)
						{
							python << _T("message(") << PythonString(_T("Press cycle start when ready to continue with ") + pTool->m_title) << _T(")\n");
							python << _T("program_stop(optional=False)\n");
						}
					}
					break;

				default:
					// Don't do any probing after changing tools.
					break;
				} // End switch
			} // End if - then

			// Now move above the current fixture's touch-off point (if it's defined)  That way we end up
			// moving straight down (nearly) to the next machine operation.
			if (this->Fixture().m_params.m_touch_off_point_defined)
			{
				wxString coordinate_system;
				coordinate_system << this->Fixture().m_coordinate_system_number;

				python << this->Fixture().AppendTextToProgram();	// Reinstate the current coordinate system.
				python	<< _T("comment('Move above the touch-off point for ") << coordinate_system.c_str() << _T("')\n");
				// Invoke new coordinate system.
				python << Fixture().AppendTextToProgram();
				python	<< _T("rapid(x=") << (this->Fixture().m_params.m_touch_off_point.X() / PROGRAM->m_units)
						<< _T(", y=") << (this->Fixture().m_params.m_touch_off_point.Y() / PROGRAM->m_units) << _T(")\n");
			}
        } // End if - then
    }

    return(python);
}


/**
    If the machine is changing fixtures, we may need to move up to a safety height before moving on to the
    next fixture.  If it is, indeed, a different fixture then issue the appropriate GCode to make the switch.  This
    routine should not add the GCode unless it's necessary.  We want the AppendTextToProgram() methods to be
    able to call this routine repeatedly without worrying about unnecessary movements.

	If we're moving between two different fixtures, move above the new fixture's touch-off point before
	continuing on with the other machine operations.  This ensures that the tool is somewhere above
	the new fixture before we start any other movements.
 */
Python CMachineState::Fixture( CFixture new_fixture )
{
    Python python;

    if ((m_fixture != new_fixture) || (! m_fixture_has_been_set))
    {
        // The fixture has been changed.  Move to the highest safety-height between the two fixtures.
        if (m_fixture.m_params.m_safety_height_defined)
        {
			if (new_fixture.m_params.m_safety_height_defined)
			{
				wxString comment;
				comment << _("Moving to a safety height common to both ") << m_fixture.m_coordinate_system_number << _(" and ") << new_fixture.m_coordinate_system_number;
				python << _T("comment(") << PythonString(comment) << _T(")\n");

				// Both fixtures have a safety height defined.  Move the highest of the two.
				if (m_fixture.m_params.m_safety_height > new_fixture.m_params.m_safety_height)
				{
					python << _T("rapid(z=") << m_fixture.m_params.m_safety_height / PROGRAM->m_units << _T(", machine_coordinates=True)\n");
				} // End if - then
				else
				{
					python << _T("rapid(z=") << new_fixture.m_params.m_safety_height / PROGRAM->m_units << _T(", machine_coordinates=True)\n");
				} // End if - else
			} // End if - then
			else
			{
				// The old fixture has a safety height but the new one doesn't
				python << _T("rapid(z=") << m_fixture.m_params.m_safety_height / PROGRAM->m_units << _T(", machine_coordinates=True)\n");
			} // End if - else
        }

		// Invoke new coordinate system.
		python << new_fixture.AppendTextToProgram();

		if (m_fixture_has_been_set == true)
		{
			// We must be moving between fixtures rather than an initial fixture setup.

			// Now move to above the touch-off point so that, when we plunge down, we won't hit the old fixture.
			if (new_fixture.m_params.m_touch_off_description.Length() > 0)
			{
				python << _T("comment(") << PythonString(new_fixture.m_params.m_touch_off_description) << _T(")\n");
			}

			if (new_fixture.m_params.m_touch_off_point_defined)
			{
				wxString comment;
				comment << _("Move above touch-off point for ") << new_fixture.m_coordinate_system_number;
				python << _T("comment(") << PythonString(comment) << _T(")\n");
				python << _T("rapid(x=") << new_fixture.m_params.m_touch_off_point.X()/PROGRAM->m_units << _T(", y=") << new_fixture.m_params.m_touch_off_point.Y()/PROGRAM->m_units << _T(")\n");
			}
		} // End if - then

		m_location_is_known = false;
		m_fixture_has_been_set = true;
    }

    m_fixture = new_fixture;
    return(python);
}

/**
	Look to see if this object has been handled for this fixture already.
 */
bool CMachineState::AlreadyProcessed( const HeeksObj *object, const CFixture fixture )
{
    Instance instance;
    instance.Object(object);
    instance.Fixture(fixture);

    return(m_already_processed.find(instance) != m_already_processed.end());
}

/**
	Remember which objects have been processed for which fixtures.
 */
void CMachineState::MarkAsProcessed( const HeeksObj *object, const CFixture fixture )
{
    Instance instance;
    instance.Object(object);
    instance.Fixture(fixture);

	m_already_processed.insert( instance );
}

CMachineState::Instance::Instance( const CMachineState::Instance & rhs ) : m_fixture(rhs.m_fixture)
{
    *this = rhs;
}

CMachineState::Instance & CMachineState::Instance::operator= ( const CMachineState::Instance & rhs )
{
    if (this != &rhs)
    {
        m_object = rhs.m_object;
        m_fixture = rhs.m_fixture;
    }

    return(*this);
}

bool CMachineState::Instance::operator== ( const CMachineState::Instance & rhs ) const
{
    if (m_object != rhs.m_object) return(false);
    if (m_fixture != rhs.m_fixture) return(false);

    return(true);
}

bool CMachineState::Instance::operator< ( const CMachineState::Instance & rhs ) const
{
    if (m_object < rhs.m_object) return(true);
    if (m_object > rhs.m_object) return(false);

    return(m_fixture < rhs.m_fixture);
}


/**
	Go through and probe each fixture (and the tool length switch) to determine the height offsets (if appropriate)
 */
Python CMachineState::ToolChangeMovement_Preamble(std::set<CFixture> & fixtures)
{
	Python python;

	if (! m_pMachine)
	{
		// We need the various parameters from the machine definition to implement this functionality.
		return(python);	// empty.
	}

	CTool *pFixtureProbeTool = (CTool *) CTool::Find(m_pMachine->m_fixture_probe_tool_number);
	CTool *pToolLengthSwitch = (CTool*) CTool::Find(m_pMachine->m_tool_length_switch_number);

	if ((m_pMachine->m_safety_height_defined == false) ||
		(pFixtureProbeTool == NULL) ||
		(pToolLengthSwitch == NULL))
	{
		// We need these things for this functionality.
		return(python);	// empty.
	}


	// Add some comments and pauses to make sure the operator has setup the machine as this
	// program expects.
	if ((m_pMachine->m_safety_height_defined) &&
        ((m_pMachine->m_tool_change_movement == CMachine::ToolChangeMovement_G28_and_probe) ||
         (m_pMachine->m_tool_change_movement == CMachine::ToolChangeMovement_G30_and_probe) ||
         (m_pMachine->m_tool_change_movement == CMachine::ToolChangeMovement_ExplicitlyDefined_and_probe)))
	{
		if (PROGRAM->m_machine.m_skip_switch_and_fixture_probing_cycle == false)
		{
			switch (m_pMachine->m_tool_change_movement)
			{
				case CMachine::ToolChangeMovement_G28_and_probe:
					python << _T("message('Confirm that the tool length switch position has been marked with G28.1')\n");
					python << _T("program_stop(optional=False)\n");
					break;

				case CMachine::ToolChangeMovement_G30_and_probe:
					python << _T("message('Confirm that the tool length switch position has been marked with G30.1')\n");
					python << _T("program_stop(optional=False)\n");
					break;

				case CMachine::ToolChangeMovement_ExplicitlyDefined_and_probe:
					python << _T("message('Confirm that the tool length switch position is ")
							<< m_pMachine->m_explicit_tool_change_position[0] << _T(",")
							<< m_pMachine->m_explicit_tool_change_position[1]
							<< _T(" machine units')\n");
					python << _T("program_stop(optional=False)\n");
					break;
			} // End switch
		} // End if - then

		// Make sure we confirm that the operator has manually touched-off all the coordinate systems we're going to need.
		for (std::set<CFixture>::const_iterator l_itFixture = fixtures.begin(); l_itFixture != fixtures.end(); l_itFixture++)
		{
			if (l_itFixture->m_params.m_touch_off_point_defined)
			{
				wxString name;
				name << l_itFixture->m_coordinate_system_number;
				if (PROGRAM->m_machine.m_skip_switch_and_fixture_probing_cycle)
				{
					python << _T("message('Confirm that the X,Y touch-off point for ") << name << _T(" has been probed earlier in this SAME session')\n");
					python << _T("program_stop(optional=False)\n");
				}
				else
				{
					python << _T("message('Confirm that the X,Y touch-off point for ") << name << _T(" has been MANUALLY set')\n");
					python << _T("program_stop(optional=False)\n");
				}
			}
			else
			{
				wxString message;
				message << _("Tool change movement configured for use but no touch-off point defined for ") << l_itFixture->m_coordinate_system_number;
				wxMessageBox(message);
				return(python);
			}
		} // End for


		// Probe the height of the tool length switch (if appropriate)
		if ((m_pMachine->m_safety_height_defined) && 
			(pFixtureProbeTool != NULL) && 
			(PROGRAM->m_machine.m_skip_switch_and_fixture_probing_cycle == false))
		{
			python << _T("comment(") << PythonString(_T("tool change to ") + pFixtureProbeTool->m_title) << _T(")\n");
			python << _T("tool_change(id=") << m_pMachine->m_fixture_probe_tool_number << _T(")\n");
			python << _T("feedrate(") << m_pMachine->m_tool_change_probe_feed_rate / theApp.m_program->m_units << _T(")\n");

			// Make sure it's negative as we're stepping down.  There is no option
			// to specify a starting height so this can only be a relative distance.
			double depth = m_pMachine->m_tool_change_probe_depth / theApp.m_program->m_units;
			double relative_depth = (depth > 0)?(-1.0 * depth):depth;

			switch (m_pMachine->m_tool_change_movement)
			{
				case CMachine::ToolChangeMovement_G28_and_probe:
					python << _T("comment('Move back up to safety height')\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << _T("predefined_position(type='G28')\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << _T("comment('Probe to find the height of the tool length switch.  NOTE: We MUST do this from the machine safety height for consistency across fixtures')\n");
					python << _T("probe_downward_point(depth=") << relative_depth << _T(", intersection_variable_z='<tool_length_switch_offset>')\n");
					python << _T("comment('Move back up to safety height')\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << Fixture().AppendTextToProgram();	// Reinstate the current fixture.
					break;

				case CMachine::ToolChangeMovement_G30_and_probe:
					python << _T("comment('Move back up to safety height')\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << _T("predefined_position(type='G30')\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << _T("comment('Probe to find the height of the tool length switch.  NOTE: We MUST do this from the machine safety height for consistency across fixtures')\n");
					python << _T("probe_downward_point(depth=") << relative_depth << _T(", intersection_variable_z='<tool_length_switch_offset>')\n");
					python << _T("comment('Move back up to safety height')\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << Fixture().AppendTextToProgram();	// Reinstate the current fixture.
					break;

				case CMachine::ToolChangeMovement_ExplicitlyDefined_and_probe:
					python << _T("comment('Move back up to safety height')\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << _T("rapid(x=") << (m_pMachine->m_explicit_tool_change_position[0] / theApp.m_program->m_units) << _T(",")
							<< _T("y=") << (m_pMachine->m_explicit_tool_change_position[1] / theApp.m_program->m_units)
							<< _T(", machine_coordinates=True)\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << _T("comment('Probe to find the height of the tool length switch')\n");
                    python << _T("probe_downward_point(depth=") << relative_depth << _T(", intersection_variable_z='<tool_length_switch_offset>')\n");
					python << _T("comment('Move back up to safety height')\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << Fixture().AppendTextToProgram();	// Reinstate the current fixture.
					break;
			} // End switch
		} // End if - then

		if (PROGRAM->m_machine.m_skip_switch_and_fixture_probing_cycle == false)
		{
			// Now probe for the heights of all the fixtures we're using.
			for (std::set<CFixture>::const_iterator l_itFixture = fixtures.begin(); l_itFixture != fixtures.end(); l_itFixture++)
			{
				// Make sure it's negative as we're stepping down.  There is no option
				// to specify a starting height so this can only be a relative distance.
				double depth = m_pMachine->m_tool_change_probe_depth / theApp.m_program->m_units;
				double relative_depth = (depth > 0)?(-1.0 * depth):depth;

				if (l_itFixture->m_params.m_touch_off_point_defined)
				{
					python << _T("feedrate(") << m_pMachine->m_tool_change_probe_feed_rate / theApp.m_program->m_units << _T(")\n");
					wxString name;
					name << l_itFixture->m_coordinate_system_number;
					python << _T("comment('Probing for height of ") << name << _T(" fixture')\n");
					python << _T("comment('Move back up to safety height')\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << l_itFixture->AppendTextToProgram();
					python << _T("rapid(x=") << (l_itFixture->m_params.m_touch_off_point.X() / theApp.m_program->m_units) << _T(",")
							<< _T("y=") << (l_itFixture->m_params.m_touch_off_point.Y() / theApp.m_program->m_units)
							<< _T(")\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << _T("probe_downward_point(depth=") << relative_depth << _T(", intersection_variable_z='<") << name << _T("_fixture_height_offset>', touch_off_as_z=") << (l_itFixture->m_params.m_touch_off_point.Z() / theApp.m_program->m_units) << _T(", rapid_down_to_height=") << (5.0 / theApp.m_program->m_units) << _T(")\n");
					python << _T("comment('Move back up to safety height')\n");
					python << _T("rapid(z=") << (m_pMachine->m_safety_height / theApp.m_program->m_units) << _T(", machine_coordinates=True)\n");
					python << l_itFixture->AppendTextToProgram();	// Reinstate the current fixture.
					python << _T("rapid(x=") << (l_itFixture->m_params.m_touch_off_point.X() / theApp.m_program->m_units) << _T(",")
							<< _T("y=") << (l_itFixture->m_params.m_touch_off_point.Y() / theApp.m_program->m_units)
							<< _T(")\n");
				}
			} // End for
		}
	} // End if - then

	return(python);
}

void CMachineState::Location( const CNCPoint rhs )
{
	m_location = rhs; m_location_is_known = true;
	m_previous_locations.insert( std::make_pair( Fixture(), rhs ) );	// Remember where we've been.
}

/**
	Find the nearest location to the one passed in (in the XY plane ONLY).  If an
	exact match can be found in XY then return the one with the smallest Z value.
	The idea is that, when we need to move back down from a clearance height, we
	don't want to bothter ramping slowly down to a location we've already machined.
	We could just feed down to this previously machined location and then ramp
	from there.
 */
bool CMachineState::NearestLocation(CFixture fixture, CNCPoint location, CNCPoint *pPreviousLocation) const
{
	double tolerance = heeksCAD->GetTolerance();
	std::list<CNCPoint> options;
	for (std::multimap<CFixture, CNCPoint>::const_iterator itLocation = m_previous_locations.lower_bound(fixture);
		itLocation != m_previous_locations.upper_bound(fixture); itLocation++)
	{
		if (itLocation->second.XYDistance(location) < tolerance)
		{
			options.push_back(itLocation->second);			
		}
	}

	options.sort();
	if (options.size() > 0)
	{
		if (pPreviousLocation) *pPreviousLocation = *(options.begin());
		return(true);
	}

	return(false);	// Nothing found that matches the X,Y coordinate pair.
}

