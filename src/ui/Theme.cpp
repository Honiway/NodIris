/**
 * @file Theme.cpp
 * @brief Builds the light and dark palettes and their style sheet.
 */

#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#include <QStyleHints>

namespace Ogt
{

namespace
{

ThemeTokens LightTokens()
{
    ThemeTokens theme;
    theme.dark = false;
    theme.canvas = QColor("#f4f3ef");
    theme.gridDot = QColor("#d6d3cb");
    theme.surface = QColor("#ffffff");
    theme.raised = QColor("#f7f6f3");
    theme.selected = QColor("#e9eef9");
    theme.hairline = QColor(28, 28, 30, 22);
    theme.text = QColor("#1c1c1e");
    theme.textSecondary = QColor("#55555b");
    theme.textMuted = QColor("#8b8b91");
    theme.accent = QColor("#2f5fc4");
    theme.onAccent = QColor("#ffffff");
    theme.link = QColor("#9c9a94");
    theme.danger = QColor("#b4483f");
    return theme;
}

ThemeTokens DarkTokens()
{
    ThemeTokens theme;
    theme.dark = true;
    theme.canvas = QColor("#151517");
    theme.gridDot = QColor("#2a2a2e");
    theme.surface = QColor("#1e1e21");
    theme.raised = QColor("#26262a");
    theme.selected = QColor("#24304a");
    theme.hairline = QColor(255, 255, 255, 20);
    theme.text = QColor("#ececee");
    theme.textSecondary = QColor("#a6a6ad");
    theme.textMuted = QColor("#6f6f77");
    theme.accent = QColor("#7ca2f0");
    theme.onAccent = QColor("#101012");
    theme.link = QColor("#5c5c63");
    theme.danger = QColor("#e0786e");
    return theme;
}

QString Css(const QColor& color)
{
    return QString("rgba(%1,%2,%3,%4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
}

// The style sheet adds values that QPalette cannot set.
QString StyleSheet(const ThemeTokens& theme)
{
    QString css = R"(
QToolBar { background: @surface; border: none; border-bottom: 1px solid @hairline; padding: 4px 8px; spacing: 4px; }
QToolButton { border: none; border-radius: 8px; padding: 6px; color: @text; }
QToolButton:hover { background: @raised; }
QToolButton:pressed, QToolButton:checked { background: @selected; }
QToolButton::menu-indicator { image: none; width: 0; }
QFrame[role="titleBar"] { background: @surface; border-bottom: 1px solid @hairline; }
QToolButton[windowButton="true"] { border-radius: 0; min-width: 46px; max-width: 46px; min-height: 34px; padding: 0; }
QToolButton[closeButton="true"]:hover { background: #c42b1c; color: white; }
QSplitter::handle { background: @hairline; }
QLineEdit, QPlainTextEdit, QComboBox, QDateEdit {
    background: @raised; border: 1px solid @hairline; border-radius: 8px; padding: 6px 8px; color: @text;
    selection-background-color: @selected; selection-color: @text;
}
QLineEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QDateEdit:focus { border: 1px solid @accent; }
QComboBox::drop-down, QDateEdit::drop-down { border: none; width: 20px; }
QPushButton { background: @raised; border: 1px solid @hairline; border-radius: 8px; padding: 6px 12px; color: @text; }
QPushButton:hover { border-color: @textMuted; }
QPushButton:focus { border-color: @accent; }
QPushButton[primary="true"] { background: @accent; border-color: @accent; color: @onAccent; }
QPushButton[primary="true"]:hover { background: @accent; border-color: @text; }
QPushButton[flat="true"] {
    background: transparent; border-color: transparent; color: @textSecondary; padding: 4px 8px;
}
QPushButton[flat="true"]:hover { color: @text; background: @raised; }
QMenu { background: @surface; border: 1px solid @hairline; border-radius: 10px; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 8px; border-radius: 6px; color: @text; }
QMenu::item:selected { background: @raised; }
QMenu::item:disabled { color: @textMuted; }
QMenu::separator { height: 1px; background: @hairline; margin: 4px 8px; }
QMenu::icon { padding-left: 8px; }
QListWidget { background: transparent; border: none; outline: none; }
QListWidget::item { border-radius: 8px; padding: 8px; color: @text; }
QListWidget::item:hover { background: @raised; }
QListWidget::item:selected { background: @selected; color: @text; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 4px 0; }
QScrollBar::handle:vertical { background: @hairline; border-radius: 4px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar:horizontal { height: 0; }
QToolTip { background: @surface; color: @text; border: 1px solid @hairline; border-radius: 6px; padding: 4px 8px; }
QLabel[role="muted"] { color: @textMuted; font-size: 12px; }
QLabel[role="secondary"] { color: @textSecondary; }
QLabel[role="heading"] { font-size: 20px; font-weight: 500; }
QLineEdit[role="heading"] { font-size: 20px; font-weight: 500; border: none; background: transparent; padding: 0; }
QPlainTextEdit[role="document"] { border: none; background: transparent; padding: 0; }
QFrame[role="bar"] { background: @surface; border-bottom: 1px solid @hairline; }
QLabel[role="section"] { color: @textMuted; font-size: 12px; }
QFrame[role="panel"] { background: @surface; border-left: 1px solid @hairline; }
QFrame[role="preview"] { background: @surface; border: 1px solid @hairline; border-radius: 10px; }
QFrame[role="hairline"] { background: @hairline; max-height: 1px; border: none; }
)";
    const std::pair<const char*, QColor> tokens[] = {
        {"@surface", theme.surface},     {"@raised", theme.raised},       {"@selected", theme.selected},
        {"@hairline", theme.hairline},   {"@textMuted", theme.textMuted}, {"@textSecondary", theme.textSecondary},
        {"@text", theme.text},           {"@onAccent", theme.onAccent},   {"@accent", theme.accent},
    };
    // @text is a prefix of @textMuted and @textSecondary, so it is replaced after them.
    for (const auto& [name, color] : tokens)
        css.replace(name, Css(color));
    return css;
}

QPalette MakePalette(const ThemeTokens& theme)
{
    QPalette palette;
    palette.setColor(QPalette::Window, theme.surface);
    palette.setColor(QPalette::WindowText, theme.text);
    palette.setColor(QPalette::Base, theme.raised);
    palette.setColor(QPalette::AlternateBase, theme.surface);
    palette.setColor(QPalette::Text, theme.text);
    palette.setColor(QPalette::PlaceholderText, theme.textMuted);
    palette.setColor(QPalette::Button, theme.raised);
    palette.setColor(QPalette::ButtonText, theme.text);
    palette.setColor(QPalette::Highlight, theme.selected);
    palette.setColor(QPalette::HighlightedText, theme.text);
    palette.setColor(QPalette::ToolTipBase, theme.surface);
    palette.setColor(QPalette::ToolTipText, theme.text);
    palette.setColor(QPalette::Link, theme.accent);
    palette.setColor(QPalette::Mid, theme.hairline);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, theme.textMuted);
    return palette;
}

} // namespace

Theme& Theme::Get()
{
    static Theme theme;
    return theme;
}

Theme::Theme()
{
    QSettings settings;
    appearance_.fontFamily = settings.value("appearance/fontFamily").toString();
    appearance_.canvas = settings.value("appearance/canvas").value<QColor>();
    appearance_.surface = settings.value("appearance/surface").value<QColor>();
    appearance_.text = settings.value("appearance/text").value<QColor>();
    appearance_.accent = settings.value("appearance/accent").value<QColor>();
    appearance_.link = settings.value("appearance/link").value<QColor>();
    appearance_.linkStyle = LinkStyle(settings.value("appearance/linkStyle", int(LinkStyle::Curved)).toInt());
    appearance_.linkWidth = settings.value("appearance/linkWidth", 1.5).toDouble();
}

QFont Theme::Font(int pixelSize, bool medium)
{
    QFont font = QApplication::font();
    font.setPixelSize(pixelSize);
    font.setWeight(medium ? QFont::Medium : QFont::Normal);
    return font;
}

QColor Theme::KindColor(NeuronKind kind) const
{
    // Toned down so a canvas full of dots still reads as calm.
    const bool dark = tokens_.dark;
    switch (kind)
    {
    case NeuronKind::Note: return dark ? QColor("#a3a3aa") : QColor("#86868c");
    case NeuronKind::Task: return dark ? QColor("#d8b552") : QColor("#b48a1c");
    case NeuronKind::Link: return dark ? QColor("#5fb3ba") : QColor("#2f8a92");
    case NeuronKind::Image: return dark ? QColor("#e09a5c") : QColor("#c0763a");
    case NeuronKind::Document: return dark ? QColor("#7ca2f0") : QColor("#2f5fc4");
    case NeuronKind::Sheet: return dark ? QColor("#62b585") : QColor("#2f8a57");
    case NeuronKind::Pdf: return dark ? QColor("#e0786e") : QColor("#b4483f");
    case NeuronKind::File: return dark ? QColor("#8c97a3") : QColor("#66727e");
    case NeuronKind::Folder: return dark ? QColor("#93a3c4") : QColor("#4b5b7c");
    }
    return tokens_.textMuted;
}

QColor Theme::StatusColor(TaskStatus status) const
{
    switch (status)
    {
    case TaskStatus::Todo: return tokens_.textMuted;
    case TaskStatus::Doing: return KindColor(NeuronKind::Task);
    case TaskStatus::Done: return KindColor(NeuronKind::Sheet);
    }
    return tokens_.textMuted;
}

void Theme::Apply(QApplication& app, ThemeMode mode)
{
    mode_ = mode;
    QSettings().setValue("theme", int(mode));
    Rebuild(app);
}

void Theme::Toggle(QApplication& app)
{
    Apply(app, tokens_.dark ? ThemeMode::Light : ThemeMode::Dark);
}

void Theme::ApplyAppearance(QApplication& app, const AppearanceSettings& appearance)
{
    appearance_ = appearance;
    QSettings settings;
    settings.setValue("appearance/fontFamily", appearance.fontFamily);
    settings.setValue("appearance/canvas", appearance.canvas);
    settings.setValue("appearance/surface", appearance.surface);
    settings.setValue("appearance/text", appearance.text);
    settings.setValue("appearance/accent", appearance.accent);
    settings.setValue("appearance/link", appearance.link);
    settings.setValue("appearance/linkStyle", int(appearance.linkStyle));
    settings.setValue("appearance/linkWidth", appearance.linkWidth);
    Rebuild(app);
}

void Theme::ResetAppearance(QApplication& app)
{
    QSettings settings;
    settings.remove("appearance");
    appearance_ = {};
    Rebuild(app);
}

void Theme::Rebuild(QApplication& app)
{
    bool dark = mode_ == ThemeMode::Dark;
    if (mode_ == ThemeMode::System)
        dark = app.styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    tokens_ = dark ? DarkTokens() : LightTokens();
    if (appearance_.canvas.isValid())
        tokens_.canvas = appearance_.canvas;
    if (appearance_.surface.isValid())
        tokens_.surface = appearance_.surface;
    if (appearance_.text.isValid())
        tokens_.text = appearance_.text;
    if (appearance_.accent.isValid())
        tokens_.accent = appearance_.accent;
    if (appearance_.link.isValid())
        tokens_.link = appearance_.link;

    // Fusion draws the same on every OS, so the palette is the whole look.
    static bool styled = false;
    if (!styled)
    {
        app.setStyle(QStyleFactory::create("Fusion"));
        connect(app.styleHints(), &QStyleHints::colorSchemeChanged, this, [this, &app]
        {
            if (mode_ == ThemeMode::System)
                Rebuild(app);
        });
        styled = true;
    }
    static const QFont defaultFont = app.font();
    QFont font = defaultFont;
    if (!appearance_.fontFamily.isEmpty())
        font.setFamily(appearance_.fontFamily);
    font.setPixelSize(kBody);
    app.setFont(font);
    app.setPalette(MakePalette(tokens_));
    app.setStyleSheet(StyleSheet(tokens_));
    emit Changed();
}

} // namespace Ogt
