#include "exports.h"
#include "objectfile.h"
#include "section.h"

void ExportFixup
(ElfObjectFile &eof,
 const std::vector<section_mapping_t> &mapping)
{
    const ElfObjectFile::Section *exportSect = eof.getNamedSection(".edata");
    if (!exportSect) return;
    uint8_t *exportTable = exportSect->getSectionData();
    if (!exportTable) return;
    // Fixup the words of the export directory
    uint8_t *exportTarget = exportTable;
    uint32_t exportRva = FindRVA(mapping, exportSect->getNumber());

    uint32_t nameRva, ordinalBase, numberOfAddress, numberOfNames, addressRva,
        namePtrRva, ordinalTableRva;
    int i;

    le32write_postinc(exportTarget, le32read(exportTarget));
    le32write_postinc(exportTarget, le32read(exportTarget));
    le16write_postinc(exportTarget, le16read(exportTarget));
    le16write_postinc(exportTarget, le16read(exportTarget));
    le32write_postinc(exportTarget, (nameRva = le32read(exportTarget)));
    le32write_postinc(exportTarget, (ordinalBase = le32read(exportTarget)));
    le32write_postinc(exportTarget, (numberOfAddress = le32read(exportTarget)));
    le32write_postinc(exportTarget, (numberOfNames = le32read(exportTarget)));
    le32write_postinc(exportTarget, (addressRva = le32read(exportTarget)));
    le32write_postinc(exportTarget, (namePtrRva = le32read(exportTarget)));
    le32write_postinc(exportTarget, (ordinalTableRva = le32read(exportTarget)));
    
    // Address Table
    exportTarget = exportTable + addressRva - exportRva;
    for (i = 0; i < numberOfAddress; i++)
        le32write_postinc(exportTarget, le32read(exportTarget));
    
    // Name table
    exportTarget = exportTable + namePtrRva - exportRva;
    for (i = 0; i < numberOfNames; i++)
        le32write_postinc(exportTarget, le32read(exportTarget));

    // Ordinal table
    exportTarget = exportTable + ordinalTableRva - exportRva;
    for (i = 0; i < numberOfAddress; i++)
        le16write_postinc(exportTarget, le16read(exportTarget));
}
