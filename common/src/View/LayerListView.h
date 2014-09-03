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

#ifndef __TrenchBroom__LayerListView__
#define __TrenchBroom__LayerListView__

#include "Model/ModelTypes.h"
#include "View/ViewTypes.h"

#include <wx/panel.h>

class wxScrolledWindow;

namespace TrenchBroom {
    namespace View {
        class LayerListView : public wxPanel {
        private:
            MapDocumentWPtr m_document;
            ControllerWPtr m_controller;
            
            wxScrolledWindow* m_scrollWindow;
        public:
            LayerListView(wxWindow* parent, MapDocumentWPtr document, ControllerWPtr controller);
            ~LayerListView();
        private:
            void bindObservers();
            void unbindObservers();
            void documentWasChanged();
            void layersWereAdded(const Model::LayerList& layers);
            void layersWereRemoved(const Model::LayerList& layers);
            void layerDidChange(Model::Layer* layer);

            void createGui();
            void bindEvents();
            
            void reload();
        };
    }
}

#endif /* defined(__TrenchBroom__LayerListView__) */