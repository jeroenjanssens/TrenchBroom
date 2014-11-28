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

#include "CommandProcessor.h"

#include "Exceptions.h"

#include <wx/time.h>

#include <algorithm>

namespace TrenchBroom {
    namespace Controller {
        const Command::CommandType CommandGroup::Type = Command::freeType();
        
        CommandGroup::CommandGroup(const String& name, const bool undoable, const Command::List& commands,
                                   Notifier1<Command::Ptr>& commandDoNotifier,
                                   Notifier1<Command::Ptr>& commandDoneNotifier,
                                   Notifier1<Command::Ptr>& commandUndoNotifier,
                                   Notifier1<Command::Ptr>& commandUndoneNotifier) :
        Command(Type, name, undoable, false),
        m_commands(commands),
        m_commandDoNotifier(commandDoNotifier),
        m_commandDoneNotifier(commandDoneNotifier),
        m_commandUndoNotifier(commandUndoNotifier),
        m_commandUndoneNotifier(commandUndoneNotifier) {}
        
        bool CommandGroup::doPerformDo() {
            List::iterator it, end;
            for (it = m_commands.begin(), end = m_commands.end(); it != end; ++it) {
                Command::Ptr command = *it;
                m_commandDoNotifier(command);
                if (!command->performDo())
                    throw CommandProcessorException("Partial failure while executing command group");
                m_commandDoneNotifier(command);
            }
            return true;
        }
        
        bool CommandGroup::doPerformUndo() {
            List::reverse_iterator it, end;
            for (it = m_commands.rbegin(), end = m_commands.rend(); it != end; ++it) {
                Command::Ptr command = *it;
                m_commandUndoNotifier(command);
                if (!command->performUndo())
                    throw CommandProcessorException("Partial failure while undoing command group");
                m_commandUndoneNotifier(command);
            }
            return true;
        }
        
        bool CommandGroup::doIsRepeatDelimiter() const {
            Command::List::const_iterator it, end;
            for (it = m_commands.begin(), end = m_commands.end(); it != end; ++it) {
                Command::Ptr command = *it;
                if (command->isRepeatDelimiter())
                    return true;
            }
            return false;
        }
        
        bool CommandGroup::doIsRepeatable(View::MapDocumentSPtr document) const {
            Command::List::const_iterator it, end;
            for (it = m_commands.begin(), end = m_commands.end(); it != end; ++it) {
                Command::Ptr command = *it;
                if (!command->isRepeatable(document))
                    return false;
            }
            return true;
        }
        
        Command* CommandGroup::doRepeat(View::MapDocumentSPtr document) const {
            Command::List clones;
            Command::List::const_iterator it, end;
            for (it = m_commands.begin(), end = m_commands.end(); it != end; ++it) {
                Command::Ptr command = *it;
                assert(command->isRepeatable(document));
                Command::Ptr clone = command->repeat(document);
                clones.push_back(clone);
            }
            return new CommandGroup(name(), undoable(), clones, m_commandDoNotifier, m_commandDoneNotifier, m_commandUndoNotifier, m_commandUndoneNotifier);
        }
        
        bool CommandGroup::doCollateWith(Ptr command) {
            return false;
        }
        
        const wxLongLong CommandProcessor::CollationInterval(1000);
        
        CommandProcessor::CommandProcessor() :
        m_clearRepeatableCommandStack(false),
        m_lastCommandTimestamp(0),
        m_groupUndoable(false),
        m_groupLevel(0) {}
        
        bool CommandProcessor::hasLastCommand() const {
            return !m_lastCommandStack.empty();
        }
        
        bool CommandProcessor::hasNextCommand() const {
            return !m_nextCommandStack.empty();
        }
        
        const String& CommandProcessor::lastCommandName() const {
            if (!hasLastCommand())
                throw CommandProcessorException("Command stack is empty");
            return m_lastCommandStack.back()->name();
        }
        
        const String& CommandProcessor::nextCommandName() const {
            if (!hasNextCommand())
                throw CommandProcessorException("Undo stack is empty");
            return m_nextCommandStack.back()->name();
        }
        
        void CommandProcessor::beginUndoableGroup(const String& name) {
            beginGroup(name, true);
        }
        
        void CommandProcessor::beginOneShotGroup(const String& name) {
            beginGroup(name, false);
        }
        
        void CommandProcessor::closeGroup() {
            if (m_groupLevel == 0)
                throw CommandProcessorException("Group stack is empty");
            --m_groupLevel;
            if (m_groupLevel == 0)
                createAndStoreCommandGroup();
        }
        
        void CommandProcessor::undoGroup() {
            while (!m_groupedCommands.empty())
                popGroupedCommand()->performUndo();
        }
        
        bool CommandProcessor::submitCommand(Command::Ptr command) {
            if (!doCommand(command))
                return false;
            if (!command->undoable()) {
                m_lastCommandStack.clear();
                m_nextCommandStack.clear();
            }
            return true;
        }
        
        bool CommandProcessor::submitAndStoreCommand(Command::Ptr command) {
            if (submitAndStoreCommand(command, true)) {
                if (m_groupLevel == 0)
                    pushRepeatableCommand(command);
                return true;
            }
            return false;
        }
        
        bool CommandProcessor::undoLastCommand() {
            if (m_groupLevel > 0)
                throw CommandProcessorException("Cannot undo individual commands of a command group");
            
            Command::Ptr command = popLastCommand();
            if (undoCommand(command)) {
                pushNextCommand(command);
                popLastRepeatableCommand(command);
                return true;
            }
            return false;
        }
        
        bool CommandProcessor::redoNextCommand() {
            if (m_groupLevel > 0)
                throw CommandProcessorException("Cannot redo while in a command group");
            
            Command::Ptr command = popNextCommand();
            if (doCommand(command)) {
                pushLastCommand(command, false);
                if (m_groupLevel == 0)
                    pushRepeatableCommand(command);
                return true;
            }
            return false;
        }
        
        bool CommandProcessor::repeatLastCommands(View::MapDocumentWPtr document) {
            View::MapDocumentSPtr doc = lock(document);
            Command::List commands;
            CommandStack::iterator it, end;
            for (it = m_repeatableCommandStack.begin(), end = m_repeatableCommandStack.end(); it != end; ++it) {
                Command::Ptr command = *it;
                if (command->isRepeatable(doc))
                    commands.push_back(command->repeat(doc));
            }
            
            if (commands.empty())
                return false;

            StringStream name;
            name << "Repeat " << commands.size() << " Commands";

            Command::Ptr repeatableCommand = createCommandGroup(name.str(), true, commands);
            return submitAndStoreCommand(repeatableCommand, false);
        }
        
        void CommandProcessor::clearRepeatableCommands() {
            m_repeatableCommandStack.clear();
            m_clearRepeatableCommandStack = false;
        }

        bool CommandProcessor::submitAndStoreCommand(Command::Ptr command, const bool collate) {
            if (!submitCommand(command))
                return false;
            if (command->undoable())
                storeCommand(command, collate);
            if (!m_nextCommandStack.empty())
                m_nextCommandStack.clear();
            return true;
        }
        
        bool CommandProcessor::doCommand(Command::Ptr command) {
            if (command->type() != CommandGroup::Type)
                commandDoNotifier(command);
            if (command->performDo()) {
                if (command->type() != CommandGroup::Type)
                    commandDoneNotifier(command);
                return true;
            }
            if (command->type() != CommandGroup::Type)
                commandDoFailedNotifier(command);
            return false;
        }
        
        bool CommandProcessor::undoCommand(Command::Ptr command) {
            if (command->type() != CommandGroup::Type)
                commandUndoNotifier(command);
            if (command->performUndo()) {
                if (command->type() != CommandGroup::Type)
                    commandUndoneNotifier(command);
                return true;
            }
            if (command->type() != CommandGroup::Type)
                commandUndoFailedNotifier(command);
            return false;
        }
        
        void CommandProcessor::storeCommand(Command::Ptr command, const bool collate) {
            if (m_groupLevel == 0)
                pushLastCommand(command, collate);
            else
                pushGroupedCommand(command);
        }
        
        void CommandProcessor::beginGroup(const String& name, const bool undoable) {
            if (m_groupLevel == 0) {
                m_groupName = name;
                m_groupUndoable = undoable;
            }
            ++m_groupLevel;
        }
        
        void CommandProcessor::pushGroupedCommand(Command::Ptr command) {
            assert(m_groupLevel > 0);
            if (m_groupUndoable && !command->undoable())
                throw CommandProcessorException("Cannot add one-shot command to undoable command group");
            
            if (!m_groupedCommands.empty()) {
                Command::Ptr lastCommand = m_groupedCommands.back();
                if (!lastCommand->collateWith(command))
                    m_groupedCommands.push_back(command);
            } else {
                m_groupedCommands.push_back(command);
            }
        }
        
        Command::Ptr CommandProcessor::popGroupedCommand() {
            assert(m_groupLevel > 0);
            if (m_groupedCommands.empty())
                throw CommandProcessorException("Group command stack is empty");
            Command::Ptr groupedCommand = m_groupedCommands.back();
            m_groupedCommands.pop_back();
            return groupedCommand;
        }
        
        void CommandProcessor::createAndStoreCommandGroup() {
            if (!m_groupedCommands.empty()) {
                if (m_groupName.empty())
                    m_groupName = m_groupedCommands.front()->name();
                Command::Ptr group = createCommandGroup(m_groupName, m_groupUndoable, m_groupedCommands);
                m_groupedCommands.clear();
                pushLastCommand(group, false);
                pushRepeatableCommand(group);
            }
            m_groupName = "";
            m_groupUndoable = false;
        }
        
        Command::Ptr CommandProcessor::createCommandGroup(const String& name, const bool undoable, const Command::List& commands) {
            return Command::Ptr(new CommandGroup(name, undoable, commands,
                                                 commandDoNotifier,
                                                 commandDoneNotifier,
                                                 commandUndoNotifier,
                                                 commandUndoneNotifier));
        }

        void CommandProcessor::pushLastCommand(Command::Ptr command, const bool collate) {
            assert(m_groupLevel == 0);
            
            const wxLongLong timestamp = ::wxGetLocalTimeMillis();
            if (collate && !m_lastCommandStack.empty() && timestamp - m_lastCommandTimestamp <= CollationInterval) {
                Command::Ptr lastCommand = m_lastCommandStack.back();
                if (!lastCommand->collateWith(command))
                    m_lastCommandStack.push_back(command);
            } else {
                m_lastCommandStack.push_back(command);
            }
            m_lastCommandTimestamp = timestamp;
        }

        void CommandProcessor::pushNextCommand(Command::Ptr command) {
            assert(m_groupLevel == 0);
            m_nextCommandStack.push_back(command);
        }
        
        void CommandProcessor::pushRepeatableCommand(Command::Ptr command) {
            if (command->isRepeatDelimiter()) {
                m_clearRepeatableCommandStack = true;
            } else {
                if (m_clearRepeatableCommandStack) {
                    m_repeatableCommandStack.clear();
                    m_clearRepeatableCommandStack = false;
                }
                m_repeatableCommandStack.push_back(command);
            }
        }
        
        Command::Ptr CommandProcessor::popLastCommand() {
            assert(m_groupLevel == 0);
            if (m_lastCommandStack.empty())
                throw CommandProcessorException("Command stack is empty");
            Command::Ptr lastCommand = m_lastCommandStack.back();
            m_lastCommandStack.pop_back();
            return lastCommand;
        }

        Command::Ptr CommandProcessor::popNextCommand() {
            assert(m_groupLevel == 0);
            if (m_nextCommandStack.empty())
                throw CommandProcessorException("Command stack is empty");
            Command::Ptr nextCommand = m_nextCommandStack.back();
            m_nextCommandStack.pop_back();
            return nextCommand;
        }
        
        void CommandProcessor::popLastRepeatableCommand(Command::Ptr command) {
            if (!m_repeatableCommandStack.empty() && m_repeatableCommandStack.back() == command)
                m_repeatableCommandStack.pop_back();
        }
    }
}