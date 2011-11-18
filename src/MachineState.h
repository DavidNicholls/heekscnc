// MachineState.h
/*
 * Copyright (c) 2009, Dan Heeks
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#pragma once

#include "Fixture.h"
#include "PythonStuff.h"
#include "CNCPoint.h"

#include <map>

class Python;
class CFixture;
class CNCPoint;
class CAttachOp;
class CMachine;

/**
    The CMachineState class stores information about the machine for use
    in the GCode generation routines.  An object of this class will be passed
    both into and back out of each gcode generation routine so that
    subsequent routines can know whether tool changes or fixture changes
    are either necessary or have occured.

    Location values returned also allow subsequent gcode generation routines
    to sort their objects so that starting points can be selected based on
    previous operation's ending points.

	This class also keeps track of which objects have had their gcode generated
	for which fixtures.  We need to know this so that we don't double-up while
	we are handling the various private and public fixture settings.
 */
class CMachineState
{
private:
	/**
		This class remembers an individual machine operation along with
		the fixture used for gcode generation.  It's really just a placeholder
		for a tuple of two values so we can keep track of what we've already
		processed.
	 */
   class Instance
    {
    public:
        Instance() : m_fixture(NULL, CFixture::G54, false, 0.0) { }
        ~Instance() { }
        Instance & operator= ( const Instance & rhs );
        Instance( const Instance & rhs );
        bool operator==( const Instance & rhs ) const;
        bool operator< ( const Instance & rhs ) const;

        void Object(const HeeksObj *object) { m_object = object; }
        void Fixture( const CFixture fixture ) { m_fixture = fixture; }

    private:
        const HeeksObj	*m_object;
        CFixture    m_fixture;
    }; // End Instance class definition

public:
	CAttachOp* m_attached_to_surface;

	CMachineState(CMachine *pMachine, CFixture fixture);
    ~CMachineState();

    CMachineState(CMachineState & rhs);
    CMachineState & operator= ( CMachineState & rhs );

    int Tool() const { return(m_tool_number); }
    Python Tool( const int new_tool );

    CFixture Fixture() const { return(m_fixture); }
    Python Fixture( CFixture fixture );

    CNCPoint Location() const { return(m_location); }
    void Location( const CNCPoint rhs );
	bool NearestLocation(CFixture fixture, CNCPoint location, CNCPoint *pPreviousLocation) const;

	bool LocationIsKnown() const { return(m_location_is_known); }

    bool operator== ( const CMachineState & rhs ) const;
    bool operator!= ( const CMachineState & rhs ) const { return(! (*this == rhs)); }

	bool AlreadyProcessed( const HeeksObj *object, const CFixture fixture );
	void MarkAsProcessed( const HeeksObj *object, const CFixture fixture );
	Python ToolChangeMovement_Preamble(std::set<CFixture> & fixtures);

private:
    int         m_tool_number;
    CFixture    m_fixture;
    CNCPoint      m_location;
	bool		m_location_is_known;	// false when fixture changes occur.
    bool        m_fixture_has_been_set;
	CMachine	*m_pMachine;

	std::set<Instance> m_already_processed;

	// Keep a list of visited points so we can avoid 
	// unnesseary ramping when we could feed down to a previously visited location.
	std::multimap<CFixture, CNCPoint> m_previous_locations;

}; // End CMachineState class definition
