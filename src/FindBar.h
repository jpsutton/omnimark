#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QToolButton;

// Inline find bar for the preview. Search runs as you type; Enter and
// Shift+Enter step through matches; Escape closes.
class FindBar : public QWidget
{
    Q_OBJECT
public:
    explicit FindBar(QWidget *parent = nullptr);

    void activate(const QString &initial = {});
    QString text() const;
    bool caseSensitive() const;
    void setResult(int active, int total);

signals:
    void findRequested(const QString &text, bool backward, bool caseSensitive);
    void closed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLineEdit *m_input;
    QCheckBox *m_case;
    QLabel *m_count;
};
