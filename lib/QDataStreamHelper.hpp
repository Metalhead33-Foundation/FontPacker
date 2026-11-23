#ifndef QDATASTREAMHELPER_HPP
#define QDATASTREAMHELPER_HPP
#include <QString>
#include <QDataStream>

void saveToDatastreamAsUtf8(QDataStream& stream, const QString& str);
QString loadFromDatastreamAsUtf8(QDataStream& stream);

#endif // QDATASTREAMHELPER_HPP
