// Program.cpp
// Copyright (c) 2009, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.

#include "stdafx.h"
#include "Program.h"
#include "PythonStuff.h"
#include "tinyxml/tinyxml.h"
#include "ProgramCanvas.h"
#include "NCCode.h"
#include "interface/MarkedObject.h"
#include "interface/PropertyString.h"
#include "interface/PropertyFile.h"
#include "interface/PropertyChoice.h"
#include "interface/PropertyDouble.h"
#include "interface/PropertyLength.h"
#include "interface/PropertyCheck.h"
#include "interface/PropertyVertex.h"
#include "interface/Tool.h"
#include "Profile.h"
#include "Pocket.h"
#include "ZigZag.h"
#include "Waterline.h"
#include "Drilling.h"
#include "CTool.h"
#include "Op.h"
#include "CNCConfig.h"
#include "CounterBore.h"
#include "Fixture.h"
#include "SpeedOp.h"
#include "Operations.h"
#include "Fixtures.h"
#include "Tools.h"
#include "interface/strconv.h"
#include "MachineState.h"
#include "AttachOp.h"

#include <wx/stdpaths.h>
#include <wx/filename.h>

#include <vector>
#include <algorithm>
#include <fstream>
#include <memory>
using namespace std;


#ifdef HEEKSCNC
#include "Fixtures.h"
#define FIND_FIRST_TOOL CTool::FindFirstByType
#define FIND_ALL_TOOLS CTool::FindAllTools
#define MACHINE_STATE_TOOL(t) pMachineState->Tool(t)
#else
#define FIND_FIRST_TOOL heeksCNC->FindFirstToolByType
#define FIND_ALL_TOOLS heeksCNC->FindAllTools
#define MACHINE_STATE_TOOL(t) heeksCNC->MachineStateTool(pMachineState, t)
#endif



wxString CProgram::alternative_machines_file = _T("");

CProgram::CProgram():m_nc_code(NULL), m_operations(NULL), m_tools(NULL), m_speed_references(NULL)
							, m_fixtures(NULL)
							, m_script_edited(false)
{
	CNCConfig config(ConfigScope());
	wxString machine_file_name;
	config.Read(_T("ProgramMachine"), &machine_file_name, _T("iso"));
	m_machine = CProgram::GetMachine(machine_file_name);

	config.Read(_T("OutputFileNameFollowsDataFileName"), &m_output_file_name_follows_data_file_name, true);
	config.Read(_T("UseInternalBackplotting"), &m_use_internal_backplotting,  true);
	config.Read(_T("Emc2VariablesUnits"), (int *) &m_emc2_variables_units,  int(CProgram::eUndefined));

    wxStandardPaths standard_paths;
    wxFileName default_path( standard_paths.GetTempDir().c_str(), _T("test.tap"));

	config.Read(_T("ProgramOutputFile"), &m_output_file, default_path.GetFullPath().c_str());
	config.Read(_T("EMC2VariablesFileName"), &m_emc2_variables_file_name, _T("emc.var"));

	config.Read(_T("ProgramUnits"), &m_units, 1.0);
	config.Read(_("ProgramPathControlMode"), (int *) &m_path_control_mode, (int) ePathControlUndefined );
	config.Read(_("ProgramMotionBlendingTolerance"), &m_motion_blending_tolerance, 0.0001);
	config.Read(_("ProgramNaiveCamTolerance"), &m_naive_cam_tolerance, 0.0001);

	config.Read(_("ClearanceSource"), (int *) &m_clearance_source, int(CProgram::eClearanceDefinedByOperation) );
}

const wxBitmap &CProgram::GetIcon()
{
	static wxBitmap* icon = NULL;
	if(icon == NULL)icon = new wxBitmap(wxImage(theApp.GetResFolder() + _T("/icons/program.png")));
	return *icon;
}

HeeksObj *CProgram::MakeACopy(void)const
{
	return new CProgram(*this);
}


CProgram::CProgram( const CProgram & rhs ) : ObjList(rhs)
{
    m_nc_code = NULL;
    m_operations = NULL;
    m_tools = NULL;
    m_speed_references = NULL;
    m_fixtures = NULL;
    m_script_edited = false;

    m_raw_material = rhs.m_raw_material;
    m_machine = rhs.m_machine;
    m_output_file = rhs.m_output_file;
    m_emc2_variables_file_name = rhs.m_emc2_variables_file_name;
    m_output_file_name_follows_data_file_name = rhs.m_output_file_name_follows_data_file_name;
    m_use_internal_backplotting = rhs.m_use_internal_backplotting;
    m_emc2_variables_units = rhs.m_emc2_variables_units;

    m_script_edited = rhs.m_script_edited;
    m_units = rhs.m_units;

	m_path_control_mode = rhs.m_path_control_mode;
	m_motion_blending_tolerance = rhs.m_motion_blending_tolerance;
	m_naive_cam_tolerance = rhs.m_naive_cam_tolerance;

	m_clearance_source = rhs.m_clearance_source;

    ReloadPointers();
    AddMissingChildren();

    if ((m_nc_code != NULL) && (rhs.m_nc_code != NULL)) *m_nc_code = *(rhs.m_nc_code);
    if ((m_operations != NULL) && (rhs.m_operations != NULL)) *m_operations = *(rhs.m_operations);
    if ((m_tools != NULL) && (rhs.m_tools != NULL)) *m_tools = *(rhs.m_tools);
    if ((m_speed_references != NULL) && (rhs.m_speed_references != NULL)) *m_speed_references = *(rhs.m_speed_references);
    if ((m_fixtures != NULL) && (rhs.m_fixtures != NULL)) *m_fixtures = *(rhs.m_fixtures);
}

CProgram::~CProgram()
{
	// Only remove the global pointer if 'we are the one'.  When a file is imported, extra
	// CProgram objects exist temporarily.  They're not all used as the master data pointer.
	if (theApp.m_program == this)
	{
		theApp.m_program = NULL;
	}
}

/**
	This is ALMOST the same as the assignment operator.  The difference is that
	this, and its subordinate methods, augment themselves with the contents
	of the object passed in rather than replacing their own copies.
 */
void CProgram::CopyFrom(const HeeksObj* object)
{
	if (object->GetType() == GetType())
	{
		CProgram *rhs = (CProgram *) object;
		// ObjList::operator=(*rhs);	// I don't think this will do anything in this case. But it might one day.

		if ((m_nc_code != NULL) && (rhs->m_nc_code != NULL)) m_nc_code->CopyFrom( rhs->m_nc_code );
		if ((m_operations != NULL) && (rhs->m_operations != NULL)) m_operations->CopyFrom( rhs->m_operations );
		if ((m_tools != NULL) && (rhs->m_tools != NULL)) m_tools->CopyFrom( rhs->m_tools );
		if ((m_speed_references != NULL) && (rhs->m_speed_references != NULL)) m_speed_references->CopyFrom(rhs->m_speed_references);
		if ((m_fixtures != NULL) && (rhs->m_fixtures != NULL)) m_fixtures->CopyFrom(rhs->m_fixtures);

		m_raw_material = rhs->m_raw_material;
		m_machine = rhs->m_machine;
		m_output_file = rhs->m_output_file;
		m_emc2_variables_file_name = rhs->m_emc2_variables_file_name;
		m_output_file_name_follows_data_file_name = rhs->m_output_file_name_follows_data_file_name;
		m_use_internal_backplotting = rhs->m_use_internal_backplotting;
		m_emc2_variables_units = rhs->m_emc2_variables_units;

		m_script_edited = rhs->m_script_edited;
		m_units = rhs->m_units;

		m_path_control_mode = rhs->m_path_control_mode;
		m_motion_blending_tolerance = rhs->m_motion_blending_tolerance;
		m_naive_cam_tolerance = rhs->m_naive_cam_tolerance;

		m_clearance_source = rhs->m_clearance_source;
	}
}

CProgram & CProgram::operator= ( const CProgram & rhs )
{
	if (this != &rhs)
	{
		ObjList::operator=(rhs);
		ReloadPointers();

		if ((m_nc_code != NULL) && (rhs.m_nc_code != NULL)) *m_nc_code = *(rhs.m_nc_code);
		if ((m_operations != NULL) && (rhs.m_operations != NULL)) *m_operations = *(rhs.m_operations);
		if ((m_tools != NULL) && (rhs.m_tools != NULL)) *m_tools = *(rhs.m_tools);
		if ((m_speed_references != NULL) && (rhs.m_speed_references != NULL)) *m_speed_references = *(rhs.m_speed_references);
		if ((m_fixtures != NULL) && (rhs.m_fixtures != NULL)) *m_fixtures = *(rhs.m_fixtures);

		m_raw_material = rhs.m_raw_material;
		m_machine = rhs.m_machine;
		m_output_file = rhs.m_output_file;
		m_emc2_variables_file_name = rhs.m_emc2_variables_file_name;
		m_output_file_name_follows_data_file_name = rhs.m_output_file_name_follows_data_file_name;
		m_use_internal_backplotting = rhs.m_use_internal_backplotting;
		m_emc2_variables_units = rhs.m_emc2_variables_units;

		m_script_edited = rhs.m_script_edited;
		m_units = rhs.m_units;

		m_path_control_mode = rhs.m_path_control_mode;
		m_motion_blending_tolerance = rhs.m_motion_blending_tolerance;
		m_naive_cam_tolerance = rhs.m_naive_cam_tolerance;

		m_clearance_source = rhs.m_clearance_source;
	}

	return(*this);
}


CMachine::CMachine()
{
	m_max_spindle_speed = 0.0;
	file_name = _T("not found");
	description = _T("not found");

	CNCConfig config(CMachine::ConfigScope());
	config.Read(_T("safety_height_defined"), &m_safety_height_defined, false );
	config.Read(_T("safety_height"), &m_safety_height, 0.0 );		// in G53 machine units - indicates where to move to for tool changes
	config.Read(_("ClearanceHeight"), (double *) &(m_clearance_height), 50.0 ); // in local coordinate system (G54 etc.) to show how tall clamps and vices are for movement between machine operations.

	config.Read(_("ToolChangeMovement"), (int *) &m_tool_change_movement, (int) ToolChangeMovement_NotDefined );
	config.Read(_("ToolChangePosition_x"), (double *) &(m_explicit_tool_change_position[0]), 0.0 );
	config.Read(_("ToolChangePosition_y"), (double *) &(m_explicit_tool_change_position[1]), 0.0 );
	config.Read(_("ToolChangePosition_z"), (double *) &(m_explicit_tool_change_position[2]), 0.0 );

	config.Read(_("ToolChangeProbeDepth"), (double *) &(m_tool_change_probe_depth), 300.0 );
	config.Read(_("ToolLengthSwitchToolNumber"), (int *) &(m_tool_length_switch_number), 0 );
	config.Read(_("FixtureProbeToolNumber"), (int *) &(m_fixture_probe_tool_number), 0 );
	config.Read(_("ToolChangeProbingFeedRate"), (double *) &(m_tool_change_probe_feed_rate), 300.0 );

	config.Read(_T("nurbs_supported"), &m_nurbs_supported, false );
	config.Read(_T("pause_after_tool_change"), &m_pause_after_tool_change, true );
	config.Read(_T("skip_switch_and_fixture_probing_cycle"), &m_skip_switch_and_fixture_probing_cycle, false );
	config.Read(_T("auto_check_design_rules"), &m_auto_check_design_rules, true );
}

CMachine::CMachine( const CMachine & rhs )
{
	*this = rhs;	// call the assignment operator
}


CMachine & CMachine::operator= ( const CMachine & rhs )
{
	if (this != &rhs)
	{
		configuration_file_name = rhs.configuration_file_name;
		file_name = rhs.file_name;
		description = rhs.description;
		m_max_spindle_speed = rhs.m_max_spindle_speed;
		m_safety_height_defined = rhs.m_safety_height_defined;
		m_safety_height = rhs.m_safety_height;
		m_clearance_height = rhs.m_clearance_height;

		m_nurbs_supported = rhs.m_nurbs_supported;
		m_pause_after_tool_change = rhs.m_pause_after_tool_change;
		m_skip_switch_and_fixture_probing_cycle = rhs.m_skip_switch_and_fixture_probing_cycle;
		m_auto_check_design_rules = rhs.m_auto_check_design_rules;

		m_tool_change_movement = rhs.m_tool_change_movement;
		for (::size_t i = 0; i < sizeof(m_explicit_tool_change_position) / sizeof(m_explicit_tool_change_position[0]); i++)
		{
			m_explicit_tool_change_position[i] = rhs.m_explicit_tool_change_position[i];
		}

		m_tool_change_probe_depth = rhs.m_tool_change_probe_depth;
		m_tool_length_switch_number = rhs.m_tool_length_switch_number;
		m_tool_change_probe_feed_rate = rhs.m_tool_change_probe_feed_rate;
		m_fixture_probe_tool_number = rhs.m_fixture_probe_tool_number;
	} // End if - then

	return(*this);
} // End assignment operator.


static void on_set_machine(int value, HeeksObj* object)
{
	std::vector<CMachine> machines;
	CProgram::GetMachines(machines);
	((CProgram*)object)->m_machine = machines[value];
	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("ProgramMachine"), ((CProgram*)object)->m_machine.file_name);
	heeksCAD->RefreshProperties();
}

static void on_set_output_file(const wxChar* value, HeeksObj* object)
{
	((CProgram*)object)->m_output_file = value;
	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("ProgramOutputFile"), ((CProgram*)object)->m_output_file);
}

static void on_set_emc2_variables_file_name(const wxChar* value, HeeksObj* object)
{
	((CProgram*)object)->m_emc2_variables_file_name = value;
	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("EMC2VariablesFileName"), ((CProgram*)object)->m_emc2_variables_file_name);
}

void CProgram::SetEmc2VariablesFileName(const wxChar *value)
{
	on_set_emc2_variables_file_name( value, this );
}


static void on_set_units(int value, HeeksObj* object)
{
	((CProgram*)object)->ChangeUnits((value == 0) ? 1.0:25.4);

	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("ProgramUnits"), ((CProgram*)object)->m_units);

    if (heeksCAD->GetViewUnits() != ((CProgram*)object)->m_units)
    {
        int response;
        response = wxMessageBox( _("Would you like to change the HeeksCAD view units too?"), _("Change Units"), wxYES_NO );
        if (response == wxYES)
        {
            heeksCAD->SetViewUnits(((CProgram*)object)->m_units, true);
            heeksCAD->RefreshOptions();
        }
    }
    heeksCAD->RefreshProperties();
}

static void on_set_output_file_name_follows_data_file_name(int zero_based_choice, HeeksObj *object)
{
	CProgram *pProgram = (CProgram *) object;
	pProgram->m_output_file_name_follows_data_file_name = (zero_based_choice != 0);
	heeksCAD->RefreshProperties();

	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("OutputFileNameFollowsDataFileName"), pProgram->m_output_file_name_follows_data_file_name );
}

static void on_set_use_internal_backplotting(int zero_based_choice, HeeksObj *object)
{
	CProgram *pProgram = (CProgram *) object;
	pProgram->m_use_internal_backplotting = (zero_based_choice != 0);
	heeksCAD->RefreshProperties();

	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("UseInternalBackplotting"), pProgram->m_use_internal_backplotting );
}

static void on_set_emc2_variables_units(int zero_based_choice, HeeksObj *object)
{
	CProgram *pProgram = (CProgram *) object;
	pProgram->m_emc2_variables_units = CProgram::eUnits_t(zero_based_choice);
	heeksCAD->RefreshProperties();

	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("Emc2VariablesUnits"), pProgram->m_emc2_variables_units );
}


static void on_set_clearance_source(int zero_based_choice, HeeksObj *object)
{
	CProgram *pProgram = (CProgram *) object;
	pProgram->m_clearance_source = CProgram::eClearanceSource_t(zero_based_choice);

	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("ClearanceSource"), (int) pProgram->m_clearance_source );
}

static void on_set_path_control_mode(int zero_based_choice, HeeksObj *object)
{
	CProgram *pProgram = (CProgram *) object;
	pProgram->m_path_control_mode = CProgram::ePathControlMode_t(zero_based_choice);

	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("ProgramPathControlMode"), (int) pProgram->m_path_control_mode );
}

static void on_set_motion_blending_tolerance(double value, HeeksObj *object)
{
	CProgram *pProgram = (CProgram *) object;
	pProgram->m_motion_blending_tolerance = value;

	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("ProgramMotionBlendingTolerance"), pProgram->m_motion_blending_tolerance );
}

static void on_set_naive_cam_tolerance(double value, HeeksObj *object)
{
	CProgram *pProgram = (CProgram *) object;
	pProgram->m_naive_cam_tolerance = value;

	CNCConfig config(CProgram::ConfigScope());
	config.Write(_T("ProgramNaiveCamTolerance"), pProgram->m_naive_cam_tolerance );
}


void CProgram::GetProperties(std::list<Property *> *list)
{
	{
		std::vector<CMachine> machines;
		GetMachines(machines);

		std::list< wxString > choices;
		int choice = 0;
		for(unsigned int i = 0; i < machines.size(); i++)
		{
			CMachine& machine = machines[i];
			choices.push_back(machine.description);
			if(machine.file_name == m_machine.file_name)choice = i;
		}
		list->push_back ( new PropertyChoice ( _("machine"),  choices, choice, this, on_set_machine ) );
	}

	{
		std::list<wxString> choices;
		int choice = int(m_output_file_name_follows_data_file_name?1:0);
		choices.push_back(_T("False"));
		choices.push_back(_T("True"));

		list->push_back(new PropertyChoice(_("output file name follows data file name"), choices, choice, this, on_set_output_file_name_follows_data_file_name));
	}

	{
		std::list<wxString> choices;
		int choice = int(m_use_internal_backplotting?1:0);
		choices.push_back(_T("Use Python Backplotting"));
		choices.push_back(_T("Use Internal Backplotting"));

		list->push_back(new PropertyChoice(_("Backplotting"), choices, choice, this, on_set_use_internal_backplotting));
	}

    if (m_use_internal_backplotting)
    {
        list->push_back(new PropertyFile(_("EMC2 Variables File Name"), m_emc2_variables_file_name, this, on_set_emc2_variables_file_name));

        {
            std::list<wxString> choices;
            int choice = int(m_emc2_variables_units);
            choices.push_back(_T("Undefined"));
            choices.push_back(_T("Metric"));
            choices.push_back(_T("Imperial"));

            list->push_back(new PropertyChoice(_("EMC2 Variables Units"), choices, choice, this, on_set_emc2_variables_units));
        }

    }

	typedef enum
	{
		eClearanceDefinedByMachine = 0,
		eClearanceDefinedByFixture,
		eClearanceDefinedByOperation
	} eClearanceSource_t;


	{
		std::list< wxString > choices;
		choices.push_back ( wxString ( _("By Machine") ) );
		choices.push_back ( wxString ( _("By Fixture") ) );
		choices.push_back ( wxString ( _("By Operation") ) );
		int choice = int(m_clearance_source);

		list->push_back ( new PropertyChoice ( _("Clearance Height Defined"),  choices, choice, this, on_set_clearance_source ) );
	}

	if (m_output_file_name_follows_data_file_name == false)
	{
		list->push_back(new PropertyFile(_("output file"), m_output_file, this, on_set_output_file));
	} // End if - then

	{
		std::list< wxString > choices;
		choices.push_back ( wxString ( _("mm") ) );
		choices.push_back ( wxString ( _("inch") ) );
		int choice = 0;
		if(m_units > 25.0)choice = 1;
		list->push_back ( new PropertyChoice ( _("units for nc output"),  choices, choice, this, on_set_units ) );
	}

	m_machine.GetProperties(this, list);
	m_raw_material.GetProperties(this, list);

	{
		std::list< wxString > choices;
		choices.push_back(_("Exact Path Mode"));
		choices.push_back(_("Exact Stop Mode"));
		choices.push_back(_("Best Possible Speed"));
		choices.push_back(_("Undefined"));

		list->push_back ( new PropertyChoice ( _("Path Control Mode"),  choices, (int) m_path_control_mode, this, on_set_path_control_mode ) );

		if (m_path_control_mode == eBestPossibleSpeed)
		{
			list->push_back( new PropertyLength( _("Motion Blending Tolerance"), m_motion_blending_tolerance, this, on_set_motion_blending_tolerance ) );
			list->push_back( new PropertyLength( _("Naive CAM Tolerance"), m_naive_cam_tolerance, this, on_set_naive_cam_tolerance ) );
		} // End if - then
	}

	HeeksObj::GetProperties(list);
}

static void on_set_max_spindle_speed(double value, HeeksObj* object)
{
    ((CProgram *) object)->m_machine.m_max_spindle_speed = value;
	heeksCAD->RefreshProperties();
}

static void on_set_nurbs_supported(const bool value, HeeksObj *object)
{
    ((CProgram *)object)->m_machine.m_nurbs_supported = value;

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("nurbs_supported"), ((CProgram *)object)->m_machine.m_nurbs_supported );

    heeksCAD->Changed();
}

static void on_set_pause_after_tool_change(const bool value, HeeksObj *object)
{
    ((CProgram *)object)->m_machine.m_pause_after_tool_change = value;

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("pause_after_tool_change"), ((CProgram *)object)->m_machine.m_pause_after_tool_change );

    heeksCAD->Changed();
}

static void on_set_skip_switch_and_fixture_probing_cycle(const bool value, HeeksObj *object)
{
    ((CProgram *)object)->m_machine.m_skip_switch_and_fixture_probing_cycle = value;

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("skip_switch_and_fixture_probing_cycle"), ((CProgram *)object)->m_machine.m_skip_switch_and_fixture_probing_cycle );

    heeksCAD->Changed();
}

static void on_set_auto_check_design_rules(const bool value, HeeksObj *object)
{
    ((CProgram *)object)->m_machine.m_auto_check_design_rules = value;

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("auto_check_design_rules"), ((CProgram *)object)->m_machine.m_auto_check_design_rules );

    heeksCAD->Changed();
}


static void on_set_safety_height_defined(const bool value, HeeksObj *object)
{
    ((CProgram *)object)->m_machine.m_safety_height_defined = value;

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("safety_height_defined"), ((CProgram *)object)->m_machine.m_safety_height_defined );

    heeksCAD->Changed();
}

static void on_set_safety_height(const double value, HeeksObj *object)
{
    ((CProgram *)object)->m_machine.m_safety_height = value;

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("safety_height"), ((CProgram *)object)->m_machine.m_safety_height );

    heeksCAD->Changed();
}

static void on_set_tool_change_position( const double *position, HeeksObj *object)
{
	CMachine *pMachine = &(((CProgram *)object)->m_machine);
	pMachine->m_explicit_tool_change_position[0] = position[0];
	pMachine->m_explicit_tool_change_position[1] = position[1];
	pMachine->m_explicit_tool_change_position[2] = position[2];

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("ToolChangePosition_x"), pMachine->m_explicit_tool_change_position[0]);
	config.Write(_T("ToolChangePosition_y"), pMachine->m_explicit_tool_change_position[1]);
	config.Write(_T("ToolChangePosition_z"), pMachine->m_explicit_tool_change_position[2]);

	heeksCAD->RefreshProperties();
}





static void on_set_tool_change_probe_depth( const double value, HeeksObj *object)
{
	CMachine *pMachine = &(((CProgram *)object)->m_machine);
	pMachine->m_tool_change_probe_depth = value;

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("ToolChangeProbeDepth"), pMachine->m_tool_change_probe_depth);

	heeksCAD->RefreshProperties();
}

static void on_set_tool_change_probe_feed_rate( const double value, HeeksObj *object)
{
	CMachine *pMachine = &(((CProgram *)object)->m_machine);
	pMachine->m_tool_change_probe_feed_rate = value;

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("ToolChangeProbingFeedRate"), pMachine->m_tool_change_probe_feed_rate);

	heeksCAD->RefreshProperties();
}

static void on_set_clearance_height( const double value, HeeksObj *object)
{
	CMachine *pMachine = &(((CProgram *)object)->m_machine);
	pMachine->m_clearance_height = value;

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("ClearanceHeight"), pMachine->m_clearance_height);

	heeksCAD->RefreshProperties();
}

static void on_set_tool_length_switch_number( const int zero_based_choice, HeeksObj *object)
{
	CMachine *pMachine = &(((CProgram *)object)->m_machine);

	if (zero_based_choice < 0) return;	// An error has occured.

	std::vector< std::pair< int, wxString > > tools = FIND_ALL_TOOLS();

	if ((zero_based_choice >= int(0)) && (zero_based_choice <= int(tools.size()-1)))
	{
		pMachine->m_tool_length_switch_number = tools[zero_based_choice].first;	// Convert the choice offset to the tool number for that choice
	} // End if - then

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("ToolLengthSwitchToolNumber"), pMachine->m_tool_length_switch_number);

	heeksCAD->RefreshProperties();
}


static void on_set_fixture_probe_tool_number( const int zero_based_choice, HeeksObj *object)
{
	CMachine *pMachine = &(((CProgram *)object)->m_machine);

	if (zero_based_choice < 0) return;	// An error has occured.

	std::vector< std::pair< int, wxString > > tools = FIND_ALL_TOOLS();

	if ((zero_based_choice >= int(0)) && (zero_based_choice <= int(tools.size()-1)))
	{
		pMachine->m_fixture_probe_tool_number = tools[zero_based_choice].first;	// Convert the choice offset to the tool number for that choice
	} // End if - then

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("FixtureProbeToolNumber"), pMachine->m_fixture_probe_tool_number);

	heeksCAD->RefreshProperties();
}







static void on_set_tool_change_movement( const int zero_based_choice, HeeksObj *object)
{
	CMachine *pMachine = &(((CProgram *)object)->m_machine);
	pMachine->m_tool_change_movement = CMachine::eToolChangeMovement_t(zero_based_choice);

	CNCConfig config(CMachine::ConfigScope());
	config.Write(_T("ToolChangeMovement"), (int) pMachine->m_tool_change_movement);

	heeksCAD->RefreshProperties();
}

void CMachine::GetProperties(CProgram *parent, std::list<Property *> *list)
{
	list->push_back(new PropertyDouble(_("Maximum Spindle Speed (RPM)"), m_max_spindle_speed, parent, on_set_max_spindle_speed));
	list->push_back(new PropertyCheck(_("Safety Height Defined"), m_safety_height_defined, parent, on_set_safety_height_defined));

	if (theApp.m_program->m_clearance_source == CProgram::eClearanceDefinedByMachine)
	{
		list->push_back(new PropertyLength(_("Clearance Height (for inter-operation movement)"), m_clearance_height, parent, on_set_clearance_height));
	}

    if (m_safety_height_defined)
    {
        list->push_back(new PropertyLength(_("Safety Height (in G53 - Machine - coordinates)"), m_safety_height, parent, on_set_safety_height));

		{
			std::list<wxString> choices;
			choices.push_back(_("Not Defined"));
			choices.push_back(_("G28"));
			choices.push_back(_("G28 and then probe for tool length"));
			choices.push_back(_("G30"));
			choices.push_back(_("G30 and then probe for tool length"));
			choices.push_back(_("Excplicitly Defined Coordinates"));
			choices.push_back(_("Excplicitly Defined Coordinates and then probe for tool length"));

			int choice = (int) m_tool_change_movement;
			list->push_back(new PropertyChoice(_("Tool Change Movement"), choices, choice, parent, on_set_tool_change_movement));
		}

		if ((m_tool_change_movement == ToolChangeMovement_ExplicitlyDefined) ||
			(m_tool_change_movement == ToolChangeMovement_ExplicitlyDefined_and_probe))
		{
			list->push_back(new PropertyVertex( _("Tool Change Position - (in G53 - Machine - coordinates)"), m_explicit_tool_change_position, parent, on_set_tool_change_position ));
		}

		switch (m_tool_change_movement)
		{
		case ToolChangeMovement_G28_and_probe:
		case ToolChangeMovement_G30_and_probe:
		case ToolChangeMovement_ExplicitlyDefined_and_probe:
			{
			list->push_back(new PropertyLength(_("Tool Change Probe Depth"), m_tool_change_probe_depth, parent, on_set_tool_change_probe_depth));
			list->push_back(new PropertyLength(_("Tool Change Probe Feed Rate"), m_tool_change_probe_feed_rate, parent, on_set_tool_change_probe_feed_rate));
			list->push_back(new PropertyCheck(_("Pause After Tool Change?"), m_pause_after_tool_change, parent, on_set_pause_after_tool_change));
			list->push_back(new PropertyCheck(_("Skip Switch and Fixture Probing Cycle?"), m_skip_switch_and_fixture_probing_cycle, parent, on_set_skip_switch_and_fixture_probing_cycle));

			// Add the selection of tools so the operator can select the tool length switch to use.  (we need this
			// for the switch's height to offset after probing)
			std::vector< std::pair< int, wxString > > tools = FIND_ALL_TOOLS();

			int choice = 0;
			int probe_tool_choice = 0;
			std::list< wxString > choices;
			for (std::vector< std::pair< int, wxString > >::size_type i=0; i<tools.size(); i++)
			{
				choices.push_back(tools[i].second);

				if (m_tool_length_switch_number == tools[i].first)
				{
					choice = int(i);
				} // End if - then

				if (m_fixture_probe_tool_number == tools[i].first)
				{
					probe_tool_choice = int(i);
				} // End if - then
			} // End for

			list->push_back(new PropertyChoice(_("Tool Length Switch"), choices, choice, parent, on_set_tool_length_switch_number));
			list->push_back(new PropertyChoice(_("Fixture Probe Tool"), choices, probe_tool_choice, parent, on_set_fixture_probe_tool_number));
			}
			break;

		default:
			// No need to determine these if we're not probing.
			break;
		}
	} // End if - then

	list->push_back(new PropertyCheck(_("NURBS Supported by controller?"), m_nurbs_supported, parent, on_set_nurbs_supported));
	list->push_back(new PropertyCheck(_("Automatically run design rules check before GCode generation?"), m_auto_check_design_rules, parent, on_set_auto_check_design_rules));


} // End GetProperties() method



bool CProgram::CanAdd(HeeksObj* object)
{
    if (object == NULL) return(false);

	return object->GetType() == NCCodeType ||
		object->GetType() == OperationsType ||
		object->GetType() == ToolsType ||
		object->GetType() == SpeedReferencesType
		|| object->GetType() == FixturesType
		;
}

bool CProgram::CanAddTo(HeeksObj* owner)
{
	return ((owner != NULL) && (owner->GetType() == DocumentType));
}

void CProgram::SetClickMarkPoint(MarkedObject* marked_object, const double* ray_start, const double* ray_direction)
{
	if(marked_object->m_map.size() > 0)
	{
		MarkedObject* sub_marked_object = marked_object->m_map.begin()->second;
		if(sub_marked_object)
		{
			HeeksObj* object = sub_marked_object->m_map.begin()->first;
			if(object && object->GetType() == NCCodeType)
			{
				((CNCCode*)object)->SetClickMarkPoint(sub_marked_object, ray_start, ray_direction);
			}
		}
	}
}

void CProgram::WriteXML(TiXmlNode *root)
{
	TiXmlElement * element;
	element = heeksCAD->NewXMLElement( "Program" );
	heeksCAD->LinkXMLEndChild( root,  element );
	element->SetAttribute( "machine", m_machine.file_name.utf8_str());
	element->SetAttribute( "output_file", m_output_file.utf8_str());
	element->SetAttribute( "output_file_name_follows_data_file_name", (int) (m_output_file_name_follows_data_file_name?1:0));

	element->SetAttribute( "program", theApp.m_program_canvas->m_textCtrl->GetValue().utf8_str());
	element->SetDoubleAttribute( "units", m_units);

	element->SetAttribute( "ProgramPathControlMode", int(m_path_control_mode));
	element->SetDoubleAttribute( "ProgramMotionBlendingTolerance", m_motion_blending_tolerance);
	element->SetDoubleAttribute( "ProgramNaiveCamTolerance", m_naive_cam_tolerance);

	m_raw_material.WriteBaseXML(element);
	m_machine.WriteBaseXML(element);
	WriteBaseXML(element);
}

bool CProgram::Add(HeeksObj* object, HeeksObj* prev_object)
{
	switch(object->GetType())
	{
	case NCCodeType:
		m_nc_code = (CNCCode*)object;
		break;

	case OperationsType:
		m_operations = (COperations*)object;
		break;

	case ToolsType:
			m_tools = (CTools*)object;
		break;

	case SpeedReferencesType:
		m_speed_references = (CSpeedReferences*)object;
		break;

	case FixturesType:
		m_fixtures = (CFixtures*)object;
		break;
	}

	return ObjList::Add(object, prev_object);
}

void CProgram::Remove(HeeksObj* object)
{
	// This occurs when the HeeksCAD application performs a 'Reset()'.  This, in turn, deletes
	// the whole of the master data tree.  With this tree's destruction, we must ensure that
	// we delete ourselves cleanly so that this plugin doesn't end up with pointers to
	// deallocated memory.
	//
	// Since these pointes are also stored as children, the ObjList::~ObjList() destructor will
	// delete the children but our pointers to them won't get cleaned up.  That's what this
	// method is all about.

	if(object == m_nc_code)m_nc_code = NULL;
	else if(object == m_operations)m_operations = NULL;
	else if(object == m_tools)m_tools = NULL;
	else if(object == m_speed_references)m_speed_references = NULL;
	else if(object == m_fixtures)m_fixtures = NULL;

	ObjList::Remove(object);
}

// static member function
HeeksObj* CProgram::ReadFromXMLElement(TiXmlElement* pElem)
{
	CProgram* new_object = new CProgram;
	if (theApp.m_program == NULL)
	{
	    theApp.m_program = new_object;
	}

	// get the attributes
	for(TiXmlAttribute* a = pElem->FirstAttribute(); a; a = a->Next())
	{
		std::string name(a->Name());
		if(name == "machine")new_object->m_machine = GetMachine(Ctt(a->Value()));
		else if(name == "output_file"){new_object->m_output_file.assign(Ctt(a->Value()));}
		else if(name == "emc2_variables_file_name"){new_object->m_emc2_variables_file_name.assign(Ctt(a->Value()));}
		else if(name == "output_file_name_follows_data_file_name"){new_object->m_output_file_name_follows_data_file_name = (atoi(a->Value()) != 0); }
		else if(name == "use_internal_backplotting"){new_object->m_use_internal_backplotting = (atoi(a->Value()) != 0); }
		else if(name == "emc2_variables_units"){new_object->m_emc2_variables_units = CProgram::eUnits_t(atoi(a->Value())); }
		else if(name == "program"){theApp.m_program_canvas->m_textCtrl->SetValue(Ctt(a->Value()));}
		else if(name == "units"){new_object->m_units = a->DoubleValue();}
		else if(name == "ProgramPathControlMode"){new_object->m_path_control_mode = ePathControlMode_t(atoi(a->Value()));}
		else if(name == "ProgramMotionBlendingTolerance"){new_object->m_motion_blending_tolerance = a->DoubleValue();}
		else if(name == "ProgramNaiveCamTolerance"){new_object->m_naive_cam_tolerance = a->DoubleValue();}
	}

	new_object->ReadBaseXML(pElem);
	new_object->m_raw_material.ReadBaseXML(pElem);
	new_object->m_machine.ReadBaseXML(pElem);

	new_object->AddMissingChildren();

	return new_object;
}


void CMachine::WriteBaseXML(TiXmlElement *element)
{
	element->SetDoubleAttribute( "max_spindle_speed", m_max_spindle_speed);
	element->SetAttribute( "safety_height_defined", m_safety_height_defined);
	element->SetDoubleAttribute( "safety_height", m_safety_height);
	element->SetDoubleAttribute( "clearance_height", m_clearance_height);
	element->SetAttribute( "nurbs_supported", m_nurbs_supported);
	element->SetAttribute( "pause_after_tool_change", m_pause_after_tool_change);
	element->SetAttribute( "skip_switch_and_fixture_probing_cycle", m_skip_switch_and_fixture_probing_cycle);
	element->SetAttribute( "auto_check_design_rules", m_auto_check_design_rules);


} // End WriteBaseXML() method

void CMachine::ReadBaseXML(TiXmlElement* element)
{
	if (element->Attribute("max_spindle_speed"))
	{
		element->Attribute("max_spindle_speed", &m_max_spindle_speed);
	} // End if - then

	int flag = 0;
	if (element->Attribute("safety_height_defined"))
	{
	    element->Attribute("safety_height_defined", &flag);
        m_safety_height_defined = (flag != 0);
	}

	if (element->Attribute("safety_height")) element->Attribute("safety_height", &m_safety_height);
	if (element->Attribute("clearance_height")) element->Attribute("clearance_height", &m_clearance_height);

	flag = 0;
	if (element->Attribute("nurbs_supported"))
	{
	    element->Attribute("nurbs_supported", &flag);
        m_nurbs_supported = (flag != 0);
	}

	flag = 1;
	if (element->Attribute("pause_after_tool_change"))
	{
	    element->Attribute("pause_after_tool_change", &flag);
        m_pause_after_tool_change = (flag != 0);
	}

	flag = 0;
	if (element->Attribute("skip_switch_and_fixture_probing_cycle"))
	{
	    element->Attribute("skip_switch_and_fixture_probing_cycle", &flag);
        m_skip_switch_and_fixture_probing_cycle = (flag != 0);
	}

	flag = 1;
	if (element->Attribute("auto_check_design_rules"))
	{
        element->Attribute("auto_check_design_rules", &flag);
        m_auto_check_design_rules = (flag != 0);
	}
} // End ReadBaseXML() method



/**
	Sort the NC operations by;
		- execution order
		- centre drilling operations
		- drilling operations
		- all other operations (sorted by tool number to avoid unnecessary tool changes)
 */
struct sort_operations : public std::binary_function< bool, COp *, COp * >
{
	bool operator() ( const COp *lhs, const COp *rhs ) const
	{
		if (lhs->m_execution_order < rhs->m_execution_order) return(true);
		if (lhs->m_execution_order > rhs->m_execution_order) return(false);

		// We want to run through all the centre drilling, then drilling, then milling then chamfering.

		if ((((HeeksObj *)lhs)->GetType() == ChamferType) && (((HeeksObj *)rhs)->GetType() != ChamferType)) return(false);
		if ((((HeeksObj *)lhs)->GetType() != ChamferType) && (((HeeksObj *)rhs)->GetType() == ChamferType)) return(true);

		if ((((HeeksObj *)lhs)->GetType() == DrillingType) && (((HeeksObj *)rhs)->GetType() != DrillingType)) return(true);
		if ((((HeeksObj *)lhs)->GetType() != DrillingType) && (((HeeksObj *)rhs)->GetType() == DrillingType)) return(false);

		if ((((HeeksObj *)lhs)->GetType() == DrillingType) && (((HeeksObj *)rhs)->GetType() == DrillingType))
		{
			// They're both drilling operations.  Select centre drilling over normal drilling.
			CTool *lhsPtr = (CTool *) CTool::Find( lhs->m_tool_number );
			CTool *rhsPtr = (CTool *) CTool::Find( rhs->m_tool_number );

			if ((lhsPtr != NULL) && (rhsPtr != NULL))
			{
				if ((lhsPtr->m_params.m_type == CToolParams::eCentreDrill) &&
				    (rhsPtr->m_params.m_type != CToolParams::eCentreDrill)) return(true);

				if ((lhsPtr->m_params.m_type != CToolParams::eCentreDrill) &&
				    (rhsPtr->m_params.m_type == CToolParams::eCentreDrill)) return(false);

				// There is no preference for centre drill.  Neither tool is a centre drill.  Give preference
				// to a normal drill bit over a milling bit now.

				if ((lhsPtr->m_params.m_type == CToolParams::eDrill) &&
				    (rhsPtr->m_params.m_type != CToolParams::eDrill)) return(true);

				if ((lhsPtr->m_params.m_type != CToolParams::eDrill) &&
				    (rhsPtr->m_params.m_type == CToolParams::eDrill)) return(false);

				// Finally, give preference to a milling bit over a chamfer bit.
				if ((lhsPtr->m_params.m_type == CToolParams::eChamfer) &&
				    (rhsPtr->m_params.m_type != CToolParams::eChamfer)) return(false);

				if ((lhsPtr->m_params.m_type != CToolParams::eChamfer) &&
				    (rhsPtr->m_params.m_type == CToolParams::eChamfer)) return(true);
			} // End if - then
		} // End if - then

		// The execution orders are the same.  Let's group on tool number so as
		// to avoid unnecessary tool change operations.

		if (lhs->m_tool_number < rhs->m_tool_number) return(true);
		if (lhs->m_tool_number > rhs->m_tool_number) return(false);

		return(false);
	} // End operator
};


std::set<CFixture> CProgram::AllFixtures()
{
	std::set<CFixture> fixtures;

	for (HeeksObj *publicFixture = theApp.m_program->Fixtures()->GetFirstChild(); publicFixture != NULL;
        publicFixture = theApp.m_program->Fixtures()->GetNextChild())
	{
		fixtures.insert( *((CFixture *) publicFixture) );
	} // End for

    // Aggregate a list of all the fixtures we're going to use so that we can probe their heights before we start.
    for(HeeksObj* object = m_operations->GetFirstChild(); object; object = m_operations->GetNextChild())
	{
		if ((COperations::IsAnOperation(object->GetType())) &&
            (((COp*)object)->m_active))
		{
		    std::list<CFixture> private_fixtures = ((COp *) object)->PrivateFixtures();
            for (std::list<CFixture>::iterator itFix = private_fixtures.begin();
                    itFix != private_fixtures.end(); itFix++)
            {
				fixtures.insert( *itFix );
            }
		}
	}

	if (fixtures.size() == 0)
	{
		// We need at least one fixture definition to generate any GCode.  Generate one
		// that provides no rotation at all.

		fixtures.insert( CFixture( NULL, CFixture::G54, false, 0.0 ) );
	} // End if - then

	return(fixtures);
}



std::set<CFixture> CProgram::SelectFixturesToProcess(std::set<CFixture> fixtures)
{
	wxString message(_("Select which fixture(s) you want GCode generated for"));
	wxString caption(_("Fixture selection"));
	wxArrayString choices;

	choices.Add(_("All fixtures"));

	for (std::set<CFixture>::const_iterator l_itFixture = fixtures.begin(); l_itFixture != fixtures.end(); l_itFixture++)
    {
		wxString choice;
		choice << l_itFixture->m_title;
		choices.Add(choice);
	} // End for

	wxArrayInt selections;
	::wxGetMultipleChoices(selections, message, caption, choices );

	std::set<CFixture> chosen_fixtures;
	std::vector<CFixture> fixtures_array;
	std::copy( fixtures.begin(), fixtures.end(), std::inserter( fixtures_array, fixtures_array.begin() ));
	for (::size_t i=0; i<selections.size(); i++)
	{
		if (selections[i] == 0)
		{
			// We're doing all fixtures.
			std::copy( fixtures.begin(), fixtures.end(), std::inserter(chosen_fixtures, chosen_fixtures.end()));
		}
		else
		{
			// The offset through the 'chosen_fixtures' array is one less than that
			// of the selections array due to the 'all fixtures' option added at
			// the top of the selections list.  Hence the '-1'.
			chosen_fixtures.insert( fixtures_array[selections[i]-1] );
		}
	}
	fixtures.clear();
	std::copy( chosen_fixtures.begin(), chosen_fixtures.end(), std::inserter( fixtures, fixtures.begin() ));
	return(chosen_fixtures);
}



Python CProgram::RewritePythonProgram()
{
	Python python;



	theApp.m_program_canvas->m_textCtrl->Clear();
	CZigZag::number_for_stl_file = 1;
	CWaterline::number_for_stl_file = 1;
	CAttachOp::number_for_stl_file = 1;

	// call any OnRewritePython functions from other plugins
	for(std::list< void(*)() >::iterator It = theApp.m_OnRewritePython_list.begin(); It != theApp.m_OnRewritePython_list.end(); It++)
	{
		void(*callbackfunc)() = *It;
		(*callbackfunc)();
	}

	bool kurve_funcs_needed = false;
	bool area_module_needed = false;
	bool area_funcs_needed = false;
	bool ocl_module_needed = false;
	bool ocl_funcs_needed = false;
	bool nc_attach_needed = false;
	bool turning_module_needed = false;

	OperationsMap_t operations;

	if (m_operations == NULL)
	{
		// If there are no operations then there is no GCode.
		// No socks, no shirt, no service.
		return(python);
	} // End if - then

	for(HeeksObj* object = m_operations->GetFirstChild(); object; object = m_operations->GetNextChild())
	{
		operations.push_back( (COp *) object );

		if(((COp*)object)->m_active)
		{

			switch(object->GetType())
			{
			case ProfileType:
				area_module_needed = true;
				kurve_funcs_needed = true;
				break;

			case PocketType:
			case InlayType:
				area_module_needed = true;
				area_funcs_needed = true;
				break;

			case AttachOpType:
			case UnattachOpType:
			case ScriptOpType:
				ocl_module_needed = true;
				nc_attach_needed = true;
			case ZigZagType:
			case WaterlineType:
				ocl_funcs_needed = true;
				break;

			case TurnRoughType:
				area_module_needed = true;
				turning_module_needed = true;
			}
		}
	}

	// Sort the operations in order of execution_order and then by tool_number
	std::sort( operations.begin(), operations.end(), sort_operations() );

	// Language and Windows codepage detection and correction
	#ifndef WIN32
		python << _T("# coding=UTF8\n");
		python << _T("# No troubled Microsoft Windows detected\n");
	#else
		switch((wxLocale::GetSystemLanguage()))
		{
			case wxLANGUAGE_SLOVAK :
				python << _T("# coding=CP1250\n");
				python << _T("# Slovak language detected in Microsoft Windows\n");
				break;
			case wxLANGUAGE_GERMAN:
			case wxLANGUAGE_GERMAN_AUSTRIAN:
			case wxLANGUAGE_GERMAN_BELGIUM:
			case wxLANGUAGE_GERMAN_LIECHTENSTEIN:
			case wxLANGUAGE_GERMAN_LUXEMBOURG:
			case wxLANGUAGE_GERMAN_SWISS  :
				python << _T("# coding=CP1252\n");
				python << _T("# German language or it's variant detected in Microsoft Windows\n");
				break;
			case wxLANGUAGE_DUTCH  :
			case wxLANGUAGE_DUTCH_BELGIAN  :
				python << _T("# coding=CP1252\n");
				python << _T("# Dutch language or it's variant detected in Microsoft Windows\n");
				break;
			case wxLANGUAGE_FRENCH:
			case wxLANGUAGE_FRENCH_BELGIAN:
			case wxLANGUAGE_FRENCH_CANADIAN:
			case wxLANGUAGE_FRENCH_LUXEMBOURG:
			case wxLANGUAGE_FRENCH_MONACO:
			case wxLANGUAGE_FRENCH_SWISS:
				python << _T("# coding=CP1252\n");
				python << _T("# French language or it's variant detected in Microsoft Windows\n");
				break;
			case wxLANGUAGE_ITALIAN:
			case wxLANGUAGE_ITALIAN_SWISS :
				python << _T("# coding=CP1252\n");
				python << _T("#Italian language or it's variant detected in Microsoft Windows\n");
				break;
			case wxLANGUAGE_ENGLISH:
			case wxLANGUAGE_ENGLISH_UK:
			case wxLANGUAGE_ENGLISH_US:
			case wxLANGUAGE_ENGLISH_AUSTRALIA:
			case wxLANGUAGE_ENGLISH_BELIZE:
			case wxLANGUAGE_ENGLISH_BOTSWANA:
			case wxLANGUAGE_ENGLISH_CANADA:
			case wxLANGUAGE_ENGLISH_CARIBBEAN:
			case wxLANGUAGE_ENGLISH_DENMARK:
			case wxLANGUAGE_ENGLISH_EIRE:
			case wxLANGUAGE_ENGLISH_JAMAICA:
			case wxLANGUAGE_ENGLISH_NEW_ZEALAND:
			case wxLANGUAGE_ENGLISH_PHILIPPINES:
			case wxLANGUAGE_ENGLISH_SOUTH_AFRICA:
			case wxLANGUAGE_ENGLISH_TRINIDAD:
			case wxLANGUAGE_ENGLISH_ZIMBABWE:
				python << _T("# coding=CP1252\n");
				python << _T("#English language or it's variant detected in Microsoft Windows\n");
				break;
			default:
				python << _T("# coding=CP1252\n");
				python << _T("#Not supported language detected in Microsoft Windows. Assuming English alphabet\n");
				break;
		}
	#endif
	// add standard stuff at the top
	//hackhack, make it work on unix with FHS
	python << _T("import sys\n");

#ifdef CMAKE_UNIX
	#ifdef RUNINPLACE
	        python << _T("sys.path.insert(0,'") << theApp.GetResFolder() << _T("/')\n");
	#else
	        python << _T("sys.path.insert(0,'/usr/lib/heekscnc/')\n");
	#endif
#else
#ifndef WIN32
#ifndef RUNINPLACE
	python << _T("sys.path.insert(0,") << PythonString(_T("/usr/local/lib/heekscnc/")) << _T(")\n");
#endif
#endif
	python << _T("sys.path.insert(0,") << PythonString(theApp.GetResFolder()) << _T(")\n");
#endif
	python << _T("import math\n");

	// area related things
	if(area_module_needed)
	{
#ifdef WIN32
	python << _T("sys.path.insert(0,") << PythonString(theApp.GetResFolder() + (theApp.m_use_Clipper_not_Boolean ? _T("/Clipper"):_T("/Boolean"))) << _T(")\n");
#endif

		python << _T("import area\n");
		python << _T("area.set_units(") << m_units << _T(")\n");
	}

	// kurve related things
	if(kurve_funcs_needed)
	{
		python << _T("import kurve_funcs\n");
	}

	if(area_funcs_needed)
	{
		python << _T("import area_funcs\n");
	}

	// attach operations
	if(nc_attach_needed)
	{
		python << _T("import nc.attach\n");
	}

	// OpenCamLib stuff
	if(ocl_module_needed)
	{
		python << _T("import ocl\n");
	}
	if(ocl_funcs_needed)
	{
		python << _T("import ocl_funcs\n");
	}

	if(turning_module_needed)
	{
		python << _T("import turning\n");
		python << _T("\n");
	}


	// machine general stuff
	python << _T("from nc.nc import *\n");

	// specific machine
	if (m_machine.file_name == _T("not found"))
	{
		wxMessageBox(_T("Machine name (defined in Program Properties) not found"));
	} // End if - then
	else
	{
		python << _T("import nc.") + m_machine.file_name + _T("\n");
		python << _T("\n");
	} // End if - else

	// output file
	python << _T("output(") << PythonString(GetOutputFileName()) << _T(")\n");

	// begin program
	python << _T("program_begin(123, ") << PythonString(_T("Test program")) << _T(")\n");
	python << _T("absolute()\n");
	if(m_units > 25.0)
	{
		python << _T("imperial()\n");
	}
	else
	{
		python << _T("metric()\n");
	}
	python << _T("set_plane(0)\n");
	switch (m_emc2_variables_units)
	{
        case eMetric:
            python << _T("machine_units_metric(True)\n");
        case eImperial:
            python << _T("machine_units_metric(False)\n");
        default:
            break;
    }

	python << _T("\n");

	if (m_path_control_mode != ePathControlUndefined)
	{
		python << _T("set_path_control_mode(") << (int) m_path_control_mode << _T(",") << m_motion_blending_tolerance << _T(",") << m_naive_cam_tolerance << _T(")\n");
	}

	python << m_raw_material.AppendTextToProgram();

	// write the tools setup code.
	if (m_tools != NULL)
	{
		// Write the new tool table entries first.
		for(HeeksObj* object = m_tools->GetFirstChild(); object; object = m_tools->GetNextChild())
		{
			switch(object->GetType())
			{
			case ToolType:
				python << ((CTool*)object)->AppendTextToProgram();
				break;
			}
		} // End for
	} // End if - then

	// copied operations ( with same id ) were not being done, so I've removed fixtures completely for the Windows installation
	// Write all the operations once for each fixture.
	std::set<CFixture> fixtures = AllFixtures();

	if (fixtures.size() > 1)
	{
		// There are at least two different fixtures included in this model.  Ask the user which
		// ones they want GCode generated for.
		fixtures = SelectFixturesToProcess(fixtures);
	} // End if - then

	if (fixtures.size() == 0)
	{
		wxMessageBox(_("Aborting GCode generation as no fixtures were selected"));
		Python empty;
		return(empty);
	}

	CMachineState machine(&m_machine, *(fixtures.begin()));

    // Go through and probe each fixture (and the tool length switch) to determine the height offsets (if appropriate)
	python << machine.ToolChangeMovement_Preamble(fixtures);

	for (std::set<CFixture>::const_iterator l_itFixture = fixtures.begin(); l_itFixture != fixtures.end(); l_itFixture++)
	{
        // And then all the rest of the operations.
		for (OperationsMap_t::const_iterator l_itOperation = operations.begin(); l_itOperation != operations.end(); l_itOperation++)
		{
			HeeksObj *object = (HeeksObj *) *l_itOperation;
			if (object == NULL) continue;

			if(COperations::IsAnOperation(object->GetType()))
			{
				bool private_fixture_matches = false;
			    bool already_processed = false;
			    std::list<CFixture> private_fixtures = ((COp *) object)->PrivateFixtures();
			    for (std::list<CFixture>::iterator itFix = private_fixtures.begin();
                        itFix != private_fixtures.end(); itFix++)
                {
                    if (machine.AlreadyProcessed(object, *itFix)) already_processed = true;

					// Check to see if this private fixture is the same as the one we're processing.  If it's not then
					// don't process this time around.
					if (*itFix == *l_itFixture)
					{
						private_fixture_matches = true;
					}
                }

                if (private_fixtures.size() == 0)
                {
                    // Make sure the public fixture is in place.
                    python << machine.Fixture(*l_itFixture);
                }
				else
				{
					if (! private_fixture_matches) continue;	// Don't process this time around.
				}

				if ((! already_processed) &&
                    (((COp*)object)->m_active) &&
                    (! machine.AlreadyProcessed(object, *l_itFixture)))
				{
					python << ((COp*)object)->AppendTextToProgram( &machine );
					machine.MarkAsProcessed(object, machine.Fixture());
				}
			}
		} // End for - operation
	} // End for - fixture

	python << _T("program_end()\n");
	m_python_program = python;
	theApp.m_program_canvas->m_textCtrl->AppendText(python);
	if (python.Length() > theApp.m_program_canvas->m_textCtrl->GetValue().Length())
	{
		// The python program is longer than the text control object can handle.  The maximum
		// length of the text control objects changes depending on the operating system (and its
		// implementation of wxWidgets).  Rather than showing the truncated program, tell the
		// user that it has been truncated and where to find it.

		wxStandardPaths standard_paths;
		wxFileName file_str( standard_paths.GetTempDir().c_str(), _T("post.py"));

		theApp.m_program_canvas->m_textCtrl->Clear();
		theApp.m_program_canvas->m_textCtrl->AppendText(_("The Python program is too long \n"));
		theApp.m_program_canvas->m_textCtrl->AppendText(_("to display in this window.\n"));
		theApp.m_program_canvas->m_textCtrl->AppendText(_("Please edit the python program directly at \n"));
		theApp.m_program_canvas->m_textCtrl->AppendText(file_str.GetFullPath());
	}

	return(python);
}

ProgramUserType CProgram::GetUserType()
{
	if((m_nc_code != NULL) && (m_nc_code->m_user_edited)) return ProgramUserTypeNC;
	if(m_script_edited)return ProgramUserTypeScript;
	if((m_operations != NULL) && (m_operations->GetFirstChild()))return ProgramUserTypeTree;
	return ProgramUserTypeUnkown;
}

void CProgram::UpdateFromUserType()
{
#if 0
	switch(GetUserType())
	{
	case ProgramUserTypeUnkown:

	case tree:
   Enable "Operations" icon in tree
     editing the tree, recreates the python script
   Read only "Program" window
     pressing "Post-process" creates the NC code ( and backplots it )
   Read only "Output" window
   Disable all buttons on "Output" window

	}
#endif
}

// static
void CProgram::GetMachines(std::vector<CMachine> &machines)
{
	wxString machines_file = CProgram::alternative_machines_file;
#ifdef CMAKE_UNIX
	#ifdef RUNINPLACE
		if(machines_file.Len() == 0)machines_file = theApp.GetResFolder() + _T("/nc/machines.txt");
	#else
		if(machines_file.Len() == 0)machines_file = _T("/usr/lib/heekscnc/nc/machines.txt");
	#endif
#else
	if(machines_file.Len() == 0)machines_file = theApp.GetResFolder() + _T("/nc/machines.txt");
#endif
    ifstream ifs(Ttc(machines_file.c_str()));
	if(!ifs)
	{
#ifdef UNICODE
		std::wostringstream l_ossMessage;
#else
		std::ostringstream l_ossMessage;
#endif

		l_ossMessage << "Could not open '" << machines_file.c_str() << "' for reading";
		wxMessageBox( l_ossMessage.str().c_str() );
		return;
	}

	char str[1024] = "";

	while(!(ifs.eof()))
	{
		CMachine m;

		ifs.getline(str, 1024);

		m.configuration_file_name = machines_file;

		std::vector<wxString> tokens = Tokens( Ctt(str), _T(" \t\n\r") );

		// The first token is the machine name (post processor name)
		if (tokens.size() > 0) {
			m.file_name = tokens[0];
			tokens.erase(tokens.begin());
		} // End if - then

		// If there are other tokens, check the last one to see if it could be a maximum
		// spindle speed.
		if (tokens.size() > 0)
		{
			// We may have a material rate value.
			if (AllNumeric( *tokens.rbegin() ))
			{
				tokens.rbegin()->ToDouble(&(m.m_max_spindle_speed));
				tokens.resize(tokens.size() - 1);	// Remove last token.
			} // End if - then
		} // End if - then

		// Everything else must be a description.
		m.description.clear();
		for (std::vector<wxString>::const_iterator l_itToken = tokens.begin(); l_itToken != tokens.end(); l_itToken++)
		{
			if (l_itToken != tokens.begin()) m.description << _T(" ");
			m.description << *l_itToken;
		} // End for

		if(m.file_name.Length() > 0)
		{
			machines.push_back(m);
		}
	}

}

// static
CMachine CProgram::GetMachine(const wxString& file_name)
{
	std::vector<CMachine> machines;
	GetMachines(machines);
	for(unsigned int i = 0; i<machines.size(); i++)
	{
		if(machines[i].file_name == file_name)
		{
			return machines[i];
		}
	}

	if(machines.size() > 0)return machines[0];

	CMachine machine;

	return machine;
}

/**
	If the m_output_file_name_follows_data_file_name flag is true then
	we don't want to use the temporary directory.
 */
wxString CProgram::GetOutputFileName() const
{
	if (m_output_file_name_follows_data_file_name)
	{
		if (heeksCAD->GetFileFullPath())
		{
			wxString path(heeksCAD->GetFileFullPath());
			int offset = -1;
			if ((offset = path.Find('.', true)) != wxNOT_FOUND)
			{
				path.Remove(offset); // chop off the end.
			} // End if - then

			path << _T(".ngc");
			return(path);
		} // End if - then
		else
		{
			// The user hasn't assigned a filename yet.  Use the default.
			return(m_output_file);
		} // End if - else
	} // End if - then
	else
	{
		return(m_output_file);
	} // End if - else
} // End GetOutputFileName() method

wxString CProgram::GetBackplotFilePath() const
{
	// The creation of a ".nc.xml" file, in the users own folder, is not good.
	wxStandardPaths standard_paths;
	wxFileName file_str( standard_paths.GetTempDir().c_str(), _T("backplot.xml"));
	return file_str.GetFullPath();
}

CNCCode* CProgram::NCCode()
{
    if (m_nc_code == NULL) ReloadPointers();
    return m_nc_code;
}

COperations* CProgram::Operations()
{
    if (m_operations == NULL) ReloadPointers();
    return m_operations;
}

CTools* CProgram::Tools()
{
    if (m_tools == NULL) ReloadPointers();
    return m_tools;
}

CSpeedReferences *CProgram::SpeedReferences()
{
    if (m_speed_references == NULL) ReloadPointers();
    return m_speed_references;
}

CFixtures *CProgram::Fixtures()
{
    if (m_fixtures == NULL) ReloadPointers();
    return m_fixtures;
}

void CProgram::ReloadPointers()
{
    for (HeeksObj *child = GetFirstChild(); child != NULL; child = GetNextChild())
	{
	    if (child->GetType() == ToolsType) m_tools = (CTools *) child;
	    if (child->GetType() == FixturesType) m_fixtures = (CFixtures *) child;
	    if (child->GetType() == OperationsType) m_operations = (COperations *) child;
	    if (child->GetType() == SpeedReferencesType) m_speed_references = (CSpeedReferences *) child;
	    if (child->GetType() == NCCodeType) m_nc_code = (CNCCode *) child;
	} // End for
}

void CProgram::AddMissingChildren()
{
    // Make sure the pointers are not already amongst existing children.
	ReloadPointers();

	// make sure tools, operations, fixtures, etc. exist
	if(m_tools == NULL){m_tools = new CTools; Add( m_tools, NULL );}
	if(m_fixtures == NULL){m_fixtures = new CFixtures; Add( m_fixtures, NULL );}
	if(m_operations == NULL){m_operations = new COperations; Add( m_operations, NULL );}
	if(m_speed_references == NULL){m_speed_references = new CSpeedReferences; Add( m_speed_references, NULL );}
	if(m_nc_code == NULL){m_nc_code = new CNCCode; Add( m_nc_code, NULL );}
}

void CProgram::ChangeUnits( const double units )
{
    m_units = units;
    Tools()->OnChangeUnits(units);
    Operations()->OnChangeUnits(units);
}
bool CProgram::operator==( const CProgram & rhs ) const
{
	if (m_raw_material != rhs.m_raw_material) return(false);
	if (m_machine != rhs.m_machine) return(false);
	if (m_output_file != rhs.m_output_file) return(false);
	if (m_emc2_variables_file_name != rhs.m_emc2_variables_file_name) return(false);
	if (m_output_file_name_follows_data_file_name != rhs.m_output_file_name_follows_data_file_name) return(false);
	if (m_use_internal_backplotting != rhs.m_use_internal_backplotting) return(false);
	if (m_emc2_variables_units != rhs.m_emc2_variables_units) return(false);
	if (m_script_edited != rhs.m_script_edited) return(false);
	if (m_units != rhs.m_units) return(false);

	return(ObjList::operator==(rhs));
}


bool CMachine::operator==( const CMachine & rhs ) const
{
	if (configuration_file_name != rhs.configuration_file_name) return(false);
	if (file_name != rhs.file_name) return(false);
	if (description != rhs.description) return(false);
	if (m_max_spindle_speed != rhs.m_max_spindle_speed) return(false);
	if (m_safety_height_defined != rhs.m_safety_height_defined) return(false);
	if (m_safety_height != rhs.m_safety_height) return(false);
	if (m_clearance_height != rhs.m_clearance_height) return(false);
	if (m_tool_change_movement != rhs.m_tool_change_movement) return(false);
	if (m_tool_change_probe_depth != rhs.m_tool_change_probe_depth) return(false);
	if (m_tool_length_switch_number != rhs.m_tool_length_switch_number) return(false);
	if (m_fixture_probe_tool_number != rhs.m_fixture_probe_tool_number) return(false);
	if (m_tool_change_probe_feed_rate != rhs.m_tool_change_probe_feed_rate) return(false);
	if (m_nurbs_supported != rhs.m_nurbs_supported) return(false);
	if (m_pause_after_tool_change != rhs.m_pause_after_tool_change) return(false);
	if (m_skip_switch_and_fixture_probing_cycle != rhs.m_skip_switch_and_fixture_probing_cycle) return(false);
	if (m_auto_check_design_rules != rhs.m_auto_check_design_rules) return(false);

	return(true);
}



