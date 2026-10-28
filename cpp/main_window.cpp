#include "main_window.h"

#include "capture_worker.h"

#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QMetaObject>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

#ifndef EEVESNET_HAS_QT_WEBENGINE
#define EEVESNET_HAS_QT_WEBENGINE 0
#endif

#if EEVESNET_HAS_QT_WEBENGINE
#include <QWebEngineView>
#include <QWebEngineSettings>
#include <QUrl>
#endif

namespace
{
#if EEVESNET_HAS_QT_WEBENGINE
class EmbeddedLeafletView final : public QWebEngineView
{
public:
	explicit EmbeddedLeafletView(QWidget* parent = nullptr)
		: QWebEngineView(parent)
	{
		setObjectName(QStringLiteral("worldMap"));
		setMinimumHeight(280);
		settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);
		setHtml(QStringLiteral(R"html(
<!DOCTYPE html>
<html lang="en">
<head>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">
	<style>
		html, body, #map { width: 100%; height: 100%; margin: 0; background: #0b111b; }
		.leaflet-container { background: #0b111b; font-family: sans-serif; }
		.leaflet-control-attribution { font-size: 11px; }
	</style>
</head>
<body>
	<div id="map"></div>
	<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
	<script>
		const map = L.map('map', { worldCopyJump: false, minZoom: 2, maxZoom: 19, zoomControl: true })
			.setView([20, 0], 2);
		const worldBounds = L.latLngBounds([[-85, -180], [85, 180]]);
		map.setMaxBounds(worldBounds);
		L.tileLayer('https://services.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer/tile/{z}/{y}/{x}', {
			attribution: 'Tiles &copy; Esri — Esri, DeLorme, NAVTEQ',
			maxZoom: 19,
			noWrap: true,
			bounds: worldBounds
		}).addTo(map);
		map.fitWorld();
	</script>
</body>
</html>
)html"), QUrl(QStringLiteral("https://eevesnet.local/")));
	}
};
#endif
}

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent),
	  worker_(new CaptureWorker)
{
	setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
	setWindowTitle(QStringLiteral("EevesNet - Capture Proof of Concept"));
	setMinimumSize(720, 460);
	resize(900, 560);

	auto* centralWidget = new QWidget(this);
	centralWidget->setObjectName(QStringLiteral("windowRoot"));
	auto* rootLayout = new QVBoxLayout(centralWidget);
	rootLayout->setContentsMargins(1, 1, 1, 1);
	rootLayout->setSpacing(0);

	titleBar_ = new QWidget(centralWidget);
	titleBar_->setObjectName(QStringLiteral("titleBar"));
	titleBar_->setFixedHeight(48);
	titleBar_->setMouseTracking(true);
	titleBar_->installEventFilter(this);
	auto* titleLayout = new QHBoxLayout(titleBar_);
	titleLayout->setContentsMargins(14, 0, 8, 0);
	titleLayout->setSpacing(10);

	auto* brandMark = new QLabel(QStringLiteral("E"), titleBar_);
	brandMark->setObjectName(QStringLiteral("brandMark"));
	brandMark->setAlignment(Qt::AlignCenter);
	brandMark->setFixedSize(30, 30);
	titleLabel_ = new QLabel(QStringLiteral("EevesNet  /  NETWORK MONITOR"), titleBar_);
	titleLabel_->setObjectName(QStringLiteral("titleLabel"));
	titleLabel_->installEventFilter(this);
	titleLayout->addWidget(brandMark);
	titleLayout->addWidget(titleLabel_);
	titleLayout->addStretch();

	auto* minimizeButton = new QPushButton(QStringLiteral("—"), titleBar_);
	minimizeButton->setObjectName(QStringLiteral("windowButton"));
	minimizeButton->setAccessibleName(QStringLiteral("Minimize"));
	minimizeButton->setToolTip(QStringLiteral("Minimize"));
	minimizeButton->setFixedSize(40, 32);
	minimizeButton->setFocusPolicy(Qt::NoFocus);
	maximizeButton_ = new QPushButton(QStringLiteral("□"), titleBar_);
	maximizeButton_->setObjectName(QStringLiteral("windowButton"));
	maximizeButton_->setAccessibleName(QStringLiteral("Maximize"));
	maximizeButton_->setToolTip(QStringLiteral("Maximize"));
	maximizeButton_->setFixedSize(40, 32);
	maximizeButton_->setFocusPolicy(Qt::NoFocus);
	auto* closeButton = new QPushButton(QStringLiteral("×"), titleBar_);
	closeButton->setObjectName(QStringLiteral("closeButton"));
	closeButton->setAccessibleName(QStringLiteral("Close"));
	closeButton->setToolTip(QStringLiteral("Close"));
	closeButton->setFixedSize(40, 32);
	closeButton->setFocusPolicy(Qt::NoFocus);
	titleLayout->addWidget(minimizeButton);
	titleLayout->addWidget(maximizeButton_);
	titleLayout->addWidget(closeButton);
	connect(minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
	connect(maximizeButton_, &QPushButton::clicked, this, &MainWindow::toggleMaximize);
	connect(closeButton, &QPushButton::clicked, this, &QWidget::close);

	auto* contentWidget = new QWidget(centralWidget);
	auto* mainLayout = new QVBoxLayout(contentWidget);
	mainLayout->setContentsMargins(18, 16, 18, 18);
	mainLayout->setSpacing(12);
	auto* sectionTabs = new QTabWidget(contentWidget);
	sectionTabs->setObjectName(QStringLiteral("sectionTabs"));
	sectionTabs->tabBar()->setExpanding(true);
	auto* monitorPage = new QWidget(sectionTabs);
	auto* dashboardPage = new QWidget(sectionTabs);
	dashboardPage->setObjectName(QStringLiteral("dashboardPage"));
	auto* dashboardLayout = new QVBoxLayout(dashboardPage);
	dashboardLayout->setContentsMargins(18, 18, 18, 18);
	auto* dashboardTitle = new QLabel(QStringLiteral("Network Dashboard"), dashboardPage);
	dashboardTitle->setObjectName(QStringLiteral("dashboardTitle"));
#if EEVESNET_HAS_QT_WEBENGINE
	auto* mapPlaceholder = new EmbeddedLeafletView(dashboardPage);
#else
	auto* mapPlaceholder = new QLabel(QStringLiteral("Install Qt WebEngine for this Qt kit to display the embedded Leaflet map."), dashboardPage);
	mapPlaceholder->setAlignment(Qt::AlignCenter);
	mapPlaceholder->setObjectName(QStringLiteral("mapUnavailable"));
#endif
	mapPlaceholder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
#if EEVESNET_HAS_QT_WEBENGINE
	auto* mapAttribution = new QLabel(QStringLiteral("Tiles © Esri — Esri, DeLorme, NAVTEQ"), dashboardPage);
	mapAttribution->setObjectName(QStringLiteral("mapAttribution"));
	mapAttribution->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
#endif
	dashboardLayout->addWidget(dashboardTitle);
	dashboardLayout->addWidget(mapPlaceholder, 1);
#if EEVESNET_HAS_QT_WEBENGINE
	dashboardLayout->addWidget(mapAttribution);
#endif
	auto* monitorLayout = new QVBoxLayout(monitorPage);
	monitorLayout->setContentsMargins(0, 0, 0, 0);
	monitorLayout->setSpacing(12);
	auto* controlsLayout = new QHBoxLayout;
	controlsLayout->setSpacing(10);
	interfaceSelector_ = new QComboBox(monitorPage);
	interfaceSelector_->setObjectName(QStringLiteral("interfaceSelector"));
	interfaceSelector_->setMinimumWidth(420);
	startButton_ = new QPushButton(QStringLiteral("Start capture"), monitorPage);
	startButton_->setObjectName(QStringLiteral("startButton"));
	stopButton_ = new QPushButton(QStringLiteral("Stop capture"), monitorPage);
	stopButton_->setObjectName(QStringLiteral("stopButton"));
	stopButton_->setEnabled(false);
	controlsLayout->addWidget(interfaceSelector_, 1);
	controlsLayout->addWidget(startButton_);
	controlsLayout->addWidget(stopButton_);

	statusLabel_ = new QLabel(QStringLiteral("Initializing capture support…"), monitorPage);
	statusLabel_->setObjectName(QStringLiteral("statusLabel"));
	statusLabel_->setWordWrap(true);
	packetCountLabel_ = new QLabel(QStringLiteral("Packets captured: 0"), monitorPage);
	packetCountLabel_->setObjectName(QStringLiteral("packetCountLabel"));
	packetList_ = new QListWidget(monitorPage);
	packetList_->setObjectName(QStringLiteral("packetList"));
	packetList_->setAlternatingRowColors(true);
	auto* packetDetailsPanel = new QWidget(monitorPage);
	packetDetailsPanel->setObjectName(QStringLiteral("packetDetailsPanel"));
	auto* packetDetailsLayout = new QVBoxLayout(packetDetailsPanel);
	packetDetailsLayout->setContentsMargins(10, 8, 10, 8);
	packetDetailsLayout->setSpacing(6);
	auto* packetDetailsTitle = new QLabel(QStringLiteral("Selected packet details"), packetDetailsPanel);
	packetDetailsTitle->setObjectName(QStringLiteral("packetDetailsTitle"));
	packetDetails_ = new QTextBrowser(packetDetailsPanel);
	packetDetails_->setObjectName(QStringLiteral("packetDetails"));
	packetDetails_->setText(QStringLiteral("Select a packet above to view its details."));
	packetDetailsLayout->addWidget(packetDetailsTitle);
	packetDetailsLayout->addWidget(packetDetails_);
	auto* packetPayloadPanel = new QWidget(monitorPage);
	packetPayloadPanel->setObjectName(QStringLiteral("packetDetailsPanel"));
	auto* packetPayloadLayout = new QVBoxLayout(packetPayloadPanel);
	packetPayloadLayout->setContentsMargins(10, 8, 10, 8);
	packetPayloadLayout->setSpacing(6);
	auto* packetPayloadTitle = new QLabel(QStringLiteral("Selected packet payload (hex / ASCII)"), packetPayloadPanel);
	packetPayloadTitle->setObjectName(QStringLiteral("packetDetailsTitle"));
	packetPayload_ = new QPlainTextEdit(packetPayloadPanel);
	packetPayload_->setObjectName(QStringLiteral("packetPayload"));
	packetPayload_->setReadOnly(true);
	packetPayload_->setLineWrapMode(QPlainTextEdit::NoWrap);
	packetPayload_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	packetPayload_->setPlainText(QStringLiteral("Select a packet to view its payload."));
	packetPayloadLayout->addWidget(packetPayloadTitle);
	packetPayloadLayout->addWidget(packetPayload_);
	auto* detailsSplitter = new QSplitter(Qt::Horizontal, monitorPage);
	detailsSplitter->addWidget(packetDetailsPanel);
	detailsSplitter->addWidget(packetPayloadPanel);
	detailsSplitter->setStretchFactor(0, 1);
	detailsSplitter->setStretchFactor(1, 1);
	detailsSplitter->setSizes({450, 450});
	auto* packetSplitter = new QSplitter(Qt::Vertical, monitorPage);
	packetSplitter->addWidget(packetList_);
	packetSplitter->addWidget(detailsSplitter);
	packetSplitter->setStretchFactor(0, 3);
	packetSplitter->setStretchFactor(1, 1);
	packetSplitter->setSizes({300, 130});

	monitorLayout->addLayout(controlsLayout);
	monitorLayout->addWidget(statusLabel_);
	monitorLayout->addWidget(packetCountLabel_);
	monitorLayout->addWidget(packetSplitter, 1);
	sectionTabs->addTab(dashboardPage, QStringLiteral("Dashboard"));
	sectionTabs->addTab(monitorPage, QStringLiteral("Monitor"));
	sectionTabs->setCurrentWidget(dashboardPage);
	mainLayout->addWidget(sectionTabs);
	rootLayout->addWidget(titleBar_);
	rootLayout->addWidget(contentWidget, 1);
	setCentralWidget(centralWidget);
	setStyleSheet(QStringLiteral(R"(
		QMainWindow { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0b111b, stop:1 #0b1b1d); }
		QWidget#windowRoot { background: #0b111b; border: 1px solid #293e50; }
		QWidget#titleBar { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #112137, stop:1 #112923); border-bottom: 1px solid #293e50; }
		QLabel { color: #d7e0eb; }
		QLabel#brandMark { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #27a9f5, stop:1 #25d39a); color: #07150f; border-radius: 8px; font-size: 18px; font-weight: 800; }
		QLabel#titleLabel { color: #eaf2fb; font-size: 13px; font-weight: 700; }
		QLabel#statusLabel { background: #111c2b; border: 1px solid #24384a; border-left: 3px solid #22b7b7; border-radius: 7px; padding: 10px; color: #b9c8d9; }
		QLabel#packetCountLabel { color: #58c8dc; font-size: 13px; font-weight: 700; }
		QLabel#packetDetailsTitle { color: #58c8dc; font-size: 12px; font-weight: 700; }
		QLabel#dashboardTitle { color: #eaf2fb; font-size: 20px; font-weight: 700; }
		QWidget#worldMap, QLabel#mapUnavailable { background: #0a1521; border: 1px solid #29384d; border-radius: 7px; color: #aebed0; }
		QLabel#mapAttribution { color: #7f91a6; font-size: 10px; }
		QLabel#mapUnavailable { padding: 18px; }
		QTabWidget#sectionTabs::pane { background: #0b111b; border: 1px solid #293e50; border-radius: 7px; }
		QTabBar::tab { background: #111c2b; color: #aebed0; border: 1px solid #29384d; border-bottom: 0; padding: 8px 18px; margin-right: 4px; border-top-left-radius: 6px; border-top-right-radius: 6px; }
		QTabBar::tab:selected { background: #176f7d; color: #ffffff; }
		QTabBar::tab:hover:!selected { background: #1a2839; }
		QWidget#packetDetailsPanel { background: #0e1927; border: 1px solid #29384d; border-radius: 7px; }
		QComboBox, QListWidget, QTextBrowser, QPlainTextEdit { background: #111c2b; color: #d7e0eb; border: 1px solid #29384d; border-radius: 7px; padding: 8px; selection-background-color: #176f7d; }
		QComboBox:hover { border-color: #26b8cb; }
		QComboBox::drop-down { border: 0; width: 26px; }
		QListWidget::item { padding: 5px; border-radius: 3px; }
		QListWidget::item:alternate { background: #142131; }
		QListWidget::item:selected { background: #176f7d; color: #ffffff; }
		QPushButton { background: #1a2839; color: #d7e0eb; border: 1px solid #304158; border-radius: 7px; padding: 8px 14px; font-weight: 600; }
		QPushButton:hover { background: #24354a; border-color: #3d5875; }
		QPushButton:disabled { background: #141d29; color: #687789; border-color: #202b3a; }
		QPushButton#startButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #168fe0, stop:1 #16a879); color: #f1fff9; border-color: #28aeb8; }
		QPushButton#startButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #27a7f2, stop:1 #24bd8b); }
		QPushButton#stopButton:enabled { background: #612c38; border-color: #82404b; color: #ffe9eb; }
		QPushButton#stopButton:enabled:hover { background: #7a3442; }
		QPushButton#windowButton, QPushButton#closeButton { background: transparent; border: 0; border-radius: 5px; padding: 0; font-size: 18px; }
		QPushButton#windowButton:hover { background: #253449; }
		QPushButton#closeButton:hover { background: #d94452; color: #ffffff; }
	)"));

	worker_->moveToThread(&workerThread_);
	connect(&workerThread_, &QThread::started, worker_, &CaptureWorker::initialize);
	connect(&workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
	connect(worker_, &CaptureWorker::interfacesAvailable, this, &MainWindow::populateInterfaces);
	connect(worker_, &CaptureWorker::statusChanged, statusLabel_, &QLabel::setText);
	connect(worker_, &CaptureWorker::packetsCaptured, this, &MainWindow::appendPackets);
	connect(packetList_, &QListWidget::itemClicked, this, &MainWindow::showPacketDetails);
	connect(worker_, &CaptureWorker::captureFinished, this, &MainWindow::captureFinished);
	connect(startButton_, &QPushButton::clicked, this, &MainWindow::startCapture);
	connect(stopButton_, &QPushButton::clicked, this, &MainWindow::stopCapture);

	workerThread_.start();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == titleBar_ || watched == titleLabel_)
	{
		if (event->type() == QEvent::MouseButtonDblClick)
		{
		auto* mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->button() == Qt::LeftButton)
			{
				toggleMaximize();
				return true;
			}
		}
		else if (event->type() == QEvent::MouseButtonPress)
		{
			auto* mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->button() == Qt::LeftButton)
			{
				if (!isMaximized())
				{
					dragOffset_ = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
					dragging_ = true;
				}
				return true;
			}
		}
		else if (event->type() == QEvent::MouseMove && dragging_)
		{
			auto* mouseEvent = static_cast<QMouseEvent*>(event);
			if ((mouseEvent->buttons() & Qt::LeftButton) != 0 && !isMaximized())
			{
				move(mouseEvent->globalPosition().toPoint() - dragOffset_);
				return true;
			}
		}
		else if (event->type() == QEvent::MouseButtonRelease)
		{
			auto* mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->button() == Qt::LeftButton)
			{
				dragging_ = false;
				return true;
			}
		}
	}

	return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event)
{
	QMainWindow::changeEvent(event);
	if (event->type() == QEvent::WindowStateChange && maximizeButton_ != nullptr)
	{
		const bool maximized = isMaximized();
		maximizeButton_->setText(maximized ? QStringLiteral("❐") : QStringLiteral("□"));
		maximizeButton_->setAccessibleName(maximized ? QStringLiteral("Restore") : QStringLiteral("Maximize"));
		maximizeButton_->setToolTip(maximized ? QStringLiteral("Restore") : QStringLiteral("Maximize"));
	}
}

void MainWindow::toggleMaximize()
{
	if (isMaximized())
	{
		showNormal();
	}
	else
	{
		showMaximized();
	}
}

MainWindow::~MainWindow()
{
	worker_->requestStop();
	workerThread_.quit();
	workerThread_.wait();
}

void MainWindow::populateInterfaces(const QStringList& deviceNames, const QStringList& interfaceLabels)
{
	interfaceSelector_->clear();
	for (qsizetype index = 0; index < deviceNames.size() && index < interfaceLabels.size(); ++index)
	{
		interfaceSelector_->addItem(interfaceLabels.at(index), deviceNames.at(index));
	}
	startButton_->setEnabled(!deviceNames.isEmpty());
}

void MainWindow::startCapture()
{
	if (interfaceSelector_->currentIndex() < 0)
	{
		statusLabel_->setText(QStringLiteral("Select a network adapter before starting capture."));
		return;
	}

	packetCount_ = 0;
	packetCountLabel_->setText(QStringLiteral("Packets captured: 0"));
	packetList_->clear();
	packetDetails_->setText(QStringLiteral("Select a packet above to view its details."));
	packetPayload_->setPlainText(QStringLiteral("Select a packet to view its payload."));
	setCaptureControlsEnabled(false);
	stopButton_->setEnabled(true);
	worker_->prepareCapture();
	const auto deviceName = interfaceSelector_->currentData().toString();
	QMetaObject::invokeMethod(worker_, "startCapture", Qt::QueuedConnection, Q_ARG(QString, deviceName));
}

void MainWindow::stopCapture()
{
	worker_->requestStop();
	stopButton_->setEnabled(false);
	statusLabel_->setText(QStringLiteral("Stopping capture…"));
}

void MainWindow::appendPackets(const QList<QPair<QString, QByteArray>>& packets)
{
	packetCount_ += static_cast<quint64>(packets.size());
	packetList_->setUpdatesEnabled(false);
	const auto timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
	for (const auto& packet : packets)
	{
		const auto& summary = packet.first;
		const auto visibleSummary = summary.section(QStringLiteral(" | "), 0, 0);
		auto* item = new QListWidgetItem(QStringLiteral("%1  %2")
			.arg(timestamp, visibleSummary));
		item->setData(Qt::UserRole, summary);
		item->setData(Qt::UserRole + 1, packet.second);
		packetList_->addItem(item);
		if (packetList_->count() > 1000)
		{
			delete packetList_->takeItem(0);
		}
	}
	packetList_->setUpdatesEnabled(true);
	packetCountLabel_->setText(QStringLiteral("Packets captured: %1").arg(packetCount_));
	packetList_->scrollToBottom();
}

void MainWindow::showPacketDetails(QListWidgetItem* item)
{
	if (item == nullptr)
	{
		return;
	}

	const auto payload = item->data(Qt::UserRole + 1).toByteArray();
	if (payload.isEmpty())
	{
		packetPayload_->setPlainText(QStringLiteral("No TCP/UDP application payload captured."));
	}
	else
	{
		QStringList payloadLines;
		const auto displayLength = qMin(qsizetype(4096), payload.size());
		for (qsizetype offset = 0; offset < displayLength; offset += 16)
		{
			QString hex;
			QString ascii;
			const auto lineLength = qMin(qsizetype(16), displayLength - offset);
			for (qsizetype index = 0; index < lineLength; ++index)
			{
				const auto byte = static_cast<unsigned char>(payload.at(offset + index));
				hex.append(QStringLiteral("%1 ").arg(byte, 2, 16, QLatin1Char('0')));
				ascii.append(byte >= 32 && byte <= 126 ? QLatin1Char(byte) : QLatin1Char('.'));
			}
			payloadLines.append(QStringLiteral("%1  %2 %3")
				.arg(offset, 8, 16, QLatin1Char('0'))
				.arg(hex.leftJustified(48, QLatin1Char(' ')), ascii));
		}
		if (payload.size() > 4096)
		{
			payloadLines.append(QStringLiteral("Payload display limited to 4096 bytes."));
		}
		packetPayload_->setPlainText(payloadLines.join(QLatin1Char('\n')));
	}

	const auto fields = item->data(Qt::UserRole).toString().split(QStringLiteral(" | "));
	QStringList rows;
	const auto addField = [&rows](const QString& label, const QString& value)
	{
		rows.append(QStringLiteral("<tr><th align=\"left\">%1</th><td>%2</td></tr>")
			.arg(label.toHtmlEscaped(), value.toHtmlEscaped()));
	};

	if (!fields.isEmpty())
	{
		const auto endpointSummary = fields.first();
		const auto separator = endpointSummary.indexOf(QStringLiteral("  "));
		const auto arrow = endpointSummary.indexOf(QStringLiteral(" → "));
		if (separator >= 0 && arrow > separator)
		{
			addField(QStringLiteral("Protocol"), endpointSummary.left(separator).trimmed());
			addField(QStringLiteral("Source"), endpointSummary.mid(separator + 2, arrow - separator - 2).trimmed());
			addField(QStringLiteral("Destination"), endpointSummary.mid(arrow + 3).trimmed());
		}
		else
		{
			addField(QStringLiteral("Summary"), endpointSummary);
		}
	}
	if (fields.size() > 1) addField(QStringLiteral("Packet details"), fields.at(1));
	if (fields.size() > 2) addField(QStringLiteral("TTL / hop limit"), fields.at(2));
	if (fields.size() > 3) addField(QStringLiteral("Frame size"), fields.at(3));
	if (fields.size() > 4) addField(QStringLiteral("Application protocol"), fields.at(4));
	packetDetails_->setHtml(QStringLiteral("<html><body><table cellspacing=\"8\">%1</table></body></html>")
		.arg(rows.join(QString())));
}

void MainWindow::captureFinished()
{
	setCaptureControlsEnabled(true);
	stopButton_->setEnabled(false);
}

void MainWindow::setCaptureControlsEnabled(bool enabled)
{
	interfaceSelector_->setEnabled(enabled);
	startButton_->setEnabled(enabled && interfaceSelector_->count() > 0);
}
