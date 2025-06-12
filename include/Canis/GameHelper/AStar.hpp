#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

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
            std::unordered_map<glm::vec3, unsigned int> m_umap;

            std::vector<glm::vec3> BuildPath(AStarNode *node);
            
            bool VecHas(std::vector<unsigned int> &toCheck, unsigned int value);

        public:
            AStar();

            unsigned int AddPoint(glm::vec3 _position); // returns id
            unsigned int GetClosestPoint(glm::vec3 _position); // returns id
            unsigned int GetPointByPosition(glm::vec3 _position); // returns id

            void ConnectPoints(unsigned int idFrom, unsigned int idTo);
            void RemovePoint(unsigned int id);

            bool ArePointsConnected(unsigned int idFrom, unsigned int idTo);
            bool ValidPoint(glm::vec3 position);

            std::vector<glm::vec3> GetPath(unsigned int idFrom, unsigned int idTo);

        
    };
} // end of Canis namespace
