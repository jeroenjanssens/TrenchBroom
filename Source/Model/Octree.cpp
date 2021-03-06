/*
 Copyright (C) 2010-2012 Kristian Duske
 
 This file is part of TrenchBroom.
 
 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with TrenchBroom.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Octree.h"

#include "Model/Brush.h"
#include "Model/Entity.h"
#include "Model/Map.h"
#include "Model/MapObject.h"

#include <algorithm>
#include <cmath>
#include <cassert>


namespace TrenchBroom {
    namespace Model {
        bool OctreeNode::addObject(MapObject& object, unsigned int childIndex) {
            if (m_children[childIndex] == NULL) {
                BBoxf childBounds;
                switch (childIndex) {
                    case WSB:
                        childBounds.min[0] = m_bounds.min[0];
                        childBounds.min[1] = m_bounds.min[1];
                        childBounds.min[2] = m_bounds.min[2];
                        childBounds.max[0] = (m_bounds.min[0] + m_bounds.max[0]) / 2.0f;
                        childBounds.max[1] = (m_bounds.min[1] + m_bounds.max[1]) / 2.0f;
                        childBounds.max[2] = (m_bounds.min[2] + m_bounds.max[2]) / 2.0f;
                        break;
                    case WST:
                        childBounds.min[0] = m_bounds.min[0];
                        childBounds.min[1] = m_bounds.min[1];
                        childBounds.min[2] = (m_bounds.min[2] + m_bounds.max[2]) / 2.0f;
                        childBounds.max[0] = (m_bounds.min[0] + m_bounds.max[0]) / 2.0f;
                        childBounds.max[1] = (m_bounds.min[1] + m_bounds.max[1]) / 2.0f;
                        childBounds.max[2] = m_bounds.max[2];
                        break;
                    case WNB:
                        childBounds.min[0] = m_bounds.min[0];
                        childBounds.min[1] = (m_bounds.min[1] + m_bounds.max[1]) / 2.0f;
                        childBounds.min[2] = m_bounds.min[2];
                        childBounds.max[0] = (m_bounds.min[0] + m_bounds.max[0]) / 2.0f;
                        childBounds.max[1] = m_bounds.max[1];
                        childBounds.max[2] = (m_bounds.min[2] + m_bounds.max[2]) / 2.0f;
                        break;
                    case ESB:
                        childBounds.min[0] = (m_bounds.min[0] + m_bounds.max[0]) / 2.0f;
                        childBounds.min[1] = m_bounds.min[1];
                        childBounds.min[2] = m_bounds.min[2];
                        childBounds.max[0] = m_bounds.max[0];
                        childBounds.max[1] = (m_bounds.min[1] + m_bounds.max[1]) / 2.0f;
                        childBounds.max[2] = (m_bounds.min[2] + m_bounds.max[2]) / 2.0f;
                        break;
                    case EST:
                        childBounds.min[0] = (m_bounds.min[0] + m_bounds.max[0]) / 2.0f;
                        childBounds.min[1] = m_bounds.min[1];
                        childBounds.min[2] = (m_bounds.min[2] + m_bounds.max[2]) / 2.0f;
                        childBounds.max[0] = m_bounds.max[0];
                        childBounds.max[1] = (m_bounds.min[1] + m_bounds.max[1]) / 2.0f;
                        childBounds.max[2] = m_bounds.max[2];
                        break;
                    case ENB:
                        childBounds.min[0] = (m_bounds.min[0] + m_bounds.max[0]) / 2.0f;
                        childBounds.min[1] = (m_bounds.min[1] + m_bounds.max[1]) / 2.0f;
                        childBounds.min[2] = m_bounds.min[2];
                        childBounds.max[0] = m_bounds.max[0];
                        childBounds.max[1] = m_bounds.max[1];
                        childBounds.max[2] = (m_bounds.min[2] + m_bounds.max[2]) / 2.0f;
                        break;
                    case ENT:
                        childBounds.min[0] = (m_bounds.min[0] + m_bounds.max[0]) / 2.0f;
                        childBounds.min[1] = (m_bounds.min[1] + m_bounds.max[1]) / 2.0f;
                        childBounds.min[2] = (m_bounds.min[2] + m_bounds.max[2]) / 2.0f;
                        childBounds.max[0] = m_bounds.max[0];
                        childBounds.max[1] = m_bounds.max[1];
                        childBounds.max[2] = m_bounds.max[2];
                        break;
                }
                m_children[childIndex] = new OctreeNode(childBounds, m_minSize);
            }
            return m_children[childIndex]->addObject(object);
        }

        OctreeNode::OctreeNode(const BBoxf& bounds, unsigned int minSize) :
        m_minSize(minSize),
        m_bounds(bounds) {
            for (unsigned int i = 0; i < 8; i++)
                m_children[i] = NULL;
        }
        
        OctreeNode::~OctreeNode() {
            for (unsigned int i = 0; i < 8; i++)
                if (m_children != NULL) {
                    delete m_children[i];
                    m_children[i] = NULL;
                }
        }
        
        bool OctreeNode::addObject(MapObject& object) {
            if (!m_bounds.contains(object.bounds()))
                return false;
            if (m_bounds.max[0] - m_bounds.min[0] > m_minSize)
                for (unsigned int i = 0; i < 8; i++)
                    if (addObject(object, i))
                        return true;
            m_objects.push_back(&object);
            return true;
        }
        
        bool OctreeNode::removeObject(MapObject& object) {
            if (!m_bounds.contains(object.bounds()))
                return false;
            for (unsigned int i = 0; i < 8; i++) {
                if (m_children[i] != NULL && m_children[i]->removeObject(object)) {
                    if (m_children[i]->empty()) {
                        delete m_children[i];
                        m_children[i] = NULL;
                    }
                    return true;
                }
            }

            MapObjectList::iterator it = find(m_objects.begin(), m_objects.end(), &object);
            if (it == m_objects.end())
                return false;

            m_objects.erase(it);
            return true;
        }
        
        bool OctreeNode::empty() const {
            if (!m_objects.empty())
                return false;
            for (unsigned int i = 0; i < 8; i++)
                if (m_children[i] != NULL)
                    return false;
            return true;
        }

        size_t OctreeNode::count() const {
            size_t count = m_objects.size();
            for (size_t i = 0; i < 8; i++) {
                if (m_children[i] != NULL)
                    count += m_children[i]->count();
            }
            return count;
        }

        void OctreeNode::intersect(const Rayf& ray, MapObjectList& objects) {
            if (m_bounds.contains(ray.origin) || !Math<float>::isnan(m_bounds.intersectWithRay(ray))) {
                objects.insert(objects.end(), m_objects.begin(), m_objects.end());
                for (unsigned int i = 0; i < 8; i++)
                    if (m_children[i] != NULL)
                        m_children[i]->intersect(ray, objects);
            }
        }
        
        Octree::Octree(Map& map, unsigned int minSize) :
        m_minSize(minSize),
        m_map(map),
        m_root(new OctreeNode(map.worldBounds(), minSize)) {}
        
        Octree::~Octree() {
            delete m_root;
            m_root = NULL;
        }
        
        void Octree::loadMap() {
            const EntityList& entities = m_map.entities();
            for (unsigned int i = 0; i < entities.size(); i++) {
                Entity* entity = entities[i];
                m_root->addObject(*entity);
                const BrushList& brushes = entity->brushes();
                for (unsigned int j = 0; j < brushes.size(); j++) {
                    Brush* brush = brushes[j];
                    m_root->addObject(*brush);
                }
            }
        }
        
        void Octree::clear() {
            delete m_root;
            m_root = new OctreeNode(m_map.worldBounds(), m_minSize);
        }
        
        void Octree::addObject(MapObject& object) {
            bool result = m_root->addObject(object);
            assert(result);
        }

        void Octree::addObjects(const MapObjectList& objects) {
            bool result;
            for (unsigned int i = 0; i < objects.size(); i++) {
                MapObject* object = objects[i];
                result = m_root->addObject(*object);
                assert(result);
            }
        }
        
        void Octree::removeObject(MapObject& object) {
            bool result = m_root->removeObject(object);
            assert(result);
        }
        
        void Octree::removeObjects(const MapObjectList& objects) {
            bool result;
            for (unsigned int i = 0; i < objects.size(); i++) {
                MapObject* object = objects[i];
                result = m_root->removeObject(*object);
                assert(result);
            }
        }
        
        size_t Octree::count() const {
            return m_root->count();
        }

        MapObjectList Octree::intersect(const Rayf& ray) {
            MapObjectList result;
            m_root->intersect(ray, result);
            return result;
        }
    }
}
