#include "WorldModel.h"
#include "UserInput.h"
#include <string>
#include <sstream>

using std::vector;
using std::string;
using std::stringstream;

#define WORLDMESH_PATH "scenefiles/worldmesh.3ds"
#define ARMADILLO_PATH "scenefiles/armadillo.3ds"
#define SPHERE_PATH "scenefiles/sphere.3ds"

#define ARMADILLO_BASE_Y 3.3

void
WorldModel::Init(SceneGraph& sceneGraph)
{
    // Save parameters
    mSceneGraph = &sceneGraph;

    // Load the static parts of the scene into the scenegraph
    sceneGraph.LoadScene(WORLDMESH_PATH, "WorldMesh", &sceneGraph.rootNode);
    Matrix armTransform;
    armTransform.Translate(0.0, ARMADILLO_BASE_Y, 0.0);
    SceneNode* armParent = sceneGraph.AddNode(&sceneGraph.rootNode, armTransform,
                                              "armadilloParent");
    sceneGraph.LoadScene(ARMADILLO_PATH, "Armadillo", armParent);

    // Environment map
    Vector emapPos(0.0, 3.0 + ARMADILLO_BASE_Y, 0.0, 1.0);
    sceneGraph.FindMesh("Armadillo_0")->EnvironmentMap(emapPos);

    // Setup physics simulation
    broadphase = new btDbvtBroadphase();

    collisionConfiguration = new btDefaultCollisionConfiguration();
    dispatcher = new btCollisionDispatcher(collisionConfiguration);

    solver = new btSequentialImpulseConstraintSolver;

    dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher,broadphase,solver,collisionConfiguration);

    dynamicsWorld->setGravity(btVector3(0,-10,0));

    // Create the ground rigidBody
    //groundShape = new btStaticPlaneShape(btVector3(0,1,0),1);
    groundShape = new btCylinderShape(btVector3(15,3,15));

    btDefaultMotionState* groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1),btVector3(0,1,0)));

    btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(0,groundMotionState,groundShape,btVector3(0,0,0));
    groundRigidBodyCI.m_friction = 0.5;
    groundRigidBodyCI.m_restitution = 0.1;
    groundRigidBody = new btRigidBody(groundRigidBodyCI);
    dynamicsWorld->addRigidBody(groundRigidBody);
    
    // Create the platform
    platform = new Platform(200);
    
    // Enable the debug drawer
    debugDrawer.setDebugMode(btIDebugDraw::DBG_DrawWireframe);
    dynamicsWorld->setDebugDrawer(&debugDrawer);
}

WorldModel::~WorldModel()
{
    // Delete our player objects
    for (vector<Player*>::iterator it = mPlayers.begin();
         it != mPlayers.end(); ++it)
        delete *it;

    // Destroy physics simulation
    for(unsigned i = 0; i < mPlayers.size(); ++i) {
        Player *player = mPlayers[i];
        assert(player);
        btRigidBody *playerRigidBody = mPlayerRigidBodies[player];
        dynamicsWorld->removeRigidBody(playerRigidBody);
        delete playerRigidBody->getMotionState();
        delete playerRigidBody;
        delete mPlayerShapes[player];
    }

    dynamicsWorld->removeRigidBody(groundRigidBody);
    delete groundRigidBody->getMotionState();
    delete groundRigidBody;

    delete groundShape;

    delete dynamicsWorld;
    delete solver;
    delete collisionConfiguration;
    delete dispatcher;
    delete broadphase;
    
    // Delete the platform
    delete platform;
}

void
WorldModel::Step(sf::Clock& clck, GLint shaderID)
{
    // BOF step physics
    dynamicsWorld->stepSimulation(1/60.f, 10);

    // Loop over players
    for(unsigned i = 0; i < mPlayers.size(); ++i){
        Player *player = mPlayers[i];
        assert(player);
        HandleInputForPlayer(player->GetPlayerID());
        btTransform trans;
        mPlayerRigidBodies[player]->getMotionState()->getWorldTransform(trans);
        Vector playerPos(trans.getOrigin().getX(), trans.getOrigin().getY()-3.0, trans.getOrigin().getZ(), 1.0);
        //std::cout << "player y: " << trans.getOrigin().getY() << "\n";
        player->moveTo(playerPos);
    }

    //std::cout << "sphere x: " << trans.getOrigin().getX() << std::endl;
    // EOF step physics
    
    // BOF update platform
    
    // update platform position
    float time = clck.GetElapsedTime();
    if(time > 0.05) {
        clck.Reset();
        platform->update();
    }
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Disable the shader so we can draw the platform using the fixed pipeline
    GL_CHECK(glUseProgram(0));
    
    // Draw debug wireframes
    dynamicsWorld->debugDrawWorld();
    
    // Draw platform
    platform->render();
    
    // Flush
    GL_CHECK(glFlush());
    
    // Reenable the shader
    GL_CHECK(glUseProgram(shaderID));
    // EOF update platform
}

void
WorldModel::GetState(WorldState& stateOut)
{
    std::vector<PlayerInfo> playerInfoVec;
    for (size_t i=0; i<mPlayers.size(); i++) {
        PlayerInfo playerInfo;
        playerInfo.playerID = mPlayers[i]->GetPlayerID();
        playerInfo.pos =  mPlayers[i]->getPosition();
        playerInfoVec.push_back(playerInfo);
    }
    stateOut.playerVec = playerInfoVec;
}

void
WorldModel::SetState(WorldState& stateIn)
{
    std::vector<PlayerInfo> playerInfoVec = stateIn.playerVec;
    for (size_t i=0; i< playerInfoVec.size(); i++) {
        std::cout << "received: " << playerInfoVec[i].playerID << "\n";
        Player* player = GetPlayer(playerInfoVec[i].playerID);
        if (player == NULL) { // Add players to the client if they have not yet been added
            AddPlayer(playerInfoVec[i].playerID);
        }
        //Vector playerPos = playerInfoVec[i].pos;
        //player->moveTo(playerPos);
    }
}

static float sInitialPositions[][3] = { {-8.0, 5.0, 0.0},
                                        {-4.0, 5.0, 4.0} };

void
WorldModel::AddPlayer(unsigned playerID)
{
    // Generate the initial position.
    //
    // We only have enough initial positions for two players. This can be trivially
    // fixed.
    unsigned posIndex = mPlayers.size();
    assert(posIndex <= 1);
    Vector initialPosition(sInitialPositions[posIndex][0],
                           sInitialPositions[posIndex][1],
                           sInitialPositions[posIndex][2],
                           0.0f);

    // Call the internal helper
    AddPlayer(playerID, initialPosition);
}

void
WorldModel::AddPlayer(unsigned playerID, Vector initialPosition)
{
    // Make sure we don't already have a player by this ID
    assert(GetPlayer(playerID) == NULL);

    // Add the player to the scenegraph
    Matrix playerTransform;
    //playerTransform.Translate(initialPosition.x, initialPosition.y, initialPosition.z);
    stringstream numSS;
    numSS << playerID;
    string nodeName = string("PlayerNode_") + numSS.str();
    string rootName = string("PlayerRoot_") + numSS.str();
    SceneNode* playerNode = mSceneGraph->AddNode(&mSceneGraph->rootNode,
                                                 playerTransform,
                                                 nodeName.c_str());
    mSceneGraph->LoadScene(SPHERE_PATH, rootName.c_str(), playerNode);

    // Initialize the model representation of the player
    Player* player = new Player(playerID, playerNode, initialPosition);

    // Add it to our list of players
    mPlayers.push_back(player);

    // Create the player rigidBody
    btCollisionShape* playerShape = new btSphereShape(1);
    mPlayerShapes[player] = playerShape;
    btDefaultMotionState* playerMotionState =
    new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1),
    btVector3(initialPosition.x,initialPosition.y,initialPosition.z)));

    btScalar playerMass = 1;
    btVector3 playerInertia(0, 0, 0);
    playerShape->calculateLocalInertia(playerMass,playerInertia);

    btRigidBody::btRigidBodyConstructionInfo playerRigidBodyCI(playerMass, playerMotionState, playerShape, playerInertia);
    playerRigidBodyCI.m_friction = 0.5;
    playerRigidBodyCI.m_restitution = 0.1;
    playerRigidBodyCI.m_angularDamping = 0.5;
    btRigidBody *playerRigidBody = new btRigidBody(playerRigidBodyCI);
    mPlayerRigidBodies[player] = playerRigidBody;
    playerRigidBody->setActivationState(DISABLE_DEACTIVATION);
    dynamicsWorld->addRigidBody(playerRigidBody);
}

Player*
WorldModel::GetPlayer(unsigned playerID)
{
    // Search our list of players
    for (vector<Player*>::iterator it = mPlayers.begin();
         it != mPlayers.end(); ++it)
        if ((*it)->GetPlayerID() == playerID)
            return *it;

    // None found. Return null.
    return NULL;
}

void
WorldModel::ApplyInput(UserInput& input)
{
    // Get the player the input applies to
    Player* player = GetPlayer(input.playerID);
    assert(player);

    // Apply it
    player->applyInput(input);
}

void
WorldModel::HandleInputForPlayer(unsigned playerID)
{
    // Get the referenced player
    Player* player = GetPlayer(playerID);
    assert(player);
    btRigidBody* playerRigidBody = mPlayerRigidBodies[player];

    // Get the inputs
    uint32_t activeInputs = player->GetActiveInputs();

    if (activeInputs & GEN_INPUT_MASK(USERINPUT_INDEX_UP, true))
        playerRigidBody->applyForce(btVector3(1.0, 0.0, 0.0),
                                    btVector3(1.0, 1.0, 1.0));
    if (activeInputs & GEN_INPUT_MASK(USERINPUT_INDEX_DOWN, true))
        playerRigidBody->applyForce(btVector3(-1.0, 0.0, 0.0),
                                    btVector3(1.0, 1.0, 1.0));
    if (activeInputs & GEN_INPUT_MASK(USERINPUT_INDEX_LEFT, true))
        playerRigidBody->applyForce(btVector3(0.0, 0.0, -1.0),
                                    btVector3(1.0, 1.0, 1.0));
    if (activeInputs & GEN_INPUT_MASK(USERINPUT_INDEX_RIGHT, true))
        playerRigidBody->applyForce(btVector3(1.0, 0.0, 1.0),
                                    btVector3(1.0, 1.0, 1.0));
}

