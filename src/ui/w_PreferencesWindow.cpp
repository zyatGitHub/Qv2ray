﻿#include "w_PreferencesWindow.hpp"
#include <QFileDialog>
#include <QColorDialog>
#include <QStyleFactory>
#include <QStyle>
#include <QDesktopServices>

#include "QvUtils.hpp"
#include "QvKernelInteractions.hpp"
#include "QvNetSpeedPlugin.hpp"
#include "QvCoreConfigOperations.hpp"

#include "QvHTTPRequestHelper.hpp"
#include "QvLaunchAtLoginConfigurator.hpp"

#define LOADINGCHECK if(!finishedLoading) return;
#define NEEDRESTART if(finishedLoading) IsConnectionPropertyChanged = true;

PreferencesWindow::PreferencesWindow(QWidget *parent) : QDialog(parent),
    CurrentConfig()
{
    setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    // We add locales
    languageComboBox->clear();
    QDirIterator it(":/translations");

    while (it.hasNext()) {
        languageComboBox->addItem(it.next().split("/").last().split(".").first());
    }

    // Set auto start button state
    SetAutoStartButtonsState(GetLaunchAtLoginStatus());
    //
    nsBarContentCombo->addItems(NetSpeedPluginMessages.values());
    themeCombo->addItems(QStyleFactory::keys());
    //
    qvVersion->setText(QV2RAY_VERSION_STRING);
    qvBuildTime->setText(__DATE__ " " __TIME__);
    CurrentConfig = GetGlobalConfig();
    //
    themeCombo->setCurrentText(CurrentConfig.uiConfig.theme);
    darkThemeCB->setChecked(CurrentConfig.uiConfig.useDarkTheme);
    darkTrayCB->setChecked(CurrentConfig.uiConfig.useDarkTrayIcon);
#ifdef QV2RAY_USE_BUILTIN_DARKTHEME
    // If we use built in theme, it should always be fusion.
    themeCombo->setEnabled(!CurrentConfig.uiConfig.useDarkTheme);
    darkThemeLabel->setText(tr("Use Darkmode Theme"));
#endif
    languageComboBox->setCurrentText(CurrentConfig.uiConfig.language);
    logLevelComboBox->setCurrentIndex(CurrentConfig.logLevel);
    tProxyCheckBox->setChecked(CurrentConfig.tProxySupport);
    //
    //
    listenIPTxt->setText(CurrentConfig.inboundConfig.listenip);
    bool pacEnabled = CurrentConfig.inboundConfig.pacConfig.enablePAC;
    enablePACCB->setChecked(pacEnabled);
    setSysProxyCB->setChecked(CurrentConfig.inboundConfig.setSystemProxy);
    //
    // PAC
    pacGroupBox->setEnabled(pacEnabled);
    pacPortSB->setValue(CurrentConfig.inboundConfig.pacConfig.port);
    pacProxyTxt->setText(CurrentConfig.inboundConfig.pacConfig.localIP);
    pacProxyCB->setCurrentIndex(CurrentConfig.inboundConfig.pacConfig.useSocksProxy ? 1 : 0);
    //
    bool have_http = CurrentConfig.inboundConfig.useHTTP;
    httpCB->setChecked(have_http);
    httpPortLE->setValue(CurrentConfig.inboundConfig.http_port);
    httpAuthCB->setChecked(CurrentConfig.inboundConfig.http_useAuth);
    //
    httpAuthCB->setChecked(CurrentConfig.inboundConfig.http_useAuth);
    httpAuthUsernameTxt->setEnabled(CurrentConfig.inboundConfig.http_useAuth);
    httpAuthPasswordTxt->setEnabled(CurrentConfig.inboundConfig.http_useAuth);
    httpAuthUsernameTxt->setText(CurrentConfig.inboundConfig.httpAccount.user);
    httpAuthPasswordTxt->setText(CurrentConfig.inboundConfig.httpAccount.pass);
    httpGroupBox->setEnabled(have_http);
    //
    //
    bool have_socks = CurrentConfig.inboundConfig.useSocks;
    socksCB->setChecked(have_socks);
    socksPortLE->setValue(CurrentConfig.inboundConfig.socks_port);
    //
    socksAuthCB->setChecked(CurrentConfig.inboundConfig.socks_useAuth);
    socksAuthUsernameTxt->setEnabled(CurrentConfig.inboundConfig.socks_useAuth);
    socksAuthPasswordTxt->setEnabled(CurrentConfig.inboundConfig.socks_useAuth);
    socksAuthUsernameTxt->setText(CurrentConfig.inboundConfig.socksAccount.user);
    socksAuthPasswordTxt->setText(CurrentConfig.inboundConfig.socksAccount.pass);
    // Socks UDP Options
    socksUDPCB->setChecked(CurrentConfig.inboundConfig.socksUDP);
    socksUDPIP->setEnabled(CurrentConfig.inboundConfig.socksUDP);
    socksUDPIP->setText(CurrentConfig.inboundConfig.socksLocalIP);
    socksGroupBox->setEnabled(have_socks);
    //
    //
    vCorePathTxt->setText(CurrentConfig.v2CorePath);
    vCoreAssetsPathTxt->setText(CurrentConfig.v2AssetsPath);
    statsPortBox->setValue(CurrentConfig.connectionConfig.statsPort);
    //
    //
    bypassCNCb->setChecked(CurrentConfig.connectionConfig.bypassCN);
    proxyDefaultCb->setChecked(CurrentConfig.connectionConfig.enableProxy);
    //
    localDNSCb->setChecked(CurrentConfig.connectionConfig.withLocalDNS);
    //
    DNSListTxt->clear();

    foreach (auto dnsStr, CurrentConfig.connectionConfig.dnsList) {
        auto str = dnsStr.trimmed();

        if (!str.isEmpty()) {
            DNSListTxt->appendPlainText(str);
        }
    }

    //
    cancelIgnoreVersionBtn->setEnabled(CurrentConfig.ignoredVersion != "");
    ignoredNextVersion->setText(CurrentConfig.ignoredVersion);

    for (auto i = 0; i < CurrentConfig.toolBarConfig.Pages.size(); i++) {
        nsBarPagesList->addItem(tr("Page") + QSTRN(i + 1) + ": " + QSTRN(CurrentConfig.toolBarConfig.Pages[i].Lines.size()) + " " + tr("Item(s)"));
    }

    if (CurrentConfig.toolBarConfig.Pages.size() > 0) {
        nsBarPagesList->setCurrentRow(0);
        on_nsBarPagesList_currentRowChanged(0);
    } else {
        nsBarVerticalLayout->setEnabled(false);
        nsBarLinesList->setEnabled(false);
        nsBarLineDelBTN->setEnabled(false);
        nsBarLineAddBTN->setEnabled(false);
        nsBarPageYOffset->setEnabled(false);
    }

    CurrentBarPageId = 0;
    //
    // Empty for global config.
    auto autoSub = CurrentConfig.autoStartConfig.subscriptionName;
    auto autoCon = CurrentConfig.autoStartConfig.connectionName;
    autoStartConnCombo->addItem("");

    for (auto item : CurrentConfig.subscriptions.keys()) {
        autoStartSubsCombo->addItem(item);
    }

    autoStartSubsCombo->setCurrentText(autoSub);

    if (CurrentConfig.autoStartConfig.subscriptionName.isEmpty()) {
        autoStartConnCombo->addItems(CurrentConfig.configs);
    } else {
        auto list = GetSubscriptionConnection(autoSub);
        autoStartConnCombo->addItems(list.keys());
    }

    autoStartConnCombo->setCurrentText(autoCon);
    finishedLoading = true;
}

PreferencesWindow::~PreferencesWindow()
{
    //
}

void PreferencesWindow::on_buttonBox_accepted()
{
    int sp = socksPortLE->text().toInt();
    int hp = httpPortLE->text().toInt() ;

    if (!(sp == 0 || hp == 0) && sp == hp) {
        QvMessageBox(this, tr("Preferences"), tr("Port numbers cannot be the same"));
        return;
    }

    SetGlobalConfig(CurrentConfig);
    emit s_reload_config(IsConnectionPropertyChanged);
}

void PreferencesWindow::on_httpCB_stateChanged(int checked)
{
    NEEDRESTART
    bool enabled = checked == Qt::Checked;
    httpGroupBox->setEnabled(enabled);
    CurrentConfig.inboundConfig.useHTTP = enabled;
}

void PreferencesWindow::on_socksCB_stateChanged(int checked)
{
    NEEDRESTART
    bool enabled = checked == Qt::Checked;
    socksGroupBox->setEnabled(enabled);
    CurrentConfig.inboundConfig.useSocks = enabled;
}

void PreferencesWindow::on_httpAuthCB_stateChanged(int checked)
{
    NEEDRESTART
    bool enabled = checked == Qt::Checked;
    httpAuthUsernameTxt->setEnabled(enabled);
    httpAuthPasswordTxt->setEnabled(enabled);
    CurrentConfig.inboundConfig.http_useAuth = enabled;
}

void PreferencesWindow::on_socksAuthCB_stateChanged(int checked)
{
    NEEDRESTART
    bool enabled = checked == Qt::Checked;
    socksAuthUsernameTxt->setEnabled(enabled);
    socksAuthPasswordTxt->setEnabled(enabled);
    CurrentConfig.inboundConfig.socks_useAuth = enabled;
}

void PreferencesWindow::on_languageComboBox_currentTextChanged(const QString &arg1)
{
    LOADINGCHECK
    //
    // A strange bug prevents us to change the UI language online
    //    https://github.com/lhy0403/Qv2ray/issues/34
    //
    CurrentConfig.uiConfig.language = arg1;
    //
    //
    //if (QApplication::installTranslator(getTranslator(arg1))) {
    //    LOG(MODULE_UI, "Loaded translations " + arg1)
    //    retranslateUi(this);
    //} else {
    //    QvMessageBox(this, tr("#Preferences"), tr("#SwitchTranslationError"));
    //}
    //
    //emit retranslateUi(this);
}

void PreferencesWindow::on_logLevelComboBox_currentIndexChanged(int index)
{
    NEEDRESTART
    CurrentConfig.logLevel = index;
}

void PreferencesWindow::on_vCoreAssetsPathTxt_textEdited(const QString &arg1)
{
    NEEDRESTART
    CurrentConfig.v2AssetsPath = arg1;
}

void PreferencesWindow::on_listenIPTxt_textEdited(const QString &arg1)
{
    NEEDRESTART
    CurrentConfig.inboundConfig.listenip = arg1;
    //pacAccessPathTxt->setText("http://" + arg1 + ":" + QSTRN(pacPortSB->value()) + "/pac");
}

void PreferencesWindow::on_httpAuthUsernameTxt_textEdited(const QString &arg1)
{
    NEEDRESTART
    CurrentConfig.inboundConfig.httpAccount.user = arg1;
}

void PreferencesWindow::on_httpAuthPasswordTxt_textEdited(const QString &arg1)
{
    NEEDRESTART
    CurrentConfig.inboundConfig.httpAccount.pass = arg1;
}

void PreferencesWindow::on_socksAuthUsernameTxt_textEdited(const QString &arg1)
{
    NEEDRESTART
    CurrentConfig.inboundConfig.socksAccount.user = arg1;
}

void PreferencesWindow::on_socksAuthPasswordTxt_textEdited(const QString &arg1)
{
    NEEDRESTART
    CurrentConfig.inboundConfig.socksAccount.pass = arg1;
}

void PreferencesWindow::on_proxyDefaultCb_stateChanged(int arg1)
{
    NEEDRESTART
    CurrentConfig.connectionConfig.enableProxy = arg1 == Qt::Checked;
}

void PreferencesWindow::on_localDNSCb_stateChanged(int arg1)
{
    NEEDRESTART
    CurrentConfig.connectionConfig.withLocalDNS = arg1 == Qt::Checked;
}

void PreferencesWindow::on_selectVAssetBtn_clicked()
{
    NEEDRESTART
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open v2ray assets folder"), QDir::currentPath());

    if (!dir.isEmpty()) {
        vCoreAssetsPathTxt->setText(dir);
        on_vCoreAssetsPathTxt_textEdited(dir);
    }
}

void PreferencesWindow::on_selectVCoreBtn_clicked()
{
    QString core = QFileDialog::getOpenFileName(this, tr("Open v2ray core file"), QDir::currentPath());

    if (!core.isEmpty()) {
        vCorePathTxt->setText(core);
        on_vCorePathTxt_textEdited(core);
    }
}

void PreferencesWindow::on_vCorePathTxt_textEdited(const QString &arg1)
{
    NEEDRESTART
    CurrentConfig.v2CorePath = arg1;
}

void PreferencesWindow::on_DNSListTxt_textChanged()
{
    if (finishedLoading) {
        try {
            QStringList hosts = DNSListTxt->toPlainText().replace("\r", "").split("\n");
            CurrentConfig.connectionConfig.dnsList.clear();

            foreach (auto host, hosts) {
                if (host != "" && host != "\r") {
                    // Not empty, so we save.
                    CurrentConfig.connectionConfig.dnsList.push_back(host);
                    NEEDRESTART
                }
            }

            BLACK(DNSListTxt)
        } catch (...) {
            RED(DNSListTxt)
        }
    }
}

void PreferencesWindow::on_aboutQt_clicked()
{
    QApplication::aboutQt();
}

void PreferencesWindow::on_cancelIgnoreVersionBtn_clicked()
{
    CurrentConfig.ignoredVersion.clear();
    cancelIgnoreVersionBtn->setEnabled(false);
}

void PreferencesWindow::on_tProxyCheckBox_stateChanged(int arg1)
{
#ifdef __linux

    if (finishedLoading) {
        // Set UID and GID for linux
        // Steps:
        // --> 1. Copy v2ray core files to the #CONFIG_DIR#/vcore/ dir.
        // --> 2. Change GlobalConfig.v2CorePath.
        // --> 3. Call `pkexec setcap CAP_NET_ADMIN,CAP_NET_RAW,CAP_NET_BIND_SERVICE=eip` on the v2ray core.
        if (arg1 == Qt::Checked) {
            // We enable it!
            if (QvMessageBoxAsk(this, tr("Enable tProxy Support"),
                                tr("This will append capabilities to the v2ray executable.")  + NEWLINE + NEWLINE +
                                tr("Qv2ray will copy your v2ray core to this path: ") + NEWLINE + QV2RAY_DEFAULT_VCORE_PATH + NEWLINE + NEWLINE +
                                tr("If anything goes wrong after enabling this, please refer to issue #57 or the link below:") + NEWLINE +
                                " https://lhy0403.github.io/Qv2ray/zh-CN/FAQ.html ") != QMessageBox::Yes) {
                tProxyCheckBox->setChecked(false);
                LOG(MODULE_UI, "Canceled enabling tProxy feature.")
            } else {
                LOG(MODULE_VCORE, "ENABLING tProxy Support")
                LOG(MODULE_FILE, " --> Origin v2ray core file is at: " + CurrentConfig.v2CorePath)
                auto v2ctlPath = QFileInfo(CurrentConfig.v2CorePath).path() + "/v2ctl";
                auto newPath = QFileInfo(QV2RAY_DEFAULT_VCORE_PATH).path();
                //
                LOG(MODULE_FILE, " --> Origin v2ctl file is at: " + v2ctlPath)
                LOG(MODULE_FILE, " --> New v2ray files will be placed in: " + newPath)
                //
                LOG(MODULE_FILE, " --> Copying files....")

                if (QFileInfo(CurrentConfig.v2CorePath).absoluteFilePath() !=  QFileInfo(QV2RAY_DEFAULT_VCORE_PATH).absoluteFilePath()) {
                    // Only trying to remove file when they are not in the default dir.
                    // (In other words...) Keep using the current files. <Because we don't know where else we can copy the file from...>
                    if (QFile(QV2RAY_DEFAULT_VCORE_PATH).exists()) {
                        LOG(MODULE_FILE, QString(QV2RAY_DEFAULT_VCORE_PATH) + ": File already exists.")
                        LOG(MODULE_FILE, QString(QV2RAY_DEFAULT_VCORE_PATH) + ": Deleting file.")
                        QFile(QV2RAY_DEFAULT_VCORE_PATH).remove();
                    }

                    if (QFile(newPath + "/v2ctl").exists()) {
                        LOG(MODULE_FILE, newPath + "/v2ctl : File already exists.")
                        LOG(MODULE_FILE, newPath + "/v2ctl : Deleting file.")
                        QFile(newPath + "/v2ctl").remove();
                    }

                    QString vCoreresult = QFile(CurrentConfig.v2CorePath).copy(QV2RAY_DEFAULT_VCORE_PATH) ? "OK" : "FAILED";
                    LOG(MODULE_FILE, " --> v2ray Core: " + vCoreresult)
                    //
                    QString vCtlresult = QFile(v2ctlPath).copy(newPath + "/v2ctl") ? "OK" : "FAILED";
                    LOG(MODULE_FILE, " --> v2ray Ctl: " + vCtlresult)
                    //

                    if (vCoreresult == "OK" && vCtlresult == "OK") {
                        LOG(MODULE_VCORE, " --> Done copying files.")
                        on_vCorePathTxt_textEdited(QV2RAY_DEFAULT_VCORE_PATH);
                    } else {
                        LOG(MODULE_VCORE, "FAILED to copy v2ray files. Aborting.")
                        QvMessageBox(this, tr("Enable tProxy Support"),
                                     tr("Qv2ray cannot copy one or both v2ray files from: ") + NEWLINE + NEWLINE +
                                     CurrentConfig.v2CorePath + NEWLINE + v2ctlPath + NEWLINE + NEWLINE +
                                     tr("to this path: ") + NEWLINE + newPath);
                        return;
                    }
                } else {
                    LOG(MODULE_VCORE, "Skipped removing files since the current v2ray core is in the default path.")
                    LOG(MODULE_VCORE, " --> Actually because we don't know where else to obtain the files.")
                }

                LOG(MODULE_UI, "Calling pkexec and setcap...")
                int ret = QProcess::execute("pkexec setcap CAP_NET_ADMIN,CAP_NET_RAW,CAP_NET_BIND_SERVICE=eip " + CurrentConfig.v2CorePath);

                if (ret != 0) {
                    LOG(MODULE_UI, "WARN: setcap exits with code: " + QSTRN(ret))
                    QvMessageBox(this, tr("Preferences"), tr("Failed to setcap onto v2ray executable. You may need to run `setcap` manually."));
                }

                CurrentConfig.tProxySupport = true;
                NEEDRESTART
            }
        } else {
            int ret = QProcess::execute("pkexec setcap -r " + CurrentConfig.v2CorePath);

            if (ret != 0) {
                LOG(MODULE_UI, "WARN: setcap exits with code: " + QSTRN(ret))
                QvMessageBox(this, tr("Preferences"), tr("Failed to setcap onto v2ray executable. You may need to run `setcap` manually."));
            }

            CurrentConfig.tProxySupport = false;
            NEEDRESTART
        }
    }

#else
    Q_UNUSED(arg1)
    // No such tProxy thing on Windows and macOS
    QvMessageBox(this, tr("Preferences"), tr("tProxy is not supported on macOS and Windows"));
    CurrentConfig.tProxySupport = false;
    tProxyCheckBox->setChecked(false);
#endif
}

void PreferencesWindow::on_bypassCNCb_stateChanged(int arg1)
{
    NEEDRESTART
    CurrentConfig.connectionConfig.bypassCN = arg1 == Qt::Checked;
}

void PreferencesWindow::on_statsPortBox_valueChanged(int arg1)
{
    NEEDRESTART
    CurrentConfig.connectionConfig.statsPort = arg1;
}

void PreferencesWindow::on_socksPortLE_valueChanged(int arg1)
{
    NEEDRESTART
    CurrentConfig.inboundConfig.socks_port = arg1;
}

void PreferencesWindow::on_httpPortLE_valueChanged(int arg1)
{
    NEEDRESTART
    CurrentConfig.inboundConfig.http_port = arg1;
}

void PreferencesWindow::on_socksUDPCB_stateChanged(int arg1)
{
    NEEDRESTART
    CurrentConfig.inboundConfig.socksUDP = arg1 == Qt::Checked;
    socksUDPIP->setEnabled(arg1 == Qt::Checked);
}

void PreferencesWindow::on_socksUDPIP_textEdited(const QString &arg1)
{
    NEEDRESTART
    CurrentConfig.inboundConfig.socksLocalIP = arg1;
}

// ------------------- NET SPEED PLUGIN OPERATIONS -----------------------------------------------------------------

#define CurrentBarPage CurrentConfig.toolBarConfig.Pages[this->CurrentBarPageId]
#define CurrentBarLine CurrentBarPage.Lines[this->CurrentBarLineId]
#define SET_LINE_LIST_TEXT nsBarLinesList->currentItem()->setText(GetBarLineDescription(CurrentBarLine));

void PreferencesWindow::on_nsBarPageAddBTN_clicked()
{
    QvBarPage page;
    CurrentConfig.toolBarConfig.Pages.push_back(page);
    CurrentBarPageId = CurrentConfig.toolBarConfig.Pages.size() - 1 ;
    // Add default line.
    QvBarLine line;
    CurrentBarPage.Lines.push_back(line);
    CurrentBarLineId = 0;
    nsBarPagesList->addItem(QSTRN(CurrentBarPageId));
    ShowLineParameters(CurrentBarLine);
    LOG(MODULE_UI, "Adding new page Id: " + QSTRN(CurrentBarPageId))
    nsBarPageDelBTN->setEnabled(true);
    nsBarLineAddBTN->setEnabled(true);
    nsBarLineDelBTN->setEnabled(true);
    nsBarLinesList->setEnabled(true);
    nsBarPageYOffset->setEnabled(true);
    on_nsBarPagesList_currentRowChanged(static_cast<int>(CurrentBarPageId));
    nsBarPagesList->setCurrentRow(static_cast<int>(CurrentBarPageId));
}

void PreferencesWindow::on_nsBarPageDelBTN_clicked()
{
    if (nsBarPagesList->currentRow() >= 0) {
        CurrentConfig.toolBarConfig.Pages.removeAt(nsBarPagesList->currentRow());
        nsBarPagesList->takeItem(nsBarPagesList->currentRow());

        if (nsBarPagesList->count() <= 0) {
            nsBarPageDelBTN->setEnabled(false);
            nsBarLineAddBTN->setEnabled(false);
            nsBarLineDelBTN->setEnabled(false);
            nsBarLinesList->setEnabled(false);
            nsBarVerticalLayout->setEnabled(false);
            nsBarPageYOffset->setEnabled(false);
            nsBarLinesList->clear();
        }
    }
}

void PreferencesWindow::on_nsBarPageYOffset_valueChanged(int arg1)
{
    LOADINGCHECK
    CurrentBarPage.OffsetYpx = arg1;
}

void PreferencesWindow::on_nsBarLineAddBTN_clicked()
{
    // WARNING Is it really just this simple?
    QvBarLine line;
    CurrentBarPage.Lines.push_back(line);
    CurrentBarLineId = CurrentBarPage.Lines.size() - 1;
    nsBarLinesList->addItem(QSTRN(CurrentBarLineId));
    ShowLineParameters(CurrentBarLine);
    nsBarLineDelBTN->setEnabled(true);
    LOG(MODULE_UI, "Adding new line Id: " + QSTRN(CurrentBarLineId))
    nsBarLinesList->setCurrentRow(static_cast<int>(CurrentBarPage.Lines.size() - 1));
}

void PreferencesWindow::on_nsBarLineDelBTN_clicked()
{
    if (nsBarLinesList->currentRow() >= 0) {
        CurrentBarPage.Lines.removeAt(nsBarLinesList->currentRow());
        nsBarLinesList->takeItem(nsBarLinesList->currentRow());
        CurrentBarLineId = 0;

        if (nsBarLinesList->count() <= 0) {
            nsBarVerticalLayout->setEnabled(false);
            nsBarLineDelBTN->setEnabled(false);
        }

        // TODO Disabling some UI;
    }
}

void PreferencesWindow::on_nsBarPagesList_currentRowChanged(int currentRow)
{
    if (currentRow < 0) return;

    // Change page.
    // We reload the lines
    // Set all parameters item to the property of the first line.
    CurrentBarPageId = currentRow;
    CurrentBarLineId = 0;
    nsBarPageYOffset->setValue(CurrentBarPage.OffsetYpx);
    nsBarLinesList->clear();

    if (!CurrentBarPage.Lines.empty()) {
        for (auto line : CurrentBarPage.Lines) {
            auto description = GetBarLineDescription(line);
            nsBarLinesList->addItem(description);
        }

        nsBarLinesList->setCurrentRow(0);
        ShowLineParameters(CurrentBarLine);
    } else {
        nsBarVerticalLayout->setEnabled(false);
    }
}

void PreferencesWindow::on_nsBarLinesList_currentRowChanged(int currentRow)
{
    if (currentRow < 0) return;

    CurrentBarLineId = currentRow;
    ShowLineParameters(CurrentBarLine);
}

void PreferencesWindow::on_fontComboBox_currentFontChanged(const QFont &f)
{
    LOADINGCHECK
    CurrentBarLine.Family = f.family();
    SET_LINE_LIST_TEXT
}

void PreferencesWindow::on_nsBarFontBoldCB_stateChanged(int arg1)
{
    LOADINGCHECK
    CurrentBarLine.Bold = arg1 == Qt::Checked;
    SET_LINE_LIST_TEXT
}

void PreferencesWindow::on_nsBarFontItalicCB_stateChanged(int arg1)
{
    LOADINGCHECK
    CurrentBarLine.Italic = arg1 == Qt::Checked;
    SET_LINE_LIST_TEXT
}

void PreferencesWindow::on_nsBarFontASB_valueChanged(int arg1)
{
    LOADINGCHECK
    CurrentBarLine.ColorA = arg1;
    ShowLineParameters(CurrentBarLine);
    SET_LINE_LIST_TEXT
}

void PreferencesWindow::on_nsBarFontRSB_valueChanged(int arg1)
{
    LOADINGCHECK
    CurrentBarLine.ColorR = arg1;
    ShowLineParameters(CurrentBarLine);
    SET_LINE_LIST_TEXT
}

void PreferencesWindow::on_nsBarFontGSB_valueChanged(int arg1)
{
    LOADINGCHECK
    CurrentBarLine.ColorG = arg1;
    ShowLineParameters(CurrentBarLine);
    SET_LINE_LIST_TEXT
}

void PreferencesWindow::on_nsBarFontBSB_valueChanged(int arg1)
{
    LOADINGCHECK
    CurrentBarLine.ColorB = arg1;
    ShowLineParameters(CurrentBarLine);
    SET_LINE_LIST_TEXT
}

void PreferencesWindow::on_nsBarFontSizeSB_valueChanged(double arg1)
{
    LOADINGCHECK
    CurrentBarLine.Size = arg1;
    SET_LINE_LIST_TEXT
}

QString PreferencesWindow::GetBarLineDescription(QvBarLine barLine)
{
    QString result = "Empty";
    result = NetSpeedPluginMessages[barLine.ContentType];

    if (barLine.ContentType == 0) {
        result +=  " (" + barLine.Message + ")";
    }

    result = result.append(barLine.Bold ?  ", " + tr("Bold") : "");
    result = result.append(barLine.Italic ? ", " + tr("Italic") : "");
    return result;
}

void PreferencesWindow::ShowLineParameters(QvBarLine &barLine)
{
    finishedLoading = false;

    if (!barLine.Family.isEmpty()) {
        fontComboBox->setCurrentFont(QFont(barLine.Family));
    }

    // Colors
    nsBarFontASB->setValue(barLine.ColorA);
    nsBarFontBSB->setValue(barLine.ColorB);
    nsBarFontGSB->setValue(barLine.ColorG);
    nsBarFontRSB->setValue(barLine.ColorR);
    //
    QColor color = QColor::fromRgb(barLine.ColorR, barLine.ColorG, barLine.ColorB, barLine.ColorA);
    QString s(QStringLiteral("background: #")
              + ((color.red() < 16) ? "0" : "") + QString::number(color.red(), 16)
              + ((color.green() < 16) ? "0" : "") + QString::number(color.green(), 16)
              + ((color.blue() < 16) ? "0" : "") + QString::number(color.blue(), 16) + ";");
    chooseColorBtn->setStyleSheet(s);
    nsBarFontSizeSB->setValue(barLine.Size);
    nsBarFontBoldCB->setChecked(barLine.Bold);
    nsBarFontItalicCB->setChecked(barLine.Italic);
    nsBarContentCombo->setCurrentText(NetSpeedPluginMessages[barLine.ContentType]);
    nsBarTagTxt->setText(barLine.Message);
    finishedLoading = true;
    nsBarVerticalLayout->setEnabled(true);
}

void PreferencesWindow::on_chooseColorBtn_clicked()
{
    LOADINGCHECK
    QColorDialog d(QColor::fromRgb(CurrentBarLine.ColorR, CurrentBarLine.ColorG, CurrentBarLine.ColorB, CurrentBarLine.ColorA), this);
    d.exec();

    if (d.result() == QDialog::DialogCode::Accepted) {
        d.selectedColor().getRgb(&CurrentBarLine.ColorR, &CurrentBarLine.ColorG, &CurrentBarLine.ColorB, &CurrentBarLine.ColorA);
        ShowLineParameters(CurrentBarLine);
        SET_LINE_LIST_TEXT
    }
}

void PreferencesWindow::on_nsBarTagTxt_textEdited(const QString &arg1)
{
    LOADINGCHECK
    CurrentBarLine.Message = arg1;
    SET_LINE_LIST_TEXT
}

void PreferencesWindow::on_nsBarContentCombo_currentIndexChanged(const QString &arg1)
{
    LOADINGCHECK
    CurrentBarLine.ContentType = NetSpeedPluginMessages.key(arg1);
    SET_LINE_LIST_TEXT
}

void PreferencesWindow::on_applyNSBarSettingsBtn_clicked()
{
    auto conf = GetGlobalConfig();
    conf.toolBarConfig = CurrentConfig.toolBarConfig;
    SetGlobalConfig(conf);
}

void PreferencesWindow::on_themeCombo_currentTextChanged(const QString &arg1)
{
    LOADINGCHECK
    CurrentConfig.uiConfig.theme = arg1;
}

void PreferencesWindow::on_darkThemeCB_stateChanged(int arg1)
{
    LOADINGCHECK
    CurrentConfig.uiConfig.useDarkTheme = arg1 == Qt::Checked;
    QvMessageBox(this, tr("Dark Mode"), tr("Please restart Qv2ray to fully apply this feature."));
#ifdef QV2RAY_USE_BUILTIN_DARKTHEME
    themeCombo->setEnabled(arg1 != Qt::Checked);

    if (arg1 == Qt::Checked) {
        themeCombo->setCurrentIndex(QStyleFactory::keys().indexOf("Fusion"));
        CurrentConfig.uiConfig.theme = "Fusion";
    }

#endif
}

void PreferencesWindow::on_darkTrayCB_stateChanged(int arg1)
{
    LOADINGCHECK
    CurrentConfig.uiConfig.useDarkTrayIcon = arg1 == Qt::Checked;
}

void PreferencesWindow::on_enablePACCB_stateChanged(int arg1)
{
    LOADINGCHECK
    NEEDRESTART
    bool enabled = arg1 == Qt::Checked;
    CurrentConfig.inboundConfig.pacConfig.enablePAC = enabled;
    pacGroupBox->setEnabled(enabled);
}

void PreferencesWindow::on_pacGoBtn_clicked()
{
    LOADINGCHECK
    QString gfwLocation;
    QString fileContent;
    pacGoBtn->setEnabled(false);
    gfwListCB->setEnabled(false);
    auto request = new QvHttpRequestHelper();
    LOG(MODULE_PROXY, "Downloading GFWList file.")

    switch (gfwListCB->currentIndex()) {
        case 0:
            gfwLocation = "https://gitlab.com/gfwlist/gfwlist/raw/master/gfwlist.txt";
            fileContent = QString::fromUtf8(request->syncget(gfwLocation));
            break;

        case 1:
            gfwLocation = "https://pagure.io/gfwlist/raw/master/f/gfwlist.txt";
            fileContent = QString::fromUtf8(request->syncget(gfwLocation));
            break;

        case 2:
            gfwLocation = "http://repo.or.cz/gfwlist.git/blob_plain/HEAD:/gfwlist.txt";
            fileContent = QString::fromUtf8(request->syncget(gfwLocation));
            break;

        case 3:
            gfwLocation = "https://bitbucket.org/gfwlist/gfwlist/raw/HEAD/gfwlist.txt";
            fileContent = QString::fromUtf8(request->syncget(gfwLocation));
            break;

        case 4:
            gfwLocation = "https://raw.githubusercontent.com/gfwlist/gfwlist/master/gfwlist.txt";
            fileContent = QString::fromUtf8(request->syncget(gfwLocation));
            break;

        case 5:
            gfwLocation = "https://git.tuxfamily.org/gfwlist/gfwlist.git/plain/gfwlist.txt";
            fileContent = QString::fromUtf8(request->syncget(gfwLocation));
            break;

        case 6:
            QFileDialog d;
            d.exec();
            auto file = d.getOpenFileUrl(this, tr("Select GFWList in base64")).toString();
            fileContent = StringFromFile(new QFile(file));
            break;
    }

    LOG(MODULE_NETWORK, "Fetched: " + gfwLocation)
    QvMessageBox(this, tr("Download GFWList"), tr("Successfully downloaded GFWList."));
    pacGoBtn->setEnabled(true);
    gfwListCB->setEnabled(true);

    if (!QDir(QV2RAY_RULES_DIR).exists()) {
        QDir(QV2RAY_RULES_DIR).mkpath(QV2RAY_RULES_DIR);
    }

    QFile privateGFWListFile(QV2RAY_RULES_GFWLIST_PATH);
    StringToFile(&fileContent, &privateGFWListFile);
}

void PreferencesWindow::on_pacPortSB_valueChanged(int arg1)
{
    LOADINGCHECK
    NEEDRESTART
    CurrentConfig.inboundConfig.pacConfig.port = arg1;
    //pacAccessPathTxt->setText("http://" + listenIPTxt->text() + ":" + QSTRN(arg1) + "/pac");
}

void PreferencesWindow::on_setSysProxyCB_stateChanged(int arg1)
{
    LOADINGCHECK
    NEEDRESTART
    CurrentConfig.inboundConfig.setSystemProxy = arg1 == Qt::Checked;
}

void PreferencesWindow::on_pacProxyCB_currentIndexChanged(int index)
{
    LOADINGCHECK
    NEEDRESTART
    // 0 -> http
    // 1 -> socks
    CurrentConfig.inboundConfig.pacConfig.useSocksProxy = index == 1;
}

void PreferencesWindow::on_pushButton_clicked()
{
    LOADINGCHECK
    QDesktopServices::openUrl(QUrl::fromUserInput(QV2RAY_RULES_DIR));
}

void PreferencesWindow::on_pacProxyTxt_textEdited(const QString &arg1)
{
    LOADINGCHECK
    NEEDRESTART
    CurrentConfig.inboundConfig.pacConfig.localIP = arg1;
}

void PreferencesWindow::on_autoStartSubsCombo_currentIndexChanged(const QString &arg1)
{
    LOADINGCHECK
    CurrentConfig.autoStartConfig.subscriptionName = arg1;
    autoStartConnCombo->clear();

    if (arg1.isEmpty()) {
        autoStartConnCombo->addItem("");
        autoStartConnCombo->addItems(CurrentConfig.configs);
    } else {
        auto list = GetSubscriptionConnection(arg1);
        autoStartConnCombo->addItems(list.keys());
    }
}

void PreferencesWindow::on_autoStartConnCombo_currentIndexChanged(const QString &arg1)
{
    LOADINGCHECK
    CurrentConfig.autoStartConfig.connectionName = arg1;
}

void PreferencesWindow::on_installBootStart_clicked()
{
    SetLaunchAtLoginStatus(true);

    // If failed to set the status.
    if (!GetLaunchAtLoginStatus()) {
        QvMessageBox(this, tr("Start with boot"), tr("Failed to set auto start option."));
    }

    SetAutoStartButtonsState(GetLaunchAtLoginStatus());
}

void PreferencesWindow::on_removeBootStart_clicked()
{
    SetLaunchAtLoginStatus(false);

    // If that setting still present.
    if (GetLaunchAtLoginStatus()) {
        QvMessageBox(this, tr("Start with boot"), tr("Failed to set auto start option."));
    }

    SetAutoStartButtonsState(GetLaunchAtLoginStatus());
}

void PreferencesWindow::SetAutoStartButtonsState(bool isAutoStart)
{
    installBootStart->setEnabled(!isAutoStart);
    removeBootStart->setEnabled(isAutoStart);
}
