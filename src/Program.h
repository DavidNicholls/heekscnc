// Program.h
// Copyright (c) 2009, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.

// one of these stores all the operations, and which machine it is for, and stuff like clamping info if I ever get round to it

#pragma once

#include "interface/ObjList.h"
#include "HeeksCNCTypes.h"
#include "RawMaterial.h"
#include "SpeedReferences.h"
#include "Op.h"

class CNCCode;
class CProgram;
class COperations;
class CFixtures;
class CFixture;
class CTools;
class CMachineState;

class CFixture;

enum ProgramUserType{
	ProgramUserTypeUnkown,
	ProgramUserTypeTree,
	ProgramUserTypeScript,
	ProgramUserTypeNC
};

class CMachine
{
public:
	CMachine();
	CMachine( const CMachine & rhs );
	CMachine & operator= ( const CMachine & rhs );

	typedef enum {
		ToolChangeMovement_NotDefined = 0,	// Don't do any extra movements when a tool change occurs.
		ToolChangeMovement_G28,				// Issue a G28 Z command and then G28 X Y command when changing tools
		ToolChangeMovement_G28_and_probe,	// Issue a G28 Z command and then G28 X Y command when changing tools
		ToolChangeMovement_G30,				// Issue a G30 Z command and then G28 X Y command when changing tools
		ToolChangeMovement_G30_and_probe,	// Issue a G30 Z command and then G28 X Y command when changing tools
		ToolChangeMovement_ExplicitlyDefined,	// Move to an explicit Z and then to explicity X,Y coordinates (in machine coordinates) when changing tools.
		ToolChangeMovement_ExplicitlyDefined_and_probe	// Move to an explicit Z and then to explicity X,Y coordinates (in machine coordinates) when changing tools.
	} eToolChangeMovement_t;

	wxString configuration_file_name;
	wxString file_name;
	wxString description;
	double m_max_spindle_speed;		// in revolutions per minute (RPM)
	bool m_safety_height_defined;
	double m_safety_height;
	eToolChangeMovement_t m_tool_change_movement;
	double m_explicit_tool_change_position[3];	// Only valid if m_tool_change_movement == ToolChangeMovement_ExplicitlyDefined - NOTE: These are in MACHINE coordinates (G53)
	double m_tool_change_probe_depth;	// Max distance from m_explicit_tool_change_position[2] downwards that the machine can handle.
	int m_tool_length_switch_number;	// tool number of probe switch (to get the height from) - used for tool changes.
	int m_fixture_probe_tool_number;	// tool number of probe switch (to get the surface height for each fixture) - used for tool changes.
	double m_tool_change_probe_feed_rate;
	double m_clearance_height;		// Default clearance height when CProgram::m_clearance_definition == eClearanceDefinedByMachine
	bool m_nurbs_supported;		// Does the machine's controller accept NURBS (G5.2) path definitions?
	bool m_pause_after_tool_change;	// To give the user an opportunity to clear away the tool length switch and turn on the spindle manually.
	bool m_skip_switch_and_fixture_probing_cycle; // NOTE: This is dangerous.  It saves time if the same program is repeatedly run but otherwise should be set to FALSE.
	bool m_auto_check_design_rules;	// Before generating GCode.

	void GetProperties(CProgram *parent, std::list<Property *> *list);
	void WriteBaseXML(TiXmlElement *element);
	void ReadBaseXML(TiXmlElement* element);

	static wxString ConfigScope() { return(_T("Machine")); }

	bool operator==( const CMachine & rhs ) const;
	bool operator!=( const CMachine & rhs ) const { return(! (*this == rhs)); }

};


class CProgram:public ObjList
{
private:
	CNCCode* m_nc_code;						// Access via NCCode() method
	COperations* m_operations;				// Access via Operations() method
	CTools* m_tools;						// Access via Tools() method
	CSpeedReferences *m_speed_references;	// Access via SpeedReferences() method
	CFixtures *m_fixtures;					// Access via Fixtures() method

public:
	static wxString ConfigScope(void) {return _T("Program");}

	typedef enum
	{
		eExactPathMode = 0,
		eExactStopMode,
		eBestPossibleSpeed,
		ePathControlUndefined
	} ePathControlMode_t;

	typedef enum
	{
		eClearanceDefinedByMachine = 0,
		eClearanceDefinedByFixture,
		eClearanceDefinedByOperation
	} eClearanceSource_t;

	typedef enum
	{
	    eUndefined = 0,
	    eMetric,
	    eImperial
	} eUnits_t;

	ePathControlMode_t m_path_control_mode;
	eClearanceSource_t m_clearance_source;
	double m_motion_blending_tolerance;	// Only valid if m_path_control_mode == eBestPossibleSpeed
	double m_naive_cam_tolerance;		// Only valid if m_path_control_mode == eBestPossibleSpeed

public:
	static wxString alternative_machines_file;
	CRawMaterial m_raw_material;	// for material hardness - to determine feeds and speeds.
	CMachine m_machine;
	wxString m_output_file;		// NOTE: Only relevant if the filename does NOT follow the data file's name.
	bool m_output_file_name_follows_data_file_name;	// Just change the extension to determine the NC file name
	bool m_use_internal_backplotting;
	wxString m_emc2_variables_file_name;
	eUnits_t m_emc2_variables_units;

	// Data access methods.
	CNCCode* NCCode();
	COperations* Operations();
	CTools* Tools();
	CSpeedReferences *SpeedReferences();
	CFixtures *Fixtures();

	CMachineState *m_active_machine_state;	// Pointer to current machine state (only valid during Python output)
	bool m_script_edited;
	double m_units; // 1.0 for mm, 25.4 for inches
	Python m_python_program;

	CProgram();
	CProgram( const CProgram & rhs );
	CProgram & operator= ( const CProgram & rhs );
	~CProgram();

	bool operator== ( const CProgram & rhs ) const;
	bool operator!= ( const CProgram & rhs ) const { return(! (*this == rhs)); }

	bool IsDifferent( HeeksObj *other ) { return(*this != (*(CProgram *)other)); }

	wxString GetOutputFileName() const;
	wxString GetBackplotFilePath() const;

	// HeeksObj's virtual functions
	int GetType()const{return ProgramType;}
	const wxChar* GetTypeString(void)const{return _T("Program");}
	const wxBitmap &GetIcon();
	HeeksObj *MakeACopy(void)const;
	void CopyFrom(const HeeksObj* object);
	void GetProperties(std::list<Property *> *list);
	void WriteXML(TiXmlNode *root);
	bool Add(HeeksObj* object, HeeksObj* prev_object);
	void Remove(HeeksObj* object);
	bool CanBeRemoved(){return false;}
	bool CanAdd(HeeksObj* object);
	bool CanAddTo(HeeksObj* owner);
	bool OneOfAKind(){return true;}
	void SetClickMarkPoint(MarkedObject* marked_object, const double* ray_start, const double* ray_direction);
	bool AutoExpand(){return true;}
	static HeeksObj* ReadFromXMLElement(TiXmlElement* pElem);

	Python RewritePythonProgram();
	ProgramUserType GetUserType();
	void UpdateFromUserType();

	static void GetMachines(std::vector<CMachine> &machines);
	static CMachine GetMachine(const wxString& file_name);
	void AddMissingChildren();

	void ChangeUnits( const double units );
	void ReloadPointers();
	void SetEmc2VariablesFileName(const wxChar *value);

	typedef std::vector< COp * > OperationsMap_t;

	std::set<CFixture> AllFixtures();
	std::set<CFixture> SelectFixturesToProcess(std::set<CFixture> fixtures);
};
