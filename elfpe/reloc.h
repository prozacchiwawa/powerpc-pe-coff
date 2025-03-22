#ifndef _ELFPE_RELOC_H
#define _ELFPE_RELOC_H

#include <vector>
#include "section.h"
#include "objectfile.h"

class SingleRelocation {
public:
  Elf32_Rela reloc;
  Elf32_Sym symbol;
  uint32_t targetSection;
  uint32_t relocAddr;

  uint16_t rendered;

  uint16_t render();

  SingleRelocation
  (Elf32_Rela &reloc,
   Elf32_Sym &symbol,
   uint32_t targetSection,
   uint32_t relocAddr,
   uint16_t rendered) : reloc(reloc), symbol(symbol), targetSection(targetSection), relocAddr(relocAddr), rendered(rendered) {
  }

  SingleRelocation &operator = (const SingleRelocation &) = default;
};

class RelocationSlice {
public:
  uint32_t addr;
  std::map<uint32_t, SingleRelocation> relocations;

  void render(std::vector<uint8_t> &relocSect, uint32_t imageBase);
  void add(const SingleRelocation &reloc);

  RelocationSlice(uint32_t addr) : addr(addr) { }

  RelocationSlice &operator = (const RelocationSlice &) = default;
};

class RelocationSection {
public:
  std::map<uint32_t, RelocationSlice> slices;

  void render(std::vector<uint8_t> &relocSect, uint32_t imageBase);
  void add(const SingleRelocation &reloc);
};

void SingleReloc
(const ElfObjectFile &eof,
 const std::vector<section_mapping_t> &rvas,
 Elf32_Rela &reloc,
 Elf32_Sym &symbol,
 const ElfObjectFile::Section &target,
 RelocationSection &relocs,
 uint32_t imageBase);

  void SingleRelocSection
(const ElfObjectFile &obf,
 const ElfObjectFile::Section &section,
 const std::vector<section_mapping_t> &rvas,
 RelocationSection &relocs,
 uint32_t imageBase);

#endif//_ELFPE_RELOC_H
