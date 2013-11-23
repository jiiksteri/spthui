#ifndef MPRIS_SYMTAB_H__INCLUDED
#define MPRIS_SYMTAB_H__INCLUDED

#include "symbol.h"

struct mpris_symtab;

int mpris_symtab_init(struct mpris_symtab **symtabp);
void mpris_symtab_destroy(struct mpris_symtab *symtab);

const struct mpris_symbol *mpris_symtab_lookup(const struct mpris_symtab *symtab,
					       const char *iface, const char *member);

#endif
