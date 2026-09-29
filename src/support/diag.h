#ifndef LANCE_DIAG_H
#define LANCE_DIAG_H

#include <stddef.h>
#include <stdint.h>

// A position in a source file. `line` is 1-based, `column` is 0-based (as
// produced by the lexer); diagnostics print both 1-based.
typedef struct {
    const char* file;
    uint32_t line;
    uint32_t column;
} SourceLoc;

#define SOURCE_LOC(fileName, lineNumber, columnNumber) \
    ((SourceLoc){ .file = (fileName), .line = (lineNumber), .column = (columnNumber) })

// Registers the text of a source file so diagnostics can quote the offending
// line. Both strings must outlive every diagnostic that refers to them.
void DiagRegisterSource(const char* file, const char* source);
void FreeDiagnostics(void);

// Prints "<file>:<line>:<col>: <category>: <message>" followed by the source
// line and a caret under the column, and increments the error count.
[[gnu::format(printf, 3, 4)]]
void ReportError(const char* category, SourceLoc loc, const char* format, ...);

size_t DiagErrorCount(void);

#endif // LANCE_DIAG_H
