/**
 * @file Theme.h
 * @brief Stores the colours and fonts used by the light and dark themes.
 */

#ifndef OGT_THEME_H
#define OGT_THEME_H

#include "core/NeuronKind.h"

#include <QColor>
#include <QObject>

class QApplication;
class QFont;

namespace Ogt
{

enum class ThemeMode
{
    System,
    Light,
    Dark,
};

enum class LinkStyle
{
    Curved,
    Straight,
};

struct AppearanceSettings
{
    QString fontFamily;
    QColor canvas;
    QColor surface;
    QColor text;
    QColor accent;
    QColor link;
    LinkStyle linkStyle = LinkStyle::Curved;
    qreal linkWidth = 1.5;
};

struct ThemeTokens
{
    bool dark = false;
    QColor canvas;      // behind the graph
    QColor gridDot;
    QColor surface;     // panels, neurons
    QColor raised;      // hover, inputs
    QColor selected;    // selected rows
    QColor hairline;    // borders, already at their low opacity
    QColor text;
    QColor textSecondary;
    QColor textMuted;
    QColor accent;
    QColor onAccent;
    QColor link;        // edges between neurons
    QColor danger;
};

class Theme final : public QObject
{
    Q_OBJECT

public:
    static Theme& Get();

    const ThemeTokens& Tokens() const { return tokens_; }
    ThemeMode Mode() const { return mode_; }
    const AppearanceSettings& Appearance() const { return appearance_; }
    QColor KindColor(NeuronKind kind) const;
    QColor StatusColor(TaskStatus status) const;

    /// Applies the palette to the whole application and remembers the choice.
    void Apply(QApplication& app, ThemeMode mode);
    /// Flips between light and dark from whatever is showing now.
    void Toggle(QApplication& app);
    /// Applies and persists user-selected colours, font and connection styling.
    void ApplyAppearance(QApplication& app, const AppearanceSettings& appearance);
    void ResetAppearance(QApplication& app);

    // Type scale. Hierarchy comes from size and colour, weights stay at 400 and 500.
    static constexpr int kSmall = 12;
    static constexpr int kBody = 13;
    static constexpr int kTitle = 15;
    static constexpr int kHeading = 20;
    static QFont Font(int pixelSize, bool medium = false);

signals:
    void Changed();

private:
    Theme();
    void Rebuild(QApplication& app);

    ThemeMode mode_ = ThemeMode::System;
    ThemeTokens tokens_;
    AppearanceSettings appearance_;
};

} // namespace Ogt

#endif // OGT_THEME_H
