#include "FindBar.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
    , m_input(new QLineEdit)
    , m_case(new QCheckBox(tr("Match case")))
    , m_count(new QLabel)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 3, 6, 3);

    auto *close = new QToolButton;
    close->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    close->setAutoRaise(true);
    close->setToolTip(tr("Close (Esc)"));

    auto *prev = new QToolButton;
    prev->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    prev->setAutoRaise(true);
    prev->setToolTip(tr("Previous match (Shift+Enter)"));
    auto *next = new QToolButton;
    next->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    next->setAutoRaise(true);
    next->setToolTip(tr("Next match (Enter)"));

    m_input->setPlaceholderText(tr("Find in document"));
    m_input->setClearButtonEnabled(true);
    m_input->installEventFilter(this);
    m_count->setMinimumWidth(80);

    layout->addWidget(close);
    layout->addWidget(m_input, 1);
    layout->addWidget(prev);
    layout->addWidget(next);
    layout->addWidget(m_case);
    layout->addWidget(m_count);

    connect(close, &QToolButton::clicked, this, [this] {
        hide();
        emit closed();
    });
    connect(prev, &QToolButton::clicked, this, [this] { emit findRequested(text(), true, caseSensitive()); });
    connect(next, &QToolButton::clicked, this, [this] { emit findRequested(text(), false, caseSensitive()); });
    connect(m_input, &QLineEdit::textChanged, this, [this] { emit findRequested(text(), false, caseSensitive()); });
    connect(m_case, &QCheckBox::toggled, this, [this] { emit findRequested(text(), false, caseSensitive()); });
}

void FindBar::activate(const QString &initial)
{
    show();
    if (!initial.isEmpty())
        m_input->setText(initial);
    m_input->setFocus();
    m_input->selectAll();
}

QString FindBar::text() const
{
    return m_input->text();
}

bool FindBar::caseSensitive() const
{
    return m_case->isChecked();
}

void FindBar::setResult(int active, int total)
{
    if (text().isEmpty())
        m_count->clear();
    else if (total == 0)
        m_count->setText(tr("No matches"));
    else
        m_count->setText(tr("%1 of %2").arg(active).arg(total));
}

bool FindBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            emit findRequested(text(), key->modifiers() & Qt::ShiftModifier, caseSensitive());
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            hide();
            emit closed();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
