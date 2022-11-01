/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016 Nicira, Inc.
 * Copyright (c) 2022 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Adapted from ovs_strzcpy and ovs_parse_key_value.

#include "gnmi_ctl_utils.h"

#include <stdbool.h>
#include <string.h>

static size_t parse_value(const char *s, const char *delimiters)
{
    size_t n = 0;

    /* Iterate until we reach a delimiter.
     *
     * strchr(s, '\0') returns s+strlen(s), so this test handles the null
     * terminator at the end of 's'.  */
    while (!strchr(delimiters, s[n])) {
        if (s[n] == '(') {
            int level = 0;
            do {
                switch (s[n]) {
                case '\0':
                    return n;
                case '(':
                    level++;
                    break;
                case ')':
                    level--;
                    break;
                }
                n++;
            } while (level > 0);
        } else {
            n++;
        }
    }
    return n;
}

/* Parses a key or a key-value pair from '*stringp'.
 *
 * On success: Stores the key into '*keyp'.  Stores the value, if present, into
 * '*valuep', otherwise an empty string.  Advances '*stringp' past the end of
 * the key-value pair, preparing it for another call.  '*keyp' and '*valuep'
 * are substrings of '*stringp' created by replacing some of its bytes by null
 * terminators.  Returns true.
 *
 * If '*stringp' is just white space or commas, sets '*keyp' and '*valuep' to
 * NULL and returns false. */
bool client_parse_key_value(char **stringp, char **keyp, char **valuep)
{
    /* Skip white space and delimiters.  If that brings us to the end of the
     * input string, we are done and there are no more key-value pairs. */
    *stringp += strspn(*stringp, ", \t\r\n");
    if (**stringp == '\0') {
        *keyp = *valuep = NULL;
        return false;
    }

    /* Extract the key and the delimiter that ends the key-value pair or begins
     * the value.  Advance the input position past the key and delimiter. */
    char *key = *stringp;
    size_t key_len = strcspn(key, ":=(, \t\r\n");
    char key_delim = key[key_len];
    key[key_len] = '\0';
    *stringp += key_len + (key_delim != '\0');

    /* Figure out what delimiter ends the value:
     *
     *     - If key_delim is ":" or "=", the value extends until white space
     *       or a comma.
     *
     *     - If key_delim is "(", the value extends until ")".
     *
     * If there is no value, we are done. */
    const char *value_delims;
    if (key_delim == ':' || key_delim == '=') {
        value_delims = ", \t\r\n";
    } else if (key_delim == '(') {
        value_delims = ")";
    } else {
        *keyp = key;
        *valuep = key + key_len; /* Empty string. */
        return true;
    }

    /* Extract the value.  Advance the input position past the value and
     * delimiter. */
    char *value = *stringp;
    size_t value_len = parse_value(value, value_delims);
    char value_delim = value[value_len];

    /* Handle the special case if the value is of the form "(x)->y".
     * After parsing, 'valuep' will be pointing to - "x)->y".
     * */
    if (key_delim == '(' && value[value_len] == ')' &&
        value[value_len + 1] == '-' && value[value_len + 2] == '>') {
        value_delims = ", \t\r\n";
        value_len += parse_value(&value[value_len], value_delims);
        value_delim = value[value_len];
    }
    value[value_len] = '\0';
    *stringp += value_len + (value_delim != '\0');

    *keyp = key;
    *valuep = value;
    return true;
}

/* Copies 'src' to 'dst'.  Reads no more than 'size - 1' bytes from 'src'.
 * Always null-terminates 'dst' (if 'size' is nonzero), and writes a zero byte
 * to every otherwise unused byte in 'dst'.
 *
 * Except for performance, the following call:
 *     client_strzcpy(dst, src, size);
 * is equivalent to these two calls:
 *     memset(dst, '\0', size);
 *     client_strlcpy(dst, src, size);
 *
 * (Thus, client_strzcpy() is similar to strncpy() without some of the pitfalls.)
 */
void client_strzcpy(char *dst, const char *src, size_t size)
{
    if (size > 0) {
        size_t len = strnlen(src, size - 1);
        memcpy(dst, src, len);
        memset(dst + len, '\0', size - len);
    }
}
