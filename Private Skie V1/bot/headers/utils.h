#pragma once

int util_strlen(char *);
void util_memcpy(void *, void *, int);
int util_strcpy(char *, char *);
int util_atoi(char *, int);

static inline int util_isupper(char);
static inline int util_isalpha(char);
static inline int util_isspace(char);
static inline int util_isdigit(char);

