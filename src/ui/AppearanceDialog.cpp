/**
 * @file AppearanceDialog.cpp
 * @brief Edits and saves the visual settings of the application.
 */

#include "ui/AppearanceDialog.h"

#include "ui/Theme.h"

#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace Ogt
{

namespace
{

void SetButtonColor(QPushButton* button, const QColor& color)
{
    button->setProperty("color", color);
    button->setText(color.name(QColor::HexRgb).toUpper());
    button->setStyleSheet(QString("QPushButton { border-left: 24px solid %1; }").arg(color.name()));
}

QColor ButtonColor(const QPushButton* button)
{
    return button->property("color").value<QColor>();
}

} // namespace

AppearanceDialog::AppearanceDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Appearance"));
    setMinimumWidth(420);

    const AppearanceSettings& saved = Theme::Get().Appearance();
    const ThemeTokens& tokens = Theme::Get().Tokens();
    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    font_ = new QFontComboBox(this);
    font_->setCurrentFont(QFont(saved.fontFamily.isEmpty() ? QApplication::font().family() : saved.fontFamily));
    form->addRow(tr("Font"), font_);
    canvas_ = AddColorRow(form, tr("Canvas color"), saved.canvas.isValid() ? saved.canvas : tokens.canvas);
    surface_ = AddColorRow(form, tr("Node color"), saved.surface.isValid() ? saved.surface : tokens.surface);
    text_ = AddColorRow(form, tr("Text color"), saved.text.isValid() ? saved.text : tokens.text);
    accent_ = AddColorRow(form, tr("Accent color"), saved.accent.isValid() ? saved.accent : tokens.accent);
    link_ = AddColorRow(form, tr("Connection color"), saved.link.isValid() ? saved.link : tokens.link);

    linkStyle_ = new QComboBox(this);
    linkStyle_->addItem(tr("Curved"), int(LinkStyle::Curved));
    linkStyle_->addItem(tr("Straight"), int(LinkStyle::Straight));
    linkStyle_->setCurrentIndex(linkStyle_->findData(int(saved.linkStyle)));
    form->addRow(tr("Connection style"), linkStyle_);

    linkWidth_ = new QDoubleSpinBox(this);
    linkWidth_->setRange(0.75, 5.0);
    linkWidth_->setSingleStep(0.25);
    linkWidth_->setSuffix(tr(" px"));
    linkWidth_->setValue(saved.linkWidth);
    form->addRow(tr("Connection width"), linkWidth_);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    auto* reset = buttons->addButton(tr("Restore defaults"), QDialogButtonBox::ResetRole);
    connect(reset, &QPushButton::clicked, this, &AppearanceDialog::Reset);
    connect(buttons, &QDialogButtonBox::accepted, this, &AppearanceDialog::Apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

QPushButton* AppearanceDialog::AddColorRow(QFormLayout* layout, const QString& label, const QColor& initial)
{
    auto* button = new QPushButton(this);
    SetButtonColor(button, initial);
    connect(button, &QPushButton::clicked, this, [this, button] { ChooseColor(button); });
    layout->addRow(label, button);
    return button;
}

void AppearanceDialog::ChooseColor(QPushButton* button)
{
    const QColor color = QColorDialog::getColor(ButtonColor(button), this, tr("Choose a color"));
    if (color.isValid())
        SetButtonColor(button, color);
}

void AppearanceDialog::Apply()
{
    AppearanceSettings appearance;
    appearance.fontFamily = font_->currentFont().family();
    appearance.canvas = ButtonColor(canvas_);
    appearance.surface = ButtonColor(surface_);
    appearance.text = ButtonColor(text_);
    appearance.accent = ButtonColor(accent_);
    appearance.link = ButtonColor(link_);
    appearance.linkStyle = LinkStyle(linkStyle_->currentData().toInt());
    appearance.linkWidth = linkWidth_->value();
    Theme::Get().ApplyAppearance(*qApp, appearance);
    accept();
}

void AppearanceDialog::Reset()
{
    Theme::Get().ResetAppearance(*qApp);
    accept();
}

} // namespace Ogt
