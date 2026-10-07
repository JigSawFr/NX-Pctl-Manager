// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/table_export.hpp"

#include <algorithm>
#include <borealis/extern/nlohmann/json.hpp>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <utility>

#include "util/paths.hpp"

namespace table_export
{

namespace
{
std::string cell(const std::vector<std::string>& row, size_t i)
{
    return i < row.size() ? row[i] : std::string();
}

bool is_integer(const std::string& s)
{
    size_t i = (!s.empty() && s[0] == '-') ? 1 : 0;
    if (i >= s.size() || s.size() - i > 18) return false;
    for (; i < s.size(); i++)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

std::tm local_now()
{
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    return tmv;
}

// ---------------------------------------------------------------- CSV

std::string csv_field(std::string s, bool numeric)
{
    // A text cell starting like a formula is shown as text by spreadsheets.
    if (!numeric && !s.empty() && (s[0] == '=' || s[0] == '+' || s[0] == '-' || s[0] == '@')) s.insert(0, "'");
    if (s.find_first_of(",\"\r\n") == std::string::npos && (s.empty() || (s.front() != ' ' && s.back() != ' ')))
        return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + "\"";
}

std::string render_csv(const Table& t)
{
    std::string out = "\xEF\xBB\xBF";   // UTF-8 byte-order mark: spreadsheets then read accents right
    for (size_t i = 0; i < t.columns.size(); i++) out += (i ? "," : "") + csv_field(t.columns[i].label, false);
    out += "\r\n";
    for (const auto& row : t.rows) {
        for (size_t i = 0; i < t.columns.size(); i++)
            out += (i ? "," : "") + csv_field(cell(row, i), t.columns[i].numeric);
        out += "\r\n";
    }
    return out;
}

// ---------------------------------------------------------------- JSON

std::string render_json(const Table& t)
{
    nlohmann::ordered_json j;
    j["title"]    = t.title;
    j["subtitle"] = t.subtitle;
    nlohmann::ordered_json rows = nlohmann::ordered_json::array();
    for (const auto& row : t.rows) {
        nlohmann::ordered_json o = nlohmann::ordered_json::object();
        for (size_t i = 0; i < t.columns.size(); i++) {
            const std::string v = cell(row, i);
            if (!t.columns[i].numeric) o[t.columns[i].key] = v;
            else if (is_integer(v)) o[t.columns[i].key] = std::stoll(v);
            else o[t.columns[i].key] = nullptr;
        }
        rows.push_back(o);
    }
    j["rows"] = rows;
    return j.dump(2, ' ', false, nlohmann::ordered_json::error_handler_t::replace) + "\n";
}

// ---------------------------------------------------------------- XLSX

std::string xml_escape(const std::string& s)
{
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default:
                if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') break;   // not allowed in XML
                out += (char)c;
        }
    }
    return out;
}

std::string column_letters(size_t i)
{
    std::string s;
    for (++i; i > 0; i = (i - 1) / 26) s.insert(s.begin(), (char)('A' + (i - 1) % 26));
    return s;
}

// Excel refuses []:*?/\ in a sheet name and more than 31 characters.
std::string sheet_name(const std::string& in)
{
    std::string out;
    size_t chars = 0;
    for (size_t i = 0; i < in.size() && chars < 31; chars++) {
        size_t len = 1;
        const unsigned char c = (unsigned char)in[i];
        if (c >= 0xF0) len = 4;
        else if (c >= 0xE0) len = 3;
        else if (c >= 0xC0) len = 2;
        const std::string ch = in.substr(i, len);
        i += len;
        if (ch.size() == 1 && std::string("[]:*?/\\").find(ch[0]) != std::string::npos) continue;
        out += ch;
    }
    return out.empty() ? "Sheet1" : out;
}

std::string sheet_xml(const Table& t)
{
    std::string x = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                    "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><cols>";
    for (size_t i = 0; i < t.columns.size(); i++)
        x += "<col min=\"" + std::to_string(i + 1) + "\" max=\"" + std::to_string(i + 1) + "\" width=\"" +
             (i == 0 ? "40" : "18") + "\" customWidth=\"1\"/>";
    x += "</cols><sheetData>";

    auto text = [](size_t col, size_t row, const std::string& v, bool bold) {
        return "<c r=\"" + column_letters(col) + std::to_string(row) + "\" t=\"inlineStr\"" + (bold ? " s=\"1\"" : "") +
               "><is><t xml:space=\"preserve\">" + xml_escape(v) + "</t></is></c>";
    };
    // Title, subtitle, a blank row, then the header and the data.
    x += "<row r=\"1\">" + text(0, 1, t.title, true) + "</row>";
    x += "<row r=\"2\">" + text(0, 2, t.subtitle, false) + "</row>";
    x += "<row r=\"4\">";
    for (size_t i = 0; i < t.columns.size(); i++) x += text(i, 4, t.columns[i].label, true);
    x += "</row>";
    size_t r = 5;
    for (const auto& row : t.rows) {
        x += "<row r=\"" + std::to_string(r) + "\">";
        for (size_t i = 0; i < t.columns.size(); i++) {
            const std::string v = cell(row, i);
            if (v.empty()) continue;
            if (t.columns[i].numeric && is_integer(v))
                x += "<c r=\"" + column_letters(i) + std::to_string(r) + "\"><v>" + v + "</v></c>";
            else
                x += text(i, r, v, false);
        }
        x += "</row>";
        r++;
    }
    return x + "</sheetData></worksheet>";
}

uint32_t crc32(const std::string& data)
{
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        ready = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char b : data) crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

void put16(std::string& o, uint32_t v)
{
    o += (char)(v & 0xFF);
    o += (char)((v >> 8) & 0xFF);
}

void put32(std::string& o, uint32_t v)
{
    put16(o, v & 0xFFFF);
    put16(o, v >> 16);
}

// A zip archive with every file stored (no compression), which every XLSX
// reader accepts.
std::string zip(const std::vector<std::pair<std::string, std::string>>& files)
{
    const std::tm now = local_now();
    const uint32_t dos_time = ((uint32_t)now.tm_hour << 11) | ((uint32_t)now.tm_min << 5) | ((uint32_t)now.tm_sec / 2);
    const uint32_t dos_date = ((uint32_t)std::max(0, now.tm_year - 80) << 9) | ((uint32_t)(now.tm_mon + 1) << 5) |
                              (uint32_t)now.tm_mday;
    std::string out, central;
    for (const auto& f : files) {
        const uint32_t crc = crc32(f.second), size = (uint32_t)f.second.size(), offset = (uint32_t)out.size();
        put32(out, 0x04034B50); put16(out, 20); put16(out, 0); put16(out, 0);   // version, flags, stored
        put16(out, dos_time); put16(out, dos_date);
        put32(out, crc); put32(out, size); put32(out, size);
        put16(out, (uint32_t)f.first.size()); put16(out, 0);
        out += f.first + f.second;

        put32(central, 0x02014B50); put16(central, 20); put16(central, 20); put16(central, 0); put16(central, 0);
        put16(central, dos_time); put16(central, dos_date);
        put32(central, crc); put32(central, size); put32(central, size);
        put16(central, (uint32_t)f.first.size()); put16(central, 0); put16(central, 0);   // name, extra, comment
        put16(central, 0); put16(central, 0); put32(central, 0);                           // disk, attributes
        put32(central, offset);
        central += f.first;
    }
    const uint32_t central_offset = (uint32_t)out.size();
    out += central;
    put32(out, 0x06054B50); put16(out, 0); put16(out, 0);
    put16(out, (uint32_t)files.size()); put16(out, (uint32_t)files.size());
    put32(out, (uint32_t)central.size()); put32(out, central_offset); put16(out, 0);
    return out;
}

std::string render_xlsx(const Table& t)
{
    const std::string head = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    const std::string rel  = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/";
    const std::string doc  = "application/vnd.openxmlformats-officedocument.spreadsheetml.";
    return zip({
        { "[Content_Types].xml",
          head + "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                 "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                 "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
                 "<Override PartName=\"/xl/workbook.xml\" ContentType=\"" + doc + "sheet.main+xml\"/>"
                 "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"" + doc + "worksheet+xml\"/>"
                 "<Override PartName=\"/xl/styles.xml\" ContentType=\"" + doc + "styles+xml\"/></Types>" },
        { "_rels/.rels",
          head + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                 "<Relationship Id=\"rId1\" Type=\"" + rel + "officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>" },
        { "xl/workbook.xml",
          head + "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                 "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>"
                 "<sheet name=\"" + xml_escape(sheet_name(t.sheet)) + "\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>" },
        { "xl/_rels/workbook.xml.rels",
          head + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                 "<Relationship Id=\"rId1\" Type=\"" + rel + "worksheet\" Target=\"worksheets/sheet1.xml\"/>"
                 "<Relationship Id=\"rId2\" Type=\"" + rel + "styles\" Target=\"styles.xml\"/></Relationships>" },
        { "xl/styles.xml",
          head + "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                 "<fonts count=\"2\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font>"
                 "<font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>"
                 "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill>"
                 "<fill><patternFill patternType=\"gray125\"/></fill></fills>"
                 "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
                 "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
                 "<cellXfs count=\"2\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
                 "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/></cellXfs>"
                 "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>"
                 "</styleSheet>" },
        { "xl/worksheets/sheet1.xml", sheet_xml(t) },
    });
}

// ---------------------------------------------------------------- PDF

// Windows-1252 (the standard fonts' WinAnsiEncoding) for one code point.
char winansi(uint32_t cp)
{
    if (cp < 0x20) return ' ';
    if (cp < 0x7F || (cp >= 0xA0 && cp <= 0xFF)) return (char)cp;
    static const struct { uint16_t cp; uint8_t byte; } extra[] = {
        { 0x20AC, 0x80 }, { 0x201A, 0x82 }, { 0x0192, 0x83 }, { 0x201E, 0x84 }, { 0x2026, 0x85 }, { 0x2020, 0x86 },
        { 0x2021, 0x87 }, { 0x02C6, 0x88 }, { 0x2030, 0x89 }, { 0x0160, 0x8A }, { 0x2039, 0x8B }, { 0x0152, 0x8C },
        { 0x017D, 0x8E }, { 0x2018, 0x91 }, { 0x2019, 0x92 }, { 0x201C, 0x93 }, { 0x201D, 0x94 }, { 0x2022, 0x95 },
        { 0x2013, 0x96 }, { 0x2014, 0x97 }, { 0x02DC, 0x98 }, { 0x2122, 0x99 }, { 0x0161, 0x9A }, { 0x203A, 0x9B },
        { 0x0153, 0x9C }, { 0x017E, 0x9E }, { 0x0178, 0x9F }, { 0x202F, 0xA0 }, { 0x2009, 0x20 },
    };
    for (const auto& e : extra)
        if (e.cp == cp) return (char)e.byte;
    return '?';
}

std::string to_winansi(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        size_t len = 1;
        uint32_t cp = c;
        if (c >= 0xF0 && c < 0xF8) { len = 4; cp = c & 0x07; }
        else if (c >= 0xE0) { len = 3; cp = c & 0x0F; }
        else if (c >= 0xC0) { len = 2; cp = c & 0x1F; }
        else if (c >= 0x80) { out += '?'; i++; continue; }   // a stray continuation byte
        bool ok = i + len <= s.size();
        for (size_t k = 1; k < len && ok; k++) {
            const unsigned char cc = (unsigned char)s[i + k];
            ok = (cc & 0xC0) == 0x80;
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { out += '?'; i++; continue; }
        out += winansi(cp);
        i += len;
    }
    return out;
}

// Advance widths (1/1000 em) of Helvetica for 0x20..0x7E; other bytes use 556.
const uint16_t HELVETICA[95] = {
    278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278, 556, 556, 556, 556,
    556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556, 1015, 667, 667, 722, 722, 667, 611, 778,
    722, 278, 500, 667, 556, 833, 722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278,
    278, 278, 469, 556, 333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,
    556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584,
};

// Width in points of WinAnsi text; bold is about 8 % wider.
double text_width(const std::string& s, double size, bool bold)
{
    double w = 0;
    for (unsigned char c : s) w += (c >= 0x20 && c <= 0x7E) ? HELVETICA[c - 0x20] : (c == 0x85 ? 1000 : 556);
    return w * size / 1000.0 * (bold ? 1.08 : 1.0);
}

// Widths are compared with this much slack: a column is sized from the very
// same measurement, give or take rounding.
const double SLACK = 0.01;

// Cuts WinAnsi text to `max` points, ending with "…".
std::string fit(std::string s, double max, double size, bool bold)
{
    max += SLACK;
    if (text_width(s, size, bold) <= max) return s;
    while (!s.empty() && text_width(s + "\x85", size, bold) > max) s.pop_back();
    return s + "\x85";
}

std::string pdf_string(const std::string& winansi_text)
{
    std::string out = "(";
    for (unsigned char c : winansi_text) {
        if (c == '(' || c == ')' || c == '\\') {
            out += '\\';
            out += (char)c;
        } else if (c < 0x20 || c >= 0x7F) {
            char oct[8];
            std::snprintf(oct, sizeof(oct), "\\%03o", c);
            out += oct;
        } else {
            out += (char)c;
        }
    }
    return out + ")";
}

std::string num(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

std::string text_at(double x, double y, const std::string& winansi_text, double size, bool bold)
{
    return "BT /" + std::string(bold ? "F2 " : "F1 ") + num(size) + " Tf " + num(x) + " " + num(y) + " Td " +
           pdf_string(winansi_text) + " Tj ET\n";
}

// The narrowest width that shows a header label on at most two lines.
double header_width(const std::string& s, double size)
{
    double best = text_width(s, size, true);
    for (size_t i = s.find(' '); i != std::string::npos; i = s.find(' ', i + 1))
        best = std::min(best, std::max(text_width(s.substr(0, i), size, true), text_width(s.substr(i + 1), size, true)));
    return best;
}

// A header label on one line, or two split at a space (the longest first
// line that leaves a second one that fits too).
std::vector<std::string> header_lines(const std::string& s, double max, double size)
{
    max += SLACK;
    if (text_width(s, size, true) <= max) return { s };
    size_t best = std::string::npos;
    for (size_t i = s.find(' '); i != std::string::npos; i = s.find(' ', i + 1))
        if (text_width(s.substr(0, i), size, true) <= max && text_width(s.substr(i + 1), size, true) <= max) best = i;
    if (best == std::string::npos) return { fit(s, max, size, true) };
    return { s.substr(0, best), s.substr(best + 1) };
}

// UTF-16BE hex string for the document information (PDFDocEncoding has no em dash).
std::string pdf_text_string(const std::string& utf8)
{
    std::string out = "<FEFF";
    char hex[8];
    for (size_t i = 0; i < utf8.size();) {
        const unsigned char c = (unsigned char)utf8[i];
        size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        uint32_t cp = len == 1 ? c : (c & (0x7F >> len));
        if (i + len > utf8.size()) break;
        for (size_t k = 1; k < len; k++) cp = (cp << 6) | ((unsigned char)utf8[i + k] & 0x3F);
        i += len;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            std::snprintf(hex, sizeof(hex), "%04X", (unsigned)(0xD800 + (cp >> 10)));
            out += hex;
            cp = 0xDC00 + (cp & 0x3FF);
        }
        std::snprintf(hex, sizeof(hex), "%04X", (unsigned)(cp & 0xFFFF));
        out += hex;
    }
    return out + ">";
}

std::string render_pdf(const Table& t)
{
    // A4 landscape, in points.
    const double W = 842, H = 595, M = 36, PAD = 4;
    const double FONT = 8.5, HEAD_FONT = 8, LINE = 13;
    const size_t ncol = t.columns.size();

    // Everything in WinAnsi first, so widths are measured on what is drawn.
    std::vector<std::string> labels;
    for (const auto& c : t.columns) labels.push_back(to_winansi(c.label));
    std::vector<std::vector<std::string>> rows;
    for (const auto& r : t.rows) {
        std::vector<std::string> out;
        for (size_t i = 0; i < ncol; i++) out.push_back(to_winansi(cell(r, i)));
        rows.push_back(out);
    }

    // Column widths: what the content needs (a header may take two lines),
    // then the first column (the name) absorbs what is left or missing.
    std::vector<double> width(ncol, 0);
    for (size_t i = 0; i < ncol; i++) {
        width[i] = header_width(labels[i], HEAD_FONT);
        for (const auto& r : rows) width[i] = std::max(width[i], text_width(r[i], FONT, false));
        width[i] += 2 * PAD;
    }
    double used = 0;
    for (size_t i = 1; i < ncol; i++) used += width[i];
    if (ncol > 0) width[0] = std::max(80.0, W - 2 * M - used);
    std::vector<double> x(ncol + 1, M);
    for (size_t i = 0; i < ncol; i++) x[i + 1] = x[i] + width[i];

    const double table_top = H - M - 52;   // below the title and subtitle
    const double header_h  = 2 * (HEAD_FONT + 2) + 6;
    const size_t per_page  = std::max<size_t>(1, (size_t)((table_top - header_h - M - 10) / LINE));
    const size_t pages     = std::max<size_t>(1, (rows.size() + per_page - 1) / per_page);
    const std::string title = to_winansi(t.title), subtitle = to_winansi(t.subtitle);

    std::vector<std::string> contents;
    for (size_t p = 0; p < pages; p++) {
        std::string c;
        c += text_at(M, H - M - 16, fit(title, W - 2 * M, 16, true), 16, true);
        c += "0.4 g\n" + text_at(M, H - M - 32, fit(subtitle, W - 2 * M, 9, false), 9, false) + "0 g\n";
        // Header (one or two lines), then a rule under it.
        for (size_t i = 0; i < ncol; i++) {
            const auto lines = header_lines(labels[i], width[i] - 2 * PAD, HEAD_FONT);
            for (size_t k = 0; k < lines.size(); k++) {
                const double y = table_top - (HEAD_FONT + 2) * (k + 1) - (lines.size() == 1 ? HEAD_FONT + 2 : 0);
                const double tx = t.columns[i].numeric ? x[i + 1] - PAD - text_width(lines[k], HEAD_FONT, true) : x[i] + PAD;
                c += text_at(tx, y, lines[k], HEAD_FONT, true);
            }
        }
        const double rule = table_top - header_h;
        c += "0.6 w " + num(M) + " " + num(rule) + " m " + num(x[ncol]) + " " + num(rule) + " l S\n";
        // Rows, with a light band on every other one.
        const size_t first = p * per_page, last = std::min(rows.size(), first + per_page);
        for (size_t r = first; r < last; r++) {
            const double base = rule - LINE * (double)(r - first + 1);
            if ((r - first) % 2 == 1)
                c += "0.94 g " + num(M) + " " + num(base - 3.5) + " " + num(x[ncol] - M) + " " + num(LINE) + " re f 0 g\n";
            for (size_t i = 0; i < ncol; i++) {
                const std::string v = fit(rows[r][i], width[i] - 2 * PAD, FONT, false);
                if (v.empty()) continue;
                const double tx = t.columns[i].numeric ? x[i + 1] - PAD - text_width(v, FONT, false) : x[i] + PAD;
                c += text_at(tx, base, v, FONT, false);
            }
        }
        const std::string page = std::to_string(p + 1) + " / " + std::to_string(pages);
        c += "0.4 g\n" + text_at(W - M - text_width(page, 8, false), M / 2, page, 8, false) + "0 g\n";
        contents.push_back(c);
    }

    // Objects: 1 catalog, 2 page tree, 3-4 fonts, 5 info, then content + page per page.
    std::vector<std::string> objs(5);
    std::string kids;
    for (size_t p = 0; p < pages; p++) {
        const size_t content = objs.size() + 1, page = content + 1;
        objs.push_back("<< /Length " + std::to_string(contents[p].size()) + " >>\nstream\n" + contents[p] + "\nendstream");
        objs.push_back("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 842 595] /Resources << /Font << /F1 3 0 R "
                       "/F2 4 0 R >> >> /Contents " + std::to_string(content) + " 0 R >>");
        kids += std::to_string(page) + " 0 R ";
    }
    objs[0] = "<< /Type /Catalog /Pages 2 0 R >>";
    objs[1] = "<< /Type /Pages /Kids [ " + kids + "] /Count " + std::to_string(pages) + " >>";
    objs[2] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
    objs[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>";
    objs[4] = "<< /Title " + pdf_text_string(t.title) + " /Producer (PlayGuard) >>";

    std::string out = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    std::vector<size_t> offsets;
    for (size_t i = 0; i < objs.size(); i++) {
        offsets.push_back(out.size());
        out += std::to_string(i + 1) + " 0 obj\n" + objs[i] + "\nendobj\n";
    }
    const size_t xref = out.size();
    out += "xref\n0 " + std::to_string(objs.size() + 1) + "\n0000000000 65535 f \n";
    for (size_t off : offsets) {
        char entry[24];
        std::snprintf(entry, sizeof(entry), "%010zu 00000 n \n", off);
        out += entry;
    }
    out += "trailer\n<< /Size " + std::to_string(objs.size() + 1) + " /Root 1 0 R /Info 5 0 R >>\nstartxref\n" +
           std::to_string(xref) + "\n%%EOF\n";
    return out;
}
}   // namespace

const char* extension(Format f)
{
    switch (f) {
        case Format::Csv:  return "csv";
        case Format::Json: return "json";
        case Format::Xlsx: return "xlsx";
        case Format::Pdf:  return "pdf";
    }
    return "txt";
}

std::string render(const Table& t, Format f)
{
    switch (f) {
        case Format::Csv:  return render_csv(t);
        case Format::Json: return render_json(t);
        case Format::Xlsx: return render_xlsx(t);
        case Format::Pdf:  return render_pdf(t);
    }
    return "";
}

std::string save(const Table& t, Format f, const std::string& dir, const std::string& base, std::string* error)
{
    const std::tm now = local_now();
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &now);
    const std::string content = render(t, f);
    for (unsigned i = 0; i < 100; ++i) {
        std::string path = dir + "/" + base + "_" + stamp;
        if (i > 0) path += "_" + std::to_string(i);
        path += std::string(".") + extension(f);
        std::string existing;
        if (paths::read_file(path, existing)) continue;
        return paths::atomic_write(path, content, error) ? path : "";
    }
    if (error) *error = "No free file name";
    return "";
}

}   // namespace table_export
