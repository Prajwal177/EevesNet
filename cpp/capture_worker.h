#pragma once

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QStringList>

#include <atomic>

class CaptureWorker final : public QObject
{
	Q_OBJECT

public:
	explicit CaptureWorker(QObject* parent = nullptr);
	void prepareCapture() noexcept;
	void requestStop() noexcept;

public slots:
	void initialize();
	void startCapture(const QString& deviceName);

signals:
	void interfacesAvailable(const QStringList& deviceNames, const QStringList& interfaceLabels);
	void statusChanged(const QString& status);
	void packetsCaptured(const QList<QPair<QString, QByteArray>>& packets);
	void captureFinished();

private:
	std::atomic_bool stopRequested_{false};
};
