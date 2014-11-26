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

#include "TextRenderer.h"

#include "CollectionUtils.h"
#include "AttrString.h"
#include "Renderer/Camera.h"
#include "Renderer/FontManager.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderUtils.h"
#include "Renderer/ShaderManager.h"
#include "Renderer/Shaders.h"
#include "Renderer/TextAnchor.h"
#include "Renderer/TextureFont.h"

#include <map>

namespace TrenchBroom {
    namespace Renderer {
        const size_t TextRenderer::RectCornerSegments = 3;
        const float TextRenderer::RectCornerRadius = 3.0f;
        
        struct TextRenderer::Entry {
            float distance;
            bool onTop;
            Vec2f::List vertices;
            Vec2f size;
            Vec3f offset;
            Color textColor;
            Color backgroundColor;
            
            Entry(const float i_distance, const bool i_onTop, Vec2f::List& i_vertices, const Vec2f& i_size, const Vec3f& i_offset, const Color& i_textColor, const Color& i_backgroundColor) :
            distance(i_distance),
            onTop(i_onTop),
            vertices(0),
            size(i_size),
            offset(i_offset),
            textColor(i_textColor),
            backgroundColor(i_backgroundColor) {
                using std::swap;
                swap(vertices, i_vertices);
            }
        };

        TextRenderer::TextRenderer(const FontDescriptor& fontDescriptor, const Vec2f& inset) :
        m_fontDescriptor(fontDescriptor),
        m_inset(inset),
        m_fadeOverlapping(true) {}

        void TextRenderer::renderString(RenderContext& renderContext, const Color& textColor, const Color& backgroundColor, const AttrString& string, const TextAnchor& position) {
            renderString(renderContext, textColor, backgroundColor, string, position, false);
        }
        
        void TextRenderer::renderStringOnTop(RenderContext& renderContext, const Color& textColor, const Color& backgroundColor, const AttrString& string, const TextAnchor& position) {
            renderString(renderContext, textColor, backgroundColor, string, position, true);
        }

        void TextRenderer::renderString(RenderContext& renderContext, const Color& textColor, const Color& backgroundColor, const AttrString& string, const TextAnchor& position, const bool onTop) {
            const Camera& camera = renderContext.camera();
            const float distance = camera.perpendicularDistanceTo(position.position());
            
            if (distance <= 0.0f)
                return;

            FontManager& fontManager = renderContext.fontManager();
            TextureFont& font = fontManager.font(m_fontDescriptor);
            
            const Vec2f size = font.measure(string).rounded();
            const Vec3f offset = position.offset(camera, size);
            const Vec2f actualSize = size + 2.0f * m_inset;

            const Camera::Viewport& viewport = camera.unzoomedViewport();
            if (!viewport.contains(offset.x() - m_inset.x(), offset.y() - m_inset.y(), actualSize.x(), actualSize.y()))
                return;

            Vec2f::List vertices = font.quads(string, true);
            m_entries.push_back(Entry(distance, onTop, vertices, size, offset, textColor, backgroundColor));
            
            m_fadeOverlapping = renderContext.render3D();
        }

        class TextRenderer::CompareEntriesByDistance {
        public:
            bool operator()(const Entry& lhs, const Entry& rhs) const {
                if (lhs.onTop && !rhs.onTop)
                    return true;
                if (!lhs.onTop && rhs.onTop)
                    return false;
                if (Math::lt(lhs.distance, rhs.distance))
                    return true;
                if (Math::gt(lhs.distance, rhs.distance))
                    return false;
                if (Math::lt(lhs.offset.x(), rhs.offset.x()))
                    return true;
                if (Math::gt(lhs.offset.x(), rhs.offset.x()))
                    return false;
                return lhs.offset.y() < rhs.offset.y();
            }
        };
        
        void TextRenderer::doPrepare(Vbo& vbo) {
            EntryItList entries;
            EntryItList entriesOnTop;
            
            size_t textVertexCount = 0;
            size_t rectVertexCount = 0;
            size_t textVertexCountOnTop = 0;
            size_t rectVertexCountOnTop = 0;
            
            VectorUtils::sort(m_entries, CompareEntriesByDistance());
            
            EntryList::iterator it, cur, end;
            for (it = m_entries.begin(), end = m_entries.end(); it != end; ++it) {
                Entry& entry = *it;
                const BBox2f entryBounds(entry.offset - m_inset, entry.offset + entry.size + m_inset);

                float overlappingArea = 0.0f;
                cur = m_entries.begin();
                while (cur != it && overlappingArea <= 0.0f) {
                    const Entry& other = *cur;
                    const BBox2f otherBounds(other.offset - m_inset, other.offset + other.size + m_inset);
                    const BBox2f intersection = entryBounds.intersectedWith(otherBounds);
                    const Vec2f intersectionSize = intersection.size();
                    overlappingArea = intersectionSize.x() * intersectionSize.y();
                    
                    ++cur;
                }

                
                if (overlappingArea > 0.0f) {
                    const float factor = 1.0f - overlappingArea / 100.0f;
                    if (m_fadeOverlapping && factor <= 1.0f) {
                        entry.textColor[3] *= factor;
                        entry.backgroundColor[3] *= factor;
                    } else {
                        continue;
                    }
                }
                
                if (entry.onTop) {
                    entriesOnTop.push_back(it);
                    textVertexCountOnTop += entry.vertices.size();
                    rectVertexCountOnTop += roundedRect2DVertexCount(RectCornerSegments);
                } else {
                    entries.push_back(it);
                    textVertexCount += entry.vertices.size();
                    rectVertexCount += roundedRect2DVertexCount(RectCornerSegments);
                }
            }
            
            TextVertex::List textVertices, textVerticesOnTop;
            RectVertex::List rectVertices, rectVerticesOnTop;

            textVertices.reserve(textVertexCount);
            rectVertices.reserve(rectVertexCount);
            textVerticesOnTop.reserve(textVertexCountOnTop);
            rectVerticesOnTop.reserve(rectVertexCountOnTop);
            
            buildArrays(entries, textVertices, rectVertices);
            buildArrays(entriesOnTop, textVerticesOnTop, rectVerticesOnTop);
            
            m_textArray = VertexArray::swap(GL_QUADS, textVertices);
            m_rectArray = VertexArray::swap(GL_TRIANGLES, rectVertices);
            m_textArrayOnTop = VertexArray::swap(GL_QUADS, textVerticesOnTop);
            m_rectArrayOnTop = VertexArray::swap(GL_TRIANGLES, rectVerticesOnTop);
            
            m_textArray.prepare(vbo);
            m_rectArray.prepare(vbo);
            m_textArrayOnTop.prepare(vbo);
            m_rectArrayOnTop.prepare(vbo);
        }

        void TextRenderer::buildArrays(const EntryItList& entries, TextVertex::List& textVertices, RectVertex::List& rectVertices) {
            EntryItList::const_iterator it, end;
            for (it = entries.begin(), end = entries.end(); it != end; ++it)
                addEntry(**it, textVertices, rectVertices);
        }

        void TextRenderer::addEntry(const Entry& entry, TextVertex::List& textVertices, RectVertex::List& rectVertices) {
            const Vec2f::List& stringVertices = entry.vertices;
            const Vec2f& stringSize = entry.size;
            
            const Vec3f& offset = entry.offset;
            const Color& textColor = entry.textColor;
            const Color& rectColor = entry.backgroundColor;
            
            for (size_t i = 0; i < stringVertices.size() / 2; ++i) {
                const Vec2f& position2 = stringVertices[2 * i];
                const Vec2f& texCoords = stringVertices[2 * i + 1];
                textVertices.push_back(TextVertex(Vec3f(position2 + offset, -offset.z()), texCoords, textColor));
            }

            const Vec2f::List rect = roundedRect2D(stringSize + 2.0f * m_inset, RectCornerRadius, RectCornerSegments);
            for (size_t i = 0; i < rect.size(); ++i) {
                const Vec2f& vertex = rect[i];
                rectVertices.push_back(RectVertex(Vec3f(vertex + offset + stringSize / 2.0f, -offset.z()), rectColor));
            }
        }

        void TextRenderer::doRender(RenderContext& renderContext) {
            const Camera::Viewport& viewport = renderContext.camera().unzoomedViewport();
            const Mat4x4f projection = orthoMatrix(0.0f, 1.0f,
                                                   static_cast<float>(viewport.x),
                                                   static_cast<float>(viewport.height),
                                                   static_cast<float>(viewport.width),
                                                   static_cast<float>(viewport.y));
            const Mat4x4f view = viewMatrix(Vec3f::NegZ, Vec3f::PosY);
            ReplaceTransformation ortho(renderContext.transformation(), projection, view);
            
            render(renderContext, m_textArray, m_rectArray);
            
            glDisable(GL_DEPTH_TEST);
            render(renderContext, m_textArrayOnTop, m_rectArrayOnTop);
            glEnable(GL_DEPTH_TEST);
        }

        void TextRenderer::render(RenderContext& renderContext, VertexArray& textArray, VertexArray& rectArray) {
            FontManager& fontManager = renderContext.fontManager();
            TextureFont& font = fontManager.font(m_fontDescriptor);
            
            glDisable(GL_TEXTURE_2D);
            
            ActiveShader backgroundShader(renderContext.shaderManager(), Shaders::TextBackgroundShader);
            rectArray.render();
            
            glEnable(GL_TEXTURE_2D);
            
            ActiveShader textShader(renderContext.shaderManager(), Shaders::ColoredTextShader);
            textShader.set("Texture", 0);
            font.activate();
            textArray.render();
            font.deactivate();
        }
    }
}
