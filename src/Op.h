// Op.h
/*
 * Copyright (c) 2009, Dan Heeks
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#ifndef OP_HEADER
#define OP_HEADER

#include "interface/ObjList.h"
#include "PythonStuff.h"
#include <set>

#ifndef OP_SKETCHES_AS_CHILDREN
	#define OP_SKETCHES_AS_CHILDREN
#endif // OP_SKETCHES_AS_CHILDREN

class CFixture;	// Forward declaration.
class CMachineState;

class COp : public ObjList
{
public:
	typedef int ToolNumber_t;

	wxString m_comment;
	bool m_active; // don't make NC code, if this is not active
	wxString m_title;
	int m_execution_order;	// Order by which the GCode sequences are generated.
	ToolNumber_t m_tool_number;	// joins the m_tool_number in one of the CTool objects in the tools list.
	int m_operation_type; // Type of operation (because GetType() overloading does not allow this class to call the parent's method)

	COp(const wxString& title, const int tool_number = 0, const int operation_type = UnknownType )
            :m_active(true), m_title(title), m_execution_order(0), m_tool_number(tool_number),
            m_operation_type(operation_type)
    {
        ReadDefaultValues();
    }

	COp & operator= ( const COp & rhs );
	COp( const COp & rhs );

	// HeeksObj's virtual functions
	void GetProperties(std::list<Property *> *list);
	void WriteBaseXML(TiXmlElement *element);
	void ReadBaseXML(TiXmlElement* element);
	const wxBitmap& GetInactiveIcon();
	virtual std::list<wxBitmap> GetIcons();
	virtual const wxChar* GetShortString(void)const{return m_title.c_str();}
	bool CanEditString(void)const{return true;}
	void OnEditString(const wxChar* str);
	void GetTools(std::list<Tool*>* t_list, const wxPoint* p);
	void glCommands(bool select, bool marked, bool no_color);

	virtual void WriteDefaultValues();
	virtual void ReadDefaultValues();
	virtual Python AppendTextToProgram( CMachineState *pMachineState );
	virtual std::list<CFixture> PrivateFixtures();
	virtual unsigned int MaxNumberOfPrivateFixtures() const { return(1); }
	virtual bool UsesTool(){return true;} // some operations don't use the tool number
	virtual std::set<ToolNumber_t> GetMachineTools() const;
	virtual void OnSetTool(const ToolNumber_t new_tool_number) { }

	void ReloadPointers() { ObjList::ReloadPointers(); }

	// The DesignRulesAdjustment() method is the opportunity for all Operations objects to
	// adjust their parameters to values that 'make sense'.  eg: If a drilling cycle has a
	// profile operation as a reference then it should not have a depth value that is deeper
	// than the profile operation.
	// The list of strings provides a description of what was changed.
	virtual std::list<wxString> DesignRulesAdjustment(const bool apply_changes);
	virtual wxString DesignRulesPreamble() const;

	bool operator==(const COp & rhs) const;
	bool operator!=(const COp & rhs) const { return(! (*this == rhs)); }
	bool IsDifferent(HeeksObj *other) { return( *this != (*((COp *)other)) ); }
};

#endif

