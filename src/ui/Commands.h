/**
 * @file Commands.h
 * @brief Undo steps. Each one keeps the data it needs to go both ways.
 */

#ifndef OGT_COMMANDS_H
#define OGT_COMMANDS_H

#include "core/Graph.h"

#include <QHash>
#include <QUndoCommand>

#include <vector>

namespace Ogt
{

class GraphScene;

/// Adds or removes a group of neurons and links in one undo step.
class ChangeSetCommand final : public QUndoCommand
{
public:
    ChangeSetCommand(GraphScene* scene, std::vector<Neuron> neurons, std::vector<Link> links, bool remove,
                     const QString& text);
    void redo() override;
    void undo() override;

private:
    void Insert();
    void Erase();

    GraphScene* scene_;
    std::vector<Neuron> neurons_;
    std::vector<Link> links_;
    bool remove_;
};

class MoveCommand final : public QUndoCommand
{
public:
    MoveCommand(GraphScene* scene, QHash<quint32, QPoint> before, QHash<quint32, QPoint> after, const QString& text);
    void redo() override;
    void undo() override;

private:
    GraphScene* scene_;
    QHash<quint32, QPoint> before_;
    QHash<quint32, QPoint> after_;
};

/// Typing in the inspector makes one of these per key; consecutive ones on the same neuron merge.
class EditNeuronCommand final : public QUndoCommand
{
public:
    EditNeuronCommand(GraphScene* scene, Neuron before, Neuron after);
    void redo() override;
    void undo() override;
    int id() const override { return 1; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    GraphScene* scene_;
    Neuron before_;
    Neuron after_;
};

class EditLinkCommand final : public QUndoCommand
{
public:
    EditLinkCommand(GraphScene* scene, Link before, Link after);
    void redo() override;
    void undo() override;
    int id() const override { return 2; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    GraphScene* scene_;
    Link before_;
    Link after_;
};

} // namespace Ogt

#endif // OGT_COMMANDS_H
