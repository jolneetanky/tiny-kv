// // This class handles writes to disk, and reads from disk.
// #ifndef DISK_MANAGER_H
// #define DISK_MANAGER_H

// #include <iostream>

// // TODO: make this into an interface or something
// // This will be a virtual class
// class DiskManager
// {
// protected:
//     std::string m_filename;

// public:
//     DiskManager(std::string filename);
//     virtual void write(const std::string &serializedData) = 0;

//     virtual std::string get(const std::string &key) const = 0;

//     virtual void del(const std::string &key) = 0;
// };

// #endif