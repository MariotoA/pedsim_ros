// Simulating Group Dynamics in Crowds of Pedestrians (SGDiCoP)
// Author: Sven Wehner <mail@svenwehner.de>
//
// Forked from: PedSim's demo application (version 2.2).
// (http://pedsim.silmaril.org/)
// Copyright text:
// 		pedsim - A microscopic pedestrian simulation system.
// 		Copyright (c) 2003 - 2012 by Christian Gloor


#ifndef _agent_h_
#define _agent_h_

// → PedSim
#include "ped_agent.h"
#include "ped_vector.h"
// → Qt
#include <QWeakPointer>

// Forward Declarations
class Scene;
class Waypoint;


class Agent : public Ped::Tagent 
{
    // Constructor and Destructor
public:
    Agent(const QWeakPointer<Scene>& sceneIn, double xIn = 0, double yIn = 0);
    virtual ~Agent();


    // Methods
public:
    void move(double h);
    Ped::Tvector socialForce() const;
    Ped::Tvector obstacleForce() const;
    Ped::Tvector desiredForce();
    Ped::Tvector lookaheadForce(Ped::Tvector desired) const;
    Ped::Tvector myForce(Ped::Tvector desired) const;

protected:
    void updateView();

    // → Ped::Tagent Overrides/Overloads
public:
    void addWaypoint(Waypoint* waypointIn);
    void setPosition(double px, double py);
    void setX(double xIn);
    void setY(double yIn);
    void setType(int t);

    // → ScenarioElement Overrides/Overloads
public:
    void updateLookOnSelection(bool selectedIn);

    // → QGraphicsItem Overrides
    virtual int type() const { Ped::Tagent::gettype(); }


    // Attributes
public:
    // → reference to the scene
    QWeakPointer<Scene> scene;


    // → waypoints
    //HACK: we need to save them here again, because Ped::Tagent doesn't give
    //      access to them in any way
public:
    QList<Waypoint*> waypoints;
};

#endif
