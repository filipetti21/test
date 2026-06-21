# AI Level Designer (Geode mod)

Mod do Geometry Dash (przez [Geode](https://geode-sdk.org/)), ktory generuje fragment
layoutu levela na podstawie opisu tekstowego (promptu) wyslanego do wybranego API AI
(OpenAI, Anthropic, albo dowolny serwer kompatybilny z OpenAI Chat Completions, np.
lokalny LLM).

## Jak to dziala

1. W edytorze levelu pojawia sie nowy przycisk **"AI"** (prawy gorny obszar ekranu).
2. Klikniecie otwiera popup z polem tekstowym - wpisujesz opis sekcji, np.
   *"latwa sekcja kostka, dwa kolce i platforma do przeskoczenia"*.
3. Mod wysyla prompt (wraz z instrukcja formatu) do skonfigurowanego API AI.
4. Odpowiedz (JSON z lista obiektow) jest parsowana i zamieniana na string w
   **natywnym formacie obiektow Geometry Dash**.
5. Ten string trafia do **schowka systemowego**. Wracasz do edytora i wciskasz
   **Ctrl+V** - obiekty pojawiaja sie w levelu, dokladnie tak, jakbys wkleil
   skopiowane wczesniej obiekty.

### Dlaczego przez schowek, a nie "od razu w edytorze"?

Geometry Dash ma wbudowana, natywna funkcje wklejania obiektow ze schowka
(dokladnie to samo, co dzieje sie po Ctrl+C / Ctrl+V w edytorze). Ten mod
korzysta wylacznie z tego mechanizmu zamiast wolac wewnetrzne, niezdokumentowane
funkcje silnika gry do bezposredniego wstawiania obiektow. Dzieki temu:

- kod jest prosty i nie zalezy od dokladnych (i czesto niepewnych/zmieniajacych
  sie miedzy wersjami) sygnatur wewnetrznych funkcji GD,
- masz pelna kontrole - widzisz layout dopiero gdy go wklejasz i mozesz go od
  razu zaznaczyc/przesunac/usunac jak kazda inna wklejona grupe obiektow.

Jesli kiedys zechcesz w pelni automatyczne wstawianie obiektow bez Ctrl+V, trzeba
by uzyc wewnetrznej funkcji `LevelEditorLayer` odpowiedzialnej za parsowanie
stringa obiektow (uzywanej tez przez wbudowany paste) - nie jest ona tu uzyta
celowo, zeby nie opierac moda na niezweryfikowanej sygnaturze.

## Wymagania

- [Geode SDK](https://docs.geode-sdk.org/getting-started/) (mod celuje w Geode
  **v5.x** - nowy system async/coroutine i nieszablonowy `Popup`; jesli masz
  starsze Geode v4.x, zobacz [przewodnik migracji](https://docs.geode-sdk.org/tutorials/migrate-v5/)
  i dostosuj `LevelGenerator.hpp` / `AIPromptPopup.cpp` do starszego API
  (`EventListener<WebTask>` zamiast `async::TaskHolder`)).
- Klucz API do wybranego dostawcy (OpenAI / Anthropic / wlasny serwer).

## Budowanie

```bash
geode build
```

(albo standardowo przez CMake/Geode CLI - patrz [dokumentacja Geode](https://docs.geode-sdk.org/getting-started/create-mod/)).
Plik wynikowy `.geode` wgrywasz do folderu modow Geode (albo `geode build --install`
jesli masz spiety CLI z urzadzeniem/PC).

## Konfiguracja (w grze: Geode -> lista modow -> AI Level Designer -> Settings)

| Ustawienie | Opis |
|---|---|
| **Dostawca AI** | `openai`, `anthropic` albo `custom` |
| **Klucz API** | Twoj klucz (przechowywany lokalnie, NIE szyfrowany) |
| **Model** | nazwa modelu czatu (np. aktualny model tekstowy Twojego dostawcy - sprawdz dokumentacje dostawcy) |
| **Wlasny endpoint** | pelny URL, gdy Dostawca = `custom` (kompatybilny z OpenAI Chat Completions) |
| **Limit obiektow** | twardy limit liczby obiektow przyjmowanych z jednej odpowiedzi (zabezpieczenie) |

> **Koszt:** zapytania do OpenAI/Anthropic/innych platnych API kosztuja wedlug
> cennika dostawcy. To Twoja odpowiedzialnosc - mod nie ma wbudowanych limitow
> kosztow, tylko limit liczby obiektow.

## Tabela ID obiektow - co jest pewne, a co warto zweryfikowac

Caly mapping typ -> ID obiektu znajduje sie w `src/LevelGenerator.hpp` w funkcji
`palette()`:

```cpp
{"block", 1},          // pewne
{"spike", 8},           // pewne
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
```

`block` i `spike` sa pewne. Reszta to powszechnie cytowane w community ID -
w wiekszosci wspolczesnych wersji GD powinny dzialac, ale **przetestuj wygenerowany
layout w grze** zanim zaczniesz budowac na nim cos powazniejszego. Jezeli ktorys
ID renderuje sie jako zly obiekt, podmien wartosc w tabeli.

### Jak samemu zweryfikowac/dodac ID obiektu

Najpewniejszy sposob: w edytorze GD zaznacz interesujacy Cie obiekt, skopiuj go
(Ctrl+C), a nastepnie odczytaj zawartosc schowka (np. tymczasowo zaloguj
`clipboard::read()` w dowolnym mod-debug-hooku, albo wklej do dowolnego edytora
tekstu na PC) - pierwsza liczba po `1,` w wygenerowanym stringu to ID tego
obiektu. W ten sposob mozesz rozbudowac `palette()` o dowolne kolejne obiekty
(bloki dekoracyjne, triggery, itp.) ze 100% pewnoscia dla Twojej wersji gry.

## Struktura projektu

```
AILevelDesigner/
|- mod.json                 - metadane + ustawienia moda
|- CMakeLists.txt
`- src/
   |- main.cpp               - hook EditorUI, przycisk "AI"
   |- AIPromptPopup.hpp/.cpp - UI popupu (prompt, status, przycisk Generuj)
   `- LevelGenerator.hpp     - zapytanie do AI + parsowanie + konwersja na obiekty GD
```

## Znane ograniczenia

- Mod nie waliduje "grywalnosci" wygenerowanego layoutu poza prosta instrukcja
  w prompt-cie systemowym - AI czasem wygeneruje cos niemozliwego do przejscia.
  Zawsze przetestuj level po wklejeniu.
- Brak streamingu / podgladu na zywo - dostajesz cala odpowiedz na raz.
- Klucz API jest trzymany w ustawieniach moda Geode w postaci jawnej (nie
  szyfrowanej) - nie udostepniaj configu/save'ow Geode innym osobom.
