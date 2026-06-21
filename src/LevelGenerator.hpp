#pragma once

// =============================================================================
// LevelGenerator.hpp
//
// Cala logika "AI -> layout levela" w jednym, header-only pliku:
//   1) buildRequest()         - sklada zapytanie HTTP do skonfigurowanego API AI
//   2) extractAssistantText() - wyciaga surowy tekst odpowiedzi modelu z JSON-a API
//   3) convertToPasteString() - parsuje JSON wygenerowany przez AI i zamienia go
//                                na string w formacie obiektow Geometry Dash,
//                                ktory mozna wkleic w edytorze (Ctrl+V)
//
// WAZNE (przeczytaj):
// Krok (3) korzysta z publicznie znanego formatu zapisu obiektow GD
// ("1,<id>,2,<x>,3,<y>,...;") - to ten sam format, ktory trafia do schowka
// systemowego po skopiowaniu obiektow w edytorze (Ctrl+C). Dlatego ten mod
// NIE wola zadnych wewnetrznych/niezdokumentowanych funkcji silnika GD do
// wstawiania obiektow - zamiast tego podmienia zawartosc schowka, a samo
// wklejenie (Ctrl+V) robi juz wbudowana, natywna funkcja edytora. Dzieki temu
// kod jest odporny na drobne zmiany w bindingach miedzy wersjami GD/Geode.
//
// Tabela ID obiektow (funkcja palette() ponizej): "block" i "spike" sa pewne na 100%.
// Reszta (pady/orby/portale predkosci) to powszechnie cytowane w community
// wartosci - DZIALAJA w wiekszosci wspolczesnych wersji GD, ale przed
// powaznym uzyciem warto je zweryfikowac samemu (patrz README.md).
// =============================================================================

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <matjson.hpp>

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdio>

using namespace geode::prelude;

namespace ai_level {

// ---------------------------------------------------------------------------
// Wynik konwersji odpowiedzi AI na string gotowy do wklejenia w edytorze.
// ---------------------------------------------------------------------------
struct GenerateResult {
    bool ok = false;
    std::string error;          // wypelnione, gdy ok == false
    std::string pasteString;    // gotowy string obiektow GD (Ctrl+V w edytorze)
    int objectCount = 0;
    int skippedCount = 0;       // obiekty pominiete (nieznany "type" / zly format)
};

// ---------------------------------------------------------------------------
// Tabela typ-AI -> ID obiektu w Geometry Dash.
// "block" oraz "spike" sa pewne. Reszte zweryfikuj / rozbuduj wedlug potrzeb.
// ---------------------------------------------------------------------------
inline const std::unordered_map<std::string, int>& palette() {
    static const std::unordered_map<std::string, int> table = {
        {"block", 1},
        {"spike", 8},
        // ponizsze sa powszechnie uzywane w community, ale zweryfikuj je sam:
        {"jump_pad_yellow", 35},
        {"jump_ring_yellow", 36},
        {"jump_pad_pink", 67},
        {"jump_ring_pink", 84},
        {"gravity_portal_normal", 10},
        {"gravity_portal_flipped", 11},
        {"speed_slow", 200},
        {"speed_normal", 201},
        {"speed_fast", 202},
        {"speed_faster", 203},
        {"speed_fastest", 1334},
    };
    return table;
}

// Jedna jednostka siatki = 30 jednostek silnika GD (standardowy rozmiar bloku).
constexpr double kGridUnit = 30.0;
// Gdzie ma wyladowac lewy-dolny rog wygenerowanego fragmentu (w jednostkach silnika).
// x=300 -> kawalek w prawo od startu levelu, y=105 -> wysokosc standardowej podlogi.
constexpr double kOriginX = 300.0;
constexpr double kOriginY = 105.0;

// ---------------------------------------------------------------------------
// Prompt systemowy wysylany do modelu AI (po angielsku - modele zazwyczaj
// najlepiej trzymaja sie formatu, gdy instrukcja jest w jezyku angielskim).
// ---------------------------------------------------------------------------
inline const std::string& systemPrompt() {
    static const std::string prompt =
        "You are a level layout generator for the game Geometry Dash. "
        "You receive a short natural-language description of a level section "
        "and must respond with ONLY raw JSON - no markdown, no code fences, "
        "no commentary, no explanation - in exactly this shape: "
        "{\"objects\":[{\"type\":\"block\",\"x\":0,\"y\":0},"
        "{\"type\":\"spike\",\"x\":3,\"y\":0}]} "
        "Allowed \"type\" values: block, spike, jump_pad_yellow, jump_ring_yellow, "
        "jump_pad_pink, jump_ring_pink, gravity_portal_normal, gravity_portal_flipped, "
        "speed_slow, speed_normal, speed_fast, speed_faster, speed_fastest. "
        "\"x\" and \"y\" are integer grid coordinates (1 unit = 1 block), measured "
        "from the bottom-left of the section; y=0 is ground level. "
        "Design a playable, ground-based cube-gameplay section roughly 40 to 80 "
        "units wide. Never place a spike so that it fully blocks a passable path - "
        "always leave a way to jump over or around every obstacle. "
        "Keep the object count reasonable (under 150) and do not stack duplicate "
        "objects on the exact same x/y.";
    return prompt;
}

// ---------------------------------------------------------------------------
// Male pomocnicze funkcje string-owe (zero zaleznosci od niepewnych API).
// ---------------------------------------------------------------------------
inline void trimInPlace(std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) {
        s.clear();
        return;
    }
    s = s.substr(start, end - start + 1);
}

// Usuwa ewentualne ```/```json ... ``` wokol odpowiedzi modelu (czesto
// modele dorzucaja code-fence mimo instrukcji, by tego nie robic).
inline std::string stripCodeFences(std::string s) {
    trimInPlace(s);
    if (s.rfind("```", 0) == 0) {
        auto nl = s.find('\n');
        if (nl != std::string::npos) {
            s = s.substr(nl + 1);
        }
        auto pos = s.rfind("```");
        if (pos != std::string::npos) {
            s = s.substr(0, pos);
        }
    }
    trimInPlace(s);
    return s;
}

// Escapowanie tekstu do wstrzykniecia jako wartosc string w recznie
// sklejanym JSON-ie (zamiast polegac na niepewnym API budowania matjson::Value).
inline std::string jsonEscape(std::string const& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Buduje i wysyla zapytanie HTTP do skonfigurowanego dostawcy AI.
// Zwraca typ deduced (auto) - to, co zwraca WebRequest::post(), czyli
// "spawnowalny" task/future, ktory mozna przekazac wprost do TaskHolder::spawn().
// ---------------------------------------------------------------------------
inline auto buildRequest(std::string const& userPrompt) {
    auto mod = Mod::get();
    auto provider = mod->getSettingValue<std::string>("ai-provider");
    auto apiKey = mod->getSettingValue<std::string>("api-key");
    auto model = mod->getSettingValue<std::string>("model");
    auto customEndpoint = mod->getSettingValue<std::string>("custom-endpoint");

    auto req = web::WebRequest();
    req.header("Content-Type", "application/json");
    req.timeout(std::chrono::seconds(60));

    if (provider == "anthropic") {
        req.header("x-api-key", apiKey);
        req.header("anthropic-version", "2023-06-01");

        std::string body = "{";
        body += "\"model\":\"" + jsonEscape(model) + "\",";
        body += "\"max_tokens\":4096,";
        body += "\"system\":\"" + jsonEscape(systemPrompt()) + "\",";
        body += "\"messages\":[{\"role\":\"user\",\"content\":\"" + jsonEscape(userPrompt) + "\"}]";
        body += "}";

        req.bodyString(body);
        return req.post("https://api.anthropic.com/v1/messages");
    }

    // "openai" oraz "custom" -> traktujemy jako endpoint kompatybilny
    // z OpenAI Chat Completions (dziala tez z wiekszoscia lokalnych serwerow LLM).
    std::string url = (provider == "custom")
        ? customEndpoint
        : "https://api.openai.com/v1/chat/completions";

    if (!apiKey.empty()) {
        req.header("Authorization", "Bearer " + apiKey);
    }

    std::string body = "{";
    body += "\"model\":\"" + jsonEscape(model) + "\",";
    body += "\"temperature\":0.9,";
    body += "\"messages\":[";
    body += "{\"role\":\"system\",\"content\":\"" + jsonEscape(systemPrompt()) + "\"},";
    body += "{\"role\":\"user\",\"content\":\"" + jsonEscape(userPrompt) + "\"}";
    body += "]}";

    req.bodyString(body);
    return req.post(url);
}

// ---------------------------------------------------------------------------
// Wyciaga tekst wygenerowany przez model z surowej odpowiedzi JSON API.
// ---------------------------------------------------------------------------
inline Result<std::string> extractAssistantText(matjson::Value const& res, std::string const& provider) {
    try {
        if (provider == "anthropic") {
            auto textRes = res["content"][0]["text"].asString();
            if (textRes) {
                return Ok(textRes.unwrap());
            }
            return Err(std::string(
                "Nie udalo sie odczytac pola content[0].text z odpowiedzi Anthropic API. "
                "Sprawdz, czy klucz API i model sa poprawne."
            ));
        }

        auto textRes = res["choices"][0]["message"]["content"].asString();
        if (textRes) {
            return Ok(textRes.unwrap());
        }
        return Err(std::string(
            "Nie udalo sie odczytac pola choices[0].message.content z odpowiedzi API. "
            "Sprawdz, czy klucz API, model i endpoint sa poprawne."
        ));
    } catch (std::exception const& e) {
        return Err(std::string("Niespodziewany blad podczas odczytu odpowiedzi API: ") + e.what());
    } catch (...) {
        return Err(std::string("Niespodziewany blad podczas odczytu odpowiedzi API."));
    }
}

// ---------------------------------------------------------------------------
// Parsuje JSON wygenerowany przez model i zamienia go na string obiektow GD.
// Funkcja jest celowo bardzo defensywna (try/catch + pomijanie zlych
// wpisow) - traktujemy tekst od AI jako niezaufany / nieprzewidywalny input.
// ---------------------------------------------------------------------------
inline GenerateResult convertToPasteString(std::string const& assistantText, int maxObjects) {
    GenerateResult result;

    std::string cleaned = stripCodeFences(assistantText);

    auto parsed = matjson::parse(cleaned);
    if (!parsed) {
        result.error = "AI nie zwrocilo poprawnego JSON-a. Sprobuj ponownie lub doprecyzuj opis.";
        return result;
    }

    matjson::Value root = parsed.unwrap();

    try {
        // Akceptujemy zarowno {"objects":[...]}, jak i goly [...] na wypadek,
        // gdyby model zignorowal instrukcje formatu. Sprawdzamy typ PRZED
        // probami indeksowania, zeby nie indeksowac stringiem czegos, co nie
        // jest obiektem JSON (np. golej tablicy).
        matjson::Value arrVal = root;
        if (root.type() == matjson::Type::Object) {
            matjson::Value objectsVal = root["objects"];
            if (objectsVal.type() == matjson::Type::Array) {
                arrVal = objectsVal;
            }
        }

        auto arrRes = arrVal.asArray();
        if (!arrRes) {
            result.error = "Oczekiwano tablicy obiektow (klucz \"objects\": [...]) w odpowiedzi AI.";
            return result;
        }

        auto arr = arrRes.unwrap();
        auto const& pal = palette();

        std::string out;
        int count = 0;
        int skipped = 0;

        for (auto& item : arr) {
            if (count >= maxObjects) break;

            try {
                auto typeRes = item["type"].asString();
                auto xRes = item["x"].asInt();
                auto yRes = item["y"].asInt();

                if (!typeRes || !xRes || !yRes) {
                    skipped++;
                    continue;
                }

                auto it = pal.find(typeRes.unwrap());
                if (it == pal.end()) {
                    skipped++;
                    continue;
                }

                double px = kOriginX + static_cast<double>(xRes.unwrap()) * kGridUnit;
                double py = kOriginY + static_cast<double>(yRes.unwrap()) * kGridUnit;

                out += "1," + std::to_string(it->second);
                out += ",2," + std::to_string(static_cast<long long>(px));
                out += ",3," + std::to_string(static_cast<long long>(py));

                auto rotRes = item["rot"].asInt();
                if (rotRes) {
                    out += ",6," + std::to_string(rotRes.unwrap());
                }

                out += ";";
                count++;
            } catch (...) {
                skipped++;
                continue;
            }
        }

        result.pasteString = out;
        result.objectCount = count;
        result.skippedCount = skipped;
        result.ok = count > 0;
        if (!result.ok) {
            result.error = "Z odpowiedzi AI nie udalo sie zbudowac ani jednego znanego obiektu.";
        }
        return result;
    } catch (std::exception const& e) {
        result.error = std::string("Niespodziewany blad podczas przetwarzania JSON-a: ") + e.what();
        return result;
    } catch (...) {
        result.error = "Niespodziewany blad podczas przetwarzania JSON-a.";
        return result;
    }
}

} // namespace ai_level
