#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "AIPromptPopup.hpp"

using namespace geode::prelude;

class $modify(AILevelDesignerEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto sprite = ButtonSprite::create("AI");
        auto button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(AILevelDesignerEditorUI::onOpenAIPopup)
        );

        auto menu = CCMenu::create();
        menu->addChild(button);
        // Lewy gorny obszar ekranu - poza standardowymi paskami narzedzi edytora.
        menu->setPosition(winSize.width - 35.f, winSize.height - 130.f);
        this->addChild(menu, 200);

        return true;
    }

    void onOpenAIPopup(CCObject*) {
        AIPromptPopup::create()->show();
    }
};
