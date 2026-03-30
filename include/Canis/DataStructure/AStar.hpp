#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

#include <Canis/Math.hpp>

#include "../Debug.hpp"

namespace Canis
{
    struct AStarNode {
        Vector3 position;
        float g;
        float h;
        float f = 0.0f;
        unsigned int predecessorID = 0;
        std::vector<unsigned int> adjacentPointIDs;
    };

    class AStar {
        private:
            std::vector<AStarNode> graph;
            std::unordered_map<Vector3, unsigned int> m_umap;

            std::vector<Vector3> BuildPath(AStarNode *node);
            
            bool VecHas(std::vector<unsigned int> &toCheck, unsigned int value);

        public:
            AStar();

            unsigned int AddPoint(Vector3 _position); // returns id
            unsigned int GetClosestPoint(Vector3 _position); // returns id
            unsigned int GetPointByPosition(Vector3 _position); // returns id

            void ConnectPoints(unsigned int idFrom, unsigned int idTo);
            void RemovePoint(unsigned int id);

            bool ArePointsConnected(unsigned int idFrom, unsigned int idTo);
            bool ValidPoint(Vector3 position);

            std::vector<Vector3> GetPath(unsigned int idFrom, unsigned int idTo);

        
    };
} // end of Canis namespace
