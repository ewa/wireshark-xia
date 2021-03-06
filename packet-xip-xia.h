/* packet-xip-xia.h
 * Utility routines and definitions for XIP packet dissection.
 * Copyright 2012, Cody Doucette <doucette@bu.edu>
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _NET_XIA_H
#define _NET_XIA_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <asm/byteorder.h>

#ifdef __KERNEL__
#include <net/sock.h>
#else
#define BUILD_BUG_ON(x)
#define __force
#include <epan/value_string.h>
#include <string.h>
#endif

/* XIA Principal Types. */
#define XIDTYPE_AD		0x10
#define XIDTYPE_HID		0x11
#define XIDTYPE_CID		0x12
#define XIDTYPE_SID		0x13
#define XIDTYPE_UNI4ID		0x14
#define XIDTYPE_I4ID		0x15
#define XIDTYPE_U4ID		0x16
#define XIDTYPE_XDP		0x17
#define XIDTYPE_SRVCID		0x18
#define XIDTYPE_FLOWID		0x19
#define XIDTYPE_ZF		0x20

static const value_string xidtype_vals[] = {
	{ XIDTYPE_AD,		"ad" },
	{ XIDTYPE_HID,		"hid" },
	{ XIDTYPE_CID,		"cid" },
	{ XIDTYPE_SID,		"sid" },
	{ XIDTYPE_UNI4ID,	"uni4id" },
	{ XIDTYPE_I4ID,		"i4id" },
	{ XIDTYPE_U4ID,		"u4id" },
	{ XIDTYPE_XDP,		"xdp" },
	{ XIDTYPE_SRVCID,	"serval" },
	{ XIDTYPE_FLOWID,	"flowid" },
	{ XIDTYPE_ZF,		"zf" },
	{ 0,			NULL }
};

static void
map_types(gchar *str, gchar *copy, guint32 type)
{
	gchar *start, *end;
	const gchar *name = val_to_str(type, xidtype_vals, "%s");
	int len, off = 0;
	start = strchr(str, '-') + 1;
	end = strchr(str, '\0');
	len = end - start;

	if (str[0] == '!') {
		copy[0] = '!';
		off = 1;
	}

	strncpy(copy + off, name, strlen(name));
	strncpy(copy + off + strlen(name), "-", strlen("-"));
	strncpy(copy + off + strlen(name) + strlen("-"), start, len);
}

/*
 * XIA address
 */

/* Not A Type.
 * Notice that this constant is little and big endian at same time. */
#define XIDTYPE_NAT 0
/* The range 0x01--0x0f is reserved for future use.
 * Identification numbers for new principals should be requested from
 * Michel Machado <michel@digirati.com.br>.
 */

/* Row or a node in a DAG. */
#define XIA_OUTDEGREE_MAX	4
#define XIA_XID_MAX		20
typedef __be32 xid_type_t;

struct xia_xid {
	xid_type_t	xid_type;		/* XID type		*/
	__u8		xid_id[XIA_XID_MAX];	/* eXpressive IDentifier*/
};

static inline int are_xids_equal(const __u8 *xid1, const __u8 *xid2)
{
	const __u32 *n1 = (const __u32 *)xid1;
	const __u32 *n2 = (const __u32 *)xid2;
	BUILD_BUG_ON(XIA_XID_MAX != sizeof(const __u32) * 5);
	return	n1[0] == n2[0] &&
		n1[1] == n2[1] &&
		n1[2] == n2[2] &&
		n1[3] == n2[3] &&
		n1[4] == n2[4];
}

static inline int are_sxids_equal(const struct xia_xid *xid1,
	const struct xia_xid *xid2)
{
	const __u64 *n1 = (const __u64 *)xid1;
	const __u64 *n2 = (const __u64 *)xid2;
	BUILD_BUG_ON(sizeof(struct xia_xid) != sizeof(const __u64) * 3);
	return	n1[0] == n2[0] &&
		n1[1] == n2[1] &&
		n1[2] == n2[2];
}

struct xia_row {
	struct xia_xid	s_xid;
	union {
		__u8	a[XIA_OUTDEGREE_MAX];
		__be32	i;
	} s_edge;				/* Out edges		*/
};

#define XIA_CHOSEN_EDGE		0x80
#define XIA_EMPTY_EDGE		0x7f
#define XIA_ENTRY_NODE_INDEX	0x7e

/* Notice that these constants are little and big endian
 * at same time up to 32bits.
 */
#define XIA_EMPTY_EDGES (XIA_EMPTY_EDGE << 24 | XIA_EMPTY_EDGE << 16 |\
			 XIA_EMPTY_EDGE <<  8 | XIA_EMPTY_EDGE)
#define XIA_CHOSEN_EDGES (XIA_CHOSEN_EDGE << 24 | XIA_CHOSEN_EDGE << 16 |\
			 XIA_CHOSEN_EDGE <<  8 | XIA_CHOSEN_EDGE)

static inline int is_edge_chosen(__u8 e)
{
	return e & XIA_CHOSEN_EDGE;
}

/* To be used when flipping bytes isn't necessary. */
#define be32_to_raw_cpu(n)	((__force __u32)(n))

static inline int is_any_edge_chosen(const struct xia_row *row)
{
	return be32_to_raw_cpu(row->s_edge.i) & XIA_CHOSEN_EDGES;
}

static inline int is_empty_edge(__u8 e)
{
	return (e & XIA_EMPTY_EDGE) == XIA_EMPTY_EDGE;
}

static inline int is_it_a_sink(struct xia_row *row, __u8 node, __u8 num_dst)
{
	return	node == (num_dst - 1) ||
		(be32_to_raw_cpu(row->s_edge.i) & XIA_EMPTY_EDGES) ==
			XIA_EMPTY_EDGES;
}

static inline int is_row_valid(__u8 row, __u8 num_dst)
{
	return row < num_dst || row == XIA_ENTRY_NODE_INDEX;
}

static inline void xia_mark_edge(__u8 *edge)
{
	*edge |= XIA_CHOSEN_EDGE;
}

static inline void xia_unmark_edge(__u8 *edge)
{
	*edge &= ~XIA_CHOSEN_EDGE;
}

/* XIA address. */
#define XIA_NODES_MAX		9
struct xia_addr {
	struct xia_row s_row[XIA_NODES_MAX];
};

static inline void xia_null_addr(struct xia_addr *addr)
{
	addr->s_row[0].s_xid.xid_type = XIDTYPE_NAT;
}

static inline int xia_is_nat(xid_type_t ty)
{
	return ty == XIDTYPE_NAT;
}

#ifndef __KERNEL__
/* XXX This section is only needed to make compiling applications with
 * old kernels' headers installed easier.
 */
#undef	_K_SS_MAXSIZE
#define	_K_SS_MAXSIZE 256
#endif

/* Structure describing an XIA socket address. */
struct sockaddr_xia {
	__kernel_sa_family_t	sxia_family;	/* Address family	*/
	__u16		__pad0;		/* Ensure 32-bit alignment	*/
	struct xia_addr	sxia_addr;	/* XIA address			*/

	/* Pad to size of `struct __kernel_sockaddr_storage'. */
	__u8		__pad1[_K_SS_MAXSIZE - sizeof(__kernel_sa_family_t) -
			sizeof(__u16) - sizeof(struct xia_addr)];
};

#endif	/* _NET_XIA_H	*/
