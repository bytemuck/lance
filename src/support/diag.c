#include "diag.h"
#include "vec.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	const char *file;
	const char *source;
} SourceEntry;

static VEC(SourceEntry) gSources = {0};
static size_t gErrorCount		 = 0;

void DiagRegisterSource(const char *file, const char *source) {
	if (!file || !source)
		return;
	VEC_PUSH(gSources, ((SourceEntry) {.file = file, .source = source}));
}

void FreeDiagnostics(void) {
	VEC_FREE(gSources);
	gErrorCount = 0;
}

size_t DiagErrorCount(void) { return gErrorCount; }

static const char *FindSource(const char *file) {
	if (!file)
		return nullptr;
	for (size_t i = 0; i < gSources.count; i++) {
		if (strcmp(gSources.items[i].file, file) == 0)
			return gSources.items[i].source;
	}
	return nullptr;
}

static void PrintSourceLine(const SourceLoc loc) {
	const char *source = FindSource(loc.file);
	if (!source || loc.line == 0)
		return;

	const char *lineStart = source;
	for (uint32_t line = 1; line < loc.line && *lineStart; lineStart++) {
		if (*lineStart == '\n')
			line++;
	}
	if (!*lineStart && loc.line > 1)
		return;

	size_t lineLength = strcspn(lineStart, "\r\n");
	fprintf(stderr, "%5u | %.*s\n", loc.line, (int) lineLength, lineStart);

	fprintf(stderr, "      | ");
	for (uint32_t i = 0; i < loc.column && i < lineLength; i++) {
		fputc(lineStart[i] == '\t' ? '\t' : ' ', stderr);
	}
	fprintf(stderr, "^\n");
}

void ReportError(const char *category, const SourceLoc loc, const char *format, ...) {
	gErrorCount++;

	if (loc.file)
		fprintf(stderr, "%s:", loc.file);
	if (loc.line > 0)
		fprintf(stderr, "%u:%u:", loc.line, loc.column + 1);
	if (loc.file || loc.line > 0)
		fputc(' ', stderr);
	fprintf(stderr, "%s: ", category);

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputc('\n', stderr);

	PrintSourceLine(loc);
}
