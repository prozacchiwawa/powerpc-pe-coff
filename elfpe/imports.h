#ifndef COMPDVR_IMPORTS_H
#define COMPDVR_IMPORTS_H

#include <vector>
#include <utility>
#include "pedef.h"
#include "util.h"
#include "objectfile.h"
#include "section.h"

struct DelayedReloc {
    int target_section;
    uint32_t offset_into_section;
    Elf32_Sym symbol;
    Elf32_Rela reloc;
};

void ImportFixup
(ElfObjectFile &eof, const std::vector<section_mapping_t> &mapping);

void CombineImportSections(ElfObjectFile &eof, std::vector<DelayedReloc> &needed_relocs);

#endif//COMPDVR_IMPORTS_H
