This is to describe the contract that the class SSTableFileManager should abide by.

Unit tests for this class will test the adherence to these contracts.

1. `SSTableFileManagerImpl(std::string directoryPath);` - use this to initialize, if the file doesn't exist yet. When this is called, it should create a new file in the directory.

2. `SSTableFileManagerImpl(const std::string &directoryPath, const std::string &fileName);` - initializes an SSTableFileManager with an existing fileName

3. `std::optional<Error> write(std::vector<const Entry\*> entryPtrs) override;`

4. `std::optional<Entry> get(const std::string& key) override;` // searches for a key

5. `std::optional<std::vector<Entry>> getEntries() override;`
6. `std::optional<TimestampType> getTimestamp() override;`
7. `std::string getFullPath() const override;`
8. `std::optional<std::string> getStartKey() override;`
9. `std::optional<std::string> getEndKey() override;`

10. `bool contains(std::string key) override;`
