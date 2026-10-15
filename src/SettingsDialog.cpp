#include "SettingsDialog.h"

#include "Settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));

    // ---- appearance
    auto *theme = new QComboBox;
    theme->addItem(tr("Follow system"), int(Settings::Theme::System));
    theme->addItem(tr("Light"), int(Settings::Theme::Light));
    theme->addItem(tr("Dark"), int(Settings::Theme::Dark));
    theme->setCurrentIndex(theme->findData(int(settings->theme())));
    connect(theme, &QComboBox::currentIndexChanged, settings, [settings, theme] {
        settings->setTheme(Settings::Theme(theme->currentData().toInt()));
    });

    auto *style = new QComboBox;
    style->addItem(tr("GitHub"), QStringLiteral("github"));
    style->addItem(tr("Serif (reading)"), QStringLiteral("serif"));
    style->addItem(tr("Compact"), QStringLiteral("compact"));
    style->setCurrentIndex(qMax(0, style->findData(settings->previewStyle())));
    connect(style, &QComboBox::currentIndexChanged, settings, [settings, style] {
        settings->setPreviewStyle(style->currentData().toString());
    });

    auto *previewFont = new QSpinBox;
    previewFont->setRange(10, 32);
    previewFont->setSuffix(tr(" px"));
    previewFont->setValue(settings->previewFontSize());
    connect(previewFont, &QSpinBox::valueChanged, settings, &Settings::setPreviewFontSize);

    auto *width = new QSpinBox;
    width->setRange(0, 3000);
    width->setSingleStep(40);
    width->setSuffix(tr(" px"));
    width->setSpecialValueText(tr("Full width"));
    width->setValue(settings->previewWidth());
    connect(width, &QSpinBox::valueChanged, settings, &Settings::setPreviewWidth);

    auto *lineNumbers = new QCheckBox(tr("Show line numbers in code blocks"));
    lineNumbers->setChecked(settings->codeLineNumbers());
    connect(lineNumbers, &QCheckBox::toggled, settings, &Settings::setCodeLineNumbers);

    auto *remote = new QCheckBox(tr("Load remote images"));
    remote->setToolTip(tr("When off, images from the internet are replaced with a link, so opening a "
                          "document never contacts other servers."));
    remote->setChecked(settings->remoteImages());
    connect(remote, &QCheckBox::toggled, settings, &Settings::setRemoteImages);

    auto *plantuml = new QLineEdit(settings->plantumlServer());
    plantuml->setPlaceholderText(QStringLiteral("https://www.plantuml.com/plantuml"));
    plantuml->setClearButtonEnabled(true);
    plantuml->setToolTip(tr("PlantUML diagrams are sent to this server to be drawn. Leave empty to keep "
                            "them local (shown as source)."));
    connect(plantuml, &QLineEdit::editingFinished, settings, [settings, plantuml] {
        settings->setPlantumlServer(plantuml->text().trimmed());
    });

    auto *customCss = new QPushButton(tr("Edit Custom CSS…"));
    connect(customCss, &QPushButton::clicked, this, &SettingsDialog::editCustomCssRequested);
    auto *cssPath = new QLabel(settings->customCssPath());
    cssPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cssPath->setWordWrap(true);
    cssPath->setEnabled(false);

    auto *appearance = new QGroupBox(tr("Preview"));
    auto *appearanceForm = new QFormLayout(appearance);
    appearanceForm->addRow(tr("Theme:"), theme);
    appearanceForm->addRow(tr("Style:"), style);
    appearanceForm->addRow(tr("Font size:"), previewFont);
    appearanceForm->addRow(tr("Max width:"), width);
    appearanceForm->addRow(lineNumbers);
    appearanceForm->addRow(remote);
    appearanceForm->addRow(tr("PlantUML server:"), plantuml);
    appearanceForm->addRow(customCss);
    appearanceForm->addRow(cssPath);

    // ---- editor
    auto *editorFont = new QSpinBox;
    editorFont->setRange(8, 32);
    editorFont->setSuffix(tr(" px"));
    editorFont->setValue(settings->editorFontSize());
    connect(editorFont, &QSpinBox::valueChanged, settings, &Settings::setEditorFontSize);

    auto *defaultMode = new QComboBox;
    defaultMode->addItem(tr("Preview"), QStringLiteral("preview"));
    defaultMode->addItem(tr("Split"), QStringLiteral("split"));
    defaultMode->addItem(tr("Source"), QStringLiteral("source"));
    defaultMode->setCurrentIndex(qMax(0, defaultMode->findData(settings->defaultViewMode())));
    connect(defaultMode, &QComboBox::currentIndexChanged, settings, [settings, defaultMode] {
        settings->setDefaultViewMode(defaultMode->currentData().toString());
    });

    auto *toolbar = new QComboBox;
    toolbar->addItem(tr("Icons only"), QStringLiteral("icons"));
    toolbar->addItem(tr("Text beside icons"), QStringLiteral("beside"));
    toolbar->addItem(tr("Text under icons"), QStringLiteral("under"));
    toolbar->setCurrentIndex(qMax(0, toolbar->findData(settings->toolbarStyle())));
    connect(toolbar, &QComboBox::currentIndexChanged, settings, [settings, toolbar] {
        settings->setToolbarStyle(toolbar->currentData().toString());
    });

    auto *wrap = new QCheckBox(tr("Wrap long lines"));
    wrap->setChecked(settings->wordWrap());
    connect(wrap, &QCheckBox::toggled, settings, &Settings::setWordWrap);

    auto *sync = new QCheckBox(tr("Synchronize scrolling in split view"));
    sync->setChecked(settings->syncScroll());
    connect(sync, &QCheckBox::toggled, settings, &Settings::setSyncScroll);

    auto *vim = new QCheckBox(tr("Vim key bindings"));
    vim->setChecked(settings->vim());
    connect(vim, &QCheckBox::toggled, settings, &Settings::setVim);

    auto *editor = new QGroupBox(tr("Window && Editor"));
    auto *editorForm = new QFormLayout(editor);
    editorForm->addRow(tr("Toolbar:"), toolbar);
    editorForm->addRow(tr("Open files in:"), defaultMode);
    editorForm->addRow(tr("Font size:"), editorFont);
    editorForm->addRow(wrap);
    editorForm->addRow(sync);
    editorForm->addRow(vim);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    plantuml->setMinimumWidth(260);
    for (QFormLayout *form : { appearanceForm, editorForm }) {
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    auto *groups = new QHBoxLayout;
    groups->addWidget(appearance, 3);
    groups->addWidget(editor, 2, Qt::AlignTop);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(groups);
    layout->addWidget(buttons);
}
