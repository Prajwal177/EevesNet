#include "capture_worker.h"

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QQueue>
#include <QSet>
#include <QStringList>

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef EEVESNET_HAS_NPCAP
#define EEVESNET_HAS_NPCAP 0
#endif

#if EEVESNET_HAS_NPCAP
#include <pcap.h>
#endif

namespace
{
#if EEVESNET_HAS_NPCAP
QString protocolName(std::uint8_t protocol)
{
	switch (protocol)
	{
	case 1:
		return QStringLiteral("ICMP");
	case 6:
		return QStringLiteral("TCP");
	case 17:
		return QStringLiteral("UDP");
	case 58:
		return QStringLiteral("ICMPv6");
	default:
		return QStringLiteral("IP");
	}
}

QSet<QString> adapterAddresses(const QString& deviceName)
{
	QSet<QString> addresses;
	const auto deviceNameBytes = deviceName.toLocal8Bit();
	pcap_if_t* devices = nullptr;
	char errorBuffer[PCAP_ERRBUF_SIZE]{};
	if (pcap_findalldevs(&devices, errorBuffer) == -1)
	{
		return addresses;
	}

	for (auto* device = devices; device != nullptr; device = device->next)
	{
		if (device->name == nullptr || deviceNameBytes != device->name)
		{
			continue;
		}

		for (auto* address = device->addresses; address != nullptr; address = address->next)
		{
			if (address->addr == nullptr)
			{
				continue;
			}

			if (address->addr->sa_family == AF_INET)
			{
				const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address->addr);
				const auto* bytes = reinterpret_cast<const unsigned char*>(&ipv4->sin_addr);
				addresses.insert(QStringLiteral("%1.%2.%3.%4")
					.arg(static_cast<unsigned int>(bytes[0]))
					.arg(static_cast<unsigned int>(bytes[1]))
					.arg(static_cast<unsigned int>(bytes[2]))
					.arg(static_cast<unsigned int>(bytes[3])));
			}
			else if (address->addr->sa_family == AF_INET6)
			{
				const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(address->addr);
				Q_IPV6ADDR bytes{};
				std::memcpy(bytes.c, &ipv6->sin6_addr, sizeof(bytes.c));
				addresses.insert(QHostAddress(bytes).toString());
			}
		}
		break;
	}

	pcap_freealldevs(devices);
	return addresses;
}

class IpAddressCache final
{
	static constexpr qsizetype maximumEntries = 4096;

public:
	template <typename Address, typename Formatter>
	QString get(const Address& address, qsizetype length, Formatter formatter)
	{
		QByteArray key;
		key.reserve(length + 1);
		key.append(static_cast<char>(length));
		for (qsizetype index = 0; index < length; ++index)
		{
			key.append(static_cast<char>(address[index]));
		}

		const auto existing = entries_.constFind(key);
		if (existing != entries_.cend())
		{
			return existing.value();
		}

		const auto value = formatter();
		if (entries_.size() == maximumEntries)
		{
			entries_.remove(insertionOrder_.dequeue());
		}
		entries_.insert(key, value);
		insertionOrder_.enqueue(std::move(key));
		return value;
	}

private:
	QHash<QByteArray, QString> entries_;
	QQueue<QByteArray> insertionOrder_;
};

QString ipv4Address(const u_char* bytes, IpAddressCache& addressCache)
{
	return addressCache.get(bytes, 4, [bytes]
	{
		return QStringLiteral("%1.%2.%3.%4")
			.arg(static_cast<unsigned int>(bytes[0]))
			.arg(static_cast<unsigned int>(bytes[1]))
			.arg(static_cast<unsigned int>(bytes[2]))
			.arg(static_cast<unsigned int>(bytes[3]));
	});
}

std::uint16_t network16(const u_char* bytes)
{
	return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[0]) << 8) | bytes[1]);
}

QString tcpFlagNames(const u_char* tcp)
{
	QStringList flags;
	if ((tcp[12] & 0x01) != 0) flags.append(QStringLiteral("NS"));
	if ((tcp[13] & 0x01) != 0) flags.append(QStringLiteral("FIN"));
	if ((tcp[13] & 0x02) != 0) flags.append(QStringLiteral("SYN"));
	if ((tcp[13] & 0x04) != 0) flags.append(QStringLiteral("RST"));
	if ((tcp[13] & 0x08) != 0) flags.append(QStringLiteral("PSH"));
	if ((tcp[13] & 0x10) != 0) flags.append(QStringLiteral("ACK"));
	if ((tcp[13] & 0x20) != 0) flags.append(QStringLiteral("URG"));
	if ((tcp[13] & 0x40) != 0) flags.append(QStringLiteral("ECE"));
	if ((tcp[13] & 0x80) != 0) flags.append(QStringLiteral("CWR"));
	return flags.isEmpty() ? QStringLiteral("none") : flags.join(QLatin1Char(','));
}

QString icmpTypeName(std::uint8_t protocol, std::uint8_t type)
{
	if (protocol == 1)
	{
		switch (type)
		{
		case 0: return QStringLiteral("echo reply");
		case 3: return QStringLiteral("destination unreachable");
		case 8: return QStringLiteral("echo request");
		case 11: return QStringLiteral("time exceeded");
		default: break;
		}
	}
	else if (protocol == 58)
	{
		switch (type)
		{
		case 1: return QStringLiteral("destination unreachable");
		case 2: return QStringLiteral("packet too big");
		case 3: return QStringLiteral("time exceeded");
		case 4: return QStringLiteral("parameter problem");
		case 128: return QStringLiteral("echo request");
		case 129: return QStringLiteral("echo reply");
		default: break;
		}
	}
	return QStringLiteral("ICMP");
}

QString transportDetails(std::uint8_t protocol, const u_char* transport, std::size_t length)
{
	switch (protocol)
	{
	case 6:
		if (length < 20)
		{
			return QStringLiteral("TCP header truncated");
		}
		if ((transport[12] >> 4) < 5 || static_cast<std::size_t>((transport[12] >> 4) * 4) > length)
		{
			return QStringLiteral("Malformed TCP header");
		}
		return QStringLiteral("source port=%1, destination port=%2, flags=%3, window=%4")
			.arg(network16(transport))
			.arg(network16(transport + 2))
			.arg(tcpFlagNames(transport))
			.arg(network16(transport + 14));
	case 17:
		if (length < 8)
		{
			return QStringLiteral("UDP header truncated");
		}
		return QStringLiteral("source port=%1, destination port=%2, UDP length=%3")
			.arg(network16(transport))
			.arg(network16(transport + 2))
			.arg(network16(transport + 4));
	case 1:
	case 58:
		if (length < 4)
		{
			return QStringLiteral("ICMP header truncated");
		}
		return QStringLiteral("%1 (type=%2, code=%3)")
			.arg(icmpTypeName(protocol, transport[0]))
			.arg(transport[0])
			.arg(transport[1]);
	default:
		return QStringLiteral("IP protocol=%1").arg(protocol);
	}
}

QString applicationProtocol(std::uint8_t protocol, const u_char* transport, std::size_t length)
{
	if (protocol == 6 && length >= 20)
	{
		const auto sourcePort = network16(transport);
		const auto destinationPort = network16(transport + 2);
		const auto tcpHeaderLength = static_cast<std::size_t>((transport[12] >> 4) * 4);
		if (tcpHeaderLength < 20 || tcpHeaderLength > length)
		{
			return {};
		}

		const auto* payload = transport + tcpHeaderLength;
		const auto payloadLength = length - tcpHeaderLength;
		const auto startsWith = [payload, payloadLength](const char* text, std::size_t textLength)
		{
			return payloadLength >= textLength && std::memcmp(payload, text, textLength) == 0;
		};

		if (startsWith("HTTP/1.", 7) || startsWith("GET ", 4) || startsWith("POST ", 5)
			|| startsWith("HEAD ", 5) || startsWith("PUT ", 4) || startsWith("DELETE ", 7)
			|| startsWith("OPTIONS ", 8) || startsWith("PATCH ", 6) || startsWith("CONNECT ", 8))
		{
			return QStringLiteral("HTTP");
		}

		const auto tlsRecord = payloadLength >= 3
			&& (payload[0] == 0x16 || payload[0] == 0x17)
			&& payload[1] == 0x03;
		if (tlsRecord)
		{
			return sourcePort == 443 || destinationPort == 443
				|| sourcePort == 8443 || destinationPort == 8443
				? QStringLiteral("HTTPS") : QStringLiteral("TLS");
		}

		if (sourcePort == 443 || destinationPort == 443
			|| sourcePort == 8443 || destinationPort == 8443)
		{
			return QStringLiteral("HTTPS");
		}
		if (sourcePort == 80 || destinationPort == 80 || sourcePort == 8080 || destinationPort == 8080
			|| sourcePort == 8000 || destinationPort == 8000)
		{
			return QStringLiteral("HTTP");
		}
	}
	else if (protocol == 17 && length >= 8)
	{
		const auto sourcePort = network16(transport);
		const auto destinationPort = network16(transport + 2);
		if (sourcePort == 443 || destinationPort == 443)
		{
			return QStringLiteral("QUIC / HTTPS");
		}
	}

	return {};
}

QString ipPacketSummary(const QString& protocol, const QString& application, const QString& source,
	const QString& destination, const QString& details, const QString& hopLabel, bpf_u_int32 frameLength)
{
	const auto protocolLabel = application.isEmpty()
		? protocol : QStringLiteral("%1 / %2").arg(protocol, application);
	return QStringLiteral("%1  %2 → %3 | %4 | %5 | %6 bytes | %7")
		.arg(protocolLabel)
		.arg(source)
		.arg(destination)
		.arg(details)
		.arg(hopLabel)
		.arg(frameLength)
		.arg(application.isEmpty() ? QStringLiteral("Application protocol not identified") : application);
}

QString packetSummary(const pcap_pkthdr* header, const u_char* packet, int linkType,
	IpAddressCache& addressCache, const QSet<QString>& localAddresses, QByteArray& payload)
{
	payload.clear();
	const auto length = static_cast<std::size_t>(header->caplen);
	if (linkType != DLT_EN10MB || length < 14)
	{
		return QStringLiteral("Captured frame (%1 bytes)").arg(header->len);
	}

	std::size_t offset = 14;
	std::uint16_t etherType = static_cast<std::uint16_t>((packet[12] << 8) | packet[13]);
	while ((etherType == 0x8100 || etherType == 0x88a8) && length >= offset + 4)
	{
		etherType = static_cast<std::uint16_t>((packet[offset + 2] << 8) | packet[offset + 3]);
		offset += 4;
	}

	if (etherType == 0x0800)
	{
		if (length < offset + 20)
		{
			return QStringLiteral("Truncated IPv4 frame (%1 bytes)").arg(header->len);
		}

		const auto headerLength = static_cast<std::size_t>((packet[offset] & 0x0f) * 4);
		if ((packet[offset] >> 4) != 4 || headerLength < 20 || length < offset + headerLength)
		{
			return QStringLiteral("Malformed IPv4 frame (%1 bytes)").arg(header->len);
		}

		const auto* ip = packet + offset;
		const auto sourceAddress = ipv4Address(ip + 12, addressCache);
		const auto destinationAddress = ipv4Address(ip + 16, addressCache);
		const auto source = localAddresses.contains(sourceAddress)
			? sourceAddress + QStringLiteral(" (Me)") : sourceAddress;
		const auto destination = localAddresses.contains(destinationAddress)
			? destinationAddress + QStringLiteral(" (Me)") : destinationAddress;
		const auto totalLength = network16(ip + 2);
		if (totalLength < headerLength)
		{
			return QStringLiteral("Malformed IPv4 length (%1 bytes)").arg(header->len);
		}

		const auto fragmentField = network16(ip + 6);
		const auto fragmentOffset = fragmentField & 0x1fff;
		const auto* transport = ip + headerLength;
		const auto capturedIpPayloadLength = length - offset - headerLength;
		const auto declaredIpPayloadLength = static_cast<std::size_t>(totalLength) - headerLength;
		const auto transportLength = capturedIpPayloadLength < declaredIpPayloadLength
			? capturedIpPayloadLength : declaredIpPayloadLength;
		if (fragmentOffset == 0)
		{
			std::size_t dataOffset = 0;
			if (ip[9] == 6 && transportLength >= 20)
			{
				dataOffset = static_cast<std::size_t>((transport[12] >> 4) * 4);
				if (dataOffset < 20 || dataOffset > transportLength) dataOffset = transportLength;
			}
			else if (ip[9] == 17 && transportLength >= 8)
			{
				dataOffset = 8;
			}
			if (transportLength > dataOffset)
			{
				const auto payloadLength = transportLength - dataOffset < 4097
					? transportLength - dataOffset : 4097;
				payload = QByteArray(reinterpret_cast<const char*>(transport + dataOffset),
					static_cast<qsizetype>(payloadLength));
			}
		}
		const auto details = fragmentOffset != 0
			? QStringLiteral("non-initial fragment, offset=%1").arg(fragmentOffset)
			: QStringLiteral("IP length=%1; %2")
				.arg(totalLength)
				.arg(transportDetails(ip[9], transport, transportLength));
		const auto application = fragmentOffset == 0
			? applicationProtocol(ip[9], transport, transportLength) : QString();
		return ipPacketSummary(protocolName(ip[9]), application, source, destination, details,
			QStringLiteral("TTL=%1").arg(ip[8]), header->len);
	}

	if (etherType == 0x86dd)
	{
		if (length < offset + 40)
		{
			return QStringLiteral("Truncated IPv6 frame (%1 bytes)").arg(header->len);
		}

		Q_IPV6ADDR sourceBytes{};
		Q_IPV6ADDR destinationBytes{};
		const auto* ipv6 = packet + offset;
		for (int index = 0; index < 16; ++index)
		{
			sourceBytes[index] = ipv6[8 + index];
			destinationBytes[index] = ipv6[24 + index];
		}

		const auto sourceAddress = addressCache.get(sourceBytes, 16, [&sourceBytes]
		{
			return QHostAddress(sourceBytes).toString();
		});
		const auto destinationAddress = addressCache.get(destinationBytes, 16, [&destinationBytes]
		{
			return QHostAddress(destinationBytes).toString();
		});
		const auto source = localAddresses.contains(sourceAddress)
			? sourceAddress + QStringLiteral(" (Me)") : sourceAddress;
		const auto destination = localAddresses.contains(destinationAddress)
			? destinationAddress + QStringLiteral(" (Me)") : destinationAddress;
		const auto declaredPayloadLength = static_cast<std::size_t>(network16(ipv6 + 4));
		const auto capturedPayloadLength = length - offset - 40;
		const auto payloadLength = declaredPayloadLength < capturedPayloadLength
			? declaredPayloadLength
			: capturedPayloadLength;
		auto nextHeader = ipv6[6];
		const u_char* transport = ipv6 + 40;
		auto transportLength = payloadLength;
		std::uint16_t fragmentOffset = 0;
		QString extensionError;

		for (int extensionCount = 0; extensionCount < 8; ++extensionCount)
		{
			if (nextHeader == 0 || nextHeader == 43 || nextHeader == 60)
			{
				if (transportLength < 2)
				{
					extensionError = QStringLiteral("Truncated IPv6 extension header");
					break;
				}
				const auto extensionLength = static_cast<std::size_t>((transport[1] + 1) * 8);
				if (extensionLength > transportLength)
				{
					extensionError = QStringLiteral("Malformed IPv6 extension header");
					break;
				}
				nextHeader = transport[0];
				transport += extensionLength;
				transportLength -= extensionLength;
			}
			else if (nextHeader == 44)
			{
				if (transportLength < 8)
				{
					extensionError = QStringLiteral("Truncated IPv6 fragment header");
					break;
				}
				fragmentOffset = static_cast<std::uint16_t>((network16(transport + 2) & 0xfff8) >> 3);
				nextHeader = transport[0];
				transport += 8;
				transportLength -= 8;
			}
			else if (nextHeader == 51)
			{
				if (transportLength < 2)
				{
					extensionError = QStringLiteral("Truncated IPv6 authentication header");
					break;
				}
				const auto extensionLength = static_cast<std::size_t>((transport[1] + 2) * 4);
				if (extensionLength > transportLength)
				{
					extensionError = QStringLiteral("Malformed IPv6 authentication header");
					break;
				}
				nextHeader = transport[0];
				transport += extensionLength;
				transportLength -= extensionLength;
			}
			else
			{
				break;
			}
		}

		QString details;
		if (!extensionError.isEmpty())
		{
			details = extensionError;
		}
		else if (fragmentOffset != 0)
		{
			details = QStringLiteral("non-initial fragment, offset=%1").arg(fragmentOffset);
		}
		else
		{
			details = transportDetails(nextHeader, transport, transportLength);
		}
		if (extensionError.isEmpty() && fragmentOffset == 0)
		{
			std::size_t dataOffset = 0;
			if (nextHeader == 6 && transportLength >= 20)
			{
				dataOffset = static_cast<std::size_t>((transport[12] >> 4) * 4);
				if (dataOffset < 20 || dataOffset > transportLength) dataOffset = transportLength;
			}
			else if (nextHeader == 17 && transportLength >= 8)
			{
				dataOffset = 8;
			}
			if (transportLength > dataOffset)
			{
				const auto payloadLength = transportLength - dataOffset < 4097
					? transportLength - dataOffset : 4097;
				payload = QByteArray(reinterpret_cast<const char*>(transport + dataOffset),
					static_cast<qsizetype>(payloadLength));
			}
		}
		details.prepend(QStringLiteral("IPv6 payload length=%1; ").arg(network16(ipv6 + 4)));
		const auto application = extensionError.isEmpty() && fragmentOffset == 0
			? applicationProtocol(nextHeader, transport, transportLength) : QString();
		return ipPacketSummary(protocolName(nextHeader), application, source, destination, details,
			QStringLiteral("hop limit=%1").arg(ipv6[7]), header->len);
	}

	return QStringLiteral("Ethernet frame 0x%1 (%2 bytes)")
		.arg(etherType, 4, 16, QLatin1Char('0'))
		.arg(header->len);
}
#endif
}

CaptureWorker::CaptureWorker(QObject* parent)
	: QObject(parent)
{
}

void CaptureWorker::requestStop() noexcept
{
	stopRequested_.store(true, std::memory_order_relaxed);
}

void CaptureWorker::prepareCapture() noexcept
{
	stopRequested_.store(false, std::memory_order_relaxed);
}

void CaptureWorker::initialize()
{
#if EEVESNET_HAS_NPCAP
	pcap_if_t* devices = nullptr;
	char errorBuffer[PCAP_ERRBUF_SIZE]{};
	if (pcap_findalldevs(&devices, errorBuffer) == -1)
	{
		emit interfacesAvailable({}, {});
		emit statusChanged(QStringLiteral("Unable to enumerate Npcap adapters: %1")
			.arg(QString::fromLocal8Bit(errorBuffer)));
		return;
	}

	QStringList deviceNames;
	QStringList interfaceLabels;
	for (auto* device = devices; device != nullptr; device = device->next)
	{
		const auto name = QString::fromLocal8Bit(device->name);
		auto label = device->description != nullptr
			? QString::fromLocal8Bit(device->description)
			: QString();
		label = label.trimmed();
		if (label.isEmpty())
		{
			label = QStringLiteral("Interface %1").arg(deviceNames.size() + 1);
		}
		deviceNames.append(name);
		interfaceLabels.append(label);
	}
	pcap_freealldevs(devices);

	emit interfacesAvailable(deviceNames, interfaceLabels);
	if (deviceNames.isEmpty())
	{
		emit statusChanged(QStringLiteral("No capture adapters found. Check that the Npcap driver is installed and running."));
	}
	else
	{
		emit statusChanged(QStringLiteral("Ready. Select an adapter to begin packet capture."));
	}
#else
	emit interfacesAvailable({}, {});
	emit statusChanged(QStringLiteral("Npcap capture is unavailable because the Npcap SDK was not found at build time. Install the SDK and configure NPCAP_SDK_ROOT."));
#endif
}

void CaptureWorker::startCapture(const QString& deviceName)
{
#if EEVESNET_HAS_NPCAP
	if (deviceName.isEmpty())
	{
		emit statusChanged(QStringLiteral("Select a network adapter before starting capture."));
		emit captureFinished();
		return;
	}

	const auto deviceNameBytes = deviceName.toLocal8Bit();
	char errorBuffer[PCAP_ERRBUF_SIZE]{};
	pcap_t* capture = pcap_open_live(deviceNameBytes.constData(), 65536, 1, 250, errorBuffer);
	if (capture == nullptr)
	{
		emit statusChanged(QStringLiteral("Unable to open adapter: %1. Check Npcap installation and administrator permissions.")
			.arg(QString::fromLocal8Bit(errorBuffer)));
		emit captureFinished();
		return;
	}

	emit statusChanged(QStringLiteral("Capturing on %1")
		.arg(deviceName));
	const auto linkType = pcap_datalink(capture);
	IpAddressCache addressCache;
	const auto localIpAddresses = adapterAddresses(deviceName);
	QList<QPair<QString, QByteArray>> packetBatch;
	packetBatch.reserve(128);
	while (!stopRequested_.load(std::memory_order_relaxed))
	{
		pcap_pkthdr* header = nullptr;
		const u_char* packet = nullptr;
		const auto result = pcap_next_ex(capture, &header, &packet);
		if (result == 1)
		{
			QByteArray payload;
			const auto summary = packetSummary(header, packet, linkType, addressCache, localIpAddresses, payload);
			packetBatch.append(qMakePair(summary, std::move(payload)));
			if (packetBatch.size() >= 128)
			{
				emit packetsCaptured(packetBatch);
				packetBatch.clear();
			}
		}
		else if (result == -1)
		{
			emit statusChanged(QStringLiteral("Capture stopped due to an error: %1")
				.arg(QString::fromLocal8Bit(pcap_geterr(capture))));
			break;
		}
		else if (result == -2)
		{
			break;
		}
	}

	if (!packetBatch.isEmpty())
	{
		emit packetsCaptured(packetBatch);
	}
	pcap_close(capture);
	if (stopRequested_.load(std::memory_order_relaxed))
	{
		emit statusChanged(QStringLiteral("Capture stopped."));
	}
	emit captureFinished();
#else
	Q_UNUSED(deviceName);
	emit statusChanged(QStringLiteral("Npcap capture is unavailable. Reconfigure with the Npcap SDK installed."));
	emit captureFinished();
#endif
}
