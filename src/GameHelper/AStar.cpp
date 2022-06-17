#include <Canis/GameHelper/AStar.hpp>

namespace Canis
{
    AStar::AStar()
    {
        AStarNode node = {};

        graph.push_back(node);
    }

    unsigned int AStar::AddPoint(glm::vec3 position)
    {
        AStarNode node = {};

        node.position = position;

        graph.push_back(node);

        return graph.size();
    }

    unsigned int AStar::GetClosestPoint(glm::vec3 position)
    {
        if (graph.size() == 0)
            FatalError("AStar::GetClosetPoint was called before a point was added to the graph.");
        
        unsigned int id = 0u;
        float minDistance = FLT_MAX; // set float to its largest value
        float distance = 0.0f;

        for(int i = 1; i < graph.size(); i++)
        {
            distance = glm::distance(position, graph[i].position);

            if (minDistance < distance)
            {
                id = i;
                minDistance = distance;
            }
        }

        return id;
    }

    unsigned int AStar::GetPointByPosition(glm::vec3 position)
    {
        for (unsigned int i = 1; i < graph.size();i++)
        {
            if (graph[i].position == position)
                return i;
        }


        return 0;
    }

    void AStar::ConnectPoints(unsigned int idFrom, unsigned int idTo)
    {
        if (graph.size() <= idFrom)
            FatalError("AStar::ConnectPoints idFrom has not been added to the graph.");

        if (graph.size() <= idTo)
            FatalError("AStar::ConnectPoints idTo has not been added to the graph.");
        
        if (idTo == idFrom)
            return;
        
        if (!VecHas(graph[idFrom].adjacentPointIDs, idTo))
            graph[idFrom].adjacentPointIDs.push_back(idTo);
        if (!VecHas(graph[idTo].adjacentPointIDs, idFrom))
            graph[idTo].adjacentPointIDs.push_back(idFrom);
    }

    bool AStar::ArePointsConnected(unsigned int idFrom, unsigned int idTo)
    {
        if (graph.size() <= idFrom)
            FatalError("AStar::ArePointsConnected idFrom has not been added to the graph.");

        if (graph.size() <= idTo)
            FatalError("AStar::ArePointsConnected idTo has not been added to the graph.");
        
        if (graph[idFrom].adjacentPointIDs.size() )

        for(int i = 0; i < graph[idFrom].adjacentPointIDs.size(); i++)
        {
            if (graph[idFrom].adjacentPointIDs[i] == idTo)
            {
                return true;
            }
        }

        return false;
    }

    bool AStar::ValidPoint(glm::vec3 position)
    {
        for (int i = 1; i < graph.size();i++)
        {
            if (graph[i].position == position)
                return true;
        }

        return false;
    }

    std::vector<glm::vec3> AStar::GetPath(unsigned int idFrom, unsigned int idTo)
    {
        if (graph.size() <= idFrom)
            FatalError("AStar::GetPath idFrom has not been added to the graph.");

        if (graph.size() <= idTo)
            FatalError("AStar::GetPath idTo has not been added to the graph.");
        
        std::vector<unsigned int> searchingSet;
        std::vector<unsigned int> hasSearchedSet;
        unsigned int graphNodeIndex;

        searchingSet.push_back(idFrom);

        while (searchingSet.size() > 0)
        {
            unsigned int lowestPath = 0;

            for (unsigned int i = 0; i < searchingSet.size(); i++)
            {
                if (graph[searchingSet[lowestPath]].f > graph[searchingSet[i]].f)
                {
                    lowestPath = i;
                }
            }

            

            AStarNode *node = &graph[searchingSet[lowestPath]];
            graphNodeIndex = searchingSet[lowestPath];

            if (idTo == searchingSet[lowestPath])
            {
                return BuildPath(node);
            }

            hasSearchedSet.push_back(searchingSet[lowestPath]);

            searchingSet.erase(searchingSet.begin() + lowestPath);

            std::vector<unsigned int> neighborIDs = node->adjacentPointIDs;

            for(unsigned int i = 0; i < neighborIDs.size(); i++)
            {
                AStarNode *neighborNode = &graph[neighborIDs[i]];                

                if (!VecHas(hasSearchedSet, neighborIDs[i]))
                {
                    if (graphNodeIndex == neighborIDs[i])
                    {
                        continue;
                    }

                    float currentG = node->g + std::abs(glm::distance(node->position, neighborNode->position));

                    bool isNewPath = false;

                    if (VecHas(searchingSet, neighborIDs[i]))
                    {
                        if (currentG < neighborNode->g)
                        {
                            isNewPath = true;
                            neighborNode->g = currentG;
                        }
                    }
                    else
                    {
                        isNewPath = true;
                        neighborNode->g = currentG;
                        searchingSet.push_back(neighborIDs[i]);
                    }

                    if (isNewPath)
                    {
                        neighborNode->h = std::abs(glm::distance(neighborNode->position, graph[idTo].position));
                        neighborNode->f = neighborNode->g + neighborNode->h;
                        neighborNode->predecessorID = graphNodeIndex;
                    }
                }
            }
        }

        return std::vector<glm::vec3> {};
    }

    std::vector<glm::vec3> AStar::BuildPath(AStarNode *node)
    {
        std::vector<glm::vec3> path;
        AStarNode *currentNode = node;

        while (currentNode->predecessorID != 0)
        {
            path.insert(path.begin(), currentNode->position);
            currentNode = &graph[currentNode->predecessorID];
        }

        return path;
    }

    bool AStar::VecHas(std::vector<unsigned int> &toCheck, unsigned int value)
    {
        for (unsigned int n = 0; n < toCheck.size(); n++)
        {
            if (toCheck[n] == value)
            {
                return true;
            }
        }

        return false;
    }
} // end of Canis namespace