
#include <scene.h>

/// global to keep persistence over instances of scene
Grid* Scene::grid_ = NULL;


Scene::Scene(const ros::NodeHandle& node)  
    : nh_(node)
{
    QRect area(0, 0, 820, 820); // grid test large
//  QRect area(0, 0, 820, 200); // grid test hard

    Scene::grid_ = new Grid(area.x(), area.y(), area.width(), area.height());
    tree = new Ped::Ttree(this, 0, area.x(), area.y(), area.width(), area.height());

       
    QObject::connect(&movetimer, SIGNAL(timeout()), this, SLOT(moveAllAgents()));
    movetimer.setInterval(500);

    QObject::connect(&cleanuptimer, SIGNAL(timeout()), this, SLOT(cleanupSlot()));
    cleanuptimer.setInterval(200);

    // start the time steps
    timestep = 0;

    /// setup the list of all agents and the robot agent
    all_agents_.clear();
    all_agents_ = getAllAgents();


    // setup services and publishers
    pub_all_agents_ = nh_.advertise<pedsim_msgs::AllAgentsState>("AllAgentsStatus", 1);
    srv_move_agent_ = nh_.advertiseService("SetAgentState", &Scene::srvMoveAgentHandler, this);

    // additional initialization in separat methods to keep constructor clean
    unpauseUpdates();
}


Scene::~Scene() { clear(); }
Grid* Scene::getGrid() { return grid_; }


bool Scene::srvMoveAgentHandler(pedsim_srvs::SetAgentState::Request& req, pedsim_srvs::SetAgentState::Response& res)
{
    pedsim_msgs::AgentState state = req.state;

    if (robot_->getid() == state.id)  {
        robot_->setPosition(state.position.x*20.0, state.position.y*20.0, state.position.z*20.0 );
        // robot_->setPosition(state.position.y*20.0, state.position.x*20.0, state.position.z*20.0 );
        robot_->setvx(state.velocity.x);
        robot_->setvy(state.velocity.y);

        moveAgent(robot_);
    }

    res.finished = true;

    return true;
}


bool Scene::isPaused() const
{
    return !movetimer.isActive();
}

void Scene::pauseUpdates()
{
    movetimer.stop();
    cleanuptimer.stop();
}

void Scene::unpauseUpdates()
{
    movetimer.start();
    cleanuptimer.start();
}


void Scene::cleanupSlot() {
    cleanup();
}

void Scene::clear() {
    foreach(Agent* agent, agents)
        delete agent;
    agents.clear();
    all_agents_.clear();

    foreach(Waypoint* waypoint, waypoints)
        delete waypoint;
    waypoints.clear();

    foreach(Obstacle* obs, obstacles)
        delete obs;
    obstacles.clear();
}


void Scene::moveAllAgents() {
    if(!movetimer.isActive()) return;

    all_agents_ = getAllAgents();

    for (vector<Ped::Tagent*>::const_iterator iter = all_agents_.begin(); iter != all_agents_.end(); ++iter) 
    {
        Ped::Tagent *a = (*iter);

        if (a->gettype() == 2) 
            robot_ = a;
    }

    publicAgentStatus();

    /// NOTE Test Termination criterion for the robot
    if (eDist(robot_->getx(), robot_->gety(), 100, 720) < 1.0) exit(0);

    timestep++;
    ros::spinOnce();

    // move the agents by social force
    Ped::Tscene::moveAgents(CONFIG.simh);
}



void Scene::publicAgentStatus()
{
    pedsim_msgs::AllAgentsState all_status;
    all_status.header.stamp = ros::Time::now();

    for (vector<Ped::Tagent*>::const_iterator iter = all_agents_.begin(); iter != all_agents_.end(); ++iter) {
        Ped::Tagent *a = (*iter);

        pedsim_msgs::AgentState state;

        state.header.stamp = ros::Time::now();
        state.id = a->getid();
        state.position.x = a->getx() * (1/20.0);
        state.position.y = a->gety() * (1/20.0);
        state.position.z = a->getz() * (1/20.0);

        state.velocity.x = a->getvx();
        state.velocity.y = a->getvy();
        state.velocity.z = a->getvz();

        all_status.agent_states.push_back(state);

    }

    pub_all_agents_.publish(all_status);
}



std::set<const Ped::Tagent*> Scene::getNeighbors(double x, double y, double maxDist)
{
    std::set<const Ped::Tagent*> potentialNeighbours = Ped::Tscene::getNeighbors(x, y, maxDist);

    // filter according to euclidean distance
    auto agentIter = potentialNeighbours.begin();
    while(agentIter != potentialNeighbours.end())
    {
        double aX = (*agentIter)->getx();
        double aY = (*agentIter)->gety();
        double distance = sqrt((x-aX)*(x-aX) + (y-aY)*(y-aY));

        // remove distant neighbors
        if(distance > maxDist) {
            potentialNeighbours.erase(agentIter++);
        } else {
            ++agentIter;
        }
    }

    return potentialNeighbours;
}


bool Scene::readFromFile(const QString& filename) {
    // ROS_INFO("Loading scenario file '%s'.", filename.toStdString);

    // open file
    QFile file(filename);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // ERROR_LOG("Couldn't open scenario file!");
        return false;
    }

    // read input
    while(!file.atEnd()) {
        QByteArray line = file.readLine();
        processData(line);
    }

    // report success
    return true;
}


/// Called for each line in the file
/// \date    2012-02-03
void Scene::processData(QByteArray& data) {
    xmlReader.addData(data);
    CONFIG.obstacle_positions.clear();

    while(!xmlReader.atEnd()) {
        xmlReader.readNext();
        if(xmlReader.isStartElement()) {
            if((xmlReader.name() == "scenario")
                    || (xmlReader.name() == "welcome")) {
                // nothing to do
            }
            else if(xmlReader.name() == "obstacle") {
                double x1 = xmlReader.attributes().value("x1").toString().toDouble();
                double y1 = xmlReader.attributes().value("y1").toString().toDouble();
                double x2 = xmlReader.attributes().value("x2").toString().toDouble();
                double y2 = xmlReader.attributes().value("y2").toString().toDouble();
                Obstacle* obs = new Obstacle(this, x1, y1, x2, y2);
                this->addObstacle(obs);
                // drawObstacles(x1, y1, x2, y2);
            }
            else if(xmlReader.name() == "waypoint") {
                // TODO - add an explicit waypoint type
                QString id = xmlReader.attributes().value("id").toString();
                double x = xmlReader.attributes().value("x").toString().toDouble();
                double y = xmlReader.attributes().value("y").toString().toDouble();
                double r = xmlReader.attributes().value("r").toString().toDouble();

                Waypoint* w = new Waypoint(this, id, x, y, r);

                // if (boost::starts_with(id, "start")) {
                //     w->setType(Ped::Twaypoint::TYPE_BIRTH);
                //     std::cout << "adding a birth waypoint" << std::endl;
                // }

                // if (boost::starts_with(id, "stop")) {
                //     w->setType(Ped::Twaypoint::TYPE_DEATH);
                //     std::cout << "adding a death waypoint" << std::endl;
                // }


                this->waypoints[id] = w;
            }
            else if(xmlReader.name() == "agent") {
                double x = xmlReader.attributes().value("x").toString().toDouble();
                double y = xmlReader.attributes().value("y").toString().toDouble();
                int n = xmlReader.attributes().value("n").toString().toDouble();
                double dx = xmlReader.attributes().value("dx").toString().toDouble();
                double dy = xmlReader.attributes().value("dy").toString().toDouble();
                double type = xmlReader.attributes().value("type").toString().toInt();
                //TODO: keep agent group and expand later!?
                for (int i=0; i<n; i++) {
                    Agent* a = new Agent(this);
                    double randomizedX = x;
                    double randomizedY = y;
                    // handle dx=0 or dy=0 cases
                    //TODO: qrand() actually needs to be seeded to create different runs
                    if(dx != 0)
                        randomizedX += qrand()/(double)RAND_MAX * dx - dx/2;
                    if(dy != 0)
                        randomizedY += qrand()/(double)RAND_MAX * dy - dy/2;
                    a->setPosition(randomizedX, randomizedY);
                    a->setType(type);
                    this->addAgent(a);
                    currentAgents.append(a);
                }
            }
            else if(xmlReader.name() == "addwaypoint") {
                QString id = xmlReader.attributes().value("id").toString();
                // add waypoints to current agents, not just those of the current <agent> element
                foreach(Agent* a, currentAgents)
                    a->addWaypoint(this->waypoints[id]);
            }
            else {
                // inform the user about invalid elements
                // ROS_WARN("Unknown element: %s", xmlReader.name().toString());
            }
        }
        else if(xmlReader.isEndElement()) {
            if (xmlReader.name() == "agent") {
                currentAgents.clear();
            }
        }
    }

}



int main(int argc, char** argv) {
    // initialize resources
    ros::init(argc, argv, "simulator");

    ros::NodeHandle node;

    QSharedPointer<Scene> scene = QSharedPointer<Scene>(new Scene(node));


    ScenarioReader scenarioReader(scene);

    return 0;    
}