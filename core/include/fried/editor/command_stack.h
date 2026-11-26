#pragma once

#include "fried/editor/scene_document.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fried::editor
{
    // One undoable edit of the document.
    class Command
    {
    public:
        virtual ~Command() = default;

        virtual std::string label() const = 0;

        // Returns false when the edit does not apply, and then changes nothing.
        virtual bool apply(SceneDocument &document) = 0;
        virtual void revert(SceneDocument &document) = 0;

        // Takes in a later edit of the same thing, so a drag is one step to undo. Returns false when it is not that.
        virtual bool mergeWith(const Command &later) { return false; }
    };

    class CommandStack
    {
    public:
        explicit CommandStack(SceneDocument &document);

        // Applies the command and makes it the next thing to undo, dropping whatever was undone. With merge, a command that continues the last one is folded into it.
        bool execute(std::unique_ptr<Command> command, bool merge = false);

        bool canUndo() const { return m_position > m_floor; }
        bool canRedo() const { return m_position < m_commands.size(); }
        std::string undoLabel() const { return canUndo() ? m_commands[m_position - 1]->label() : std::string(); }
        std::string redoLabel() const { return canRedo() ? m_commands[m_position]->label() : std::string(); }

        void undo();
        void redo();

        // The steps that can be undone or redone, from the oldest: a held stack lists only what was done since it was held.
        std::size_t firstStep() const { return m_floor; }
        std::size_t stepCount() const { return m_commands.size(); }
        // How many steps are done: the ones before this position are applied, the ones from it on can be redone.
        std::size_t position() const { return m_position; }
        std::string stepLabel(std::size_t step) const { return m_commands[step]->label(); }
        // Undoes or redoes steps until exactly `position` of them are done, as far as the stack allows.
        void jumpTo(std::size_t position);

        // Takes back the last command and drops it, so it cannot be redone either.
        void undoAndForget();

        // Edits made from now on are temporary: nothing made before can be undone or merged into, and revertHeld() takes back all of them, so the document is what it was when the hold began.
        void hold();
        bool isHeld() const { return m_isHeld; }
        void revertHeld();
        // The same, but the commands are handed back, oldest first, to be applied again. Commands that had been undone are dropped.
        std::vector<std::unique_ptr<Command>> takeHeld();

        // The stack is clean where the document was last saved.
        void markClean();

        // The document differs from its file without a command having made it so, as a scene whose nodes were put under a root when it was read.
        void markUnsaved() { m_isCleanLost = true; }
        bool isClean() const { return m_position == m_cleanPosition && !m_isCleanLost; }

        std::function<void()> m_onChange;

    private:
        void notify();

        SceneDocument &m_document;
        std::vector<std::unique_ptr<Command>> m_commands;
        std::size_t m_position = 0;
        std::size_t m_cleanPosition = 0;
        std::size_t m_floor = 0;
        bool m_isHeld = false;

        // Set once the step the clean position points at is no longer what was saved, by a merge or by a new edit taking its place.
        bool m_isCleanLost = false;
    };
}
