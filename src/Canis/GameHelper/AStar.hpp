#pragma once
#include <string>
#include <vector>
#include <map>

#include <glm/glm.hpp>

#include "../Debug.hpp"

namespace Canis
{
    struct AStarNode {
        glm::vec3 position;
        float g;
        float h;
        float f = 0.0f;
        unsigned int predecessorID = 0;
        std::vector<unsigned int> adjacentPointIDs;
    };

    class AStar {
        private:
            std::vector<AStarNode> graph;

            std::vector<glm::vec3> BuildPath(AStarNode *node);
            
            bool VecHas(std::vector<unsigned int> &toCheck, unsigned int value);

        public:
            AStar();

            unsigned int AddPoint(glm::vec3 position); // returns id
            unsigned int GetClosestPoint(glm::vec3 position); // returns id
            unsigned int GetPointByPosition(glm::vec3 position); // returns id

            void ConnectPoints(unsigned int idFrom, unsigned int idTo);

            bool ArePointsConnected(unsigned int idFrom, unsigned int idTo);
            bool ValidPoint(glm::vec3 position);

            std::vector<glm::vec3> GetPath(unsigned int idFrom, unsigned int idTo);

        
    };
} // end of Canis namespace