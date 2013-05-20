#include "bdffont.hh"
#include "datafile.hh"
#include "encode.hh"
#include "optimize.hh"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>

static std::string strip_extension(std::string filename)
{
    size_t pos = filename.find_last_of('.');
    
    if (pos == std::string::npos)
    {
        return filename;
    }
    else
    {
        return filename.substr(0, pos);
    }
}

static const char *usage_msg =
    "Usage:\n"
    "   import <bdffile>                Import a .bdf font into a data file.\n"
    "   size <datfile>                  Check the encoded size of the data file.\n"
    "   optimize <datfile>              Perform an optimization pass on the data file.\n"
    "   show_encoded <datfile>          Show the encoded data for debugging.\n"
    "   show_glyph <datfile> <index>    Show the glyph at index.\n";

int main(int argc, char **argv)
{
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++)
        args.push_back(argv[i]);
    
    if (args.size() == 2 && args.at(0) == "import")
    {
        std::string src = args.at(1);
        std::string dest = strip_extension(args.at(1)) + ".dat";
        std::ifstream infile(src);
        
        if (!infile.good())
        {
            std::cerr << "Could not open " << src << std::endl;
            return 1;
        }
        
        std::cout << "Importing " << src << " to " << dest << std::endl;
        
        std::unique_ptr<DataFile> f = LoadBDF(infile);
        
        std::ofstream outfile(dest);
        f->Save(outfile);
        
        std::cout << "Done: " << f->GetGlyphCount() << " unique glyphs." << std::endl;
        return 0;
    }
    else if (args.size() == 2 && args.at(0) == "size")
    {
        std::string src = args.at(1);
        std::ifstream infile(src);
        
        if (!infile.good())
        {
            std::cerr << "Could not open " << src << std::endl;
            return 1;
        }
        
        std::unique_ptr<DataFile> f = DataFile::Load(infile);
        size_t size = get_encoded_size(*f);
        
        std::cout << "Current size of " << src << " is " << size << std::endl;
        return 0;
    }
    else if (args.size() == 2 && args.at(0) == "optimize")
    {
        std::string src = args.at(1);
        std::ifstream infile(src);
        
        if (!infile.good())
        {
            std::cerr << "Could not open " << src << std::endl;
            return 1;
        }
        
        std::unique_ptr<DataFile> f = DataFile::Load(infile);
        size_t oldsize = get_encoded_size(*f);
        optimize(*f);
        size_t newsize = get_encoded_size(*f);
        
        std::ofstream outfile(src);
        f->Save(outfile);
        
        std::cout << "Old size " << oldsize << ", new size " << newsize << std::endl;
        return 0;
    }
    else if (args.size() == 2 && args.at(0) == "show_encoded")
    {
        std::string src = args.at(1);
        std::ifstream infile(src);
        
        if (!infile.good())
        {
            std::cerr << "Could not open " << src << std::endl;
            return 1;
        }
        
        std::unique_ptr<DataFile> f = DataFile::Load(infile);
        std::unique_ptr<encoded_font_t> e = encode_font(*f);
    
        int i = 0;
        for (encoded_font_t::rlestring_t d : e->dictionary)
        {
            std::cout << "Dict " << 4 + i++ << ": ";
            for (uint8_t v : d)
                std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)v << " ";
            std::cout << std::endl;
        }
        
        i = 0;
        for (encoded_font_t::refstring_t g : e->glyphs)
        {
            std::cout << "Glyph " << i++ << ": ";
            for (uint8_t v : g)
                std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)v << " ";
            std::cout << std::endl;
        }
    }
    else if (args.size() == 3 && args.at(0) == "show_glyph")
    {
        std::string src = args.at(1);
        std::ifstream infile(src);
        
        if (!infile.good())
        {
            std::cerr << "Could not open " << src << std::endl;
            return 1;
        }
        
        std::unique_ptr<DataFile> f = DataFile::Load(infile);
        
        size_t index = 0;
        if (args.at(2) == "largest")
        {
            std::unique_ptr<encoded_font_t> e = encode_font(*f);
            size_t maxlen = 0;
            size_t i = 0;
            for (encoded_font_t::refstring_t g : e->glyphs)
            {
                if (g.size() > maxlen)
                {
                    maxlen = g.size();
                    index = i;
                }
                i++;
            }
            
            std::cout << "Index " << index << ", length " << maxlen << std::endl;
        }
        else
        {
            index = strtol(args.at(2).c_str(), nullptr, 0);
        }
            
        if (index < 0 || index >= f->GetGlyphCount())
        {
            std::cerr << "No such glyph " << index << std::endl;
            return 2;
        }
        
        std::cout << f->GlyphToText(index);
        return 0;
    }
    else
    {
        std::cout << usage_msg << std::endl;
        return 1;
    }
}
