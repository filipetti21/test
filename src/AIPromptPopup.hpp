#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

// Popup z polem tekstowym na opis levelu (prompt) i przyciskiem "Generuj".
// Po udanym wygenerowaniu layoutu wynik trafia do schowka systemowego -
// wystarczy wrocic do edytora i wcisnac Ctrl+V.
class AIPromptPopup : public Popup {
protected:
    TextInput* m_promptInput = nullptr;
    CCLabelBMFont* m_statusLabel = nullptr;
    async::TaskHolder<web::WebResponse> m_requestHolder;

    bool init();
    void onGenerate(CCObject* sender);
    void setStatus(std::string const& text, ccColor3B color);

public:
    static AIPromptPopup* create();
};
