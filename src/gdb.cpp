// Copyright (C) 2022 Kyle Sylvestre
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include <iostream>

#include "common.h"

AtomIter GDB_IterChild(const Record &rec, const RecordAtom *parent)
{
    // allow for fudge factor while iterating child atoms
    // need to fail gracefully for bad/missing msgs
    AtomIter result = {};

    if ( (size_t(parent - rec.atoms.data()) < rec.atoms.size()) &&
         (parent->type == Atom_Array || parent->type == Atom_Struct) &&
         (parent->value.index + parent->value.length <= rec.atoms.size()) )
    {
        const Span &span = parent->value;
        result.iter_begin = &rec.atoms[ span.index ];
        result.iter_end = result.iter_begin + span.length;
    }

    return result;
}

AtomType GDB_InferAtomStart(char c)
{
    AtomType result = Atom_None;
    if (c == '{')
    {
        result = Atom_Struct;
    }
    else if (c == '[')
    {
        result = Atom_Array;
    }
    else if (c == '\"')
    {
        result = Atom_String;
    }
    else if ( (c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c == '-' || c == '_') )
    {
        result = Atom_Name;
    }

    return result;
}

void GDB_PushUnordered(ParseRecordContext &ctx, RecordAtom atom)
{
    Assert(ctx.num_end_atoms <= ctx.atoms.size() &&
           ctx.atom_idx < ctx.atoms.size() - ctx.num_end_atoms);
    memcpy(&ctx.atoms[ ctx.atom_idx ], &atom, sizeof(atom));
    ctx.atom_idx++;
}

RecordAtom GDB_PopUnordered(ParseRecordContext &ctx, size_t start_idx)
{
    // pop unordered atoms to the end of the array 
    RecordAtom result = {};
    Assert(start_idx <= ctx.atom_idx);
    size_t num_atoms = ctx.atom_idx - start_idx;
    Assert(ctx.atom_idx + num_atoms < ctx.atoms.size());

    RecordAtom *dest = 
        &ctx.atoms[ ctx.atoms.size() - ctx.num_end_atoms - num_atoms ];
    memmove(dest, &ctx.atoms[ start_idx ],
            num_atoms * sizeof(RecordAtom));

    ctx.num_end_atoms += num_atoms;
    result.value.length = num_atoms;
    result.value.index = (num_atoms == 0) ? 0 : dest - &ctx.atoms[0];

    // clear the popped memory
    ctx.atom_idx -= num_atoms;
    //memset(ctx.atoms + ctx.atom_idx, 0, num_atoms * sizeof(RecordAtom));

    return result;
}

RecordAtom GDB_RecurseRecord(ParseRecordContext &ctx)
{
    static const auto RecurseError = [&](const char *message, char error_char)
    {
        fprintf(stderr, "parse record error: %s\n", message);
        fprintf(stderr, "   before error: %.*s\n", int(ctx.i), ctx.buf);
        fprintf(stderr, "   error char: %c\n", error_char);
        fprintf(stderr, "   after error: %.*s\n", int(ctx.bufsize - (ctx.i + 1)), ctx.buf + ctx.i + 1);

        timeval te;
        gettimeofday(&te, NULL); // get current time
        long long msec = te.tv_sec*1000LL + te.tv_usec/1000; 

        char filename[128]; 
        tsnprintf(filename, "badrecord_%lld.txt", msec);

        FILE *f = fopen(filename, "wb");
        fprintf(f, "error message: %s\n", message);
        fprintf(f, "error index: %zu\n", ctx.i);
        fprintf(f, "%.*s", (int)ctx.bufsize, ctx.buf);
        fclose(f);
        
        // force to end then bail out of here
        Assert(false);
        ctx.error = true;
        ctx.i = ctx.bufsize;
    };

    RecordAtom result = {};
    size_t string_start_idx = 0;
    size_t aggregate_start_idx = 0;

    for (; ctx.i < ctx.bufsize; ctx.i++)
    {
        char c = ctx.buf[ ctx.i ];

        // skip over chars outside of string
        if (result.type != Atom_String && (c == ' ' || c == ',' || c == ';' || c == '_' || c == '\n') ) 
            continue;

        switch (result.type)
        {
            case Atom_None:
            {
                // figure out what kind of block this is 
                AtomType start = GDB_InferAtomStart(c);
                if (start == Atom_String)
                {
                    // start after " index
                    string_start_idx = ctx.i + 1;
                }
                else if (start == Atom_Name)
                {
                    string_start_idx = ctx.i;
                }
                else if (start == Atom_Array || start == Atom_Struct)
                {
                    // store the start of the aggregates
                    aggregate_start_idx = ctx.atom_idx;
                }
                else if (start == Atom_None)
                {
                    RecurseError("can't deduce block type", c);
                    continue;
                }

                result.type = start;

            } break;

            case Atom_Name:
            {
                if (c == '=')
                {
                    // make the string, reset block state
                    Assert(ctx.i >= string_start_idx);
                    result.name.index = string_start_idx;
                    result.name.length = ctx.i - string_start_idx;
                    result.type = Atom_None;
                }
                else if (Atom_Name != GDB_InferAtomStart(c))
                {
                    RecurseError("hit bad atom name character", c);
                    continue;
                }

            } break;

            case Atom_String:
            {
                // TODO: pointer previews of strings are messing this up
                //       ex: value="0x555555556004 "%d""
                char n = '\0';
                char p = '\0';
                if (ctx.i + 1 < ctx.bufsize)
                    n = ctx.buf[ ctx.i + 1 ];
                if (ctx.i >= 1)
                    p = ctx.buf[ ctx.i - 1 ];

                if (c == '"' && p != '\\' && (n == ',' || n == '}' || n == ']'))
                {
                    // hit closing quote
                    // make the string, advance idx, return
                    Assert(ctx.i >= string_start_idx);
                    result.value.index = string_start_idx;
                    result.value.length = ctx.i - string_start_idx;
                    return result;
                }
            } break;

            case Atom_Array:
            case Atom_Struct:
            {
                AtomType start = GDB_InferAtomStart(c);

                if (start != Atom_None)
                {
                    // start of new elem, recurse and add
                    RecordAtom elem = GDB_RecurseRecord(ctx);
                    GDB_PushUnordered(ctx, elem);
                }
                else if (c == ']' || c == '}')
                {
                    if ((c == ']' && result.type != Atom_Array) ||
                        (c == '}' && result.type != Atom_Struct))
                    {
                        // hit wrong ending character
                        char buf[128] = {};
                        const char *type_str = (result.type == Atom_Array) ? "array" : "struct";
                        tsnprintf(buf, "wrong ending character for %s", type_str);
                        RecurseError(buf, c);
                    }
                    else
                    {
                        // end of aggregate, pop from unordered and store in gdb 
                        RecordAtom pop = GDB_PopUnordered(ctx, aggregate_start_idx);
                        result.value.index = pop.value.index;
                        result.value.length = pop.value.length;
                        return result;
                    }
                }
                else
                {
                    RecurseError("hit bad aggregate char", c);
                    continue;
                }

            } break;
        }
    }

    return result;
}

RecordAtomSequence GDB_RecurseEvaluation(ParseRecordContext &ctx)
{
    // parse the atom of a -data-evaluate-expression
    // close to a GDB record, but not quite the same differences being: 
    // -not packed, there are spaces in the buffer
    // -array has {} syntax instead of []
    // -for arrays, if there are more than 200 elements it ends in "...}"
    // -run length for arrays ex: {0 <repeats 1024 times>}

    const auto EvaluateRunLength = [&](size_t &rle_last_idx, 
                                       size_t &rle_num_repeat) -> bool
    {
        bool atom = false;
        if (ctx.i + 10 < ctx.bufsize &&
            0 == strcmp(&ctx.buf[ ctx.i + 2 ], "<repeats "))
        {
            // TODO: make run length atom that precedes an atom array/value with
            // a <repeats XXX times> string appended to it
            // Options:
            // 1. carry around run length signifier in atom
            // 2. expand <repeats XXX times> text out 

            atom = true;
            rle_num_repeat = 0;
            size_t dig_idx = ctx.i + 10 + 1;
            for (; dig_idx < ctx.bufsize; dig_idx++)
            {
                char c = ctx.buf[ dig_idx ];
                if (c >= '0' && c <= '9')
                {
                    rle_num_repeat *= 10;
                    rle_num_repeat += (c - '0');
                }
                else 
                {
                    Assert(c == ' ');
                    break;
                }
            }

            // skip over " times>"
            Assert(ctx.buf[ dig_idx ] == ' ');
            rle_last_idx = dig_idx + 6;
        }

        return atom;
    };

    RecordAtomSequence sequence = {};
    sequence.length = 1;
    RecordAtom &atom = sequence.atom;
    size_t string_start_idx = 0;
    size_t aggregate_start_idx = 0;
    bool inside_string_literal = false;
    size_t rle_last_idx = 0;
    size_t rle_num_repeat = 0;
    size_t num_children = 0;

    for (; ctx.i < ctx.bufsize; ctx.i++)
    {
        char c = ctx.buf[ ctx.i ];
        char p = (ctx.i > 1) ? ctx.buf[ ctx.i - 1 ] : '\0';
        char pp = (ctx.i > 2) ? ctx.buf[ ctx.i - 2 ] : '\0';
        char n = (ctx.i + 1 < ctx.bufsize) ? ctx.buf[ ctx.i + 1 ] : '\0';
        char nn = (ctx.i + 2 < ctx.bufsize) ? ctx.buf[ ctx.i + 2 ] : '\0';
        if (pp != '\\' && p == '\\' && c == '\"')
            inside_string_literal = !inside_string_literal;

        if (inside_string_literal)
            continue;

        if (EvaluateRunLength(rle_last_idx, rle_num_repeat) &&
            (atom.type == Atom_Name || atom.type == Atom_String))
        {
            // not a Atom_Name, actually an Atom_String
            Assert(ctx.i >= string_start_idx);
            atom.type = Atom_String;
            atom.value.index = string_start_idx;
            atom.value.length = (ctx.i + 1) - string_start_idx;

            ctx.i = rle_last_idx; //+1 = one past '>' index
            sequence.length = rle_num_repeat;

            return sequence;
        }



        switch (atom.type)
        {

        case Atom_None:
        {
            if (c == ' ' || c == ',')
            {
                continue;
            }
            else if (c == '{')
            {
                // store the start of the aggregates
                aggregate_start_idx = ctx.atom_idx;
                atom.type = Atom_Struct;
            }
            else
            {
                string_start_idx = ctx.i;
                if ((n == ',' || n == '}' || nn == '<') && ctx.i > 0) 
                    ctx.i--; // single digit elements like {0, 1, 2}

                if (atom.name.length == 0)
                {
                    atom.type = Atom_Name;
                }
                else
                {
                    atom.type = Atom_String;
                }
            }
        } break;

        case Atom_Name:
        {

            if (c == '=')
            {
                // name = value, -1 to step back to space index
                Assert(ctx.i - 1 >= string_start_idx);
                atom.name.index = string_start_idx;
                atom.name.length = (ctx.i - 1) - string_start_idx;
                atom.type = Atom_None;
            }
            else if (n == ',' || n == '}')
            {
                // not a Atom_Name, actually an Atom_String
                Assert(ctx.i >= string_start_idx);
                atom.type = Atom_String;
                atom.value.index = string_start_idx;
                atom.value.length = (ctx.i + 1) - string_start_idx;
                return sequence;
            }
        } break;

        case Atom_String:
        {
            if (n == ',' || n == '}')
            {
                Assert(ctx.i >= string_start_idx);
                atom.value.index = string_start_idx;
                atom.value.length = (ctx.i + 1) - string_start_idx;

                return sequence;
            }
        } break;

        case Atom_Array:
        case Atom_Struct:
        {
            if (c == '}')
            {
                // end of aggregate, pop from unordered and store in gdb 
                RecordAtom pop = GDB_PopUnordered(ctx, aggregate_start_idx);
                atom.value.index = pop.value.index;
                atom.value.length = pop.value.length;

                if (EvaluateRunLength(rle_last_idx, rle_num_repeat))
                {
                    ctx.i = rle_last_idx;
                    sequence.length = rle_num_repeat;
                }

                return sequence;
            }
            else
            {
                // start of new elem, recurse and add
                size_t saved_num_end_atoms = ctx.num_end_atoms;
                RecordAtomSequence elem = GDB_RecurseEvaluation(ctx);
                if (elem.atom.name.length == 0)
                    atom.type = Atom_Array;

                if (num_children < AGGREGATE_MAX)
                {
                    size_t addcount = GetMin(elem.length, AGGREGATE_MAX - num_children);
                    for (size_t i = 0; i < addcount; i++)
                        GDB_PushUnordered(ctx, elem.atom);
                    num_children += addcount;
                }
                else
                {
                    // no atoms added, remove any child in order pushes
                    // to the end of the array
                    ctx.num_end_atoms = saved_num_end_atoms;
                }
            }
        } break;

        default:
            break;
        }
    }

    return sequence;
}

void GDB_PrintRecordAtom(const Record &rec, const RecordAtom &iter, 
                         int tab_level, FILE *out)
{
    for (int i = 0; i < tab_level; i++)
        fprintf(out, "  ");

    switch (iter.type)
    {
        case Atom_String:  // key value pair
        {
            fprintf(out, "%.*s=\"%.*s\"\n",
                    int(iter.name.length), &rec.buf[ iter.name.index ],
                    int(iter.value.length), &rec.buf[ iter.value.index ]);
        } break;

        case Atom_Struct:
        case Atom_Array:
        {
            fprintf(out, "%.*s\n", int(iter.name.length), 
                    &rec.buf[ iter.name.index ]);

            for (const RecordAtom &child : GDB_IterChild(rec, &iter))
            {
                GDB_PrintRecordAtom(rec, child, tab_level + 1, out);
            }

        } break;
        default:
            fprintf(out, "---BAD ATOM TYPE---\n");
    }
}

const RecordAtom *GDB_ExtractAtom(const char *name, size_t namelen, 
                                  size_t name_idx, const RecordAtom &iter,
                                  const Record &rec)
{
    // copy segment of full name to temp buffer
    char temp[256] = {};
    const char *dotpos = strchr(name + name_idx, '.');
    size_t end_idx = (dotpos != NULL) ? dotpos - name : namelen;
    memcpy(temp, name + name_idx, end_idx - name_idx);

    unsigned long long index = ~0;
    char *bracket;
    if ( (bracket = strchr(temp, '[')) )
    {
        int num_found = sscanf(bracket, "[%llu]", &index);
        Assert(num_found == 1);
        size_t bracket_offset = bracket - temp;
        memset(bracket, '\0', sizeof(temp) - bracket_offset);
    }
    size_t tempsize = strlen(temp);

    for (const RecordAtom &child : GDB_IterChild(rec, &iter))
    {
        if (index < child.value.length && child.type == Atom_Array)
        {
            // array[] syntax, select n'th child
            const RecordAtom *child_base = 
                &rec.atoms[ child.value.index ];
            return GDB_ExtractAtom(name, namelen, end_idx + 1,
                                   child_base[index], rec);
        }
        else if (child.name.length == tempsize &&
                 0 == memcmp(temp, &rec.buf[ child.name.index ], tempsize))
        {
            if (end_idx == namelen)
            {
                // found the target
                return &child;
            }
            else
            {
                return GDB_ExtractAtom(name, namelen, end_idx + 1, child, rec);
            }
        }
    }
    return NULL;
}

String GDB_ExtractValue(const char *keyname, const RecordAtom &root, const Record &rec)
{
    String result = "";
    const RecordAtom *target = GDB_ExtractAtom(keyname, strlen(keyname), 0, root, rec);
    if (target)
    {
        Assert(target->type == Atom_String);
        result = GetAtomString(target->value, rec);
    }
    return result;
}

int GDB_ExtractInt(const char *name, const RecordAtom &root, const Record &rec)
{
    int result = 0;
    const RecordAtom *target = GDB_ExtractAtom(name, strlen(name), 0, root, rec);
    if (target)
    {
        Assert(target->type == Atom_String);
        result = atoi( GDB_ExtractValue(name, root, rec).c_str() );
    }
    return result;
}

const RecordAtom *GDB_ExtractAtom(const char *name, const RecordAtom &root,
                                  const Record &rec)
{
    const RecordAtom *result = GDB_ExtractAtom(name, strlen(name), 0, root, rec);
    return result;
}

// 
// helper functions
//
String GDB_ExtractValue(const char *name, const Record &rec)
{
    return (rec.atoms.size() == 0) ? "" : GDB_ExtractValue(name, rec.atoms[0], rec);
}
int GDB_ExtractInt(const char *name, const Record &rec)
{
    return (rec.atoms.size() == 0) ? 0 : GDB_ExtractInt(name, rec.atoms[0], rec);
}
const RecordAtom *GDB_ExtractAtom(const char *name, const Record &rec)
{
    return (rec.atoms.size() == 0) ? NULL : GDB_ExtractAtom(name, rec.atoms[0], rec);
}

void GDB_ReadELF(const char *elf_filename)
{
    // extract info from ELF file
    FILE *f = NULL;
    int rc;
    char buf[512] = {};                 // error scratch
    bool is_elf_little_endian = true;   // start off not byte swapping


    const auto ReadOffset = [&buf](FILE *file, size_t file_offset, const char *label,
                                   void *dest, size_t bytes)
    {
        fseek(file, file_offset, SEEK_SET);

        size_t num_elems = fread(dest, bytes, 1, file);
        if (num_elems != 1)
        {
            tsnprintf(buf, "label: %s, errno: %s",
                     label, strerror(errno));
            throw buf;
        }
    };

    const auto EndianSwap = [&](const void *src, size_t bytes) -> uint64_t
    {
        uint64_t result = 0;
        Assert(bytes <= sizeof(result));
        memcpy(&result, src, bytes);
        if (!is_elf_little_endian)
        {
            char swapped[8];
            const char *src_bytes = (const char *)src;
            for (size_t i = 0; i < bytes; i++)
            {
                swapped[i] = src_bytes[ (bytes - 1) - i ];
            }
            memcpy(&result, swapped, bytes);
        }
        return result;
    };

    const auto EndianSwap2 = [&](const char *label, size_t file_offset, size_t struct_offset, size_t bytes) -> uint64_t
    {
        uint64_t result;
        ReadOffset(f, file_offset + struct_offset, label, &result, bytes);
        result = EndianSwap(&result, bytes);
        return result;
    };

    // HACK: looks bad, works good
#define Val(foffset, stname, stvalue)\
    (decltype(stname::stvalue))EndianSwap2(#stname "." #stvalue, foffset, (size_t)&((stname *)0)->stvalue, sizeof(stname::stvalue))

    try
    {
        f = fopen(elf_filename, "rb");
        if (f == NULL)
        {
            tsnprintf(buf, "GDB_ReadELF error opening file: %s", strerror(errno));
            throw buf;
        }

        // read common header to determine 32/64 bit
        char ident[EI_NIDENT];
        ReadOffset(f, 0, "common 16 byte header", ident, EI_NIDENT);
        if (0 != memcmp(ident, ELFMAG, SELFMAG))
            throw "doesn't start with ELF magic number";

        is_elf_little_endian = (ident[EI_DATA] == 1) ? true : 
                               (ident[EI_DATA] == 2) ? false : 
                               throw "neither big nor little endian";

        bool is_64_bit = (ident[EI_CLASS] == 2) ? true : 
                         (ident[EI_CLASS] == 1) ? false : 
                         throw "neither 32 nor 64 bit";

        uint64_t foff = 0;      // file offset
        uint64_t e_shoff;       // section header offset from base
        uint64_t e_shnum;       // number of section headers
        uint64_t e_shentsize;   // bytes per section header
        uint64_t e_shstrndx;    // index of string table for section header names

        // get the beginning off the section header table
        e_shoff = is_64_bit ?
                  Val(foff, Elf64_Ehdr, e_shoff) : 
                  Val(foff, Elf32_Ehdr, e_shoff);

        e_shnum = is_64_bit ?
                  Val(foff, Elf64_Ehdr, e_shnum) : 
                  Val(foff, Elf32_Ehdr, e_shnum);

        e_shentsize = is_64_bit ?
                      Val(foff, Elf64_Ehdr, e_shentsize) : 
                      Val(foff, Elf32_Ehdr, e_shentsize);

        e_shstrndx = is_64_bit ?
                     Val(foff, Elf64_Ehdr, e_shstrndx) : 
                     Val(foff, Elf32_Ehdr, e_shstrndx);


        // iterate through the section headers for symbol table entries
        Vector<char> sh_string_table;      // names of section headers
        Vector<char> sym_string_table;     // names of symbols
        Vector<uint64_t> string_offsets;   // st_name's, offsets into sym_string_table

        // read in sh string table for finding .strtab later
        foff = e_shoff + (e_shstrndx * e_shentsize);
        uint64_t shstr_size = is_64_bit ? 
                              Val(foff, Elf64_Shdr, sh_size) : 
                              Val(foff, Elf32_Shdr, sh_size);

        uint64_t shstr_offset = is_64_bit ? 
                                Val(foff, Elf64_Shdr, sh_offset) : 
                                Val(foff, Elf32_Shdr, sh_offset);
        sh_string_table.resize(shstr_size);
        ReadOffset(f, shstr_offset, ".strtab", sh_string_table.data(), shstr_size);


        for (size_t sh_index = 0; sh_index < e_shnum; sh_index++)
        {

            foff = e_shoff + (sh_index * e_shentsize);
            uint64_t sh_type = is_64_bit ? 
                               Val(foff, Elf64_Shdr, sh_type) : 
                               Val(foff, Elf32_Shdr, sh_type);

            //uint64_t sh_entsize = is_64_bit ? 
            //                      Val(foff, Elf64_Shdr, sh_entsize) : 
            //                      Val(foff, Elf32_Shdr, sh_entsize);

            uint64_t sh_size = is_64_bit ? 
                               Val(foff, Elf64_Shdr, sh_size) : 
                               Val(foff, Elf32_Shdr, sh_size);

            uint64_t sh_offset = is_64_bit ? 
                                 Val(foff, Elf64_Shdr, sh_offset) : 
                                 Val(foff, Elf32_Shdr, sh_offset);

            uint64_t sh_name = is_64_bit ? 
                               Val(foff, Elf64_Shdr, sh_name) : 
                               Val(foff, Elf32_Shdr, sh_name);

            if (sh_type == SHT_SYMTAB)
            {
                uint64_t st_size = is_64_bit ? sizeof(Elf64_Sym) : sizeof(Elf32_Sym);
                uint64_t fsym_base = sh_offset;

                for (uint64_t st_index = 0; st_index < (sh_size / st_size); st_index++)
                {
                    foff = fsym_base + (st_index * st_size);

                    auto st_info = is_64_bit ? 
                                   Val(foff, Elf64_Sym, st_info) :
                                   Val(foff, Elf32_Sym, st_info) ;

                    //auto st_shndx = is_64_bit ? 
                    //                Val(foff, Elf64_Sym, st_shndx) : 
                    //                Val(foff, Elf32_Sym, st_shndx); 

                    uint64_t stb = is_64_bit ? 
                                   ELF64_ST_BIND(st_info) : 
                                   ELF32_ST_BIND(st_info);

                    uint64_t stt = is_64_bit ? 
                                   ELF64_ST_TYPE(st_info) : 
                                   ELF32_ST_TYPE(st_info);

                    auto st_name = is_64_bit ? 
                                    Val(foff, Elf64_Sym, st_name) : 
                                    Val(foff, Elf32_Sym, st_name); 

                    // get the name of this global in the string table
                    // TODO: might separate watches for global and file local vars
                    // TODO: assembly files only list symbols as STT_NOTYPE, how to handle this
                    if ( (stt == STT_OBJECT || stt == STT_NOTYPE) && 
                         (stb == STB_GLOBAL || stb == STB_LOCAL) )
                    {
                        string_offsets.push_back(st_name);
                    }
                }
            }
            else if (sh_type == SHT_STRTAB && 
                     0 == strcmp(sh_string_table.data() + sh_name, ".strtab"))
            {
                sym_string_table.resize(sh_size);
                ReadOffset(f, sh_offset, "string table", sym_string_table.data(), sh_size);
                
                int i = 0;
                while (0 && i < (int)sym_string_table.size())
                {
                    printf("%6d ", i);
                    int num_chars = printf("%s", sym_string_table.data() + i);
                    i += (num_chars + 1);
                    printf("\n");
                }
            }
        }

        if (1) for (uint64_t iter : string_offsets)
        {
            printf("%s\n", sym_string_table.data() + iter);
        }
    }
    catch(const char *label)
    {
        PrintErrorf("GDB_ReadELF error: %s", label);
    }

    if (f != NULL)
    {
        rc = fclose(f);
        if (rc < 0)
        {
            tsnprintf(buf, "GDB_ReadELF error closing %s:", elf_filename);
            PrintErrorLibC("close");
        }
    }

#undef Val

}

int big_global_energy = 1;

bool GDB_ParseRecord(char *buf, size_t bufsize, ParseRecordContext &ctx)
{
    // parse async/sync record
    ctx = {};
    ctx.buf = buf;
    ctx.bufsize = bufsize;

    // get the record keyword, immediately after type prefix
    char *comma = (char *)memchr(buf, ',', bufsize);
    RecordAtom root = {};

    if (comma)
    {
        ctx.i = comma - buf;

        // this is a record with child elements
        // convert the root atom into an array
        char *last = buf + bufsize - 1;
        char prev_comma = *comma;
        char prev_eol = *last;
        *comma = '[';
        *last = ']';

        // scan the buffer to the the total amount of atoms
        size_t num_atoms_found = 0;
        for (size_t i = 0; i < ctx.bufsize; i++)
        {
            char n = (i + 1 < ctx.bufsize) ? ctx.buf[i + 1] : '\0';
            char c = ctx.buf[i];
            if ((c == '[' || c == '{') || 
                (c == '=' && n == '"') || 
                (c == '"' && n == ',') )
                num_atoms_found++;
        }

        // total=(name+value atoms) * num atoms
        ctx.atoms.resize(num_atoms_found * 2);

        root = GDB_RecurseRecord(ctx);

        // restore the modified chars
        *comma = prev_comma;
        *last = prev_eol;    // @@@ case where no comma is found problem

    }
    else
    {
        // this is a prefix-one word record i.e. ^done
        ctx.error = false;
        ctx.atoms.resize(1);    // root
    }

    if (!ctx.error)
    {
        // put the root in place since it doesn't get popped to the 
        // ordered section of the array
        ctx.num_end_atoms++;

        // atoms are in order at the end of the array,
        // subtract the difference from base ordered
        Assert(ctx.num_end_atoms <= ctx.atoms.size());
        size_t ordered_offset = ctx.atoms.size() - ctx.num_end_atoms;
        RecordAtom *ordered_base = &ctx.atoms[ ordered_offset ];
        ordered_base[0] = root;

        for (size_t i = 0; i < ctx.num_end_atoms; i++)
        {
            RecordAtom *iter = &ordered_base[i];
            if ( (iter->type == Atom_Array || iter->type == Atom_Struct) &&
                 (iter->value.length != 0))
            {
                // adjust aggregate offset
                Assert(iter->value.index > ordered_offset);
                iter->value.index -= ordered_offset;
            }
        }

        // num_end_atoms = num atoms
        auto begin = ctx.atoms.begin();
        ctx.atoms.erase(begin, begin + ctx.atoms.size() - ctx.num_end_atoms);
    }

    return !ctx.error;
}

bool GDB_Send(const char *cmd)
{
    bool result = false;

#if defined(DEBUG)
    WriteToConsoleBuffer(cmd, cmdsize);
#endif

    if (!prog.running || gdb.supports_async_execution)
    {
        // write to GDB
        size_t cmdsize = strlen(cmd);
        ssize_t written = write(gdb.fd_out_write, cmd, cmdsize);
        if (written != (ssize_t)cmdsize)
        {
            PrintErrorLibC("GDB_Send");
        }
        else
        {
            ssize_t newline_written = write(gdb.fd_out_write, "\n", 1);
            if (newline_written != 1)
            {
                PrintErrorLibC("GDB_Send");
            }
            else
            {
                result = true;
            }
        }
    }

    return result;
}

static size_t GDB_SendBlockingInternal(const char *cmd, bool remove_after)
{
    uint32_t this_record_id = gdb.record_id++;
    char fullrecord[8 * 1024];
    tsnprintf(fullrecord, "%u%s", this_record_id, cmd);
    size_t result = BAD_INDEX;

    if (GDB_Send(fullrecord))
    {
        bool found = false;
        do
        {
            timespec wait_for;
            clock_gettime(CLOCK_REALTIME, &wait_for);
            wait_for.tv_sec += 3;

            int rc = sem_timedwait(gdb.recv_block, &wait_for);
            if (rc < 0)
            {
                if (errno == ETIMEDOUT)
                {
                    // TODO: retry counts
                    PrintErrorf("Command Timeout: %s", cmd);
                }
                else
                {
                    PrintErrorLibC("sem_timedwait");
                }
                break;
            }
            else
            {
                GDB_GrabBlockData();

                // scan the lines for a result record, mark it as read
                // to prevent later processing
                for (size_t i = 0; i < prog.num_recs; i++)
                {
                    RecordHolder &iter = prog.read_recs[i];
                    const char *bufstr = iter.rec.buf.c_str();

                    if (!iter.parsed && iter.rec.id == this_record_id)
                    {
                        if (NULL != strstr(bufstr, "^error"))
                        {
                            if (NULL != strstr(bufstr, "optimized out"))
                            {
                                // @GDB: match up results for optimized out variables
                                // 1. -data-evaluate-expression argv --> ^done,value="<optimized out>"
                                // 2. -data-evaluate-expression argv[0] --> ^error,msg="value has been optimized out"

                                const Record OPTIMIZED_OUT_FIX = {
                                    this_record_id,
                                    {
                                        { Atom_Array, {0, 0}, {1,1} }, // root atom
                                        { Atom_String, /*value*/{6, 5}, /*<optimized out>*/{13, 15} }
                                    },
                                    "^done,value=\"<optimized out>\""
                                };

                                prog.num_recs++;
                                RecordHolder &last = prog.read_recs[ prog.num_recs - 1 ];
                                last.rec = OPTIMIZED_OUT_FIX;
                                last.parsed = false;
                            }
                            else
                            {
                                // convert error record to GDB console output record
                                String errmsg = GDB_ExtractValue("msg", iter.rec);
                                errmsg = "&\"GDB MI Error: " + errmsg + "\\n\"\n";
                                WriteToConsoleBuffer(errmsg.data(), errmsg.size());
                                iter.parsed = true;
                                return BAD_INDEX;
                            }
                        }
                        else
                        {
                            iter.parsed = remove_after;
                            result = i;
                            found = true;
                        }
                    }
                }
            }

        } while (!found);
    }

    return result;
}

bool GDB_SendBlocking(const char *cmd, bool remove_after)
{
    size_t index = GDB_SendBlockingInternal(cmd, remove_after);
    return (index < prog.read_recs.size());
}

bool GDB_SendBlocking(const char *cmd, Record &rec)
{
    // errno or result record index
    size_t index = GDB_SendBlockingInternal(cmd, false);
    bool result;

    if (index < prog.read_recs.size())
    {
        rec = prog.read_recs[index].rec;
        prog.read_recs[index].parsed = true;
        result = true;
    }
    else
    {
        rec = {};
        result = false;
    }

    return result;
}

static void GDB_ProcessBlock(char *block, size_t blocksize)
{
    // parse buffer of interpreter lines ending in (gdb)
    // sync/async records get stored 
    // console/debug logs get written to log_lines
    size_t block_idx = 0;
    while (block_idx < blocksize)
    {

        // parse the optional id preceding the record
        uint32_t this_record_id = 0;
        while (block_idx < blocksize)
        {
            char c = block[ block_idx ];
            if (c >= '0' && c <= '9')
            {
                this_record_id *= 10;
                this_record_id += (c - '0');
                block_idx++;
            }
            else
            {
                break;
            }
        }

        char *start = block + block_idx;
        char *eol = strchr(start, '\n');
        if (eol)
        {
            if (eol[-1] == '\r')
            {
                eol[-1] = ' ';
            }
            eol[0] = ' ';

            eol++; // GDB_ParseRecord inserts a ']' at the end
        }
        else
        {
            // all records should be NL terminated
            Assert(false);
            break;
        }

        size_t linesize = eol - start;
        WriteToConsoleBuffer(start, linesize);

        // get the record type
        char c = start[0];
        if (c == PREFIX_RESULT || c == PREFIX_ASYNC0 || c == PREFIX_ASYNC1) 
        {
            static ParseRecordContext ctx;
            if ( GDB_ParseRecord(start, linesize, ctx) )
            {
                // search for unused block before adding new one
                RecordHolder *out = NULL;
                for (size_t i = 0; i < prog.num_recs; i++)
                {
                    if (prog.read_recs[i].parsed)
                    {
                        // reused parsed elem in read records
                        out = &prog.read_recs[i];
                        break;
                    }
                }

                if (out == NULL)
                {
                    if (prog.read_recs.size() < prog.num_recs + 1)
                    {
                        size_t newcount = (prog.num_recs + 1) * 4;
                        prog.read_recs.resize(newcount);
                    }

                    out = &prog.read_recs[ prog.num_recs ];
                    prog.num_recs++;
                }

                *out = {};
                out->parsed = false;

                Record &rec = out->rec;
                rec.atoms = ctx.atoms;
                rec.buf.resize(linesize);
                rec.id = this_record_id;
                memcpy(const_cast<char*>(rec.buf.data()), start, linesize);

                // @Debug
                //GDB_PrintRecordAtom(rec, rec.atoms[0], 0);
            }
        }

        // advance to next line, skip over index of NT
        block_idx = block_idx + linesize;
    }
}


void GDB_GrabBlockData()
{
    pthread_mutex_lock(&gdb.modify_block);

    for (size_t i = 0; i < gdb.num_blocks; i++)
    {
        Span &iter = gdb.block_spans[i];
        GDB_ProcessBlock(gdb.block_data + iter.index, iter.length);
        iter = {};
    }
    gdb.num_blocks = 0;

    pthread_mutex_unlock(&gdb.modify_block);
}


#ifdef _WIN32

GDB gdb;

// good ole visual studio testing
char debug_str[] = 
R"(
^done,bkpt={number="1",type="breakpoint",disp="keep",enabled="y",addr="0x0000000000001175",func="main",file="debug.c",fullname="/mnt/c/Users/Kyle/Documents/Visual Studio 2017/Projects/Imgui/examples/example_glfw_opengl3/debug.c",line="13",thread-groups=["i1"],times="0",original-location="/mnt/c/Users/Kyle/Documents/Visual Studio 2017/Projects/Imgui/examples/example_glfw_opengl3/debug.c:12"}
)";

int main(int argc, char **argv)
{
    String s = "lsdkfjslk";

    const char *path = 
        //R"(C:\Users\Kyle\Documents\Visual Studio 2017\Projects\Imgui\examples\example_glfw_opengl3\out)";
        R"(C:\Users\Kyle\Downloads\Chrome Downloads\ARM\AARCH32\advent.out)";
    GDB_ReadELF(path);

    static ParseRecordContext ctx;
    bool ok = GDB_ParseRecord(debug_str, sizeof(debug_str), ctx);

    Record r;
    r.atoms = ctx.atoms;
    r.buf.resize(sizeof(debug_str));
    r.buf.assign(debug_str, debug_str + sizeof(debug_str));
    auto b = GDB_ExtractValue("bkpt.addr", r);

    GDB_PrintRecordAtom(r, r.atoms[0], 0);

    // win32 testing
    //size_t debug_strlen = strlen(debug_str);
    //memcpy(gdb.buf, debug_str, debug_strlen);
    //gdb.bufsize = debug_strlen;
    //RecordAtom root = GDB_ParseRecord();
    //GDB_PrintRecordAtom(root, 0);
     return 0;
}

#endif
