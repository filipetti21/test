#include "AIPromptPopup.hpp"
#include "LevelGenerator.hpp"

bool AIPromptPopup::init() {
    if (!Popup::init(380.f, 220.f)) return false;

    this->setTitle("AI Level Designer");

    auto infoLabel = CCLabelBMFont::create("Opisz fragment levelu, ktory ma powstac:", "bigFont.fnt");
    infoLabel->setScale(0.4f);
    m_mainLayer->addChildAtPosition(infoLabel, Anchor::Top, ccp(0, -40));

    m_promptInput = TextInput::create(320.f, "np. latwa sekcja, dwa kolce i platforma", "chatFont.fnt");
    m_promptInput->setMaxCharCount(300);
    m_mainLayer->addChildAtPosition(m_promptInput, Anchor::Center, ccp(0, 12));

    m_statusLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_statusLabel->setScale(0.35f);
    m_mainLayer->addChildAtPosition(m_statusLabel, Anchor::Bottom, ccp(0, 44));

    auto genSprite = ButtonSprite::create("Generuj");
    auto genBtn = CCMenuItemSpriteExtra::create(
        genSprite, this, menu_selector(AIPromptPopup::onGenerate)
    );

    auto menu = CCMenu::create();
    menu->addChild(genBtn);
    m_mainLayer->addChildAtPosition(menu, Anchor::Bottom, ccp(0, 16));

    return true;
}

void AIPromptPopup::setStatus(std::string const& text, ccColor3B color) {
    if (!m_statusLabel) return;
    m_statusLabel->setString(text.c_str());
    m_statusLabel->setColor(color);
}

void AIPromptPopup::onGenerate(CCObject*) {
    if (!m_promptInput) return;

    auto prompt = m_promptInput->getString();
    if (prompt.empty()) {
        this->setStatus("Wpisz najpierw opis fragmentu levelu.", ccc3(255, 90, 90));
        return;
    }

    auto apiKey = Mod::get()->getSettingValue<std::string>("api-key");
    if (apiKey.empty()) {
        this->setStatus("Ustaw klucz API w ustawieniach moda (Geode -> AI Level Designer -> Settings).", ccc3(255, 90, 90));
        return;
    }

    auto provider = Mod::get()->getSettingValue<std::string>("ai-provider");
    if (provider == "custom") {
        auto endpoint = Mod::get()->getSettingValue<std::string>("custom-endpoint");
        if (endpoint.empty()) {
            this->setStatus("Dostawca = custom, ale nie ustawiono Wlasnego endpointu w ustawieniach.", ccc3(255, 90, 90));
            return;
        }
    }

    this->setStatus("Generowanie... moze potrwac kilkanascie sekund.", ccc3(255, 255, 255));

    int64_t maxObjects = Mod::get()->getSettingValue<int64_t>("max-objects");

    m_requestHolder.spawn(
        ai_level::buildRequest(prompt),
        [this, maxObjects](web::WebResponse resp) {
            try {
                if (!resp.ok()) {
                    this->setStatus(
                        fmt::format("Blad sieci/API: HTTP {}. Sprawdz klucz API, model i polaczenie.", resp.code()),
                        ccc3(255, 90, 90)
                    );
                    return;
                }

                auto jsonRes = resp.json();
                if (!jsonRes) {
                    this->setStatus("Odpowiedz API nie jest poprawnym JSON-em.", ccc3(255, 90, 90));
                    return;
                }

                auto provider = Mod::get()->getSettingValue<std::string>("ai-provider");
                auto textRes = ai_level::extractAssistantText(jsonRes.unwrap(), provider);
                if (!textRes) {
                    this->setStatus(textRes.unwrapErr(), ccc3(255, 90, 90));
                    return;
                }

                auto result = ai_level::convertToPasteString(textRes.unwrap(), static_cast<int>(maxObjects));
                if (!result.ok) {
                    this->setStatus(result.error, ccc3(255, 90, 90));
                    return;
                }

                auto written = clipboard::write(result.pasteString);
                if (!written) {
                    log::info("AI Level Designer - wygenerowany string obiektow (nie udalo sie skopiowac do schowka): {}", result.pasteString);
                    this->setStatus("Wygenerowano, ale nie udalo sie zapisac do schowka. String zalogowany w konsoli Geode.", ccc3(255, 200, 90));
                    return;
                }

                this->setStatus(
                    fmt::format(
                        "Gotowe! {} obiektow w schowku ({} pominietych). Wroc do edytora i wcisnij Ctrl+V.",
                        result.objectCount, result.skippedCount
                    ),
                    ccc3(120, 255, 120)
                );
            } catch (std::exception const& e) {
                this->setStatus(std::string("Niespodziewany blad: ") + e.what(), ccc3(255, 90, 90));
            } catch (...) {
                this->setStatus("Niespodziewany blad podczas przetwarzania odpowiedzi.", ccc3(255, 90, 90));
            }
        }
    );
}

AIPromptPopup* AIPromptPopup::create() {
    auto ret = new AIPromptPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}
