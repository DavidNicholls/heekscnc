// gcode_parser.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "NCCode.h"

#include "gcode_parser.h"
#include "Program.h"

#define PROGRAM theApp.m_program

static std::vector<std::string> g_svLines;
CNCCode *g_pNcCode = NULL;
static wxString xml;

EmcVariables emc_variables;

typedef std::list<std::string> StringTokens_t;
StringTokens_t string_tokens;


#ifdef __cplusplus
	extern "C" {
#endif // __cplusplus
	extern FILE *yyin;
	void yyrestart( FILE *input_file );
	int  yyparse();
	int yylex(void);
	#include "y.tab.h"

	struct ParseState_t *pParseState = NULL;
#ifdef __cplusplus
	}
#endif // __cplusplus

    std::set<wxString> popup_warnings;


ParseState_t::ParseState_t()
{
    memset( this, 0, sizeof(*this) );
	current_coordinate_system = csG53;
	modal_coordinate_system = csG54;
	units = 1.0;	// Metric by default.
	ePlane_t plane = eXYPlane;
	tool_length_offset = 0.0;
}


std::string XmlData( const char *p_szData )
	{

		std::ostringstream	l_ossResult;

		while ((p_szData != NULL) && (*p_szData != '\0')) {
			switch (*p_szData) {
			case	'<':	l_ossResult << "&lt;";
							break;

			case	'>':	l_ossResult << "&gt;";
							break;

			case	'&':	l_ossResult << "&amp;";
							break;

			case	'\'':	l_ossResult << "&apos;";
							break;

			case	'"':	l_ossResult << "&quot;";
							break;

			default:		l_ossResult << *p_szData;
							break;
			} // End switch

			p_szData++;
		} // End while

		return(l_ossResult.str());

	} // End of XmlData() method.


wxString ColourForStatementType( const eStatement_t statement_type )
{
	/* Values come from the NCCode.cpp file where it associates configuration names
	   with colour values
	config.Read(_T("ColorDefaultType"),		&col, HeeksColor(0, 0, 0).COLORREF_color()); AddColor("default", HeeksColor((long)col));
	config.Read(_T("ColorBlockType"),		&col, HeeksColor(0, 0, 222).COLORREF_color()); AddColor("blocknum", HeeksColor((long)col));
	config.Read(_T("ColorMiscType"),		&col, HeeksColor(0, 200, 0).COLORREF_color()); AddColor("misc", HeeksColor((long)col));
	config.Read(_T("ColorProgramType"),		&col, HeeksColor(255, 128, 0).COLORREF_color()); AddColor("program", HeeksColor((long)col));
	config.Read(_T("ColorToolType"),		&col, HeeksColor(200, 200, 0).COLORREF_color()); AddColor("tool", HeeksColor((long)col));
	config.Read(_T("ColorCommentType"),		&col, HeeksColor(0, 200, 200).COLORREF_color()); AddColor("comment", HeeksColor((long)col));
	config.Read(_T("ColorVariableType"),	&col, HeeksColor(164, 88, 188).COLORREF_color()); AddColor("variable", HeeksColor((long)col));
	config.Read(_T("ColorPrepType"),		&col, HeeksColor(255, 0, 175).COLORREF_color()); AddColor("prep", HeeksColor((long)col));
	config.Read(_T("ColorAxisType"),		&col, HeeksColor(128, 0, 255).COLORREF_color()); AddColor("axis", HeeksColor((long)col));
	config.Read(_T("ColorRapidType"),		&col, HeeksColor(222, 0, 0).COLORREF_color()); AddColor("rapid", HeeksColor((long)col));
	config.Read(_T("ColorFeedType"),		&col, HeeksColor(0, 179, 0).COLORREF_color()); AddColor("feed", HeeksColor((long)col));
	*/

	switch (statement_type)
	{
	case stUndefined:
		return(_T("default"));

	case stPreparation:
		return(_T("prep"));

	case stAxis:
		return(_T("axis"));

	case stProgram:
		return(_T("program"));

	case stVariable:
		return(_T("variable"));

	case stToolLengthEnabled:
	case stToolLengthDisabled:
	case stToolChange:
		return(_T("tool"));

    case stG28:
    case stG30:
	case stRapid:
		return(_T("rapid"));

	case stProbe:
	case stFeed:
	case stArcClockwise:
	case stArcCounterClockwise:
	case stDrilling:
	case stTapping:
	case stBoring:
		return(_T("feed"));

	case stComment:
		return(_T("comment"));

	default:
		return(_T("default"));
	} // End switch
} // End ColourForStatementType() routine

double Emc2Units(const double value_in_parse_units)
{
    double value_in_mm = value_in_parse_units * pParseState->units;

    switch (PROGRAM->m_emc2_variables_units)
    {
        case CProgram::eImperial:
            return(value_in_mm / 25.4); // imperial - inches

        case CProgram::eMetric:
        default:
            return(value_in_mm);
    }
}

// Heeks uses millimeters
double HeeksUnits( const double value_in_emc2_units )
{
    switch (PROGRAM->m_emc2_variables_units)
    {
        case CProgram::eImperial:
            return(value_in_emc2_units * 25.4);


        case CProgram::eMetric:
        default:
            return(value_in_emc2_units);
    }
}

double ParseUnits( const double value_in_emc2_units )
{
    double value_in_mm = HeeksUnits( value_in_emc2_units );
    return( value_in_mm / pParseState->units );
}

double ParseUnitsFromHeeksUnits( const double value_in_heeks_units )
{
	double value_in_mm = value_in_heeks_units;	// always.
    return( value_in_mm / pParseState->units );
}


/**
	When the GCode switches from metric to imperial (or vice-versa) we need
	to change the values in out ParseState_t structure to the new standards
	so that we're comparing 'apples with apples' when interpreting
	subsequent coordinates.
 */
extern "C" void SwitchParseUnits( const int metric )
{
    if (metric)
    {
        if (pParseState->units == 25.4)
        {
            // Need to adjust the existing settings from imperial to metric.

            pParseState->x *= 25.4;
            pParseState->y *= 25.4;
            pParseState->z *= 25.4;

			/*
			Do not adjust a,b or c values as these are rotational units (degrees) rather than linear units
            pParseState->a *= 25.4;
            pParseState->b *= 25.4;
            pParseState->c *= 25.4;
			*/

            pParseState->u *= 25.4;
            pParseState->v *= 25.4;
            pParseState->w *= 25.4;

            pParseState->i *= 25.4;
            pParseState->j *= 25.4;
            pParseState->k *= 25.4;

            pParseState->l *= 25.4;
            pParseState->p *= 25.4;
            pParseState->q *= 25.4;
            pParseState->r *= 25.4;

            for (::size_t i=0; i<sizeof(pParseState->previous)/sizeof(pParseState->previous[0]); i++)
            {
                pParseState->previous[i] *= 25.4;
            }

            pParseState->feed_rate *= 25.4;
        }

        pParseState->units = 1.0;  // Switch to metric
    }
    else
    {
        if (pParseState->units == 1.0)
        {
            // Need to adjust the existing settings from metric to imperial.

            pParseState->x /= 25.4;
            pParseState->y /= 25.4;
            pParseState->z /= 25.4;

			/*
			Do not adjust a,b or c values as these are rotational units (degrees) rather than linear units
            pParseState->a /= 25.4;
            pParseState->b /= 25.4;
            pParseState->c /= 25.4;
			*/

            pParseState->u /= 25.4;
            pParseState->v /= 25.4;
            pParseState->w /= 25.4;

            pParseState->i /= 25.4;
            pParseState->j /= 25.4;
            pParseState->k /= 25.4;

            pParseState->l /= 25.4;
            pParseState->p /= 25.4;
            pParseState->q /= 25.4;
            pParseState->r /= 25.4;

            for (::size_t i=0; i<sizeof(pParseState->previous)/sizeof(pParseState->previous[0]); i++)
            {
                pParseState->previous[i] /= 25.4;
            }

            pParseState->feed_rate /= 25.4;
        }

        pParseState->units = 25.4;  // Switch to imperial
    }
}


extern "C" int AddSymbol( const char *name, const double value )
{
	if ((name == NULL) || (*name == '\0'))
	{
		int id = emc_variables.new_id();
		emc_variables[id] = value;
		return(id);
	}
	else
	{
		double unused = emc_variables[name];	// This either finds or creates an entry.
		return(emc_variables.hash(name));
	}
}


// Either find an existing symbol with this name and return it's ID or
// add a new symbol with this name.
extern "C" int Symbol( const char *name )
{
	if ((name == NULL) || (*name == '\0'))
	{
		int id = emc_variables.new_id();
		emc_variables[id] = 0.0;
		return(id);
	}
	else
	{
		double unused = emc_variables[name];	// This either finds or creates an entry.
		return(emc_variables.hash(name));
	}
}


extern "C" int LHSequivalenttoRHS(const int lhs, const int rhs)
{
	return(( emc_variables[lhs] == emc_variables[rhs] )?1:0);
}

extern "C" int LHSnotequaltoRHS(const int lhs, const int rhs)
{
	return(( emc_variables[lhs] != emc_variables[rhs] )?1:0);
}

extern "C" int LHSgreaterthanRHS(const int lhs, const int rhs)
{
	return(( emc_variables[lhs] > emc_variables[rhs] )?1:0);
}

extern "C" int LHSlessthanRHS(const int lhs, const int rhs)
{
	return(( emc_variables[lhs] < emc_variables[rhs] )?1:0);
}


extern "C" int LHSplusRHS(const int lhs, const int rhs)
{
	int id = emc_variables.new_id();
	emc_variables[id] = emc_variables[lhs] + emc_variables[rhs];
	return(id);
}

extern "C" int LHSminusRHS(const int lhs, const int rhs)
{
	int id = emc_variables.new_id();
	emc_variables[id] = emc_variables[lhs] - emc_variables[rhs];
	return(id);
}

extern "C" int LHStimesRHS(const int lhs, const int rhs)
{
	int id = emc_variables.new_id();
	emc_variables[id] = emc_variables[lhs] * emc_variables[rhs];
	return(id);
}

extern "C" int LHSdividedbyRHS(const int lhs, const int rhs)
{
	int id = emc_variables.new_id();
	if (emc_variables[rhs] == 0.0)
	{
		// We don't want to get into too much trouble just for backplotting.
		emc_variables[id] = DBL_MAX;
	}
	else
	{
		emc_variables[id] = emc_variables[lhs] / emc_variables[rhs];
	}

	return(id);
}


extern "C" int LHSassignmentfromRHS( const int lhs, const int rhs )
{
	emc_variables[lhs] = emc_variables[rhs];
	return(lhs);
}


extern "C" double Value(const int name)
{
	return( emc_variables[name] );
}


extern "C" const char * StringFromDouble(const double name)
{
	std::ostringstream ossName;
	ossName << name;

	string_tokens.push_back(ossName.str());
	return(string_tokens.back().c_str());
}


extern "C" const char *StringDuplication( const char *value )
{
	string_tokens.push_back(value);
	return(string_tokens.back().c_str());
}

double radians_to_degrees( const double radians )
{
	double a = radians;
	return( (radians / (2.0 * PI)) * 360.0 );
}

double degrees_to_radians( const double degrees )
{
	double a = degrees;
	return( (degrees / 360.0) * (2.0 * PI) );
}


extern "C" int ASin(const int symbol_id)
{
	int id=emc_variables.new_id();
	emc_variables[id] = radians_to_degrees( asin(degrees_to_radians(emc_variables[symbol_id] )) );
	return(id);
}

extern "C" int ACos(const int symbol_id)
{
	int id=emc_variables.new_id();
	emc_variables[id] = radians_to_degrees( acos(degrees_to_radians(emc_variables[symbol_id] )) );
	return(id);
}

extern "C" int ATan(const int symbol_id)
{
	int id=emc_variables.new_id();
	double degrees = emc_variables[symbol_id];
	double radians = atan(degrees_to_radians(degrees));
	emc_variables[id] = radians_to_degrees( radians );
	return(id);
}

extern "C" int Sin(const int symbol_id)
{
	int id=emc_variables.new_id();
	emc_variables[id] = radians_to_degrees( sin(degrees_to_radians(emc_variables[symbol_id] )) );
	return(id);
}

extern "C" int Cos(const int symbol_id)
{
	int id=emc_variables.new_id();
	emc_variables[id] = radians_to_degrees( cos(degrees_to_radians(emc_variables[symbol_id] )) );
	return(id);
}

extern "C" int Tan(const int symbol_id)
{
	int id=emc_variables.new_id();
	emc_variables[id] = radians_to_degrees( tan(degrees_to_radians(emc_variables[symbol_id] )) );
	return(id);
}

extern "C" int AbsoluteValue(const int symbol_id)
{
	int id=emc_variables.new_id();
	emc_variables[id] = abs(emc_variables[symbol_id]);
	return(id);
}

extern "C" int	Sqrt(const int symbol_id)
{
	int id=emc_variables.new_id();
	emc_variables[id] = sqrt(emc_variables[symbol_id]);
	return(id);
}




/**
    Adjust the value for the active coordinate system.  The parameter_offset is 0 for x, 1 for y etc.
    We want to arrange the GCode so that the G54 (the first relative coordinate system) lines up
    with the HeeksCAD drawing coordinate system.  All other coordinates are moved around that
    relative position.  We're using the fact that the gcode_variables map was initialized
    from the emc.var file to set the offsets of the various coordinate systems.

    The parameter_offset is added to the modal_coordinate_system base number (eg: 5221 for G54)
    to indicate which variable within that coordinate system we're referring to.  eg: 0 = x,
    1 = y etc.
 */
double adjust( const int parameter_offset, const double value_in_parse_units )
{
	double value_in_emc2_units = Emc2Units(value_in_parse_units);
	double tool_length_offset = 0.0;

	if (parameter_offset == 2)
	{
		// We're adjusting a Z coordinate so add the tool length offset
		tool_length_offset = pParseState->tool_length_offset;
	}

    if (parameter_offset >= 9)
    {
        printf("Parameter offset must be less than 9.  It is %d instead\n", parameter_offset);
        return( HeeksUnits(value_in_emc2_units) );
    }

    double g54_offset = emc_variables[eG54VariableBase + parameter_offset];
    double g92_offset = emc_variables[eG92VariableBase + parameter_offset];

    if (emc_variables[eG92Enabled] > 0.0)
    {
        // Copy the values from the gcode_variables into the local cache.
		return(HeeksUnits(value_in_emc2_units - g54_offset + g92_offset + tool_length_offset));
    }

    if (pParseState->current_coordinate_system == csG53)
    {
        // We need to move the point so that it's relavitve to the eG54 coordinate system.
		return(HeeksUnits(value_in_emc2_units - g54_offset + tool_length_offset));
	}

    int coordinate_system_offset = eG54VariableBase + ((pParseState->modal_coordinate_system - 1) * 20);
    int name = coordinate_system_offset + parameter_offset;

    return(HeeksUnits(value_in_emc2_units - g54_offset + emc_variables[name] + tool_length_offset));
}


/**
    We have received an 'end of block' character (a newline character).  Take all the settings we've found for
    this block and add the XML describing them. This includes both a verbatim copy of the original GCode line
    (taken from the g_svLines cache) and the various line/arc (etc.) elements that will allow Heeks to draw
    the path's meaning.
 */
extern "C" void AddToHeeks()
{

	if (pParseState->statement_type == stUndefined)
	{
		switch (pParseState->previous_statement_type)
		{
			// If it's one of the 'modal' commands then we don't actually NEED to see
			// that command again in the next block.  We could just be given the next
			// set of coordinates for the same command.  In this case, keep the
			// statement type as it is.  If another is seen, it will be overridden.
			case stProbe:
			case stFeed:
			case stArcClockwise:
			case stArcCounterClockwise:
			case stDrilling:
			case stBoring:
			case stTapping:
				pParseState->statement_type = pParseState->previous_statement_type;	// Reinstate the previous statement type.
			break;
		}
	} // End if - then


	if (::size_t(pParseState->line_offset) < g_svLines.size())
	{
		xml << _T("<ncblock>\n");

		// See if the line number is the first part of the line.  If so, colour it.
		if ((strlen(pParseState->line_number) > 0) && (g_svLines[pParseState->line_offset].find(pParseState->line_number) == 0))
		{
			xml << _T("<text col=\"blocknum\">") << Ctt(pParseState->line_number) << _T("</text>\n");
			xml << _T("<text><![CDATA[ ]]></text>\n");
			std::string value = XmlData(g_svLines[pParseState->line_offset].c_str());
			value.erase(0, strlen(pParseState->line_number) );
			xml << _T("<text col=\"") << ColourForStatementType(pParseState->statement_type) << _T("\">") << Ctt(value.c_str()) << _T("</text>\n");
		}
		else
		{
			xml << _T("<text col=\"") << ColourForStatementType(pParseState->statement_type) << _T("\">") << Ctt(XmlData(g_svLines[pParseState->line_offset].c_str()).c_str()) << _T("</text>\n");
		}

		switch (pParseState->statement_type)
		{
		case stUndefined:
			break;

		case stDataSetting:
			// The G10 statement has been specified.  Look at the L argument to see what needs to be set.
			if ((pParseState->l_specified) && (pParseState->p_specified))
			{
				if (pParseState->l == 20)
				{
					// We need to make the current coordinate be whatever the arguments say they are.  i.e.
					// adjust the appropriate coordinate system offset accordingly.

					int coordinate_system = pParseState->p;

					if (pParseState->x_specified)
					{
						int parameter_offset = 0;	// x
						int coordinate_system_offset = eG54VariableBase + ((pParseState->p - 1) * 20);
						int name = coordinate_system_offset + parameter_offset;
						double offset_in_heeks_units = HeeksUnits( emc_variables[name] );
						double offset_in_emc2_units = Emc2Units( pParseState->x ) - emc_variables[name];
						emc_variables[name] = emc_variables[name] + offset_in_emc2_units;
					}

					if (pParseState->y_specified)
					{
						int parameter_offset = 1;	// y
						int coordinate_system_offset = eG54VariableBase + ((pParseState->p - 1) * 20);
						int name = coordinate_system_offset + parameter_offset;
						double offset_in_heeks_units = HeeksUnits( emc_variables[name] );
						double offset_in_emc2_units = Emc2Units( pParseState->y ) - emc_variables[name];
						emc_variables[name] = emc_variables[name] + offset_in_emc2_units;
					}

					if (pParseState->z_specified)
					{
						int parameter_offset = 2;	// z
						int coordinate_system_offset = eG54VariableBase + ((pParseState->p - 1) * 20);
						int name = coordinate_system_offset + parameter_offset;
						double offset_in_heeks_units = HeeksUnits( emc_variables[name] );
						double offset_in_emc2_units = Emc2Units( pParseState->z ) - emc_variables[name];
						emc_variables[name] = emc_variables[name] + offset_in_emc2_units;
					}
				}
			}
			break;

		case stToolLengthEnabled:
			// The Z parameters given determine where we should think
			// we are right now.
			// pParseState->tool_length_offset = pParseState->k - ParseUnits(emc_variables[eG54VariableBase+2]);
			xml << _T("<path col=\"rapid\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line z=\"") << adjust(2,pParseState->z) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
			break;

		case stToolLengthDisabled:
			// The Z parameters given determine where we should think
			// we are right now.
			pParseState->tool_length_offset = 0.0;
			xml << _T("<path col=\"rapid\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line z=\"") << adjust(2,pParseState->previous[2]) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
			break;

		case stRapid:
            xml << _T("<path col=\"rapid\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line ");
			if (pParseState->x_specified) xml << _T("x=\"") << adjust(0,pParseState->x) << _T("\" ");
            if (pParseState->y_specified) xml << _T("y=\"") << adjust(1,pParseState->y) << _T("\" ");
            if (pParseState->z_specified) xml << _T("z=\"") << adjust(2,pParseState->z) << _T("\" ");
            xml << _T("/>\n")
                << _T("</path>\n");
			break;

		case stFeed:
            xml << _T("<path col=\"feed\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line ");
			if (pParseState->x_specified) xml << _T("x=\"") << adjust(0,pParseState->x) << _T("\" ");
            if (pParseState->y_specified) xml << _T("y=\"") << adjust(1,pParseState->y) << _T("\" ");
            if (pParseState->z_specified) xml << _T("z=\"") << adjust(2,pParseState->z) << _T("\" ");
            xml << _T("/>\n")
                << _T("</path>\n");
            if (pParseState->feed_rate <= 0.0) popup_warnings.insert(_("Zero feed rate found for feed movement"));
			break;

		case stProbe:
            xml << _T("<path col=\"feed\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line ");
			if (pParseState->x_specified) xml << _T("x=\"") << adjust(0,pParseState->x) << _T("\" ");
            if (pParseState->y_specified) xml << _T("y=\"") << adjust(1,pParseState->y) << _T("\" ");
            if (pParseState->z_specified) xml << _T("z=\"") << adjust(2,pParseState->z) << _T("\" ");
            xml << _T("/>\n")
                << _T("</path>\n");

			// Assume that the furthest point of probing tripped the switch.  Store this location
			// as though we found our probed object here.
			emc_variables[eG38_2VariableBase + 0] = ParseUnitsFromHeeksUnits(adjust(0,pParseState->x));
			emc_variables[eG38_2VariableBase + 1] = ParseUnitsFromHeeksUnits(adjust(1,pParseState->y));
			emc_variables[eG38_2VariableBase + 2] = ParseUnitsFromHeeksUnits(adjust(2,pParseState->z));

			emc_variables[eG38_2VariableBase + 3] = adjust(3,pParseState->a);
			emc_variables[eG38_2VariableBase + 4] = adjust(4,pParseState->b);
			emc_variables[eG38_2VariableBase + 5] = adjust(5,pParseState->c);

			emc_variables[eG38_2VariableBase + 6] = ParseUnitsFromHeeksUnits(adjust(6,pParseState->u));
			emc_variables[eG38_2VariableBase + 7] = ParseUnitsFromHeeksUnits(adjust(7,pParseState->v));
			emc_variables[eG38_2VariableBase + 8] = ParseUnitsFromHeeksUnits(adjust(8,pParseState->w));

			if (pParseState->feed_rate <= 0.0) popup_warnings.insert(_("Zero feed rate found for probe movement"));
			break;

		case stArcClockwise:
            xml << _T("<path col=\"feed\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<arc x=\"") << adjust(0,pParseState->x) << _T("\" ")
                << _T("y=\"") << adjust(1,pParseState->y) << _T("\" ")
				<< _T("z=\"") << adjust(1,pParseState->z) << _T("\" ")
                << _T("i=\"") << HeeksUnits(Emc2Units(pParseState->i)) << _T("\" ")
                << _T("j=\"") << HeeksUnits(Emc2Units(pParseState->j)) << _T("\" ")
				<< _T("k=\"") << HeeksUnits(Emc2Units(pParseState->k)) << _T("\" ")
                << _T("d=\"-1\" ")
                << _T("/>\n")
                << _T("</path>\n");
                if (pParseState->feed_rate <= 0.0) popup_warnings.insert(_("Zero feed rate found for arc movement"));
			break;

		case stArcCounterClockwise:
            xml << _T("<path col=\"feed\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<arc x=\"") << adjust(0,pParseState->x) << _T("\" ")
                << _T("y=\"") << adjust(1,pParseState->y) << _T("\" ")
				<< _T("z=\"") << adjust(1,pParseState->z) << _T("\" ")
                << _T("i=\"") << HeeksUnits(Emc2Units(pParseState->i)) << _T("\" ")
                << _T("j=\"") << HeeksUnits(Emc2Units(pParseState->j)) << _T("\" ")
				<< _T("k=\"") << HeeksUnits(Emc2Units(pParseState->k)) << _T("\" ")
                << _T("d=\"1\" ")
                << _T("/>\n")
                << _T("</path>\n");
                if (pParseState->feed_rate <= 0.0) popup_warnings.insert(_("Zero feed rate found for arc movement"));
			break;

		case stBoring:
		case stDrilling:
            xml << _T("<path col=\"rapid\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line x=\"") << adjust(0,pParseState->x) << _T("\" ")
                << _T("y=\"") << adjust(1,pParseState->y) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
            xml << _T("<path col=\"rapid\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line z=\"") << adjust(2,pParseState->r) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
            xml << _T("<path col=\"feed\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line z=\"") << adjust(2,pParseState->z) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
            xml << _T("<path col=\"rapid\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line z=\"") << adjust(2,pParseState->r) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
            pParseState->z = pParseState->r;	// We end up at the clearance (r) position.
            if (pParseState->feed_rate <= 0.01) popup_warnings.insert(_("Zero feed rate found for drilling movement"));
			break;

        case stTapping:
            xml << _T("<path col=\"rapid\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line x=\"") << adjust(0,pParseState->x) << _T("\" ")
                << _T("y=\"") << adjust(1,pParseState->y) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
            xml << _T("<path col=\"rapid\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line z=\"") << adjust(2,pParseState->r) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
            xml << _T("<path col=\"feed\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line z=\"") << adjust(2,pParseState->z) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
            xml << _T("<path col=\"feed\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line z=\"") << adjust(2,pParseState->r) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
            pParseState->z = pParseState->r;	// We end up at the clearance (r) position.
            if (pParseState->feed_rate <= 0.0) popup_warnings.insert(_("Zero feed rate found for tapping movement"));
			break;


		case stG28:
            // The saved position can be found in variables 5161 to 5169.
            pParseState->x = ParseUnits(emc_variables[ eG28VariableBase + 0 ] - emc_variables[ eG54VariableBase + 0 ]);
            pParseState->y = ParseUnits(emc_variables[ eG28VariableBase + 1 ] - emc_variables[ eG54VariableBase + 1 ]);
            pParseState->z = ParseUnits(emc_variables[ eG28VariableBase + 2 ] - emc_variables[ eG54VariableBase + 2 ]);
            pParseState->a = ParseUnits(emc_variables[ eG28VariableBase + 3 ] - emc_variables[ eG54VariableBase + 3 ]);
            pParseState->b = ParseUnits(emc_variables[ eG28VariableBase + 4 ] - emc_variables[ eG54VariableBase + 4 ]);
            pParseState->c = ParseUnits(emc_variables[ eG28VariableBase + 5 ] - emc_variables[ eG54VariableBase + 5 ]);
            pParseState->u = ParseUnits(emc_variables[ eG28VariableBase + 6 ] - emc_variables[ eG54VariableBase + 6 ]);
            pParseState->v = ParseUnits(emc_variables[ eG28VariableBase + 7 ] - emc_variables[ eG54VariableBase + 7 ]);
            pParseState->w = ParseUnits(emc_variables[ eG28VariableBase + 8 ] - emc_variables[ eG54VariableBase + 8 ]);

            xml << _T("<path col=\"rapid\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
                << _T("<line x=\"") << adjust(0,pParseState->x) << _T("\" ")
                << _T("y=\"") << adjust(1,pParseState->y) << _T("\" ")
                << _T("z=\"") << adjust(2,pParseState->z) << _T("\" ")
                << _T("/>\n")
                << _T("</path>\n");
		    break;

		case stG30:
			// The saved position can be found in variables 5181 to 5189.
			pParseState->x = ParseUnits(emc_variables[ eG30VariableBase + 0 ] - emc_variables[ eG54VariableBase + 0 ]);
			pParseState->y = ParseUnits(emc_variables[ eG30VariableBase + 1 ] - emc_variables[ eG54VariableBase + 1 ]);
			pParseState->z = ParseUnits(emc_variables[ eG30VariableBase + 2 ] - emc_variables[ eG54VariableBase + 2 ]);
			pParseState->a = ParseUnits(emc_variables[ eG30VariableBase + 3 ] - emc_variables[ eG54VariableBase + 3 ]);
			pParseState->b = ParseUnits(emc_variables[ eG30VariableBase + 4 ] - emc_variables[ eG54VariableBase + 4 ]);
			pParseState->c = ParseUnits(emc_variables[ eG30VariableBase + 5 ] - emc_variables[ eG54VariableBase + 5 ]);
			pParseState->u = ParseUnits(emc_variables[ eG30VariableBase + 6 ] - emc_variables[ eG54VariableBase + 6 ]);
			pParseState->v = ParseUnits(emc_variables[ eG30VariableBase + 7 ] - emc_variables[ eG54VariableBase + 7 ]);
			pParseState->w = ParseUnits(emc_variables[ eG30VariableBase + 8 ] - emc_variables[ eG54VariableBase + 8 ]);

			xml << _T("<path col=\"rapid\" fixture=\"") << int(pParseState->modal_coordinate_system) << _T("\">\n")
				<< _T("<line x=\"") << adjust(0,pParseState->x) << _T("\" ")
				<< _T("y=\"") << adjust(1,pParseState->y) << _T("\" ")
				<< _T("z=\"") << adjust(2,pParseState->z) << _T("\" ")
				<< _T("/>\n")
				<< _T("</path>\n");
            break;

        case stG92:
            // Make the axis values just read in become the current machine position by
            // figuring out what these values are (in machine coordinates) and adding
            // the offsets into a new set of coordinate system variables.  That's so
            // we can use the adjust() routine to allow for them in subsequent
            // commands.  This must occur until we find a G92.1 command which turns
            // this 'temporary coordinate system' functionality off.
            pParseState->current_coordinate_system = csG92;
			emc_variables[eG92Enabled] = 1.0;

			emc_variables[eG92VariableBase + 0] = Emc2Units(pParseState->previous[0]) - Emc2Units(pParseState->x);
			emc_variables[eG92VariableBase + 1] = Emc2Units(pParseState->previous[1]) - Emc2Units(pParseState->y);
			emc_variables[eG92VariableBase + 2] = Emc2Units(pParseState->previous[2]) - Emc2Units(pParseState->z);

			emc_variables[eG92VariableBase + 3] = Emc2Units(pParseState->previous[3]) - Emc2Units(pParseState->a);
			emc_variables[eG92VariableBase + 4] = Emc2Units(pParseState->previous[4]) - Emc2Units(pParseState->b);
			emc_variables[eG92VariableBase + 5] = Emc2Units(pParseState->previous[5]) - Emc2Units(pParseState->c);

			emc_variables[eG92VariableBase + 6] = Emc2Units(pParseState->previous[6]) - Emc2Units(pParseState->u);
			emc_variables[eG92VariableBase + 7] = Emc2Units(pParseState->previous[7]) - Emc2Units(pParseState->v);
			emc_variables[eG92VariableBase + 8] = Emc2Units(pParseState->previous[8]) - Emc2Units(pParseState->w);
            break;

        case stG92_1:
            // Turn off the 'temporary coordinate system' functionality.
            pParseState->current_coordinate_system = csUndefined;

            // Disable the G92 offset function.
			emc_variables[eG92Enabled] = 0.0;

            // Reset the G92 offsets to all zero.
            for (int i=eG92VariableBase; i<eG92VariableBase+9; i++)
            {
                emc_variables[i] = 0.0;
            }

            break;

        case stG92_2:
            // Disable the G92 offset function but don't reset the offsets in the memory locations.
			emc_variables[eG92Enabled] = 0.0;
            break;

        case stG92_3:
            // Re-Enable the G92 offset function and don't change the offsets in the memory locations.
			emc_variables[eG92Enabled] = 1.0;
            break;

		case stAxis:
			// Nothing extra special to do here.
			break;

		} // End switch

		xml << _T("</ncblock>\n");
	}

} // End AddToHeeks() routine


extern "C" void ResetForEndOfBlock()
{
	/*
	FILE *fp = fopen("c:\\temp\\david.log","a+t");
	fprintf(fp,"%s\n", pParseState->line_number);
	fprintf(fp,"%s\n", emc_variables.log().c_str());
	fclose(fp);
	*/

	pParseState->previous_statement_type = pParseState->statement_type;
	pParseState->statement_type = stUndefined;

	memset( pParseState->comment, '\0', sizeof(pParseState->comment) );
	memset( pParseState->line_number, '\0', sizeof(pParseState->line_number) );

	pParseState->previous[0] = pParseState->x;
	pParseState->previous[1] = pParseState->y;
	pParseState->previous[2] = pParseState->z;
	pParseState->previous[3] = pParseState->a;
	pParseState->previous[4] = pParseState->b;
	pParseState->previous[5] = pParseState->c;
	pParseState->previous[6] = pParseState->u;
	pParseState->previous[7] = pParseState->v;
	pParseState->previous[8] = pParseState->w;

	pParseState->l = 0.0;
	pParseState->p = 0.0;
	pParseState->r = 0.0;
	pParseState->q = 0.0;

	pParseState->i = 0.0;
	pParseState->j = 0.0;
	pParseState->k = 0.0;

	pParseState->x_specified = 0;
	pParseState->y_specified = 0;
	pParseState->z_specified = 0;

	pParseState->a_specified = 0;
	pParseState->b_specified = 0;
	pParseState->c_specified = 0;

	pParseState->u_specified = 0;
	pParseState->v_specified = 0;
	pParseState->w_specified = 0;

	pParseState->i_specified = 0;
	pParseState->j_specified = 0;
	pParseState->k_specified = 0;

	pParseState->l_specified = 0;
	pParseState->p_specified = 0;
	pParseState->q_specified = 0;
	pParseState->r_specified = 0;

} // End ResetForEndOfBlock() routine




/**
	Initialize all nine variables from this base number to the value given.
 */
void InitOneCoordinateSystem( const int base, const double value )
{
	for (int var=base; var<=base + 8; var++)
	{
		emc_variables[var] = value;
	}
}

void InitializeGCodeVariables()
{
	InitOneCoordinateSystem(eG38_2VariableBase, 0.0);
	InitOneCoordinateSystem(eG28VariableBase, 0.0);
	InitOneCoordinateSystem(eG30VariableBase, 0.0);
	InitOneCoordinateSystem(eG92VariableBase, 0.0);

	// Coordinate system number (1 = G54, 2=G55 etc.)
	{
		emc_variables[eCoordinateSystemInUse] = 1.0;
	}


	const wxChar *not_used = _("Not Used");
    if (PROGRAM->m_emc2_variables_file_name != not_used)
	{
		FILE *fp = fopen(PROGRAM->m_emc2_variables_file_name.utf8_str(),"r");
		if (fp == NULL)
		{
	        wxString error;
			error << _("Could not open ") << PROGRAM->m_emc2_variables_file_name << _(" for reading. Would you like to turn off the use of the EMC2 variables file?");
			int answer = wxMessageBox( error, _("EMC2 Variables File"), wxYES_NO );
			if (answer == wxYES)
			{
				PROGRAM->SetEmc2VariablesFileName( not_used );
			} // End if - then
		}
		else
		{
			char buf[1024];
			memset( buf, '\0', sizeof(buf) );
			while (fgets(buf, sizeof(buf)-1, fp) != NULL)
			{
				std::string line(buf);
				std::string::size_type offset=line.find_first_of("\t ");
				if (offset != std::string::npos)
				{
					std::string name = line.substr(0, offset);
					while (isspace(line[offset])) offset++;
					std::string value = line.substr(offset);

					emc_variables[name.c_str()] = atof(value.c_str());
				} // End if - then
			} // End while
			fclose(fp);
			fp = NULL;
		} // End if - then
	} // End if - then
}


extern "C" void StatementType(const int type)
{
	// Only override the statement type if this integer is larger than the
	// current value.  This allows the order of the statement type
	// enumeration to determine the precendence used for statement
	// types (for colour etc.)

	if (type > int(pParseState->statement_type)) pParseState->statement_type = eStatement_t(type);
}


wxString ParseGCodeFile(const wxString & filename)
{
	struct ParseState_t state;
	pParseState = (struct ParseState_t *) &state;

	// Initialize the gcode_variables so that we can use their values to interpret
	// the gcode (including coordinate system offsets)
	InitializeGCodeVariables();
	popup_warnings.clear();

	// Now set the machine's initial position to the origin of the G54 coordinate system.
    pParseState->x = ParseUnits(emc_variables[eG54VariableBase + 0]);
	pParseState->y = ParseUnits(emc_variables[eG54VariableBase + 1]);
	pParseState->z = ParseUnits(emc_variables[eG54VariableBase + 2]);

	pParseState->a = ParseUnits(emc_variables[eG54VariableBase + 3]);
	pParseState->b = ParseUnits(emc_variables[eG54VariableBase + 4]);
	pParseState->c = ParseUnits(emc_variables[eG54VariableBase + 5]);

	pParseState->u = ParseUnits(emc_variables[eG54VariableBase + 6]);
	pParseState->v = ParseUnits(emc_variables[eG54VariableBase + 7]);
	pParseState->w = ParseUnits(emc_variables[eG54VariableBase + 8]);

    ResetForEndOfBlock();   // Copy the x,y,z (etc.) values into the 'previous' array.

	FILE *fp = fopen(filename.utf8_str(),"r");
	if (! fp)
	{
		wxString error;
		error << _("Could not open ") << filename << _(" for reading");
		wxMessageBox(error);
		return(wxString(_T("")));
	}


	// Read the GCode file into a vector of strings so that we can publish the exact
	// text in the file at the same time that we're intepreting that text to produce
	// NCCode objects (for display purposes).
	g_svLines.clear();
	char buf[1024];
	memset( buf, '\0', sizeof(buf) );
	while (fgets(buf, sizeof(buf)-1, fp) != NULL)
	{
		while ((buf[strlen(buf)-1] == '\r') || (buf[strlen(buf)-1] == '\n')) buf[strlen(buf)-1] = '\0';
		g_svLines.push_back(buf);
	}

	// Rewind the file pointer to the beginning of the file ready to start parsing it.
	rewind(fp);

	// Tell the generated parsing code which file to take input from.
	yyrestart( fp );

	// Create a new object to hold the results.
	xml = _T("");
	xml << _T("<?xml version=\"1.0\" ?>\n<nccode>\n");

	// This is the actual parsing (i.e. generated source) routine.
	int l_iStatus = yyparse();

	if (l_iStatus == 0)
	{
		xml << _T("</nccode>\n");
	}
	else
	{
	    wxString error;
	    error << _("Failed to parse ") << filename << _(" nearby to (maybe immediately after) ") << Ctt(pParseState->line_number);
		wxMessageBox(error);
		xml = _T("");
	}

	fclose(fp);

	emc_variables.clear();
	string_tokens.clear();

	wxString warnings;
	for (std::set<wxString>::iterator itWarning = popup_warnings.begin(); itWarning != popup_warnings.end(); itWarning++)
	{
	    if (warnings.size() > 0) warnings << _T("\n");
	    warnings << *itWarning;
	}
	if (warnings.size() > 0)
	{
	    wxMessageBox(warnings);
	}

	return((l_iStatus == 0)?xml:wxString(_T("")));
}

