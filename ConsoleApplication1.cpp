#define _CRT_SECURE_NO_WARNINGS
#include <fcntl.h>
#ifdef _WIN32
#include <IO.h>
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <algorithm>
#include <bitset>
#include <codecvt>
#include <ctime>
#include <cwctype>
#include <iomanip>
#include <iostream>
#include <map>
#include <stack>
#include <string.h>
#include <string>
#include <vector>
using namespace std;

class Utility {
public:
    static void trim(wstring& string) {
        int i = string.size() - 1;
        for (; i >= 0 &&
            (string[i] == L' ' || string[i] == 0 || string[i] == 0xFFFF);
            i--)
            string.pop_back();
    }
    static bool endsWith(const wstring& fullString, const wstring& ending) {
        if (fullString.length() >= ending.length()) {
            return (fullString.compare(fullString.length() - ending.length(), ending.length(), ending) == 0);
        }
        else {
            return false;
        }
    }
};

class Entry {
public:
    union {
        struct {
            bool readOnly : 1;
            bool hidden : 1;
            bool system : 1;
            bool volume : 1;
            bool directory : 1;
            bool archive : 1;
            bool device : 1;
            bool normal : 1;
        };
        int32_t data;
    } attribute;
    wstring name, extension;
    uint64_t pos, parentPos;
    uint64_t size;
    time_t lastModifiedTime;
    Entry() {}
    virtual ~Entry() {}
    virtual void printName() {
        wcout << setw(50) << left << name << setw(10) << size;
        wcout << setw(10) << pos;
        //char* time = ctime(&lastModifiedTime);
        //time[strlen(time) - 1] = ' ';
        //wcout << setw(20) << time;
    }
    virtual void printContent() = 0;
};

class Folder : public Entry {
protected:
    vector<Entry*> subEntries;

public:
    ~Folder() {
        for (Entry* e : subEntries)
            delete e;
    }
    Entry* find(wstring _name) {
        for (Entry* entry : subEntries)
            if (entry->name.compare(_name) == 0)
                return entry;
        return 0;
    }
    void printContent() {
        wcout << left << setw(10) << "Status" << setw(50) << left << L"Name"
            << setw(10) << L"Size";
        wcout << setw(10) << L"Cluster";
        //wcout << setw(20) << L"Last Modified Time\n";
        wcout << L"\n";
        for (Entry* entry : subEntries)
            entry->printName();
    }
    void printName() {
        wcout << setw(10) << L"Folder";
        Entry::printName();
        wcout << L"\n";
    }
    friend class FAT32;
    friend class NTFS;
    friend class Filesystem;
    friend class CMD;
};
class File : public Entry {
protected:
    void* dataPtr;

public:
    virtual ~File() { free(dataPtr); }
    void printContent() {
        wcout << L"Can't open directly! Please use another program.\n";
    }
    void printName() {
        wcout << setw(10) << L"File";
        Entry::printName();
        wcout << L"\n";
    }
    friend class FAT32;
    friend class NTFS;
};
class TXT : public File {
    void printContent() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated"
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
#pragma GCC diagnostic pop
        std::wstring wstr = converter.from_bytes((char*)dataPtr, (char*)dataPtr+size);
        wcout << wstr << endl;
    }
    void printName() {
        wcout << setw(10) << L"TXT";
        Entry::printName();
        wcout << L"\n";
    }
};

class Filesystem {
protected:
#pragma pack(push, 1) /* Byte align in memory (no padding) */
    struct BIOS_PARAMETER_BLOCK {
        uint8_t BS_jmpBoot[3]; /* Jump instruction to the boot code */
        uint8_t BS_OEMName[8]; /* Name of system that formatted the volume */
        uint16_t bytes_per_sector;           /* Size of a sector in bytes. */
        uint8_t sectors_per_cluster;         /* Size of a cluster in sectors. */
        uint16_t reserved_sectors;           /* zero */
        uint8_t fats;                        /* zero */
        uint16_t root_entries;               /* zero */
        uint16_t sectors;                    /* zero */
        uint8_t media_type;                  /* 0xf8 = hard disk */
        uint16_t sectors_per_fat;            /* zero */
        /*0x0d*/ uint16_t sectors_per_track; /* Required to boot Windows. */
        /*0x0f*/ uint16_t heads;             /* Required to boot Windows. */
        /*0x11*/ uint32_t hidden_sectors;    /* Offset to the start of the
                                        partition    relative to the disk in
                                        sectors.    Required to boot Windows. */
    } *bpb;
#pragma pack(pop) /* End strict alignment */

#ifdef _WIN32
    HANDLE hDisk;
#else
    int fd;
#endif
public:
    char* firstSector;
    Folder* rootDirectory;
    Filesystem() : firstSector(new char[512]), rootDirectory(0) {
        bpb = (BIOS_PARAMETER_BLOCK*)firstSector;
    }
    Filesystem(wstring diskPath) : Filesystem() {
#ifdef _WIN32
        hDisk = CreateFileW(diskPath.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, 0, NULL);
#else
        string str = string(diskPath.begin(), diskPath.end());
        fd = open(str.c_str(), O_RDONLY);
#endif
    }
    virtual void getData(Entry*) {};
    virtual void printInfo() {
        wcout << L"Bytes per sector: " << bpb->bytes_per_sector
            << L" (bytes)\n";
        wcout << L"Sectors per cluster: " << (int)bpb->sectors_per_cluster
            << '\n';
        wcout << L"Bootsector size: " << bpb->reserved_sectors << '\n';
        wcout << L"Number of FATs: " << (int)bpb->fats << '\n';
        wcout << L"Sectors per tracks: " << bpb->sectors_per_track << '\n';
        wcout << L"Number of heads: " << bpb->heads << '\n';
        wcout << L"Hidden sectors: " << bpb->hidden_sectors << L" \n";
    }
    virtual void readInfo() { read(firstSector, 0, 512); }
    virtual ~Filesystem() {
#ifdef _WIN32
        CloseHandle(hDisk);
#else
        close(fd);
#endif
        delete[] firstSector;
        delete rootDirectory;
    }
    bool read(void* buffer, uint64_t pos, uint64_t bufferSize) {
#ifdef _WIN32
        if (hDisk == INVALID_HANDLE_VALUE)
            return 0;
        DWORD bytesRead;
        LARGE_INTEGER l;
        l.QuadPart = pos;
        SetFilePointerEx(hDisk, l, NULL, FILE_BEGIN);
        if (!ReadFile(hDisk, buffer, bufferSize, &bytesRead, NULL))
            return 0;
#else
        if (fd == -1) {
            wcout << L"reading failed\n";
            wcout << L"Error opening the file: " << strerror(errno) << endl;
            return 0;
        }
        lseek(fd, pos, SEEK_SET);
        ssize_t bytesRead;
        bytesRead = ::read(fd, buffer, bufferSize);
        if (bytesRead <= 0)
            return 0;
#endif
        return 1;
    }
};

class FAT32 : public Filesystem {
private:
#pragma pack(push, 1)            /* Byte align in memory (no padding) */
    typedef struct {             // (total 16 bits--a unsigned short)
        unsigned short sec : 5;  // low-order 5 bits are the seconds
        unsigned short min : 6;  // next 6 bits are the minutes
        unsigned short hour : 5; // high-order 5 bits are the hour
    } FATTime;
    typedef struct {              // (total 16 bits--a unsigned short)
        unsigned short day : 5;   // low-order 5 bits are the day
        unsigned short month : 4; // next 4 bits are the month
        unsigned short year : 7;  // high-order 7 bits are the year
    } FATDate;
    struct FAT32BS {
        uint32_t total_sectors_32;
        uint32_t table_size_32;
        uint16_t extended_flags;
        uint16_t fat_version;
        uint32_t root_cluster;
        uint16_t fat_info;
        uint16_t backup_BS_sector;
        uint8_t reserved_0[12];
        uint8_t drive_number;
        uint8_t reserved_1;
        uint8_t boot_signature;
        uint32_t volume_id;
        uint8_t volume_label[11];
        uint8_t fat_type_label[8];
    } *fat32bs;
#pragma pack(pop) /* End strict alignment */
    vector<uint32_t> fileAllocationTable;

public:
    void readInfo() {
        Filesystem::readInfo();
        fat32bs = (FAT32BS*)(firstSector + sizeof(BIOS_PARAMETER_BLOCK));
    }
    void printInfo() {
        wcout << L"FAT32 file system\n";
        Filesystem::printInfo();
        wcout << L"RDET cluster: " << fat32bs->root_cluster << '\n';
        wcout << L"Total number of sectors: " << fat32bs->total_sectors_32
            << '\n';
        wcout << L"FAT size: " << fat32bs->table_size_32 << '\n';
    }
    void readFAT() {
        fileAllocationTable =
            vector<uint32_t>(fat32bs->table_size_32 * bpb->bytes_per_sector);
        read(fileAllocationTable.data(),
            bpb->reserved_sectors * bpb->bytes_per_sector,
            fileAllocationTable.size());
    }
    vector<Entry*> readDET(int startCluster) {
        vector<Entry*> directoryTree;
        char* buffer =
            (char*)malloc(bpb->sectors_per_cluster * bpb->bytes_per_sector);
        wstring tempName;
        while (1) {
            readCluster(buffer, startCluster);
            for (char* entry = buffer;
                entry - buffer <
                bpb->sectors_per_cluster * bpb->bytes_per_sector;
                entry += 32) {
                if (*entry == 0)
                    goto exit;
                if (*(unsigned char*)entry == 0xe5)
                    continue;
                if (*(entry + 0xb) == 0xF) {
                    char16_t ucs2Data[13];
                    memcpy(ucs2Data, entry + 0x1, 10);
                    memcpy(&ucs2Data[5], entry + 0xe, 12);
                    memcpy(&ucs2Data[11], entry + 0x1c, 4);
                    wstring wstr(ucs2Data, ucs2Data + 13);
                    tempName.insert(0, wstr);
                }
                else {
                    if (*(entry + 0xb) & 0x10)
                        directoryTree.push_back(new Folder);
                    else {
                        if (memcmp(entry + 0x8, "TXT", 3) == 0)
                            directoryTree.push_back(new TXT);
                        else
                            directoryTree.push_back(new File);
                    }
                    if (tempName.length() == 0) {
                        directoryTree.back()->name = wstring(entry, entry + 8);
                        Utility::trim(directoryTree.back()->name);
                        if (isalnum(*(char*)(entry + 0x8)))
                            directoryTree.back()->name.append(
                                L"." + wstring(entry + 0x8, entry + 0xb));
                        Utility::trim(directoryTree.back()->name);
                        transform(directoryTree.back()->name.begin(),
                            directoryTree.back()->name.end(),
                            directoryTree.back()->name.begin(),
                            ::tolower);

                    }
                    else {
                        directoryTree.back()->name = tempName;
                        tempName.clear();
                        Utility::trim(directoryTree.back()->name);
                    }
                    directoryTree.back()->pos =
                        *(uint16_t*)(entry + 0x1a) |
                        ((*(uint16_t*)(entry + 0x14)) << 16);
                    if (directoryTree.back()->pos == 0)
                        directoryTree.back()->pos = fat32bs->root_cluster;
                    directoryTree.back()->size = *(uint32_t*)(entry + 0x1c);
                    directoryTree.back()->attribute.data =
                        *(uint8_t*)(entry + 0xb);

                    FATTime* time = (FATTime*)(entry + 0x16);
                    FATDate* date = (FATDate*)(entry + 0x18);
                    tm input_time = { 0 };
                    input_time.tm_year = date->year + 1980 - 1900;
                    input_time.tm_mon = date->month - 1;
                    input_time.tm_mday = date->day;
                    input_time.tm_hour = time->hour;
                    input_time.tm_min = time->min;
                    input_time.tm_sec = time->sec;
                    directoryTree.back()->lastModifiedTime =
                        mktime(&input_time);
                }
            }
            startCluster++;
        }
    exit:
        free(buffer);
        return directoryTree;
    }
    FAT32() {}
    FAT32(wstring diskPath) : Filesystem(diskPath) {
        readInfo();
        readFAT();
        rootDirectory = new Folder;
        rootDirectory->pos = fat32bs->root_cluster;
        getData(rootDirectory);
    }

    bool readCluster(void* buffer, uint32_t cluster) {
        return read(
            buffer,
            bpb->bytes_per_sector *
            ((bpb->reserved_sectors + bpb->fats * fat32bs->table_size_32) +
                (cluster - 2) * bpb->sectors_per_cluster),
            bpb->sectors_per_cluster * bpb->bytes_per_sector);
    }
    void getData(Entry* e) {
        if (dynamic_cast<File*>(e)) {
            vector<uint32_t> clusters;
            for (uint32_t i = e->pos;
                i < (fat32bs->total_sectors_32 / bpb->sectors_per_cluster) &&
                fileAllocationTable[i] != 0;
                i = fileAllocationTable[i])
                clusters.push_back(i);
            void* data = malloc(clusters.size() * bpb->bytes_per_sector *
                bpb->sectors_per_cluster);
            for (uint32_t i = 0; i < clusters.size(); i++)
                readCluster((char*)data + i * bpb->bytes_per_sector *
                    bpb->sectors_per_cluster,
                    clusters[i]);
            dynamic_cast<File*>(e)->dataPtr = data;
        }
        else {
            dynamic_cast<Folder*>(e)->subEntries = readDET(e->pos);
        }
    }
};

class NTFS : public Filesystem {
private:
#pragma pack(push, 1) /* Byte align in memory (no padding) */
    struct NTFSBS {
        /*0x15*/ uint32_t large_sectors; /* zero */
        uint8_t physical_drive;          /* 0x00 floppy, 0x80 hard disk */
        uint8_t current_head;            /* zero */
        uint8_t extended_boot_signature; /* 0x80 */
        uint8_t reserved2;               /* zero */
        /*0x28*/ int64_t number_of_sectors;
        uint64_t mft_lcn;                /* Cluster location of mft data. */
        uint64_t mftmirr_lcn;            /* Cluster location of copy of mft. */
        uint8_t clusters_per_mft_record; /* Mft record size in clusters. */
        uint8_t reserved0[3];            /* zero */
        uint8_t clusters_per_index_record; /* Index block size in clusters. */
        uint8_t reserved1[3];              /* zero */
        uint64_t volume_serial_number;     /* Irrelevant (serial number). */
        uint32_t checksum;                 /* Boot sector checksum. */
        /*0x54*/ uint8_t bootstrap[426];   /* Irrelevant (boot up code). */
        uint16_t end_of_sector_marker;     /* End of bootsector magic. Always is
                                              0xaa55 in little endian. */
                                              /* sizeof() = 512 (0x200) bytes */
    } *ntfsbs;

    struct MFT_REFERENCE {
        uint64_t indx : 48;
        uint64_t sequenceNumber : 16;
    };

    typedef struct {
        uint32_t magic;     /* Usually the magic is "FILE". */
        uint16_t usa_ofs;   /* See NTFS_RECORD definition above. */
        uint16_t usa_count; /* See NTFS_RECORD definition above. */
        /*  8*/ uint64_t lsn;
        /* 16*/ uint16_t sequence_number;
        /* 18*/ uint16_t link_count;
        /* 20*/ uint16_t attrs_offset;
        /* 22*/ uint16_t flags;
        /* 24*/ uint32_t bytes_in_use;
        /* 28*/ uint32_t bytes_allocated;
        /* 32*/ MFT_REFERENCE base_mft_record;
        uint16_t next_attr_instance;
        /* 42*/ uint16_t reserved;          /* Reserved/alignment. */
        /* 44*/ uint32_t mft_record_number; /* Number of this mft record. */
    } MFT_RECORD;

    typedef struct {
        /*Ofs*/
        /*  0*/ uint32_t type; /* The (32-bit) type of the attribute. */
        /*  4*/ uint32_t length;
        /*  8*/ uint8_t non_resident;
        /*  9*/ uint8_t name_length;
        /* 10*/ uint16_t name_offset;
        /* 12*/ uint16_t flags; /* Flags describing the attribute. */
        /* 14*/ uint16_t instance;
        /* 16*/ union {
            struct {
                /* 16 */ uint32_t
                    value_length; /* Byte size of attribute value. */
                /* 20 */ uint16_t value_offset;
                /* 22 */ uint8_t resident_flags; /* See above. */
                /* 23 */ int8_t reservedR;
                /* 24 */ void* resident_end[0];
            };
            struct {
                /* 16*/ uint64_t lowest_vcn;
                /* 24*/ uint64_t highest_vcn;
                /* 32*/ uint16_t mapping_pairs_offset;
                /* 34*/ uint8_t compression_unit;
                /* 35*/ uint8_t reserved1[5];
                /* 40*/ int64_t allocated_size;
                /* 48*/ int64_t data_size;
                /* 56*/ int64_t initialized_size;
                /* 64 */ int64_t non_resident_end;
                /* 72 */ void* compressed_end[0];
            };
        };
    } ATTR_RECORD;

    typedef struct {
        /*  0*/ uint32_t type;   /* Type of referenced attribute. */
        /*  4*/ uint16_t length; /* Byte size of this entry. */
        /*  6*/ uint8_t name_length;
        /*  7*/ uint8_t name_offset;
        /*  8*/ uint64_t lowest_vcn;
        /* 16*/ MFT_REFERENCE mft_reference;
        /* 24*/ uint32_t instance;
        /* 26*/ uint16_t name[0];
    } ATTR_LIST_ENTRY;

    typedef struct {
        /*  0*/ uint32_t entries_offset;
        /*  4*/ uint32_t index_length;
        /*  8*/ uint32_t allocated_size;
        /* 12*/ uint8_t ih_flags;    /* Bit field of INDEX_HEADER_FLAGS.  */
        /* 13*/ uint8_t reserved[3]; /* Reserved/align to 8-byte boundary.*/
    } INDEX_HEADER;

    typedef struct {
        /*  0*/ uint32_t type;
        /*  4*/ uint32_t collation_rule;
        /*  8*/ uint32_t index_block_size;
        /* 12*/ int8_t clusters_per_index_block;
        /* 13*/ uint8_t reserved[3]; /* Reserved/align to 8-byte boundary. */
        /* 16*/ INDEX_HEADER index;
    } INDEX_ROOT;

    typedef struct {
        uint32_t magic;     /* Magic is "INDX". */
        uint16_t usa_ofs;   /* See NTFS_RECORD definition. */
        uint16_t usa_count; /* See NTFS_RECORD definition. */
        /*  8*/ uint64_t lsn;
        /* 16*/ uint64_t index_block_vcn;
        /* 24*/ INDEX_HEADER index; /* Describes the following index entries. */
    } INDEX_ALLOCATION;

    typedef struct {
        union { /* Only valid when INDEX_ENTRY_END is not set. */
            MFT_REFERENCE indexed_file;
            struct {
                uint16_t data_offset;
                uint16_t data_length; /* Data length in bytes. */
                uint32_t reservedV;   /* Reserved (zero). */
            };
        };
        /*  8*/ uint16_t length;
        /* 10*/ uint16_t key_length;
        /* 12*/ uint16_t ie_flags; /* Bit field of INDEX_ENTRY_* flags. */
        /* 14*/ uint16_t reserved;
    } INDEX_ENTRY;

    typedef struct {
        /*  0*/ MFT_REFERENCE parent_directory;
        /*  8*/ int64_t creation_time; /* Time file was created. */
        /* 10*/ int64_t last_data_change_time;
        /* 18*/ int64_t last_mft_change_time;
        /* 20*/ int64_t last_access_time;
        /* 28*/ int64_t allocated_size;
        /* 30*/ int64_t data_size;
        /* 38*/ uint32_t file_attributes; /* Flags describing the file. */
        /* 3c*/ union {
            /* 3c*/ struct {
                /* 3c*/ uint16_t packed_ea_size;
                /* 3e*/ uint16_t reserved; /* Reserved for alignment. */
            };
            /* 3c*/ uint32_t reparse_point_tag;
        };
        /* 40*/ uint8_t file_name_length;
        /* 41*/ uint8_t file_name_type; /* Namespace of the file name.*/
        /* 42*/ char16_t* file_name;    /* File name in Unicode. */
    } FILE_NAME_ATTR;

#pragma pack(pop) /* End strict alignment */
    uint64_t clusters_per_index_record, clusters_per_mft_record,
        sectors_per_cluster;
    bool mft_record_size_in_bytes;

    time_t convertWindowsTimeToUnixTime(long long int input) {
        long long int temp;
        temp = input / 10000000; // convert from 100ns intervals to seconds;
        temp =
            temp - 11644473600LL; // subtract number of seconds between epochs
        return (time_t)temp;
    }

    void restoreFixup(char* data, uint16_t* fixupArray, int n) {
        char* sector = data + bpb->bytes_per_sector;
        for (int i = 0; i < n-1; i++, sector += bpb->bytes_per_sector)
            *(uint16_t*)(sector - 2) = fixupArray[i];
    }

public:
    NTFS(wstring diskPath) : Filesystem(diskPath) {
        readInfo();
        rootDirectory = (Folder*)readMFTEntry(0, 5);
        //test->printName();
        //rootDirectory->pos = 5;
        //getData(rootDirectory);
    }
    void readInfo() {
        Filesystem::readInfo();
        ntfsbs = (NTFSBS*)(firstSector + sizeof(BIOS_PARAMETER_BLOCK));
        sectors_per_cluster = (bpb->sectors_per_cluster >= 244)
            ? 1 << (256 - bpb->sectors_per_cluster)
            : bpb->sectors_per_cluster;
        clusters_per_index_record =
            (ntfsbs->clusters_per_index_record > 127)
            ? 1 << (256 - ntfsbs->clusters_per_index_record)
            : ntfsbs->clusters_per_index_record;
        if (ntfsbs->clusters_per_mft_record > 127) {
            clusters_per_mft_record =
                1 << (256 - ntfsbs->clusters_per_mft_record);
            mft_record_size_in_bytes = true;
        }
        else {
            mft_record_size_in_bytes = false;
            clusters_per_mft_record = ntfsbs->clusters_per_mft_record;
        }
    }
    void printInfo() {
        wcout << L"NTFS file system\n";
        Filesystem::printInfo();
        wcout << L"Total number of sectors: " << ntfsbs->number_of_sectors
            << '\n';
        wcout << L"MFT cluster: " << ntfsbs->mft_lcn << '\n';
        wcout << L"Clusters per Index block: " << clusters_per_index_record
            << '\n';
        wcout << L"Clusters per MFT record: " << clusters_per_mft_record;
        if (mft_record_size_in_bytes)
            wcout << " (bytes)\n";
        else
            wcout << " (sectors)\n";
    }
    bool readCluster(void* buffer, uint64_t cluster) {
        return read(buffer,
            bpb->bytes_per_sector * cluster * sectors_per_cluster,
            sectors_per_cluster * bpb->bytes_per_sector);
    }
    void readMFT() {}
    void getMFTEntryData(char*& buffer, uint64_t indx) {
        buffer = new char[clusters_per_mft_record *
            (mft_record_size_in_bytes
                ? 1
                : sectors_per_cluster * bpb->bytes_per_sector)];
        if (mft_record_size_in_bytes)
            read(buffer,
                indx * clusters_per_mft_record + ntfsbs->mft_lcn *sectors_per_cluster *bpb->bytes_per_sector,
                clusters_per_mft_record);
        else
            for (int i = 0; i < clusters_per_mft_record; i++)
                readCluster(buffer, (indx + i) * clusters_per_mft_record +
                    ntfsbs->mft_lcn);
        MFT_RECORD* mftrc = (MFT_RECORD*)buffer;
        restoreFixup((char*)mftrc,
            (uint16_t*)(mftrc->usa_ofs + (char*)mftrc),
            mftrc->usa_count);
    }
    void readFilenameAttribute(Entry*& rt, char* attributeData) {
        rt->parentPos = ((MFT_REFERENCE*)attributeData)->indx;
        unsigned char nameSpace = *(uint8_t*)(attributeData + 65);
        rt->size=  *(uint64_t*)(attributeData + 48);
        if (1) { // win32 namespace
            unsigned char size = *(uint8_t*)(attributeData + 64);
            vector<char16_t> ucs2Data(size);
            memcpy(ucs2Data.data(), attributeData + 66,
                size * sizeof(char16_t));
            rt->name = wstring(ucs2Data.begin(),
                ucs2Data.end());
        }
    }
    void readStandardInfomationAttribute(Entry* rt, char* attributeData) {
        rt->attribute.data = *(int32_t*)(attributeData + 32);
        rt->lastModifiedTime =
            convertWindowsTimeToUnixTime(*(int64_t*)(attributeData + 8));
    }
    Entry* readMFTEntry(Entry* rt, uint64_t indx) {
        map<uint64_t, pair<char*, char*>> buffer;
        buffer[indx].first = 0;
        getMFTEntryData(buffer[indx].first, indx);
        MFT_RECORD* mftrc = (MFT_RECORD*)buffer[indx].first;
        if (!rt) {
            if (mftrc->flags & 0x0002)
                rt = new Folder;
            else
                rt = new File;
        }
        buffer[indx].second = mftrc->attrs_offset + (char*)mftrc;
        vector<pair<uint32_t, uint64_t>> attributes;
        bool useAttributeList = false;
        int currentAttributes = 0;
        bool chain = false;
        uint64_t lastWritten = 0;
        char* attributeData = 0;
        char* indexAlloc=0,*indexRoot = 0;
        while (1) {
            uint64_t currentIndex =
                useAttributeList ? attributes[currentAttributes].second : indx;
            MFT_RECORD* mftrc = (MFT_RECORD*)buffer[currentIndex].first;
            ATTR_RECORD* attributePtr =
                (ATTR_RECORD*&)buffer[currentIndex].second;
            while (1) {
                if ((((char*)attributePtr - (char*)mftrc) >
                    clusters_per_mft_record *
                    (mft_record_size_in_bytes
                        ? 1
                        : sectors_per_cluster * bpb->bytes_per_sector)))
                    break;
                if (*(int32_t*)attributePtr == -1)
                    break;

                // getData
                if (attributePtr->non_resident == 0) {
                    if (chain) {
                        char* data = (char*)realloc(
                            attributeData, attributePtr->value_length);
                        if (data)
                            attributeData = data;
                        memcpy(attributeData + lastWritten,
                            attributePtr->value_offset +
                            (char*)attributePtr,
                            attributePtr->value_length);
                    }
                    else {
                        attributeData =
                            (char*)malloc(attributePtr->value_length);
                        memcpy(attributeData,
                            attributePtr->value_offset +
                            (char*)attributePtr,
                            attributePtr->value_length);
                    }
                    lastWritten += attributePtr->value_length;
                }
                else {
                    char* dataRunStart = attributePtr->mapping_pairs_offset +
                        (char*)attributePtr;
                    uint64_t totalSize = attributePtr->highest_vcn -
                        attributePtr->lowest_vcn + 1;
                    vector<pair<int, int>> dataRuns;
                    while (1) {
                        uint8_t clusterAddressOffsetFieldSize =
                            *(uint8_t*)dataRunStart >> 4;
                        uint8_t clusterNumberFieldSize =
                            *(uint8_t*)dataRunStart & 15;
                        int64_t* clusterNumber =
                            (int64_t*)malloc(clusterNumberFieldSize);
                        int64_t* clusterAddressOffset =
                            (int64_t*)malloc(clusterAddressOffsetFieldSize);
                        *clusterNumber = 0, * clusterAddressOffset = 0;
                        memcpy(clusterNumber, dataRunStart + 1,
                            clusterNumberFieldSize);
                        memcpy(clusterAddressOffset,
                            dataRunStart + 1 + clusterNumberFieldSize,
                            clusterAddressOffsetFieldSize);
                        *clusterNumber =
                            *clusterNumber
                            << ((8 - clusterNumberFieldSize) * 8) >>
                            ((8 - clusterNumberFieldSize) * 8);
                        *clusterAddressOffset =
                            *clusterAddressOffset
                            << ((8 - clusterAddressOffsetFieldSize) * 8) >>
                            ((8 - clusterAddressOffsetFieldSize) * 8);
                        if (*clusterNumber == 0)
                            break;
                        if (dataRuns.empty())
                            dataRuns.push_back(
                                { *clusterNumber, *clusterAddressOffset });
                        else
                            dataRuns.push_back(
                                { *clusterNumber, *clusterAddressOffset +
                                                     dataRuns.back().second });
                        dataRunStart += 1 + clusterNumberFieldSize +
                            clusterAddressOffsetFieldSize;
                    }

                    int totalCluster = 0;
                    for (auto x : dataRuns)
                        totalCluster += x.first;
                    if (chain) {
                        char* data = (char*)realloc(
                            attributeData, (lastWritten+ totalCluster * sectors_per_cluster *
                                bpb->bytes_per_sector));
                        if (data)
                            attributeData = data;
                    }
                    else
                        attributeData =
                        (char*)malloc(totalCluster * sectors_per_cluster *
                            bpb->bytes_per_sector);
                    for (auto run : dataRuns) {
                        for (int i = 0; i < run.first;
                            i++, lastWritten +=
                            sectors_per_cluster * bpb->bytes_per_sector)
                            readCluster(attributeData + lastWritten,
                                run.second + i);
                    }
                }
                ATTR_RECORD* nextAttributePtr;
                bool switchIndex = useAttributeList &&
                    currentAttributes < attributes.size() - 1 &&
                    attributes[currentAttributes].second !=
                    attributes[currentAttributes + 1].second;
                if (switchIndex)
                    nextAttributePtr =
                    (ATTR_RECORD
                        *)(buffer[attributes[currentAttributes + 1].second]
                            .second);
                else
                    nextAttributePtr = (ATTR_RECORD*)((char*)attributePtr +
                        attributePtr->length);

                chain = (attributePtr->type == // attribute chains
                    nextAttributePtr->type && nextAttributePtr->length==0);
                if (chain)
                    goto nextAttribute;

                switch (*(uint32_t*)attributePtr) {
                case 0x00000010: //$STANDARD_INFORMATION
                    readStandardInfomationAttribute(rt, attributeData);
                    free(attributeData);
                    break;
                case 0x00000030: //$FILE_NAME
                    readFilenameAttribute(rt, attributeData);
                    free(attributeData);
                    break;
                case 0x00000080: //$DATA
                {
                    File* file = dynamic_cast<File*>(rt);
                    if (rt->size == 0) rt->size = lastWritten;
                    if (file) {
                        file->dataPtr = malloc(file->size);
                        memcpy(file->dataPtr, attributeData, file->size);
                    }
                    free(attributeData);
                } break;
                case 0x00000020: //$ATTRIBUTE_LIST
                {
                    useAttributeList = true;
                    ATTR_LIST_ENTRY* attr_list =
                        (ATTR_LIST_ENTRY*)attributeData;
                    ATTR_LIST_ENTRY* attr_list_original = attr_list;
                    while (1) {
                        if (((char*)attr_list - (char*)attr_list_original) >=
                            ((ATTR_RECORD*)attributePtr)->value_length)
                            break;
                        uint64_t index = attr_list->mft_reference.indx;
                        attributes.push_back({ attr_list->type, index });
                        attr_list = (ATTR_LIST_ENTRY*)((char*)attr_list +
                            attr_list->length);
                    }
                    for (auto& a : attributes) {
                        if (buffer.find(a.second) == buffer.end()) {
                            buffer[a.second] = { 0, 0 };
                            getMFTEntryData(buffer[a.second].first, a.second);
                            MFT_RECORD* mftrc =
                                (MFT_RECORD*)buffer[a.second].first;
                            buffer[a.second].second =
                                mftrc->attrs_offset + (char*)mftrc;
                        }
                    }
                    free(attributeData);
                } break;
                case 0x00000090: //$INDEX_ROOT
                {
                    indexRoot = attributeData;
                } break;
                case 0x000000a0: //$INDEX_ALLOCATION
                {
                    indexAlloc = attributeData;
                }   break;
                case 0x000000b0: //$BITMAP
                    break;
                default:
                    free(attributeData);
                }
            nextAttribute:

                if (!chain)
                    lastWritten = 0;
                buffer[currentIndex].second =
                    ((char*)attributePtr + attributePtr->length);
                attributePtr = nextAttributePtr;
                if (useAttributeList)
                    currentAttributes++;
                if (switchIndex)
                    break;
            }
            if (!useAttributeList ||
                (useAttributeList && currentAttributes >= attributes.size()))
                break;
        }

        if (dynamic_cast<Folder*>(rt))
            dynamic_cast<Folder*>(rt)->subEntries=readIndex(indexRoot, 1, indexAlloc);
       
        return rt;
    }
    vector<Entry*> readIndex(char* attributeData, bool root,char* indexAlloc) {
        vector<Entry*> rt;
        INDEX_ENTRY* indexEntryStart,*end;
        if (root) {
            INDEX_ROOT* indxRt = (INDEX_ROOT*)attributeData;
            indexEntryStart =
                (INDEX_ENTRY*)(indxRt->index.entries_offset +
                    (char*) & indxRt->index);
            end =
                (INDEX_ENTRY*)(indxRt->index.index_length +
                    (char*)&indxRt->index);
        }
        else {
            INDEX_ALLOCATION* indxRt = (INDEX_ALLOCATION*)attributeData;
            restoreFixup((char*)indxRt, (uint16_t*)(indxRt->usa_ofs + (char*)indxRt), indxRt->usa_count);
            indexEntryStart =
                (INDEX_ENTRY*)(indxRt->index.entries_offset +
                    (char*)&indxRt->index);
            end =
                (INDEX_ENTRY*)(indxRt->index.index_length +
                    (char*)&indxRt->index);
        }

        while (indexEntryStart<end) {
            INDEX_ENTRY* i = indexEntryStart;
            if (i->key_length > 0) {
                Entry* e;
                if (*(uint32_t*)((char*)i + sizeof(INDEX_ENTRY) + 56) &
                    0x10000000)
                    e = new Folder;
                else
                    e = new File;
                readFilenameAttribute(e, (char*)i + 16);
                if (dynamic_cast<File*>(e)
                    && Utility::endsWith(e->name, wstring(L".txt")))
                {
                    TXT* newE = new TXT;
                    delete e;
                    e = newE;
                    readFilenameAttribute(e, (char*)i + 16);
                }
                readStandardInfomationAttribute(e, (char*)i +
                    16 + 8);
                e->pos = i->indexed_file.indx;
                if (e->name[0] == L'$')
                    delete e;
                else
                    rt.push_back(e);
            }
            if (indexAlloc!=0&&(i->ie_flags & 1)) {
                uint64_t vcn = *(uint64_t*)((char*)i + i->length - 8);
                vector<Entry*> recur=(readIndex(indexAlloc +
                    vcn * sectors_per_cluster * bpb->bytes_per_sector,false, indexAlloc));
                rt.insert(rt.end(), recur.begin(), recur.end());
                for (size_t i = 0; i < rt.size(); ++i) {
                    for (size_t j = i + 1; j < rt.size(); ++j) {
                        // If a duplicate is found, erase it from the vector
                        if (rt[i]->pos == rt[j]->pos) {
                            if (rt[i]->name.size()> rt[j]->name.size()){
                            rt.erase(rt.begin() + j);
                            --j; // Adjust the loop variable to stay at the current index
                            }
                            else {
                                rt.erase(rt.begin() + i);
                                --i; // Adjust the loop variable to stay at the current index
                            }
                        }
                    }
                }
            }
            if (indexAlloc != 0 && (i->ie_flags & 2))
                break;
            indexEntryStart =
                (INDEX_ENTRY*)((char*)indexEntryStart +
                    indexEntryStart->length);
        }
        for (size_t i = 0; i < rt.size(); ++i) {
            for (size_t j = i + 1; j < rt.size(); ++j) {
                // If a duplicate is found, erase it from the vector
                if (rt[i]->pos == rt[j]->pos) {
                    if (rt[i]->name.size() > rt[j]->name.size()) {
                        rt.erase(rt.begin() + j);
                        --j; // Adjust the loop variable to stay at the current index
                    }
                    else {
                        rt.erase(rt.begin() + i);
                        --i; // Adjust the loop variable to stay at the current index
                    }
                }
            }
        }
        return rt;
        
    }
    void getData(Entry* entry) { 
        if (dynamic_cast<TXT*>(entry)||dynamic_cast<Folder*>(entry))
        readMFTEntry(entry, entry->pos); }
};

class CMD {
private:
    Filesystem* fs;
    stack<Folder*> currentDir;
    wstring diskPath;
    void printCurrentDir(stack<Folder*> currentDir) {
        if (currentDir.empty())
            return;
        Folder* t = currentDir.top();
        currentDir.pop();
        printCurrentDir(currentDir);
        wcout << t->name << L"/";
        currentDir.push(t);
    }
    Filesystem* getFS(wstring diskPath) {
        Filesystem fs(diskPath);
        fs.readInfo();
        if (strncmp(fs.firstSector + 0x52, "FAT32", 5) == 0)
            return new FAT32(diskPath);
        if (strncmp(fs.firstSector + 3, "NTFS", 4) == 0)
            return new NTFS(diskPath);
        return 0;
    }

public:
    CMD() {}
    ~CMD() { delete fs; }
    void run() {
        wcout << L"Input disk: ";
        getline(wcin, diskPath);
#ifdef _WIN32
        diskPath = L"\\\\.\\" + diskPath + L":";
#else
        diskPath = L"/dev/" + diskPath;
#endif
        fs = getFS(diskPath);
        if (fs == 0) {
            wcout << L"Not supported filesystem!\n";
            return;
        }
        currentDir.push(fs->rootDirectory);
        wstring commandInput;
        while (1) {
            printCurrentDir(currentDir);
            getline(wcin, commandInput);
            wstring command =
                commandInput.substr(0, commandInput.find_first_of(' '));
            if (command == L"dir" || command == L"ls")
                currentDir.top()->printContent();
            else if (command == L"info")
                fs->printInfo();
            else if (command == L"cls" || command == L"clear")
                system("clear || cls");
            else if (command == L"exit")
                return;
            else if (command == L"open" || command == L"cd") {
                wstring argument =
                    commandInput.substr(commandInput.find_first_of(' ') + 1);

                if (argument == L".")
                    continue;
                else if (argument == L"..")
                    currentDir.pop();
                else {
                    Entry* found = currentDir.top()->find(
                        wstring(argument.begin(), argument.end()));
                    if (found) {
                        fs->getData(found);
                        if (command == L"cd") {
                            if (dynamic_cast<Folder*>(found))
                                currentDir.push(dynamic_cast<Folder*>(found));
                            else
                                wcout << L"Can't change directory to a file!\n";
                        }
                        else
                            found->printContent();
                    }
                    else
                        wcout << L"Doesn't found!\n";
                }
            }
            else if (command == L"help")
                showHelp();
            else
                wcout << L"Wrong command! Type help for more info.\n";
        }
    }
    void showHelp() {
        wcout << L"dir/ls - print content of current directory\n";
        wcout << L"open - open file\n";
        wcout << L"cd - open directory\n";
        wcout << L"info - print info about filesystem\n";
        wcout << L"cls/clear - clear screen\n";
        wcout << L"exit - exit program\n";
    }
};

int main() {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
#else
    locale::global(locale("en_US.UTF-8"));
    wcout.imbue(locale());
    wcin.imbue(locale());
#endif
    CMD cmd;
    cmd.run();
}

