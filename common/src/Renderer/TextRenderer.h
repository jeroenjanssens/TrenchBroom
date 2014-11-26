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

#ifndef __TrenchBroom__TextRenderer__
#define __TrenchBroom__TextRenderer__

#include "VecMath.h"
#include "Color.h"
#include "Renderer/FontDescriptor.h"
#include "Renderer/Renderable.h"
#include "Renderer/VertexArray.h"
#include "Renderer/VertexSpec.h"

#include <map>
#include <vector>

namespace TrenchBroom {
    class AttrString;
    
    namespace Renderer {
        class RenderContext;
        class TextAnchor;
        
        class TextRenderer : public Renderable {
        private:
            static const size_t RectCornerSegments;
            static const float RectCornerRadius;
            
            struct Entry;
            struct RenderInfo;
            
            typedef std::vector<Entry> EntryList;
            typedef std::vector<EntryList::const_iterator> EntryItList;
            
            typedef VertexSpecs::P3T2C4::Vertex TextVertex;
            typedef VertexSpecs::P3C4::Vertex RectVertex;
            
            FontDescriptor m_fontDescriptor;
            Vec2f m_inset;
            bool m_fadeOverlapping;
            
            EntryList m_entries;
            
            VertexArray m_textArray;
            VertexArray m_rectArray;
            VertexArray m_textArrayOnTop;
            VertexArray m_rectArrayOnTop;
        public:
            TextRenderer(const FontDescriptor& fontDescriptor, const Vec2f& inset = Vec2f(4.0f, 4.0f));
            
            void renderString(RenderContext& renderContext, const Color& textColor, const Color& backgroundColor, const AttrString& string, const TextAnchor& position);
            void renderStringOnTop(RenderContext& renderContext, const Color& textColor, const Color& backgroundColor, const AttrString& string, const TextAnchor& position);
        private:
            void renderString(RenderContext& renderContext, const Color& textColor, const Color& backgroundColor, const AttrString& string, const TextAnchor& position, bool onTop);
        private:
            class CompareEntriesByDistance;
            
            void doPrepare(Vbo& vbo);
            void buildArrays(const EntryItList& entries, TextVertex::List& textVertices, RectVertex::List& rectVertices);
            void addEntry(const Entry& entry, TextVertex::List& textVertices, RectVertex::List& rectVertices);
            
            void doRender(RenderContext& renderContext);
            void render(RenderContext& renderContext, VertexArray& textArray, VertexArray& rectArray);
        };
    }
}

#endif /* defined(__TrenchBroom__TextRenderer__) */
