#include <string.h>
#include "objectfile.h"
#include "reloc.h"
#include "section.h"
#include "util.h"
#include <assert.h>

uint16_t SingleRelocation::render() {
  return this->rendered;
}

void RelocationSlice::render(std::vector<uint8_t> &relocSect, uint32_t imageBase) {
  auto cur_offset = relocSect.size();
  auto size = 8 + 2 * this->relocations.size();
  if (size & 2) {
    size += 2;
  }
  relocSect.resize(relocSect.size() + size);
  auto ptr = &relocSect[cur_offset];
  le32write_postinc(ptr, this->addr);
  le32write_postinc(ptr, size);
  for (auto r = this->relocations.begin(); r != this->relocations.end(); r++) {
    le16write_postinc(ptr, r->second.render());
  }
  if (size & 2) {
    le16write_postinc(ptr, 0);
  }
}

void RelocationSlice::add(const SingleRelocation &r) {
  auto P = r.relocAddr;
  this->relocations.insert(std::pair(P & 0xfff, r));
}

void RelocationSection::render(std::vector<uint8_t> &relocSect, uint32_t imageBase) {
  for (auto slice = this->slices.begin(); slice != this->slices.end(); slice++) {
    slice->second.render(relocSect, imageBase);
  }
}

void RelocationSection::add(const SingleRelocation &r) {
  uint32_t P = r.relocAddr;
  uint32_t PPage = P & ~0xfff;
  auto found = this->slices.find(PPage);
  if (found == this->slices.end()) {
    this->slices.insert(std::pair(PPage, RelocationSlice(PPage)));
    found = this->slices.find(PPage);
  }
  found->second.add(r);
}

void AddReloc
(RelocationSection &relocs,
 Elf32_Rela &reloc,
 Elf32_Sym &symbol,
 uint32_t targetSection,
 uint32_t relocTarget,
 int type)
{
    relocs.add
      (SingleRelocation
       (reloc, symbol, targetSection, relocTarget, (relocTarget & 0xfff) | (type << 12)));
}

#define ADDR24_MASK 0xfc000003

void SingleReloc
(const ElfObjectFile &eof,
 const std::vector<section_mapping_t> &rvas,
 Elf32_Rela &reloc,
 Elf32_Sym &symbol,
 const ElfObjectFile::Section &target,
 RelocationSection &relocs,
 uint32_t imageBase)
{
    int j;
    uint32_t S,A,P;

#if 0
    printf("RELOC: offset %08x info %08x addend %08x [%02x %06x]\n",
           reloc.r_offset, reloc.r_info, reloc.r_addend,
           ELF32_R_TYPE(reloc.r_info), ELF32_R_SYM(reloc.r_info));
#endif

    auto resource = false;
    uint32_t dataOffset;
    if (target.getName() == ".rsrc") {
        resource = true;
        printf("RSRC: offset %08x info %08x addend %08x [%02x %06x]\n",
               reloc.r_offset, reloc.r_info, reloc.r_addend,
               ELF32_R_TYPE(reloc.r_info), ELF32_R_SYM(reloc.r_info));
    }

    /* Compute addends */
    S = symbol.st_value +
        FindRVA(rvas, symbol.st_shndx) +
        imageBase;
    A = reloc.r_addend;
    P = reloc.r_offset + FindRVA(rvas, target.getNumber());
    //printf("start of target elf section %08x\n", target.getStartRva());

//#if 0
    printf("SYMBOL: value %08x size %08x info %02x other %02x shndx %08x total %08x\n",
           symbol.st_value,
           symbol.st_size,
           symbol.st_info,
           symbol.st_other,
           symbol.st_shndx,
           S);
//#endif

    uint8_t *Target = TargetPtr(rvas, target, P);
    uint8_t *tword = TargetPtr(rvas, target, P & ~3);
    uint8_t oldBytes[sizeof(uint32_t)];
    memcpy(oldBytes, tword, sizeof(oldBytes));

    P += imageBase;

    switch (ELF32_R_TYPE(reloc.r_info))
    {
    case R_PPC_NONE:
        break;
    case R_PPC_ADDR32:
        printf("ADDR32 S %08x A %08x P %08x\n", S, A, P);
        le32write(Target, S + A);
        AddReloc(relocs, reloc, symbol, target.getNumber(), P - imageBase, 3);
        break;
    case R_PPC_REL32:
        //printf("REL32 S %08x A %08x P %08x\n", S, A, P);
        le32write(Target, S + A - P);
        break;
    case R_PPC_UADDR32: /* Special: Treat as RVA */
        printf("UADDR32 S %08x A %08x P %08x\n", S, A, P);
        if (resource) {
            dataOffset = le32read(Target);
        } else {
            dataOffset = 0;
        }
        le32write(Target, S + A - imageBase + dataOffset);
        break;
    case R_PPC_REL24:
        //printf("REL24 S %08x A %08x P %08x\n", S, A, P);
        //printf("New Offset: %08x to Addr %08x from %08x\n", S+A-P, S+A, P);
        le32write(Target, ((S+A-P) & ~ADDR24_MASK) | (le32read(Target) & ADDR24_MASK));
        break;
    case R_PPC_ADDR16_LO:
        //printf("ADDR16_LO S %08x A %08x P %08x\n", S, A, P);
        le16write(Target, S + A);
        AddReloc(relocs, reloc, symbol, target.getNumber(), P - imageBase, 2);
        break;
    case R_PPC_ADDR16_HA:
        //printf("ADDR16_HA S %08x A %08x P %08x\n", S, A, P);
        le16write(Target, (S + A + 0x8000) >> 16);
        AddReloc(relocs, reloc, symbol, target.getNumber(), P - imageBase, 4);
        // AddReloc(relocSect, imageBase, relocAddr, S + A, -1);
        break;
    default:
        break;
    }

    uint8_t newBytes[sizeof(uint32_t)];
    memcpy(newBytes, tword, sizeof(newBytes));
    printf("Reloc changed %08x [%02x %02x %02x %02x] --> [%02x %02x %02x %02x]\n",
           P & ~3,
           oldBytes[0],
           oldBytes[1],
           oldBytes[2],
           oldBytes[3],
           newBytes[0],
           newBytes[1],
           newBytes[2],
           newBytes[3]);
}

void SingleRelocSection
(const ElfObjectFile &obf,
 const ElfObjectFile::Section &section,
 const std::vector<section_mapping_t> &rvas,
 RelocationSection &relocs,
 uint32_t imageBase)
{
    Elf32_Rela reloc = { };
    uint8_t *Target;
    int numreloc, relstart, relsize, j;
    uint8_t *sectionData = section.getSectionData();

    relsize = section.getType() == SHT_RELA ? 12 : 8;
    numreloc = section.logicalSize() / relsize;
    const ElfObjectFile::Section &targetSection = obf.getSection(section.getInfo());

    /* Don't relocate non-program section */
    if (!(targetSection.getFlags() & SHF_ALLOC)) {
        return;
    }

    targetSection.setDirty();

    /* Get the symbol section */
    if (!section.getLink()) {
        return;
    }

    const ElfObjectFile::Section &symbolSection = obf.getSection(section.getLink());

    for(j = 0; j < numreloc; j++)
    {
        Elf32_Rela reloc;
        Elf32_Sym symbol;

        /* Get the reloc */
        memcpy(&reloc, sectionData + j * relsize, relsize);

        /* Get the symbol */
        auto symptr = &symbolSection.getSectionData()
            [ELF32_R_SYM(reloc.r_info) * sizeof(symbol)];
        memcpy(&symbol, symptr, sizeof(symbol));
        auto name = obf.getSymbolName(symbolSection.getLink(), symbol.st_name);
        auto comdat = obf.getComdat(name);
        if (comdat != 0xffffffff) {
          auto bss = obf.getNamedSection(".bss");
          if (!bss) {
              printf("comdat %s with no bss\n", name);
              exit(1);
          }
          printf("reloc: %s is comdat at %08x\n", name.c_str(), (unsigned int)comdat);
          symbol.st_shndx = bss->getNumber();
          symbol.st_value = comdat;
        }

        SingleReloc
            (obf,
             rvas,
             reloc,
             symbol,
             targetSection,
             relocs,
             imageBase);
    }
}
