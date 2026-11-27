/**
 * @file Commands.cpp
 * @brief Neurons go in before their links and come out after them.
 */

#include "ui/Commands.h"

#include "ui/GraphScene.h"

#include <QCoreApplication>

namespace Ogt
{

ChangeSetCommand::ChangeSetCommand(GraphScene* scene, std::vector<Neuron> neurons, std::vector<Link> links,
                                   bool remove, const QString& text)
    : QUndoCommand(text)
    , scene_(scene)
    , neurons_(std::move(neurons))
    , links_(std::move(links))
    , remove_(remove)
{
}

void ChangeSetCommand::redo()
{
    remove_ ? Erase() : Insert();
}

void ChangeSetCommand::undo()
{
    remove_ ? Insert() : Erase();
}

void ChangeSetCommand::Insert()
{
    for (const Neuron& neuron : neurons_)
        scene_->InsertNeuron(neuron);
    for (const Link& link : links_)
        scene_->InsertLink(link);
}

void ChangeSetCommand::Erase()
{
    for (const Link& link : links_)
        scene_->EraseLink(link.from, link.to);
    for (const Neuron& neuron : neurons_)
        scene_->EraseNeuron(neuron.id);
}

MoveCommand::MoveCommand(GraphScene* scene, QHash<quint32, QPoint> before, QHash<quint32, QPoint> after,
                         const QString& text)
    : QUndoCommand(text)
    , scene_(scene)
    , before_(std::move(before))
    , after_(std::move(after))
{
}

void MoveCommand::redo()
{
    scene_->PlaceNeurons(after_);
}

void MoveCommand::undo()
{
    scene_->PlaceNeurons(before_);
}

EditNeuronCommand::EditNeuronCommand(GraphScene* scene, Neuron before, Neuron after)
    : QUndoCommand(QCoreApplication::translate("Ogt", "Edit %1").arg(after.title))
    , scene_(scene)
    , before_(std::move(before))
    , after_(std::move(after))
{
}

void EditNeuronCommand::redo()
{
    scene_->ReplaceNeuron(after_);
}

void EditNeuronCommand::undo()
{
    scene_->ReplaceNeuron(before_);
}

bool EditNeuronCommand::mergeWith(const QUndoCommand* other)
{
    const auto* edit = static_cast<const EditNeuronCommand*>(other);
    if (edit->after_.id != after_.id)
        return false;
    after_ = edit->after_;
    return true;
}

EditLinkCommand::EditLinkCommand(GraphScene* scene, Link before, Link after)
    : QUndoCommand(QCoreApplication::translate("Ogt", "Edit link"))
    , scene_(scene)
    , before_(std::move(before))
    , after_(std::move(after))
{
}

void EditLinkCommand::redo()
{
    scene_->ReplaceLink(after_);
}

void EditLinkCommand::undo()
{
    scene_->ReplaceLink(before_);
}

bool EditLinkCommand::mergeWith(const QUndoCommand* other)
{
    const auto* edit = static_cast<const EditLinkCommand*>(other);
    if (edit->after_.from != after_.from || edit->after_.to != after_.to)
        return false;
    after_ = edit->after_;
    return true;
}

} // namespace Ogt
