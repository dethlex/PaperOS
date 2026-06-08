#pragma once
#include <string>

namespace paperos {

// Percent-декод ("%20"→" "). Усечённые/битые "%" остаются литералами.
std::string urlDecode(const std::string& in);

// Percent-энкод для href в XML. Сохраняет unreserved + '/' (разделитель пути).
std::string urlEncode(const std::string& in);

// URL-путь запроса → абсолютный путь под корнем /paperos.
// false, если после нормализации путь выходит за корень: любой сегмент "..",
// либо null-байт в декодированной строке. При false outSdPath очищается.
// "/" -> "/paperos"; "/books/x.txt" -> "/paperos/books/x.txt".
// NB: percent-декод выполняется ДО разбиения на сегменты, поэтому "%2F"
// внутри сегмента трактуется как разделитель пути (а не литеральный слэш).
// На FAT32 имя файла не может содержать '/', так что это безвредно.
bool mapUrlToSdPath(const std::string& urlPath, std::string& outSdPath);

} // namespace paperos
