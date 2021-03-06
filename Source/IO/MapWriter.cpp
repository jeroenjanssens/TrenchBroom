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

#include "MapWriter.h"

#include "Model/Texture.h"
#include "Model/Brush.h"
#include "Model/BrushGeometry.h"
#include "Model/Entity.h"
#include "Model/EntityDefinition.h"
#include "Model/Face.h"
#include "Model/Map.h"
#include "IO/FileManager.h"
#include "IO/IOException.h"

#include <cassert>
#include <fstream>
#include <limits>

namespace TrenchBroom {
    namespace IO {
        size_t MapWriter::writeFace(Model::Face& face, const size_t lineNumber, FILE* stream) {
            const String textureName = Utility::isBlank(face.textureName()) ? Model::Texture::Empty : face.textureName();

            std::fprintf(stream, FaceFormat.c_str(),
                    face.point(0).x(),
                    face.point(0).y(),
                    face.point(0).z(),
                    face.point(1).x(),
                    face.point(1).y(),
                    face.point(1).z(),
                    face.point(2).x(),
                    face.point(2).y(),
                    face.point(2).z(),
                    textureName.c_str(),
                    face.xOffset(),
                    face.yOffset(),
                    face.rotation(),
                    face.xScale(),
                    face.yScale());
            face.setFilePosition(lineNumber);
            return 1;
        }
        
        size_t MapWriter::writeBrush(Model::Brush& brush, const size_t lineNumber, FILE* stream) {
            size_t lineCount = 0;
            std::fprintf(stream, "{\n"); lineCount++;
            const Model::FaceList& faces = brush.faces();
            Model::FaceList::const_iterator faceIt, faceEnd;
            for (faceIt = faces.begin(), faceEnd = faces.end(); faceIt != faceEnd; ++faceIt) {
                lineCount += writeFace(**faceIt, lineNumber + lineCount, stream);
            }
            std::fprintf(stream, "}\n"); lineCount++;
            brush.setFilePosition(lineNumber, lineCount);
            return lineCount;
        }
        
        size_t MapWriter::writeEntityHeader(Model::Entity& entity, FILE* stream) {
            size_t lineCount = 0;
            std::fprintf(stream, "{\n"); lineCount++;
            
            const Model::PropertyList& properties = entity.properties();
            Model::PropertyList::const_iterator it, end;
            for (it = properties.begin(), end = properties.end(); it != end; ++it) {
                const Model::Property& property = *it;
                std::fprintf(stream, "\"%s\" \"%s\"\n", property.key().c_str(), property.value().c_str()); lineCount++;
            }
            return lineCount;
        }
        
        size_t MapWriter::writeEntityFooter(FILE* stream) {
            std::fprintf(stream, "}\n");
            return 1;
        }
        
        size_t MapWriter::writeEntity(Model::Entity& entity, const size_t lineNumber, FILE* stream) {
            size_t lineCount = writeEntityHeader(entity, stream);
            const Model::BrushList& brushes = entity.brushes();
            for (unsigned int i = 0; i < brushes.size(); i++)
                lineCount += writeBrush(*brushes[i], lineNumber + lineCount, stream);
            lineCount += writeEntityFooter(stream);
            entity.setFilePosition(lineNumber, lineCount);
            return lineCount;
        }

        void MapWriter::writeFace(const Model::Face& face, std::ostream& stream) {
            const String textureName = Utility::isBlank(face.textureName()) ? Model::Texture::Empty : face.textureName();
            
            stream.precision(FloatPrecision);
            stream <<
            "( " <<
            face.point(0).x() << " " <<
            face.point(0).y() << " " <<
            face.point(0).z() <<
            " ) ( "           <<
            face.point(1).x() << " " <<
            face.point(1).y() << " " <<
            face.point(1).z() <<
            " ) ( "           <<
            face.point(2).x() << " " <<
            face.point(2).y() << " " <<
            face.point(2).z() <<
            " ) ";

            stream.precision(6);
            stream <<
            textureName     << " " <<
            face.xOffset()  << " " <<
            face.yOffset()  << " " <<
            face.rotation() << " " <<
            face.xScale()   << " " <<
            face.yScale()   << "\n";
        }

        void MapWriter::writeBrush(const Model::Brush& brush, std::ostream& stream) {
            stream << "{\n";
            const Model::FaceList& faces = brush.faces();
            Model::FaceList::const_iterator faceIt, faceEnd;
            for (faceIt = faces.begin(), faceEnd = faces.end(); faceIt != faceEnd; ++faceIt)
                writeFace(**faceIt, stream);
            stream << "}\n";
        }

        void MapWriter::writeEntityHeader(const Model::Entity& entity, std::ostream& stream) {
            stream << "{\n";
            
            const Model::PropertyList& properties = entity.properties();
            Model::PropertyList::const_iterator it, end;
            for (it = properties.begin(), end = properties.end(); it != end; ++it) {
                const Model::Property& property = *it;
                stream << "\"" << property.key() << "\" \"" << property.value() << "\"" << "\n";
            }
        }
        
        void MapWriter::writeEntityFooter(std::ostream& stream) {
            stream << "}\n";
        }

        void MapWriter::writeEntity(const Model::Entity& entity, std::ostream& stream) {
            writeEntityHeader(entity, stream);
            const Model::BrushList& brushes = entity.brushes();
            for (unsigned int i = 0; i < brushes.size(); i++)
                writeBrush(*brushes[i], stream);
            writeEntityFooter(stream);
        }

        MapWriter::MapWriter() {
            StringStream str;
            str <<
            "( %." << FloatPrecision << "g " <<
              "%." << FloatPrecision << "g " <<
              "%." << FloatPrecision << "g ) " <<
            "( %." << FloatPrecision << "g " <<
              "%." << FloatPrecision << "g " <<
              "%." << FloatPrecision << "g ) " <<
            "( %." << FloatPrecision << "g " <<
              "%." << FloatPrecision << "g " <<
              "%." << FloatPrecision << "g ) " <<
            "%s %.6g %.6g %.6g %.6g %.6g\n";
            
            FaceFormat = str.str();
        }
        
        void MapWriter::writeObjectsToStream(const Model::EntityList& pointEntities, const Model::BrushList& brushes, std::ostream& stream) {
            assert(stream.good());
            stream.unsetf(std::ios::floatfield);

            Model::Entity* worldspawn = NULL;
            
            // group the brushes by their containing entities
            typedef std::map<Model::Entity*, Model::BrushList> EntityBrushMap;
            EntityBrushMap entityToBrushes;
            
            Model::BrushList::const_iterator brushIt, brushEnd;
            for (brushIt = brushes.begin(), brushEnd = brushes.end(); brushIt != brushEnd; ++brushIt) {
                Model::Brush& brush = **brushIt;
                Model::Entity& entity = *brush.entity();
                entityToBrushes[&entity].push_back(&brush);
                if (entity.worldspawn())
                    worldspawn = &entity;
            }
            
            // write worldspawn first
            if (worldspawn != NULL) {
                Model::BrushList& brushList = entityToBrushes[worldspawn];
                writeEntityHeader(*worldspawn, stream);
                for (brushIt = brushList.begin(), brushEnd = brushList.end(); brushIt != brushEnd; ++brushIt) {
                    writeBrush(**brushIt, stream);
                }
                writeEntityFooter(stream);
            }
            
            // now write the point entities
            Model::EntityList::const_iterator entityIt, entityEnd;
            for (entityIt = pointEntities.begin(), entityEnd = pointEntities.end(); entityIt != entityEnd; ++entityIt) {
                Model::Entity& entity = **entityIt;
                writeEntity(entity, stream);
            }

            // finally write the brush entities
            EntityBrushMap::iterator it, end;
            for (it = entityToBrushes.begin(), end = entityToBrushes.end(); it != end; ++it) {
                Model::Entity* entity = it->first;
                if (entity != worldspawn) {
                    Model::BrushList& brushList = it->second;
                    writeEntityHeader(*entity, stream);
                    for (brushIt = brushList.begin(), brushEnd = brushList.end(); brushIt != brushEnd; ++brushIt) {
                        writeBrush(**brushIt, stream);
                    }
                    writeEntityFooter(stream);
                }
            }
        }
        
        void MapWriter::writeFacesToStream(const Model::FaceList& faces, std::ostream& stream) {
            assert(stream.good());
            stream.unsetf(std::ios::floatfield);
            
            for (unsigned int i = 0; i < faces.size(); i++)
                writeFace(*faces[i], stream);
        }

        void MapWriter::writeToStream(const Model::Map& map, std::ostream& stream) {
            assert(stream.good());
            stream.unsetf(std::ios::floatfield);
            
            const Model::EntityList& entities = map.entities();
            for (unsigned int i = 0; i < entities.size(); i++)
                writeEntity(*entities[i], stream);
        }
        
        void MapWriter::writeToFileAtPath(Model::Map& map, const String& path, bool overwrite) {
            FileManager fileManager;
            if (fileManager.exists(path) && !overwrite)
                return;
            
            const String directoryPath = fileManager.deleteLastPathComponent(path);
            if (!fileManager.exists(directoryPath))
                fileManager.makeDirectory(directoryPath);
            
            FILE* stream = fopen(path.c_str(), "w");
            if (stream == NULL)
                throw IOException::openError(path);
            // std::fstream stream(path.c_str(), std::ios::out | std::ios::trunc);

            size_t lineNumber = 1;
            const Model::EntityList& entities = map.entities();
            for (unsigned int i = 0; i < entities.size(); i++)
                lineNumber += writeEntity(*entities[i], lineNumber, stream);
            fclose(stream);
        }
    }
}
