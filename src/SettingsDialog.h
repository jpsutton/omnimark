#pragma once

#include <QDialog>

class Settings;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(Settings *settings, QWidget *parent = nullptr);

signals:
    void editCustomCssRequested();
};
