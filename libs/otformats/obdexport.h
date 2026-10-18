#pragma once

#include <QString>
#include <QUrl>

class DatReader;
class SprReader;

namespace ObdExport {
// ObjectBuilder v2, using the loaded client's version. Empty string on success.
QString save(const DatReader &dat, SprReader &spr, int clientId, const QUrl &destination);
}
