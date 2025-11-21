/*
Copyright (C) 2024 Jonathan "Paril" Barkley

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "common/zone.hpp"
#include "common/files.hpp"

#define JSMN_HEADER
#include "common/jsmn.hpp"

#include <assert.h>
#include <setjmp.h>

typedef struct {
	bool guard_active;
	jmp_buf exception;
	char error[256];
	char error_loc[256];

	char *buffer;
	size_t buffer_len;
	jsmntok_t *tokens, *pos;
	int num_tokens;
} json_parse_t;

/*
=============
Json_JsmnErrorString

Return a human-readable error description for jsmn_parse failures.
=============
*/
static inline const char *Json_JsmnErrorString(int err)
{
	switch (err) {
	case JSMN_ERROR_NOMEM:
		return "Ran out of JSON tokens while parsing";
	case JSMN_ERROR_INVAL:
		return "Invalid character encountered while parsing JSON";
	case JSMN_ERROR_PART:
		return "Unexpected end of JSON data";
	default:
		return "Unknown JSON parse error";
}
}

static inline void Json_Free(json_parse_t *parser)
{
	parser->guard_active = false;
	Z_Free(parser->tokens);
	FS_FreeFile(parser->buffer);
}

/*
=============
	Json_ErrorLocation

Compute the line/column of the provided token for error reporting.
=============
*/
static inline void Json_ErrorLocation(json_parse_t *parser, jsmntok_t *tok)
{
	if (!parser->tokens || !parser->buffer || !tok || tok < parser->tokens || tok >= parser->tokens + parser->num_tokens) {
		Q_strlcpy(parser->error_loc, "unknown location", sizeof(parser->error_loc));
		return;
}

	int col = 1, row = 0;

	for (intptr_t i = tok->start - 1; i >= 0; --i) {
		if (parser->buffer[i] == '\r' || parser->buffer[i] == '\n') {
			col++;

			if (i > 0 && parser->buffer[i - 1] == '\r') {
				--i;
}
		} else if (col == 1) {
			row++;
}
}

	Q_snprintf(parser->error_loc, sizeof(parser->error_loc), "%i:%i", col, row);
}

q_noreturn static inline void Json_Error(json_parse_t *parser, jsmntok_t *tok, const char *err)
{
	assert(parser->guard_active);
	Json_ErrorLocation(parser, tok);
	Q_strlcpy(parser->error, err, sizeof(parser->error));
	longjmp(parser->exception, -1);
}

q_noreturn static inline void Json_Errorno(json_parse_t *parser, jsmntok_t *tok, int err)
{
	assert(parser->guard_active);
	Json_ErrorLocation(parser, tok);
	Q_strlcpy(parser->error, Q_ErrorString(err), sizeof(parser->error));
	longjmp(parser->exception, -1);
}

static inline int Json_ErrorHandler(json_parse_t *parser)
{
	parser->guard_active = true;
	return setjmp(parser->exception);
}

// a simple little wrapper to using jsmn to
// load/validate/parse JSON stuff.
// will return false if a failure occurs (and it jumps to
// that location as well). always run this with a 
// zeroed parser.
static inline void Json_Load(const char *filename, json_parse_t *parser)
{
	jsmn_parser p;

	parser->tokens = NULL;
	parser->pos = NULL;
	parser->buffer = NULL;
	parser->num_tokens = 0;
	parser->buffer_len = 0;

	Q_strlcpy(parser->error, "unknown error", sizeof(parser->error));
		Q_strlcpy(parser->error_loc, "unknown location", sizeof(parser->error_loc));

	int buffer_len;
	if ((buffer_len = FS_LoadFile(filename, (void **) &parser->buffer)) < 0)
		Json_Error(parser, NULL, va("Couldn't load file \"%s\"", filename));
	parser->buffer_len = buffer_len;

	// calculate the total token size so we can grok all of them.
	jsmn_init(&p);

	parser->num_tokens = jsmn_parse(&p, parser->buffer, parser->buffer_len, NULL, 0);
	if (parser->num_tokens < 0)
		Json_Error(parser, parser->pos, Json_JsmnErrorString(parser->num_tokens));

	parser->tokens = Z_Malloc(sizeof(jsmntok_t) * (parser->num_tokens));

	if (!parser->tokens)
		Json_Errorno(parser, parser->pos, Q_ERR(ENOMEM));

	// decode all tokens
	jsmn_init(&p);
	int parse_result = jsmn_parse(&p, parser->buffer, parser->buffer_len, parser->tokens, parser->num_tokens);
	if (parse_result < 0)
		Json_Error(parser, parser->pos, Json_JsmnErrorString(parse_result));

	parser->pos = parser->tokens;
}

// skips the current token entirely, making sure that
// current_token will point to the actual next logical
// token to be parsed.
static inline void Json_SkipToken(json_parse_t *parser)
{
    // just in case...
    if ((parser->pos - parser->tokens) >= parser->num_tokens)
        return;

    int num_to_parse = parser->pos->size;
    jsmntype_t type = parser->pos->type;

    switch (type) {
    case JSMN_UNDEFINED:
    case JSMN_STRING:
    case JSMN_PRIMITIVE:
        parser->pos++;
        break;
    case JSMN_ARRAY:
    case JSMN_OBJECT:
        parser->pos++;
        for (size_t i = 0; i < num_to_parse; i++) {
            if (type == JSMN_OBJECT)
                parser->pos++;
            Json_SkipToken(parser);
        }
        break;
    }
}

static inline jsmntok_t *Json_Ensure(json_parse_t *parser, jsmntype_t id)
{
    if ((parser->pos - parser->tokens) >= parser->num_tokens)
        Json_Error(parser, parser->pos, "tried to read past the end of the JSON token list");
    if (parser->pos->type != id)
        Json_Errorno(parser, parser->pos, Q_ERR_INVALID_FORMAT);
    return parser->pos;
}

static inline jsmntok_t *Json_EnsureNext(json_parse_t *parser, jsmntype_t id)
{
    jsmntok_t *tok = Json_Ensure(parser, id);
    parser->pos++;
    return tok;
}

static inline bool Json_Strcmp(json_parse_t *parser, const char *s)
{
    Json_Ensure(parser, JSMN_STRING);
    return strncmp(parser->buffer + parser->pos->start, s, parser->pos->end - parser->pos->start);
}

static inline size_t Json_Strlen(json_parse_t *parser)
{
    Json_Ensure(parser, JSMN_STRING);
    return parser->pos->end - parser->pos->start;
}

static inline jsmntok_t *Json_Next(json_parse_t *parser)
{
    jsmntok_t *t = parser->pos;
    parser->pos++;
    return t;
}