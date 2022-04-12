/**
----------------------------------------------------------------------
KeyValues.hpp - A simple keyvalues parser
For the uninitiated, keyvalues is a super simple key->value format used by Valve's Source Engine
This parses a subset of that.
 
To use in your project, include this file whereever, and include it in a translation unit after 
defining KEYVALUES_IMPLEMENTATION. Same style as the STB libraries! 
----------------------------------------------------------------------

----------------------------------------------------------------------
-                      LICENSE FOR THIS COMPONENT                    -
----------------------------------------------------------------------
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
----------------------------------------------------------------------
*/
#pragma once

#include <vector>
#include <string>
#include <cstdio>
#include <memory.h>
#include <cstdlib>
#include <cctype>
#include <stack>
#include <cerrno>

class KeyValues {
public:
	static constexpr int MAX_INDENT_LEVEL = 128;

	typedef void *(*KeyValuesMalloc_t)(size_t);
	typedef void (*KeyValuesFree_t)(void *);

	class Key {
	public:
		char *key;
		char *value;
		enum class ELastCached {
			NONE = 0,
			INT,
			FLOAT,
			BOOL,
		} cached;

		union {
			long int ival;
			double fval;
			bool bval;
		} cachedv;

		bool quoted;

		Key() {
			cached = ELastCached::NONE;
		};

		inline bool ReadBool(bool &ok);
		inline long int ReadInt(bool &ok);
		inline double ReadFloat(bool &ok);
	};

public:
	explicit KeyValues(const char *name, KeyValuesMalloc_t customMalloc = nullptr,
					   KeyValuesFree_t customFree = nullptr);
	KeyValues();
	~KeyValues();

	// Name of this kv block
	const char* Name() const {
		return name;
	};

	// Returns true if parsing was successful
	bool Good() const {
		return good;
	}

	/* Getters */
	bool GetBool(const char *key, bool _default = false);
	int GetInt(const char *key, int _default = -1);
	float GetFloat(const char *key, float _default = 0.0f);
	const char *GetString(const char *key, const char *_default = nullptr);
	double GetDouble(const char *key, double _default = 0.0);
	KeyValues *GetChild(const char *name);

	bool HasKey(const char *key);

	/* Setters */
	void SetBool(const char *key, bool v);
	void SetInt(const char *key, int v);
	void SetFloat(const char *key, float v);
	void SetString(const char *key, const char *v);

	/* Parse from a file */
	static KeyValues* FromFile(const char* file, bool use_escape_codes = false);
	static KeyValues* FromString(const char* string, bool use_escape_codes = false);
	bool ParseFile(const char *file, bool use_escape_codes = false);
	bool ParseString(const char *string, bool use_escape_codes = false, long long len = -1);

	/* Clears a key's value setting it to "" */
	void ClearKey(const char *key);

	/* Completely removes a key */
	void RemoveKey(const char *key);

	/* Dumps this to stdout */
	void DumpToStream(FILE *fs);

	enum class EError {
		NONE = 0,
		UNEXPECTED_EOF,
		MISSING_BRACKET,
		MISSING_QUOTE,
		UNNAMED_SECTION,
		UNTERMINATED_SECTION,
	};

	typedef void (*pfnErrorCallback_t)(int, int, EError);

	void SetErrorCallback(pfnErrorCallback_t callback);

	// Returns list of all normal keys
	const std::vector<Key> &Keys() const {
		return keys;
	}

	// Returns list of all subkeys
	const std::vector<KeyValues *> &SubKeys() const {
		return child_sections;
	};

private:
	void ReportError(int line, int _char, EError err);
	pfnErrorCallback_t pCallback;
	
	void DumpToStreamInternal(FILE *fs, int indent);
	
	static inline bool _internal_isspace(char c) {
		return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
	}
	
	std::vector<KeyValues *> child_sections;
	std::vector<Key> keys;
	
	char *name;
	bool good;
	bool quoted;
	
	KeyValuesFree_t m_free;
	KeyValuesMalloc_t m_malloc;

	void *kvmalloc(size_t sz) const;
	void kvfree(void *ptr) const;
	char *kvstrdup(const char *s) const;
};

#ifdef KEYVALUES_IMPLEMENTATION

KeyValues::KeyValues(const char *name, KeyValuesMalloc_t customMalloc, KeyValuesFree_t customFree)
	: KeyValues() {
	m_free = customFree;
	m_malloc = customMalloc;
	this->name = kvstrdup(name);
	this->keys.reserve(10);
	this->quoted = false;
}

KeyValues::KeyValues() : pCallback(nullptr), good(true), quoted(false), name(nullptr), m_free(nullptr), m_malloc(nullptr) {
}

KeyValues::~KeyValues() {
	if (this->name)
		kvfree(name);

	/* Free the keys */
	for (auto key : this->keys) {
		if (key.key)
			kvfree(key.key);
		if (key.value)
			kvfree(key.value);
	}

	/* Free child sections */
	for (auto section : this->child_sections)
		delete section;
}

void *KeyValues::kvmalloc(size_t sz) const {
	if (m_malloc)
		return m_malloc(sz);
	return malloc(sz);
}

void KeyValues::kvfree(void *ptr) const {
	if (m_free) {
		m_free(ptr);
		return;
	}
	free(ptr);
}

char *KeyValues::kvstrdup(const char *ptr) const {
	size_t sz = strlen(ptr) + 1;
	char *s = (char *)malloc(sz);
	memcpy(s, ptr, sz);
	return s;
}

KeyValues* KeyValues::FromFile(const char* file, bool use_escape_codes) {
	auto* kv = new KeyValues();
	if (!kv->ParseFile(file, use_escape_codes)) {
		delete kv;
		return nullptr;
	}
	return kv;
}

KeyValues* KeyValues::FromString(const char* string, bool use_escape_codes) {
	auto* kv = new KeyValues();
	if (!kv->ParseString(string, use_escape_codes)) {
		delete kv;
		return nullptr;
	}
	return kv;
}


bool KeyValues::ParseFile(const char *file, bool use_escape_codes) {
	FILE *fs = fopen(file, "r");
	if (!fs) {
		this->good = false;
		return false;
	}

	/* Get file size */
	fseek(fs, 0, SEEK_END);
	long int size = ftell(fs);

	/* Read the entire file */
	char *buffer = static_cast<char *>(kvmalloc(size + 1));
	fseek(fs, 0, SEEK_SET);
	fread(buffer, size, 1, fs);
	fclose(fs);
	buffer[size] = 0;

	this->ParseString(buffer, use_escape_codes, size);

	/* Free the allocated buffer */
	kvfree(buffer);
	
	return good;
}

bool KeyValues::ParseString(const char *string, bool escape, long long len) {
	int nline = 0, nchar = 0, bracket_level = 0;
	bool inquote = false, incomment = false, parsed_key = false;
	char buf[512];
	int bufpos = 0;

	size_t nlen = 0;
	if (len < 0)
		nlen = strlen(string);
	else
		nlen = len;

	KeyValues *RootKV = this;
	KeyValues *CurrentKV = this;
	Key CurrentKey;

	/* Replaces the std::stack calls */
	int stackpos = 0;
	KeyValues *stack[512];
	stack[stackpos] = this;
	// stackpos++;

	std::stack<KeyValues *> SectionStack;
	SectionStack.push(this);

	this->good = true;

	char c, nc, pc;
	for (int i = 0; i < nlen; i++, nchar++) {
		c = string[i];
		if (i > 0)
			pc = string[i - 1];
		if (i < nlen - 1)
			nc = string[i + 1];

		if (c == '\n') {
			/* Check for errors */
			if (inquote) {
				this->ReportError(nline, nchar, EError::MISSING_QUOTE);
				return false;
			}
			
			/* Save any tokens we might have at the end of the line */
			if (bufpos > 0) {
				buf[bufpos] = 0;
				if (parsed_key) {
					parsed_key = false;
					CurrentKey.value = kvstrdup(buf);
					CurrentKV->keys.push_back(CurrentKey);
					CurrentKey.key = CurrentKey.value = nullptr;
				}
				else {
					CurrentKey.key = kvstrdup(buf);
					parsed_key = true;
				}
				bufpos = 0;
			}
			incomment = false;
			inquote = false;

			nline++;
			nchar = 0;

			continue;
		}

		if (c == '/' && (pc == '/' || nc == '/') && !inquote) {
			incomment = true;
			continue;
		}

		if (incomment)
			continue;

		/* Handle exit/enter quote */
		if (c == '"') {
			if (inquote) {
				inquote = false;
				buf[bufpos] = 0;
				if (parsed_key) {
					parsed_key = false;
					CurrentKey.value = kvstrdup(buf);
					CurrentKey.quoted = true;
					CurrentKV->keys.push_back(CurrentKey);
					CurrentKey.key = CurrentKey.value = nullptr;
				}
				else {
					CurrentKey.quoted = true;
					CurrentKey.key = kvstrdup(buf);
					parsed_key = true;
				}
				bufpos = 0;
			}
			else {
				inquote = true;
			}
			continue;
		}

		/* Enter scope */
		if (!inquote && c == '{') {
			KeyValues *pKV;
			if (parsed_key) {
				pKV = new KeyValues(CurrentKey.key);
				kvfree(CurrentKey.key);
				CurrentKey.key = 0;
			}
			else if (bufpos > 0) {
				buf[bufpos] = 0;
				bufpos = 0;
				pKV = new KeyValues(buf);
			}
			/* In the event that buf is empty and we've not yet parsed a key, issue an error about an unnamed section */
			else {
				pKV = new KeyValues();
				this->ReportError(nline, nchar, EError::UNNAMED_SECTION);
				return false;
			}
			if (CurrentKey.quoted)
				pKV->quoted = true;
			parsed_key = false;
			CurrentKV->child_sections.push_back(pKV);
			CurrentKV = pKV;
			stack[++stackpos] = pKV;
			bracket_level++;
			continue;
		}
		/* Exit scope */
		else if (!inquote && c == '}') {
			CurrentKV = stack[--stackpos];
			bracket_level--;
			continue;
		}

		/* Eat anything that isn't space into the buffer */
		if (!_internal_isspace(c) || (inquote)) {
			buf[bufpos++] = c;
			continue;
		}

		/* Finally, handle cases where we've encountered a space and we are not in a quote */
		if (_internal_isspace(c) && !inquote && bufpos > 0) {
			inquote = false;
			buf[bufpos] = 0;
			if (parsed_key) {
				parsed_key = false;
				CurrentKey.value = kvstrdup(buf);
				CurrentKey.quoted = false;
				CurrentKV->keys.push_back(CurrentKey);
				CurrentKey.key = CurrentKey.value = nullptr;
			}
			else {
				CurrentKey.quoted = false;
				CurrentKey.key = kvstrdup(buf);
				parsed_key = true;
			}
			bufpos = 0;
		}
		for (; i < nlen - 1 && _internal_isspace(string[i + 1]); i++)
			;
	}

	/* Verify that the parsing completed fine */
	if (inquote)
		this->ReportError(-1, 0, EError::MISSING_QUOTE);
	if (bracket_level > 0)
		this->ReportError(-1, 0, EError::UNTERMINATED_SECTION);
	return good;
}

void KeyValues::ReportError(int line, int _char, EError err) {
	if (pCallback)
		pCallback(line, _char, err);
	this->good = false;
}

void KeyValues::DumpToStream(FILE *fs) {
	this->DumpToStreamInternal(fs, 0);
}

void KeyValues::DumpToStreamInternal(FILE *fs, int indent) {
	/* Ensure indent < 128 or fail */
	if (!fs || indent > MAX_INDENT_LEVEL)
		return;

	char buf[MAX_INDENT_LEVEL+1];
	for (int i = 0; i < indent; i++)
		buf[i] = '\t';
	buf[indent] = 0;
	if (this->name) {
		if (this->quoted)
			fprintf(fs, "%s\"%s\"\n%s{\n", buf, this->name, buf);
		else
			fprintf(fs, "%s%s\n%s{\n", buf, this->name, buf);
		buf[indent] = '\t';
		buf[indent + 1] = 0;
	}

	for (auto &key : this->keys) {
		if (key.quoted)
			fprintf(fs, "%s\"%s\" \"%s\"\n", buf, key.key, key.value);
		else
			fprintf(fs, "%s%s \"%s\"\n", buf, key.key, key.value);
	}
	for (auto section : this->child_sections) {
		section->DumpToStreamInternal(fs, indent + 1);
	}

	buf[indent] = 0;
	if (this->name)
		fprintf(fs, "%s}\n", buf);
}

bool KeyValues::GetBool(const char *key, bool _default) {
	for (auto _key : this->keys) {
		if (_key.key && strcmp(_key.key, key) == 0) {
			bool ok = false;
			bool b = _key.ReadBool(ok);
			if (ok)
				return b;
			return _default;
		}
	}
	return _default;
}

int KeyValues::GetInt(const char *key, int _default) {
	for (auto _key : this->keys) {
		if (_key.key && strcmp(_key.key, key) == 0) {
			bool ok = false;
			int i = (int)_key.ReadInt(ok);
			if (ok)
				return i;
			return _default;
		}
	}
	return _default;
}

float KeyValues::GetFloat(const char *key, float _default) {
	for (auto _key : this->keys) {
		if (_key.key && strcmp(_key.key, key) == 0) {
			bool ok = false;
			float f = (float)_key.ReadFloat(ok);
			if (ok)
				return f;
			return _default;
		}
	}
	return _default;
}

double KeyValues::GetDouble(const char *key, double _default) {
	for (auto _key : this->keys) {
		if (_key.key && strcmp(_key.key, key) == 0) {
			bool ok = false;
			double f = _key.ReadFloat(ok);
			if (ok)
				return f;
			return _default;
		}
	}
	return _default;
}

const char *KeyValues::GetString(const char *key, const char *_default) {
	for (auto _key : this->keys) {
		if (_key.key && strcmp(key, _key.key) == 0)
			return _key.value;
	}
	return _default;
}

bool KeyValues::HasKey(const char *key) {
	for (auto _key : this->keys) {
		if (_key.key && strcmp(key, _key.key) == 0)
			return true;
	}
	return false;
}

bool KeyValues::Key::ReadBool(bool &ok) {
	ok = true;
	if (this->cached == ELastCached::BOOL) {
		return this->cachedv.bval;
	}
	/* If the value is not cached, parse it from a string */
	if (!this->value) {
		ok = false;
		return false;
	}

	/* For bool, check if we've got TRUE or FALSE */
	if (strcasecmp(this->value, "true") == 0 || strcmp(this->value, "1") == 0) {
		this->cachedv.bval = true;
		this->cached = ELastCached::BOOL;
		return true;
	}
	else if (strcasecmp(this->value, "false") == 0 || strcmp(this->value, "0") == 0) {
		this->cachedv.bval = false;
		this->cached = ELastCached::BOOL;
		return false;
	}
	ok = false;
	return false;
}

long int KeyValues::Key::ReadInt(bool &ok) {
	ok = true;
	if (this->cached == ELastCached::INT) {
		return this->cachedv.ival;
	}

	/* Check that value is not null */
	if (!this->value) {
		ok = false;
		return 0;
	}

	long int v = strtol(this->value, nullptr, 10);
	if (errno == 0) {
		this->cached = ELastCached ::INT;
		this->cachedv.ival = v;
		return v;
	}
	ok = false;
	return 0;
}

double KeyValues::Key::ReadFloat(bool &ok) {
	ok = true;
	if (this->cached == ELastCached::FLOAT) {
		return this->cachedv.fval;
	}

	/* Check if value is null */
	if (!this->value) {
		ok = false;
		return 0.0f;
	}

	double f = strtod(this->value, nullptr);
	if (errno == 0) {
		this->cached = ELastCached::FLOAT;
		this->cachedv.fval = f;
		return f;
	}
	ok = false;
	return 0.0f;
}

KeyValues *KeyValues::GetChild(const char *name) {
	for (auto _child : this->child_sections) {
		if (_child->name && strcmp(name, _child->name) == 0) {
			return _child;
		}
	}
	return nullptr;
}

void KeyValues::SetBool(const char *key, bool v) {
	for (auto &_key : this->keys) {
		if (_key.key && strcmp(_key.key, key) == 0) {
			_key.cached = Key::ELastCached::BOOL;
			_key.cachedv.bval = v;
			return;
		}
	}
}

void KeyValues::SetInt(const char *key, int v) {
	for (auto &_key : this->keys) {
		if (_key.key && strcmp(_key.key, key) == 0) {
			_key.cached = Key::ELastCached::INT;
			_key.cachedv.ival = v;
			return;
		}
	}
}

void KeyValues::SetFloat(const char *key, float v) {
	for (auto &_key : this->keys) {
		if (_key.key && strcmp(_key.key, key) == 0) {
			_key.cached = Key::ELastCached::FLOAT;
			_key.cachedv.fval = v;
			return;
		}
	}
}

void KeyValues::SetString(const char *key, const char *v) {
	for (auto &_key : this->keys) {
		if (_key.key && strcmp(_key.key, key) == 0) {
			_key.cached = Key::ELastCached::NONE;
			if (_key.value)
				kvfree(_key.value);
			_key.value = kvstrdup(v);
			return;
		}
	}
}

void KeyValues::ClearKey(const char *key) {
	for (auto &_key : this->keys) {
		if (_key.key && strcmp(_key.key, key) == 0) {
			kvfree(_key.value);
			_key.value = kvstrdup("");
			_key.cached = Key::ELastCached::NONE;
			return;
		}
	}
}

void KeyValues::RemoveKey(const char *key) {
	for (auto it = this->keys.begin(); it != this->keys.end(); it++) {
		if (it->value && strcmp(key, it->value) == 0) {
			this->keys.erase(it);
			return;
		}
	}
}

void KeyValues::SetErrorCallback(KeyValues::pfnErrorCallback_t callback) {
	this->pCallback = callback;
}

#endif //KEYVALUES_IMPLEMENTATION