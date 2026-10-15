#pragma once

#include <QMarginsF>
#include <QObject>
#include <QPageSize>

#include <functional>

class DocumentTab;

// Exports the rendered preview of a tab. PDF and printing always use the
// light theme so output looks right on paper.
class Exporter : public QObject
{
    Q_OBJECT
public:
    using Done = std::function<void(bool ok, const QString &error)>;

    explicit Exporter(QObject *parent = nullptr);

    void exportPdf(DocumentTab *tab, const QString &path, const QPageSize &pageSize,
                   const QMarginsF &marginsMm, const Done &done);
    void exportHtml(DocumentTab *tab, const QString &path, bool inlineImages, const Done &done);
    void print(DocumentTab *tab, QWidget *dialogParent);

    bool isBusy() const { return m_busy; }

private:
    // Switches the tab to light, runs `job` after it re-rendered, then restores.
    void withLightTheme(DocumentTab *tab, const std::function<void(const std::function<void()> &restore)> &job);
    // Wraps `done` so the busy flag clears exactly once.
    Done finishing(const Done &done);

    bool m_busy = false;
};
