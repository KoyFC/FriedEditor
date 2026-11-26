#include "fried/editor/command_stack.h"

#include <algorithm>

namespace fried::editor
{
    CommandStack::CommandStack(SceneDocument &document)
        : m_document(document)
    {
    }

    bool CommandStack::execute(std::unique_ptr<Command> command, bool merge)
    {
        if (!command->apply(m_document))
        {
            return false;
        }

        if (m_cleanPosition > m_position)
        {
            m_isCleanLost = true;
        }
        m_commands.resize(m_position);

        bool isMerged = merge && m_position > m_floor && m_commands[m_position - 1]->mergeWith(*command);
        if (isMerged && m_position == m_cleanPosition)
        {
            m_isCleanLost = true;
        }
        if (!isMerged)
        {
            m_commands.push_back(std::move(command));
            ++m_position;
        }
        notify();
        return true;
    }

    void CommandStack::undo()
    {
        if (canUndo())
        {
            m_commands[--m_position]->revert(m_document);
            notify();
        }
    }

    void CommandStack::undoAndForget()
    {
        if (canUndo())
        {
            m_commands[--m_position]->revert(m_document);
            m_commands.resize(m_position);
            notify();
        }
    }

    void CommandStack::redo()
    {
        if (canRedo())
        {
            m_commands[m_position]->apply(m_document);
            ++m_position;
            notify();
        }
    }

    void CommandStack::jumpTo(std::size_t position)
    {
        position = std::clamp(position, m_floor, m_commands.size());
        if (position == m_position)
        {
            return;
        }
        while (m_position > position)
        {
            m_commands[--m_position]->revert(m_document);
        }
        while (m_position < position)
        {
            m_commands[m_position]->apply(m_document);
            ++m_position;
        }
        notify();
    }

    void CommandStack::hold()
    {
        m_floor = m_position;
        m_isHeld = true;
    }

    void CommandStack::revertHeld()
    {
        while (m_position > m_floor)
        {
            m_commands[--m_position]->revert(m_document);
        }
        m_commands.resize(m_position);
        m_floor = 0;
        m_isHeld = false;
        notify();
    }

    std::vector<std::unique_ptr<Command>> CommandStack::takeHeld()
    {
        const std::size_t done = m_position;
        while (m_position > m_floor)
        {
            m_commands[--m_position]->revert(m_document);
        }
        std::vector<std::unique_ptr<Command>> taken;
        for (std::size_t step = m_floor; step < done; ++step)
        {
            taken.push_back(std::move(m_commands[step]));
        }
        m_commands.resize(m_position);
        m_floor = 0;
        m_isHeld = false;
        notify();
        return taken;
    }

    void CommandStack::markClean()
    {
        m_cleanPosition = m_position;
        m_isCleanLost = false;
        m_document.markSaved();
    }

    void CommandStack::notify()
    {
        if (m_onChange)
        {
            m_onChange();
        }
    }
}
