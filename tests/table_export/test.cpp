// Host tests for source/util/table_export.cpp: the four export formats. The
// XLSX and PDF structure is checked with independent code (a bitwise CRC-32,
// a cross-reference walk), not with the writer's own helpers.
#include <borealis/extern/nlohmann/json.hpp>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/table_export.hpp"

using table_export::Format;

static table_export::Table sample(size_t extra_rows = 0)
{
    table_export::Table t;
    t.title    = "PlayGuard — Activité";
    t.subtitle = "Exporté le 2026-10-07 18:40";
    t.sheet    = "Activité: [jeux]";
    t.columns  = { { "game", "Jeu", false }, { "today_min", "Aujourd'hui (min)", true },
                   { "week_min", "7 derniers jours (min)", true }, { "last_played", "Dernière partie", false } };
    t.rows = {
        { "Star Kart, \"Deluxe\"", "45", "310", "2026-10-07 17:31" },
        { "Pokémon <Écarlate> & co", "0", "", "" },
        { "=cmd()", "", "30", "2026-10-02 12:00" },
        { "ゼルダの伝説", "15", "15", "" },
    };
    for (size_t i = 0; i < extra_rows; i++) t.rows.push_back({ "Game " + std::to_string(i), "1", "2", "" });
    return t;
}

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

static uint32_t u16(const std::string& s, size_t at) { return (uint8_t)s[at] | ((uint8_t)s[at + 1] << 8); }
static uint32_t u32(const std::string& s, size_t at) { return u16(s, at) | (u16(s, at + 2) << 16); }

static uint32_t bitwise_crc(const std::string& data)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char b : data) {
        crc ^= b;
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

static void test_csv()
{
    const std::string csv = table_export::render(sample(), Format::Csv);
    assert(csv.compare(0, 3, "\xEF\xBB\xBF") == 0);
    assert(has(csv, "Jeu,Aujourd'hui (min),7 derniers jours (min),Dernière partie\r\n"));
    assert(has(csv, "\"Star Kart, \"\"Deluxe\"\"\",45,310,2026-10-07 17:31\r\n"));
    assert(has(csv, "Pokémon <Écarlate> & co,0,,\r\n"));
    assert(has(csv, "'=cmd(),,30,"));   // a formula-looking name stays text
    assert(has(csv, "ゼルダの伝説,15,15,\r\n"));

    // Tab and carriage return start a formula too; a numeric column holds an
    // integer as is (a negative one included), anything else as guarded text.
    table_export::Table t;
    t.columns = { { "name", "Name", false }, { "n", "N", true } };
    t.rows    = { { "\tcmd", "-5" }, { "\rcmd", "=1+1" }, { "-x", "12abc" }, { "@x", "-" } };
    const std::string g = table_export::render(t, Format::Csv);
    assert(has(g, "'\tcmd,-5\r\n"));
    assert(has(g, "\"'\rcmd\",'=1+1\r\n"));
    assert(has(g, "'-x,12abc\r\n"));
    assert(has(g, "'@x,'-\r\n"));
}

static void test_json()
{
    const auto j = nlohmann::json::parse(table_export::render(sample(), Format::Json));
    assert(j["title"] == "PlayGuard — Activité");
    assert(j["rows"].size() == 4);
    assert(j["rows"][0]["game"] == "Star Kart, \"Deluxe\"");
    assert(j["rows"][0]["today_min"].is_number_integer() && j["rows"][0]["today_min"] == 45);
    assert(j["rows"][1]["week_min"].is_null());
    assert(j["rows"][1]["last_played"] == "");
    assert(j["rows"][3]["game"] == "ゼルダの伝説");
    // Columns keep their order.
    assert(j["rows"][0].begin().key() == "game");
}

// Walks the central directory; returns the stored files by name.
static std::vector<std::pair<std::string, std::string>> unzip(const std::string& z)
{
    std::vector<std::pair<std::string, std::string>> files;
    assert(z.size() > 22);
    const size_t eocd = z.size() - 22;
    assert(u32(z, eocd) == 0x06054B50);
    const uint32_t entries = u16(z, eocd + 10), cd_size = u32(z, eocd + 12), cd_off = u32(z, eocd + 16);
    assert(cd_off + cd_size == eocd);
    size_t p = cd_off;
    for (uint32_t i = 0; i < entries; i++) {
        assert(u32(z, p) == 0x02014B50);
        assert(u16(z, p + 10) == 0);   // stored
        const uint32_t crc = u32(z, p + 16), csize = u32(z, p + 20), usize = u32(z, p + 24);
        const uint32_t nlen = u16(z, p + 28), xlen = u16(z, p + 30), clen = u16(z, p + 32), local = u32(z, p + 42);
        const std::string name = z.substr(p + 46, nlen);
        assert(csize == usize);
        assert(u32(z, local) == 0x04034B50 && z.substr(local + 30, nlen) == name);
        assert(u32(z, local + 14) == crc && u32(z, local + 18) == csize);
        const std::string data = z.substr(local + 30 + nlen + u16(z, local + 28), csize);
        assert(bitwise_crc(data) == crc);
        files.push_back({ name, data });
        p += 46 + nlen + xlen + clen;
    }
    assert(p == eocd);
    return files;
}

static std::string part(const std::vector<std::pair<std::string, std::string>>& files, const std::string& name)
{
    for (const auto& f : files)
        if (f.first == name) return f.second;
    assert(!"missing part");
    return "";
}

static void test_xlsx()
{
    const auto files = unzip(table_export::render(sample(), Format::Xlsx));
    assert(files.size() == 6 && files[0].first == "[Content_Types].xml");
    const std::string sheet = part(files, "xl/worksheets/sheet1.xml");
    assert(has(sheet, "<c r=\"A1\" t=\"inlineStr\" s=\"1\"><is><t xml:space=\"preserve\">PlayGuard — Activité</t>"));
    assert(has(sheet, "<c r=\"B4\" t=\"inlineStr\" s=\"1\"><is><t xml:space=\"preserve\">Aujourd'hui (min)</t>"));
    assert(has(sheet, "<c r=\"B5\"><v>45</v></c><c r=\"C5\"><v>310</v></c>"));
    assert(has(sheet, "Star Kart, &quot;Deluxe&quot;"));
    assert(has(sheet, "Pokémon &lt;Écarlate&gt; &amp; co"));
    assert(!has(sheet, "r=\"C6\""));   // empty cells are left out
    assert(has(part(files, "xl/workbook.xml"), "<sheet name=\"Activité jeux\""));   // []: removed
    assert(has(part(files, "xl/_rels/workbook.xml.rels"), "Target=\"worksheets/sheet1.xml\""));
    assert(has(part(files, "_rels/.rels"), "Target=\"xl/workbook.xml\""));
    assert(has(part(files, "xl/styles.xml"), "<b/>"));

    // Invalid UTF-8 becomes U+FFFD, characters XML 1.0 refuses are left out.
    table_export::Table t;
    t.title   = std::string("a\x01" "b\x80" "c\xC3(d\xEF\xBF\xBE" "e\xED\xA0\x80" "f\xC0\xAF" "g\tok\xC3");
    t.sheet   = "''" + std::string(29, 'x') + "é'";
    t.columns = { { "n", "N", true } };
    const auto bad = unzip(table_export::render(t, Format::Xlsx));
    const std::string fffd = "\xEF\xBF\xBD";
    assert(has(part(bad, "xl/worksheets/sheet1.xml"),
               ">ab" + fffd + "c" + fffd + "(de" + fffd + fffd + fffd + "f" + fffd + fffd + "g\tok" + fffd + "</t>"));
    // 31 characters, not 31 bytes, the é kept whole; no apostrophe at either end.
    assert(has(part(bad, "xl/workbook.xml"), "<sheet name=\"" + std::string(29, 'x') + "é\""));
    t.sheet = std::string(30, 'y') + "é";   // 31 characters, 32 bytes
    assert(has(part(unzip(table_export::render(t, Format::Xlsx)), "xl/workbook.xml"), "name=\"" + t.sheet + "\""));
    t.sheet = std::string(30, 'y') + "\xF0\x9F\x8E\xAE";   // a 2-unit emoji does not fit in the last unit
    assert(has(part(unzip(table_export::render(t, Format::Xlsx)), "xl/workbook.xml"),
               "name=\"" + std::string(30, 'y') + "\""));
    t.sheet = std::string(3, '\'');
    assert(has(part(unzip(table_export::render(t, Format::Xlsx)), "xl/workbook.xml"), "name=\"Sheet1\""));
}

static void test_pdf()
{
    const std::string pdf = table_export::render(sample(), Format::Pdf);
    assert(pdf.compare(0, 9, "%PDF-1.4\n") == 0);
    assert(pdf.size() > 6 && pdf.compare(pdf.size() - 6, 6, "%%EOF\n") == 0);

    // startxref points at the table; every entry points at its object.
    const size_t sx = pdf.rfind("startxref\n");
    const size_t xref = std::stoul(pdf.substr(sx + 10));
    assert(pdf.compare(xref, 5, "xref\n") == 0);
    const size_t count = std::stoul(pdf.substr(xref + 7));
    size_t entry = pdf.find('\n', xref + 5) + 1 + 20;   // after the free entry
    for (size_t n = 1; n < count; n++, entry += 20) {
        assert(pdf[entry + 18] == ' ' && pdf[entry + 19] == '\n');   // 20-byte entries
        const size_t off = std::stoul(pdf.substr(entry, 10));
        assert(pdf.compare(off, std::to_string(n).size() + 6, std::to_string(n) + " 0 obj") == 0);
    }
    // Stream lengths match.
    for (size_t at = pdf.find("/Length "); at != std::string::npos; at = pdf.find("/Length ", at + 1)) {
        const size_t len = std::stoul(pdf.substr(at + 8));
        const size_t start = pdf.find("stream\n", at) + 7;
        assert(pdf.compare(start + len, 10, "\nendstream") == 0);
    }
    assert(has(pdf, "/Count 1 "));
    assert(has(pdf, "/Title <FEFF0050006C0061007900470075006100720064002020140020"));   // UTF-16: "PlayGuard — "
    assert(has(pdf, "(PlayGuard \\227 Activit\\351)"));   // WinAnsi: em dash, é
    assert(has(pdf, "(Pok\\351mon <\\311carlate> & co)"));
    assert(has(pdf, "(" + std::string(6, '?') + ")"));   // no Japanese in Helvetica

    // Many rows: several pages, each with its header.
    const std::string longer = table_export::render(sample(80), Format::Pdf);
    assert(has(longer, "/Count 3 ") && has(longer, "(3 / 3)"));
}

static void test_save()
{
    char dir[] = "/tmp/playguard_export_XXXXXX";
    assert(mkdtemp(dir) != nullptr);
    std::string err;
    const std::string a = table_export::save(sample(), Format::Csv, std::string(dir) + "/exports", "activity", &err);
    const std::string b = table_export::save(sample(), Format::Csv, std::string(dir) + "/exports", "activity", &err);
    assert(!a.empty() && !b.empty() && a != b && err.empty());
    assert(a.size() > 4 && a.compare(a.size() - 4, 4, ".csv") == 0 && has(a, "/exports/activity_"));
    assert(std::string(table_export::extension(Format::Xlsx)) == "xlsx");
    const std::string cleanup = std::string("rm -rf '") + dir + "'";
    assert(std::system(cleanup.c_str()) == 0);
}

int main()
{
    test_csv();
    test_json();
    test_xlsx();
    test_pdf();
    test_save();
    std::puts("table export CSV, JSON, XLSX and PDF assertions passed");
    return 0;
}
