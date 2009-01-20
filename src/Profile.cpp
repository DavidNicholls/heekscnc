// Profile.cpp

#include "stdafx.h"
#include "Profile.h"
#include "CNCConfig.h"
#include "ProgramCanvas.h"
#include "../../interface/HeeksObj.h"
#include "../../interface/PropertyDouble.h"
#include "../../interface/PropertyChoice.h"
#include "../../interface/PropertyVertex.h"
#include "../../interface/PropertyCheck.h"
#include "../../tinyxml/tinyxml.h"

CProfileParams::CProfileParams()
{
	m_tool_diameter = 0.0;
	m_clearance_height = 0.0;
	m_final_depth = 0.0;
	m_rapid_down_to_height = 0.0;
	m_horizontal_feed_rate = 0.0;
	m_vertical_feed_rate = 0.0;
	m_spindle_speed = 0.0;
	m_tool_on_side = 0;
	m_auto_roll_on = true;
	m_auto_roll_off = true;
	m_roll_on_point[0] = m_roll_on_point[1] = m_roll_on_point[2] = 0.0;
	m_roll_off_point[0] = m_roll_off_point[1] = m_roll_off_point[2] = 0.0;
}

void CProfileParams::set_initial_values()
{
	CNCConfig config;
	config.Read(_T("ProfileToolDiameter"), &m_tool_diameter, 3.0);
	config.Read(_T("ProfileClearanceHeight"), &m_clearance_height, 5.0);
	config.Read(_T("ProfileFinalDepth"), &m_final_depth, -0.1);
	config.Read(_T("ProfileRapidDown"), &m_rapid_down_to_height, 2.0);
	config.Read(_T("ProfileHorizFeed"), &m_horizontal_feed_rate, 100.0);
	config.Read(_T("ProfileVertFeed"), &m_vertical_feed_rate, 100.0);
	config.Read(_T("ProfileSpindleSpeed"), &m_spindle_speed, 7000);
	config.Read(_T("ProfileToolOnSide"), &m_tool_on_side, 1);
	config.Read(_T("ProfileAutoRollOn"), &m_auto_roll_on, true);
	if(m_auto_roll_on)
	{
		m_roll_on_point[0] = m_roll_on_point[1] = m_roll_on_point[2] = 0.0;
	}
	else
	{
		config.Read(_T("ProfileRollOnX"), &m_roll_on_point[0], 0.0);
		config.Read(_T("ProfileRollOnY"), &m_roll_on_point[1], 0.0);
		config.Read(_T("ProfileRollOnZ"), &m_roll_on_point[2], 0.0);
	}
	config.Read(_T("ProfileAutoRollOff"), &m_auto_roll_off, true);
	if(m_auto_roll_off)
	{
		m_roll_off_point[0] = m_roll_off_point[1] = m_roll_off_point[2] = 0.0;
	}
	else
	{
		config.Read(_T("ProfileRollOffX"), &m_roll_off_point[0], 0.0);
		config.Read(_T("ProfileRollOffY"), &m_roll_off_point[1], 0.0);
		config.Read(_T("ProfileRollOffZ"), &m_roll_off_point[2], 0.0);
	}
}

void CProfileParams::write_values_to_config()
{
	CNCConfig config;
	config.Write(_T("ProfileToolDiameter"), m_tool_diameter);
	config.Write(_T("ProfileClearanceHeight"), m_clearance_height);
	config.Write(_T("ProfileFinalDepth"), m_final_depth);
	config.Write(_T("ProfileRapidDown"), m_rapid_down_to_height);
	config.Write(_T("ProfileHorizFeed"), m_horizontal_feed_rate);
	config.Write(_T("ProfileVertFeed"), m_vertical_feed_rate);
	config.Write(_T("ProfileSpindleSpeed"), m_spindle_speed);
	config.Write(_T("ProfileToolOnSide"), m_tool_on_side);
	config.Write(_T("ProfileAutoRollOn"), m_auto_roll_on);
	config.Write(_T("ProfileRollOnX"), m_roll_on_point[0]);
	config.Write(_T("ProfileRollOnY"), m_roll_on_point[1]);
	config.Write(_T("ProfileRollOnZ"), m_roll_on_point[2]);
	config.Write(_T("ProfileAutoRollOff"), m_auto_roll_off);
	config.Write(_T("ProfileRollOffX"), m_roll_off_point[0]);
	config.Write(_T("ProfileRollOffY"), m_roll_off_point[1]);
	config.Write(_T("ProfileRollOffZ"), m_roll_off_point[2]);
}

static void on_set_tool_diameter(double value, HeeksObj* object){((CProfile*)object)->m_params.m_tool_diameter = value;}
static void on_set_clearance_height(double value, HeeksObj* object){((CProfile*)object)->m_params.m_clearance_height = value;}
static void on_set_final_depth(double value, HeeksObj* object){((CProfile*)object)->m_params.m_final_depth = value;}
static void on_set_rapid_down_to_height(double value, HeeksObj* object){((CProfile*)object)->m_params.m_rapid_down_to_height = value;}
static void on_set_horizontal_feed_rate(double value, HeeksObj* object){((CProfile*)object)->m_params.m_horizontal_feed_rate = value;}
static void on_set_vertical_feed_rate(double value, HeeksObj* object){((CProfile*)object)->m_params.m_vertical_feed_rate = value;}
static void on_set_spindle_speed(double value, HeeksObj* object){((CProfile*)object)->m_params.m_spindle_speed = value;}
static void on_set_tool_on_side(int value, HeeksObj* object){
	switch(value)
	{
	case 0:
		((CProfile*)object)->m_params.m_tool_on_side = 1;
		break;
	case 1:
		((CProfile*)object)->m_params.m_tool_on_side = -1;
		break;
	default:
		((CProfile*)object)->m_params.m_tool_on_side = 0;
		break;
	}
}
static void on_set_auto_roll_on(bool value, HeeksObj* object){((CProfile*)object)->m_params.m_auto_roll_on = value; heeksCAD->RefreshProperties();}
static void on_set_roll_on_point(const double* vt, HeeksObj* object){memcpy(((CProfile*)object)->m_params.m_roll_on_point, vt, 3*sizeof(double));}
static void on_set_auto_roll_off(bool value, HeeksObj* object){((CProfile*)object)->m_params.m_auto_roll_off = value; heeksCAD->RefreshProperties();}
static void on_set_roll_off_point(const double* vt, HeeksObj* object){memcpy(((CProfile*)object)->m_params.m_roll_off_point, vt, 3*sizeof(double));}

void CProfileParams::GetProperties(CProfile* parent, std::list<Property *> *list)
{
	list->push_back(new PropertyDouble(_("tool diameter"), m_tool_diameter, parent, on_set_tool_diameter));
	list->push_back(new PropertyDouble(_("clearance height"), m_clearance_height, parent, on_set_clearance_height));
	list->push_back(new PropertyDouble(_("final depth"), m_final_depth, parent, on_set_final_depth));
	list->push_back(new PropertyDouble(_("rapid down to height"), m_rapid_down_to_height, parent, on_set_rapid_down_to_height));
	list->push_back(new PropertyDouble(_("horizontal feed rate"), m_horizontal_feed_rate, parent, on_set_horizontal_feed_rate));
	list->push_back(new PropertyDouble(_("vertical feed rate"), m_vertical_feed_rate, parent, on_set_vertical_feed_rate));
	list->push_back(new PropertyDouble(_("spindle speed"), m_spindle_speed, parent, on_set_spindle_speed));
	{
		std::list< wxString > choices;
		choices.push_back(_("Left"));
		choices.push_back(_("Right"));
		choices.push_back(_("On"));
		int choice = 0;
		if(m_tool_on_side == -1)choice = 1;
		else if(m_tool_on_side == 0)choice = 2;
		list->push_back(new PropertyChoice(_("tool on side"), choices, choice, parent, on_set_tool_on_side));
	}
	list->push_back(new PropertyCheck(_("auto roll on"), m_auto_roll_on, parent, on_set_auto_roll_on));
	if(!m_auto_roll_on)list->push_back(new PropertyVertex(_("roll_on_point"), m_roll_on_point, parent, on_set_roll_on_point));
	list->push_back(new PropertyCheck(_("auto roll off"), m_auto_roll_off, parent, on_set_auto_roll_off));
	if(!m_auto_roll_off)list->push_back(new PropertyVertex(_("roll_off_point"), m_roll_off_point, parent, on_set_roll_off_point));
}

void CProfileParams::WriteXMLAttributes(TiXmlElement *root)
{
	TiXmlElement * element;
	element = new TiXmlElement( "params" );
	root->LinkEndChild( element );  
	element->SetAttribute("toold", m_tool_diameter);
	element->SetAttribute("clear", m_clearance_height);
	element->SetAttribute("depth", m_final_depth);
	element->SetAttribute("r", m_rapid_down_to_height);
	element->SetAttribute("hfeed", m_horizontal_feed_rate);
	element->SetAttribute("vfeed", m_vertical_feed_rate);
	element->SetAttribute("spin", m_spindle_speed);
	element->SetAttribute("side", m_tool_on_side);
	element->SetAttribute("auto_roll_on", m_auto_roll_on ? 1:0);
	if(!m_auto_roll_on)
	{
		element->SetAttribute("roll_onx", m_roll_on_point[0]);
		element->SetAttribute("roll_ony", m_roll_on_point[1]);
		element->SetAttribute("roll_onz", m_roll_on_point[2]);
	}
	element->SetAttribute("auto_roll_off", m_auto_roll_off ? 1:0);
	if(!m_auto_roll_off)
	{
		element->SetAttribute("roll_offx", m_roll_off_point[0]);
		element->SetAttribute("roll_offy", m_roll_off_point[1]);
		element->SetAttribute("roll_offz", m_roll_off_point[2]);
	}
}

void CProfileParams::ReadFromXMLElement(TiXmlElement* pElem)
{
	// get the attributes
	for(TiXmlAttribute* a = pElem->FirstAttribute(); a; a = a->Next())
	{
		std::string name(a->Name());
		if(name == "toold"){m_tool_diameter = a->DoubleValue();}
		else if(name == "clear"){m_clearance_height = a->DoubleValue();}
		else if(name == "depth"){m_final_depth = a->DoubleValue();}
		else if(name == "r"){m_rapid_down_to_height = a->DoubleValue();}
		else if(name == "hfeed"){m_horizontal_feed_rate = a->DoubleValue();}
		else if(name == "vfeed"){m_vertical_feed_rate = a->DoubleValue();}
		else if(name == "spin"){m_spindle_speed = a->DoubleValue();}
		else if(name == "side"){m_tool_on_side = a->IntValue();}
		else if(name == "auto_roll_on"){m_auto_roll_on = (a->IntValue() != 0);}
		else if(name == "roll_onx"){m_roll_on_point[0] = a->DoubleValue();}
		else if(name == "roll_ony"){m_roll_on_point[1] = a->DoubleValue();}
		else if(name == "roll_onz"){m_roll_on_point[2] = a->DoubleValue();}
		else if(name == "auto_roll_off"){m_auto_roll_off = (a->IntValue() != 0);}
		else if(name == "roll_offx"){m_roll_off_point[0] = a->DoubleValue();}
		else if(name == "roll_offy"){m_roll_off_point[1] = a->DoubleValue();}
		else if(name == "roll_offz"){m_roll_off_point[2] = a->DoubleValue();}
	}
}

void CProfileParams::GetRollOnPos(HeeksObj* sketch, double &x, double &y)
{
	// roll on
	if(m_auto_roll_on)
	{
	}
	else
	{
		x = m_roll_on_point[0];
		y = m_roll_on_point[1];
	}
}

void CProfileParams::GetRollOffPos(HeeksObj* sketch, double &x, double &y)
{
	// roll off
	if(m_auto_roll_off)
	{
	}
	else
	{
		x = m_roll_off_point[0];
		y = m_roll_off_point[1];
	}
}

static void WriteSketchDefn(HeeksObj* sketch)
{
	theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("k%d = kurve.new()\n"), sketch->m_id));

	bool started = false;

	for(HeeksObj* span_object = sketch->GetFirstChild(); span_object; span_object = sketch->GetNextChild())
	{
		double s[3] = {0, 0, 0};
		double e[3] = {0, 0, 0};
		double c[3] = {0, 0, 0};

		if(span_object){
			int type = span_object->GetType();
			if(type == LineType || type == ArcType)
			{
				if(!started)
				{
					span_object->GetStartPoint(s);
					theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("kurve.add_point(k%d, %d, %g, %g, %g, %g)\n"), sketch->m_id, 0, s[0], s[1], 0.0, 0.0));
					started = true;
				}
				span_object->GetEndPoint(e);
				if(type == LineType)
				{
					theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("kurve.add_point(k%d, %d, %g, %g, %g, %g)\n"), sketch->m_id, 0, e[0], e[1], 0.0, 0.0));
				}
				else if(type == ArcType)
				{
					span_object->GetCentrePoint(c);
					double pos[3];
					heeksCAD->GetArcAxis(span_object, pos);
					int span_type = (pos[2] >=0) ? 1:-1;
					theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("kurve.add_point(k%d, %d, %g, %g, %g, %g)\n"), sketch->m_id, span_type, e[0], e[1], c[0], c[1]));
				}
			}
		}
	}

	theApp.m_program_canvas->m_textCtrl->AppendText(_T("\n"));
}

void CProfile::AppendTextToProgram()
{
	theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("clearance = %g\n"), m_params.m_clearance_height));
	theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("rapid_down_to_height = %g\n"), m_params.m_rapid_down_to_height));
	theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("final_depth = %g\n"), m_params.m_final_depth));
	theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("tool_diameter = %g\n"), m_params.m_tool_diameter));
	theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("spindle(%g)\n"), m_params.m_spindle_speed));
	theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("feedrate(%g)\n"), m_params.m_horizontal_feed_rate));
	theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("tool_change(1)\n")));
	for(std::list<int>::iterator It = m_sketches.begin(); It != m_sketches.end(); It++)
	{
		int sketch = *It;

		// write a kurve definition
		HeeksObj* object = heeksCAD->GetIDObject(SketchType, sketch);
		if(object)
		{
			WriteSketchDefn(object);

			// start - assume we are at a suitable clearance height

			// get roll on position
			double sx = 0.0, sy =0.0;
			m_params.GetRollOnPos(object, sx, sy);

			// rapid across to it
			theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("rapid(%lf, %lf)\n"), sx, sy));

			// rapid down to just above the material
			theApp.m_program_canvas->m_textCtrl->AppendText(wxString(_T("rapid(z = rapid_down_to_height)\n")));

			// feed down to final depth
			theApp.m_program_canvas->m_textCtrl->AppendText(wxString(_T("feed(z = final_depth)\n")));			

			// get roll off position
			double ex = 0.0, ey =0.0;
			m_params.GetRollOffPos(object, ex, ey);

			// get offset side
			wxString side_string;
			switch(m_params.m_tool_on_side)
			{
			case 1:
				side_string = _T("left");
				break;
			case -1:
				side_string = _T("right");
				break;
			default:
				side_string = _T("on");
				break;
			}

			// profile the kurve
			theApp.m_program_canvas->m_textCtrl->AppendText(wxString::Format(_T("stdops.profile(k%d, %lf, %lf, '%s', tool_diameter/2, %lf, %lf)\n"), sketch, sx, sy, side_string.c_str(), ex, ey));

			// rapid back up to clearance plane
			theApp.m_program_canvas->m_textCtrl->AppendText(wxString(_T("rapid(z = clearance)\n")));			
		}
	}
}

void CProfile::glCommands(bool select, bool marked, bool no_color)
{
	if(marked && !no_color)
	{
		for(std::list<int>::iterator It = m_sketches.begin(); It != m_sketches.end(); It++)
		{
			int sketch = *It;
			HeeksObj* object = heeksCAD->GetIDObject(SketchType, sketch);
			if(object)object->glCommands(false, true, false);
		}
	}
}

void CProfile::GetProperties(std::list<Property *> *list)
{
	m_params.GetProperties(this, list);
	HeeksObj::GetProperties(list);
}

HeeksObj *CProfile::MakeACopy(void)const
{
	return new CProfile(*this);
}

void CProfile::CopyFrom(const HeeksObj* object)
{
	operator=(*((CProfile*)object));
}

bool CProfile::CanAddTo(HeeksObj* owner)
{
	return owner->GetType() == OperationsType;
}

void CProfile::WriteXML(TiXmlElement *root)
{
	TiXmlElement * element = new TiXmlElement( "Profile" );
	root->LinkEndChild( element );  
	m_params.WriteXMLAttributes(element);

	// write sketch ids
	for(std::list<int>::iterator It = m_sketches.begin(); It != m_sketches.end(); It++)
	{
		int sketch = *It;
		TiXmlElement * sketch_element = new TiXmlElement( "sketch" );
		element->LinkEndChild( sketch_element );  
		sketch_element->SetAttribute("id", sketch);
	}

	WriteBaseXML(element);
}

// static member function
HeeksObj* CProfile::ReadFromXMLElement(TiXmlElement* element)
{
	CProfile* new_object = new CProfile;

	// read sketch ids
	for(TiXmlElement* pElem = TiXmlHandle(element).FirstChildElement().Element(); pElem; pElem = pElem->NextSiblingElement())
	{
		std::string name(pElem->Value());
		if(name == "params"){
			new_object->m_params.ReadFromXMLElement(pElem);
		}
		else if(name == "sketch"){
			for(TiXmlAttribute* a = pElem->FirstAttribute(); a; a = a->Next())
			{
				std::string name(a->Name());
				if(name == "id"){
					int id = a->IntValue();
					new_object->m_sketches.push_back(id);
				}
			}
		}
	}

	new_object->ReadBaseXML(element);

	return new_object;
}