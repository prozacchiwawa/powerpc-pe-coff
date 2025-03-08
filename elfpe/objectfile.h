#ifndef COMPDVR_ELFOBJECT_H
#define COMPDVR_ELFOBJECT_H

#include <string>
#include <vector>
#include <map>
#include <libelf.h>

class ElfObjectFile {
public:
    typedef std::vector<uint8_t> secdata_t;

    class Symbol {
    public:
      Symbol(const std::string &name, uint32_t offset,
             int section, int flags) :
        name(name), offset(offset), section(section), flags(flags) { }
      std::string name;
      uint32_t offset;
      int section;
      int flags;
    };

    class Section {
    public:
      Section(const Section &other) :
        obj(other.obj), have_data(false),
        number(other.number) {
      }
      Section(ElfObjectFile &obj, int number, Elf_Scn *sechdr) :
        obj(&obj), have_data(false), number(number) {
      }
      Section &operator = (const Section &other) {
        obj = other.obj;
        have_data = false;
        number = other.number;
      }
      operator bool () { return !!section(); }
      std::string getName() const {
          return obj->getString(shdr()->sh_name);
      }

      int getType() const {
          return shdr()->sh_type;
      }

      int getNumber() const {
        return number;
      }

      int getLink() const {
          return shdr()->sh_link;
      }

      int getInfo() const {
        return shdr()->sh_info;
      }

      int getFlags() const {
        return shdr()->sh_flags;
      }

      int logicalSize() const {
        return shdr()->sh_size;
      }

      uint32_t getStartRva() const {
        return shdr()->sh_addr;
      }

      uint32_t getFileOffset() const {
        return shdr()->sh_offset;
      }

      uint8_t *getSectionData() const {
          if(!have_data) {
              data = *elf_getdata(section(), NULL);
              have_data = true;
          }
          return (uint8_t *)data.d_buf;
      }

      void setDirty() const {
          elf_flagscn(section(), ELF_C_SET, ELF_F_DIRTY);
      }

    private:
      Elf_Scn *section() const { return obj->get_scn(getNumber()); }
      Elf32_Shdr *shdr() const {
          auto scn = section();
          return elf32_getshdr(scn);
      }

      const ElfObjectFile *obj;
      int number;
      mutable bool have_data;
      mutable Elf_Data data;
    };

    ElfObjectFile(const std::string &filename);
    ~ElfObjectFile();

    Elf *operator -> () { return fd >= 0 ? elfHeader : 0; }
    bool operator ! () const { return fd == -1 ? true : false; }
    int getNumSections() const { return sections.size(); }
    uint32_t getEntryPoint() const;
    std::string getString(int offset) const {
        return elf_strptr(elfHeader, shstrndx, offset);
    }
    std::string getSymbolName(int link, int offset) const {
        if (offset) {
            auto data = sections[link]->getSectionData();
            return std::string((char *)(data + offset));
        } else {
            return std::string();
        }
    }
    void addSection
    (const std::string &name, const secdata_t &data, int type = SHT_PROGBITS, int link = 0);
    const Section &getSection(int sect) const { return *sections[sect]; }
    const Section *getNamedSection(const std::string &name) const;
    const Symbol &getSymbol(int n) const { return *symbols[n]; }
    const Symbol *getNamedSymbol(const std::string &symname) const;
    void update()
    {
        elf_flagelf(elfHeader, ELF_C_SET, ELF_F_DIRTY);
        elf_update(elfHeader, ELF_C_WRITE);
        finalize();
        init();
    }
    void removeSection(const std::string &name);
    const Section *findRelocSection(int for_section);

private:
    int fd;
    int shnum, phnum;
    int shstrndx;
    Elf *elfHeader;
    Elf_Data *lastStr;
    std::string filename;
    std::vector<Section*> sections;
    std::map<std::string, const Section *> sections_by_name;
    std::vector<Symbol*> symbols;
    std::map<std::string, const Symbol *> symbols_by_name;

    void init();
    void finalize();
    void populateSymbolTable();
    void populateSections();

    Elf32_Ehdr *get_ehdr() const {
        return elf32_getehdr(elfHeader);
    }
    Elf_Scn *get_scn(int number) const {
        return elf_getscn(elfHeader, number);
    }
};

#endif//COMPDVR_ELFOBJECT_H
