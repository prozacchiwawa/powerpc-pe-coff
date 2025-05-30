#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <list>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <fstream>
#include "objectfile.h"
#include "header.h"
#include "argparse.h"
#include "reloc.h"
#include "coff.h"
#include "exports.h"
#include "imports.h"

#define TYPE_PEHEADER (SHT_LOOS + 1)
#define TYPE_PERELOC  (SHT_LOOS + 2)

int execute_command( bool verbose, const std::vector<std::string> &args )
{
	std::vector<const char *> argvect;
	pid_t pid;
	int status = 1;

  if (verbose) {
      std::ostringstream oss;
      for (auto arg = args.begin(); arg != args.end(); arg++) {
          oss << " \"" << *arg << "\"";
      }
      fprintf(stderr, "exec %s\n", oss.str().c_str());
  }

	for( size_t i = 0; i < args.size(); i++ )
	{
		argvect.push_back(args[i].c_str());
	}
	argvect.push_back(NULL);
	if (!(pid = vfork()))
	{
		execv((const char *)argvect[0], (char *const*)&argvect[0]);
	}
	else
	{
		int w = waitpid(pid, &status, 0);
		if (verbose) {
        fprintf(stderr, "waitpid res %d\n", w);
    }
	}
  if (verbose) {
      fprintf(stderr, "status result %d\n", status);
  }
	return status;
}

void possibly_define
(std::vector<std::string> &args,
 std::string label,
 std::string body = "1")
{
    std::string the_definition = std::string("-D") + label;
    for (size_t i = 0; i < args.size(); i++)
    {
        if (the_definition.size() <= args[i].size())
        {
            if (the_definition == args[i].substr(0,the_definition.size()))
            {
                // Already defined
                return;
            }
        }
    }
    args.insert(args.begin(), the_definition + "=" + body);
}

std::vector<uint8_t>
ProcessRelocSections
(const ElfObjectFile &eof,
 uint32_t imageBase,
 std::vector<DelayedReloc> &needed_relocs,
 const std::vector<section_mapping_t> &rvas)
{
    RelocationSection relocs;
    std::vector<uint8_t> relocData;
    int i, j;
    for (i = 1; i < eof.getNumSections(); i++)
    {
        std::vector<DelayedReloc> artificial_relocs;

        std::copy_if(needed_relocs.begin(), needed_relocs.end(), std::back_inserter(artificial_relocs), [&](DelayedReloc &d) { return d.target_section == i; });

        const ElfObjectFile::Section &section = eof.getSection(i);
        if (artificial_relocs.size() > 0) {
            fprintf(stderr, "Synthetic relocs for %s\n", section.getName().c_str());
            uint32_t section_rva = FindRVA(rvas, i);
            for (auto r = artificial_relocs.begin(); r != artificial_relocs.end(); r++) {
                // Relocations for this section were synthesized.
                auto reloc = r->reloc;
                SingleReloc
                  (eof, rvas, reloc, r->symbol, section, relocs, imageBase);
            }
            fprintf(stderr, "Synthetic relocs for %s end\n", section.getName().c_str());
            section.setDirty();
        } else {
            if (section.getType() == SHT_REL ||
                section.getType() == SHT_RELA) {
                SingleRelocSection(eof, section, rvas, relocs, imageBase);
            }
        }
    }

    relocs.render(relocData, imageBase);
    return relocData;
}

int main( int argc, char **argv ) {
    bool verbose = false, nostdlib = false,
        nostartfiles = false, compile_only = false, is_dll = false, entry,
        mkheader = true;
    int status = 0;
    std::string gcc_name, gcc_hash_output, gcc_line,
        mingw_lib_dir, output_file, entry_point = "",
        image_base = "0x02000000",
        section_align = "0x1000", file_align = "0x1000", subsystem = "cui",
        result;
    std::vector<std::string> gcc_args_str;
    const char *installdir = getenv("INSTALLDIR");
    installdir = installdir ? installdir : ".";

    for( int i = 1; i < argc; i++ )
        gcc_args_str.push_back(argv[i]);

    gcc_name = std::string(installdir) + "/bin/powerpcle-unknown-elf-gcc";
    mingw_lib_dir = std::string(installdir) + "/lib";

    getarg("-gcc-name", "", true, gcc_args_str, gcc_name);
    getarg("-o", "", false, gcc_args_str, output_file);
    mkheader = !getarg("-N", "", false, gcc_args_str, result);
    compile_only = getarg("-c", "", false, gcc_args_str, result) ||
        getarg("-E", "", false, gcc_args_str, result);
    getarg("-mingw-lib-dir", "", true, gcc_args_str, mingw_lib_dir);
    verbose = getarg("-v", "", false, gcc_args_str, result);
    nostdlib = getarg("-nostdlib", "", false, gcc_args_str, result);
    nostartfiles = getarg("-nostartfiles", "", false, gcc_args_str, result);
    getarg("", "--subsystem", true, gcc_args_str, subsystem);
    entry = getarg("-e", "--entry", true, gcc_args_str, entry_point);
    getarg("", "--image-base", true, gcc_args_str, image_base);
    getarg("", "--section-alignment", true, gcc_args_str, section_align);
    getarg("", "--file-alignment", true, gcc_args_str, file_align);
    is_dll = getarg("-shared", "", true, gcc_args_str, result, true);
    if (output_file.find(".gch") != std::string::npos)
    	compile_only = true;

    if( entry )
    {
        if(entry_point.size() && entry_point[0] == '_')
        {
            entry_point = entry_point.substr(1);
        }
        size_t at = entry_point.find('@');
        if (at != std::string::npos)
        {
            entry_point = entry_point.substr(0, at);
        }
    } else if (subsystem == "windows") {
        entry_point = "WinMainCRTStartup";
    } else {
        entry_point = "mainCRTStartup";
    }

    if( !compile_only )
    {
        // Generate an object file that names the entry point and override the normal entry
        // point.
        gcc_args_str.push_back(std::string("-Wl,--entry=") + entry_point);
        gcc_args_str.push_back(std::string("-Wl,--undefined=") + entry_point);
        if (!nostartfiles) {
            gcc_args_str.push_back(std::string(installdir) + "/lib32/crt1.o");
        }
    }

    const char *system_include_dir_list[] = {
        "lib/mingw-w64-headers/include",
        "lib/mingw-w64-headers/crt",
        "lib/mingw-w64-headers",
        "include",
        nullptr
    };

    for (auto i = 0; system_include_dir_list[i]; i++) {
        std::ostringstream oss;
        oss << installdir << "/" << system_include_dir_list[i];
        gcc_args_str.insert(gcc_args_str.begin(), oss.str().c_str());
        gcc_args_str.insert(gcc_args_str.begin(), "-isystem");
    }

    // We never use the system start files or standard libs
    gcc_args_str.insert(gcc_args_str.begin(),"-nostdlib");
    gcc_args_str.insert(gcc_args_str.begin(),"-nostartfiles");
    gcc_args_str.insert(gcc_args_str.begin(),"-mlittle-endian");
    gcc_args_str.insert(gcc_args_str.begin(),"-mwinnt");
    // Add necessary definitions but allow overrides
    possibly_define(gcc_args_str,"_M_PPC");
    possibly_define(gcc_args_str,"_PPC_");
    possibly_define(gcc_args_str,"__powerpc__");
    possibly_define(gcc_args_str,"__PowerPC__");
    possibly_define(gcc_args_str,"stdcall","");
    possibly_define(gcc_args_str,"__stdcall","");
    possibly_define(gcc_args_str,"__stdcall__","");
    possibly_define(gcc_args_str,"cdecl","");
    possibly_define(gcc_args_str,"__cdecl","");
    possibly_define(gcc_args_str,"__cdecl__","");
    possibly_define(gcc_args_str,"fastcall","");
    possibly_define(gcc_args_str,"__fastcall__","");
    possibly_define(gcc_args_str,"__MINGW_IMPORT","extern");
    possibly_define(gcc_args_str,"_CRTIMP","");
    possibly_define(gcc_args_str,"NTSYSAPI","");
    possibly_define(gcc_args_str,"__NO_CTYPE_INLINES");
    possibly_define(gcc_args_str,"__declspec(x)","");
    /*if (!compile_only) gcc_args_str.push_back(mingw_lib_dir + "/libgcc.a"); */

    // Stack on driver name and dump commands flag
    if( verbose ) gcc_args_str.insert(gcc_args_str.begin(),"-v");
    gcc_args_str.insert(gcc_args_str.begin(), "-mlittle-endian");
    if (!compile_only)
    {
        gcc_args_str.insert(gcc_args_str.begin(), "-Wl,-r");
        gcc_args_str.insert(gcc_args_str.begin(), "-Wl,-EL");
        gcc_args_str.insert(gcc_args_str.begin(), "-Wl,--start-group");
        gcc_args_str.insert(gcc_args_str.begin(), std::string("-Wl,-T,") + mingw_lib_dir + "/ldscript");
        //gcc_args_str.insert(gcc_args_str.begin(), "-Wl,-shared,-Bsymbolic,-z,defs");
    }

    gcc_args_str.insert(gcc_args_str.begin(),gcc_name);

    // Nostdlib
    if( !nostdlib && !compile_only ) {
        gcc_args_str.push_back(std::string("-L") + mingw_lib_dir);
        gcc_args_str.push_back("-lmingw32");
        gcc_args_str.push_back("-lmsvcrt");
        gcc_args_str.push_back("-lcrtdll");
        gcc_args_str.push_back("-lkernel32");
    }

    if( verbose ) {
        fprintf( stderr, "#" );
        for( size_t i = 0; i < gcc_args_str.size(); i++ ) {
            fprintf( stderr, " \"%s\"", gcc_args_str[i].c_str() );
        }
        fprintf( stderr, "\n" );
    }
    if (!compile_only) gcc_args_str.insert(gcc_args_str.end(), "-Wl,--end-group");

    std::vector<section_mapping_t> sectionRvas;
    if ( !(status = execute_command( verbose, gcc_args_str )) && !compile_only && mkheader )
    {
        /* Ok fixup the elf object file */
        if (verbose) {
            std::vector<std::string> copy_cmd;
            copy_cmd.push_back("/bin/cp");
            copy_cmd.push_back(output_file);
            copy_cmd.push_back(output_file + ".elf");
            execute_command(verbose, copy_cmd);
        }

        ElfObjectFile eof(output_file);
        if(!eof) exit(1);

        eof.allocateComdat();

        uint32_t imageSize;
        const ElfObjectFile::Symbol *entry_sym;
        entry_sym = eof.getNamedSymbol(entry_point);

        /* This computes the section RVAs */
        ElfPeHeader header
            (strtoul(image_base.c_str(), 0, 0),
             strtoul(section_align.c_str(), 0, 0),
             strtoul(file_align.c_str(), 0, 0),
             entry_sym,
             0x100000,
             0x1000,
             0x100000,
             0x1000,
             subsystem == "windows" ? 2 : (subsystem == "cui" ? 3 : atoi(subsystem.c_str())),
             is_dll,
             &eof);
        eof.update();

        // Combine the import sections like the ldscript for mingw does.
        std::vector<DelayedReloc> needed_relocs;
        // CombineImportSections(eof, needed_relocs);
        eof.update();

        // Get base section layout
        imageSize = header.getSectionRvas(sectionRvas);

        // Add relocation info
        std::vector<uint8_t> relocSection =
            ProcessRelocSections
          (eof, strtoul(image_base.c_str(), 0, 0), needed_relocs, sectionRvas);
        eof.addSection(".reloc", relocSection, SHT_PROGBITS);
        eof.update();

        // Fixup exports
        ExportFixup(eof, sectionRvas);
        eof.update();

        // Fixup imports (just remember the iat so far)
        ImportFixup(eof, sectionRvas);
        eof.update();

        // Recompute RVAs after adding reloc section
        imageSize = header.getSectionRvas(sectionRvas);
        header.createHeaderSection(sectionRvas, imageSize);
        eof.addSection(".peheader", header.getData(), SHT_PROGBITS);
        eof.update();

        if (verbose) {
            eof.listSymbols(sectionRvas, output_file);
        }

        PECoffExecutable cof(eof, output_file + ".tmp", strtoul(file_align.c_str(), 0, 0));
        cof.Write(output_file);
    }

    return status;
}
