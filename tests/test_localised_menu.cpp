#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "launcher.h"

static void Save(const Canvas& c, const std::string& name) {
    std::ofstream f(name, std::ios::binary); f << "P6\n" << c.w << " " << c.h << "\n255\n";
    for (auto p : c.px) { char rgb[] = {char(p >> 16), char(p >> 8), char(p)}; f.write(rgb, 3); }
}
static void TextMatches(const Canvas& c, int x, int y, const std::string& text, int scale, uint32_t colour) {
    Canvas expected; expected.Resize(c.w, c.h); expected.Text(x, y, text, scale, colour);
    const int width = Canvas::TextWidth(text, scale);
    assert(x >= 0 && x + width <= c.w);
    for (int py=y; py<y+7*scale; ++py) for (int px=x; px<x+width; ++px)
        assert((c.px[py*c.w+px] == colour) == (expected.px[py*c.w+px] == colour));
}
int main(int argc, char** argv) {
    // Italian may leave these words unchanged; all other Spanish keys need an Italian entry.
    const std::vector<std::string> identity = {"AUTO", "VOLUME", "NO", "APR", "CONTROLLER", "ACCOUNT", "PASSWORD"};
    int missing = 0;
    for (const auto& entry : UiLang::Spanish()) if (!UiLang::Italian().count(entry.first)) {
        bool allowed = std::find(identity.begin(), identity.end(), entry.first) != identity.end();
        if (!allowed) { std::cerr << "Missing Italian: " << entry.first << "\n"; ++missing; }
    }
    assert(missing == 0);
    for (const char* language : {"en", "es", "it"}) {
        UiLang::Set(language);
        for (const auto& prefix : language == std::string("it") ? UiLang::ItalianPrefixes() : UiLang::SpanishPrefixes()) {
            const auto text = prefix.first + "PLAYER42";
            assert(UiLang::Tr(text) == (UiLang::lang ? prefix.second + "PLAYER42" : text));
        }
        for (auto size : {std::pair<int,int>{1280,720}, {720,1280}, {320,240}}) {
            Canvas c; c.Resize(size.first,size.second);
            Menu menu; menu.items = {{"OTHER VERSION POKEMON", "ON"}, {"SCREEN LAYOUT", "MERGED"}, {"WIDESCREEN WORLD", "OFF"}, {"SAVE BACKUPS", ""}};
            menu.Draw(c,"SETTINGS","");
            // (The glass menus of 4-6 November replaced the pixel layout this compared against: what
            // still matters is that the translated rows fit the screen and are where taps find them.)
            assert(menu.drawnX >= 0 && menu.drawnX + menu.drawnW <= c.w);
            assert(menu.drawnFirstY >= 0 && menu.drawnFirstY < c.h);
            assert(!UiLang::Tr("ON").empty());
            assert(menu.ItemAt(menu.drawnX + menu.drawnW / 2, menu.drawnFirstY + 2) == 0);
            if (argc>1) Save(c,std::string(argv[1])+"/"+language+"-pause-"+std::to_string(c.w)+"x"+std::to_string(c.h)+".ppm");
            Launcher::SettingsPage page; page.group=0;
            page.items={{"wide","WIDESCREEN WORLD",{{"1","ON"},{"0","OFF"}},"1",""}};
            c.Clear(); page.Draw(c,"");
            assert(page.rows.size()==1);
            assert(page.rows[0].second >= 0 && page.rows[0].second < c.h);
            if (argc>1) Save(c,std::string(argv[1])+"/"+language+"-launcher-"+std::to_string(c.w)+"x"+std::to_string(c.h)+".ppm");
        }
    }
    std::cout << "Translation coverage, prefixes, menus fitting the screen and touch rows passed\n";
}
