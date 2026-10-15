#include "Exporter.h"

#include "Bridge.h"
#include "DocumentTab.h"
#include "WebPage.h"

#include <QFile>
#include <QPageLayout>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QSaveFile>
#include <QWebEngineView>

Exporter::Exporter(QObject *parent)
    : QObject(parent)
{
}

Exporter::Done Exporter::finishing(const Done &done)
{
    m_busy = true;
    auto fired = std::make_shared<bool>(false);
    return [this, done, fired](bool ok, const QString &error) {
        if (*fired)
            return;
        *fired = true;
        m_busy = false;
        done(ok, error);
    };
}

void Exporter::withLightTheme(DocumentTab *tab,
                              const std::function<void(const std::function<void()> &restore)> &job)
{
    const QString previous = tab->theme();
    QPointer<DocumentTab> guard(tab);
    auto restore = [guard, previous] {
        if (guard && !previous.isEmpty())
            guard->setTheme(previous);
    };
    if (previous == u"light" || previous.isEmpty()) {
        job(restore);
        return;
    }
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(tab, &DocumentTab::rendered, this, [connection, job, restore] {
        QObject::disconnect(*connection);
        job(restore);
    });
    tab->setTheme(QStringLiteral("light"));
}

void Exporter::exportPdf(DocumentTab *tab, const QString &path, const QPageSize &pageSize,
                         const QMarginsF &marginsMm, const Done &doneCallback)
{
    if (m_busy) {
        doneCallback(false, tr("Another export is still running"));
        return;
    }
    const Done done = finishing(doneCallback);
    connect(tab, &QObject::destroyed, this, [done] { done(false, tr("Tab was closed")); });
    tab->ensurePage();
    QPointer<DocumentTab> guard(tab);
    withLightTheme(tab, [this, guard, path, pageSize, marginsMm, done](const std::function<void()> &restore) {
        if (!guard) {
            done(false, tr("Tab was closed"));
            return;
        }
        WebPage *page = guard->page();
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(page, &QWebEnginePage::pdfPrintingFinished, this,
                              [connection, restore, done](const QString &, bool ok) {
            QObject::disconnect(*connection);
            restore();
            done(ok, ok ? QString() : tr("PDF generation failed"));
        });
        page->printToPdf(path, QPageLayout(pageSize, QPageLayout::Portrait, marginsMm, QPageLayout::Millimeter));
    });
}

void Exporter::exportHtml(DocumentTab *tab, const QString &path, bool inlineImages, const Done &doneCallback)
{
    if (m_busy) {
        doneCallback(false, tr("Another export is still running"));
        return;
    }
    const Done done = finishing(doneCallback);
    connect(tab, &QObject::destroyed, this, [done] { done(false, tr("Tab was closed")); });
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(tab->bridge(), &Bridge::htmlExported, this,
                          [connection, path, done](const QString &html, const QString &error) {
        QObject::disconnect(*connection);
        if (!error.isEmpty()) {
            done(false, error);
            return;
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            done(false, file.errorString());
            return;
        }
        file.write(html.toUtf8());
        if (!file.commit()) {
            done(false, file.errorString());
            return;
        }
        done(true, QString());
    });
    tab->evaluate(QStringLiteral("window.omnimark.exportHtml(%1)").arg(inlineImages ? u"true" : u"false"),
                  [](const QVariant &) {});
}

void Exporter::print(DocumentTab *tab, QWidget *dialogParent)
{
    if (m_busy)
        return;
    auto *printer = new QPrinter(QPrinter::HighResolution);
    QPrintDialog dialog(printer, dialogParent);
    if (dialog.exec() != QDialog::Accepted) {
        delete printer;
        return;
    }
    tab->ensurePage();
    QPointer<DocumentTab> guard(tab);
    withLightTheme(tab, [guard, printer](const std::function<void()> &restore) {
        if (!guard) {
            delete printer;
            return;
        }
        QWebEngineView *view = guard->view();
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(view, &QWebEngineView::printFinished, view,
                                       [connection, printer, restore](bool) {
            QObject::disconnect(*connection);
            delete printer;
            restore();
        });
        view->print(printer);
    });
}
