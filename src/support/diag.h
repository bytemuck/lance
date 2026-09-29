#ifndef LANCE_DIAG_H
#define LANCE_DIAG_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	const char *file;
	uint32_t	line;
	uint32_t	column;
} SourceLoc;

#define SOURCE_LOC(fileName, lineNumber, columnNumber)                                                                 \
	((SourceLoc) {.file = (fileName), .line = (lineNumber), .column = (columnNumber)})

void DiagRegisterSource(const char *file, const char *source);
void FreeDiagnostics(void);

[[gnu::format(printf, 3, 4)]]
void ReportError(const char *category, SourceLoc loc, const char *format, ...);

size_t DiagErrorCount(void);

#endif // LANCE_DIAG_H
