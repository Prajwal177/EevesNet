#pragma once

#include <QMainWindow>
#include <QByteArray>
#include <QList>
#include <QPair>
#include <QPoint>
#include <QStringList>
#include <QThread>

class CaptureWorker;
class QComboBox;
class QEvent;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QPlainTextEdit;
class QTextBrowser;

class MainWindow final : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow() override;

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void changeEvent(QEvent* event) override;

private slots:
	void populateInterfaces(const QStringList& deviceNames, const QStringList& interfaceLabels);
	void startCapture();
	void stopCapture();
	void appendPackets(const QList<QPair<QString, QByteArray>>& packets);
	void showPacketDetails(QListWidgetItem* item);
	void captureFinished();

private:
	void setCaptureControlsEnabled(bool enabled);
	void toggleMaximize();

	CaptureWorker* worker_ = nullptr;
	QThread workerThread_;
	QWidget* titleBar_ = nullptr;
	QLabel* titleLabel_ = nullptr;
	QPushButton* maximizeButton_ = nullptr;
	QComboBox* interfaceSelector_ = nullptr;
	QPushButton* startButton_ = nullptr;
	QPushButton* stopButton_ = nullptr;
	QLabel* statusLabel_ = nullptr;
	QLabel* packetCountLabel_ = nullptr;
	QListWidget* packetList_ = nullptr;
	QTextBrowser* packetDetails_ = nullptr;
	QPlainTextEdit* packetPayload_ = nullptr;
	QPoint dragOffset_;
	bool dragging_ = false;
	quint64 packetCount_ = 0;
};
