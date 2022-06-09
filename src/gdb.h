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

struct ParseRecordContext
{
    Vector<RecordAtom> atoms;
    size_t atom_idx;
    size_t num_end_atoms;   // contiguous atoms stored at the end of atoms

    bool error;

    size_t i;
    const char *buf;      // record line data
    size_t bufsize;
};

// traverse through all the child elements of an array/struct
struct AtomIter
{
    const RecordAtom *iter_begin;
    const RecordAtom *iter_end;
    const RecordAtom *begin() { return iter_begin; }
    const RecordAtom *end() { return iter_end; }
};
AtomIter GDB_IterChild(const Record &rec, const RecordAtom *array);

// extract values from parsed records
String GDB_ExtractValue(const char *name, const RecordAtom &root, const Record &rec);
int GDB_ExtractInt(const char *name, const RecordAtom &root, const Record &rec);
const RecordAtom *GDB_ExtractAtom(const char *name, const RecordAtom &root, const Record &rec);

// helper functions for searching the root node of a record 
String GDB_ExtractValue(const char *name, const Record &rec);
int GDB_ExtractInt(const char *name, const Record &rec);
const RecordAtom *GDB_ExtractAtom(const char *name, const Record &rec);

inline String GetAtomString(Span s, const Record &rec)
{
    Assert(s.index + s.length <= rec.buf.size());
    String result = {};
    result.assign(rec.buf.data() + s.index, s.length);
    return result;
}

bool GDB_StartProcess(String gdb_filename, String gdb_args);

bool GDB_SetInferiorExe(String filename);

bool GDB_SetInferiorArgs(String args);

void GDB_Shutdown();

// send a message to GDB, don't wait for result
bool GDB_Send(const char *cmd);

// send a message to GDB, wait for a result record
bool GDB_SendBlocking(const char *cmd, bool remove_after = true);

// send a message to GDB, wait for a result record, then retrieve it
bool GDB_SendBlocking(const char *cmd, Record &rec);

// extract a MI record from a newline terminated line
bool GDB_ParseRecord(char *buf, size_t bufsize, ParseRecordContext &ctx);

// first word after record type char
// ex: ^done, *stopped
String GDB_GetRecordAction(const Record &rec);

void GDB_GrabBlockData();

RecordAtomSequence GDB_RecurseEvaluation(ParseRecordContext &ctx);

typedef void AtomIterator(Record &rec, RecordAtom &iter, void *ctx);
void IterateAtoms(Record &rec, RecordAtom &iter, AtomIterator *iterator, void *ctx);

void GDB_PrintRecordAtom(const Record &rec, const RecordAtom &iter, int tab_level, FILE *out = stdout);
