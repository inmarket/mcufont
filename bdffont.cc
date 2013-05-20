#include "bdffont.hh"
#include <sstream>
#include <string>
#include <cctype>
#include <stdexcept>

static std::string toupper(const std::string &input)
{
    std::string result;
    for (char c: input) result.push_back(toupper(c));
    return result;
}

static int hextoint(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw std::domain_error("Hex digit not in range");
}

static void parse_fontinfo(std::istream &file, DataFile::fontinfo_t &fontinfo)
{
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream s(line);
        std::string tag;
        s >> tag;
        tag = toupper(tag);
        
        if (tag == "FONT")
        {
            while (isspace(s.peek())) s.get();
            std::getline(s, fontinfo.name);
        }
        else if (tag == "FONTBOUNDINGBOX")
        {
            int x, y;
            s >> fontinfo.max_width >> fontinfo.max_height;
            s >> x >> y;
            fontinfo.baseline_x = - x;
            fontinfo.baseline_y = fontinfo.max_height + y;
        }
        else if (tag == "STARTCHAR")
        {
            break;
        }
    }
}

static bool parse_glyph(std::istream &file, DataFile::glyphentry_t &glyph,
                        const DataFile::fontinfo_t &fontinfo)
{
    glyph.chars.clear();
    glyph.width = 0;
    
    // Initialize the character contents to all 0 with proper size.
    glyph.data.clear();
    glyph.data.resize(fontinfo.max_width * fontinfo.max_height, 0);
    
    int bbx_w = fontinfo.max_width;
    int bbx_h = fontinfo.max_height;
    int bbx_x = - fontinfo.baseline_x;
    int bbx_y = fontinfo.baseline_y - fontinfo.max_height;
    
    // Read glyph metadata
    std::string line;
    std::string tag;
    while (std::getline(file, line))
    {
        std::istringstream s(line);
        s >> tag;
        tag = toupper(tag);
        
        if (tag == "ENCODING")
        {
            int c;
            s >> c;
            glyph.chars.push_back(c);
        }
        else if (tag == "DWIDTH")
        {
            s >> glyph.width;
        }
        else if (tag == "BBX")
        {
            s >> bbx_w >> bbx_h >> bbx_x >> bbx_y;
        }
        else if (tag == "BITMAP")
        {
            break;
        }
    }
    
    if (tag != "BITMAP")
        return false;
    
    // Read glyph bits
    int x0 = fontinfo.baseline_x + bbx_x;
    int y = fontinfo.baseline_y - bbx_y - bbx_h;
    for (int i = 0; i < bbx_h; i++)
    {
        std::getline(file, line);
        line = toupper(line);
        
        for (int x = 0; x < bbx_w; x++)
        {
            int nibble = hextoint(line.at(x / 4));
            bool bit = nibble & (8 >> (x % 4));
            glyph.data.at(y * fontinfo.max_width + x0 + x) = bit;
        }
        
        y++;
    }
    
    std::getline(file, line);
    line = toupper(line);
    if (line.compare(0, 7, "ENDCHAR") == 0)
        return true;
    else
        return false;
}

static void eliminate_duplicates(std::vector<DataFile::glyphentry_t> &glyphtable)
{
    for (size_t i = 0; i + 1 < glyphtable.size(); i++)
    {
        for (size_t j = i + 1; j < glyphtable.size(); j++)
        {
            if (glyphtable.at(i).data == glyphtable.at(j).data &&
                glyphtable.at(i).width == glyphtable.at(j).width)
            {
                for (int c : glyphtable.at(j).chars)
                    glyphtable.at(i).chars.push_back(c);
                
                glyphtable.erase(glyphtable.begin() + j);
                j--;
            }
        }
    }
}

std::unique_ptr<DataFile> LoadBDF(std::istream &file)
{
    DataFile::fontinfo_t fontinfo = {};
    std::vector<DataFile::glyphentry_t> glyphtable;
    std::vector<DataFile::dictentry_t> dictionary;
    
    parse_fontinfo(file, fontinfo);
    
    while (file)
    {
        DataFile::glyphentry_t glyph = {};
        if (parse_glyph(file, glyph, fontinfo))
            glyphtable.push_back(glyph);
    }
    
    eliminate_duplicates(glyphtable);
    
    std::unique_ptr<DataFile> result(new DataFile(
        dictionary, glyphtable, fontinfo));
    return result;
}
