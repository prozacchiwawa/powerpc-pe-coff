#include <sstream>
#include <cstring>
#include "imports.h"
#include "objectfile.h"
#include "section.h"

void ImportFixup
(ElfObjectFile &eof,
 const std::vector<section_mapping_t> &mapping)
{
    const ElfObjectFile::Section *importSect = eof.getNamedSection(".idata");
    if (!importSect) return;
    uint8_t *importTable = importSect->getSectionData();
    uint8_t *importTarget = importTable, *tableAddr, *hintName;
    uint32_t importRva = FindRVA(mapping, importSect->getNumber());
    uint32_t tableRva, iatRva, hintNameEntry;

    do
    {
        le32write_postinc(importTarget, (tableRva = le32read(importTarget)));
        le32write_postinc(importTarget, le32read(importTarget));
        le32write_postinc(importTarget, le32read(importTarget));
        le32write_postinc(importTarget, le32read(importTarget));
        le32write_postinc(importTarget, (iatRva = le32read(importTarget)));

        if (!tableRva) return;

        // Rewrite the import lookup table
        tableAddr = importTable + tableRva - importRva;
        fprintf(stderr, "ImportFixup table at tableRva %08x importRva %08x\n", tableRva, importRva);
        while (hintNameEntry = le32read(tableAddr))
        {
            le32write_postinc(tableAddr, hintNameEntry);
            // Rewrite the hint/name element
            //hintName = importTable + hintNameEntry - importRva;
            //le16write(hintName, be16read(hintName));
        }

        // Do the second address table
        tableAddr = importTable + iatRva - importRva;
        while (hintNameEntry = le32read(tableAddr))
        {
            le32write_postinc(tableAddr, hintNameEntry);
        }
    } while(1);
}

static std::string idata_name(int i) {
    std::ostringstream idata_name;
    idata_name << ".idata$" << i;
    return idata_name.str();
}

// We'll create the new section and a fixed up relocation section for it.
void CombineImportSections(ElfObjectFile &eof, std::vector<DelayedReloc> &needed_relocs)
{
    const ElfObjectFile::Section *idata_sect = NULL;
    std::vector<uint8_t> idata_content;
    uint32_t section_relative_address = 0;
    int link = 0;
    std::vector<std::pair<int, uint32_t>> offsets;

    // .idata$2 is image_import_descriptors
    // .idata$3 is empty space (need to see if there's reloc info)
    // .idata$4 and .idata$5 are first thunk and original first thunk
    // .idata$6 is string data for all the imports
    // .idata$7 is empty space (need to see if there's reloc info)

    for (int i = 2; idata_sect = eof.getNamedSection(idata_name(i)); i++) {
        if (i == 4) {
            // 20 zero bytes between .idata$3 and .idata$4.
            for (int j = 0; j < 20; j++) {
                idata_content.push_back(0);
            }
            section_relative_address += 20;
            offsets.push_back(std::pair(idata_sect->getNumber(), section_relative_address));
        } else {
            offsets.push_back(std::pair(idata_sect->getNumber(), section_relative_address));
        }

        auto section_data_size = idata_sect->logicalSize();
        auto section_data = idata_sect->getSectionData();

        fprintf(stderr, "section %s\n", idata_name(i).c_str());
        for (int h = 0; h < section_data_size; h += 16) {
            fprintf(stderr, "%08x:", h);
            for (int k = h; k < section_data_size && k < h + 16; k++) {
                fprintf(stderr, " %02x", section_data[k]);
            }
            fprintf(stderr, "\n");
        }

        if (section_data) {
            for (int j = 0; j < section_data_size; j++) {
                idata_content.push_back(section_data[j]);
            }
        } else {
            for (int j = 0; j < section_data_size; j++) {
                idata_content.push_back(0);
            }
        }

        auto reloc_section = eof.findRelocSection(idata_sect->getNumber());
        if (reloc_section) {
            // We have relocations
            fprintf(stderr, "relocation section %s for %s at .idata+%x\n", idata_sect->getName().c_str(), reloc_section->getName().c_str(), (unsigned int)section_relative_address);

            Elf32_Sym symbol;
            auto symbols = eof.getSection(reloc_section->getLink());
            auto symdata = symbols.getSectionData();
            auto relsize = reloc_section->getType() == SHT_RELA ? 12 : 8;
            auto numreloc = reloc_section->logicalSize() / relsize;

            Elf32_Rela reloc = { };
            int relstart, j;
            uint8_t *sectionData = reloc_section->getSectionData();

            for (j = 0; j < numreloc; j++) {
                Elf32_Rela *relptr = (Elf32_Rela*)&sectionData[j * relsize];
                auto symptr = &symdata[ELF32_R_SYM(relptr->r_info) * sizeof(symbol)];
                memcpy(&symbol, symptr, sizeof(symbol));
                memcpy(&reloc, relptr, relsize);
                printf("RELOC: offset %08x info %08x addend %08x [%02x %06x]\n",
                       reloc.r_offset, reloc.r_info, reloc.r_addend,
                       ELF32_R_TYPE(reloc.r_info), ELF32_R_SYM(reloc.r_info));
                printf("SYMBOL: st_name %s (%s) value %08x size %04x info %02x other %02x\n",
                       eof.getSymbolName(symbols.getLink(), symbol.st_name).c_str(),
                       eof.getSection(symbol.st_shndx).getName().c_str(),
                       symbol.st_value,
                       symbol.st_size,
                       symbol.st_info,
                       symbol.st_other);

                DelayedReloc dr = { };
                dr.offset_into_section = section_relative_address;
                dr.symbol = symbol;
                dr.reloc = *relptr;
                dr.reloc.r_offset += section_relative_address;
                needed_relocs.push_back(dr);
            }
        } else {
            fprintf(stderr, "data section %s .idata+%x\n", idata_sect->getName().c_str(), (unsigned int)section_relative_address);
        }

        section_relative_address += section_data_size;
    }

    eof.addSection(".idata", idata_content, SHT_PROGBITS, link);

    // Ensure that the new relocs record what section they're for.
    auto new_idata = eof.getNamedSection(".idata");

    for (auto i = needed_relocs.begin(); i != needed_relocs.end(); i++) {
        i->target_section = new_idata->getNumber();
        // if the symbol refers to a combined section, reset st_value and st_shndx
        for (auto j = offsets.begin(); j != offsets.end(); j++) {
            if (i->symbol.st_shndx == j->first) {
                printf("adjust: (%s(+%04x):+%08x) -> (%s+%08x)\n",
                       eof.getSection(i->symbol.st_shndx).getName().c_str(),
                       (unsigned int)j->second,
                       i->symbol.st_value,
                       new_idata->getName().c_str(),
                       i->symbol.st_value + j->second);
                i->symbol.st_shndx = new_idata->getNumber();
                i->symbol.st_value += j->second;
                break;
            }
        }
    }

    // for (int i = 2; idata_sect = eof.getNamedSection(idata_name(i).c_str()); i++) {
    //     eof.removeSection(idata_name(i));
    // }
}
