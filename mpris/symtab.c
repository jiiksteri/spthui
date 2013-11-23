#include "symtab.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "symbol.h"

#include <stdio.h>

#include "introspect.h"
#include "properties.h"

static const struct mpris_symbol syms[] = {

	{
		.iface = "org.freedesktop.DBus.Introspectable",
		.member = "Introspect",
		.eval = introspect_eval,
	},

	{
		.iface = "org.freedesktop.DBus.Properties",
		.member = "Get",
		.eval = properties_get_eval,
	},

	{
		.iface = "org.freedesktop.DBus.Properties",
		.member = "GetAll",
		.eval = properties_getall_eval,
	},

	{
		.iface = "org.freedesktop.DBus.Properties",
		.member = "Set",
		.eval = properties_set_eval,
	},

	{ .iface = NULL, .member = NULL, .eval = NULL },
};

struct mpris_symtab {
	const struct mpris_symbol *tab;
};

int mpris_symtab_init(struct mpris_symtab **symtabp)
{
	struct mpris_symtab *symtab;
	int err;

	*symtabp = symtab = malloc(sizeof(*symtab));
	if (symtab == NULL) {
		err = ENOMEM;
	} else {
		/* Where we almost implemented a real lookup
		 * structure
		 */
		symtab->tab = syms;
		err = 0;
	}

	return err;
}

void mpris_symtab_destroy(struct mpris_symtab *symtab)
{
	free(symtab);
}


static const struct mpris_symbol *match(const struct mpris_symbol *sym,
					const char *iface, const char *member)
{
	return (strcmp(sym->member, member) == 0 && strcmp(sym->iface, iface) == 0)
		? sym
		: NULL;
}


const struct mpris_symbol *mpris_symtab_lookup(const struct mpris_symtab *symtab,
					       const char *iface, const char *member)
{
	const struct mpris_symbol *sym;
	int i;

	sym = NULL;
	for (i = 0; sym == NULL && symtab->tab[i].iface; i++) {
		sym = match(&symtab->tab[i], iface, member);
	}

	return sym;
}
