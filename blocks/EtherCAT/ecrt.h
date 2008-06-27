struct sync_manager {
    uint16_t index;
    enum {EC_DIR_INPUT, EC_DIR_OUTPUT} direction;
    struct pdo {
        uint16_t index;
        struct pdo_entry {
            uint16_t index;
            uint8_t subindex;
            unsigned int bitlen;
        } *entry;
        unsigned int entry_count;
    } *pdo;
    unsigned int pdo_count;
}; 
