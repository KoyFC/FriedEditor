#include "fried/editor/command_stack.h"

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

        bool isMerged = merge && m_position > 0 && m_commands[m_position - 1]->mergeWith(*command);
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

    void CommandStack::redo()
    {
        if (canRedo())
        {
            m_commands[m_position]->apply(m_document);
            ++m_position;
            notify();
        }
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
