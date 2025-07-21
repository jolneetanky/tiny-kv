#include "disk_manager_impl.h"
#include <fstream> // For ofstream
#include <filesystem>
#include <cstdio> // For `remove`

DiskManagerImpl::DiskManagerImpl(std::string filename) : DiskManager(filename) {};

// for each new entry we're scanning the whole table to search if the key alr exists
// currently writes a single row
// FORMAT: <keysize>:<key>:<val>
// time = O(n) per row due to GET and DEL operation
void DiskManagerImpl::write(const std::string &serializedData)
{
    std::cout << "[DiskManagerImpl] serialized data: " << serializedData << "\n";
    std::cout << "[DiskManagerImpl] Writing to disk..." << "\n";

    // Step 1: Parse key from sereializedData
    size_t pos = 0;
    size_t colon1 = serializedData.find(":", pos);

    // get key size
    int keysize = std::stoi(serializedData.substr(pos, colon1));
    // get key
    pos = colon1 + 1;
    std::string keystr = serializedData.substr(pos, keysize);

    std::cout << "[DiskManagerImpl.write()] keystr: " << keystr << "\n";
    // search for the key in DB. If it exists in DB, delete it.
    if (!get(keystr).empty())
    {
        del(keystr);
    }

    // attemmpts to open the file. If file doesn't exist, it will be created.
    // `std::ios::trunc` supports overwriting over appending
    std::ofstream outFile(m_filename, std::ios::app);
    if (outFile.is_open())
    {
        // write to file
        outFile << serializedData;
        outFile.close();
    }
    else
    {
        std::cerr << "Error: Unable to open file" << "\n";
    }

    std::cout << "[DiskManagerImpl] Succesfully written to disk" << "\n";
}

// current impl: time = O(n)
std::string DiskManagerImpl::get(const std::string &key) const
{
    std::string val{""};

    // NAIVE: scan row-by-row in disk
    std::ifstream file(m_filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << m_filename << "\n";
        return "";
    }

    std::string line;
    while (std::getline(file, line))
    {
        // Expected line format: <keysize>:<key>:<val>
        size_t pos = 0;

        // parse key size
        size_t colon1 = line.find(":", pos);
        if (colon1 == std::string::npos)
            continue; // if cannot find the first colon, move on to next line
        int keysize = std::stoi(line.substr(pos, colon1));

        // parse the key
        pos = colon1 + 1;
        std::string keystr = line.substr(pos, keysize);
        if (keystr != key)
            continue;

        // get value
        size_t colon2 = line.find(":", pos);
        if (colon2 == std::string::npos)
            continue;
        val = line.substr(colon2 + 1);
        std::cout << "[DiskManagerImpl] Successfuly GET (" << key << ", " << val << ")" << "\n";
        return val;
    }

    return val;
}

// current impl: time = O(n)
// write all rows to a temp file
// except the row with the matching key.
void DiskManagerImpl::del(const std::string &key)
{
    std::cout << "[DiskManagerImpl.del()] Deleting from disk..." << "\n";

    // NAIVE: scan row-by-row in disk
    std::ifstream infile(m_filename);
    if (!infile.is_open())
    {
        std::cerr << "[DiskManagerImpl.del()] Failed to open file: " << m_filename << "\n";
    }

    std::ofstream tempfile("temp.txt");
    if (!tempfile.is_open())
    {
        std::cerr << "[DiskManagerImpl.del()] Failed to open temp file" << "\n";
    }

    std::string line;
    bool found = false;

    while (std::getline(infile, line))
    {
        size_t pos = 0;
        size_t colon1 = line.find(":", pos);
        int keysize = std::stoi(line.substr(pos, colon1)); // length of keysize == colon1

        // parse key
        pos = colon1 + 1;
        std::string keystr = line.substr(pos, keysize);

        if (keystr == key)
        {
            found = true;
            continue; // skip writing this guy into the file
        }

        // write line into temp file
        tempfile << line << "\n";
    }

    infile.close();
    tempfile.close();

    // delete infile
    // rename tempfile to m_filename
    if (std::remove(m_filename.c_str()) != 0)
    {
        std::cerr << "Failed to delete original file" << "\n";
        return;
    }

    if (std::rename("temp.txt", m_filename.c_str()) != 0)
    {
        std::cerr << "Failed to rename temp file to original filename" << "\n";
        return;
    }

    if (!found)
    {
        std::cout << "[DiskManagerImpl.del()] key " << key << " does not exist" << "\n";
    }
    else
    {
        std::cout << "[DiskManagerImpl.del()] deleted key \"" << key << "\"" << "\n";
    }
}