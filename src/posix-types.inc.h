## posix-types.inc.h - Include fragment to build assuan.h.
## Copyright (C) 2010  Free Software Foundation, Inc.
##
## This file is part of Assuan.
##
## Assuan is free software; you can redistribute it and/or modify it
## under the terms of the GNU Lesser General Public License as
## published by the Free Software Foundation; either version 2.1 of
## the License, or (at your option) any later version.
##
## Assuan is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public
## License along with this program; if not, see <http://www.gnu.org/licenses/>.
## SPDX-License-Identifier: LGPL-2.1+
##
##
## This file is included by the mkheader tool.  Lines starting with
## a double hash mark are not copied to the destination file.
typedef struct msghdr *assuan_msghdr_t;
typedef pid_t assuan_pid_t;
##EOF##
