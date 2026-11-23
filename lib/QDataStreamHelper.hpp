/**
 * @file QDataStreamHelper.hpp
 * @brief Length-prefixed UTF-8 string helpers for the binary containers.
 *
 * The `.wodf`/`.wodi` formats store strings as a big-endian `uint32` byte count
 * followed by UTF-8 bytes and a trailing NUL. The count includes that NUL, so
 * an empty string is encoded as the value 1. These helpers keep the reader and
 * writer in sync; they do not change the stream's version or byte order.
 */

#ifndef QDATASTREAMHELPER_HPP
#define QDATASTREAMHELPER_HPP
#include <QString>
#include <QDataStream>

/**
 * @brief Write a length-prefixed UTF-8 string, including its trailing NUL.
 * @param stream Destination data stream. Its byte order and version are left
 *        unchanged; callers normally configure BigEndian and Qt_4_0 first.
 * @param str String to encode.
 */
void saveToDatastreamAsUtf8(QDataStream& stream, const QString& str);

/**
 * @brief Read a length-prefixed UTF-8 string written by
 *        saveToDatastreamAsUtf8().
 *
 * A length of 0 or 1 is interpreted as an empty string. A truncated payload or
 * a stream error also returns an empty string; use the stream status if the
 * difference between an empty value and malformed input matters.
 * @param stream Source data stream.
 * @return Decoded string, or an empty string on a zero/one length or read error.
 */
QString loadFromDatastreamAsUtf8(QDataStream& stream);

#endif // QDATASTREAMHELPER_HPP
