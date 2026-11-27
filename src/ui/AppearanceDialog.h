/**
 * @file AppearanceDialog.h
 * @brief Edits graph colors, fonts and link styles.
 */

#ifndef OGT_APPEARANCE_DIALOG_H
#define OGT_APPEARANCE_DIALOG_H

#include <QDialog>
#include <QColor>

class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QFormLayout;
class QPushButton;

namespace Ogt
{

class AppearanceDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AppearanceDialog(QWidget* parent = nullptr);

private:
    QPushButton* AddColorRow(QFormLayout* layout, const QString& label, const QColor& initial);
    void ChooseColor(QPushButton* button);
    void Apply();
    void Reset();

    QFontComboBox* font_;
    QPushButton* canvas_;
    QPushButton* surface_;
    QPushButton* text_;
    QPushButton* accent_;
    QPushButton* link_;
    QComboBox* linkStyle_;
    QDoubleSpinBox* linkWidth_;
};

} // namespace Ogt

#endif // OGT_APPEARANCE_DIALOG_H
