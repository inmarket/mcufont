#include "optimize.hh"
#include "encode.hh"
#include <random>
#include <iostream>

typedef std::mt19937 rnd_t;

// Select a random substring among all the glyphs in the datafile.
std::unique_ptr<DataFile::bitstring_t> random_substring(const DataFile &datafile, rnd_t &rnd)
{
    std::uniform_int_distribution<size_t> dist1(0, datafile.GetGlyphCount() - 1);
    size_t index = dist1(rnd);
    
    const DataFile::bitstring_t &bitstring = datafile.GetGlyphEntry(index).data;
    
    std::uniform_int_distribution<size_t> dist2(2, bitstring.size());
    size_t length = dist2(rnd);
    
    std::uniform_int_distribution<size_t> dist3(0, bitstring.size() - length);
    size_t start = dist3(rnd);
    
    std::unique_ptr<DataFile::bitstring_t> result;
    result.reset(new DataFile::bitstring_t(bitstring.begin() + start,
                                       bitstring.begin() + start + length));
    return result;
}

// Try to replace the worst dictionary entry with a better one.
void optimize_worst(DataFile &datafile, size_t &size, rnd_t &rnd, bool verbose)
{
    DataFile trial = datafile;
    size_t worst = trial.GetLowScoreIndex();
    DataFile::dictentry_t d = trial.GetDictionaryEntry(worst);
    d.replacement = *random_substring(datafile, rnd);
    trial.SetDictionaryEntry(worst, d);
    
    size_t newsize = get_encoded_size(trial);
    
    if (newsize < size)
    {
        d.score = size - newsize;
        datafile.SetDictionaryEntry(worst, d);
        size = newsize;
        
        if (verbose)
            std::cout << "optimize_worst: replaced " << worst
                      << " score " << d.score << std::endl;
    }
}

// Try to replace random dictionary entry with another one.
void optimize_any(DataFile &datafile, size_t &size, rnd_t &rnd, bool verbose)
{
    DataFile trial = datafile;
    std::uniform_int_distribution<size_t> dist(0, DataFile::dictionarysize - 1);
    size_t index = dist(rnd);
    DataFile::dictentry_t d = trial.GetDictionaryEntry(index);
    d.replacement = *random_substring(datafile, rnd);
    trial.SetDictionaryEntry(index, d);
    
    size_t newsize = get_encoded_size(trial);
    
    if (newsize < size)
    {
        d.score = size - newsize;
        datafile.SetDictionaryEntry(index, d);
        size = newsize;
        
        if (verbose)
            std::cout << "optimize_any: replaced " << index
                      << " score " << d.score << std::endl;
    }
}

// Try to append or prepend random dictionary entry.
void optimize_expand(DataFile &datafile, size_t &size, rnd_t &rnd, bool verbose)
{
    DataFile trial = datafile;
    std::uniform_int_distribution<size_t> dist1(0, DataFile::dictionarysize - 1);
    size_t index = dist1(rnd);
    DataFile::dictentry_t d = trial.GetDictionaryEntry(index);
    
    std::uniform_int_distribution<size_t> dist3(1, 10);
    size_t count = dist3(rnd);
    
    for (size_t i = 0; i < count; i++)
    {
        std::uniform_int_distribution<size_t> dist2(0, 1);
        bool bit = dist2(rnd);
        bool prepend = dist2(rnd);
        
        if (prepend)
        {
            d.replacement.insert(d.replacement.begin(), bit);
        }
        else
        {
            d.replacement.push_back(bit);
        }
    }
    
    trial.SetDictionaryEntry(index, d);
    
    size_t newsize = get_encoded_size(trial);
    
    if (newsize < size)
    {
        d.score = size - newsize;
        datafile.SetDictionaryEntry(index, d);
        size = newsize;
        
        if (verbose)
            std::cout << "optimize_expand: expanded " << index
                      << " by " << count << " bits, score " << d.score << std::endl;
    }
}

// Try to trim random dictionary entry.
void optimize_trim(DataFile &datafile, size_t &size, rnd_t &rnd, bool verbose)
{
    DataFile trial = datafile;
    std::uniform_int_distribution<size_t> dist1(0, DataFile::dictionarysize - 1);
    size_t index = dist1(rnd);
    DataFile::dictentry_t d = trial.GetDictionaryEntry(index);
    
    if (d.replacement.size() <= 2) return;
    
    std::uniform_int_distribution<size_t> dist2(0, std::min((int)d.replacement.size() / 2, 5));
    size_t start = dist2(rnd);
    size_t end = dist2(rnd);
    
    if (start)
    {
        d.replacement.erase(d.replacement.begin(), d.replacement.begin() + start);
    }
    
    if (end)
    {
        d.replacement.erase(d.replacement.end() - end, d.replacement.end());
    }
    
    trial.SetDictionaryEntry(index, d);
    
    size_t newsize = get_encoded_size(trial);
    
    if (newsize < size)
    {
        d.score = size - newsize;
        datafile.SetDictionaryEntry(index, d);
        size = newsize;
        
        if (verbose)
            std::cout << "optimize_trim: trimmed " << index
                      << " by " << start << " bits from start and "
                      << end << " bits from end, score " << d.score << std::endl;
    }
}

// Discard a few dictionary entries and try to incrementally find better ones.
void optimize_bigjump(DataFile &datafile, size_t &size, rnd_t &rnd, bool verbose)
{
    DataFile trial = datafile;
    std::uniform_int_distribution<size_t> dist(0, DataFile::dictionarysize - 1);
    std::uniform_int_distribution<size_t> dist2(1, 20);
    
    int dropcount = dist2(rnd);
    for (int i = 0; i < dropcount; i++)
    {
        size_t index = dist(rnd);
        DataFile::dictentry_t d = trial.GetDictionaryEntry(index);
        d.replacement.clear();
        d.score = 0;
        trial.SetDictionaryEntry(index, d);
    }
    
    size_t newsize = get_encoded_size(trial);
    
    for (size_t i = 0; i < 100; i++)
    {
        optimize_worst(trial, newsize, rnd, false);
        optimize_any(trial, newsize, rnd, false);
        optimize_expand(trial, newsize, rnd, false);
    }
    
    if (newsize < size)
    {
        if (verbose)
            std::cout << "optimize_bigjump: replaced " << dropcount
                      << " entries, score " << (size - newsize) << std::endl;
        
        datafile = trial;
        size = newsize;
    }
}

// Go through all the dictionary entries and check what it costs to remove
// them. Removes any entries with negative or zero score.
void update_scores(DataFile &datafile, bool verbose)
{
    size_t oldsize = get_encoded_size(datafile);
    
    for (size_t i = 0; i < DataFile::dictionarysize; i++)
    {
        DataFile trial = datafile;
        DataFile::dictentry_t dummy = {};
        trial.SetDictionaryEntry(i, dummy);
        size_t newsize = get_encoded_size(trial);
        
        DataFile::dictentry_t d = datafile.GetDictionaryEntry(i);
        d.score = newsize - oldsize;
        
        if (d.score > 0)
        {
            datafile.SetDictionaryEntry(i, d);
        }
        else
        {
            datafile.SetDictionaryEntry(i, dummy);
            
            if (verbose && d.replacement.size() != 0)
                std::cout << "update_scores: dropped " << i
                        << " score " << -d.score << std::endl;
        }
    }
}

void optimize(DataFile &datafile, size_t iterations)
{
    bool verbose = true;
    rnd_t rnd(datafile.GetSeed());
    
    update_scores(datafile, verbose);
    
    size_t size = get_encoded_size(datafile);
    
    for (size_t i = 0; i < iterations; i++)
    {
        optimize_worst(datafile, size, rnd, verbose);
        optimize_any(datafile, size, rnd, verbose);
        optimize_expand(datafile, size, rnd, verbose);
        optimize_trim(datafile, size, rnd, verbose);
    }
    
    optimize_bigjump(datafile, size, rnd, verbose);
    
    std::uniform_int_distribution<size_t> dist(0, std::numeric_limits<uint32_t>::max());
    datafile.SetSeed(dist(rnd));
}
