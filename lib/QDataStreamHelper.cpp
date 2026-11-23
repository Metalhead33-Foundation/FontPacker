#include "QDataStreamHelper.hpp"

void saveToDatastreamAsUtf8(QDataStream& stream, const QString& str)
{
	const QByteArray utf8 = str.toUtf8();
	// Length including the null terminator byte
	stream << static_cast<quint32>(utf8.size() + 1);

	if (!utf8.isEmpty()) {
		stream.writeRawData(utf8.constData(), static_cast<int>(utf8.size()));
	}

	// Explicitly write the null terminator byte safely
	const char nullChar = '\0';
	stream.writeRawData(&nullChar, 1);
}

QString loadFromDatastreamAsUtf8(QDataStream& stream)
{
	quint32 strSize = 0;
	stream >> strSize;

	// Handle empty or invalid size (must be at least 1 for the null byte)
	if (strSize <= 1) {
		if (strSize == 1) {
			stream.skipRawData(1); // Consume the null byte
		}
		return QString();
	}

	const quint32 contentSize = strSize - 1;
	QByteArray arr(static_cast<qsizetype>(contentSize), Qt::Uninitialized);

	const int bytesRead = stream.readRawData(arr.data(), static_cast<int>(contentSize));
	stream.skipRawData(1); // Discard the stream's null terminator byte

	if (bytesRead != static_cast<int>(contentSize)) {
		return QString(); // Handle stream error/EOF
	}

	return QString::fromUtf8(arr);
}