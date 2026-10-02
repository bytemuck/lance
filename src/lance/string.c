#include "lance/string.h"

#include <stdlib.h>
#include <string.h>

LNCString lncMakeString(const char* items) {
	if (items == NULL) {
		return (LNCString) {
				.items	= NULL,
				.length = 0,
		};
	}

	const size_t length = strlen(items);
	const char*	 copied = strdup(items);

	if (copied == NULL) {
		return (LNCString) {
				.items	= NULL,
				.length = 0,
		};
	}

	return (LNCString) {
			.items	= copied,
			.length = length,
	};
}

void lncFreeString(LNCString* string) {
	if (string == NULL) {
		return;
	}

	free((void*) string->items);

	string->items  = NULL;
	string->length = 0;
}

LNCString lncCopyString(LNCString string) {
	return lncCopyStringView(lncViewString(string));
}

LNCString lncCopyStringView(LNCStringView view) {
	if (view.items == NULL || view.length == 0) {
		return (LNCString) {
				.items	= NULL,
				.length = 0,
		};
	}

	const char* copied = strndup(view.items, view.length);

	if (copied == NULL) {
		return (LNCString) {
				.items	= NULL,
				.length = 0,
		};
	}

	return (LNCString) {
			.items	= copied,
			.length = view.length,
	};
}

LNCString lncCopySubstring(LNCString string, LNCSpan span) {
	if (span.begin > span.end || span.end > string.length) {
		return (LNCString) {
				.items	= NULL,
				.length = 0,
		};
	}

	return lncCopyStringView(lncSubstringView(lncViewString(string), span));
}

LNCStringView lncViewString(LNCString string) {
	return (LNCStringView) {
			.items	= string.items,
			.length = string.length,
	};
}

LNCStringView lncSubstringView(LNCStringView view, LNCSpan span) {
	if (span.begin > span.end || span.end > view.length || view.items == NULL || view.length == 0) {
		return (LNCStringView) {
				.items	= NULL,
				.length = 0,
		};
	}

	return (LNCStringView) {
			.items	= view.items + span.begin,
			.length = span.end - span.begin,
	};
}
