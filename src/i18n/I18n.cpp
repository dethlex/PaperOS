#include "i18n/Strings.h"
#include <cstddef>

namespace paperos {

extern const char* const kRu[];
extern const char* const kEn[];
extern const char* const kRuMon[12];
extern const char* const kEnMon[12];
extern const char* const kRuDow[7];
extern const char* const kEnDow[7];

static Lang g_lang = Lang::En;   // default language (overridden at boot from config)

void i18nSetLang(Lang l) { g_lang = l; }
Lang i18nLang()          { return g_lang; }

const char* tr(Str id) {
    size_t i = static_cast<size_t>(id);
    if (i >= static_cast<size_t>(Str::_Count)) i = 0;   // fallback: never out of bounds
    return (g_lang == Lang::En) ? kEn[i] : kRu[i];
}

const char* const* monthsShort() { return (g_lang == Lang::En) ? kEnMon : kRuMon; }
const char* const* daysShort()   { return (g_lang == Lang::En) ? kEnDow : kRuDow; }

} // namespace paperos
