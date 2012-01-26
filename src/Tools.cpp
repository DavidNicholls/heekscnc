// Tools.cpp
// Copyright (c) 2009, Dan Heeks
// This program is released under the BSD license. See the file COPYING for details.

#include "stdafx.h"
#include "Tools.h"
#include "Program.h"
#include "interface/Tool.h"
#include "CTool.h"
#include "tinyxml/tinyxml.h"
#include <wx/stdpaths.h>

#include <iostream>
#include <fstream>

bool CTools::CanAdd(HeeksObj* object)
{
	return 	((object != NULL) && (object->GetType() == ToolType));
}


HeeksObj *CTools::MakeACopy(void) const
{
    return(new CTools(*this));  // Call the copy constructor.
}


CTools::CTools()
{
    CNCConfig config(CTools::ConfigScope());
	config.Read(_T("title_format"), (int *) (&m_title_format), int(eGuageReplacesSize) );
}


CTools::CTools( const CTools & rhs ) : ObjList(rhs)
{
    m_title_format = rhs.m_title_format;
}

CTools & CTools::operator= ( const CTools & rhs )
{
    if (this != &rhs)
    {
        ObjList::operator=( rhs );
        m_title_format = rhs.m_title_format;
    }
    return(*this);
}


const wxBitmap &CTools::GetIcon()
{
	static wxBitmap* icon = NULL;
	if(icon == NULL)icon = new wxBitmap(wxImage(theApp.GetResFolder() + _T("/icons/tools.png")));
	return *icon;
}

/**
	We need to copy the tools from the CTools object passed in into our own
	list.  We don't want to duplicate tools that are already in our local tool table.
	If we import a tool, we need to make sure the tool number is unique within the
	whole tool table.  If we need to renumber a tool during this import, we need to
	also update any associated Operations objects that refer to this tool number
	so that they now point to the new tool number.
 */
void CTools::CopyFrom(const HeeksObj* object)
{
    /*
    if (object->GetType() == GetType())
    {
        for (HeeksObj *tool = object->GetFirstChild(); tool != NULL; tool = object->GetNextChild())
        {

        }
    }
    */
}

void CTools::WriteXML(TiXmlNode *root)
{
	TiXmlElement * element;
	element = heeksCAD->NewXMLElement( "Tools" );
	heeksCAD->LinkXMLEndChild( root,  element );
	WriteBaseXML(element);
}

//static
HeeksObj* CTools::ReadFromXMLElement(TiXmlElement* pElem)
{
	CTools* new_object = new CTools;
	new_object->ReadBaseXML(pElem);
	return new_object;
}


class GenerateEMC2ToolTable: public Tool{
	// Tool's virtual functions
	const wxChar* GetTitle(){return _("Generate EMC2 Tool Table");}
	void Run()
	{
		wxStandardPaths standard_paths;
		if (path.Length() == 0) path = _T("tool.tbl");

		// Prompt the user to select a file to import.
		wxFileDialog fd(heeksCAD->GetMainFrame(), _T("Select a file to export to"),
		standard_paths.GetUserConfigDir().c_str(), path.c_str(),
				wxString(_("Known Files")) + _T(" |*.heeks;*.HEEKS;")
					+ _T("*.tbl;*.TBL;*.Tbl;"),
					wxSAVE | wxOVERWRITE_PROMPT );

		fd.SetFilterIndex(1);
		if (fd.ShowModal() == wxID_CANCEL) return;
		path = fd.GetPath().c_str();

		// Define some structures so that we can format the file for readability purposes.
		typedef wxString Column_t;
		typedef std::vector< Column_t > Row_t;
		typedef std::vector< Row_t > Table_t;

		Table_t table;

		unsigned int pocket_number = 1;	// Assign as we go along.

		Row_t heading;
		heading.push_back(_("; Tool#"));
		heading.push_back(_("Pocket#"));
		heading.push_back(_("X offset"));
		heading.push_back(_("Y offset"));
		heading.push_back(_("Z offset"));
		heading.push_back(_("A offset"));
		heading.push_back(_("B offset"));
		heading.push_back(_("C offset"));
		heading.push_back(_("U offset"));
		heading.push_back(_("V offset"));
		heading.push_back(_("W offset"));
		heading.push_back(_("Diameter"));
		heading.push_back(_("Front Angle"));
		heading.push_back(_("Back Angle"));
		heading.push_back(_("Orientation"));
		heading.push_back(_("Remark"));
		// table.push_back(heading);	// Don't add the heading.  It only throws a warning when EMC2 starts up.

		double machine_units = 1.0;
		if (theApp.m_program->m_emc2_variables_units == CProgram::eImperial)
		{
			machine_units = 25.4;
		}

		std::list<HeeksObj *> tools;
		for (HeeksObj *tool = theApp.m_program->Tools()->GetFirstChild();
			tool != NULL;
			tool = theApp.m_program->Tools()->GetNextChild() )
		{
			Row_t	row;
			CTool *pTool = (CTool *) tool;

			{ wxString value; value << _T("T") << pTool->m_tool_number; row.push_back(value); }
			{ wxString value; value << _T("P") << pocket_number++; row.push_back(value); }
			{ wxString value; value << _T("X") << pTool->m_params.m_x_offset / machine_units; row.push_back(value); }	// X offset
			{ wxString value; value << _T("Y") << 0.0 / machine_units; row.push_back(value); }	// Y offset
			{ wxString value; value << _T("Z") << pTool->m_params.m_tool_length_offset / machine_units; row.push_back(value); }	// Z offset
			{ wxString value; value << _T("A") << 0.0 / machine_units; row.push_back(value); }	// A offset
			{ wxString value; value << _T("B") << 0.0 / machine_units; row.push_back(value); }	// B offset
			{ wxString value; value << _T("C") << 0.0 / machine_units; row.push_back(value); }	// C offset
			{ wxString value; value << _T("U") << 0.0 / machine_units; row.push_back(value); }	// U offset
			{ wxString value; value << _T("V") << 0.0 / machine_units; row.push_back(value); }	// V offset
			{ wxString value; value << _T("W") << 0.0 / machine_units; row.push_back(value); }	// W offset
			{ wxString value; value << _T("D") << pTool->CuttingRadius() * 2.0 / machine_units; row.push_back(value); }	// Diameter

			if (pTool->m_params.m_type == CToolParams::eTurningTool)
			{
				{ wxString value; value << _T("I") << pTool->m_params.m_front_angle; row.push_back(value); }	// Front angle
				{ wxString value; value << _T("J") << pTool->m_params.m_back_angle; row.push_back(value); }	// Back angle
				{ wxString value; value << _T("Q") << pTool->m_params.m_orientation; row.push_back(value); }	// Orientation
			}
			else
			{
				// Add dummy values to the row so the formatting looks alright.
				row.push_back(_T(" "));
				row.push_back(_T(" "));
				row.push_back(_T(" "));
			}

			{ wxString value; value << _T("; ") << pTool->m_title; row.push_back(value); }	// Remark

			table.push_back(row);
		} // End for

        #if wxUSE_UNICODE
        #ifdef __WXMSW__
            std::wofstream ofs(path);
        #else
            std::wofstream ofs(Ttc(path.c_str()));
        #endif
        #else
            std::ofstream ofs(path);
        #endif

		if (ofs.is_open())
		{
		    ofs.imbue(std::locale("C"));

			// Setup max column widths so we can format neatly.
			std::vector<unsigned int> column_widths;
			for (Table_t::size_type row = 0; row < table.size(); row++)
			{
				for (Row_t::size_type col = 0; col < table[row].size(); col++)
				{
					if (column_widths.size() < (table[row].size() + 1))
					{
						column_widths.push_back( table[row][col].size() + 1 );
					}
					else
					{
						if (column_widths[col] < (table[row][col].size() + 1))
						{
							column_widths[col] = table[row][col].size() + 1;
						}
					}
				} // End for
			} // End for

			// Write the data to the file.
			for (Table_t::size_type row = 0; row < table.size(); row++)
			{
				for (Row_t::size_type col = 0; col < table[row].size(); col++)
				{
				    ofs << table[row][col].utf8_str();

					for (size_t i=table[row][col].size(); i<column_widths[col]; i++)
					{
					    ofs << _T(" ");
					}
				} // End for

				ofs << _T("\n");
			} // End for

			if (pocket_number > 56)
			{
				wxString message;
				message << _("Warning: Tool table may only contain 56 entries but this table has ") << pocket_number-1;
				wxMessageBox(message);
			}
			ofs.close();
		}
		else
		{
			wxString error;
			error << _("Could not open ") << path << _(" for writing");
			wxMessageBox(error);
		}
	}
	wxString BitmapPath(){ return _T("export");}
	wxString path;
};

static GenerateEMC2ToolTable generate_emc2_tool_table;



class ExportTools: public Tool{
	// Tool's virtual functions
	const wxChar* GetTitle(){return _("Export");}
	void Run()
	{
		wxStandardPaths standard_paths;
		if (previous_path.Length() == 0) previous_path = _T("default.tooltable");

		// Prompt the user to select a file to import.
		wxFileDialog fd(heeksCAD->GetMainFrame(), _T("Select a file to export to"),
			theApp.m_startup_files_directory.c_str(), previous_path.c_str(),
				wxString(_("Known Files")) + _T(" |*.heeks;*.HEEKS;")
					+ _T("*.tool;*.TOOL;*.Tool;")
					+ _T("*.tools;*.TOOLS;*.Tools;")
					+ _T("*.tooltable;*.TOOLTABLE;*.ToolTable;"),
					wxSAVE | wxOVERWRITE_PROMPT );

		fd.SetFilterIndex(1);
		if (fd.ShowModal() == wxID_CANCEL) return;
		previous_path = fd.GetPath().c_str();
		std::list<HeeksObj *> tools;
		for (HeeksObj *tool = theApp.m_program->Tools()->GetFirstChild();
			tool != NULL;
			tool = theApp.m_program->Tools()->GetNextChild() )
		{
			tools.push_back( tool );
		} // End for

		heeksCAD->SaveXMLFile( tools, previous_path.c_str(), false );
	}
	wxString BitmapPath(){ return _T("export");}
	wxString previous_path;
};

static ExportTools export_tools;

void ImportToolsFile( const wxChar *file_path )
{
    // Delete the speed references that we've already got.  Otherwise we end
    // up with duplicates.  Do this in two passes.  Otherwise we end up
    // traversing the same list that we're modifying.

    std::list<HeeksObj *> tools;
    for (HeeksObj *tool = theApp.m_program->Tools()->GetFirstChild();
        tool != NULL;
        tool = theApp.m_program->Tools()->GetNextChild() )
    {
        tools.push_back( tool );
    } // End for

    for (std::list<HeeksObj *>::iterator l_itObject = tools.begin(); l_itObject != tools.end(); l_itObject++)
    {
        heeksCAD->Remove( *l_itObject );
    } // End for

    // And read the default speed references.
    // heeksCAD->OpenXMLFile( _T("default.speeds"), true, theApp.m_program->m_tools );
    heeksCAD->OpenXMLFile( file_path, theApp.m_program->Tools() );
}

class ImportTools: public Tool{
	// Tool's virtual functions
	const wxChar* GetTitle(){return _("Import");}
	void Run()
	{
		wxStandardPaths standard_paths;
		if (previous_path.Length() == 0) previous_path = _T("default.tooltable");


		// Prompt the user to select a file to import.
		wxFileDialog fd(heeksCAD->GetMainFrame(), _T("Select a file to import"),
				theApp.m_startup_files_directory.c_str(), previous_path.c_str(),
				wxString(_("Known Files")) + _T(" |*.heeks;*.HEEKS;")
					+ _T("*.tool;*.TOOL;*.Tool;")
					+ _T("*.tools;*.TOOLS;*.Tools;")
					+ _T("*.tooltable;*.TOOLTABLE;*.ToolTable;"),
					wxOPEN | wxFILE_MUST_EXIST );
		fd.SetFilterIndex(1);
		if (fd.ShowModal() == wxID_CANCEL) return;
		previous_path = fd.GetPath().c_str();

        ImportToolsFile( previous_path.c_str() );
	}
	wxString BitmapPath(){ return _T("import");}
	wxString previous_path;
};

static ImportTools import_tools;

void CTools::GetTools(std::list<Tool*>* t_list, const wxPoint* p)
{
	t_list->push_back(&import_tools);
	t_list->push_back(&export_tools);
	t_list->push_back(&generate_emc2_tool_table);

	CHeeksCNCApp::GetNewToolTools(t_list);

	ObjList::GetTools(t_list, p);
}


void CTools::OnChangeUnits(const double units)
{
    std::list<HeeksObj *> tools;
    for (HeeksObj *tool = theApp.m_program->Tools()->GetFirstChild();
        tool != NULL;
        tool = theApp.m_program->Tools()->GetNextChild() )
    {
        tools.push_back( tool );
    } // End for

    for (std::list<HeeksObj *>::iterator l_itObject = tools.begin(); l_itObject != tools.end(); l_itObject++)
    {
        ((CTool *) *l_itObject)->ResetTitle();
    } // End for
}


static void on_set_title_format(int value, HeeksObj* object)
{
	((CTools *)object)->m_title_format = CTools::TitleFormat_t(value);

	CNCConfig config(CTools::ConfigScope());
	config.Write(_T("title_format"), ((CTools *)object)->m_title_format);
}

void CTools::GetProperties(std::list<Property *> *list)
{
	{
		std::list< wxString > choices;
		choices.push_back( _("Guage number replaces size") );
		choices.push_back( _("Include guage number and size") );

		list->push_back ( new PropertyChoice ( _("Title Format"),  choices, m_title_format, this, on_set_title_format ) );
	}
	HeeksObj::GetProperties(list);
}


