/*
 Copyright (C) 2010-2014 Kristian Duske
 
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
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TrenchBroom__Map__
#define __TrenchBroom__Map__

#include "SharedPointer.h"
#include "Model/Entity.h"
#include "Model/EntityPropertyIndex.h"
#include "Model/ModelFactory.h"
#include "Model/ModelTypes.h"

#include <algorithm>

namespace TrenchBroom {
    namespace Model {
        class Map {
        private:
            ModelFactory m_factory;
            LayerList m_layers;
            EntityList m_entities;
            EntityPropertyIndex m_entityPropertyIndex;
            mutable Entity* m_worldspawn;
        public:
            Map(const ModelFactory& factory);
            ~Map();
            
            MapFormat::Type format() const;

            Layer* createLayer(const String& name) const;
            void addLayer(Layer* layer);
            bool canRemoveLayer(const Layer* layer) const;
            void removeLayer(Layer* layer);

            Layer* defaultLayer() const;
            const LayerList& layers() const;
            
            void resolveLayers();
        private:
            bool isLayerEntity(const Entity* entity) const;
            void addBrushesToLayer(const BrushList& brushes, Layer* layer);
        public:

            Entity* createEntity() const;
            Brush* createBrush(const BBox3& worldBounds, const BrushFaceList& faces) const;
            BrushFace* createFace(const Vec3& point0, const Vec3& point1, const Vec3& point2, const String& textureName) const;
            
            const EntityList& entities() const;
            void addEntity(Entity* entity);
            void removeEntity(Entity* entity);
            Entity* worldspawn() const;
            Object* findObjectByFilePosition(const size_t position) const;
            
            void addEntityPropertiesToIndex(Entity* entity);
            void removeEntityPropertiesFromIndex(Entity* entity);
            void addEntityPropertyToIndex(Entity* entity, const PropertyKey& key, const PropertyValue& value);
            void removeEntityPropertyFromIndex(Entity* entity, const PropertyKey& key, const PropertyValue& value);
            
            EntityList findEntitiesWithProperty(const PropertyKey& key, const PropertyValue& value) const;
            EntityList findEntitiesWithNumberedProperty(const PropertyKey& prefix, const PropertyValue& value) const;
            
            const BrushList brushes() const;
        private:
            Entity* findWorldspawn() const;
        };
    }
}

#endif /* defined(__TrenchBroom__Map__) */